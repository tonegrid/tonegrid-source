#pragma once
#include "Content/AtomicStorage.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_cryptography/juce_cryptography.h>
#include <json.hpp>
#include <map>
#include <set>
#include "CaptureKind.h"
#include "NamAmpState.h"
#include <cmath>

namespace tonegrid::libraryui
{
// Presentation only: no model construction, electrical calibration or DSP state.
struct Capture
{
    juce::String path,name,author,architecture,version,date,make,model,gear,tone,description,error,creatorUsername,sourceUrl;
    juce::StringArray tags;
    juce::int64 bytes=0;
    double sampleRate=0;
    juce::Image artwork;
    juce::String parent() const{return path.containsChar('/')?path.upToLastOccurrenceOf("/",false,false):juce::String{};}
    juce::String searchable() const{return path+" "+name+" "+author+" "+make+" "+model+" "+gear+" "+tone+" "+tags.joinIntoString(" ");}
};
inline juce::String parentOf(const juce::String& path)
{return path.containsChar('/')?path.upToLastOccurrenceOf("/",false,false):juce::String{};}
inline juce::String leafOf(const juce::String& path){return path.fromLastOccurrenceOf("/",false,false);}
inline juce::String categoryOf(const Capture& c)
{const auto parent=c.parent();return parent.isEmpty()?"Library root":parent.upToFirstOccurrenceOf("/",false,false);}
inline juce::String captureContext(const Capture& c)
{return categoryOf(c)+(c.author.isEmpty()?juce::String{}:" by "+c.author);}
// Display filtering only: retain source descriptions and NAM files verbatim.
// Keep ordinary numbers/units and turn enclosed Latin letters into plain text,
// so an emoji-styled A2 label still says A2 after removing its decoration.
inline juce::String plainDescription(const juce::String& source)
{
    juce::String result;
    for(auto c:source)
    {
        if((c>=0x1f130&&c<=0x1f149)||(c>=0x1f150&&c<=0x1f169)||(c>=0x1f170&&c<=0x1f189))
        {result+=juce::String::charToString('A'+(c>=0x1f170?c-0x1f170:c>=0x1f150?c-0x1f150:c-0x1f130));continue;}
        const bool musical=c==0x266d||c==0x266e||c==0x266f;
        if((c>=0x1f000&&c<=0x1faff)||(!musical&&c>=0x2600&&c<=0x27bf)
            ||c==0x200d||c==0xfe0e||c==0xfe0f||c==0x20e3||(c>=0xe0020&&c<=0xe007f)
            ||c==0x203c||c==0x2049||c==0x2139||(c>=0x2194&&c<=0x2199)||c==0x21a9||c==0x21aa
            ||c==0x2328||c==0x23cf||c==0x24c2||c==0x25aa||c==0x25ab||c==0x25b6||c==0x25c0||(c>=0x25fb&&c<=0x25fe)
            ||c==0x2934||c==0x2935||(c>=0x2b05&&c<=0x2b07)||c==0x231a||c==0x231b||(c>=0x23e9&&c<=0x23f3)||(c>=0x23f8&&c<=0x23fa)
            ||c==0x2b50||c==0x2b55||c==0x2b1b||c==0x2b1c||c==0x3030||c==0x303d||c==0x3297||c==0x3299)continue;
        result+=juce::String::charToString(c);
    }
    juce::StringArray lines;bool blank=true;
    for(auto line:juce::StringArray::fromLines(result))
    {line=line.trim();if(line.isEmpty()){if(!blank)lines.add({});blank=true;}else{lines.add(line);blank=false;}}
    return lines.joinIntoString("\n").trim();
}
inline juce::String text(const nlohmann::json& json,const char* key)
{auto p=json.find(key);return p!=json.end()&&p->is_string()?juce::String(p->get<std::string>()).trim().substring(0,256*1024):juce::String{};}
inline juce::String known(const juce::String& value)
{const auto s=value.trim();return juce::StringArray{"null","unknown","n/a","none","t3k-null","t3k-unset","unset"}.contains(s,true)?juce::String{}:s;}
inline juce::String tonePage(const juce::String& address)
{
    const juce::URL url(address);
    const auto authority=address.fromFirstOccurrenceOf("://",false,false).upToFirstOccurrenceOf("/",false,false);
    return url.isWellFormed()&&url.getScheme().equalsIgnoreCase("https")
        &&(url.getDomain().equalsIgnoreCase("tone3000.com")||url.getDomain().equalsIgnoreCase("www.tone3000.com"))
        &&(url.getPort()==0||url.getPort()==443)&&!authority.containsChar('@')&&!address.containsAnyOf("\r\n")
        &&url.getSubPath().startsWith("tones/")?address:juce::String{};
}
inline juce::String authorSearch(const Capture& c)
{
    if(c.author.isEmpty())return {};
    const auto username=c.creatorUsername.isNotEmpty()?c.creatorUsername:c.author;
    const bool handle=username.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.");
    return "https://www.tone3000.com/search?"+juce::String(handle?"creators=":"q=")+juce::URL::addEscapeChars(username,true);
}
inline Capture basic(const juce::File& file,const juce::File& root)
{Capture c;c.path=file.getRelativePathFrom(root).replaceCharacter('\\','/');c.name=file.getFileNameWithoutExtension();c.bytes=file.getSize();return c;}
inline void embedded(Capture& c,const nlohmann::json& json)
{
    if(!json.is_object())return;
    c.architecture=text(json,"architecture");c.version=text(json,"version");
    if(auto p=json.find("sample_rate");p!=json.end()&&p->is_number())
    {const auto rate=p->get<double>();if(std::isfinite(rate)&&rate>0&&rate<1000000)c.sampleRate=rate;}
    auto p=json.find("metadata");if(p==json.end()||!p->is_object())return;const auto& m=*p;
    c.author=text(m,"modeled_by");if(c.author.isEmpty())c.author=text(m,"author");
    c.make=known(text(m,"gear_make"));c.model=known(text(m,"gear_model"));c.gear=text(m,"gear_type");c.tone=text(m,"tone_type");c.description=known(text(m,"description"));
    if(auto d=m.find("date");d!=m.end())
    {
        if(d->is_string())c.date=juce::String(d->get<std::string>()).substring(0,80);
        else if(d->is_object())
        {
            auto number=[&](const char* key){auto v=d->find(key);return v!=d->end()&&v->is_number_integer()?v->get<int>():0;};
            const auto y=number("year"),mo=number("month"),day=number("day");
            if(y>=1900&&y<=9999&&mo>=1&&mo<=12&&day>=1&&day<=31)c.date=juce::String::formatted("%04d-%02d-%02d",y,mo,day);
        }
    }
    if(auto t=m.find("tags");t!=m.end()&&t->is_array())for(const auto& tag:*t)if(tag.is_string()&&c.tags.size()<20)c.tags.add(juce::String(tag.get<std::string>()).substring(0,80));
}
// Same local manifest written by the existing Tone3000 importer. Only a matching
// model entry can override embedded data; unrelated sibling manifests are ignored.
inline juce::String sidecar(Capture& c,const juce::File& file,const juce::String& sha256={})
{
    const auto manifest=file.getSiblingFile(".tonegrid-tone3000.json");
    if(!manifest.existsAsFile()||manifest.isSymbolicLink()||manifest.getSize()>1024*1024)return {};
    try
    {
        const auto json=nlohmann::json::parse(manifest.loadFileAsString().toStdString());
        if(text(json,"source")!="tone3000")return {};
        auto models=json.find("models");if(models==json.end()||!models->is_array())return {};
        for(const auto& entry:*models)if((sha256.isNotEmpty()&&text(entry,"sha256")==sha256)
            ||(text(entry,"file")==file.getFileName()&&(text(entry,"sha256").isEmpty()||text(entry,"sha256")==sha256)))
        {
            auto author=text(json,"creator");if(author.isNotEmpty())c.author=author;
            c.creatorUsername=text(json,"creator_username");c.sourceUrl=tonePage(text(json,"url"));
            for(auto pair:{std::pair{"maker",&c.make},std::pair{"model",&c.model},std::pair{"description",&c.description}})
                if(auto value=known(text(json,pair.first));value.isNotEmpty())*pair.second=value;
            if(auto gear=text(json,"gear");gear.isNotEmpty())c.gear=gear;
            if(auto gear=text(entry,"gear");gear.isNotEmpty())c.gear=gear;
            auto image=text(json,"image");return image.isNotEmpty()&&!image.containsAnyOf("/\\:")&&image!="."&&image!=".."?image:juce::String{};
        }
    }
    catch(...){ }
    return {};
}
struct Index
{
    std::map<juce::String,Capture> captures;
    std::set<juce::String> folders;
    std::map<juce::String,juce::Image> folderArtwork;
    int count(const juce::String& folder) const
    {int n=0;for(const auto& [path,c]:captures)if(folder.isEmpty()||path.startsWith(folder+"/"))++n;return n;}
    void includeParents(juce::String folder)
    {while(folder.isNotEmpty()){folders.insert(folder);folder=parentOf(folder);}}
};
// Normalized absolute paths preserve distinct physical captures. Case folding is
// Windows-only; case-sensitive macOS/Linux volumes must not collapse user files.
inline juce::String favoriteKey(const juce::File& file)
{
    auto key=file.getFullPathName().replaceCharacter('\\','/');
   #if JUCE_WINDOWS
    key=key.toLowerCase();
   #endif
    return key;
}
class Favorites
{
public:
    void load(const juce::File& location)
    {
        file=location;paths.clear();relocationStamp.clear();if(!file.existsAsFile()||file.getSize()>1024*1024)return;
        if(auto xml=juce::XmlDocument::parse(file);xml&&xml->hasTagName("NamFavorites")&&xml->getIntAttribute("version")==1)
            for(auto* node:xml->getChildIterator())if(node->hasTagName("Capture")){auto path=node->getStringAttribute("path");if(juce::File::isAbsolutePath(path))paths.insert(favoriteKey(juce::File(path)));}
    }
    bool contains(const juce::File& model) const{return paths.contains(favoriteKey(model));}
    void followFactoryMoves(const juce::File& root)
    {
        if(paths.empty())return;
        const auto map=root.getChildFile("factory-relocations.json");
        const auto stamp=map.getFullPathName()+juce::String(map.getLastModificationTime().toMilliseconds())+juce::String(map.getSize());
        if(stamp==relocationStamp)return;relocationStamp=stamp;
        if(!map.existsAsFile()||map.isSymbolicLink()||map.getSize()>2*1024*1024)return;
        try
        {
            const auto json=nlohmann::json::parse(map.loadFileAsString().toStdString());
            if(json.value("schemaVersion",0)!=1)return;
            for(const auto& move:json.at("moves"))
            {
                const juce::String from(move.at("from").get<std::string>()),to(move.at("to").get<std::string>());
                if(!namamp::validRelativePath(from)||!namamp::validRelativePath(to))continue;
                const auto old=root.getChildFile(from),next=root.getChildFile(to);const auto key=favoriteKey(old);
                if(paths.contains(key)&&!old.exists()&&next.existsAsFile()&&!next.isSymbolicLink()
                    &&next.getSize()==move.at("bytes").get<juce::int64>()&&juce::SHA256(next).toHexString().toStdString()==move.at("sha256").get<std::string>())
                {paths.erase(key);paths.insert(favoriteKey(next));}
            }
        }catch(...){}
        // Preserve the personal file until the user changes a favorite.
    }
    juce::Result toggle(const juce::File& model)
    {
        const auto previous=paths;const auto key=favoriteKey(model);if(!paths.erase(key))paths.insert(key);
        if(file==juce::File{})return juce::Result::ok();
        juce::XmlElement xml("NamFavorites");xml.setAttribute("version",1);
        for(const auto& path:paths)xml.createNewChildElement("Capture")->setAttribute("path",path);
        const auto result=storage::writeXmlAtomically(file,xml);
        if(result.failed())paths=previous;return result;
    }
private:juce::File file;std::set<juce::String> paths;juce::String relocationStamp;
};
}
