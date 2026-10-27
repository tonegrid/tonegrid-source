    struct Refinements142Probe {int phase=0;unsigned starts=0,published=0;bool ok=true;String report;};
    Refinements142Probe refinementProbe;
    int stepFactoryRefinements142(const File& evidence)
    {
        auto& probe=refinementProbe;
        const auto check=[&](bool yes,const String& label){probe.ok&=yes;probe.report+=String(yes?"PASS ":"FAIL ")+label+"\n";};
        if(!ready()||startupPresetPending||presetPreparing.valid()||!presetRequests.idle()||cabinetLoading.valid()||cabinetRefreshing.valid())return 0;
        if(probe.phase==0)
        {
            bool seven=selected()==0&&rigStore.readName(0).contains("John Mayer");for(int i=0;i<7;++i)seven&=rigStore.contains(i);seven&=!rigStore.contains(7);
            check(seven,"Fresh profile installs exactly seven requested User Presets and starts on John Mayer");
            const auto manifest=juce::JSON::parse(appPaths.starterPresets().getChildFile("manifest.json"));
            const auto entries=manifest.getProperty("presets",{});
            bool labels=true;const auto* shipped=entries.getArray();
            if(!shipped||shipped->size()!=7)labels=false;
            else for(int i=0;i<7;++i)labels&=rigStore.readName(i)==shipped->getReference(i).getProperty("name",{}).toString();
            check(labels,"All seven installed names exactly match the packaged manifest without a second slot-number prefix");
            check(!appPaths.user().getSiblingFile("ToneGridIR").exists()&&!appPaths.user().getChildFile("old-preset.txt").exists(),"Startup continues with a fresh canonical profile while the retired test profile is silently removed, without importing it");
            takeScreenshot(evidence.getChildFile("seven-starters-main.png"));
            const auto before=SHA256(rigStore.slotFile(0)).toHexString();auto repeated=tonegrid::starters::seed(appPaths.starterPresets(),rigStore,namCatalog,rigIdentities());
            check(repeated.result.wasOk()&&!repeated.installed&&SHA256(rigStore.slotFile(0)).toHexString()==before,"Repeated seeding does not overwrite existing User Presets");
            probe.starts=presetPreparationsStarted;probe.published=presetPreparationsPublished;
            for(int i=0;i<2000;++i)presetBrowser.onUser(i%7);presetBrowser.onUser(1);++probe.phase;return 0;
        }
        if(probe.phase==1)
        {
            check(selected()==1&&presetPreparationsStarted==probe.starts+1&&presetPreparationsPublished==probe.published+1,"Two thousand rapid User Preset choices prepare and publish only the final selected sound");
            const auto amp=ampName(),ir=cabinetState.sha256;cabinetPanel.power.setToggleState(false,dontSendNotification);cabinetPanel.power.onClick();
            check(cabinetBypassed&&cabinetState.bypassed&&ampName()==amp&&cabinetState.sha256==ir&&cabinetPanel.power.isEnabled(),"Head Only cabinet OFF preserves the amp, selected IR and enabled power control");
            for(int i=0;i<40;++i)cabinetPanel.advanceHover(true,1./30.);takeScreenshot(evidence.getChildFile("cabinet-power-off.png"));
            presetBrowser.onUser(0);++probe.phase;return 0;
        }
        if(probe.phase==2)
        {
            check(selected()==0&&cabinetBypassed&&cabinetState.bypassed,"The physical-cabinet preference survives a different User Preset");
            const auto full=std::find_if(namCatalog.entries.begin(),namCatalog.entries.end(),[&](const auto& e){return captureKind(e.identity.file)==tonegrid::cabinet::CaptureKind::fullRig;});
            if(full!=namCatalog.entries.end())
            {auto loaded=amp::loadNamAmp(namLibrary,{full->identity,0,0},namLite);if(loaded.source)commitSource(std::move(loaded.source));check(!cabinetPanel.power.isEnabled()&&!cabinetPanel.browse.isEnabled(),"Full Rig retains a grey Cabinet area with inactive power and browse controls");}
            else check(false,"A real Full Rig capture is available for the disabled-control test");
            presetBrowser.onUser(1);++probe.phase;return 0;
        }
        if(probe.phase==3)
        {
            cabinetPanel.power.setToggleState(true,dontSendNotification);cabinetPanel.power.onClick();
            check(!cabinetBypassed&&!cabinetState.bypassed&&cabinetPanel.power.getToggleState(),"Switching back to Head Only restores the retained power preference and ON re-enables its IR");
            probe.starts=presetPreparationsStarted;probe.published=presetPreparationsPublished;
            for(int i=0;i<1000;++i)presetBrowser.onTone(101+size_t(i%10));presetBrowser.onTone(101);++probe.phase;return 0;
        }
        if(probe.phase==4)
        {
            check(toneBankActive&&name.getText().contains("Have You Ever Seen")&&presetPreparationsStarted==probe.starts+1&&presetPreparationsPublished==probe.published+1,"Rapid ToneBank choices use the same bounded loading path and select the final lesson tone");
            deviceProcessor.prepareToPlay(48000,256);AudioBuffer<float> buffer(2,256);MidiBuffer midi;bool finite=true;float peak=0;
            for(int block=0;block<64;++block)
            {for(int i=0;i<256;++i)for(int c=0;c<2;++c)buffer.setSample(c,i,.004f*std::sin(float(block*256+i)*.031f));deviceProcessor.processBlock(buffer,midi);for(int c=0;c<2;++c)for(int i=0;i<256;++i){const auto v=buffer.getSample(c,i);finite&=std::isfinite(v);peak=jmax(peak,std::abs(v));}}
            deviceProcessor.releaseResources();check(finite&&peak>0&&!deviceProcessor.fault.load(),"A new lesson preset renders finite non-silent audio through the shared real NAM and cabinet graph");
            presetBrowser.showToneBank(true);presetBrowser.setSearch("Police");presetBrowser.setSize(1000,650);
            ++probe.phase;return 0;
        }
        if(probe.phase==5)
        {
            {MemoryOutputStream stream;PNGImageFormat png;if(png.writeImageToStream(presetBrowser.createComponentSnapshot(presetBrowser.getLocalBounds()),stream))evidence.getChildFile("tonebank-creator-columns.png").replaceWithData(stream.getData(),stream.getDataSize());}
            presetBrowser.showToneBank(false);
            {MemoryOutputStream stream;PNGImageFormat png;if(png.writeImageToStream(presetBrowser.createComponentSnapshot(presetBrowser.getLocalBounds()),stream))evidence.getChildFile("seven-starter-presets.png").replaceWithData(stream.getData(),stream.getDataSize());}
            evidence.getChildFile("native-refinements142.txt").replaceWithText(probe.report+"Preparations started: "+String(presetPreparationsStarted)+"\nPreparations published: "+String(presetPreparationsPublished)+"\n");
            ++probe.phase;return probe.ok?1:-1;
        }
        return probe.ok?1:-1;
    }
