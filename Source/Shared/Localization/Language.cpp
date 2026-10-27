#include "Language.h"
#include <ToneGridApplicationAssets.h>
#include <set>
namespace tonegrid::l10n
{
namespace
{
bool safeLocale(const juce::String& value)
{return value.isNotEmpty()&&value.length()<40&&value.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")&&!value.startsWithChar('-');}
std::set<juce::String> tokens(const juce::String& text)
{
    std::set<juce::String> result;
    for(int i=0;i<text.length();++i)if(text[i]=='{'){const auto end=text.indexOf(i+1,"}");if(end<0)break;result.insert(text.substring(i,end+1));i=end;}
    return result;
}
juce::String quoted(juce::String s)
{return "\""+s.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";}
}
Language& Language::instance(){static Language language;return language;}
Language::Language()
{
    decode(juce::String::fromUTF8(ToneGridApplicationAssets::en_json,ToneGridApplicationAssets::en_jsonSize),english);
    for(const auto& item:english.strings)keysByEnglish.emplace(item.second,item.first);
    packs.emplace("en",english);
}
bool Language::decode(const juce::String& source,Pack& target)
{
    juce::var value;if(juce::JSON::parse(source,value).failed())return false;
    auto* object=value.getDynamicObject();if(!object)return false;
    const auto version=object->getProperty("schemaVersion");
    if(!(version.isInt()||version.isInt64())||static_cast<juce::int64>(version)!=1)return false;
    const auto locale=object->getProperty("locale"),name=object->getProperty("nativeName");
    auto* strings=object->getProperty("strings").getDynamicObject();
    if(!locale.isString()||!safeLocale(locale.toString())||!name.isString()||name.toString().trim().isEmpty()||name.toString().length()>80||!strings)return false;
    Pack next;next.locale=locale.toString();next.nativeName=name.toString();
    for(const auto& item:strings->getProperties())if(item.value.isString()&&item.value.toString().trim().isNotEmpty()&&item.value.toString().length()<=16000)
        next.strings.emplace(item.name.toString(),item.value.toString());
    target=std::move(next);return true;
}
void Language::configure(juce::File shipped,juce::File editable,juce::File preference,juce::String systemLanguage)
{
    shippedFolder=std::move(shipped);userFolder=std::move(editable);preferenceFile=std::move(preference);
    packs.clear();packs.emplace("en",english);
    loadPacks();
    juce::var preferenceValue;juce::String choice;
    if(preferenceFile.existsAsFile()&&preferenceFile.getSize()<16384&&juce::JSON::parse(preferenceFile.loadFileAsString(),preferenceValue).wasOk())
        if(auto* object=preferenceValue.getDynamicObject())if(object->getProperty("locale").isString())choice=object->getProperty("locale").toString();
    selected=packs.count(choice)?choice:chooseSystem(systemLanguage);
    installJuceTranslations();notify();
}
void Language::loadPacks()
{
    const auto previous=packs;packs.clear();packs.emplace("en",english);
    const auto read=[&](const juce::File& folder,bool overridePack)
    {
        auto files=folder.findChildFiles(juce::File::findFiles,false,"*.json");files.sort();
        for(const auto& file:files)
        {
            if(file.isSymbolicLink()||file.getSize()>2*1024*1024)continue;
            Pack pack;if(!decode(file.loadFileAsString(),pack)||pack.locale!=file.getFileNameWithoutExtension())
            {
                // A malformed edit during reload keeps that locale's last valid
                // pack; a fresh launch retains the shipped pack or English.
                const auto old=previous.find(file.getFileNameWithoutExtension());
                if(overridePack&&old!=previous.end())packs[old->first]=old->second;
                continue;
            }
            const auto base=packs.find(pack.locale);Pack merged=base!=packs.end()?base->second:Pack{pack.locale,pack.nativeName,{}};
            merged.nativeName=pack.nativeName;
            for(const auto& item:pack.strings)
            {
                const auto reference=english.strings.find(item.first);
                if(reference==english.strings.end()||tokens(reference->second)!=tokens(item.second))continue;
                merged.strings[item.first]=item.second;
            }
            packs[pack.locale]=std::move(merged);
        }
    };
    read(shippedFolder,false);read(userFolder,true);
    // Editable copies are seeded once, never overwritten. Updates contribute
    // new keys through the shipped pack; missing user keys keep that fallback.
    if(userFolder!=juce::File{}&&userFolder.createDirectory().wasOk())
        for(const auto& file:shippedFolder.findChildFiles(juce::File::findFiles,false,"*.json"))
            if(!file.isSymbolicLink()&&!userFolder.getChildFile(file.getFileName()).exists())file.copyFileTo(userFolder.getChildFile(file.getFileName()));
}
juce::String Language::chooseSystem(const juce::String& value)const
{
    const auto language=value.replaceCharacter('_','-').toLowerCase();
    for(const auto& p:packs)if(p.first.equalsIgnoreCase(language))return p.first;
    const auto base=language.upToFirstOccurrenceOf("-",false,false);
    if(base=="zh"&&packs.count("zh-Hans"))return "zh-Hans";
    if(base=="pt"&&packs.count("pt-BR"))return "pt-BR";
    for(const auto& p:packs)if(p.first.upToFirstOccurrenceOf("-",false,false)==base)return p.first;
    return "en";
}
bool Language::select(const juce::String& locale,bool persist)
{
    if(!packs.count(locale))return false;
    if(persist&&preferenceFile!=juce::File{})
    {
        auto* object=new juce::DynamicObject;object->setProperty("schemaVersion",1);object->setProperty("locale",locale);
        if(preferenceFile.getParentDirectory().createDirectory().wasOk())
        {juce::TemporaryFile temp(preferenceFile);if(temp.getFile().replaceWithText(juce::JSON::toString(juce::var(object))) )temp.overwriteTargetFileWithTemporary();}
        else delete object;
    }
    if(selected==locale)return true;
    selected=locale;installJuceTranslations();notify();return true;
}
void Language::reload(){loadPacks();if(!packs.count(selected))selected="en";installJuceTranslations();notify();}
juce::String Language::text(const juce::String& key)const
{
    if(auto p=packs.find(selected);p!=packs.end())if(auto s=p->second.strings.find(key);s!=p->second.strings.end())return s->second;
    const auto fallback=english.strings.find(key);return fallback!=english.strings.end()?fallback->second:key;
}
juce::String Language::translate(const juce::String& source)const
{
    // A shared music term, including popup titles. Older editable packs may
    // still contain its former translation; do not let them rename the term.
    if(source=="Backingtrack")return source;
    const auto found=keysByEnglish.find(source);return found==keysByEnglish.end()?source:text(found->second);
}
juce::String Language::source(const juce::String& key)const
{const auto found=english.strings.find(key);return found==english.strings.end()?key:found->second;}
std::vector<Pack> Language::available()const
{
    std::vector<Pack> result;for(const auto& p:packs)result.push_back({p.first,p.second.nativeName,{}});return result;
}
size_t Language::listen(std::function<void()> callback){const auto id=++nextListener;listeners[id]=std::move(callback);return id;}
void Language::unlisten(size_t id){listeners.erase(id);}
void Language::notify()
{
    std::vector<size_t> ids;for(const auto& item:listeners)ids.push_back(item.first);
    for(auto id:ids)if(auto found=listeners.find(id);found!=listeners.end()){const auto action=found->second;action();}
}
void Language::installJuceTranslations()
{
    juce::String mapping="language: "+selected+"\ncountries: \n";
    for(const auto& item:english.strings)mapping+=quoted(item.second)+" = "+quoted(text(item.first))+"\n";
    juce::LocalisedStrings::setCurrentMappings(new juce::LocalisedStrings(mapping,false));
}
}
