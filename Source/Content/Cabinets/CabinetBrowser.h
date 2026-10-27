#pragma once
#include "CabinetProcessor.h"
#include "Tone3000.h"
#include "Platform/AppPaths.h"
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/LibraryWidgets.h"
#include "Content/Artwork/FolderArtwork.h"
#include "Shared/UI/UiMotion138.h"
#include <json.hpp>
#include <map>
#include <set>

namespace tonegrid::cabinet
{
inline juce::Image cabinetDrawing(const juce::String& category)
{
    using namespace juce;Image image(Image::ARGB,220,200,true);Graphics g(image);
    const bool four=category.startsWith("4"),two=category.startsWith("2");
    Rectangle<float> cabinet(28,four?8.f:two?35.f:32.f,164,four?182.f:two?136.f:140.f);
    g.setColour(Colour(0xff05090a));g.fillRoundedRectangle(cabinet.translated(3,5),6);
    g.setColour(Colour(0xff585c50));g.fillRoundedRectangle(cabinet,5);auto face=cabinet.reduced(8);g.setColour(Colour(0xff202720));g.fillRect(face);
    g.setColour(Colour(0xff465044));for(int y=int(face.getY());y<face.getBottom();y+=4)g.drawHorizontalLine(y,face.getX(),face.getRight());
    const int count=four?4:two?2:1;
    for(int i=0;i<count;++i)
    {
        const float d=four?62.f:two?62.f:105.f;const float x=count==1?face.getCentreX()-d/2:face.getX()+6+(i%2)*(face.getWidth()/2);
        const float y=four?face.getY()+9+(i/2)*76:face.getCentreY()-d/2;
        g.setColour(Colour(0xff151b16));g.fillEllipse(x,y,d,d);g.setColour(Colour(0xff5a6354));g.drawEllipse(x,y,d,d,1.4f);g.drawEllipse(x+8,y+8,d-16,d-16,.7f);g.setColour(Colour(0xff343c31));g.fillEllipse(x+d*.36f,y+d*.36f,d*.28f,d*.28f);
    }
    g.setColour(Colour(0xffd8d4bb));g.setFont(ui::font(11));g.drawText(category,cabinet.withHeight(15),Justification::centred);return image;
}
inline juce::Image cabinetPicture(const juce::String& category)
{
    return cabinetDrawing(category);
}
struct Entry {State state;juce::String name,category="Other / Unknown",creator,source,mic,pack;juce::Image image;};
template<class Decode>
void presentEntry(Entry& entry,const juce::File& file,const juce::File& root,const juce::File& bundle,const nlohmann::json* record,std::map<juce::String,nlohmann::json>& imports,Decode&& decode)
{
    if(record)
    {
        const auto& j=*record;
        entry.name=juce::String(j.value("name",entry.name.toStdString()));entry.category=juce::String(j.value("category",std::string("Other / Unknown")));
        entry.creator=juce::String(j.value("creator",std::string{}));entry.source=juce::String(j.value("source",std::string{}));entry.mic=juce::String(j.value("mic",std::string{}));
        entry.pack=juce::String(j.value("pack",std::string{}));const juce::String image(j.value("image",std::string{}));if(safeRelative(image))entry.image=decode(bundle.getChildFile(image));
    }
    else for(const auto* category:{"1x8","1x10","1x12","2x10","2x12","4x10","4x12"})
        if(entry.name.containsIgnoreCase(category)){entry.category=category;break;}
    bool explicitArtwork=false;
    // Match NAM precedence: capture-specific image, sidecar image, then pack thumbnail.
    for(const auto* ext:{".png",".jpg",".jpeg"})if(auto image=decode(file.withFileExtension(ext));image.isValid()){entry.image=image;explicitArtwork=true;break;}
    const auto folder=file.getParentDirectory();const auto sidecar=folder.getChildFile(".tonegrid-cabinet.json");
    if(sidecar.existsAsFile()&&!sidecar.isSymbolicLink()&&sidecar.getSize()<1024*1024)
    {
        auto it=imports.find(folder.getFullPathName());if(it==imports.end())
        {nlohmann::json j;try{j=nlohmann::json::parse(sidecar.loadFileAsString().toStdString());}catch(...){}it=imports.emplace(folder.getFullPathName(),std::move(j)).first;}
        const auto& j=it->second;if(j.is_object())
        {
            entry.pack=juce::String(j.value("title",std::string{}));entry.creator=juce::String(j.value("creator",std::string{}));entry.source=juce::String(j.value("source",std::string{}));
            const juce::String name(j.value("image",std::string{}));if(!explicitArtwork&&name.isNotEmpty()&&!name.containsAnyOf("/\\:"))
            {entry.image=decode(folder.getChildFile(name));explicitArtwork=entry.image.isValid();}
        }
    }
    const auto folderImage=libraryui::folderArtwork(folder,root,decode,[](const juce::File&){return juce::Image{};});
    if(!explicitArtwork&&folderImage.isValid())entry.image=folderImage;
    if(!entry.image.isValid())entry.image=cabinetPicture(entry.category);
}
class Library
{
public:
    Library(juce::File user,juce::File shipped):root(std::move(user)),bundle(std::move(shipped)){}
    // Loading-thread preview for one already validated IR. The full library
    // scan uses the same presentation policy and remains independent of audio.
    Entry previewSelected(const State& state)const
    {
        Entry entry;entry.state=state;const auto file=root.getChildFile(state.file);entry.name=file.getFileNameWithoutExtension();
        if(!valid(state)||!containedFile(file,root))return entry;
        nlohmann::json metadata;std::map<juce::String,nlohmann::json> imports;
        const auto catalog=bundle.getChildFile("catalog.json");
        try
        {
            if(catalog.existsAsFile()&&!catalog.isSymbolicLink()&&catalog.getSize()<8*1024*1024)
            {
                const auto doc=nlohmann::json::parse(catalog.loadFileAsString().toStdString());
                for(const auto& record:doc.at("irs"))
                    if(juce::String(record.value("file",std::string{}))==state.file&&juce::String(record.value("sha256",std::string{}))==state.sha256){metadata=record;break;}
            }
        }
        catch(const std::exception&){ /* Optional presentation never invalidates a verified IR. */ }
        const auto decode=[&](const juce::File& imageFile)->juce::Image
        {
            if((!containedFile(imageFile,root)&&!containedFile(imageFile,bundle))||!tonegrid::t3k::noLinks(imageFile))return {};
            auto image=tonegrid::t3k::boundedImage(imageFile);
            if(!image.isValid())return {};
            const double scale=std::min(1.,1024./std::max(image.getWidth(),image.getHeight()));
            return juce::SoftwareImageType().convert(image).rescaled(juce::jmax(1,juce::roundToInt(image.getWidth()*scale)),juce::jmax(1,juce::roundToInt(image.getHeight()*scale)),juce::Graphics::highResamplingQuality);
        };
        try{presentEntry(entry,file,root,bundle,metadata.is_object()?&metadata:nullptr,imports,decode);}
        catch(const std::exception&)
        {
            entry.name=file.getFileNameWithoutExtension();
            entry.image=libraryui::folderArtwork(file.getParentDirectory(),root,decode,[](const juce::File&){return juce::Image{};});
        }
        return entry;
    }
    void scan(bool seed)
    {
        ++scans;entries.clear();folders.clear();folderImages.clear();aliases.clear();identities.clear();recommendations.clear();defaultFile.clear();
        const auto migrationMarker=root.getChildFile(".tonegrid-cabinet-folders-v1");const bool migrate=seed&&!migrationMarker.exists();
        std::map<juce::String,nlohmann::json> metadata,byHash,imports;std::map<juce::String,juce::Image> images;
        try
        {
            const auto manifest=bundle.getChildFile("catalog.json");
            if(manifest.existsAsFile()&&manifest.getSize()<8*1024*1024)
            {
                const auto j=nlohmann::json::parse(manifest.loadFileAsString().toStdString());
                defaultFile=juce::String(j.value("defaultIR",std::string{}));
                for(const auto& record:j.at("irs"))
                {
                    const juce::String file(record.at("file").get<std::string>());if(!safeRelative(file))continue;
                    const juce::String hash(record.at("sha256").get<std::string>());metadata.emplace(file,record);byHash.emplace(hash,record);identities.emplace(file,hash);
                    const auto target=root.getChildFile(file),source=bundle.getChildFile(file);
                    juce::StringArray legacyPaths;legacyPaths.add(juce::String(record.value("legacyFile",std::string{})));
                    if(auto prior=record.find("legacyFiles");prior!=record.end()&&prior->is_array())for(const auto& name:*prior)if(name.is_string())legacyPaths.addIfNotAlreadyThere(juce::String(name.get<std::string>()));
                    for(const auto& legacy:legacyPaths)if(safeRelative(legacy))
                    {
                        aliases.emplace(legacy,file);const auto old=root.getChildFile(legacy);
                        if((migrate||(seed&&record.value("moveOnInstall",false)))&&old.existsAsFile()&&containedFile(old,root)&&juce::SHA256(old).toHexString()==hash)
                        {
                            retainCabinetArtwork(old.getParentDirectory(),target.getParentDirectory(),root);
                            for(const auto* ext:{".png",".jpg",".jpeg"})copyCabinetArtwork(old.withFileExtension(ext),target.withFileExtension(ext),root);
                            const auto sidecar=old.getSiblingFile(".tonegrid-cabinet.json");
                            if(sidecar.existsAsFile()&&containedFile(sidecar,root)&&sidecar.getSize()<1024*1024)try
                            {
                                const auto extra=nlohmann::json::parse(sidecar.loadFileAsString().toStdString());const juce::String image(extra.value("image",std::string{}));
                                if(image.isNotEmpty()&&!image.containsAnyOf("/\\:"))copyCabinetArtwork(old.getSiblingFile(image),target.getSiblingFile(image),root);
                            }catch(...){}
                            if(relocateCabinetFile(old,target,root)&&record.value("moveOnInstall",false))pruneEmptyCabinetParents(old.getParentDirectory(),root);
                        }
                    }
                    if(seed&&!target.exists()&&source.existsAsFile()&&!source.isSymbolicLink()&&source.getSize()<=16*1024*1024&&juce::SHA256(source).toHexString()==hash)
                        if(target.getParentDirectory().createDirectory().wasOk()&&t3k::noLinks(target.getParentDirectory()))source.copyFileTo(target);
                }
                if(migrate&&j.value("physicalFolders",false))
                {
                    unwrapLegacyCabinetFolders(root);
                    if(root.createDirectory().wasOk()&&containedFile(migrationMarker,root))migrationMarker.replaceWithText("1\n");
                }
                if(seed&&j.contains("retiredCollections"))retireCabinetCollections(root,j.at("retiredCollections"));
                // Install missing thumbnails only; user artwork always survives.
                if(seed&&j.contains("assets"))for(const auto& asset:j.at("assets"))
                {
                    const juce::String file(asset.value("file",std::string{}));if(!safeRelative(file))continue;
                    const auto source=bundle.getChildFile(file),target=root.getChildFile(file);
                    if(target.getFileName()=="thumbnail.jpg")
                    {
                        const auto png=target.withFileExtension("png");
                        if(png.existsAsFile()&&t3k::noLinks(png))
                        {
                            const auto hash=juce::SHA256(png).toHexString();bool stock=false;
                            if(j.contains("retiredAssets"))for(const auto& old:j.at("retiredAssets"))stock|=juce::String(old.value("sha256",std::string{}))==hash;
                            if(!stock)continue;
                        }
                    }
                    if(!target.exists()&&t3k::noLinks(source)&&source.existsAsFile()&&source.getSize()<=8*1024*1024&&juce::SHA256(source).toHexString()==juce::String(asset.value("sha256",std::string{})))
                        if(target.getParentDirectory().createDirectory().wasOk()&&t3k::noLinks(target.getParentDirectory()))source.copyFileTo(target);
                }
                // Retire only byte-identical former stock PNGs after their JPEG
                // replacements are installed. Customized artwork is never removed.
                if(seed&&j.contains("retiredAssets"))for(const auto& asset:j.at("retiredAssets"))
                {
                    const juce::String file(asset.value("file",std::string{})),hash(asset.value("sha256",std::string{}));
                    const bool obsolete=asset.value("obsoleteCentralArtwork",false)&&file.startsWith("Artwork/");
                    if(!safeRelative(file)||(!obsolete&&!file.endsWithIgnoreCase(".png"))||hash.length()!=64)continue;
                    const auto original=root.getChildFile(file),replacement=original.withFileExtension("jpg");
                    if((obsolete||replacement.existsAsFile())&&t3k::noLinks(original)&&original.existsAsFile()&&juce::SHA256(original).toHexString()==hash)
                    {if(original.deleteFile()&&obsolete)pruneEmptyCabinetParents(original.getParentDirectory(),root);}
                }
                for(auto i=j.at("recommendations").begin();i!=j.at("recommendations").end();++i)
                    if(i.value().is_string())recommendations.emplace(juce::String(i.key()),juce::String(i.value().get<std::string>()));
            }
        }
        catch(...){metadata.clear();recommendations.clear();defaultFile.clear();}
        if(!root.isDirectory())return;
        const auto decode=[&](const juce::File& file)->juce::Image
        {
            const auto key=file.getFullPathName();auto it=images.find(key);if(it==images.end())
            {
                auto image=t3k::noLinks(file)?t3k::boundedImage(file):juce::Image{};
                if(image.isValid()){const auto scale=std::min(1.,1024./std::max(image.getWidth(),image.getHeight()));image=juce::SoftwareImageType().convert(image).rescaled(juce::jmax(1,juce::roundToInt(image.getWidth()*scale)),juce::jmax(1,juce::roundToInt(image.getHeight()*scale)),juce::Graphics::highResamplingQuality);}
                it=images.emplace(key,image).first;
            }
            return it->second;
        };
        std::set<juce::String> seen;
        for(const auto& file:root.findChildFiles(juce::File::findFilesAndDirectories,true,"*",juce::File::FollowSymlinks::no))
        {
            if(!containedFile(file,root))continue;
            if(retiredCabinetPath(file.getRelativePathFrom(root).replaceCharacter('\\','/')))continue;
            if(file.isDirectory()){folders.insert(file.getRelativePathFrom(root).replaceCharacter('\\','/'));continue;}
            if(!file.hasFileExtension("wav")||file.isSymbolicLink()||file.getSize()>16*1024*1024)continue;
            const auto key=file.getFullPathName();seen.insert(key);auto& cached=checksums[key];const auto size=file.getSize(),stamp=file.getLastModificationTime().toMilliseconds();
            if(cached.sha.isEmpty()||cached.size!=size||cached.stamp!=stamp){cached={size,stamp,juce::SHA256(file).toHexString()};++hashes;}
            Entry entry;entry.state={file.getRelativePathFrom(root).replaceCharacter('\\','/'),cached.sha,false};
            if(!valid(entry.state))continue;entry.name=file.getFileNameWithoutExtension();
            auto it=metadata.find(entry.state.file);
            if(it==metadata.end())if(auto hash=byHash.find(entry.state.sha256);hash!=byHash.end())
                if(!root.getChildFile(juce::String(hash->second.at("file").get<std::string>())).exists())it=metadata.emplace(entry.state.file,hash->second).first;
            const auto* record=it!=metadata.end()&&juce::String(it->second.value("sha256",std::string{}))==entry.state.sha256?&it->second:nullptr;
            presentEntry(entry,file,root,bundle,record,imports,decode);
            entries.push_back(std::move(entry));
        }
        std::erase_if(checksums,[&](const auto& cached){return !seen.contains(cached.first);});
        for(const auto& folder:folders)if(auto image=libraryui::folderArtwork(root.getChildFile(folder),root,decode,[](const juce::File&){return juce::Image{};});image.isValid())folderImages.emplace(folder,image);
        std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.name.compareIgnoreCase(b.name)<0;});
    }
    void publish(Library&& other)
    {jassert(root==other.root&&bundle==other.bundle);entries=std::move(other.entries);folders=std::move(other.folders);folderImages=std::move(other.folderImages);aliases=std::move(other.aliases);identities=std::move(other.identities);recommendations=std::move(other.recommendations);defaultFile=std::move(other.defaultFile);checksums=std::move(other.checksums);scans=other.scans;hashes=other.hashes;}
    const Entry* find(const juce::String& file)const
    {
        for(const auto& e:entries)if(e.state.file==file)return &e;
        const auto alias=aliases.find(file);const auto target=alias==aliases.end()?file:alias->second;
        if(target!=file)for(const auto& e:entries)if(e.state.file==target)if(auto identity=identities.find(target);identity!=identities.end()&&e.state.sha256==identity->second)return &e;
        if(auto identity=identities.find(target);identity!=identities.end())for(const auto& e:entries)if(e.state.sha256==identity->second)return &e;return nullptr;
    }
    const Entry* find(const State& state)const{if(const auto* e=find(state.file);e&&e->state.sha256==state.sha256)return e;for(const auto& e:entries)if(e.state.sha256==state.sha256)return &e;return nullptr;}
    const Entry* declared(const juce::String& file)const{auto identity=identities.find(file);return identity==identities.end()?nullptr:find(State{file,identity->second,false});}
    const Entry* recommended(const juce::String& hash)const{auto it=recommendations.find(hash);return it==recommendations.end()?nullptr:declared(it->second);}
    const Entry* fallback()const{return declared(defaultFile);}
    const juce::File root,bundle;std::vector<Entry> entries;std::set<juce::String> folders;std::map<juce::String,juce::Image> folderImages;unsigned scans=0,hashes=0;
