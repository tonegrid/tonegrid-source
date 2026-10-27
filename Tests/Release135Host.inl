    bool verifyRelease135Ui(String& report,const File& folder)
    {
        bool ok=true;int checks=0;const auto check=[&](bool value,const String& label){++checks;ok&=value;report+=(value?"PASS ":"FAIL ")+label+"\n";};
        check(ready()&&!hardwareEnabled&&ampSource->type()==amp::SourceType::Nam,"isolated startup reaches a working NAM without audio hardware or TONEX");if(!ready())return false;
        if(folder.getFileName().endsWith("interrupted"))check(blockAutomaticVst&&!pluginValidation.valid()&&vst3Directory.getChildFile("last-failed-load.xml").existsAsFile()&&!vst3Directory.getChildFile("loading.xml").existsAsFile(),"an interrupted load starts NAM, preserves failure details and consumes the retry-block marker once");
        auto* previous=ampSource.get();const auto source=dynamic_cast<amp::NamAmp*>(previous)->state();
        path.setText(folder.getChildFile("Missing/TONEX.vst3").getFullPathName(),false);
        vstSourceButton.onClick();check(displayedSource==amp::SourceType::Vst3&&scanPlugins.isVisible()&&pluginChoice.isVisible()&&addPluginFolder.isVisible(),"VST click always exposes Scan VST3, plugin list and custom-folder button");
        check(ampSource.get()==previous&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model==source.model,"VST view selection preserves the loaded NAM and its controls");
        check(vstSourceButton.getToggleState()&&!namSourceButton.getToggleState(),"source buttons reflect the selected view independently of the active engine");
        static ampTests::Trace trace;PluginDescription stale;stale.name="Missing amp";stale.pluginFormatName="VST3";stale.manufacturerName="Synthetic";stale.uniqueId=456;stale.version="1";stale.fileOrIdentifier=folder.getChildFile("Missing/Other.vst3").getFullPathName();
        vst3::Catalog selection;selection.add(stale);selectionStore.write(selection);vstSourceButton.onClick();
        check(scanPlugins.isVisible()&&addPluginFolder.isEnabled()&&ampSource.get()==previous,"stale plugin preferences cannot hide the VST selection controls");
        std::atomic<bool> cancel{false};const auto executable=File::getSpecialLocation(File::currentExecutableFile);
        auto validation=vst3::validate(executable,stale,{},-1,cancel);
        check(validation.result.failed()&&validation.result.getErrorMessage().contains("missing")&&ampSource.get()==previous,"actual validation worker reports missing plugin without replacing the NAM");
        validation=vst3::validate(executable,stale,{},-1,cancel,0);
        check(validation.result.failed()&&validation.result.getErrorMessage().contains("timed out"),"validation timeout terminates its child process");
        cancel=true;validation=vst3::validate(executable,stale,{},-1,cancel);
        check(validation.result.failed()&&ampSource.get()==previous,"validation cancellation preserves current audio source");
        takeScreenshot(folder.getChildFile("vst-without-tonex.png"));
        namSourceButton.onClick();check(displayedSource==amp::SourceType::Nam&&ampSource.get()==previous&&namFolder.isVisible(),"return to NAM keeps the original working capture");
        auto changed=stale;changed.version="2";check(board::ampCompatibility(stale,changed,true).wasOk(),"stable plugin identity survives a version update");changed.uniqueId++;
        check(board::ampCompatibility(stale,changed,true).failed(),"a different plugin identity is still rejected");
        const auto details=diagnosticReport();check(details.contains("Models catalogue: present")&&details.contains("Embedded browser:")&&details.contains("Custom VST3 folders:"),"diagnostics include package, browser and custom-plugin-path readiness");
        vstSourceButton.onClick();requestPluginValidation(stale);namSourceButton.onClick();if(pluginValidation.valid())pluginValidation.wait();timerCallback();
        check(displayedSource==amp::SourceType::Nam&&ampSource.get()==previous&&!pluginValidation.valid(),"a pending VST request cannot replace NAM after the user changes their selection");
        report+=String(checks)+" Release135 host checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
