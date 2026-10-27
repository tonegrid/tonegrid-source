#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <map>
#include <vector>

namespace tonegrid::l10n
{
struct Pack
{
    juce::String locale, nativeName;
    std::map<juce::String,juce::String> strings;
};
// UI/message-thread service. No audio objects, network access or realtime calls.
// Stable semantic IDs are the on-disk contract; English lookup is a bridge for
// the existing UI. Content, diagnostics and logs never call this service.
class Language final
{
public:
    static Language& instance();
    static bool decode(const juce::String&,Pack&);
    void configure(juce::File shipped,juce::File editable,juce::File preference,
                   juce::String systemLanguage=juce::SystemStats::getDisplayLanguage());
    bool select(const juce::String& locale,bool persist=true);
    void reload();
    juce::String text(const juce::String& key) const;
    juce::String source(const juce::String& key) const;
    juce::String translate(const juce::String& english) const;
    juce::String locale() const {return selected;}
    std::vector<Pack> available() const;
    size_t listen(std::function<void()>);
    void unlisten(size_t);
    const juce::File& editableFolder()const{return userFolder;}
private:
    Language();
    void loadPacks();
    void notify();
    void installJuceTranslations();
    juce::String chooseSystem(const juce::String&)const;
    Pack english;
    std::map<juce::String,Pack> packs;
    std::map<juce::String,juce::String> keysByEnglish;
    std::map<size_t,std::function<void()>> listeners;
    juce::String selected="en";
    juce::File shippedFolder,userFolder,preferenceFile;
    size_t nextListener=0;
};
class Subscription final
{
public:
    explicit Subscription(std::function<void()> callback):id(Language::instance().listen(std::move(callback))){}
    ~Subscription(){Language::instance().unlisten(id);}
    Subscription(const Subscription&)=delete;
    Subscription& operator=(const Subscription&)=delete;
private:size_t id;
};
inline juce::String tr(const juce::String& key){return Language::instance().text(key);}
inline juce::String translate(const juce::String& english){return Language::instance().translate(english);}
using Arguments=std::vector<std::pair<juce::String,juce::String>>;
inline juce::String substitute(const juce::String& result,const Arguments& values)
{
    // Substitute in one pass: a user-supplied value cannot inject another token.
    juce::String output;
    for(int i=0;i<result.length();)
    {
        bool replaced=false;
        for(const auto& value:values){const auto token="{"+value.first+"}";if(result.substring(i).startsWith(token)){output+=value.second;i+=token.length();replaced=true;break;}}
        if(!replaced){output+=result.substring(i,i+1);++i;}
    }
    return output;
}
inline juce::String format(const juce::String& key,std::initializer_list<std::pair<juce::String,juce::String>> values)
{return substitute(tr(key),Arguments(values));}
}
