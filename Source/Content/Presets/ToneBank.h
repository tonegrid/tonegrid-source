#pragma once
#include "Platform/AppPaths.h"
#include "Content/Presets/RigState.h"
#include "Content/Presets/RigMigrations.h"
#include "Content/Amps/NamAmpLibrary.h"
#include <json.hpp>
#include <set>
#include <limits>

namespace tonegrid::tonebank
{
using Json = nlohmann::json;
using juce::String;
struct Resource { String file, sha256, metadataName; };
struct Preset
{
    int rank=0;
    String id,artist,song,part,genre,creator,version;
    board::State state;
    Resource amp;
    std::array<Resource,board::count> captures;
    String title() const { return artist+" — "+song+" / "+part; }
};
struct Bank { String id,version,libraryRevision; std::vector<Preset> presets; juce::StringArray genres; };
// The 1.1.25–1.1.32 bank used the same approved pedal catalogue. Amp identities
// still resolve by exact hash; unavailable legacy amps use the missing-capture UI.
inline bool supportedLibrary(const String& revision)
{return revision==board::libraryRevision||revision=="tonegrid-library-1.1.33-r1"||revision=="tonegrid-library-1.1.25-private-beta-r1";}
inline String compatibleAmpHash(const String& hash)
{
    // Same OR60 network/configuration, only capture metadata changed in the
    // publisher's final library. Verified against both original files at build.
    if(const auto* update=namamp::normalizationMetadataReplacement(hash))return update->sha256;
    if(namamp::withdrawnSilverJubilee(hash))return "3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707";
    if(hash.equalsIgnoreCase("e5631b910bfeedeb48b9fae0602db45440b177c2ecd2e26f1c5bcca34f9096f7"))
        return "98f2af328c7625835c109c796d26151fd74ecad5edafb53c72e4a76a1c90e704";
    return {};
}
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
inline String optionalCredit(const Json& item,const char* key)
{
    const auto value=item.find(key);
    if(value==item.end())return {};
    if(value->is_string()){auto text=String::fromUTF8(value->get_ref<const std::string&>().c_str()).trim();return text.length()<=256?text:String{};}
    if(value->is_number())return String(value->dump());
    return {};
}
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
        Bank bank;if(schema==2){bank.libraryRevision=text(root,"libraryRevision");if(!supportedLibrary(bank.libraryRevision))throw std::runtime_error("Incompatible capture library");}bank.id=text(root,"bankId");bank.version=text(root,"bankVersion");text(root,"targetToneGridVersion");text(root,"title");text(root,"description");
        const auto& entries=root.at("presets");
        if(!entries.is_array()||entries.empty()||entries.size()>size_t(std::numeric_limits<int>::max())
            ||integer(root,"presetCount",1,std::numeric_limits<int>::max())!=(int)entries.size())throw std::runtime_error("ToneBank presetCount must match its non-empty preset list");
        std::set<std::string> ids;std::set<int> ranks;
        for(const auto& item:entries)
        {
            Preset p;p.rank=integer(item,"rank",1,std::numeric_limits<int>::max());p.id=text(item,"id");p.artist=text(item,"artist");p.song=text(item,"song");p.part=text(item,"part");p.genre=text(item,"genre");
            p.creator=optionalCredit(item,"creator");p.version=optionalCredit(item,"version");
            if(p.version.isEmpty())p.version=optionalCredit(item,"revision");
            if(auto metadata=item.find("metadata");metadata!=item.end()&&metadata->is_object())
            {if(p.creator.isEmpty())p.creator=optionalCredit(*metadata,"creator");if(p.version.isEmpty())p.version=optionalCredit(*metadata,"version");if(p.version.isEmpty())p.version=optionalCredit(*metadata,"revision");}
            if(p.creator.equalsIgnoreCase("ToneGrid"))p.creator.clear();
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
                else if(id==0){setting.input=(float)number(params,"thresholdDb",-48,0);setting.ratio=(float)number(params,"ratio",1,20);setting.output=(float)number(params,"levelDb",-24,24);if(params.contains("mixPercent"))setting.mix=(float)(number(params,"mixPercent",0,100)/100);}
                else if(id==6){setting.input=(float)number(params,"boostDb",-18,18);setting.mix=(float)(number(params,"mixPercent",0,100)/100);setting.output=(float)number(params,"levelDb",-24,24);}
                else if(id==9){setting.wahSensitivity=(float)number(params,"sensitivity",0,100);setting.wahRange=(float)number(params,"range",0,100);setting.wahDecay=(float)number(params,"decayMs",50,800);}
                else if(id==12)
                {const auto& bands=params.at("bandsDb");if(!bands.is_array()||bands.size()!=7)throw std::runtime_error("Seven EQ bands required");for(size_t b=0;b<7;++b){Json one={{"gain",bands[b]}};setting.eqBands[b]=(float)number(one,"gain",-12,12);}}
                if(schema==2&&!board::valid(setting,id))throw std::runtime_error("Invalid pedal controls");
                s.pedals[(size_t)id]=setting;
            }
            const auto& a=state.at("amp");if(text(a,"source")!="NAM"||!boolean(a,"required"))throw std::runtime_error("ToneBank v1 requires NAM AmpSource");
            p.amp=resource(a.at("resource"),true);s.namAmp.gainDb=(float)number(a,"gainDb",-24,24);s.namAmp.volumeDb=(float)number(a,"volumeDb",-24,24);fx.ampNormalizationEnabled=boolean(a,"levelNormalization");
            if(state.contains("cabinetIR"))
            {
                const auto& ir=state.at("cabinetIR");cabinet::State cab;
                cab.file=text(ir,"file");cab.sha256=text(ir,"sha256");cab.locked=boolean(ir,"locked");
                if(cab.empty()||!cabinet::valid(cab))throw std::runtime_error("Invalid ToneBank cabinet IR");s.cabinet=cab;
            }
            if(a.contains("levelNormalizationMode"))
            {
                const int mode=enumeration(a,"levelNormalizationMode",{"off","partial","full"});
                if(fx.ampNormalizationEnabled!=(mode!=0))throw std::runtime_error("Normalization mode disagrees with levelNormalization");
                fx.ampNormalizationEnhanced=mode!=1;
            }
            if(a.contains("eq"))
            {
                const auto& eq=a.at("eq");auto& e=fx.ampEq;
                e.bassDb=(float)number(eq,"bassDb",-12,12);e.midDb=(float)number(eq,"midDb",-12,12);e.trebleDb=(float)number(eq,"trebleDb",-12,12);
                for(const auto& field:std::initializer_list<std::pair<const char*,bool*>>{{"enabled",&e.enabled},{"lowCut",&e.lowCut},{"highCut",&e.highCut}})
                    if(eq.contains(field.first))*field.second=boolean(eq,field.first);
                const auto optional=[&](const char* key,float& value,double low,double high){if(eq.contains(key))value=(float)number(eq,key,low,high);};
                optional("lowCutHz",e.lowCutHz,20,500);optional("highCutHz",e.highCutHz,2000,20000);
                optional("lowMidDb",e.lowMidDb,-12,12);optional("lowMidHz",e.lowMidHz,80,2000);optional("lowMidQ",e.lowMidQ,.3,4);
                optional("presenceDb",e.presenceDb,-12,12);optional("presenceHz",e.presenceHz,400,12000);optional("presenceQ",e.presenceQ,.3,4);
                optional("depthDb",e.depthDb,-6,6);optional("depthHz",e.depthHz,40,250);fx.expandedAmpEq=true;
            }
            const auto& spread=state.at("threeDSound");fx.spreadEnabled=boolean(spread,"enabled");fx.offsetMs=(float)number(spread,"offsetMs",-24,24);fx.wobblePercent=(float)number(spread,"wobblePercent",0,100);fx.crossoverHz=(float)number(spread,"crossoverHz",32.5,520);fx.wobbleEnabled=boolean(spread,"wobbleEnabled");fx.crossoverEnabled=boolean(spread,"crossoverEnabled");fx.diffuseEnabled=boolean(spread,"diffuseEnabled");
            const auto& mod=state.at("mod");fx.mod.enabled=boolean(mod,"enabled");fx.mod.type=(mod::Type)enumeration(mod,"type",{"Chorus","Tremolo","Phaser","Flanger"});fx.mod.rateHz=(float)number(mod,"rateHz",.05,10);fx.mod.depth=(float)number(mod,"depthPercent",0,100);fx.mod.level=(float)number(mod,"levelPercent",0,100);
            const auto& delay=state.at("delay");fx.delay.enabled=boolean(delay,"enabled");fx.delay.timeMs=(float)number(delay,"timeMs",1,2000);fx.delay.feedback=(float)number(delay,"feedbackPercent",0,95);fx.delay.mix=(float)number(delay,"mixPercent",0,100);
            const auto& rev=state.at("reverb");fx.reverb.enabled=boolean(rev,"enabled");fx.reverb.type=(reverb::Type)enumeration(rev,"type",{"Room","Plate","Large","Spring"});fx.reverb.timeSeconds=(float)number(rev,"timeSeconds",.2,10);fx.reverb.preDelayMs=(float)number(rev,"preDelayMs",0,100);fx.reverb.color=(float)number(rev,"colorPercent",0,100);fx.reverb.mixPercent=(float)number(rev,"mixPercent",0,100);
            if(!tonegrid::output::valid(fx))throw std::runtime_error("Invalid effect state");s.outputDsp=fx;
            const auto& required=item.at("requiredResources");if(!required.is_array())throw std::runtime_error("requiredResources must be an array");
            bool ampSeen=false,cabinetSeen=false;std::array<bool,board::count> requiredSeen{};
            for(const auto& r:required)
            {
                const auto kind=text(r,"kind");if(!validHash(text(r,"sha256")))throw std::runtime_error("Invalid required SHA256");
                if(kind=="amp"){if(ampSeen||text(r,"filename")!=p.amp.file||!text(r,"sha256").equalsIgnoreCase(p.amp.sha256))throw std::runtime_error("Inconsistent required amp");ampSeen=true;}
                else if(kind=="pedal")
                {const int id=integer(r,"pedalId",0,board::count-1);if(!board::isNam(id)||!s.pedals[(size_t)id].enabled||requiredSeen[(size_t)id]||text(r,"relativePath")!=p.captures[(size_t)id].file||!text(r,"sha256").equalsIgnoreCase(p.captures[(size_t)id].sha256))throw std::runtime_error("Inconsistent required pedal");requiredSeen[(size_t)id]=true;}
                else if(kind=="cabinet")
                {if(cabinetSeen||!s.cabinet||text(r,"file")!=s.cabinet->file||text(r,"sha256")!=s.cabinet->sha256)throw std::runtime_error("Inconsistent required cabinet IR");cabinetSeen=true;}
                else throw std::runtime_error("Unknown required resource kind");
            }
            if(!ampSeen)throw std::runtime_error("Required amp identity missing");
            if(s.cabinet&&!cabinetSeen)throw std::runtime_error("Required cabinet IR identity missing");
            for(int id=0;id<board::count;++id)if(board::isNam(id)&&s.pedals[(size_t)id].enabled&&!requiredSeen[(size_t)id])throw std::runtime_error("Enabled pedal identity missing");
            if(!bank.genres.contains(p.genre,true))bank.genres.add(p.genre);
            bank.presets.push_back(std::move(p));
        }
        std::sort(bank.presets.begin(),bank.presets.end(),[](const auto& a,const auto& b){return a.rank<b.rank;});bank.genres.sort(true);
        output=std::move(bank);return juce::Result::ok();
    }
    catch(const std::exception& e){return juce::Result::fail("Invalid ToneBank: "+String(e.what()));}
}
inline std::vector<size_t> filter(const Bank& bank,const String& genre,const String& search)
{
    std::vector<size_t> rows;const auto needle=search.trim();
    for(size_t i=0;i<bank.presets.size();++i){const auto& p=bank.presets[i];if(genre.isNotEmpty()&&!genre.equalsIgnoreCase("All")&&!genre.equalsIgnoreCase(p.genre)&&!genre.equalsIgnoreCase(genreGroup(p.genre)))continue;
        if(needle.isEmpty()||p.artist.containsIgnoreCase(needle)||p.song.containsIgnoreCase(needle)||p.creator.containsIgnoreCase(needle)||p.version.containsIgnoreCase(needle)||p.amp.metadataName.containsIgnoreCase(needle)||p.amp.file.containsIgnoreCase(needle))rows.push_back(i);}
    return rows;
}
inline juce::File cacheForApplicationData(const juce::File& root)
{return tonegrid::paths::userFolder(root).getChildFile("ToneBank/tonebank.json");}
struct CacheResult { Bank bank; juce::Result result=juce::Result::ok(); String warning; };
// File/cache boundary is deliberately offline. A future validated HTTPS fetch
// can call this atomic installation operation without changing the parser/UI.
inline juce::Result install(const juce::File& file,const String& raw)
{
    Bank candidate;auto result=parse(raw,candidate);if(result.failed())return result;
    if(file.getParentDirectory().createDirectory().failed())return juce::Result::fail("Cannot create ToneBank cache");
    juce::TemporaryFile temporary(file);
    if(!temporary.getFile().replaceWithData(raw.toRawUTF8(),raw.getNumBytesAsUTF8())||!temporary.overwriteTargetFileWithTemporary())return juce::Result::fail("Cannot install ToneBank cache atomically");
    return juce::Result::ok();
}
inline CacheResult readCache(const juce::File& cache,const juce::File& shipped)
{
    CacheResult out;
    const auto marker=cache.getSiblingFile("bundled-sha256.txt");
    const auto bundledHash=shipped.existsAsFile()?juce::SHA256(shipped).toHexString():String{};
    if(cache.existsAsFile())
    {
        out.result=parse(cache.loadFileAsString(),out.bank);
        if(out.result.wasOk()&&supportedLibrary(out.bank.libraryRevision)&&(bundledHash.isEmpty()||marker.loadFileAsString().trim()==bundledHash))return out;
        if(out.result.failed())out.warning="Invalid local ToneBank cache. Using the bundled bank. "+out.result.getErrorMessage();
        // Install a changed bundled bank once per package, including legacy caches
        // without a marker. Later online refreshes remain intact across restarts.
    }
    const auto raw=shipped.loadFileAsString();Bank bundled;const auto validated=parse(raw,bundled);
    if(validated.failed())
    {
        if(!out.bank.presets.empty()&&supportedLibrary(out.bank.libraryRevision)){out.warning="Bundled ToneBank unavailable. Valid local cache kept.";out.result=juce::Result::ok();return out;}
        out.result=validated;return out;
    }
    out.bank=std::move(bundled);out.result=juce::Result::ok();
    auto saved=install(cache,raw);if(saved.failed())out.warning+=" "+saved.getErrorMessage()+". Bundled bank remains available.";
    else if(!marker.replaceWithText(bundledHash))out.warning+=" Cannot save bundled ToneBank revision.";
    return out;
}
struct Plan { board::State state; juce::StringArray warnings; bool replaceAmp=false; };
inline String missingAmp(const Resource& r)
{return "Required NAM Amp is missing: "+r.file+". Please select another one or download it from Tone3000.";}
inline void bypassMissing(Plan& plan,int id,const Resource& r)
{plan.state.pedals[(size_t)id].enabled=false;plan.warnings.add("NAM for Pedal "+String(board::definition(id).displayName)+" is missing: "+r.file+". Please select another one or download it from Tone3000.");}
inline std::optional<namamp::Identity> resolveAmp(const Resource& r,const namamp::Catalog& catalog)
{
    // Filename never authorizes a hash mismatch. Duplicate exact hashes choose
    // expected basename first, then the catalog's deterministic relative order.
    const namamp::Entry* match=nullptr;
    for(const auto& entry:catalog.entries)if(entry.identity.sha256.equalsIgnoreCase(r.sha256))
    {if(!match)match=&entry;if(entry.identity.file==r.file||namamp::filename(entry.identity.file)==namamp::filename(r.file)){match=&entry;break;}}
    if(!match)
    {
        const auto compatible=compatibleAmpHash(r.sha256);
        if(compatible.isNotEmpty())for(const auto& entry:catalog.entries)if(entry.identity.sha256.equalsIgnoreCase(compatible)){match=&entry;break;}
    }
    return match?std::optional<namamp::Identity>(match->identity):std::nullopt;
}
inline Plan prepare(const Preset& p,const namamp::Catalog& catalog,const board::Processor& processor,const juce::File& models)
{
    Plan plan;plan.state=p.state;
    if(auto identity=resolveAmp(p.amp,catalog)){plan.state.namAmp.model=*identity;plan.replaceAmp=true;}else plan.warnings.add(missingAmp(p.amp));
    for(int id=0;id<board::count;++id)if(board::isNam(id)&&plan.state.pedals[(size_t)id].enabled)
    {
        auto r=p.captures[(size_t)id];int preset=plan.state.pedals[(size_t)id].preset;
        if(id==board::id(board::PedalId::BluesOd))
        {
            const int replacement=board::migrations::bluesPreset(r.sha256);
            if(replacement>=0)
            {
                preset=replacement;plan.state.pedals[size_t(id)].preset=preset;
                r.file=processor.modelFile(id,preset);r.sha256=processor.identity(id,preset);
            }
        }
        auto path=r.file;
        for(const auto& alias:std::array<std::pair<const char*,const char*>,9>{{{"ts9/","808_od/"},{"centura/","horse_od/"},{"ds1/","orange_dist/"},{"timmy/","transparent_od/"},{"muff/","vintage_fuzz/"},{"rat/","filter_dist/"},{"fuzz/","classic_fuzz/"},{"treble/","treble_boost/"},{"blues/","blues_od/"}}})
            if(path.startsWith(alias.first)){path=String(alias.second)+path.substring((int)std::strlen(alias.first));break;}
        const auto file=models.getChildFile(path);
        if(!processor.available(id,preset)||processor.modelFile(id,preset)!=path||!processor.identity(id,preset).equalsIgnoreCase(r.sha256)||!file.existsAsFile()||juce::SHA256(file).toHexString()!=r.sha256)bypassMissing(plan,id,r);
    }
    return plan;
}
}
