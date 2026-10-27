#pragma once
#include "Platform/AppPaths.h"
#include "Content/Presets/RigState.h"
#include "Content/Amps/NamAmpLibrary.h"
#include <json.hpp>
#include <set>

namespace tonegrid::tonebank132Reference
{
using Json = nlohmann::json;
using juce::String;
struct Resource { String file, sha256, metadataName; };
struct Preset
{
    int rank=0;
    String id,artist,song,part,genre;
    board::State state;
    Resource amp;
    std::array<Resource,board::count> captures;
    String title() const { return artist+" — "+song+" / "+part; }
};
struct Bank { String id,version,libraryRevision; std::vector<Preset> presets; juce::StringArray genres; };
// Preserve detailed source metadata; the browser exposes five stable groups.
inline String genreGroup(const String& detailed)
{
    if(detailed.containsIgnoreCase("metal"))return "Metal";
    if(detailed.containsIgnoreCase("blues"))return "Blues";
    if(detailed.containsIgnoreCase("alternative")||detailed.containsIgnoreCase("punk")||detailed.containsIgnoreCase("grunge"))return "Alternative & Punk";
    if(detailed.containsIgnoreCase("pop")||detailed.containsIgnoreCase("funk")||detailed.containsIgnoreCase("new wave"))return "Pop & Funk";
    return "Rock";
}
inline juce::StringArray genreGroups(){return {"Rock","Metal","Blues","Alternative & Punk","Pop & Funk"};}
inline String text(const Json& object,const char* key)
{
    const auto& value=object.at(key);
    if(!value.is_string())throw std::runtime_error(std::string(key)+" must be text");
    auto result=String::fromUTF8(value.get_ref<const std::string&>().c_str());
    if(result.trim().isEmpty()||result.length()>2048)throw std::runtime_error(std::string(key)+" is empty or too long");
    return result;
}
inline bool boolean(const Json& object,const char* key)
{const auto& value=object.at(key);if(!value.is_boolean())throw std::runtime_error(std::string(key)+" must be boolean");return value.get<bool>();}
inline double number(const Json& object,const char* key,double low,double high)
{
    const auto& value=object.at(key);
    if(!value.is_number())throw std::runtime_error(std::string(key)+" must be numeric");
    const auto result=value.get<double>();
    if(!std::isfinite(result)||result<low||result>high)throw std::runtime_error(std::string(key)+" is outside its range");
    return result;
}
inline int integer(const Json& object,const char* key,int low,int high)
{const auto value=number(object,key,low,high);if(std::floor(value)!=value)throw std::runtime_error(std::string(key)+" must be an integer");return (int)value;}
inline int enumeration(const Json& object,const char* key,const juce::StringArray& values)
{auto value=text(object,key);const int i=values.indexOf(value);if(i<0)throw std::runtime_error(std::string(key)+" has an unknown enum value");return i;}
inline bool validHash(const String& hash)
{return hash.length()==64&&hash.containsOnly("0123456789abcdefABCDEF");}
inline Resource resource(const Json& node,bool amp)
{
    Resource r;r.file=text(node,amp?"filename":"relativePath");r.sha256=text(node,"sha256").toLowerCase();
    if(!validHash(r.sha256)||!namamp::validRelativePath(r.file))throw std::runtime_error("Invalid NAM resource identity");
    if(amp)r.metadataName=text(node,"metadataName");else text(node,"display");
    return r;
}
// Transactional translation into the existing engine's value snapshot. No DSP,
// filesystem, UI or network calls; editorial fields remain in the original JSON.
inline juce::Result parse(const String& raw,Bank& output)
{
    try
    {
        if(raw.getNumBytesAsUTF8()>8*1024*1024)throw std::runtime_error("ToneBank exceeds 8 MiB");
        const auto root=Json::parse(raw.toStdString());
        const int schema=integer(root,"schemaVersion",1,2);if(!boolean(root,"readOnly"))throw std::runtime_error("ToneBank must be read-only");
        Bank bank;if(schema==2){bank.libraryRevision=text(root,"libraryRevision");if(bank.libraryRevision!="tonegrid-library-1.1.25-private-beta-r1")throw std::runtime_error("Incompatible capture library");}bank.id=text(root,"bankId");bank.version=text(root,"bankVersion");text(root,"targetToneGridVersion");text(root,"title");text(root,"description");
        const auto& entries=root.at("presets");
        if(!entries.is_array()||entries.size()!=100||integer(root,"presetCount",100,100)!=(int)entries.size())throw std::runtime_error("ToneBank v1 requires exactly 100 presets");
        std::set<std::string> ids;std::set<int> ranks;
        for(const auto& item:entries)
        {
            Preset p;p.rank=integer(item,"rank",1,100);p.id=text(item,"id");p.artist=text(item,"artist");p.song=text(item,"song");p.part=text(item,"part");p.genre=text(item,"genre");
            if(!boolean(item,"readOnly")||!ids.insert(p.id.toStdString()).second||!ranks.insert(p.rank).second)throw std::runtime_error("Duplicate ID/rank or editable ToneBank preset");
            auto& s=p.state;s.name=p.title();s.sourceType=amp::SourceType::Nam;s.genericVst3=false;
            const auto& state=item.at("state");const auto& in=state.at("input");
            s.input={(float)number(in,"gainDb",-12,12),(float)number(in,"gateThresholdDbfs",-80,-20),boolean(in,"gateEnabled")};
            tonegrid::output::Settings fx;fx.expandedEffects=true;
            const auto& pitch=state.at("pitch");fx.pitch.mode=(pitch::Mode)enumeration(pitch,"mode",{"OFF","TRANSPOSE","OCTAVER"});fx.pitch.semitones=integer(pitch,"semitones",-24,24);
            fx.pitch.dry=(float)number(pitch,"dryPercent",0,100);fx.pitch.down=(float)number(pitch,"downPercent",0,100);fx.pitch.up=(float)number(pitch,"upPercent",0,100);
            const auto& pedals=state.at("pedals");if(!pedals.is_array()||pedals.size()!=board::count)throw std::runtime_error("Exactly 13 pedals required");
            std::array<bool,board::count> seen{};
            for(const auto& pedal:pedals)
            {
                const int id=integer(pedal,"id",0,board::count-1);if(seen[(size_t)id])throw std::runtime_error("Duplicate pedal ID");seen[(size_t)id]=true;
                if(!text(pedal,"name").equalsIgnoreCase(board::definition(id).displayName))throw std::runtime_error("Pedal ID/name mismatch");
                const int preset=integer(pedal,"presetId",0,schema==2?board::presetCount(id)-1:(id==10?21:7));
                auto setting=board::defaults(id,preset);setting.enabled=boolean(pedal,"enabled");const auto& params=pedal.at("parameters");
                if(board::isNam(id))
                {p.captures[(size_t)id]=resource(pedal.at("resource"),false);setting.input=(float)number(params,"inputDb",-12,12);setting.tone=(float)number(params,"toneDb",-6,6);setting.output=(float)number(params,"levelDb",-24,24);}
                else if(id==0){setting.input=(float)number(params,"thresholdDb",-48,0);setting.ratio=(float)number(params,"ratio",1,20);setting.output=(float)number(params,"levelDb",-24,24);}
                else if(id==6){setting.input=(float)number(params,"boostDb",-18,18);setting.mix=(float)(number(params,"mixPercent",0,100)/100);setting.output=(float)number(params,"levelDb",-24,24);}
                else if(id==9){setting.wahSensitivity=(float)number(params,"sensitivity",0,100);setting.wahRange=(float)number(params,"range",0,100);setting.wahDecay=(float)number(params,"decayMs",50,800);}
                else if(id==12)
                {const auto& bands=params.at("bandsDb");if(!bands.is_array()||bands.size()!=7)throw std::runtime_error("Seven EQ bands required");for(size_t b=0;b<7;++b){Json one={{"gain",bands[b]}};setting.eqBands[b]=(float)number(one,"gain",-12,12);}}
                if(schema==2&&!board::valid(setting,id))throw std::runtime_error("Invalid pedal controls");
                s.pedals[(size_t)id]=setting;
            }
            const auto& a=state.at("amp");if(text(a,"source")!="NAM"||!boolean(a,"required"))throw std::runtime_error("ToneBank v1 requires NAM AmpSource");
            p.amp=resource(a.at("resource"),true);s.namAmp.gainDb=(float)number(a,"gainDb",-12,12);s.namAmp.volumeDb=(float)number(a,"volumeDb",-24,12);fx.ampNormalizationEnabled=boolean(a,"levelNormalization");
            if(a.contains("eq")){const auto& eq=a.at("eq");fx.ampEq={(float)number(eq,"bassDb",-12,12),(float)number(eq,"midDb",-12,12),(float)number(eq,"trebleDb",-12,12)};if(eq.contains("enabled"))fx.ampEq.enabled=boolean(eq,"enabled");fx.expandedAmpEq=true;}
            const auto& spread=state.at("threeDSound");fx.spreadEnabled=boolean(spread,"enabled");fx.offsetMs=(float)number(spread,"offsetMs",-24,24);fx.wobblePercent=(float)number(spread,"wobblePercent",0,100);fx.crossoverHz=(float)number(spread,"crossoverHz",32.5,520);fx.wobbleEnabled=boolean(spread,"wobbleEnabled");fx.crossoverEnabled=boolean(spread,"crossoverEnabled");fx.diffuseEnabled=boolean(spread,"diffuseEnabled");
            const auto& mod=state.at("mod");fx.mod.enabled=boolean(mod,"enabled");fx.mod.type=(mod::Type)enumeration(mod,"type",{"Chorus","Tremolo","Phaser","Flanger"});fx.mod.rateHz=(float)number(mod,"rateHz",.05,10);fx.mod.depth=(float)number(mod,"depthPercent",0,100);fx.mod.level=(float)number(mod,"levelPercent",0,100);
            const auto& delay=state.at("delay");fx.delay.enabled=boolean(delay,"enabled");fx.delay.timeMs=(float)number(delay,"timeMs",1,2000);fx.delay.feedback=(float)number(delay,"feedbackPercent",0,95);fx.delay.mix=(float)number(delay,"mixPercent",0,100);
            const auto& rev=state.at("reverb");fx.reverb.enabled=boolean(rev,"enabled");fx.reverb.type=(reverb::Type)enumeration(rev,"type",{"Room","Plate","Large","Spring"});fx.reverb.timeSeconds=(float)number(rev,"timeSeconds",.2,10);fx.reverb.preDelayMs=(float)number(rev,"preDelayMs",0,100);fx.reverb.color=(float)number(rev,"colorPercent",0,100);fx.reverb.mixPercent=(float)number(rev,"mixPercent",0,100);
            if(!tonegrid::output::valid(fx))throw std::runtime_error("Invalid effect state");s.outputDsp=fx;
            const auto& required=item.at("requiredResources");if(!required.is_array())throw std::runtime_error("requiredResources must be an array");
            bool ampSeen=false;std::array<bool,board::count> requiredSeen{};
            for(const auto& r:required)
            {
                const auto kind=text(r,"kind");if(!validHash(text(r,"sha256")))throw std::runtime_error("Invalid required SHA256");
                if(kind=="amp"){if(ampSeen||text(r,"filename")!=p.amp.file||!text(r,"sha256").equalsIgnoreCase(p.amp.sha256))throw std::runtime_error("Inconsistent required amp");ampSeen=true;}
                else if(kind=="pedal")
                {const int id=integer(r,"pedalId",0,board::count-1);if(!board::isNam(id)||!s.pedals[(size_t)id].enabled||requiredSeen[(size_t)id]||text(r,"relativePath")!=p.captures[(size_t)id].file||!text(r,"sha256").equalsIgnoreCase(p.captures[(size_t)id].sha256))throw std::runtime_error("Inconsistent required pedal");requiredSeen[(size_t)id]=true;}
                else throw std::runtime_error("Unknown required resource kind");
            }
            if(!ampSeen)throw std::runtime_error("Required amp identity missing");
            for(int id=0;id<board::count;++id)if(board::isNam(id)&&s.pedals[(size_t)id].enabled&&!requiredSeen[(size_t)id])throw std::runtime_error("Enabled pedal identity missing");
            if(!bank.genres.contains(p.genre,true))bank.genres.add(p.genre);
            bank.presets.push_back(std::move(p));
        }
        std::sort(bank.presets.begin(),bank.presets.end(),[](const auto& a,const auto& b){return a.rank<b.rank;});bank.genres.sort(true);
        output=std::move(bank);return juce::Result::ok();
    }
    catch(const std::exception& e){return juce::Result::fail("Invalid ToneBank: "+String(e.what()));}
}
}
