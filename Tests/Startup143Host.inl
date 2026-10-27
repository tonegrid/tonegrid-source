    bool startupProbeExisting=false,startupProbeConfigured=false,startupProbeSawWorker=false,startupProbeSawRestore=false,startupProbeSawConnect=false;
    void configureStartup143Probe(bool existing=false)
    {
        startupProbeExisting=existing;
        devices.closeAudioDevice();
        while(!devices.getAvailableDeviceTypes().isEmpty())devices.removeAudioDeviceType(devices.getAvailableDeviceTypes()[0]);
        auto state=std::make_shared<audioRecovery126Tests::State>();state->busy=false;
        devices.addAudioDeviceType(std::make_unique<audioRecovery126Tests::Type>(state));
        startupProbeConfigured=true;
    }
    int stepStartup143(const File& evidence)
    {
        const auto folder=evidence.getChildFile(startupProbeExisting?"existing":"fresh");folder.createDirectory();
        const auto snapshot=[&](Component& component,const String& name)
        {MemoryOutputStream stream;return PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()),stream)&&folder.getChildFile(name).replaceWithData(stream.getData(),stream.getDataSize());};
        if(startupPresetPending)
        {
            if(startupPreparing.valid())startupProbeSawWorker=true;
            if(startupPrepared)startupProbeSawRestore=true;
            if(startupConnectPending)
            {
                startupProbeSawConnect=true;
                // Open only our synthetic device, never the user's interface.
                if(!audioInitialisationAttempted)initialiseAudioDevices();
            }
            if(!folder.getChildFile("startup.png").exists())snapshot(*this,"startup.png");
            return 0;
        }
        bool ok=true;String result;
        const auto check=[&](bool yes,const String& label){ok&=yes;result+=String(yes?"PASS ":"FAIL ")+label+"\n";};
        check(startupProbeConfigured&&ready()&&audioReady&&player.getCurrentProcessor()==&deviceProcessor,"Startup ends with the saved sound and synthetic audio attached");
        if(startupProbeExisting)check(startupBeforeLibraryScan,"An existing preset becomes playable before the full amp library scan completes");
        check(startupAmpLoads==1,"Selected NAM amp is prepared exactly once during startup");
        check(startupTimerTicks>3&&startupProbeSawWorker,"UI timer continues while the saved sound loads on a worker");
        check(startupProbeSawRestore&&startupProbeSawConnect,"Restore and connection are distinct UI stages");
        check(!startupStatus.isVisible()&&active.getText()=="Ready to play","Loading cover closes only when audio is ready");
        board::State expected;PluginDescription plugin;const auto loaded=rigStore.loadForSelection(selected(),rigIdentities(),expected,plugin);
        const auto* native=dynamic_cast<amp::NamAmp*>(ampSource.get());
        check(loaded.wasOk()&&native&&native->state().model.sha256==expected.namAmp.model.sha256&&native->state().gainDb==expected.namAmp.gainDb&&native->state().volumeDb==expected.namAmp.volumeDb,"Prepared amp matches saved capture, gain and volume");
        bool same=loaded.wasOk();for(int p=0;p<board::count;++p){const auto a=pedals->get(p),b=expected.pedals[size_t(p)];same&=a.preset==b.preset&&a.enabled==b.enabled&&a.input==b.input&&a.output==b.output;}
        if(startupProbeExisting)
        {
            check(selectedCabinetPreview&&selectedCabinetPreview->image.isValid()&&selectedCabinetPreview->name.contains("Greenback"),"Saved cabinet name and studio artwork are ready before the full library scan");
            check(expected.cabinet&&cabinetInstalled&&installedCabinetHash==expected.cabinet->sha256&&cabinetPanel.power.isEnabled()&&cabinetPanel.power.getToggleState()&&!cabinetLoading.valid(),"Head Only starts with the exact saved IR enabled and no competing cabinet load");
        }
        check(same&&tonegrid::output::encode(outputSettings).toString()==tonegrid::output::encode(expected.outputDsp.value_or(tonegrid::output::legacySettings())).toString(),"Saved pedals and post effects are restored without a second amp load");
        const auto text=log.getText();check(text.indexOf("Ready to play.")>text.indexOf("Audio device ready:")&&!text.contains("ToneGrid ready:"),"Ready message follows audio readiness and replaces the premature message");
        snapshot(*this,"ready.png");audioSettings();
        std::function<Component*(Component*,const String&)> find=[&](Component* c,const String& id)->Component*{if(c->getComponentID()==id)return c;for(auto* child:c->getChildren())if(auto* hit=find(child,id))return hit;return nullptr;};
        auto* panel=dynamic_cast<tonegrid::AudioSettingsPanel*>(find(audioDialog.getComponent(),"audio-settings-panel"));
        check(panel&&!find(panel,"audio-send-diagnostic")&&!find(panel,"audio-editable-content"),"Audio Settings contains neither removed button");
        if(panel)
        {
            auto* calibration=find(panel,"guitar-calibration-settings");auto* viewport=panel->findParentComponentOfClass<Viewport>();
            check(calibration&&panel->getLocalBounds().contains(calibration->getBounds()),"Calibration and help fit the measured settings body");
            const bool fullHeight=viewport&&viewport->getMaximumVisibleHeight()>=panel->getHeight();
            const auto screen=audioDialog->getParentMonitorArea();
            check(fullHeight||audioDialog->getHeight()>=screen.getHeight()*.8,"Audio Settings uses its natural height or the available screen height");
            snapshot(*audioDialog->getContentComponent(),"audio-settings.png");
            audioDialog->setSize(520,400);
            if(viewport)viewport->setViewPosition(0,panel->getHeight());
            check(viewport&&calibration&&viewport->getViewArea().intersects(calibration->getBounds()),"On a small window, scrolling reaches calibration instead of clipping it");
            snapshot(*audioDialog->getContentComponent(),"audio-settings-small.png");
        }
        delete audioDialog.getComponent();
        auto* device=dynamic_cast<audioRecovery126Tests::Device*>(devices.getCurrentAudioDevice());bool rendered=false;
        if(device)for(int i=0;i<8;++i)rendered=device->render()||rendered;
        check(rendered,"The initialized NAM, pedals and cabinet graph renders finite audio");
        result+="Startup stages and timings:\n"+text;
        folder.getChildFile("startup143-tests.txt").replaceWithText(result);
        return ok?1:-1;
    }
