#pragma once
#include "TunerControls.h"
#include "Content/Amps/NamLibraryService.h"
#include "Content/Artwork/ThumbnailCache.h"
#include "ParameterMailbox.h"
#include "NamAmpTests.h"
#include "Content/Presets/ToneBank.h"
#include "RealtimeAllocationProbe.h"
#include <random>
#include <set>

namespace release113Tests
{
inline bool run(const juce::File& models,juce::String& report)
{
    using namespace juce;using namespace tonegrid;
    bool ok=true;int checks=0;auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+"1.1.13 "+label+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-113-tests-"+Uuid().toString());root.createDirectory();
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    for(double note:{41.203,61.735,73.416,82.407,110.0,146.832,195.998,246.942,329.628,440.0,1318.51})
        for(double detune:{-23.0,0.0,31.0})
        {
            const double frequency=note*std::pow(2.,detune/1200.);std::array<float,2048> wave{};
            for(size_t i=0;i<wave.size();++i){const double phase=2*MathConstants<double>::pi*frequency*i/24000;wave[i]=float(.3*std::sin(phase)+.12*std::sin(2*phase)+.06*std::sin(3*phase));}
            const auto reading=tuning::Engine::analyse(wave,24000);
            const double error=reading.frequency>0?1200*std::log2(reading.frequency/frequency):10000;
            check(std::abs(error)<2&&reading.confidence>.9,"tuner harmonic note "+String(note,2)+" Hz / "+String(detune,0)+" cents, error "+String(error,2));
        }
    std::array<float,2048> silence{},dc{},noise{};dc.fill(.25f);std::mt19937 generator(123);std::uniform_real_distribution<float> random(-.2f,.2f);for(auto& x:noise)x=random(generator);
    check(tuning::Engine::analyse(silence,12000).frequency==0&&tuning::Engine::analyse(dc,12000).frequency==0&&tuning::Engine::analyse(noise,12000).frequency==0,"tuner rejects silence, DC and deterministic broadband noise");
    {
        tuning::Engine engine;engine.prepare(48000);engine.enable(true);Thread::sleep(40);std::array<float,960> input{};int position=0;
        realtimeProbe::begin();for(int i=0;i<4;++i)engine.push(input.data(),960);const auto allocations=realtimeProbe::end();
        check(allocations==0,"input tap FIFO has no audio-thread allocation");
        for(int block=0;block<30;++block){for(auto& x:input)x=.3f*std::sin(float(2*MathConstants<double>::pi*110*position++/48000));engine.push(input.data(),960);Thread::sleep(20);}
        Thread::sleep(80);auto value=engine.reading();check(value.frequency>0&&std::abs(1200*std::log2(value.frequency/110))<2,"background FIFO analysis detects A2 at 48000 Hz");
        AudioProcessorPlayer player;tuning::AudioTap tap(player,engine);tuning::Controls controls(engine,tap);controls.open();check(tap.isMuted()&&engine.active(),"opening tuner enables raw input analysis and mute");
        tap.audioDeviceStopped();check(tap.isMuted()&&engine.active(),"device restart preserves tuner popup choices");controls.close();Thread::sleep(80);check(!tap.isMuted()&&!engine.active()&&engine.reading().frequency==0,"closing tuner restores output and clears stale note");
    }
    {
        struct Pair{uint64_t value=0,inverse=~uint64_t(0);};ParameterMailbox<Pair> mailbox;std::atomic<bool> done{false};std::atomic<bool> torn{false};
        std::thread writer([&]{for(uint64_t i=1;i<=200000;++i)mailbox.publish({i,~i});done.store(true);});
        realtimeProbe::begin();while(!done.load()){const auto value=mailbox.read();if(value.inverse!=~value.value)torn.store(true);}const auto allocations=realtimeProbe::end();writer.join();
        const auto final=mailbox.read();check(!torn.load()&&final.value==200000&&allocations==0,"settings mailbox publishes coherent tuples under concurrent stress without audio allocations");
    }
    const auto preference=root.getChildFile("Audio/nam-quality.xml");
    check(!namquality::read(preference)&&namquality::write(preference,true)&&namquality::read(preference),"NAM Lite preference writes atomically and survives a fresh read");
    check(namquality::write(preference,false)&&!namquality::read(preference),"Full preference round trip");preference.replaceWithText("<NamQuality version='999' lite='1'/>");check(!namquality::read(preference),"unknown/corrupt quality preference defaults to Full");
    {
        const auto json=nlohmann::json::parse(namTests::linear().toStdString());nam::DspLoadOptions options;options.prewarm=false;auto linear=nam::get_dsp(json,options);
        check(!namquality::apply(*linear,true),"non-slimmable NAM keeps its normal model without a failed load");
        auto files=models.findChildFiles(File::findFiles,true,"*.nam");bool found=false;
        for(const auto& file:files){auto payload=nlohmann::json::parse(file.loadFileAsString().toStdString());if(payload["architecture"]!="SlimmableContainer")continue;auto model=nam::get_dsp(payload,options);found=true;
            std::array<float,256> input{},full{},lite{};for(size_t i=0;i<input.size();++i)input[i]=.15f*std::sin(float(i*.08));float* in=input.data();float* out=full.data();
            const bool fullMode=namquality::apply(*model,false);model->Reset(48000,256);model->process(&in,&out,256);
            const bool liteMode=namquality::apply(*model,true);model->Reset(48000,256);out=lite.data();model->process(&in,&out,256);bool finite=true,different=false;for(size_t i=0;i<full.size();++i){finite&=std::isfinite(full[i])&&std::isfinite(lite[i]);different|=full[i]!=lite[i];}
            check(fullMode&&liteMode&&finite&&different,"real A2 capture selects distinct, finite Full and Lite outputs");
            const auto ampFile=root.getChildFile("Amp/A2.nam");ampFile.getParentDirectory().createDirectory();file.copyFileTo(ampFile);namamp::Library ampLibrary(ampFile.getParentDirectory());namamp::Entry entry;auto eligible=ampLibrary.read(ampFile,entry);auto loaded=amp::loadNamAmp(ampLibrary,{entry.identity,0,0},true);
            auto* facade=dynamic_cast<amp::NamAmp*>(loaded.source.get());bool rendered=false;
            if(facade){rig::StereoGraph graph;auto node=loaded.source->addToGraph(graph);auto* processor=dynamic_cast<amp::NamAmpProcessor*>(node->getProcessor());processor->prepareToPlay(48000,96);AudioBuffer<float> audio(2,96);audio.clear();audio.setSample(0,0,.1f);MidiBuffer midi;processor->processBlock(audio,midi);rendered=!facade->takeFault();for(int i=0;i<96;++i)rendered&=std::isfinite(audio.getSample(0,i));loaded.source->unload();graph.clear();}
            check(eligible.wasOk()&&loaded.result.wasOk()&&facade&&rendered,"initial Lite amp factory prepares and renders a real A2 model without an earlier Full session");break;}
        check(found,"distribution includes native slimmable A2 captures");
        board::Processor liteBoard;check(liteBoard.load(models,true).wasOk(),"initial Lite startup loads and prewarms every active pedal capture");
    }
    {
        namamp::Library library(root.getChildFile("Library"));library.folder().createDirectory();for(int i=0;i<120;++i)library.folder().getChildFile(String(i)+".nam").replaceWithText(namTests::linear());library.folder().getChildFile("bad.nam").replaceWithText("invalid");
        namamp::LibraryService service(library);namamp::Catalog catalog;Result result=Result::ok();
        const auto await=[&]{const auto end=Time::getMillisecondCounter()+10000;while(service.busy()&&Time::getMillisecondCounter()<end){service.poll(catalog,result);Thread::sleep(1);}return !service.busy()&&result.wasOk();};
        service.refresh();check(await()&&catalog.entries.size()==120&&catalog.failures.size()==1&&service.parsedCount()==121,"async cold scan publishes complete catalog and one malformed-file diagnostic");
        service.refresh();check(await()&&service.parsedCount()==0&&service.reusedCount()==121,"unchanged scan reuses all entries and failed-file metadata");
        library.folder().getChildFile("1.nam").replaceWithText(namTests::linear(.125f));library.folder().getChildFile("2.nam").deleteFile();library.folder().getChildFile("new.nam").replaceWithText(namTests::linear(.75f));
        service.refresh();check(await()&&service.parsedCount()==2&&catalog.entries.size()==120,"refresh parses only changed/new files and removes deleted entry");
        const auto imported=library.folder().getChildFile("Imported.nam");imported.replaceWithText(namTests::linear());namamp::Entry entry;library.read(imported,entry);service.refresh();service.merge({entry},catalog);
        check(await()&&std::count_if(catalog.entries.begin(),catalog.entries.end(),[&](const auto& e){return e.identity.file=="Imported.nam";})==1,"import invalidates stale in-flight scan and merges exactly once");
    }
    {
        board::RigStore store(root.getChildFile("Rigs"),root.getChildFile("NoLegacy"));store.directory().createDirectory();
        const String a="<PedalRig name='A'><Opaque>unchanged A</Opaque></PedalRig>",b="<PedalRig name='B'><Opaque>unchanged B</Opaque></PedalRig>";
        store.slotFile(2).replaceWithText(a);store.nameFile(2).replaceWithText("A");store.slotFile(3).replaceWithText(b);store.nameFile(3).replaceWithText("B");auto midi=store.directory().getChildFile("midi.xml");midi.replaceWithText("fixed MIDI assignments");const auto hash=SHA256(midi).toHexString();
        check(store.swap(2,3).wasOk()&&store.slotFile(2).loadFileAsString()==b&&store.slotFile(3).loadFileAsString()==a&&store.readName(2)=="B"&&store.readName(3)=="A"&&SHA256(midi).toHexString()==hash,"adjacent move swaps exact preset bytes/names and preserves MIDI assignment file");
        check(store.swap(3,4).wasOk()&&!store.contains(3)&&!store.nameFile(3).exists()&&store.slotFile(4).loadFileAsString()==a,"move into empty slot preserves state and clears original pair");
        check(store.swap(4,6).failed()&&store.swap(4,-1).failed()&&store.slotFile(4).loadFileAsString()==a,"invalid/boundary moves preserve original preset");
        const auto journal=store.directory().getChildFile(".preset-transaction-interrupted");journal.createDirectory();store.slotFile(4).copyFileTo(journal.getChildFile("0"));journal.getChildFile("journal.xml").replaceWithText("<PresetTransaction version='1'><File file='slot-005.tonexrig' backup='0' existed='1'/></PresetTransaction>");store.slotFile(4).replaceWithText("interrupted replacement");
        check(store.recover().wasOk()&&store.slotFile(4).loadFileAsString()==a&&!journal.exists(),"restart recovery restores original bytes from interrupted transaction");
        journal.createDirectory();store.slotFile(4).copyFileTo(journal.getChildFile("0"));journal.getChildFile("journal.xml").replaceWithText("<PresetTransaction version='1'><File file='slot-005.tonexrig' backup='0' existed='corrupt'/></PresetTransaction>");
        check(store.recover().failed()&&store.slotFile(4).loadFileAsString()==a&&journal.getChildFile("0").loadFileAsString()==a,"corrupt recovery boolean cannot delete a preset or its recovery backup");
    }
    {
        tonebank::Bank bank;tonebank::parse(File(TONEGRID_BANK_FILE).loadFileAsString(),bank);std::set<size_t> covered;int total=0;
        for(const auto& group:tonebank::genreGroups()){auto rows=tonebank::filter(bank,group,{});for(auto row:rows)covered.insert(row);total+=int(rows.size());}
        check(covered.size()==bank.presets.size()&&total==(int)bank.presets.size(),"five genres partition the complete song bank exactly once");
        PresetBrowser browser;browser.setBank(bank);auto* combo=dynamic_cast<ComboBox*>(browser.findChildWithID("tonebank-genre"));
        bool genres=combo&&combo->getNumItems()==int(bank.genres.size())+1;
        if(genres)for(int i=0;i<bank.genres.size();++i)genres&=combo->getItemText(i+1)==bank.genres[i];
        check(genres,"genre browser displays every declared JSON genre plus All");
        browser.setUsers({"A","B","empty"},{true,true,false});auto* list=dynamic_cast<ListBox*>(browser.findChildWithID("preset-list"));auto* row=list?list->getComponentForRowNumber(0):nullptr;
        auto* up=row?dynamic_cast<Button*>(row->findChildWithID("preset-up")):nullptr;auto* down=row?dynamic_cast<Button*>(row->findChildWithID("preset-down")):nullptr;int moved=-1,direction=0;browser.onMove=[&](int slot,int delta){moved=slot;direction=delta;};if(down)down->onClick();check(up&&down&&!up->isEnabled()&&down->isEnabled()&&moved==0&&direction==1,"preset arrows enforce boundaries and send the correct move");
    }
    Image image(Image::RGB,20,16,true);MemoryOutputStream png;PNGImageFormat().writeImageToStream(image,png);check(t3k::boundedImage(png.getData(),png.getDataSize()).isValid(),"bounded thumbnail decoder accepts valid PNG");
    auto huge=png.getMemoryBlock();auto* bytes=static_cast<unsigned char*>(huge.getData());bytes[16]=0x7f;check(!t3k::boundedImage(huge.getData(),huge.getSize()).isValid(),"oversized encoded dimensions rejected before allocation/decode");
    report+=String(checks)+" release 1.1.13 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
