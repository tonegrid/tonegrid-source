    bool verifyRelease134Ui(String& report,const File& folder)
    {
        bool ok=true;int checks=0;const auto check=[&](bool value,const String& text){++checks;ok&=value;report+=(value?"PASS ":"FAIL ")+String("Release134 UI ")+text+"\n";};
        const auto find=[&](const String& name)->namamp::Entry{for(const auto& e:namCatalog.entries)if(e.identity.file==name)return e;return {};};
        const auto first=find("First.nam"),second=find("Second.nam");check(first.identity.valid()&&second.identity.valid(),"both external example captures are indexed");
        const auto select=[&](const namamp::Entry& entry){for(size_t i=0;i<namCatalog.entries.size();++i)if(namCatalog.entries[i].identity==entry.identity){namChoice.setSelectedId(int(i)+1,dontSendNotification);selectNam();return;}};
        select(first);outputSettings.ampNormalizationEnabled=true;outputSettings.ampNormalizationEnhanced=true;syncOutputUi();pendingCalibrationWarning.reset();queueCaptureWarning(first);pollCalibration();
        check(!calibrationWarningDialog&&!pendingCalibrationWarning,"input +6.5 dBu / output absent AMP+CAB has no popup in Full mode");
        outputSettings.ampNormalizationEnhanced=false;queueCaptureWarning(first);pollCalibration();check(!calibrationWarningDialog&&!pendingCalibrationWarning,"same final capture has no popup in Partial mode");
        updateCalibrationStatus();check(captureHeading.getTooltip().contains("6.50 dBu")&&captureHeading.getTooltip().contains("Output: Not available"),"tooltip keeps the original calibration details");
        const auto before=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();
        outputSettings.ampNormalizationEnabled=false;queueCaptureWarning(first);check(pendingCalibrationWarning.has_value(),"OFF queues the incomplete calibration warning");
        outputSettings.ampNormalizationEnabled=true;pollCalibration();check(!calibrationWarningDialog&&!pendingCalibrationWarning,"queued OFF warning is re-evaluated and suppressed after normalization turns ON");
        check(dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model==before.model&&namGain.getValue()==0&&namVolume.getValue()==0,"warning decisions preserve source, drive and volume");
        select(second);queueCaptureWarning(second);pollCalibration();check(calibrationWarningDialog&&calibrationWarningDialog->isVisible(),"AC30 with neither reference still opens the actual popup");
        if(calibrationWarningDialog){MemoryOutputStream png;PNGImageFormat().writeImageToStream(calibrationWarningDialog->createComponentSnapshot(calibrationWarningDialog->getLocalBounds()),png);folder.getChildFile("ac30-warning.png").replaceWithData(png.getData(),png.getDataSize());delete calibrationWarningDialog.getComponent();}
        select(first);outputSettings.ampNormalizationEnabled=false;queueCaptureWarning(first);pollCalibration();check(calibrationWarningDialog&&calibrationWarningDialog->isVisible(),"Marshall still opens a warning when normalization is OFF");
        if(calibrationWarningDialog)delete calibrationWarningDialog.getComponent();report+=String(checks)+" Release134 UI checks\n";return ok;
    }
