// Included inside RigHost; this workflow uses only the isolated test profile.
    bool verifyCalibration118Ui(String& reportText,const File& folder)
    {
        using namespace tonegrid::calibration;
        bool passed=true;int checks=0;
        auto check=[&](bool value,const String& name){++checks;passed=passed&&value;reportText+=(value?"PASS ":"FAIL ")+String("calibration UI ")+name+"\n";};
        const auto* nativeSource=dynamic_cast<amp::NamAmp*>(ampSource.get());
        check(ready()&&nativeSource,"isolated calibration workflow has its expected NAM source");if(!ready()||!nativeSource)return false;
        const auto original=calibrationSettings;const auto ampState=nativeSource->state();
        check(calibrationInterfaceStatus()==InterfaceStatus::active,"saved confirmed device has independent green interface status");
        calibrationSettings.needsVerification=true;updateCalibrationStatus();check(calibrationInterfaceStatus()==InterfaceStatus::verification,"changed device/input needs verification");calibrationSettings=original;updateCalibrationStatus();
        for(const int width:{1360,1660,1920,2400})
        {
            setSize(width,940);bool inside=true;
            for(auto* c:std::initializer_list<Component*>{&guitarCalibration,&audio,&tunerButton,&midiDevice,&rescan})inside=inside&&getLocalBounds().contains(c->getBounds())&&c->getHeight()==32;
            check(inside&&!guitarCalibration.getBounds().intersects(audio.getBounds())&&!audio.getBounds().intersects(midiDevice.getBounds())&&rescan.getWidth()>=32,"single aligned header row at "+String(width));
            takeScreenshot(folder.getChildFile("header-"+String(width)+".png"));
            if(width==1360){MemoryOutputStream stream;PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds(),true,1.5f),stream);folder.getChildFile("header-1360-scale150.png").replaceWithData(stream.getData(),stream.getDataSize());}
        }
        check(midiDevice.getTooltip()==midiDevice.getText(),"MIDI full selected name is available in tooltip");
        const String longMidiName="Very long original MIDI device name that must remain available in full in the tooltip";
        midiDevice.addItem(longMidiName,2);midiDevice.setSelectedId(2,sendNotificationSync);setSize(1360,940);
        check(midiDevice.getTooltip()==longMidiName,"long MIDI selection retains full name tooltip");takeScreenshot(folder.getChildFile("header-long-midi.png"));
        rescan.onClick();check(midiDevice.getSelectedId()==1,"Refresh MIDI preserves synthetic selection");
        const auto rawGraph=[&](const String& label)
        {
            RawInputTestDevice device;AudioProcessorPlayer rawPlayer;tonegrid::tuning::Engine rawTuner;tonegrid::tuning::AudioTap tap(rawPlayer,rawTuner);
            rawPlayer.setProcessor(&graph);tap.audioDeviceAboutToStart(&device);
            AudioBuffer<float> input(1,256),output(2,256);for(int i=0;i<256;++i)input.setSample(0,i,.03f*std::sin(float(i)*.1f));input.setSample(0,128,.03125f);
            tap.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,output.getArrayOfWritePointers(),2,256,{});
            check(tap.inputConnected()&&tap.takeRawInputPeak()==.03125f&&input.getSample(0,128)==.03125f,label);
            rawPlayer.setProcessor(nullptr);tap.audioDeviceStopped();
        };
        std::array<board::Setting,board::count> pedalBefore;for(int p=0;p<board::count;++p)pedalBefore[size_t(p)]=pedals->get(p);
        const auto inputBefore=pedals->front.get();
        for(int p=0;p<board::count;++p){auto s=pedalBefore[size_t(p)];s.enabled=false;pedals->set(p,s);}rawGraph("RAW INPUT from actual NAM graph with all pedals OFF");
        for(int p=0;p<board::count;++p){auto s=pedalBefore[size_t(p)];s.enabled=true;pedals->set(p,s);}rawGraph("RAW INPUT from actual NAM graph with all available pedals ON");
        pedals->front.set({12,-20,true});rawGraph("RAW INPUT unchanged by +12 dB input trim and active gate");
        pedals->front.set({-12,-80,false});rawGraph("RAW INPUT unchanged by -12 dB input trim and bypassed gate");
        pedals->front.set(inputBefore);for(int p=0;p<board::count;++p)pedals->set(p,pedalBefore[size_t(p)]);
        check(calibrationChainStatus()==ChainStatus::calibrated,"confirmed interface and input-only final NAM are sufficient with NAM pedals bypassed");
        calibrationSettings.enabled=false;updateCalibrationStatus();check(calibrationChainStatus()==ChainStatus::disabled,"OFF reports CALIBRATION DISABLED");
        check(calibrationInterfaceStatus()==InterfaceStatus::disabled,"OFF interface indicator neutral gray");
        calibrationSettings={};updateCalibrationStatus();check(calibrationChainStatus()==ChainStatus::setup,"unconfirmed reference reports SETUP REQUIRED");
        calibrationSettings=original;
        const int uncalibrated=board::id(board::PedalId::BluesOd);const auto previous=pedals->get(uncalibrated);
        // Exercise missing calibration using a synthetic personal capture, independently
        // of the approved factory BD-2 pack's complete 10.5 dBu metadata.
        const auto partialFile=appPaths.user().getChildFile("calibration-partial-fixture.nam");
        check(partialFile.replaceWithText(namTests::linear()),"synthetic uncalibrated personal pedal fixture written");
        auto partial=tonegrid::userpedals::prepare(partialFile,namLite);
        check(partial.result.wasOk()&&partial.model.dsp,"synthetic uncalibrated personal pedal fixture prepared");
        if(!partial.model.dsp)return false;
        pedals->installCustom(uncalibrated,std::move(partial.model));
        auto enabled=previous;enabled.preset=board::presetCount(uncalibrated);enabled.enabled=true;pedals->set(uncalibrated,enabled);
        check(calibrationChainStatus()==ChainStatus::partial,"active incomplete pedal reports PARTIAL CALIBRATION");
        updateCalibrationStatus();check(calibrationInterfaceStatus()==InterfaceStatus::active&&int(guitarCalibration.getProperties()["interface-status"])==int(InterfaceStatus::active),"uncalibrated pedal never changes interface green check");
        pedals->set(uncalibrated,previous);check(calibrationChainStatus()==ChainStatus::calibrated,"bypassed incomplete pedal does not downgrade chain status");
        static ampTests::Trace trace;auto plugin=std::make_unique<ampTests::Plugin>(trace);const auto description=plugin->getPluginDescription();
        commitSource(std::make_unique<amp::Vst3AmpHost>(nullptr,description,std::move(plugin),"Synthetic VST3"));
        check(calibrationChainStatus()==ChainStatus::partial,"VST3 convention remains opaque and never claims a fully calibrated chain");
        check(calibrationInterfaceStatus()==InterfaceStatus::active,"VST3 does not downgrade configured interface");rawGraph("RAW INPUT remains correct with actual hosted VST3 graph");
        auto native=amp::loadNamAmp(namLibrary,ampState,namLite);check(native.result.wasOk(),"NAM can reload after VST3 with calibration retained");
        if(native.source)commitSource(std::move(native.source));
        check(calibrationChainStatus()==ChainStatus::calibrated,"switching back to input-calibrated NAM restores chain status");
        board::State beforeWizard;beforeWizard.input=pedals->front.get();for(int p=0;p<board::count;++p)beforeWizard.pedals[size_t(p)]=pedals->get(p);
        const auto snapshot=board::encode(beforeWizard,{},rigIdentities()).toString();
        showGuitarCalibration();check(calibrationDialog&&calibrationDialog->isVisible(),"toolbar opens the actual wizard window");
        if(calibrationDialog)
        {
            auto* frame=calibrationDialog->getContentComponent();Viewport* viewport=nullptr;
            for(auto* child:frame->getChildren())if(auto* view=dynamic_cast<Viewport*>(child))viewport=view;
            auto* wizard=viewport?dynamic_cast<GuitarWizard*>(viewport->getViewedComponent()):nullptr;
            check(wizard!=nullptr,"actual wizard uses the existing scrollable popup frame");
            if(wizard)
            {
                const int originalHeight=calibrationDialog->getHeight();wizard->advance();
                check(wizard->currentStep()==1&&wizard->getHeight()<580&&calibrationDialog->getHeight()<=originalHeight+74,"CONFIGURE is shorter and fits monitor without growing the window");
                auto* footer=frame->findChildWithID("popup-footer");auto* next=footer?footer->findChildWithID("calibration-next"):nullptr;auto* help=wizard->findChildWithID("calibration-dbu-help");
                calibrationDialog->setSize(600,420);wizard->refresh();
                check(next&&next->isVisible()&&frame->getLocalBounds().contains(frame->getLocalArea(next,next->getLocalBounds()))&&frame->getLocalArea(next,next->getLocalBounds()).getY()>=viewport->getBottom(),"NEXT remains visible below scroll area even without an available device");
                help->grabKeyboardFocus();wizard->refresh();check(help->hasKeyboardFocus(true)&&viewport->getViewArea().contains(help->getBounds()),"keyboard focus scrolls back to dBu help");
                MemoryOutputStream image;PNGImageFormat().writeImageToStream(frame->createComponentSnapshot(frame->getLocalBounds()),image);folder.getChildFile("calibration-configure-native-laptop.png").replaceWithData(image.getData(),image.getDataSize());
                if(auto* button=footer?dynamic_cast<TextButton*>(footer->findChildWithID("calibration-back")):nullptr)button->onClick();
                check(wizard->currentStep()==0&&wizard->getHeight()==440&&calibrationDialog->getHeight()==420,"BACK restores Step 1 while preserving manually resized window");
            }
            const bool escaped=static_cast<Component*>(calibrationDialog.getComponent())->keyPressed(KeyPress(KeyPress::escapeKey));
            check(escaped&&!calibrationDialog->isVisible(),"Esc dismisses the wizard without preventing continued use");
            delete calibrationDialog.getComponent();
        }
        check(calibrationSettings.profileId==original.profileId&&calibrationSettings.referenceDbU==original.referenceDbU,"dismissal does not overwrite confirmed settings");
        board::State afterWizard;afterWizard.input=pedals->front.get();for(int p=0;p<board::count;++p)afterWizard.pedals[size_t(p)]=pedals->get(p);
        check(board::encode(afterWizard,{},rigIdentities()).toString()==snapshot&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model==ampState.model,"wizard leaves pedal states, input settings, and amp selection unchanged");
        pendingCalibrationWarning.reset();delete calibrationWarningDialog.getComponent();
        for(const auto& entry:namCatalog.entries)if(entry.identity==ampState.model)
        {
            queueCaptureWarning(entry);pollCalibration();
            check(calibrationWarningDialog&&calibrationWarningDialog->isVisible(),"first external incomplete capture opens the warning");
            if(calibrationWarningDialog)
            {
                auto* frame=calibrationWarningDialog->getContentComponent();auto* mark=frame->findChildWithID("popup-brand-mark");
                check(mark&&!mark->isVisible(),"actual warning window hides branding");
                MemoryOutputStream image;PNGImageFormat().writeImageToStream(frame->createComponentSnapshot(frame->getLocalBounds()),image);
                folder.getChildFile("calibration-capture-warning.png").replaceWithData(image.getData(),image.getDataSize());
                calibrationWarningDialog->closeButtonPressed();delete calibrationWarningDialog.getComponent();
            }
            queueCaptureWarning(entry);check(!pendingCalibrationWarning,"acknowledged content identity is not warned again");break;
        }
        calibrationSettings=original;pedals->setCalibration(original);configureOutputDsp();updateCalibrationStatus();
        reportText+=String(checks)+" calibration UI checks; "+(passed?"ALL PASSED":"FAILURES")+"\n";return passed;
    }
