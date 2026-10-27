#pragma once
#include <filesystem>
#include "Platform/AppPaths.h"
#include "NamAmpState.h"
#include "Content/Amps/FactoryAmpArchive.h"
#include <juce_cryptography/juce_cryptography.h>
#include <NAM/get_dsp.h>
#include <NAM/model_config.h>
#include <vector>
#include "NamCalibrationMetadata.h"
#include <optional>
#include "Content/Amps/NamLibraryIndex.h"
#include "NamInputCompensation.h"

namespace namamp
{
// Only application/platform path policy knows where the canonical library lives.
inline juce::File libraryForApplicationData(const juce::File& root)
{return tonegrid::paths::userFolder(root).getChildFile("NAM/Amps");}
// Optional model metadata is read on the loading thread, never in processBlock.
inline std::optional<float> modelLoudness(const nlohmann::json& json)
{
    const auto metadata=json.find("metadata");
    if(metadata==json.end()||!metadata->is_object())return {};
    const auto value=metadata->find("loudness");
    if(value==metadata->end()||!value->is_number())return {};
    const auto db=value->get<double>();
    if(!std::isfinite(db)||db< -100||db>0)return {};
    return static_cast<float>(db);
}
inline std::optional<float> optionalNormalizationTrim(const nlohmann::json& json)
{
    const auto metadata=json.find("metadata");
    if(metadata==json.end()||!metadata->is_object())return {};
    const auto field=metadata->find("Tonegrid_Normalization");
    if(field==metadata->end()||!field->is_object())return {};
    const auto version=field->find("version"),trim=field->find("trim_db");
    if(version==field->end()||!version->is_number_integer()||*version!=1
       ||trim==field->end()||!trim->is_number())return {};
    const auto db=trim->get<double>();
    if(!std::isfinite(db)||db< -24.||db>24.)return {};
    return static_cast<float>(db);
}
inline float modelNormalizationTrim(const nlohmann::json& json)
{return optionalNormalizationTrim(json).value_or(0.f);}
struct Entry {Identity identity;juce::String architecture,version;std::optional<float> loudnessDb;tonegrid::calibration::Metadata calibration;float normalizationTrimDb=0.f;bool hasNormalizationTrim=false;float inputCompensationDb=0.f;};
struct Failure {juce::String file,reason;};
struct Catalog {std::vector<Entry> entries;std::vector<Failure> failures;std::shared_ptr<tonegrid::libraryui::Index> browser;};
class Library
{
public:
    explicit Library(juce::File root):directory(std::move(root)){}
    const juce::File& folder() const {return directory;}
    // A JSON/index pass only: no DSP creation, weights vector conversion, Reset,
    // prewarm or persistent pedal-catalog coupling. Recursion stays inside this root.
    juce::Result read(const juce::File& file,Entry& output,nlohmann::json* payload=nullptr,tonegrid::libraryui::Capture* presentation=nullptr) const
    {
        using namespace juce;
        if(presentation)*presentation=tonegrid::libraryui::basic(file,directory);
        try
        {
            const auto relative=file.getRelativePathFrom(directory).replaceCharacter('\\','/');
            if(!file.isAChildOf(directory)||!validRelativePath(relative)||!file.existsAsFile()||!contained(file))return Result::fail("NAM amp missing from local library: "+relative);
            const auto bytes=file.getSize();if(bytes<=0||bytes>maxFileBytes)return Result::fail("NAM amp empty or exceeds 32 MiB: "+file.getFileName());
            MemoryBlock raw;if(!file.loadFileAsData(raw)||raw.getSize()!=(size_t)bytes)return Result::fail("Cannot read NAM amp: "+file.getFileName());
            const auto* begin=static_cast<const char*>(raw.getData());auto json=nlohmann::json::parse(begin,begin+raw.getSize());
            if(presentation)try{tonegrid::libraryui::embedded(*presentation,json);}catch(...){/* Optional presentation never changes NAM validation. */}
            if(!json.is_object()||!json.contains("version")||!json.contains("architecture")||!json.contains("config")||!json.contains("weights")
               ||!json["config"].is_object()||!json["weights"].is_array())return Result::fail("Invalid NAM amp metadata: "+file.getFileName());
            const auto version=json.at("version").get<std::string>(),architecture=json.at("architecture").get<std::string>();
            nam::verify_config_version(version);
            if(!nam::ConfigParserRegistry::instance().has(architecture))return Result::fail("Unsupported NAM architecture: "+String(architecture));
            if(nam::get_sample_rate_from_nam_file(json)!=48000)return Result::fail("NAM amp requires explicit 48000 Hz training rate: "+file.getFileName());
            Entry candidate{{relative,SHA256(raw).toHexString(),bytes},String(architecture),String(version),modelLoudness(json),tonegrid::calibration::metadata(json),modelNormalizationTrim(json),optionalNormalizationTrim(json).has_value(),inputCompensation(json).value_or(0.f)};
            if(!candidate.identity.valid())return Result::fail("Invalid local NAM amp identity.");
            output=std::move(candidate);if(payload)*payload=std::move(json);return Result::ok();
        }
        catch(const std::exception& error){return Result::fail("NAM amp metadata: "+String(error.what()));}
        catch(...){return Result::fail("NAM amp metadata could not be read.");}
    }
    juce::Result refresh(Catalog& output) const
    {
        const auto made=directory.createDirectory();if(made.failed())return made;
        Catalog candidate;candidate.browser=std::make_shared<tonegrid::libraryui::Index>();
        for(const auto& folder:directory.findChildFiles(juce::File::findDirectories,true,"*",juce::File::FollowSymlinks::no))
            if(!folder.isSymbolicLink())candidate.browser->folders.insert(folder.getRelativePathFrom(directory).replaceCharacter('\\','/'));
        for(const auto& file:directory.findChildFiles(juce::File::findFiles,true,"*",juce::File::FollowSymlinks::no))if(file.hasFileExtension("nam"))
        {Entry entry;tonegrid::libraryui::Capture info;auto result=read(file,entry,nullptr,&info);info.error=result.getErrorMessage();candidate.browser->captures.emplace(info.path,std::move(info));if(result.wasOk())candidate.entries.push_back(std::move(entry));else candidate.failures.push_back({file.getRelativePathFrom(directory).replaceCharacter('\\','/'),result.getErrorMessage()});}
        std::sort(candidate.entries.begin(),candidate.entries.end(),[](const auto& a,const auto& b){return a.identity.file.compareIgnoreCase(b.identity.file)<0;});
        output=std::move(candidate);return juce::Result::ok();
    }
    juce::Result resolve(const Identity& identity,Entry& output,nlohmann::json& payload) const
    {
        if(!identity.valid())return juce::Result::fail("Invalid saved NAM amp identity.");
        Entry candidate;nlohmann::json json;auto target=directory.getChildFile(identity.file);
        // Factory reorganisations retain saved identities without rewriting any
        // personal preset. Only a declared move with identical bytes can resolve.
        if(!target.exists())
        {
            const auto map=directory.getChildFile("factory-relocations.json");
            if(map.existsAsFile()&&!map.isSymbolicLink()&&map.getSize()<2*1024*1024)
            try
            {
                const auto moves=nlohmann::json::parse(map.loadFileAsString().toStdString());
                if(moves.value("schemaVersion",0)==1)for(const auto& move:moves.at("moves"))
                {
                    const juce::String from(move.at("from").get<std::string>()),to(move.at("to").get<std::string>());
                    if(from==identity.file&&validRelativePath(to)&&move.at("sha256")==identity.sha256.toStdString()
                        &&move.at("bytes").get<juce::int64>()==identity.bytes)
                    {target=directory.getChildFile(to);break;}
                }
            }
            catch(...){} // A malformed map never relaxes the saved content check.
        }
        if(!target.exists()&&factoryArchive::declares(directory,identity))
        {
            const Library archived(factoryArchive::directory(directory));
            for(const auto& relative:{identity.file,factoryArchive::canonical(identity.file),juce::String("Factory/")+factoryArchive::canonical(identity.file)})
            {
                const auto old=archived.folder().getChildFile(relative);if(!old.exists())continue;
                const auto checked=archived.read(old,candidate,&json);if(checked.failed())return checked;
                if(candidate.identity.sha256!=identity.sha256||candidate.identity.bytes!=identity.bytes)return juce::Result::fail("Archived Full Rig has changed: "+identity.file);
                output=std::move(candidate);payload=std::move(json);return juce::Result::ok();
            }
        }
        const auto result=read(target,candidate,&json);
        if(result.failed())return result;
        if(candidate.identity.sha256!=identity.sha256||candidate.identity.bytes!=identity.bytes)return juce::Result::fail("NAM amp changed since it was selected/saved: "+identity.file+". Refresh and select it explicitly.");
        output=std::move(candidate);payload=std::move(json);return juce::Result::ok();
    }
private:
    bool contained(const juce::File& file) const
    {
        std::error_code error;
        const auto root=std::filesystem::weakly_canonical(std::filesystem::u8path(directory.getFullPathName().toStdString()),error);if(error)return false;
        const auto actual=std::filesystem::weakly_canonical(std::filesystem::u8path(file.getFullPathName().toStdString()),error);if(error)return false;
        const auto relative=actual.lexically_relative(root);
        return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!=std::filesystem::path("..");
    }
    const juce::File directory;
};
}
