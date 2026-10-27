#pragma once
#include "Content/Presets/BoardState.h"
#include "SelfTests.h"
inline bool runBoardTests(const juce::File& directory,juce::String& report)
{
    using namespace juce; bool ok=runSelfTests(report); int checks=0;
    auto check=[&](bool value,const String& name){++checks;report+=(value?"PASS ":"FAIL ")+name+"\n";ok=ok&&value;};
    {
        board::InputStage stage;std::array<float,128> samples;
        auto run=[&](float input,int blocks){for(int b=0;b<blocks;++b){samples.fill(input);stage.process(samples.data(),128);}return samples.back();};
        stage.prepare(48000);check(run(.125f,1)==.125f,"Guitar IN unity and gate bypass preserve samples exactly");
        stage.set({6,-62,false});const float boosted=run(.1f,20);check(std::abs(boosted-.1f*Decibels::decibelsToGain(6.f))<.00001f,"Guitar IN measured +6 dB gain");
        stage.set({-12,-62,false});check(std::abs(run(.1f,20)-.1f*Decibels::decibelsToGain(-12.f))<.00001f,"Guitar IN measured -12 dB gain");
        stage.set({0,-50,true});stage.prepare(48000);check(run(.0001f,100)<.000001f,"Gate closes below threshold");
        check(std::abs(run(.1f,30)-.1f)<.00001f,"Gate opens for a signal above threshold");
        const auto firstRelease=run(.0001f,1);const auto closed=run(.0001f,400);check(firstRelease>closed&&closed<.0000001f,"Gate releases progressively and closes after hold");
        stage.set({0,-50,false});check(std::abs(run(.0001f,30)-.0001f)<.000001f,"Gate OFF restores quiet signal");
        stage.takePeak();run(1.2f,1);check(stage.takePeak()>=1.2f&&stage.takePeak()==0,"Input meter holds clipping peak until consumed");
        stage.set({0,-50,true});stage.prepare(48000);run(.02f,20);const float threshold=Decibels::decibelsToGain(-50.f);check(run(threshold*.75f,80)>threshold*.70f,"Gate hysteresis preserves signals between opening and closing thresholds");
    }
    {
        board::ToneTilt tilt; tilt.prepare(48000);
        bool exact=true;for(int i=0;i<2048;++i){const float x=(float)std::sin(i*.39)*.37f;exact=exact&&tilt.process(x)==x;}
        check(exact,"Tone zero preserves samples exactly");
        auto response=[&](float db,double hz){tilt.prepare(48000,db);double in=0,out=0;for(int i=0;i<96000;++i){const float x=.1f*(float)std::sin(2*MathConstants<double>::pi*hz*i/48000);const float y=tilt.process(x);if(i>=48000){in+=x*x;out+=y*y;}}return 10*std::log10(out/in);};
        for(float db:{-6.f,6.f})
        {
            check(std::abs(response(db,20)+db)<.03,"Tone low end approaches opposite 6 dB shelf: "+String(db));
            check(std::abs(response(db,1000))<.001,"Tone pivot remains 0 dB at 1 kHz: "+String(db));
            check(std::abs(response(db,20000)-db)<.03,"Tone high end approaches 6 dB shelf: "+String(db));
        }
        tilt.prepare(48000);bool stable=true;float largest=0,previous=0;
        for(int i=0;i<48000;++i){if(i%1200==0)tilt.setTone((i/1200)%2?6.f:-6.f);const float y=tilt.process(.1f);stable=stable&&std::isfinite(y)&&std::abs(y)<.25f;if(i>100)largest=jmax(largest,std::abs(y-previous));previous=y;}
        check(stable&&largest<.002f,"Rapid Tone changes are finite and smoothly ramped");
    }
    {
        board::GraphicEq eq;std::array<float,7> bands{};eq.prepare(48000,bands);bool exact=true;
        for(int i=0;i<4000;++i){float x=.2f*(float)std::sin(i*.117);exact=exact&&eq.process(x)==x;}
        check(exact,"Graphic EQ flat preserves samples exactly");
        for(int band=0;band<7;++band)for(float db:{-12.f,12.f})
        {bands.fill(0);bands[(size_t)band]=db;eq.prepare(48000,bands);double in=0,out=0;for(int i=0;i<48000;++i){float x=.05f*(float)std::sin(2*MathConstants<double>::pi*board::eqFrequencies[(size_t)band]*i/48000);float y=eq.process(x);if(i>24000){in+=x*x;out+=y*y;}}check(std::abs(10*std::log10(out/in)-db)<.02,"EQ measured band "+String(band)+" gain "+String(db));}
        bool stable=true;for(int i=0;i<48000;++i){if(i%512==0){bands.fill((i/512)%2?12.f:-12.f);eq.set(bands);}float y=eq.process(.1f*(float)std::sin(i*.113));stable=stable&&std::isfinite(y)&&std::abs(y)<4;}
        check(stable,"Graphic EQ rapidly changing all seven bands remains bounded");
        board::AutoWah wah;auto feed=[&](float amplitude,int samples){for(int i=0;i<samples;++i){float y=wah.process(amplitude*(float)std::sin(i*.18));stable=stable&&std::isfinite(y);}return wah.currentFrequency();};
        wah.prepare(48000,55,75,160);double soft=feed(.005f,12000),loud=feed(.5f,12000);check(loud>soft*2,"Auto Wah opens with playing strength");
        wah.prepare(48000,20,75,160);double lowSens=feed(.03f,12000);wah.prepare(48000,80,75,160);check(feed(.03f,12000)>lowSens*1.5,"Auto Wah sensitivity changes envelope response");
        wah.prepare(48000,100,10,160);double narrow=feed(.5f,12000);wah.prepare(48000,100,100,160);check(feed(.5f,12000)>narrow*2,"Auto Wah range changes upper sweep frequency");
        wah.prepare(48000,55,75,50);feed(.5f,12000);double fast=feed(0,12000);wah.prepare(48000,55,75,800);feed(.5f,12000);check(feed(0,12000)>fast*2,"Auto Wah decay controls return speed");
        check(stable,"Auto Wah output remains finite");
        for(const auto test:std::array<std::array<float,2>,2>{{{{-18,75}},{{-6,35}}}})
        {
            wah.prepare(48000,test[1],70,180);double inEnergy=0,outEnergy=0;float peak=0;
            for(int i=0;i<24000;++i){float x=Decibels::decibelsToGain(test[0])*(float)std::sin(2*MathConstants<double>::pi*110*i/48000);float y=wah.process(x);if(i>12000){inEnergy+=x*x;outEnergy+=y*y;}peak=jmax(peak,std::abs(y));}
            const double db=10*std::log10(outEnergy/inEnergy);
            check(wah.currentFrequency()>1700,"Wah opens for guitar peaks "+String(test[0])+" dBFS at Sens "+String(test[1]));
            check(db>-2&&db<2&&peak<=1,"Wah open low-frequency body near unity: "+String(db,2)+" dB");
        }
        bool bounded=true;
        for(double rate:{44100.,48000.,96000.})for(int p=0;p<8;++p){const auto& preset=board::wahPresets[(size_t)p];wah.prepare(rate,preset.sensitivity,preset.range,preset.decay,p);for(int i=0;i<24000;++i){float x=i%2000<80?4.f:(float)(.9*std::sin(i*.23)+.8*std::sin(i*.031));float y=wah.process(x);bounded=bounded&&std::isfinite(y)&&std::abs(y)<=1.f;}}
        check(bounded,"Wah soft limiter bounds transients at 0 dBFS across all eight presets");
        auto response=[&](double hz){wah.prepare(48000,0,75,160);double in=0,out=0;for(int i=0;i<48000;++i){const float x=.000001f*(float)std::sin(2*MathConstants<double>::pi*hz*i/48000);const float y=wah.process(x);if(i>24000){in+=x*x;out+=y*y;}}return 10*std::log10(out/in);};
        const auto vowelPeak=response(260),bass=response(80),treble=response(2000);
        check(vowelPeak>bass+8&&vowelPeak>treble+15,"Wah moving vowel has a pronounced spectral peak: "+String(vowelPeak,2)+" dB");
        wah.prepare(48000,55,75,160,5);double reverseRest=feed(.001f,12000),reversePick=feed(.4f,12000);
        check(reverseRest>reversePick*3,"Reverse funk sweeps down on strong notes");
        wah.prepare(48000,55,75,160);bool smooth=true;float previous=0;
        for(int i=0;i<96000;++i){if(i%6000==0){int p=(i/6000)%8;const auto& v=board::wahPresets[(size_t)p];wah.set(v.sensitivity,v.range,v.decay,p);}const float y=wah.process(.01f*(float)std::sin(i*.06));smooth=smooth&&std::isfinite(y)&&std::abs(y-previous)<.05f;previous=y;}
        check(smooth,"Live changes of all eight Wah voices remain smooth and finite");

    }
    board::Processor processor; auto result=processor.load(directory);check(result.wasOk(),"Load and validate all 147 captures: "+result.getErrorMessage());if(result.failed())return false;
    {
        board::Processor reference;const auto loaded=reference.load(directory);check(loaded.wasOk(),"Load reference for serial NAM path test");
        if(loaded.wasOk())
        {
            auto setting=board::defaults(1,0);setting.enabled=true;reference.set(1,setting);reference.prepareToPlay(48000,128);
            setting.input=6;setting.tone=6;setting.output=-3;setting.mix=0;processor.set(1,setting);processor.prepareToPlay(48000,128);
            board::ToneTilt expected;expected.prepare(48000,6);AudioBuffer<float> a(2,128),b(2,128);MidiBuffer m;float error=0;
            for(int block=0;block<80;++block){for(int j=0;j<128;++j){float x=.04f*(float)std::sin((block*128+j)*.031);a.setSample(0,j,x);b.setSample(0,j,x*Decibels::decibelsToGain(6.f));}processor.processBlock(a,m);reference.processBlock(b,m);for(int j=0;j<128;++j)error=jmax(error,std::abs(a.getSample(0,j)-expected.process(b.getSample(0,j))*Decibels::decibelsToGain(-3.f)));}
            check(error<.00001f,"Measured Input Trim -> NAM -> Tone -> Level, with no dry blend even for legacy mix=0");
            setting.enabled=false;processor.set(1,setting);for(int i=0;i<12;++i){a.clear();processor.processBlock(a,m);}
        }
    }
    processor.prepareToPlay(48000,128); AudioBuffer<float> buffer(2,128); MidiBuffer midi;
    auto fill=[&](int block){for(int i=0;i<128;++i){const float s=.08f*(float)std::sin(2*MathConstants<double>::pi*220*(i+block*128)/48000);buffer.setSample(0,i,s);buffer.setSample(1,i,s);}};
    fill(0);const float original=buffer.getSample(0,20);processor.processBlock(buffer,midi);
    check(buffer.getSample(0,20)==original && buffer.getSample(1,20)==original,"All bypass preserves input exactly");
    processor.prepareToPlay(44100,128);fill(0);processor.processBlock(buffer,midi);check(buffer.getMagnitude(0,128)==0,"Reject wrong sample rate without playing pitch-shifted NAM");processor.prepareToPlay(48000,128);
    double worstRatio=0;
    for(int p=0;p<board::count;++p)
    {
        for(int n=0;n<board::presetCount(p);++n)
        {
            auto s=board::defaults(p,n);s.enabled=true;processor.set(p,s);bool finite=true;double energy=0;
            auto t=Time::getMillisecondCounterHiRes();
            for(int b=0;b<100;++b){fill(b);processor.processBlock(buffer,midi);for(int j=0;j<128;++j){const auto x=buffer.getSample(0,j);finite=finite&&std::isfinite(x);if(b>10)energy+=(double)x*x;}}
            double ratio=(Time::getMillisecondCounterHiRes()-t)/(12800.0/48.0);worstRatio=jmax(worstRatio,ratio);
            check(finite&&energy>1.e-9&&!processor.fault.load(),board::pedalName(p)+" preset "+String(n+1)+": finite, audible output");
        }
        auto off=processor.get(p);off.enabled=false;processor.set(p,off);for(int b=0;b<8;++b){fill(b);processor.processBlock(buffer,midi);}
    }
    report+="Worst single-pedal offline CPU fraction: "+String(worstRatio,3)+"\n";
    // Boost's actual measured ratio is checked independently of the parameter representation.
    // Let the 10 ms control smoothing settle after the prior +12 dB preset.
    auto boost=board::defaults(6,2);boost.enabled=true;processor.set(6,boost);for(int b=0;b<100;++b){fill(b);processor.processBlock(buffer,midi);}
    fill(30);const float x=buffer.getSample(0,20);processor.processBlock(buffer,midi);check(std::abs(buffer.getSample(0,20)/x-Decibels::decibelsToGain(3.f))<.002f,"Clean boost produces +3 dB");
    PluginDescription plugin;plugin.name="Test TONEX";plugin.pluginFormatName="VST3";plugin.fileOrIdentifier="test.vst3";plugin.uniqueId=123;plugin.version="test";
    board::State state;state.name="Round trip";const char bytes[]={1,2,3,4};state.amp.append(bytes,4);
    state.input={4.5f,-57,true};
    for(int p=0;p<board::count;++p){state.pedals[(size_t)p]=board::defaults(p,(7-p+16)%board::presetCount(p));state.pedals[(size_t)p].enabled=(p%2)==0;}
    state.pedals[9].wahSensitivity=83;state.pedals[9].wahRange=37;state.pedals[9].wahDecay=333;state.pedals[12].eqBands={-5,3,7,0,-4,2,9};
    state.pedals[1].tone=4.2f;state.pedals[4].tone=-5.1f;
    auto xml=board::encode(state,plugin,processor);board::State restored;
    check(board::decode(xml,plugin,processor,restored).wasOk()&&restored.amp==state.amp&&restored.pedals[1].preset==6&&restored.pedals[0].enabled,"Rig round trip includes amp and pedal settings");
    check(restored.input.gainDb==4.5f&&restored.input.thresholdDb==-57&&restored.input.gate&&restored.pedals[0].ratio==state.pedals[0].ratio,"Preset restores Guitar IN, gate and compressor ratio");
    check(restored.pedals[1].tone==state.pedals[1].tone&&restored.pedals[4].tone==state.pedals[4].tone,"Positive and negative Tone survive rig round trip");
    check(restored.pedals[9].wahSensitivity==83&&restored.pedals[9].wahRange==37&&restored.pedals[9].wahDecay==333&&restored.pedals[12].eqBands==state.pedals[12].eqBands,"Wah controls and seven EQ bands round trip");
    auto legacyWah=board::encode(state,plugin,processor);legacyWah.getChildByAttribute("id","9")->setAttribute("model","auto-wah-v1");
    check(board::decode(legacyWah,plugin,processor,restored).wasOk()&&restored.pedals[9].wahSensitivity==83&&restored.pedals[9].preset==state.pedals[9].preset,"Previous Wah rigs retain controls and preset index with the new engine");
    auto legacy=board::encode(state,plugin,processor);legacy.setAttribute("version",3);for(int id=7;id<board::count;++id)legacy.removeChildElement(legacy.getChildByAttribute("id",String(id)),true);legacy.removeChildElement(legacy.getChildByName("Input"),true);for(auto* pedal:legacy.getChildIterator())if(pedal->hasTagName("Pedal")){pedal->removeAttribute("ratio");pedal->removeAttribute("tone");if(board::isNam(pedal->getIntAttribute("id"))){pedal->setAttribute("mix",.3);pedal->setAttribute("input",18);}}
    board::State migrated;check(board::decode(legacy,plugin,processor,migrated).wasOk()&&migrated.input.gainDb==0&&!migrated.input.gate&&migrated.pedals[0].ratio==board::compressors[(size_t)migrated.pedals[0].preset].ratio,"Legacy rigs migrate with unity input, gate off and original compressor ratio");
    check(migrated.pedals[1].tone==0&&migrated.pedals[1].mix==1&&migrated.pedals[1].input==12,"Legacy NAM migrates to neutral Tone, serial wet path and bounded input");
    legacy.setAttribute("version",4);legacy.addChildElement(new XmlElement(*board::encode(state,plugin,processor).getChildByName("Input")));
    check(board::decode(legacy,plugin,processor,migrated).wasOk()&&migrated.pedals[4].tone==0,"Version 4 rigs migrate to neutral Tone");
    auto legacy5=board::encode(state,plugin,processor);legacy5.setAttribute("version",5);for(int id=7;id<board::count;++id)legacy5.removeChildElement(legacy5.getChildByAttribute("id",String(id)),true);
    check(board::decode(legacy5,plugin,processor,migrated).wasOk()&&migrated.pedals[1].tone==state.pedals[1].tone&&!migrated.pedals[7].enabled&&!migrated.pedals[9].enabled&&!migrated.pedals[12].enabled,"Version 5 preserves Tone and adds six bypassed defaults");
    auto missing=board::encode(state,plugin,processor);missing.removeChildElement(missing.getChildByAttribute("id","12"),true);check(board::decode(missing,plugin,processor,restored).failed(),"Version 6 rejects missing new pedal");
    auto invalidEq=board::encode(state,plugin,processor);invalidEq.getChildByAttribute("id","12")->setAttribute("eq4",13);check(board::decode(invalidEq,plugin,processor,restored).failed(),"Out of range EQ rejected");
    auto invalidWah=board::encode(state,plugin,processor);invalidWah.getChildByAttribute("id","9")->setAttribute("wahDecay",0);check(board::decode(invalidWah,plugin,processor,restored).failed(),"Invalid Wah decay rejected");
    auto badTone=board::encode(state,plugin,processor);badTone.getChildByAttribute("id","1")->setAttribute("tone",7);check(board::decode(badTone,plugin,processor,restored).failed(),"Out of range Tone rejected");
    badTone.getChildByAttribute("id","1")->removeAttribute("tone");check(board::decode(badTone,plugin,processor,restored).failed(),"Missing Tone in version 5 rejected");
    auto badInput=board::encode(state,plugin,processor);badInput.getChildByName("Input")->setAttribute("gain",100);check(board::decode(badInput,plugin,processor,restored).failed(),"Invalid input gain rejected on recall");
    auto disk=File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("nam-rig-test",".xml",false);
    bool written=rig::writeXml(xml,disk);auto read=XmlDocument::parse(disk);check(written&&read&&board::decode(*read,plugin,processor,restored).wasOk(),"Rig persists to disk and reloads");disk.deleteFile();
    auto* child=xml.getChildByName("Pedal");child->setAttribute("model","corrupt");
    check(board::decode(xml,plugin,processor,restored).failed(),"Reject changed model identity before restoring amp");
    auto bad=board::encode(state,plugin,processor);bad.getChildByName("Pedal")->setAttribute("preset",99);check(board::decode(bad,plugin,processor,restored).failed(),"Reject invalid preset index");
    auto badAmp=board::encode(state,plugin,processor);badAmp.getChildByName("Amp")->setAttribute("sha256","invalid");check(board::decode(badAmp,plugin,processor,restored).failed(),"Reject damaged TONEX state");
    board::Bindings bindings{};rig::Binding cc{2,1,42};rig::Binding pc{1,2,12};
    check(board::assign(bindings,129,cc)&&board::assign(bindings,3,pc)&&!board::assign(bindings,4,cc),"MIDI conflicts rejected across rigs and pedals");
    board::Bindings decoded{};check(board::decodeBindings(board::encodeBindings(bindings),decoded)&&board::match(decoded,cc)==129&&board::match(decoded,pc)==3,"MIDI mappings persist for pedals and rigs");
    bool newBindings=true;for(int id=7;id<board::count;++id)newBindings=newBindings&&board::assign(bindings,128+id,{2,3,60+id});board::Bindings allBindings{};newBindings=newBindings&&board::decodeBindings(board::encodeBindings(bindings),allBindings);for(int id=7;id<board::count;++id)newBindings=newBindings&&board::match(allBindings,{2,3,60+id})==128+id;check(newBindings,"Six new pedal MIDI targets persist without changing old IDs");
    rig::MidiEdges edges;int toggles=0;for(int value:{127,127,0,127,0})if(edges.accept({2,1,42,value}))++toggles;check(toggles==2,"CC press/release toggles exactly once per press");
    // Exercise stacking, including every pedal at once, to detect numerical instability.
    for(int p=0;p<board::count;++p){auto s=board::defaults(p,3);s.enabled=true;processor.set(p,s);}
    bool stable=true;double t=Time::getMillisecondCounterHiRes();
    for(int b=0;b<400;++b){fill(b);processor.processBlock(buffer,midi);for(int j=0;j<128;++j)stable=stable&&std::isfinite(buffer.getSample(0,j));}
    auto fraction=(Time::getMillisecondCounterHiRes()-t)/(51200.0/48.0);
    check(stable&&!processor.fault.load(),"All thirteen pedals stacked remain finite");report+="All-pedals offline CPU fraction: "+String(fraction,3)+"\n";
    report+=String(checks)+" board checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
