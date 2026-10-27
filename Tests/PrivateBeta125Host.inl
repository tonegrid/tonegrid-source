    bool verifyPrivateBeta125(String& report,const File& folder)
    {
        bool ok=true;int checks=0;const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("private beta startup ")+label+"\n";};
        auto* native=dynamic_cast<amp::NamAmp*>(ampSource.get());
        check(ready()&&native&&!hardwareEnabled,"fresh profile boots a real bundled NAM without TONEX or audio hardware");
        // A slow first scan must produce a test failure, never dereference a missing source.
        if(!ready()||!native){report+="INFO startup log: "+log.getText()+"\nINFO pending amp scan="+String(namService.busy()?1:0)+", cabinet scan="+String(cabinetRefreshing.valid()?1:0)+", pedal load="+String(loading.valid()?1:0)+"\n";return false;}
        bool playVisible=playModuleSurface.isVisible();for(const auto& card:cards)playVisible&=card&&card->isVisible()&&card->getParentComponent()==&playModuleSurface;
        check(playVisible,"PLAY owns visible pedal cards immediately after first launch");
        check(native&&selected()==0&&rigStore.readName(0).contains("John Mayer"),"John Mayer preset 1 starts automatically with a NAM amp");
        const auto factoryInventory=nlohmann::json::parse(appPaths.factoryAmps().getChildFile("manifest.json").loadFileAsString().toStdString());
        check(namCatalog.entries.size()==factoryInventory.at("ampCount").get<size_t>()&&board::presetCount(3)==6,"all approved factory amps and six Wampler distortion captures are indexed");
        bool presets=true;for(int i=0;i<7;++i){board::State s;PluginDescription none;presets&=rigStore.loadForSelection(i,rigIdentities(),s,none).wasOk()&&s.outputDsp&&s.outputDsp->ampNormalizationEnabled&&s.outputDsp->ampNormalizationEnhanced;}
        check(presets&&!rigStore.contains(7),"exactly seven editable starter presets install with Full normalization");
        board::State initial;PluginDescription none;const auto restored=rigStore.loadForSelection(0,rigIdentities(),initial,none);
        check(restored.wasOk()&&normalize.getSelectedId()==3&&namGain.getValue()==initial.namAmp.gainDb&&namVolume.getValue()==initial.namAmp.volumeDb&&outputSettings.ampEq.enabled==initial.outputDsp->ampEq.enabled,"active controls reflect the first requested starter sound");
        check(!missingCapturesDialog,"startup has no obsolete TS9 or missing-capture popup");
        const auto archive=appPaths.user().getParentDirectory().getChildFile("ToneGrid/Legacy/Rigs-v3/slot-001.tonexrig");
        if(archive.existsAsFile())check(archive.loadFileAsString()=="<PedalRig version=\"3\" name=\"Mayer Clean\"/>","archived prototype preset remains untouched and is not imported");
        if(rigStore.readName(0).contains("My edit"))check(name.getText().contains("My edit"),"existing User Preset edits survive startup and stale VST preferences");
        name.setText("User-edited starter",false);saveSlot();const auto before=SHA256(rigStore.slotFile(selected())).toHexString();
        auto again=tonegrid::starters::seed(appPaths.starterPresets(),rigStore,namCatalog,rigIdentities());
        check(again.result.wasOk()&&!again.installed&&SHA256(rigStore.slotFile(selected())).toHexString()==before,"repeat startup seeding never overwrites an edited User Preset");
        takeScreenshot(folder.getChildFile("private-beta125-first-launch.png"));ok=verifyToneBank133(report,folder)&&ok;report+=String(checks)+" private beta startup checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
