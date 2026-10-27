    tonegrid::ui::StartupStatus startupStatus;
    std::future<PreparedPreset> startupPreparing;
    std::optional<PreparedPreset> startupPrepared;
    bool startupRetryAfterScan=false,startupBeforeLibraryScan=false,suppressStartupCabinetChoice=false;
    bool startupSlotTried=false,startupPreferenceTried=false,startupConnectPending=false,startupFallbackUsed=false;
    size_t startupCandidate=0;
    unsigned startupAmpLoads=0,startupTimerTicks=0;
    const double startupBegan=Time::getMillisecondCounterHiRes();
    double startupStageBegan=startupBegan;
    String startupStage="Loading pedals";
    void startupStageChanged(const String& value)
    {
        const auto now=Time::getMillisecondCounterHiRes();
        report("Startup: "+startupStage+" took "+String((now-startupStageBegan)*.001,2)+" s.");
        startupStage=value;startupStageBegan=now;startupStatus.stage(value);
        active.setText(value+"...",dontSendNotification);
    }
    void finishStartupFeedback()
    {
        startupPresetPending=false;startupStatus.setVisible(false);
        startupBeforeLibraryScan=namService.busy();
        const String message=player.getCurrentProcessor()?"Ready to play":ready()?"Sound prepared - choose your audio interface":"Sound unavailable - open Audio Settings";
        startupStageChanged(message);active.setText(message,dontSendNotification);
        if(player.getCurrentProcessor())report("Ready to play. Your preset and audio interface are ready.");
        report("Startup completed in "+String((Time::getMillisecondCounterHiRes()-startupBegan)*.001,2)+" s; "+String(startupAmpLoads)+" startup NAM preparation(s).");
        refresh();
    }
    void startStartupNam(PreparedPreset prepared)
    {
        ++startupAmpLoads;startupPreparing=prepareNamPresetAsync(std::move(prepared));
    }
    void pollStarterPresets()
    {
        if(!startupPresetPending)return;
        ++startupTimerTicks;startupStatus.advance(Time::getMillisecondCounterHiRes()*.001);
        if(!pedals||loading.valid())return;
        if(startupRetryAfterScan)
        {
            if(namService.busy())return;
            startupRetryAfterScan=false;startupSlotTried=startupPreferenceTried=false;
        }
        if(startupConnectPending)
        {
            // Native plugin/driver lifecycles stay on the message thread. Give
            // the connecting stage a paint cycle before opening the driver.
            if(pluginValidation.valid())return;
            startupConnectPending=false;initialiseAudio();attach();finishStartupFeedback();return;
        }
        if(startupPrepared)
        {
            // A head with no saved IR still needs the existing default-cabinet
            // catalogue. Fully saved sounds do not wait for unrelated entries.
            if(!startupPrepared->cabinet&&!cabinetBypassed&&tonegrid::cabinet::needsIR(captureKind(startupPrepared->state.namAmp.model.file))&&(namService.busy()||cabinetRefreshing.valid()))return;
            auto prepared=std::move(*startupPrepared);startupPrepared.reset();
            const ScopedValueSetter<bool> restoring(suppressStartupCabinetChoice,prepared.cabinet.has_value());
            loadRig(std::move(prepared.amp),false,false);
            if(!ready()){finishStartupFeedback();return;}
            if(prepared.slot>=0)
            {
                if(!applyRigState(std::move(prepared.state),prepared.plugin,prepared.slot,{},std::move(prepared.cabinet),std::move(prepared.custom),true))
                    report("Saved preset could not be restored completely. Check the reported missing files.");
            }
            else if(startupFallbackUsed&&!blockAutomaticVst)
            {
                // Preserve saved VST3 validation and fallback semantics.
                if(rigStore.contains(selected()))recallSlot(selected());
                else
                {
                    const auto preference=XmlDocument::parse(sourcePreference);
                    if(startupPlugin&&(!preference||preference->getStringAttribute("type")=="VST3"))
                    {
                        PluginDescription resolved;const auto available=pluginCatalog.resolve(*startupPlugin,resolved);
                        if(available.wasOk()){pluginCatalog.add(resolved);populatePlugins();displayedSource=amp::SourceType::Vst3;syncAmpUi(true);requestPluginValidation(resolved);}
                        else report(available.getErrorMessage()+" Open VST to select another plugin.");
                    }
                }
            }
            selectedCabinetPreview=std::move(prepared.cabinetPreview);syncCabinetMode();
            startupStageChanged("Connecting your audio interface");startupConnectPending=true;return;
        }
        if(startupPreparing.valid())
        {
            if(startupPreparing.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
            auto prepared=startupPreparing.get();
            if(prepared.result.wasOk())
            {startupStageChanged("Restoring your preset");startupPrepared.emplace(std::move(prepared));return;}
            report(prepared.result.getErrorMessage()+" Trying another available startup sound.");
            if(namService.busy())
            {startupRetryAfterScan=true;startupStageChanged("Preparing your library");return;}
        }
        if(starterBootstrapPending)
        {
            // An installed starter receipt allows the saved preset to start
            // while the full amp browser refreshes independently. First installs
            // still validate all seven starter references against the catalogue.
            const auto manifest=appPaths.starterPresets().getChildFile("manifest.json");
            const bool seededAlready=manifest.existsAsFile()&&manifest.getSize()<=64*1024&&rigStore.directory().getChildFile("starter-presets-sha256.txt").loadFileAsString().trim()==SHA256(manifest).toHexString();
            if(namService.busy()&&!seededAlready)
            {if(startupStage!="Preparing your library")startupStageChanged("Preparing your library");return;}
            starterBootstrapPending=false;startupStageChanged("Loading your preset");
            const auto seeded=tonegrid::starters::seed(appPaths.starterPresets(),rigStore,namCatalog,rigIdentities());
            if(seeded.result.failed()){startupProblem=seeded.result.getErrorMessage()+"\nCheck that the complete ZIP was extracted and your user folder is writable. A local NAM will be used if available.";report(startupProblem);}
            if(seeded.installed)reloadSlots(seeded.startupSlot>=0?seeded.startupSlot:selected());
        }
        if(!startupSlotTried)
        {
            startupSlotTried=true;
            if(rigStore.contains(selected()))
            {
                PreparedPreset prepared;prepared.slot=selected();
                prepared.result=rigStore.loadForSelection(selected(),rigIdentities(),prepared.state,prepared.plugin);
                if(prepared.result.wasOk()&&prepared.state.sourceType==amp::SourceType::Nam)
                {
                    startupNam=prepared.state.namAmp;startupPlugin.reset();displayedSource=amp::SourceType::Nam;
                    startupPreferenceTried=true;startStartupNam(std::move(prepared));return;
                }
                if(prepared.result.failed())report(prepared.result.getErrorMessage());
            }
        }
        if(!startupPreferenceTried)
        {
            startupPreferenceTried=true;
            if(startupNam){PreparedPreset prepared;prepared.state.namAmp=*startupNam;startStartupNam(std::move(prepared));return;}
        }
        if(namService.busy())
        {if(startupStage!="Preparing your library")startupStageChanged("Preparing your library");return;}
        if(startupCandidate<namCatalog.entries.size())
        {
            startupFallbackUsed=true;PreparedPreset prepared;
            prepared.state.namAmp={namCatalog.entries[startupCandidate++].identity,0,0};
            startStartupNam(std::move(prepared));return;
        }
        startupProblem="No usable amp could be loaded. Extract the complete ToneGrid ZIP, including Models and FactoryAmps. Check that your user folder is writable, or import a local NAM capture.";
        report(startupProblem);finishStartupFeedback();
    }
