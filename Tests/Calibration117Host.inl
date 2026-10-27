// Included inside RigHost. Only the isolated --calibration117-ui-test uses it.
    bool verifyCalibration117(String& result,const File& folder,bool restarted=false)
    {
        bool ok=true;int checks=0;
        const auto apply=[&](bool enabled){tonegrid::calibration::Settings settings{enabled,12};settings.deviceType="Test";settings.deviceName="Synthetic";settings.channel=0;return setCalibration(settings);};
        const auto check=[&](bool pass,const String& label){++checks;ok=pass&&ok;result+=(pass?"PASS ":"FAIL ")+String("calibration host ")+label+"\n";};
        check(ready()&&dynamic_cast<amp::NamAmp*>(ampSource.get()),"synthetic NAM starts without audio hardware or a VST3 dependency");
        if(!ready()||!dynamic_cast<amp::NamAmp*>(ampSource.get()))return false;
        if(restarted)
        {
            check(calibrationSettings.enabled&&calibrationSettings.referenceDbU==12,"global calibration remains active on restart");
            check(calibrationInterfaceStatus()==tonegrid::calibration::InterfaceStatus::active,"configured interface indicator stays active on restart");
            check(namLite,"quality preference and calibration are independent and both survive restart");
        }
        else
        {
            check(calibrationSettings.enabled&&!calibrationSettings.completed,"new profile defaults ON but does not guess an interface reference");
            check(apply(true),"host applies and persists calibration");
        }
        graph.prepareToPlay(48000,256);AudioBuffer<float> buffer(2,256);MidiBuffer events;
        const auto render=[&]{for(int block=0;block<16;++block){for(int c=0;c<2;++c)for(int i=0;i<256;++i)buffer.setSample(c,i,.25f);graph.processBlock(buffer,events);}return buffer.getSample(0,255);};
        const auto approximatelyEqual=[](float a,float b){return std::abs(a-b)<.0001f;};
        auto* native=dynamic_cast<amp::NamAmp*>(ampSource.get());const auto selectedState=native->state();
        const auto expected=selectedState.model.file=="First.nam"?.25f*Decibels::decibelsToGain(-6.f):.25f*Decibels::decibelsToGain(6.f);
        check(approximatelyEqual(render(),expected),"running graph uses the interface reference before the NAM amp");
        if(!restarted)
        {
            const auto other=namChoice.getSelectedId()==1?2:1;namChoice.setSelectedId(other,sendNotificationSync);
            native=dynamic_cast<amp::NamAmp*>(ampSource.get());check(native&&native->state().gainDb==0&&native->state().volumeDb==0,"capture replacement preserves manual Gain and Volume");
            graph.prepareToPlay(48000,256);
            const auto alternate=native->state().model.file=="First.nam"?.25f*Decibels::decibelsToGain(-6.f):.25f*Decibels::decibelsToGain(6.f);
            check(approximatelyEqual(render(),alternate),"new capture automatically inherits calibration using its own metadata");
            check(setNamLite(true),"NAM Lite still applies with calibration enabled");
            graph.prepareToPlay(48000,256);check(approximatelyEqual(render(),alternate),"quality change retains calibration");
            name.setText("Calibration independent preset",dontSendNotification);saveSlot();
            check(rigStore.contains(selected()),"musical preset can be saved with calibration active");
            check(apply(false),"host can disable calibration");recallSlot(selected());
            check(!calibrationSettings.enabled,"preset recall does not overwrite the device calibration setting");
            graph.prepareToPlay(48000,256);check(approximatelyEqual(render(),.25f),"calibration off restores raw amp level after recalling a preset");
            check(apply(true),"chosen calibration is restored before restart");
            setSize(1660,940);outputMeter.update(1.25f);takeScreenshot(folder.getChildFile("calibration-main.png"));
            setSize(1360,820);outputMeter.update(.6f);takeScreenshot(folder.getChildFile("calibration-main-small.png"));
            check(outputMeter.isVisible()&&outputMeter.getWidth()>=12&&getLocalBounds().contains(outputMeter.getBounds()),"final meter remains visible in the compact main window");
            audioSettings();check(audioDialog!=nullptr,"Audio Settings opens with the new calibration section");
            if(audioDialog)
            {
                auto* content=audioDialog->getContentComponent();const auto image=content->createComponentSnapshot(content->getLocalBounds());
                auto output=folder.getChildFile("calibration-audio-settings.png").createOutputStream();
                check(output&&PNGImageFormat().writeImageToStream(image,*output),"Audio Settings preview exported for visual review");
                delete audioDialog.getComponent();
            }
        }
        if(!restarted)ok=verifyCalibration118Ui(result,folder)&&ok;
        result+=String(checks)+" calibration host checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
