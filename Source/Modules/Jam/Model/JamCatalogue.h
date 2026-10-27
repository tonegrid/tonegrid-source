#pragma once
#include "Modules/Jam/Model/JamScales.h"
#include "Shared/Media/VideoContent.h"
namespace tonegrid::jam
{
using media::Track;
using media::videoId;
using media::safeTrackUrl;
inline juce::Result decodeCatalogue(const juce::String& source,std::vector<Track>& destination)
{
    using namespace juce;var json;const auto parsed=JSON::parse(source,json);if(parsed.failed())return Result::fail("The track JSON is not valid: "+parsed.getErrorMessage());
    const auto* root=json.getDynamicObject();if(!root||int(root->getProperty("version"))!=1)return Result::fail("Expected a track catalogue with version 1.");
    const auto* rows=root->getProperty("tracks").getArray();if(!rows||rows->size()>2000)return Result::fail("Expected a tracks array (maximum 2,000 entries).");
    std::vector<Track> next;StringArray ids;
    for(const auto& row:*rows)
    {
        const auto* object=row.getDynamicObject();if(!object)return Result::fail("Each track must be an object.");
        const auto field=[&](const char* name){return object->getProperty(name).toString().trim();};
        Track t{field("id"),field("title"),field("creator"),field("style"),field("key"),field("mode"),field("url"),field("suggestedScale"),double(object->getProperty("bpm"))};
        if(t.id.isEmpty())t.id=videoId(t.url);
        if(t.id.isEmpty()||ids.contains(t.id)||t.title.isEmpty()||t.title.length()>240||t.creator.length()>160||t.style.isEmpty()||t.style.length()>40||rootFor(t.key)<0||!std::isfinite(t.bpm)||t.bpm<20||t.bpm>400||!safeTrackUrl(t.url))return Result::fail("Invalid or duplicate track: "+t.title.substring(0,80));
        if(t.suggestedScale.isNotEmpty()&&scaleIndex(t.suggestedScale)<0)return Result::fail("Unknown suggested scale in: "+t.title.substring(0,80));
        ids.add(t.id);next.push_back(std::move(t));
    }
    destination=std::move(next);return Result::ok();
}
inline bool matches(const Track& track,const juce::String& style,int root,double minimum,double maximum)
{return (style.isEmpty()||track.style==style)&&(root<0||rootFor(track.key)==root)&&track.bpm>=minimum&&track.bpm<=maximum;}
}
