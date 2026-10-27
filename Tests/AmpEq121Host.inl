// Included inside RigHost. Isolated data root, synthetic NAM and synthetic VST3.
    bool verifyAmpEq121(String& result,const File& folder,bool restarted)
    {
        bool ok=true;auto check=[&](bool pass,const String& text){ok&=pass;result+=String(pass?"PASS ":"FAIL ")+"EQ121 host "+text+"\n";};
        check(ready()&&ampSource->type()==amp::SourceType::Nam&&ampEqGear.isVisible(),"NAM starts with visible EQ");if(!ready())return false;
        const tonegrid::ampEq::Parameters wanted{6,-3,2};
        auto matches=[&]{const auto p=ampEqControls.state();return p.bassDb==6&&p.midDb==-3&&p.trebleDb==2&&outputSettings.ampEq.bassDb==6;};
        if(restarted){check(matches(),"unsaved current EQ survives full host restart");return ok;}
        auto* bass=dynamic_cast<Slider*>(ampEqControls.getChildComponent(0));auto* mid=dynamic_cast<Slider*>(ampEqControls.getChildComponent(1));auto* treble=dynamic_cast<Slider*>(ampEqControls.getChildComponent(2));
        check(bass&&mid&&treble,"three actual rotary sliders available");if(!bass||!mid||!treble)return false;
        bass->setValue(6,sendNotificationSync);mid->setValue(-3,sendNotificationSync);treble->setValue(2,sendNotificationSync);check(matches(),"actual slider changes reach DSP state");
        name.setText("EQ121 synthetic preset",dontSendNotification);saveSlot();check(rigStore.contains(selected()),"User Preset saved");
        bass->setValue(-8,sendNotificationSync);recallSlot(selected());check(matches(),"preset recall restores all three EQ controls");
        namChoice.setSelectedId(namChoice.getSelectedId()==1?2:1,sendNotificationSync);check(matches(),"manual capture change keeps EQ");
        spreadControls.onChange();check(matches(),"3D Sound changes preserve amp EQ");
        for(int width:{1360,1660,2240})
        {
            setSize(width,940);check(!ampEqGear.getBounds().intersects(namGain.getBounds())&&!ampEqGear.getBounds().intersects(namVolume.getBounds()),"NAM EQ layout without overlap at "+String(width));
            takeAmpScreenshot(folder.getChildFile("eq121-amp-"+String(width)+".png"));
        }
        setSize(1660,940);
        for(float scale:{1.25f,1.5f}){auto image=createComponentSnapshot(sections[0],true,scale);auto stream=folder.getChildFile("eq121-amp-scale-"+String(scale,2)+".png").createOutputStream();check(stream&&PNGImageFormat().writeImageToStream(image,*stream),"scaled rack snapshot "+String(scale));}
        for(int p=0;p<board::count;++p){auto setting=pedals->get(p);setting.enabled=false;pedals->set(p,setting);}pedals->front.set({0,-70,false});
        graph.prepareToPlay(48000,128);AudioBuffer<float> namAudio(2,128);MidiBuffer namEvents;
        const auto renderNam=[&]{for(int block=0;block<100;++block){for(int c=0;c<2;++c)for(int i=0;i<128;++i)namAudio.setSample(c,i,.02f);graph.processBlock(namAudio,namEvents);}return namAudio.getSample(0,127);};
        const auto* native=dynamic_cast<amp::NamAmp*>(ampSource.get());const auto before=native->state();const auto calibrationBefore=calibrationSettings.referenceDbU;
        outputSettings.ampEq={};configureOutputDsp();const float neutral=renderNam();outputSettings.ampEq=wanted;configureOutputDsp();const float boosted=renderNam();
        check(neutral>.001f&&std::abs(boosted/neutral-Decibels::decibelsToGain(6.f))<.001f,"full NAM graph applies post-model +6 dB bass without compensation");
        check(native->state().gainDb==before.gainDb&&native->state().volumeDb==before.volumeDb&&(calibrationSettings.referenceDbU==calibrationBefore||(std::isnan(calibrationSettings.referenceDbU)&&std::isnan(calibrationBefore))),"EQ leaves input calibration, GAIN and VOLUME unchanged");
        const auto nativeState=native->state();
        static ampTests::Trace trace;auto plugin=std::make_unique<ampTests::Plugin>(trace);plugin->gain=1;PluginDescription description;plugin->fillInPluginDescription(description);
        commitSource(std::make_unique<amp::Vst3AmpHost>(std::make_unique<AudioPluginFormatManager>(),description,std::move(plugin),"Synthetic EQ121"));
        check(ampSource->type()==amp::SourceType::Vst3&&!ampEqGear.isVisible()&&matches(),"real host source replacement hides EQ and retains stored values in VST3");
        for(int p=0;p<board::count;++p){auto setting=pedals->get(p);setting.enabled=false;pedals->set(p,setting);}pedals->front.set({0,-70,false});
        graph.prepareToPlay(48000,128);AudioBuffer<float> b(2,128);MidiBuffer events;bool unity=true;
        for(int block=0;block<40;++block){for(int c=0;c<2;++c)for(int i=0;i<128;++i)b.setSample(c,i,.02f);graph.processBlock(b,events);if(block>20)unity&=std::abs(b.getSample(0,127)-.02f)<1.e-6;}
        check(unity,"full host graph applies no NAM EQ to synthetic VST3 output");takeAmpScreenshot(folder.getChildFile("eq121-vst-hidden.png"));
        auto loaded=amp::loadNamAmp(namLibrary,nativeState,namLite);check(loaded.result.wasOk(),"return NAM loads");if(loaded.source)commitSource(std::move(loaded.source));
        check(ampEqGear.isVisible()&&matches(),"return to NAM restores EQ controls and values");
        return ok;
    }
