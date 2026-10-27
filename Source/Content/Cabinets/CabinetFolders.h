#pragma once
#include "CabinetState.h"
#include <juce_cryptography/juce_cryptography.h>
#include <filesystem>
#include <algorithm>
#include <map>
#include <json.hpp>

namespace tonegrid::cabinet
{
inline bool retiredCabinetPath(const juce::String& path)
{return path==".tonegrid-retired"||path.startsWith(".tonegrid-retired/");}
inline bool containedFile(const juce::File& file,const juce::File& root)
{
    if(!file.isAChildOf(root)||root.isSymbolicLink())return false;
    for(auto current=file;current!=root;current=current.getParentDirectory())if(current.isSymbolicLink())return false;
    return safeRelative(file.getRelativePathFrom(root).replaceCharacter('\\','/'));
}
// copy_options::none never overwrites a destination, including on case-sensitive
// macOS/Linux volumes. Remove the source only after verifying the complete copy.
inline bool relocateCabinetFile(const juce::File& source,const juce::File& target,const juce::File& root)
{
    if(source==target||!source.existsAsFile()||!containedFile(source,root)||!containedFile(target,root))return false;
    if(target.getParentDirectory().createDirectory().failed())return false;
    const auto hash=juce::SHA256(source).toHexString();
    if(!target.exists())
    {
        std::error_code error;
        std::filesystem::copy_file(std::filesystem::u8path(source.getFullPathName().toStdString()),std::filesystem::u8path(target.getFullPathName().toStdString()),std::filesystem::copy_options::none,error);
        if(error)return false;
    }
    return target.existsAsFile()&&juce::SHA256(target).toHexString()==hash&&juce::SHA256(source).toHexString()==hash&&source.deleteFile();
}
inline void copyCabinetArtwork(const juce::File& source,const juce::File& target,const juce::File& root)
{
    if(!target.exists()&&source.existsAsFile()&&source.getSize()<=8*1024*1024&&containedFile(source,root)&&containedFile(target,root)&&target.getParentDirectory().createDirectory().wasOk())
    {
        std::error_code error;
        std::filesystem::copy_file(std::filesystem::u8path(source.getFullPathName().toStdString()),std::filesystem::u8path(target.getFullPathName().toStdString()),std::filesystem::copy_options::none,error);
    }
}
inline void retainCabinetArtwork(const juce::File& oldFolder,const juce::File& newFolder,const juce::File& root)
{
    for(auto folder=oldFolder;folder==root||folder.isAChildOf(root);folder=folder.getParentDirectory())
    {
        bool found=false;
        for(const auto* name:{"thumbnail.jpg","thumbnail.png","thumbnail.jpeg"})
        {
            const auto source=folder.getChildFile(name);found|=source.existsAsFile();copyCabinetArtwork(source,newFolder.getChildFile(name),root);
        }
        if(found||folder==root)break;
    }
    copyCabinetArtwork(oldFolder.getChildFile(".tonegrid-cabinet.json"),newFolder.getChildFile(".tonegrid-cabinet.json"),root);
}
// The former Factory wrapper was an installation detail. Keep personal trees
// intact while exposing their real first-level folders beside the factory ones.
inline void unwrapLegacyCabinetFolders(const juce::File& root)
{
    const auto legacy=root.getChildFile("Factory");if(!legacy.isDirectory()||!containedFile(legacy,root))return;
    for(const auto& file:legacy.findChildFiles(juce::File::findFiles,true,"*",juce::File::FollowSymlinks::no))
        relocateCabinetFile(file,root.getChildFile(file.getRelativePathFrom(legacy)),root);
    // A conflicting personal file stays at its original path and remains browsable.
    auto directories=legacy.findChildFiles(juce::File::findDirectories,true,"*",juce::File::FollowSymlinks::no);
    std::sort(directories.begin(),directories.end(),[](const auto& a,const auto& b){return a.getFullPathName().length()>b.getFullPathName().length();});
    for(const auto& folder:directories)if(containedFile(folder,root)&&!folder.isSymbolicLink()&&folder.getNumberOfChildFiles(juce::File::findFilesAndDirectories)==0)folder.deleteFile();
    if(legacy.getNumberOfChildFiles(juce::File::findFilesAndDirectories)==0)legacy.deleteFile();
}
inline void pruneEmptyCabinetParents(juce::File folder,const juce::File& root)
{
    while(folder!=root&&containedFile(folder,root)&&folder.isDirectory()&&!folder.isSymbolicLink()
          &&folder.getNumberOfChildFiles(juce::File::findFilesAndDirectories)==0)
    {const auto parent=folder.getParentDirectory();if(!folder.deleteFile())break;folder=parent;}
}
// Retire only a complete, publisher-declared collection with exact original WAV
// hashes. Keep every byte under the same root so old presets/render snapshots can
// still resolve their original sound, while the browser omits the retired tree.
inline void retireCabinetCollections(const juce::File& root,const nlohmann::json& collections)
{
    if(!collections.is_array()||collections.size()>16)return;
    for(const auto& collection:collections)try
    {
        const juce::String path(collection.at("folder").get<std::string>());
        if(!safeRelative(path)||retiredCabinetPath(path))continue;
        const auto source=root.getChildFile(path),target=root.getChildFile(".tonegrid-retired/"+path);
        if(!containedFile(source,root)||!containedFile(target,root)||!source.isDirectory()||target.exists())continue;
        const auto& declared=collection.at("files");if(!declared.is_array()||declared.empty()||declared.size()>512)continue;
        std::map<juce::String,juce::String> hashes;bool valid=true;
        for(const auto& item:declared){const juce::String file(item.at("file").get<std::string>()),hash(item.at("sha256").get<std::string>());valid&=safeRelative(file)&&file.endsWithIgnoreCase(".wav")&&hash.length()==64&&hash.containsOnly("0123456789abcdef")&&hashes.emplace(file,hash).second;}
        int wavs=0;
        for(const auto& file:source.findChildFiles(juce::File::findFilesAndDirectories,true,"*",juce::File::FollowSymlinks::no))
        {
            if(!containedFile(file,root)||file.isSymbolicLink()){valid=false;break;}
            if(!file.hasFileExtension("wav")||file.isDirectory())continue;
            ++wavs;const auto found=hashes.find(file.getRelativePathFrom(source).replaceCharacter('\\','/'));
            if(found==hashes.end()||juce::SHA256(file).toHexString()!=found->second){valid=false;break;}
        }
        if(valid&&wavs==int(hashes.size())&&target.getParentDirectory().createDirectory().wasOk()
           &&containedFile(target,root))source.moveFileTo(target);
    }catch(const std::exception&){/* Malformed optional retirement data keeps the user's collection. */}
}
inline juce::File resolveCabinetFile(const juce::File& requested,const juce::File& root,const State* expected)
{
    if(requested.exists()||!expected||expected->empty()||!valid(*expected)||!containedFile(requested,root))return requested;
    // Saved rigs and recordings identify audio by SHA-256. A physical move must
    // not lose its exact sound, lock, or render/export dependency.
    auto files=root.findChildFiles(juce::File::findFiles,true,"*",juce::File::FollowSymlinks::no);
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getFullPathName()<b.getFullPathName();});
    for(const auto& file:files)if(file.hasFileExtension("wav")&&file.getSize()<=16*1024*1024&&containedFile(file,root)&&juce::SHA256(file).toHexString()==expected->sha256)return file;
    return requested;
}
}
