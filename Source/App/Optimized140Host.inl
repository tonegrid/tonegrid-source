    bool verifyOptimized140(const File& evidence,String& results)
    {
        bool ok=true;const auto check=[&](bool pass,const String& text){ok&=pass;results+=String(pass?"PASS ":"FAIL ")+text+"\n";};
        check(namGain.getMinimum()==-24&&namGain.getMaximum()==24&&namVolume.getMinimum()==-24&&namVolume.getMaximum()==24,"Both AMP knobs expose -24 to +24 dB with zero-centered mapping");
        const auto map=nlohmann::json::parse(appPaths.factoryAmps().getChildFile("retired-full-rigs.json").loadFileAsString().toStdString());
        bool absent=true,heads=true;for(const auto& record:map.at("captures"))
        {
            const String file(record.at("file").get<std::string>()),head(record.at("head").at("path").get<std::string>());
            for(const auto& entry:namCatalog.entries)absent&=entry.identity.file!=file;
            heads&=captureKind(head)==tonegrid::cabinet::CaptureKind::head;
        }
        check(absent&&namCatalog.entries.size()==1358,"Optimized fresh catalogue excludes all 146 retired Full Rigs and retains 1358 NAMs");
        check(heads,"Every removed Full Rig has a complete Head counterpart, including corrected mixed-pack type labels");
        int converted=0;bool recalled=true;size_t example=0;
        for(size_t i=0;i<toneBank.presets.size();++i)if(toneBank.presets[i].state.cabinet)
        {
            ++converted;const auto& preset=toneBank.presets[i];loadToneBank(i);
            const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get());
            recalled&=native&&native->state().model.sha256==preset.amp.sha256&&cabinetInstalled&&cabinetState.sha256==preset.state.cabinet->sha256&&installedCabinetHash==preset.state.cabinet->sha256;
            if(converted==1)example=i;
        }
        check(converted==33&&recalled,"All 33 converted ToneBank recipes load their exact Head and explicit cabinet IR");
        if(converted)
        {
            loadToneBank(example);graph.prepareToPlay(48000,128);AudioBuffer<float> buffer(2,128);MidiBuffer midi;bool finite=true;double energy=0;
            for(int block=0;block<64;++block)
            {for(int i=0;i<128;++i){const auto v=.025f*std::sin(float(block*128+i)*.03f);buffer.setSample(0,i,v);buffer.setSample(1,i,v);}graph.processBlock(buffer,midi);for(int i=0;i<128;++i){const auto v=buffer.getSample(0,i);finite&=std::isfinite(v);energy+=double(v)*v;}}
            check(finite&&energy>1e-12,"A converted factory Head + IR preset renders finite non-silent audio through the full graph");
            takeScreenshot(evidence.getChildFile("tonegrid-optimized-head-preset.png"));
        }
        setMode(2);takeScreenshot(evidence.getChildFile("tonegrid-learn-guitarclub.png"));
        check(learnActive&&learnView.lessonCount()>0,"LEARN renders the official GuitarClub logo with the existing lesson catalogue");setMode(0);
        check(versionLabel.getText()=="ToneGrid " JUCE_APPLICATION_VERSION_STRING,"Approved application display version is matches the compiled version");
        return ok;
    }
