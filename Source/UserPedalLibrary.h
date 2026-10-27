#pragma once
#include "Effects/Pedalboard/PedalBoard.h"
#include "Content/Amps/NamAmpLibrary.h"
#include <mutex>
#include <map>
namespace tonegrid::userpedals
{
inline bool hashValid(const juce::String& hash){return hash.length()==64&&hash.containsOnly("0123456789abcdef");}
struct Entry{juce::String hash,label,sourceUrl;juce::File location;};
struct Prepared
{
    juce::Result result=juce::Result::ok();board::Model model;juce::File source;
};
inline Prepared prepare(const juce::File& source,bool lite,const juce::String& expected={})
{
    Prepared result;result.source=source;
    try
    {
        namamp::Entry entry;nlohmann::json payload;
        const auto inspected=namamp::Library(source.getParentDirectory()).read(source,entry,&payload);
        if(inspected.failed()){result.result=inspected;return result;}
        if(expected.isNotEmpty()&&entry.identity.sha256!=expected){result.result=juce::Result::fail("The personal capture has changed. Import it again explicitly.");return result;}
        if(entry.calibration.invalid){result.result=juce::Result::fail("Malformed calibration metadata. This capture cannot be imported.");return result;}
        auto& m=result.model;m.hash=entry.identity.sha256;m.label=source.getFileNameWithoutExtension();m.file="User/"+m.hash+".nam";m.calibration=entry.calibration;
        m.details="Personal NAM capture / "+entry.architecture+" / NAM "+entry.version;
        tonegrid::calibration::sanitiseMetadata(payload);nam::DspLoadOptions options;options.prewarm=false;m.dsp=nam::get_dsp(payload,options);
        if(m.dsp->NumInputChannels()!=1||m.dsp->NumOutputChannels()!=1||m.dsp->GetExpectedSampleRate()!=48000){m.dsp.reset();result.result=juce::Result::fail("Pedal captures must be mono, trained at 48000 Hz.");return result;}
        m.dsp->SetPrewarmOnReset(false);m.dsp->Reset(48000,board::chunk);tonegrid::namquality::apply(*m.dsp,lite);m.dsp->prewarm();
    }
    catch(const std::exception& e){result.result=juce::Result::fail("Cannot load this NAM pedal: "+juce::String(e.what()));}
    return result;
}
// The index is refreshed off the audio/UI thread. Content hashes remain preset identities,
// while filenames are deliberately readable and may be managed in the OS file manager.
class Library final
{
    struct Cached {juce::int64 size=0,modified=0;Entry entry;juce::String error;};
public:
    explicit Library(juce::File directory):root(std::move(directory)){}
    juce::File folder(int p) const{return root.getChildFile(juce::String(p));}
    juce::Result ensureFolder(int p) const
    {return p>=0&&p<board::count&&board::isNam(p)&&safe(folder(p))?folder(p).createDirectory():juce::Result::fail("Invalid personal pedal folder.");}
    juce::File file(int p,const juce::String& hash) const
    {
        if(!hashValid(hash))return {};
        for(const auto& e:entries(p))if(e.hash==hash)return e.location;
        return {}; // A missing identity must never resolve to different file contents.
    }
    std::vector<Entry> entries(int p) const
    {std::lock_guard<std::mutex> guard(indexLock);return p>=0&&p<board::count?index[size_t(p)]:std::vector<Entry>{};}
    Entry entry(int p,const juce::String& hash) const{for(const auto& e:entries(p))if(e.hash==hash)return e;return {hash,"Missing personal capture",{},{}};}
    bool refresh() const
    {
        std::lock_guard<std::mutex> operation(operationLock);return scan();
    }
    juce::String errors() const{std::lock_guard<std::mutex> guard(indexLock);return problems;}
    juce::Result store(int p,const Prepared& prepared,const juce::String& sourceUrl={}) const
    {
        if(!prepared.model.dsp)return juce::Result::fail("Invalid personal capture.");
        return importPack(p,{{prepared.source,sourceUrl}},false,prepared.model.label);
    }
    // No calibration prompts here. Missing metadata is reported only when selected.
    // Stage every compatible capture before publishing any files; rollback only files
    // created by this transaction, preserving existing captures and factory banks.
    juce::Result importPack(int p,const std::vector<std::pair<juce::File,juce::String>>& sources,bool lite,const juce::String& label={}) const
    {
        using namespace juce;
        if(p<0||p>=board::count||!board::isNam(p)||sources.empty())return Result::fail("Invalid personal pedal pack.");
        std::lock_guard<std::mutex> operation(operationLock);
        if(!safe(folder(p))||folder(p).createDirectory().failed())return Result::fail("Cannot create the personal pedal folder.");
        const auto stage=root.getChildFile(".import-"+Uuid().toString());if(stage.createDirectory().failed())return Result::fail("Cannot stage the pedal pack.");
        const ScopeGuard cleanup{[&]{if(stage.isAChildOf(root))stage.deleteRecursively();}};
        std::vector<std::pair<File,File>> moves;std::vector<File> created;StringArray seen;
        for(const auto& source:sources)
        {
            auto prepared=prepare(source.first,lite);if(prepared.result.failed())return Result::fail(source.first.getFileName()+": "+prepared.result.getErrorMessage()+" No captures added.");
            const auto hash=prepared.model.hash;if(seen.contains(hash))continue;seen.add(hash);
            bool existing=false;
            for(const auto& e:entries(p))if(e.hash==hash&&e.location.existsAsFile()&&SHA256(e.location).toHexString()==hash){existing=true;break;}
            if(existing)continue;
            auto name=File::createLegalFileName(label.isNotEmpty()&&sources.size()==1?label:source.first.getFileNameWithoutExtension()).substring(0,100).trim().trimCharactersAtEnd(". ");if(name.isEmpty())name="Capture";
            const auto base=name.upToFirstOccurrenceOf(".",false,false).toUpperCase();
            if(base=="CON"||base=="PRN"||base=="AUX"||base=="NUL"||(base.length()==4&&(base.startsWith("COM")||base.startsWith("LPT"))&&base[3]>='1'&&base[3]<='9'))name="_"+name;
            auto destination=folder(p).getChildFile(name+".nam");int suffix=1;
            const auto reserved=[&](const File& f){if(f.exists()||f.withFileExtension("json").exists())return true;for(const auto& m:moves)if(m.second==f)return true;return false;};
            while(reserved(destination))destination=folder(p).getChildFile(name+" ("+String(++suffix)+").nam");
            const auto staged=stage.getChildFile(hash+".nam");
            if(!source.first.copyFileTo(staged)||SHA256(staged).toHexString()!=hash)return Result::fail("Capture changed during import. No captures added.");
            auto* object=new DynamicObject;object->setProperty("schema",1);object->setProperty("label",label.isNotEmpty()&&sources.size()==1?label:source.first.getFileNameWithoutExtension());object->setProperty("sha256",hash);object->setProperty("sourceUrl",source.second);
            if(!staged.withFileExtension("json").replaceWithText(JSON::toString(var(object))))return Result::fail("Cannot stage capture information.");
            moves.emplace_back(staged,destination);moves.emplace_back(staged.withFileExtension("json"),destination.withFileExtension("json"));
        }
        for(const auto& move:moves)
        {
            if(move.second.exists()||!move.first.moveFileTo(move.second))
            {for(const auto& f:created)f.deleteFile();scan();return Result::fail("Cannot save the complete pack. New captures rolled back.");}
            created.push_back(move.second);
        }
        scan();return Result::ok();
    }
    juce::Result remove(int p,const juce::String& hash) const
    {
        using namespace juce;if(!hashValid(hash)||p<0||p>=board::count||!board::isNam(p))return Result::fail("Invalid personal capture.");
        std::lock_guard<std::mutex> operation(operationLock);scan();bool found=false;
        for(const auto& f:folder(p).findChildFiles(File::findFiles,true,"*",File::FollowSymlinks::no))
            if(f.hasFileExtension("nam")&&safe(f)&&f.getSize()<=32*1024*1024&&SHA256(f).toHexString()==hash)
            {found=true;if(!f.deleteFile())return Result::fail("Cannot delete this personal capture.");const auto sidecar=f.withFileExtension("json");if(sidecar.getSize()<=65536&&JSON::parse(sidecar)["sha256"].toString()==hash)sidecar.deleteFile();}
        scan();return found?Result::ok():Result::fail("This capture is already missing from the folder.");
    }
private:
    bool safe(juce::File file) const
    {if(!file.isAChildOf(root))return false;for(auto dir=file;dir!=dir.getParentDirectory();dir=dir.getParentDirectory())if(dir.isSymbolicLink())return false;return true;}
    bool scan() const
    {
        using namespace juce;std::map<String,Cached> next;std::array<std::vector<Entry>,board::count> collected;String issues;
        for(int p=0;p<board::count;++p)if(board::isNam(p)&&safe(folder(p)))
        {
            for(const auto& f:folder(p).findChildFiles(File::findFiles,true,"*",File::FollowSymlinks::no))if(f.hasFileExtension("nam")&&safe(f))
            {
                Cached cached;const auto key=f.getFullPathName();const auto it=cache.find(key);const auto size=f.getSize(),stamp=f.getLastModificationTime().toMilliseconds();
                if(it!=cache.end()&&it->second.size==size&&it->second.modified==stamp)cached=it->second;
                else
                {
                    cached.size=size;cached.modified=stamp;auto prepared=prepare(f,true);
                    if(prepared.result.failed())cached.error=f.getFileName()+": "+prepared.result.getErrorMessage();
                    else
                    {
                        const auto sidecar=f.withFileExtension("json");const auto info=sidecar.getSize()<=65536?JSON::parse(sidecar):var{};
                        auto label=info["sha256"].toString()==prepared.model.hash?info["label"].toString():String{};
                        if(label.isEmpty())label=f.getFileNameWithoutExtension();
                        cached.entry={prepared.model.hash,label,info["sourceUrl"].toString(),f};
                    }
                }
                next.emplace(key,cached);
                if(cached.error.isNotEmpty()){issues+=cached.error+"\n";continue;}
                bool duplicate=false;for(const auto& e:collected[size_t(p)])if(e.hash==cached.entry.hash){duplicate=true;break;}
                if(!duplicate)collected[size_t(p)].push_back(cached.entry);
            }
            std::sort(collected[size_t(p)].begin(),collected[size_t(p)].end(),[](const auto& a,const auto& b){return a.label.compareIgnoreCase(b.label)<0;});
        }
        cache=std::move(next);std::lock_guard<std::mutex> guard(indexLock);bool changed=issues!=problems;
        for(size_t p=0;p<index.size();++p)
        {
            if(index[p].size()!=collected[p].size()){changed=true;continue;}
            for(size_t i=0;i<index[p].size();++i)if(index[p][i].hash!=collected[p][i].hash||index[p][i].location!=collected[p][i].location||index[p][i].label!=collected[p][i].label){changed=true;break;}
        }
        index=std::move(collected);problems=issues;return changed;
    }
    juce::File root;mutable std::mutex operationLock,indexLock;
    mutable std::map<juce::String,Cached> cache;
    mutable std::array<std::vector<Entry>,board::count> index;
    mutable juce::String problems;
};
}