private:struct Checksum{juce::int64 size=0,stamp=0;juce::String sha;};std::map<juce::String,Checksum> checksums;juce::String defaultFile;std::map<juce::String,juce::String> recommendations,aliases,identities;
};
class Panel final:public libraryui::GearCard
{
    class Power final:public juce::Button
    {
    public:
        Power():Button("Cabinet simulation power")
        {setClickingTogglesState(true);setComponentID("cabinet-power");l10n::tooltip(*this,"Turn off the cabinet simulation when using a real guitar cabinet. Your amp stays active. Click again to restore the selected IR. Available with Head Only captures.");}
        void paintButton(juce::Graphics& g,bool hover,bool down)override
        {
            auto r=getLocalBounds().toFloat().reduced(.5f);g.setColour(juce::Colour(down?0xff243c4b:0xff18252d));g.fillRoundedRectangle(r,5);
            g.setColour(isEnabled()?(getToggleState()?ui::tabBlue():ui::secondaryInk()):ui::outline());g.drawRoundedRectangle(r,5,hover?1.3f:.8f);
            const auto c=r.getCentre();juce::Path p;p.addCentredArc(c.x,c.y+1,7,7,0,.65f,juce::MathConstants<float>::twoPi-.65f,true);
            g.strokePath(p,juce::PathStrokeType(1.8f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));g.drawLine(c.x,c.y-9,c.x,c.y,1.8f);
        }
    };
public:
    Panel():GearCard("Cabinet")
    {
        l10n::button(load,"Load another IR");browse.setComponentID("cabinet-browse");load.setComponentID("cabinet-load");
        lock.setComponentID("cabinet-lock");lock.setClickingTogglesState(true);l10n::tooltip(lock,"Keep this IR when changing amp heads. Unlock it to follow the next head's recommended cabinet. Full Rig captures keep their included cabinet.");
        l10n::tooltip(help,"Keep this IR when changing amp heads. Unlock it to follow the next head's recommended cabinet. Full Rig captures keep their included cabinet.");help.setComponentID("cabinet-lock-help");recommended.setComponentID("cabinet-recommended");
        actions.addAndMakeVisible(lock);actions.addAndMakeVisible(help);actions.addChildComponent(recommended);setRecommended(false);
    }
    void update(const Entry* entry,const juce::String& status,bool active,bool locked,bool canBrowse,bool irOn=true)
    {setPresentation(status=="Cabinet included"?cabinetPicture("4x12"):entry?entry->image:cabinetPicture("4x12"),status=="Cabinet included"?juce::String("Cabinet included"):entry?entry->name:status,active,status=="Cabinet included"||!entry);browse.setEnabled(canBrowse);lock.setEnabled(active);help.setEnabled(active);power.setEnabled(active);power.setToggleState(irOn,juce::dontSendNotification);off=active&&!irOn;recommended.setEnabled(active);lock.setToggleState(locked,juce::dontSendNotification);repaint();}
    void paintOverChildren(juce::Graphics& g)override
    {if(off){auto r=juce::Rectangle<float>(float(getWidth()-74),7,62,17);g.setColour(juce::Colour(0xff101c25));g.fillRoundedRectangle(r,4);g.setColour(ui::ink());g.setFont(ui::font(10,juce::Font::bold));g.drawText("CAB OFF",r,juce::Justification::centred);}}
    void setRecommended(bool value)
    {recommended.setVisible(value);if(value)setRows({{&load,&power},{&lock,&help},{&recommended}});else setRows({{&load,&power},{&lock,&help}});}
    Power power;
    l10n::TextButton lock{"Lock this IR"},help{"i"},recommended{"Load recommended"};
protected:
    void paintFaceOverlay(juce::Graphics& g)override
    {if(off){g.setColour(juce::Colours::black.withAlpha(.5f));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),10);}}
private:bool off=false;
};
}
