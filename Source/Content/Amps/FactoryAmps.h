#pragma once
#include "Tone3000.h"
#include <map>

namespace tonegrid::factory
{
// Factory captures are installed on the NAM worker, never in the UI/audio thread.
// Models remain byte-for-byte identical; existing user captures are not replaced.
inline juce::Result seed(const juce::File& bundled,const juce::File& library,const std::atomic<bool>& cancelled)
{
    using namespace juce;
    const auto manifest=bundled.getChildFile("manifest.json");
    if(!manifest.existsAsFile())return Result::ok(); // Source-only/test builds may omit licensed content.
    if(manifest.getSize()>4*1024*1024)return Result::fail("Factory amp manifest is too large.");
    const auto hash=SHA256(manifest).toHexString();
    const auto marker=library.getParentDirectory().getChildFile("factory-amps-sha256.txt");
    if(marker.loadFileAsString().trim()==hash)return Result::ok();
    try
    {
        const auto json=nlohmann::json::parse(manifest.loadFileAsString().toStdString());
        const auto& models=json.at("amps");
        const int schema=json.at("schemaVersion").get<int>();
        if((schema!=1&&schema!=2)||!models.is_array()||models.empty()||models.size()>4096
            ||(schema==1&&models.size()!=14)||(schema==2&&json.at("ampCount")!=models.size()))return Result::fail("Invalid factory amp inventory.");
        std::set<String> names;
        // Validate the entire inventory before installing its first file.
        for(const auto& model:models)
        {
            const String name(model.at("filename").get<std::string>()),expected(model.at("sha256").get<std::string>());
            if(!namamp::validRelativePath(name)||(schema==1&&name.containsChar('/'))||expected.length()!=64||!expected.containsOnly("0123456789abcdefABCDEF")||!names.insert(name.toLowerCase()).second)
                return Result::fail("Invalid factory amp identity.");
            const auto source=bundled.getChildFile(name);
            if(cancelled.load())return Result::fail("Factory amp installation cancelled.");
            if(!source.isAChildOf(bundled)||!t3k::noLinks(source)||!source.existsAsFile()||source.getSize()>namamp::maxFileBytes||!SHA256(source).toHexString().equalsIgnoreCase(expected))
                return Result::fail("Factory amp missing or altered: "+name);
        }
        std::vector<std::pair<String,String>> assets;std::map<String,StringArray> previousAssets;
        if(json.contains("assets"))
        {
            const auto& entries=json.at("assets");if(!entries.is_array()||entries.size()>16384)return Result::fail("Invalid factory artwork inventory.");
            for(const auto& entry:entries)
            {
                const String name(entry.at("filename").get<std::string>()),expected(entry.at("sha256").get<std::string>());
                const auto parts=StringArray::fromTokens(name,"/",{});
                bool safe=name.isNotEmpty()&&name.length()<=1024&&!name.startsWithChar('/')&&!name.containsAnyOf("\\:\r\n");
                for(const auto& part:parts)safe=safe&&part.isNotEmpty()&&part!="."&&part!="..";
                const auto source=bundled.getChildFile(name);
                if(!safe||!names.insert(name.toLowerCase()).second||expected.length()!=64||!expected.containsOnly("0123456789abcdefABCDEF")
                    ||!source.isAChildOf(bundled)||!t3k::noLinks(source)||!source.existsAsFile()||source.getSize()>8*1024*1024||!SHA256(source).toHexString().equalsIgnoreCase(expected))
                    return Result::fail("Factory artwork or metadata missing or altered: "+name);
                if(entry.contains("previousSha256"))
                {const auto& prior=entry.at("previousSha256");if(!prior.is_array()||prior.size()>8)return Result::fail("Invalid artwork update history.");
                 for(const auto& value:prior){if(!value.is_string())return Result::fail("Invalid artwork update hash.");const String old(value.get<std::string>());if(old.length()!=64||!old.containsOnly("0123456789abcdefABCDEF"))return Result::fail("Invalid artwork update hash.");previousAssets[name].add(old);}}
                assets.emplace_back(name,expected);
            }
        }
        // Retire only explicitly withdrawn former factory names. Never match by
        // hash alone: an approved numbered-folder capture can share those bytes.
        std::vector<std::pair<String,String>> retired;
        if(json.contains("retiredAmps"))
        {
            const auto& entries=json.at("retiredAmps");
            if(!entries.is_array()||entries.size()>4096)return Result::fail("Invalid retired amp inventory.");
            std::set<String> retiredNames;
            for(const auto& entry:entries)
            {
                const String name(entry.at("filename").get<std::string>()),expected(entry.at("sha256").get<std::string>());
                if(!namamp::validRelativePath(name)||(schema==1&&name.containsChar('/'))||!name.endsWithIgnoreCase(".nam")
                    ||expected.length()!=64||!expected.containsOnly("0123456789abcdefABCDEF")
                    ||names.count(name.toLowerCase())!=0||!retiredNames.insert(name.toLowerCase()).second)
                    return Result::fail("Invalid retired amp identity.");
                retired.emplace_back(name,expected);
                if(entry.contains("previousSha256"))
                {
                    const auto& previous=entry.at("previousSha256");if(!previous.is_array()||previous.size()>8)return Result::fail("Invalid retired capture history.");
                    for(const auto& value:previous){if(!value.is_string())return Result::fail("Invalid retired capture history.");const String old(value.get<std::string>());if(old.length()!=64||!old.containsOnly("0123456789abcdefABCDEF"))return Result::fail("Invalid retired capture history.");retired.emplace_back(name,old);}
                }
            }
        }
        if(json.contains("retiredAssets"))
        {
            const auto& entries=json.at("retiredAssets");
            if(!entries.is_array()||entries.size()>4096)return Result::fail("Invalid retired artwork inventory.");
            std::set<String> retiredNames;
            for(const auto& entry:entries)
            {
                const String name(entry.at("filename").get<std::string>()),expected(entry.at("sha256").get<std::string>());
                const auto parts=StringArray::fromTokens(name,"/",{});
                bool safe=name.isNotEmpty()&&name.length()<=1024&&!name.startsWithChar('/')&&!name.containsAnyOf("\\:\r\n");
                for(const auto& part:parts)safe=safe&&part.isNotEmpty()&&part!="."&&part!="..";
                const auto base=parts.isEmpty()?String{}:parts[parts.size()-1];
                if(!safe||((base!="thumbnail.jpg"&&base!="thumbnail.png"&&base!="thumbnail.jpeg"&&base!=".tonegrid-tone3000.json")&&!(parts.size()==2&&parts[0]=="Artwork"&&(base.endsWithIgnoreCase(".png")||base.endsWithIgnoreCase(".jpg")||base.endsWithIgnoreCase(".jpeg")||base.endsWithIgnoreCase(".json"))))
                    ||expected.length()!=64||!expected.containsOnly("0123456789abcdefABCDEF")
                    ||names.count(name.toLowerCase())||!retiredNames.insert(name.toLowerCase()).second)
                    return Result::fail("Invalid retired artwork identity.");
                retired.emplace_back(name,expected);
                if(entry.contains("previousSha256"))
                {const auto& previous=entry.at("previousSha256");if(!previous.is_array()||previous.size()>8)return Result::fail("Invalid retired artwork history.");
                 for(const auto& value:previous){if(!value.is_string())return Result::fail("Invalid retired artwork history.");const String old(value.get<std::string>());if(old.length()!=64||!old.containsOnly("0123456789abcdef"))return Result::fail("Invalid retired artwork hash.");retired.emplace_back(name,old);}}
            }
        }
        std::vector<namamp::Identity> archived;
        if(json.contains("archivedAmps"))
        {
            const auto& entries=json.at("archivedAmps");if(!entries.is_array()||entries.size()>4096)return Result::fail("Invalid archived amp inventory.");
            std::set<String> archivedNames;
            for(const auto& entry:entries)
            {
                namamp::Identity identity{String(entry.at("filename").get<std::string>()),String(entry.at("sha256").get<std::string>()),entry.at("bytes").get<juce::int64>()};
                if(!identity.valid()||names.count(identity.file.toLowerCase())||!archivedNames.insert(identity.file.toLowerCase()).second)return Result::fail("Invalid archived amp identity.");
                archived.push_back(std::move(identity));
            }
        }
        std::map<String,String> metadataUpdates;
        if(json.contains("metadataUpdates"))
        {
            const auto& updates=json.at("metadataUpdates");if(!updates.is_array()||updates.size()>4096)return Result::fail("Invalid metadata update inventory.");
            for(const auto& update:updates)
            {
                const String name(update.at("filename").get<std::string>()),oldHash(update.at("previousSha256").get<std::string>()),newHash(update.at("sha256").get<std::string>());
                if(!namamp::validRelativePath(name)||oldHash.length()!=64||!oldHash.containsOnly("0123456789abcdef")||oldHash==newHash||metadataUpdates.contains(name))return Result::fail("Invalid metadata update identity.");
                bool matches=false;for(const auto& model:models)matches|=model.at("filename")==name.toStdString()&&model.at("sha256")==newHash.toStdString();
                if(!matches)return Result::fail("Metadata update does not match current inventory.");metadataUpdates.emplace(name,oldHash);
            }
        }
        const auto folder=library.getChildFile("Factory");
        if(!t3k::noLinks(folder)||folder.createDirectory().failed())return Result::fail("Cannot install factory amps.");
        const auto refreshMetadata=[&](const File& destination,const String& name)->Result
        {
            const auto update=metadataUpdates.find(name);
            if(update==metadataUpdates.end()||!destination.existsAsFile()||!t3k::noLinks(destination)||SHA256(destination).toHexString()!=update->second)return Result::ok();
            auto before=nlohmann::json::parse(destination.loadFileAsString().toStdString());
            auto after=nlohmann::json::parse(bundled.getChildFile(name).loadFileAsString().toStdString());
            // Only the recovered optional loudness trim may differ. Trained
            // parameters, input/output references and other metadata must match.
            if(!before.contains("metadata")||!before["metadata"].is_object()||!after.contains("metadata")||!after["metadata"].is_object())return Result::fail("Invalid metadata-only update.");
            before["metadata"].erase("Tonegrid_Normalization");after["metadata"].erase("Tonegrid_Normalization");
            if(before!=after)return Result::fail("Metadata update would change the capture: "+name);
            TemporaryFile temporary(destination);
            if(cancelled.load()||!bundled.getChildFile(name).copyFileTo(temporary.getFile())||!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot restore normalization metadata: "+name);
            return Result::ok();
        };
        for(const auto& model:models)
        {
            if(cancelled.load())return Result::fail("Factory amp installation cancelled.");
            const String name(model.at("filename").get<std::string>()),expected(model.at("sha256").get<std::string>());
            const auto existing=library.getChildFile(name);
            if(const auto updated=refreshMetadata(existing,name);updated.failed())return updated;
            if(existing.existsAsFile()&&t3k::noLinks(existing)&&SHA256(existing).toHexString().equalsIgnoreCase(expected))continue;
            // v2 retains useful source folders. A user's same-name capture is
            // preserved and the bundled original gets a separate hash-resolved path.
            const auto destination=schema==2&&!existing.exists()?existing:folder.getChildFile(name);
            if(const auto updated=refreshMetadata(destination,name);updated.failed())return updated;
            if(destination.exists())
            {
                if(!t3k::noLinks(destination)||!SHA256(destination).toHexString().equalsIgnoreCase(expected))return Result::fail("Existing factory capture kept: "+name);
                continue;
            }
            if(!t3k::noLinks(destination.getParentDirectory())||destination.getParentDirectory().createDirectory().failed())return Result::fail("Cannot create factory amp folder: "+name);
            TemporaryFile temporary(destination);
            if(!bundled.getChildFile(name).copyFileTo(temporary.getFile())||cancelled.load()||!t3k::commitPack(temporary.getFile(),destination))
                return Result::fail("Cannot install factory amp: "+name);
        }
        std::set<String> customizedThumbnailFolders;
        for(const auto& asset:assets)
        {
            const auto destination=library.getChildFile(asset.first);
            if(destination.getFileName()!="thumbnail.jpg")continue;
            const auto png=destination.withFileExtension("png");if(!png.existsAsFile()||!t3k::noLinks(png))continue;
            const auto oldHash=SHA256(png).toHexString();bool stock=false;
            const auto relative=png.getRelativePathFrom(library).replaceCharacter('\\','/');
            for(const auto& old:retired)stock|=old.first==relative&&old.second==oldHash;
            if(!stock)customizedThumbnailFolders.insert(destination.getParentDirectory().getRelativePathFrom(library).replaceCharacter('\\','/'));
        }
        for(const auto& asset:assets)
        {
            if(cancelled.load())return Result::fail("Factory amp installation cancelled.");
            const auto destination=library.getChildFile(asset.first);
            const auto parent=destination.getParentDirectory().getRelativePathFrom(library).replaceCharacter('\\','/');
            if(customizedThumbnailFolders.count(parent)&&(destination.getFileName()=="thumbnail.jpg"||destination.getFileName()==".tonegrid-tone3000.json"))continue;
            // Existing personal artwork/metadata always wins over the bundle.
            if(destination.exists())
            {
                auto prior=previousAssets.find(asset.first);if(prior==previousAssets.end()||!destination.existsAsFile()||!t3k::noLinks(destination)||!prior->second.contains(SHA256(destination).toHexString(),true))continue;
                TemporaryFile replacement(destination);if(!bundled.getChildFile(asset.first).copyFileTo(replacement.getFile())||cancelled.load()||!replacement.overwriteTargetFileWithTemporary())return Result::fail("Cannot refresh factory artwork: "+asset.first);continue;
            }
            if(!t3k::noLinks(destination.getParentDirectory())||destination.getParentDirectory().createDirectory().failed())return Result::fail("Cannot create factory artwork folder.");
            TemporaryFile temporary(destination);
            if(!bundled.getChildFile(asset.first).copyFileTo(temporary.getFile())||cancelled.load()||!t3k::commitPack(temporary.getFile(),destination))
                return Result::fail("Cannot install factory artwork: "+asset.first);
        }
        // Preserve the exact retired Full Rig outside the browsed amp library.
        // Old user presets retain their original sound and identities on upgrade.
        for(const auto& identity:archived)for(const auto& base:{library,folder})
        {
            if(cancelled.load())return Result::fail("Factory amp installation cancelled.");
            const auto original=base.getChildFile(identity.file);
            if(!original.existsAsFile()||!t3k::noLinks(original)||original.getSize()!=identity.bytes||SHA256(original).toHexString()!=identity.sha256)continue;
            const auto archive=namamp::factoryArchive::directory(library),destination=archive.getChildFile(original.getRelativePathFrom(library));
            if(!t3k::noLinks(archive)||!t3k::noLinks(destination)||destination.getParentDirectory().createDirectory().failed())return Result::fail("Cannot create Full Rig compatibility archive.");
            if(destination.exists()&&(destination.getSize()!=identity.bytes||SHA256(destination).toHexString()!=identity.sha256))return Result::fail("Existing archived Full Rig preserved: "+identity.file);
            if(!destination.exists())
            {TemporaryFile temporary(destination);if(!original.copyFileTo(temporary.getFile())||cancelled.load()||!t3k::commitPack(temporary.getFile(),destination))return Result::fail("Cannot archive former factory Full Rig: "+identity.file);}
            if(destination.getSize()!=identity.bytes||SHA256(destination).toHexString()!=identity.sha256||cancelled.load())return Result::fail("Full Rig archive verification failed: "+identity.file);
            if(!original.deleteFile())return Result::fail("Cannot finish archiving Full Rig: "+identity.file);
        }
        // All current assets have validated and installed before withdrawing
        // known legacy copies. Modified user files and symlinks are untouched.
        for(const auto& identity:retired)
            for(const auto& base:{library,folder})
            {
                if(cancelled.load())return Result::fail("Factory amp installation cancelled.");
                const auto obsolete=base.getChildFile(identity.first);
                if(obsolete.existsAsFile()&&t3k::noLinks(obsolete)
                    &&SHA256(obsolete).toHexString().equalsIgnoreCase(identity.second)&&!obsolete.deleteFile())
                    return Result::fail("Cannot retire withdrawn factory amp: "+identity.first);
                if(identity.first.containsChar('/')&&obsolete.getParentDirectory().isAChildOf(base)&&obsolete.getParentDirectory().findChildFiles(File::findFilesAndDirectories,false).isEmpty())obsolete.getParentDirectory().deleteFile();
            }
        if(!marker.replaceWithText(hash))return Result::fail("Cannot save factory amp installation state.");
        return Result::ok();
    }
    catch(...){return Result::fail("Invalid factory amp manifest.");}
}
}
