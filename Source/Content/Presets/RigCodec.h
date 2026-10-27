#pragma once
#include "Content/Presets/RigMigrations.h"
#include "Content/Presets/PresetMetadata.h"
#include "Vst3Identity.h"
#include "Vst3State.h"

namespace board
{
inline Result ampCompatibility(const PluginDescription& saved,const PluginDescription& actual,bool generic)
{
    const bool same=generic?vst3::Identity::of(saved).valid()&&vst3::Identity::of(saved)==vst3::Identity::of(actual):saved.isDuplicateOf(actual);
    return same?Result::ok():Result::fail(generic?"VST3 identity differs from this saved rig.":"TONEX identity differs from this saved rig.");
}
// Pure XML conversion/validation. A failed decode never publishes its candidate.
inline XmlElement encode(const State& original, const PluginDescription& description, const RigIdentityLookup& identity)
{
    State snapshot=original;bool custom=false;
    for(int p=0;p<count;++p)custom|=isNam(p)&&snapshot.pedals[size_t(p)].preset==presetCount(p);
    if((custom||snapshot.cabinet)&&!snapshot.outputDsp)snapshot.outputDsp=tonegrid::output::legacySettings();
    const auto& s=snapshot;
    const bool native=s.sourceType==amp::SourceType::Nam;
    const bool modern=s.outputDsp.has_value();
    XmlElement xml("PedalRig"); xml.setAttribute("version", modern&&tonegrid::ampEq::hasDepth(s.outputDsp->ampEq)?16:custom?15:modern?(tonegrid::ampEq::advanced(s.outputDsp->ampEq)?14:tonegrid::output::requiresAmpEq(*s.outputDsp)?13:tonegrid::output::requiresExpanded(*s.outputDsp)?12:s.outputDsp->reverb.type==tonegrid::reverb::Type::Spring?11:10):native?8:s.genericVst3?7:6); xml.setAttribute("name", s.name);
    if(native&&s.cabinet){xml.setAttribute("version",17);xml.addChildElement(new XmlElement(tonegrid::cabinet::encode(*s.cabinet)));}
    if(modern)xml.addChildElement(new XmlElement(tonegrid::output::encode(*s.outputDsp)));
    if(native)xml.addChildElement(new XmlElement(namamp::encode(s.namAmp)));
    else {xml.addChildElement(description.createXml().release());if(s.genericVst3||modern)xml.addChildElement(new XmlElement(vst3::Identity::of(description).xml()));}
    auto* input=xml.createNewChildElement("Input"); input->setAttribute("gain",s.input.gainDb);input->setAttribute("threshold",s.input.thresholdDb);input->setAttribute("gate",s.input.gate);
    if(!native){auto* amp = xml.createNewChildElement("Amp"); amp->setAttribute("bytes", (int)s.amp.getSize());
    amp->setAttribute("sha256", SHA256(s.amp).toHexString()); amp->addTextElement(s.amp.toBase64Encoding());}
    for (int p = 0; p < count; ++p)
    {
        const auto& v = s.pedals[(size_t)p]; auto* item = xml.createNewChildElement("Pedal");
        item->setAttribute("id", p); item->setAttribute("preset", v.preset); item->setAttribute("enabled", v.enabled);
        item->setAttribute("input", v.input); if(isNam(p))item->setAttribute("tone",v.tone);else item->setAttribute("mix", v.mix); item->setAttribute("output", v.output); item->setAttribute("ratio",v.ratio);
        if(p==id(PedalId::AutoWah)){item->setAttribute("wahSensitivity",v.wahSensitivity);item->setAttribute("wahRange",v.wahRange);item->setAttribute("wahDecay",v.wahDecay);}
        if(p==id(PedalId::GraphicEq))for(int b=0;b<7;++b)item->setAttribute("eq"+String(b),v.eqBands[(size_t)b]);
        item->setAttribute("model",isNam(p)&&v.preset==presetCount(p)&&s.customPedalHashes[size_t(p)].isNotEmpty()?s.customPedalHashes[size_t(p)]:identity(p,v.preset));
        if(isNam(p)&&v.preset==presetCount(p))item->setAttribute("personalCapture",true);
    }
    tonegrid::presets::restoreExtensions(xml, s.extensionXml);
    if(native&&s.cabinet&&s.cabinet->bypassed)
    {auto features=StringArray::fromTokens(xml.getStringAttribute("requiredFeatures"),",",{});features.addIfNotAlreadyThere("cabinet-bypass");xml.setAttribute("requiredFeatures",features.joinIntoString(","));}
    xml.setAttribute("presetId", s.presetId.isEmpty() ? "legacy-" + SHA256(xml.toString().toUTF8()).toHexString() : s.presetId);
    return xml;
}
inline Result decode(const XmlElement& xml, const PluginDescription& expected, const RigIdentityLookup& identity, State& result)
{
    const int version=xml.getIntAttribute("version");
    if (! xml.hasTagName("PedalRig") || version<3 || version>17) return Result::fail("A version 3 through 17 pedalboard rig is required.");
    if (const auto compatible = tonegrid::presets::compatibility(xml); compatible.failed()) return compatible;
    State candidate = migrations::initialState();
    candidate.presetId = xml.getStringAttribute("presetId", "legacy-" + SHA256(xml.toString().toUTF8()).toHexString());
    if (candidate.presetId.isEmpty() || candidate.presetId.length() > 128) return Result::fail("Invalid preset ID.");
    candidate.extensionXml = tonegrid::presets::captureExtensions(xml);
    if(version>=9)
    {
        int countOutput=0;for(auto* child:xml.getChildIterator())countOutput+=child->hasTagName("OutputDSP")?1:0;
        auto* node=xml.getChildByName("OutputDSP");tonegrid::output::Settings settings;
        if(countOutput!=1||!node||(version!=15&&version!=17&&node->getStringAttribute("version")!=(version==9?"1":version==10?"2":version==11?"3":version==12?"4":version==13?"5":version==16?"7":"6"))||!tonegrid::output::decode(*node,settings))return Result::fail("Invalid output DSP settings.");
        candidate.outputDsp=settings;
    }
    const auto* sourceNode=xml.getChildByName("AmpSource");
    const bool native=version==8||(version>=9&&sourceNode&&sourceNode->getStringAttribute("type")=="NAM");
    const bool generic=version==7||(version>=9&&!native);
    if((version==17&&!native)||(version!=17&&xml.getChildByName("CabinetIR")))return Result::fail("Unexpected cabinet IR preset state.");
    if(native)
    {
        int sources=0;for(auto* child:xml.getChildIterator())sources+=child->hasTagName("AmpSource")?1:0;
        auto* source=xml.getChildByName("AmpSource");
        if(sources!=1||xml.getChildByName("PLUGIN")||xml.getChildByName("Amp")||!source||!namamp::decode(*source,candidate.namAmp))return Result::fail("Invalid NAM amp rig identity/settings.");
        candidate.sourceType=amp::SourceType::Nam;
        if(version==17){int count=0;for(auto* child:xml.getChildIterator())count+=child->hasTagName("CabinetIR");auto* node=xml.getChildByName("CabinetIR");tonegrid::cabinet::State cab;if(count!=1||!node||!tonegrid::cabinet::decode(*node,cab))return Result::fail("Invalid cabinet IR preset.");candidate.cabinet=cab;}
        if(source->getStringAttribute("file")!=candidate.namAmp.model.file&&!namamp::normalizationMetadataReplacement(source->getStringAttribute("sha256"))){if(!candidate.outputDsp)candidate.outputDsp=tonegrid::output::freshSessionSettings();if(!namamp::withdrawnSilverJubilee(source->getStringAttribute("sha256")))candidate.outputDsp->ampEq.enabled=false;candidate.missingCaptureDiagnostics.add(namamp::withdrawnSilverJubilee(source->getStringAttribute("sha256"))?"Withdrawn Silver Jubilee capture replaced with the approved Wizard Modern Classic capture.":"Withdrawn Princeton capture replaced with an approved Fender Ramparte capture.");}
    }
    else
    {
    PluginDescription saved; auto* plugin = xml.getChildByName("PLUGIN");
    if (!plugin||!saved.loadFromXml(*plugin))return Result::fail(generic?"Invalid VST3 rig description.":"TONEX identity or version differs from this saved rig.");
    if(generic)
    {
        int descriptions=0,sources=0;for(auto* child:xml.getChildIterator()){descriptions+=child->hasTagName("PLUGIN")?1:0;sources+=child->hasTagName("AmpSource")?1:0;}
        if(descriptions!=1||sources!=1)return Result::fail("Invalid VST3 rig identity.");
        vst3::Identity key;auto* source=xml.getChildByName("AmpSource");
        if(!source||!vst3::Identity::read(*source,key)||!(key==vst3::Identity::of(saved)))return Result::fail("Invalid VST3 rig identity.");
    }
    if(const auto compatible=ampCompatibility(saved,expected,generic);compatible.failed())return compatible;
    candidate.genericVst3=generic;
    }
    if(auto* input=xml.getChildByName("Input")){candidate.input={ (float)input->getDoubleAttribute("gain",999),(float)input->getDoubleAttribute("threshold",999),input->getBoolAttribute("gate")};if(!validInput(candidate.input))return Result::fail("Invalid input/gate settings.");}
    else if(version>=4)return Result::fail("Missing input/gate settings.");
    candidate.name = xml.getStringAttribute("name"); auto* amp = xml.getChildByName("Amp");
    if (!native&&(! amp || ! candidate.amp.fromBase64Encoding(amp->getAllSubText()) || candidate.amp.isEmpty()
        || candidate.amp.getSize() > 45*1024*1024 || (int)candidate.amp.getSize() != amp->getIntAttribute("bytes")
        || SHA256(candidate.amp).toHexString() != amp->getStringAttribute("sha256"))) return Result::fail(generic?"Missing or corrupt VST3 state.":"Missing or corrupt TONEX state.");
    if(generic&&vst3::hasEnvelopePrefix(candidate.amp))
    {vst3::State parsed;if(!vst3::decodeState(candidate.amp,parsed))return Result::fail("Invalid extended VST3 state.");}
    std::array<bool,count> seen {};
    for (auto* item : xml.getChildIterator())
    {
        if (! item->hasTagName("Pedal")) continue;
        const int p = item->getIntAttribute("id", -1); if (p < 0 || p >= migrations::pedalCount(version) || seen[(size_t)p]) return Result::fail("Invalid pedal list.");
        auto& s = candidate.pedals[(size_t)p]; s.preset = item->getIntAttribute("preset", -1);
        s.enabled = item->getBoolAttribute("enabled"); s.input = (float)item->getDoubleAttribute("input", 999);
        s.mix = isNam(p)?1.f:(float)item->getDoubleAttribute("mix", 999); s.output = (float)item->getDoubleAttribute("output", 999);
        s.tone = isNam(p)&&version>=5?(float)item->getDoubleAttribute("tone",999):0.f;
        if (auto migrated = migrations::legacyNam(*item, version, p, s); migrated.failed()) return migrated;
        s.ratio=(float)item->getDoubleAttribute("ratio",migrations::defaultRatio(p,s.preset));
        if(p==id(PedalId::AutoWah)){s.wahSensitivity=(float)item->getDoubleAttribute("wahSensitivity",999);s.wahRange=(float)item->getDoubleAttribute("wahRange",999);s.wahDecay=(float)item->getDoubleAttribute("wahDecay",999);}
        if(p==id(PedalId::GraphicEq))for(int b=0;b<7;++b)s.eqBands[(size_t)b]=(float)item->getDoubleAttribute("eq"+String(b),999);
        auto model=item->getStringAttribute("model");
        if(p==id(PedalId::OrangeDist))
        {
            const int migrated=migrations::orangePreset(model);
            if(migrated>=0){s.preset=migrated;if(migrations::withdrawnOrange(model)){model=identity(p,migrated);candidate.missingCaptureDiagnostics.add("Withdrawn DS-1 capture replaced with the approved Wampler JCM Mod capture.");}}
            else if(version>=15&&(s.preset==9||item->getBoolAttribute("personalCapture")))s.preset=presetCount(p);
        }
        // Restore factory and personal identities across Blues library revisions;
        // numerical slots alone never authorize a change of capture.
        if(p==id(PedalId::BluesOd))
        {
            if(s.preset==14&&model=="c4fb6b032d6149e16e555860cdd715651fefec59f917519d5fe6a2bd1a680836"&&!item->getBoolAttribute("personalCapture"))
            {
                s.preset=recommendedPreset(p);s.enabled=false;
                candidate.missingCaptureDiagnostics.add("Waza Custom was withdrawn from Blues OD. Pedal bypassed; select a current capture to enable it.");
                if(!valid(s,p))return Result::fail("Invalid withdrawn Blues OD settings.");
                seen[size_t(p)]=true;continue;
            }
            const bool personal=item->getBoolAttribute("personalCapture");
            if(version>=15&&(personal||((s.preset==14||s.preset==15)&&model!=identity(p,s.preset))))s.preset=presetCount(p);
            else if(!personal)
            {
                const int replacement=migrations::bluesPreset(model);
                if(replacement>=0){s.preset=replacement;model=identity(p,replacement);}
            }
        }
        const bool legacyWah=migrations::isLegacyWah(p,model);
        if(!valid(s,p))return Result::fail("Invalid settings or changed capture for "+pedalName(p));
        if(isNam(p)&&s.preset==presetCount(p))
        {
            if(version<15||model.length()!=64||!model.containsOnly("0123456789abcdef"))return Result::fail("Invalid personal capture identity.");
            candidate.customPedalHashes[size_t(p)]=model;seen[size_t(p)]=true;continue;
        }
        const auto installed=identity(p,s.preset);
        if(isNam(p)&&installed.isEmpty())
        {s.enabled=false;candidate.missingCaptureDiagnostics.add("Missing capture for "+pedalName(p)+" (preset "+String(s.preset)+", model "+model+"). Pedal bypassed; remaining rig restored.");}
        else if(!legacyWah&&model!=installed)return Result::fail("Invalid settings or changed capture for "+pedalName(p));
        seen[(size_t)p] = true;
    }
    if (std::find(seen.begin(),seen.begin()+migrations::pedalCount(version),false) != seen.begin()+migrations::pedalCount(version)) return Result::fail("Incomplete pedalboard.");
    result = std::move(candidate); return Result::ok();
}
}
