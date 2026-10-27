// Legacy preset/graph guarantees with the new always-visible EQ.
    bool verifyNamEq123(String& result,const File& folder,bool restarted)
    {
        using namespace tonegrid;bool ok=true;auto check=[&](bool pass,const String& name){ok&=pass;result+=String(pass?"PASS ":"FAIL ")+"NAM EQ host "+name+"\n";};
        check(ready()&&ampSource->type()==amp::SourceType::Nam,"isolated NAM host ready");if(!ready())return false;
        auto matches=[&]{const auto p=ampEqControls.state();return p.depthDb==3.5f&&p.depthHz==140&&p.presenceDb==4&&p.presenceHz==2700&&p.presenceQ==1.7f&&outputSettings.ampEq.depthDb==3.5f;};
        if(restarted){check(matches(),"Depth and Presence survive full session restart");return ok;}
        const auto nativeBefore=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();setSize(1697,900);ampEqGear.onClick();check(ampEqDialog!=nullptr,"new popup opens");if(!ampEqDialog)return false;
        const int height=ampEqDialog->getHeight();for(int band=0;band<8;++band){ampEqControls.selectBand(band);check(ampEqDialog->getHeight()==height,"band selection never resizes the window");for(int i:{0,1,2,3,4,9,11})check(ampEqControls.findChildWithID("amp-eq-"+String(i))->isVisible(),"tone knobs and filters always visible");}
        const auto set=[&](int i,double value){auto* k=dynamic_cast<Slider*>(ampEqControls.findChildWithID("amp-eq-"+String(i)));if(k)k->setValue(value,sendNotificationSync);else check(false,"slider exists");};
        set(9,4);set(8,2700);set(10,1.7);set(11,3.5);set(12,140);check(matches(),"real slider changes reach published EQ parameters");
        name.setText("NAM EQ preset",dontSendNotification);saveSlot();check(rigStore.contains(selected()),"save preset with Depth");set(11,-6);set(9,-5);recallSlot(selected());check(matches(),"preset recall restores Depth and Presence");
        delete ampEqDialog.getComponent();ampEqGear.onClick();check(ampEqDialog&&matches(),"reopening does not change sound");if(ampEqDialog)delete ampEqDialog.getComponent();
        // Full graph position: shelf raises DC by +6 dB after the synthetic NAM;
        // the same stored parameters are dormant after a VST3 source switch.
        for(int p=0;p<board::count;++p){auto setting=pedals->get(p);setting.enabled=false;pedals->set(p,setting);}pedals->front.set({0,-70,false});
        const auto saved=outputSettings;outputSettings={};configureOutputDsp();graph.prepareToPlay(48000,128);
        AudioBuffer<float> audio(2,128);MidiBuffer midi;
        const auto render=[&]{for(int n=0;n<100;++n){for(int c=0;c<2;++c)for(int i=0;i<128;++i)audio.setSample(c,i,.02f);graph.processBlock(audio,midi);}return audio.getSample(0,127);};
        const float neutral=render();outputSettings.ampEq.depthDb=6;configureOutputDsp();const float weighted=render();check(neutral>.001f&&std::abs(weighted/neutral-Decibels::decibelsToGain(6.f))<.002f,"full NAM graph applies Depth without gain compensation");
        static ampTests::Trace trace;auto plugin=std::make_unique<ampTests::Plugin>(trace);plugin->gain=1;PluginDescription description;plugin->fillInPluginDescription(description);
        commitSource(std::make_unique<amp::Vst3AmpHost>(std::make_unique<AudioPluginFormatManager>(),description,std::move(plugin),"Synthetic Depth"));
        graph.prepareToPlay(48000,128);check(!ampEqGear.isVisible()&&std::abs(render()-.02f)<1.e-6f,"full VST3 graph bypasses Depth and hides NAM EQ");
        auto loaded=amp::loadNamAmp(namLibrary,nativeBefore,namLite);check(loaded.result.wasOk(),"restore NAM");if(loaded.source)commitSource(std::move(loaded.source));outputSettings=saved;syncOutputUi();
        return ok;
    }
