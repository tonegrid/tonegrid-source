#pragma once
#include "Shared/Media/VideoContent.h"
#include <set>
namespace tonegrid::learn
{
struct Video {juce::String title,url,description;};
struct Lesson {juce::String id,title,artist,instructor,date,style,summary,lessonUrl,thumbnail;std::vector<Video> videos;};
struct Partner {juce::String name="Guitar Club",url="https://www.guitarclub.io/",headline="Learn your favourite songs",message="Free song lessons from Guitar Club",button="START YOUR FREE TRIAL\non Guitar Club";};
inline bool safeLink(const juce::String& text)
{const juce::URL url(text);return text.startsWith("https://")&&url.getDomain().containsChar('.')&&!text.containsAnyOf("\r\n\t <>\"")&&url.getDomain().indexOfChar('@')<0;}
inline bool partnerLink(const juce::String& text)
{const auto domain=juce::URL(text).getDomain();return safeLink(text)&&(domain=="www.guitarclub.io"||domain=="guitarclub.io");}
inline bool localThumbnail(const juce::String& text)
{return text.isEmpty()||(text.startsWith("thumbnails/")&&!text.contains("..")&&!text.containsAnyOf("\\:\r\n")&&(text.endsWithIgnoreCase(".jpg")||text.endsWithIgnoreCase(".png")));}
inline juce::String dateLabel(const juce::String& date)
{const juce::StringArray months{"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};const int m=date.substring(5,7).getIntValue();return m>=1&&m<=12?juce::String(date.substring(8,10).getIntValue())+" "+months[m-1]+" "+date.substring(0,4):date;}
inline juce::Result decode(const juce::String& source,std::vector<Lesson>& output,Partner& partner)
{
    using namespace juce;var value;const auto parsed=JSON::parse(source,value);if(parsed.failed())return Result::fail("Invalid lesson JSON.");
    const auto* rows=value["lessons"].getArray();if(int(value["version"])!=1||!rows||rows->size()>2000)return Result::fail("Unsupported lesson catalogue.");
    Partner nextPartner;const auto p=value["partner"];if(!p.isVoid())
    {
        if(!partnerLink(p["url"].toString()))return Result::fail("Invalid Guitar Club link.");
        nextPartner.url=p["url"].toString();for(auto pair:{std::pair<const char*,String*>{"name",&nextPartner.name},{"headline",&nextPartner.headline},{"message",&nextPartner.message},{"button",&nextPartner.button}})
            if(auto s=p[pair.first].toString();s.isNotEmpty()&&s.length()<=180)*pair.second=s;
    }
    std::vector<Lesson> next;std::set<String> ids;
    for(const auto& row:*rows)
    {
        Lesson item;item.id=row["id"].toString();item.title=row["title"].toString();item.artist=row["artist"].toString();item.instructor=row["instructor"].toString();item.date=row["date"].toString();item.style=row["style"].toString();item.summary=row["summary"].toString();item.lessonUrl=row["lessonUrl"].toString();item.thumbnail=row["thumbnail"].toString();
        const auto* videos=row["videos"].getArray();
        if(item.id.isEmpty()||item.id.length()>200||!ids.insert(item.id).second||item.title.isEmpty()||item.title.length()>300||!partnerLink(item.lessonUrl)||!localThumbnail(item.thumbnail)||!videos||videos->isEmpty()||videos->size()>30)return Result::fail("Invalid or duplicate lesson entry.");
        if(item.summary.length()>6000||item.artist.length()>200||item.instructor.length()>200||item.style.length()>80||item.date.length()!=10||item.date[4]!='-'||item.date[7]!='-')return Result::fail("Invalid lesson metadata.");
        for(const auto& v:*videos){Video video{v["title"].toString(),v["url"].toString(),v["description"].toString()};if(!media::safeTrackUrl(video.url)||video.description.length()>50000||video.title.length()>400)return Result::fail("Invalid YouTube lesson video.");item.videos.push_back(std::move(video));}
        next.push_back(std::move(item));
    }
    std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.date>b.date;});output=std::move(next);partner=std::move(nextPartner);return Result::ok();
}
inline bool matches(const Lesson& lesson,const juce::String& query,const juce::String& style,const juce::String& artist,int days,juce::Time now=juce::Time::getCurrentTime())
{
    if(style.isNotEmpty()&&lesson.style!=style)return false;if(artist.isNotEmpty()&&lesson.artist!=artist)return false;
    if(query.isNotEmpty()&&!(lesson.title+" "+lesson.artist+" "+lesson.instructor).containsIgnoreCase(query))return false;
    if(days>0&&juce::Time::fromISO8601(lesson.date+"T23:59:59Z")<now-juce::RelativeTime::days(days))return false;return true;
}
class Favourites
{
public:
    void load(const juce::File& path){file=path;ids.clear();if(!file.existsAsFile())return;if(file.getSize()>1024*1024)return;auto value=juce::JSON::parse(file.loadFileAsString());if(int(value["version"])==1)if(const auto* list=value["favourites"].getArray())for(const auto& id:*list)if(auto s=id.toString();s.isNotEmpty()&&s.length()<=200)ids.insert(s);}
    bool contains(const juce::String& id)const{return ids.contains(id);}
    juce::Result toggle(const juce::String& id)
    {
        if(id.isEmpty()||id.length()>200)return juce::Result::fail("Invalid favourite.");auto next=ids;if(!next.erase(id))next.insert(id);
        if(file.getFullPathName().isNotEmpty())
        {
            if(file.getParentDirectory().createDirectory().failed())return juce::Result::fail("Could not save favourites.");
            juce::var data(new juce::DynamicObject);data.getDynamicObject()->setProperty("version",1);juce::Array<juce::var> values;for(const auto& s:next)values.add(s);data.getDynamicObject()->setProperty("favourites",values);
            juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithText(juce::JSON::toString(data))||!temp.overwriteTargetFileWithTemporary())return juce::Result::fail("Could not save favourites. Previous list kept.");
        }
        ids=std::move(next);return juce::Result::ok();
    }
private:juce::File file;std::set<juce::String> ids;
};
}
