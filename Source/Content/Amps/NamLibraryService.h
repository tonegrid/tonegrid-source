#pragma once
#include "Content/Amps/NamAmpLibrary.h"
#include "Content/Amps/FactoryAmps.h"
#include "Content/Artwork/FolderArtwork.h"
#include "Content/Amps/AmpCaptureInfo.h"
#include "Tone3000.h"
#include <future>
#include <map>
#include <set>
#include <atomic>

namespace namamp
{
// A catalogue contains metadata, never loaded neural models. All filesystem and
// JSON work runs on one worker. The UI polls and publishes completed snapshots.
class LibraryService final
{
    struct Cached {juce::int64 size=0,time=0;Entry entry;juce::String failure;tonegrid::libraryui::Capture presentation;};
    using Index=std::map<juce::String,Cached>;
    struct Artwork {juce::int64 size=0,time=0;juce::Image image;};
    using ArtworkIndex=std::map<juce::String,Artwork>;
public:
    struct Scan {Catalog catalog;Index index;ArtworkIndex artwork;juce::Result result=juce::Result::ok();unsigned generation=0;int parsed=0,reused=0;};
    explicit LibraryService(const Library& value,juce::File factory={}):library(value),factoryFolder(std::move(factory)){}
    ~LibraryService(){cancelled.store(true);if(work.valid())work.wait();}
    bool busy() const{return work.valid();}
    void refresh()
    {
        if(busy()){repeat=true;return;}
        start();
    }
    bool poll(Catalog& output,juce::Result& status)
    {
        if(!work.valid()||work.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        auto scan=work.get();const bool current=scan.generation==generation;
        if(current&&scan.result.wasOk()){cache=std::move(scan.index);artCache=std::move(scan.artwork);output=std::move(scan.catalog);lastParsed=scan.parsed;lastReused=scan.reused;}
        status=scan.result;
        if(repeat){repeat=false;start();}
        return current;
    }
    // Imported entries were already validated in staging; merge without rereading
    // the library. In-flight snapshots are discarded and reconciled afterwards.
    void merge(const std::vector<Entry>& entries,Catalog& catalog)
    {
        ++generation;
        for(const auto& entry:entries)
        {
            const auto file=library.folder().getChildFile(entry.identity.file);
            cache[entry.identity.file]={-1,file.getLastModificationTime().toMilliseconds(),entry,{}};
            auto old=std::find_if(catalog.entries.begin(),catalog.entries.end(),[&](const auto& e){return e.identity.file==entry.identity.file;});
            if(old==catalog.entries.end())catalog.entries.push_back(entry);else *old=entry;
            std::erase_if(catalog.failures,[&](const auto& f){return f.file==entry.identity.file;});
        }
        sort(catalog);refresh();
    }
    int parsedCount()const{return lastParsed;}int reusedCount()const{return lastReused;}
private:
    static void sort(Catalog& value){std::sort(value.entries.begin(),value.entries.end(),[](const auto& a,const auto& b){return a.identity.file.compareIgnoreCase(b.identity.file)<0;});}
    void start()
    {
        const auto snapshot=cache;const auto previousArtwork=artCache;const auto revision=generation;const auto root=library.folder();const auto factory=factoryFolder;
        work=std::async(std::launch::async,[snapshot,previousArtwork,revision,root,factory,stop=&cancelled]
        {
            Scan scan;scan.generation=revision;Library reader(root);scan.catalog.browser=std::make_shared<tonegrid::libraryui::Index>();

            try
            {
                scan.result=root.createDirectory();if(scan.result.failed())return scan;
                if(factory!=juce::File{}){const auto seeded=tonegrid::factory::seed(factory,root,*stop);if(seeded.failed())scan.catalog.failures.push_back({"Factory amps",seeded.getErrorMessage()});}
                const tonegrid::libraryui::AmpCaptureInfo captureInfo(factory);
                nlohmann::json typeOverrides=nlohmann::json::object();
                const auto typesFile=root.getChildFile(".tonegrid-types.json");
                if(typesFile.existsAsFile()&&!typesFile.isSymbolicLink()&&typesFile.getSize()<128*1024)try{const auto j=nlohmann::json::parse(typesFile.loadFileAsString().toStdString());if(j.is_object())typeOverrides=j;}catch(...){}
                auto decode=[&](const juce::File& imageFile)->juce::Image
                {
                    if(imageFile==juce::File{})return {};
                    const auto key=imageFile.getFullPathName();auto found=scan.artwork.find(key);
                    if(found==scan.artwork.end())
                    {
                        const auto bytes=imageFile.getSize(),modified=imageFile.getLastModificationTime().toMilliseconds();
                        auto previous=previousArtwork.find(key);Artwork art;art.size=bytes;art.time=modified;
                        if(previous!=previousArtwork.end()&&previous->second.size==bytes&&previous->second.time==modified)art.image=previous->second.image;
                        else
                        {
                            art.image=tonegrid::t3k::noLinks(imageFile)?tonegrid::t3k::boundedImage(imageFile):juce::Image{};
                            if(art.image.isValid())
                            {
                                const double scale=std::min(1.,1024./std::max(art.image.getWidth(),art.image.getHeight()));
                                art.image=juce::SoftwareImageType().convert(art.image).rescaled(juce::jmax(1,juce::roundToInt(art.image.getWidth()*scale)),juce::jmax(1,juce::roundToInt(art.image.getHeight()*scale)),juce::Graphics::highResamplingQuality);
                            }
                        }
                        found=scan.artwork.emplace(key,std::move(art)).first;
                    }
                    return found->second.image;
                };
                auto folderImage=[&](juce::File folder)->juce::Image
                {
                    return tonegrid::libraryui::folderArtwork(folder,root,decode,[](const juce::File&){return juce::Image{};});
                };
                for(const auto& file:root.findChildFiles(juce::File::findFilesAndDirectories,true,"*",juce::File::FollowSymlinks::no))
                {
                    if(stop->load()){scan.result=juce::Result::fail("NAM refresh cancelled.");return scan;}
                    if(file.isDirectory()){if(!file.isSymbolicLink())scan.catalog.browser->folders.insert(file.getRelativePathFrom(root).replaceCharacter('\\','/'));continue;}
                    if(!file.hasFileExtension("nam")||file.isSymbolicLink())continue;
                    const auto path=file.getRelativePathFrom(root).replaceCharacter('\\','/');const auto size=file.getSize(),time=file.getLastModificationTime().toMilliseconds();
                    Cached value;const auto previous=snapshot.find(path);
                    if(previous!=snapshot.end()&&previous->second.size==size&&previous->second.time==time){value=previous->second;++scan.reused;}
                    else{value.size=size;value.time=time;auto result=reader.read(file,value.entry,nullptr,&value.presentation);if(result.failed())value.failure=result.getErrorMessage();++scan.parsed;}
                    if(value.presentation.path.isEmpty())value.presentation=tonegrid::libraryui::basic(file,root);
                    auto info=value.presentation;info.error=value.failure;info.name=file.getFileNameWithoutExtension();
                    const auto manifestImage=tonegrid::libraryui::sidecar(info,file,value.entry.identity.sha256);
                    captureInfo.apply(info,value.entry.identity.sha256);
                    if(auto it=typeOverrides.find(value.entry.identity.sha256.toStdString());it!=typeOverrides.end()&&it->is_string())info.gear=juce::String(it->get<std::string>());
                    // Decode on the worker; share one bounded high-DPI image between
                    // folder rows, capture rows and the details preview.
                    std::vector<juce::File> images;
                    for(const auto* ext:{".png",".jpg",".jpeg"})images.push_back(file.withFileExtension(ext));
                    if(manifestImage.isNotEmpty())images.push_back(file.getSiblingFile(manifestImage));
                    for(const auto& imageFile:images)
                        if(auto img=decode(imageFile);img.isValid()){info.artwork=img;break;}
                    if(!info.artwork.isValid())info.artwork=folderImage(file.getParentDirectory());
                    scan.catalog.browser->includeParents(tonegrid::libraryui::parentOf(path));
                    scan.catalog.browser->captures.emplace(path,std::move(info));
                    if(value.failure.isEmpty())scan.catalog.entries.push_back(value.entry);else scan.catalog.failures.push_back({path,value.failure});
                    scan.index.emplace(path,std::move(value));
                }
                for(const auto& folder:scan.catalog.browser->folders)
                    if(auto img=folderImage(root.getChildFile(folder));img.isValid())
                        scan.catalog.browser->folderArtwork.emplace(folder,img);
                sort(scan.catalog);
            }
            catch(const std::exception& error){scan.result=juce::Result::fail("NAM refresh failed: "+juce::String(error.what()));}
            catch(...){scan.result=juce::Result::fail("NAM refresh failed.");}
            return scan;
        });
    }
    const Library library;const juce::File factoryFolder;Index cache;ArtworkIndex artCache;std::future<Scan> work;std::atomic<bool> cancelled{false};unsigned generation=0;bool repeat=false;int lastParsed=0,lastReused=0;
};
}
