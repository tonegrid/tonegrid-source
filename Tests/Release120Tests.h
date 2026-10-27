#pragma once
#include "Distribution116Tests.h"
#include "Modules/Play/UI/PedalCard.h"
#include "NamAmp.h"
#include "Content/Presets/RigStore.h"
#include "OutputDspNodes.h"
#include "EffectsNodes.h"
#include "Content/Presets/StarterPresets.h"

namespace release120Tests
{
inline bool run(const juce::File& payload,const juce::File& evidence,juce::String& report)
{
    using namespace juce;using namespace tonegrid;
    bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"Release120 "+name+"\n";};
    const auto parent=File::getSpecialLocation(File::tempDirectory),root=parent.getChildFile("TG120-"+Uuid().toString().substring(0,8));
    check(root.createDirectory().wasOk()&&evidence.createDirectory().wasOk(),"isolated storage and evidence folders");
    check(output::freshSessionSettings().ampNormalizationEnabled&&output::freshSessionSettings().ampNormalizationEnhanced,"fresh sessions select Full normalization");
    const auto models=payload.getChildFile("Models"),factoryFolder=payload.getChildFile("FactoryAmps"),shipped=payload.getChildFile("ToneBank/tonebank.json");
    board::Processor processor;check(processor.load(models).wasOk(),"all 151 bundled pedal captures load");if(!ok)return false;
    const auto libraryFolder=root.getChildFile("NAM/Amps");std::atomic<bool> cancelled{false};auto seeded=factory::seed(factoryFolder,libraryFolder,cancelled);
    check(seeded.wasOk(),"complete amplifier installation: "+seeded.getErrorMessage());if(seeded.failed())return false;
    namamp::Library library(libraryFolder);namamp::Catalog catalog;check(library.refresh(catalog).wasOk()&&catalog.entries.size()==tonebank::Json::parse(factoryFolder.getChildFile("manifest.json").loadFileAsString().toStdString()).at("ampCount").get<size_t>()&&catalog.failures.empty(),"approved IR factory inventory indexed offline, zero metadata failures");
    tonebank::Bank bank;check(tonebank::parse(shipped.loadFileAsString(),bank).wasOk()&&bank.presets.size()==101,"101 current presets parsed");if(bank.presets.empty())return false;
    const auto factoryManifest=tonebank::Json::parse(factoryFolder.getChildFile("manifest.json").loadFileAsString().toStdString());
    bool presentation=true;for(const auto& asset:factoryManifest.at("assets")){const String file(asset.at("filename").get<std::string>()),hash(asset.at("sha256").get<std::string>());presentation&=SHA256(libraryFolder.getChildFile(file)).toHexString()==hash;}
    check(presentation,"all factory thumbnails and metadata sidecars install byte-for-byte");
    bool allModels=true;for(const auto& entry:catalog.entries)
    {
        namamp::State selection{entry.identity,0,0};auto model=amp::loadNamAmp(library,selection);if(!model.source){allModels=false;report+="FAIL factory model "+entry.identity.file+": "+model.result.getErrorMessage()+"\n";continue;}
        AudioProcessorGraph graph;auto node=model.source->addToGraph(graph);auto* audio=node->getProcessor();audio->prepareToPlay(48000,128);AudioBuffer<float> buffer(2,128);MidiBuffer midi;
        for(int block=0;block<4;++block){for(int i=0;i<128;++i){const float x=.02f*std::sin(float(block*128+i)*.12f);buffer.setSample(0,i,x);buffer.setSample(1,i,x);}audio->processBlock(buffer,midi);for(int channel=0;channel<2;++channel)for(int i=0;i<128;++i)allModels&=std::isfinite(buffer.getSample(channel,i));}
        audio->releaseResources();model.source->unload();
    }
    check(allModels,"all approved factory NAMs instantiate and render finite audio");
    check(!payload.getChildFile("ToneBank/transition-legacy.json").existsAsFile(),"release ships only the current modern ToneBank");
    const auto lookup=[&](int id,int n){return processor.identity(id,n);};PluginDescription none;
    board::RigStore store(root.getChildFile("UserPresets"),root.getChildFile("legacy"));
    output::AmpEqNode equalizer;output::NormalizationNode normalization;output::SpreadNode spread;output::ReverbNode reverb;
    output::ModNode mod;output::DelayNode delay;
    for(auto* node:std::initializer_list<AudioProcessor*>{&normalization,&equalizer,&spread,&reverb,&mod,&delay})node->prepareToPlay(48000,128);
    MemoryOutputStream stats;stats.writeText("rank,title,peak_dbfs,finite,calibrated_reference_dbu,soft_peak_dbfs,medium_rms_dbfs,hard_peak_dbfs\n",false,false,nullptr);
    for(const auto& p:bank.presets)
    {
        bool references=true;for(int id=0;id<board::count;++id)if(board::isNam(id))
        {const auto n=p.state.pedals[size_t(id)].preset;references&=processor.available(id,n)&&processor.modelFile(id,n)==p.captures[size_t(id)].file&&processor.identity(id,n)==p.captures[size_t(id)].sha256;}
        auto plan=tonebank::prepare(p,catalog,processor,models);check(references&&plan.replaceAmp&&plan.warnings.isEmpty()&&plan.state.outputDsp->ampNormalizationEnabled==p.state.outputDsp->ampNormalizationEnabled&&plan.state.outputDsp->ampNormalizationEnhanced==p.state.outputDsp->ampNormalizationEnhanced,"rank "+String(p.rank)+" all enabled/bypassed references resolve; publisher normalization retained");
        if(!plan.replaceAmp)continue;
        auto loaded=amp::loadNamAmp(library,plan.state.namAmp);check(loaded.result.wasOk()&&loaded.source,"rank "+String(p.rank)+" actual amp loads");if(!loaded.source)continue;
        auto xml=board::encode(plan.state,none,lookup);board::State restored;auto decoded=board::decode(xml,none,lookup,restored);
        check(decoded.wasOk()&&restored.namAmp.model==plan.state.namAmp.model&&restored.outputDsp&&restored.outputDsp->ampNormalizationEnabled==plan.state.outputDsp->ampNormalizationEnabled&&restored.outputDsp->ampNormalizationEnhanced==plan.state.outputDsp->ampNormalizationEnhanced&&board::encode(restored,none,lookup).isEquivalentTo(&xml,true),"rank "+String(p.rank)+" complete User Preset codec round trip");
        auto* native=dynamic_cast<amp::NamAmp*>(loaded.source.get());AudioProcessorGraph graph;auto ampNode=loaded.source->addToGraph(graph);
        const calibration::Settings reference(true,12.5f);processor.setCalibration(reference);native->setCalibration(reference);native->setUpstreamReference(&processor.referenceRouting);
        for(int id=0;id<board::count;++id)processor.set(id,plan.state.pedals[size_t(id)]);processor.front.set(plan.state.input);processor.prepareToPlay(48000,128);ampNode->getProcessor()->prepareToPlay(48000,128);
        // Each recipe is an independent level measurement: no previous preset's
        // normalization ramp, EQ memory or delay/reverb tail may contaminate it.
        for(auto* node:std::initializer_list<AudioProcessor*>{&normalization,&equalizer,&spread,&reverb,&mod,&delay})node->prepareToPlay(48000,128);
        const auto fx=*plan.state.outputDsp;equalizer.configure(fx.ampEq,true);normalization.configure(fx.ampNormalizationEnabled,true,native->loudnessDb(),output::selectedNormalizationTrim(fx,native->normalizationTrimDb()));spread.configure(fx);mod.configure(fx.mod);delay.configure(fx.delay);reverb.configure(fx.reverb);processor.pitch.configure(fx.pitch);
        AudioBuffer<float> audio(2,128);MidiBuffer midi;double peak=0,energy=0;std::array<double,3> phasePeak{},phaseEnergy{};bool finite=true;
        // Identical quiet notes, medium plucks and hard six-string chords across
        // all recipes. This is a bounded engineering stimulus, not a listening verdict.
        for(int block=0;block<768;++block)
        {
            const int phase=block/256;
            for(int i=0;i<128;++i)
            {
                const auto t=double(block*128+i)/48000,k=2*MathConstants<double>::pi*t;
                double input=.025*(std::sin(k*110)+.3*std::sin(k*329.63));
                if(phase==1)input=.15*(std::sin(k*110)+.35*std::sin(k*220)+.2*std::sin(k*330))*std::exp(-5*std::fmod(t,.25));
                if(phase==2){input=0;for(double hz:{82.41,123.47,164.81,207.65,246.94,329.63})input+=.075*(std::sin(k*hz)+.2*std::sin(k*2*hz));input*=std::exp(-3*std::fmod(t,.4));}
                audio.setSample(0,i,float(input));audio.setSample(1,i,float(input));
            }
            processor.processBlock(audio,midi);ampNode->getProcessor()->processBlock(audio,midi);normalization.processBlock(audio,midi);equalizer.processBlock(audio,midi);spread.processBlock(audio,midi);mod.processBlock(audio,midi);delay.processBlock(audio,midi);reverb.processBlock(audio,midi);
            for(int channel=0;channel<2;++channel)for(int i=0;i<128;++i){const auto value=audio.getSample(channel,i);finite&=std::isfinite(value);peak=jmax(peak,std::abs(double(value)));energy+=double(value)*value;phasePeak[size_t(phase)]=jmax(phasePeak[size_t(phase)],std::abs(double(value)));phaseEnergy[size_t(phase)]+=double(value)*value;}
        }
        check(finite&&energy>1.e-12&&!processor.fault.load()&&!native->takeFault(),"rank "+String(p.rank)+" full serial chain processes finite non-silent audio");
        check(peak<.98,"rank "+String(p.rank)+" output retains headroom for the hard-chord reference stimulus");
        stats.writeText(String(p.rank)+",\""+p.title().replace("\"","\"\"")+"\","+String(Decibels::gainToDecibels(peak,-120.0),2)+","+(finite?"true":"false")+",12.5,"+String(Decibels::gainToDecibels(phasePeak[0],-120.0),2)+","+String(Decibels::gainToDecibels(std::sqrt(phaseEnergy[1]/(2*256*128)),-120.0),2)+","+String(Decibels::gainToDecibels(phasePeak[2],-120.0),2)+"\n",false,false,nullptr);
        if(p.rank==1)
        {
            plan.state.outputDsp->ampNormalizationEnabled=true;check(store.save(0,plan.state,none,lookup).result.wasOk(),"explicit normalization ON saved in User Preset");
        }
        loaded.source->unload();graph.clear();
    }
    evidence.getChildFile("tonebank-render.csv").replaceWithData(stats.getData(),stats.getDataSize());
    const auto userHash=SHA256(store.slotFile(0)).toHexString();const auto cache=tonebank::cacheForApplicationData(root);
    auto stale=tonebank::Json::parse(shipped.loadFileAsString().toStdString());stale["libraryRevision"]="tonegrid-library-1.1.20";stale["bankVersion"]="1.1.20-r1";cache.getParentDirectory().createDirectory();cache.replaceWithText(String(stale.dump()));
    auto refreshed=tonebank::readCache(cache,shipped);check(refreshed.result.wasOk()&&refreshed.bank.version==bank.version&&SHA256(store.slotFile(0)).toHexString()==userHash,"legacy official cache replaced; User Preset bytes unchanged");
    board::State user;check(store.load(0,none,lookup,user).wasOk()&&user.outputDsp->ampNormalizationEnabled,"explicit User Preset normalization ON recalled");
    distribution116Tests::FakeTransport transport;transport.bank=String(stale.dump());auto remote=updates::fetch(transport,cancelled,root.getChildFile("online.xml"),cache,JUCE_APPLICATION_VERSION_STRING,{},false,true,Time::currentTimeMillis());
    check(remote.bankStatus.failed()&&cache.loadFileAsString()==shipped.loadFileAsString()&&SHA256(store.slotFile(0)).toHexString()==userHash,"old server bank cannot downgrade new library or touch User Presets");
    // Same-name personal file in a new library survives; bundled bytes remain available.
    auto manifest=tonebank::Json::parse(factoryFolder.getChildFile("manifest.json").loadFileAsString().toStdString());manifest["amps"].erase(manifest["amps"].begin()+1,manifest["amps"].end());manifest["ampCount"]=1;manifest.erase("assets");manifest.erase("metadataUpdates");
    const auto small=root.getChildFile("SmallFactory"),collision=root.getChildFile("CollisionLibrary");const auto name=String(manifest["amps"][0]["filename"].get<std::string>());
    small.getChildFile(name).getParentDirectory().createDirectory();factoryFolder.getChildFile(name).copyFileTo(small.getChildFile(name));small.getChildFile("manifest.json").replaceWithText(String(manifest.dump()));collision.getChildFile(name).getParentDirectory().createDirectory();collision.getChildFile(name).replaceWithText("personal capture sentinel");
    check(factory::seed(small,collision,cancelled).wasOk()&&collision.getChildFile(name).loadFileAsString()=="personal capture sentinel"&&SHA256(collision.getChildFile("Factory/"+name)).toHexString()==String(manifest["amps"][0]["sha256"].get<std::string>()),"new install preserves personal collision and installs bundled original separately");
    check(std::all_of(bank.presets.begin(),bank.presets.end(),[](const auto& preset){const auto path=preset.amp.file;const auto number=path.substring(0,2).getIntValue();return path.containsChar('/')&&number>=1&&number<=6;}),"all 101 amp references use the six approved current categories");
    // Synthetic identities exercise migration without redistributing withdrawn
    // real captures in test fixtures or deleting any files outside the temp root.
    const auto migration=root.getChildFile("Migration");migration.getChildFile("Factory").createDirectory();
    const String withdrawn="01 - withdrawn test.nam",modified="02 - withdrawn modified.nam",sentinel="synthetic former capture";
    migration.getChildFile(withdrawn).replaceWithText(sentinel);migration.getChildFile("Factory/"+withdrawn).replaceWithText(sentinel);
    const auto oldHash=SHA256(migration.getChildFile(withdrawn)).toHexString();
    migration.getChildFile(modified).replaceWithText("personal replacement");migration.getChildFile("Factory/"+modified).replaceWithText("other personal replacement");
    const auto approved=migration.getChildFile("20 - approved/"+withdrawn);approved.getParentDirectory().createDirectory();approved.replaceWithText(sentinel);
    manifest["retiredAmps"]=tonebank::Json::array({{{"filename",withdrawn.toStdString()},{"sha256",oldHash.toStdString()}},{{"filename",modified.toStdString()},{"sha256",oldHash.toStdString()}}});
    small.getChildFile("manifest.json").replaceWithText(String(manifest.dump()));
    check(factory::seed(small,migration,cancelled).wasOk()&&!migration.getChildFile(withdrawn).exists()&&!migration.getChildFile("Factory/"+withdrawn).exists(),"withdrawn exact root and Factory copies removed during upgrade");
    check(approved.existsAsFile()&&SHA256(approved).toHexString()==oldHash&&migration.getChildFile(modified).loadFileAsString()=="personal replacement"&&migration.getChildFile("Factory/"+modified).loadFileAsString()=="other personal replacement","same bytes in approved folders and modified personal copies survive retirement");
    check(factory::seed(small,migration,cancelled).wasOk()&&approved.existsAsFile(),"retirement is idempotent after installation marker");
    // A malformed retirement list must fail before any current asset is copied
    // or any retired identity is removed.
    const auto invalid=root.getChildFile("InvalidMigration");invalid.createDirectory();invalid.getChildFile(withdrawn).replaceWithText(sentinel);
    manifest["retiredAmps"].push_back({{"filename","../escape.nam"},{"sha256",oldHash.toStdString()}});small.getChildFile("manifest.json").replaceWithText(String(manifest.dump()));
    check(factory::seed(small,invalid,cancelled).failed()&&invalid.getChildFile(withdrawn).existsAsFile()&&!invalid.getChildFile(name).exists(),"malformed retired path fails before installation and deletion");
    board::RigStore starterStore(root.getChildFile("Starters"),root.getChildFile("NoLegacy"));auto seededStarters=starters::seed(payload.getChildFile("StarterPresets"),starterStore,catalog,lookup);
    check(seededStarters.result.wasOk()&&seededStarters.installed&&seededStarters.startupSlot==0,"three publisher starter presets seed a clean new profile");
    for(int i=0;i<3;++i){board::State state;PluginDescription description;const auto result=starterStore.loadForSelection(i,lookup,state,description);check(result.wasOk()&&state.namAmp.gainDb==0&&state.namAmp.volumeDb==0&&state.outputDsp&&state.outputDsp->ampEq.enabled==(i==2)&&state.outputDsp->ampNormalizationEnabled&&state.outputDsp->ampNormalizationEnhanced,"starter "+String(i+1)+" resolves with publisher settings, intentional EQ, Full normalization");}
    board::State seedState;PluginDescription seedDescription;starterStore.loadForSelection(0,lookup,seedState,seedDescription);auto legacyOrange=board::encode(seedState,seedDescription,lookup);
    const std::array<const char*,3> withdrawnOrange{{"905f6d565094b63f6c33bee1f5159b64e61142f1f8e75fb7fda31aeefe9a40fe","88f7057da94b6306c561f649743703f8cbfe5d18024a18be046db8ebfa3054c9","4f5e84f86b7d5b0b958f4bdfae5455ea79eb05bac5e2af87605727982be304a2"}};
    for(int i=0;i<3;++i){auto xml=legacyOrange;auto* pedal=xml.getChildByAttribute("id","3");pedal->setAttribute("preset",i);pedal->setAttribute("model",withdrawnOrange[size_t(i)]);pedal->setAttribute("enabled",true);board::State migrated;check(board::decode(xml,seedDescription,lookup,migrated).wasOk()&&migrated.pedals[3].preset==i*2&&migrated.pedals[3].enabled,"withdrawn DS-1 variant "+String(i)+" migrates to approved Wampler by identity");}
    for(const auto* hash:{"82fa446758f20d7994dec3df6578d31a1c4aafc0743727a2684480db646cde85","964672fd9b2ab0bb7aed8a00a69fdfa855e0885623ded9486629b34558fdfdb0","7aa1084d47124551a24e3596db848fe21355e86829d75c8673ef190412f1b22b","dfbdf7f0fedffe6cd7a6486cacc17bef1e1ab5244b8217660726969ad7b0285c","6dd5d6e9a91a5a623878e7d8d47828dbc5332c19e7dc2e7b4628b931a700b6b9"})
    {
        auto xml=legacyOrange;auto* amp=xml.getChildByName("AmpSource");amp->setAttribute("sha256",hash);amp->setAttribute("file","05 - High Gain/Silver_Jubilee_1987_INTERNAL USE ONLY/Previous.nam");
        xml.getChildByName("OutputDSP")->getChildByName("AmpEq")->setAttribute("enabled",true);
        board::State migrated;const auto result=board::decode(xml,seedDescription,lookup,migrated);
        check(result.wasOk()&&migrated.namAmp.model.sha256=="3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707"&&migrated.outputDsp->ampEq.enabled&&migrated.missingCaptureDiagnostics.joinIntoString(" ").contains("Silver Jubilee"),"withdrawn Silver Jubilee User Preset migrates to Wizard without changing personal EQ");
    }
    const auto starterHash=SHA256(starterStore.slotFile(0)).toHexString();auto repeat=starters::seed(payload.getChildFile("StarterPresets"),starterStore,catalog,lookup);check(repeat.result.wasOk()&&!repeat.installed&&SHA256(starterStore.slotFile(0)).toHexString()==starterHash,"starter installation is idempotent and never overwrites existing User Presets");
    check(root.isAChildOf(parent)&&root.deleteRecursively(),"isolated temporary files removed");
    report+=String(checks)+" Release120 integration checks; "+String(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
