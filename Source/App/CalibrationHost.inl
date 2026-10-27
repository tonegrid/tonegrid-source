// Included inside RigHost: calibration is a global device preference.
    tonegrid::calibration::DeviceInput calibrationDevice() const
    {
        tonegrid::calibration::DeviceInput input;
        if(auto* device=devices.getCurrentAudioDevice())
        {
            const auto setup=devices.getAudioDeviceSetup();input.type=devices.getCurrentAudioDeviceType();
            input.name=setup.inputDeviceName;input.channels=device->getInputChannelNames();
            input.channel=device->getActiveInputChannels().findNextSetBit(0);
            input.connected=audioTap.inputConnected()&&device->isPlaying()&&input.channel>=0;
        }
        return input;
    }
    tonegrid::calibration::ChainStatus calibrationChainStatus() const
    {
        using namespace tonegrid::calibration;
        if(!calibrationSettings.enabled)return ChainStatus::disabled;
        if(!tonegrid::calibration::active(calibrationSettings))return ChainStatus::setup;
        const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get());
        if(!pedals||!pedals->completeCalibrationChain()||!native||native->calibrationInfo().invalid||!native->calibrationInfo().input)return ChainStatus::partial;
        return ChainStatus::calibrated;
    }
    tonegrid::calibration::InterfaceStatus calibrationInterfaceStatus() const
    {
        const auto input=calibrationDevice();
        const bool deviceMatches=!hardwareEnabled||(input.connected&&tonegrid::calibration::matches(calibrationSettings,input.type,input.name,input.channel));
        return tonegrid::calibration::interfaceStatus(calibrationSettings,deviceMatches);
    }
    void updateCalibrationStatus()
    {
        using namespace tonegrid::calibration;
        const auto state=calibrationInterfaceStatus();
        guitarCalibration.getProperties().set("interface-status",static_cast<int>(state));
        guitarCalibration.setNeedsAttention(!calibrationSettings.completed||!valid(calibrationSettings)||state==InterfaceStatus::setup||state==InterfaceStatus::verification);
        guitarCalibration.setTooltip(String(interfaceStatusName(state))+"\nNAM CHAIN: "+chainName(calibrationChainStatus())+"\n"+(calibrationSettings.signalVerified?"Raw input activity verified without clipping.":"Raw input has not been automatically verified.")+"\nInterface settings are user-confirmed, not independently electrically measured. VST3 amplifiers use their own level conventions.");
        guitarCalibration.repaint();
        if(const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get()))
        {
            captureHeading.setText("Amp",dontSendNotification);
            captureHeading.setTooltip(native->captureDescription());namFilename.setTooltip(native->state().model.file+"\n"+native->captureDescription());
        }
    }
    void showGuitarCalibration(Component* owner=nullptr)
    {
        if(calibrationDialog){calibrationDialog->toFront(true);return;}
        auto* wizard=new tonegrid::calibration::GuitarWizard(calibrationSettings,appPaths.calibrationProfiles());
        const auto safe=Component::SafePointer<RigHost>(this);
        wizard->currentDevice=[safe]{return safe?safe->calibrationDevice():tonegrid::calibration::DeviceInput{};};
        wizard->reading=[safe]{return safe?tonegrid::calibration::RawReading{safe->audioTap.takeRawInputPeak(),safe->calibrationDevice().connected}:tonegrid::calibration::RawReading{};};
        wizard->onChannel=[safe](int channel)
        {
            if(!safe)return false;auto setup=safe->devices.getAudioDeviceSetup();setup.inputChannels.clear();setup.inputChannels.setBit(channel);setup.useDefaultInputChannels=false;
            const auto error=safe->devices.setAudioDeviceSetup(setup,true);if(error.isNotEmpty()){safe->report(error);return false;}
            return true;
        };
        wizard->onSave=[safe](tonegrid::calibration::Settings settings){return safe&&safe->setCalibration(settings);};
        wizard->refresh();calibrationDialog=tonegrid::popup::launch("Guitar Calibration",*wizard,owner?*owner:*this,true);
        wizard->onBodyHeight=[safe](int height)
        {
            if(!safe||!safe->calibrationDialog)return;
            if(auto* dialog=dynamic_cast<tonegrid::popup::Window*>(safe->calibrationDialog.getComponent()))dialog->resizeToBody(height);
        };
    }
    tonegrid::calibration::WarningContext captureWarningContext(const namamp::Entry& entry) const
    {
        const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get());
        return {native&&native->state().model==entry.identity,outputSettings.ampNormalizationEnabled};
    }
    void queueCaptureWarning(const namamp::Entry& entry)
    {
        // The actual selected stage and current normalization mode decide the
        // warning. No filename, factory identity or metadata completeness waiver.
        if(calibrationWarnings.shouldWarn(entry.calibration,entry.identity.sha256,captureWarningContext(entry)))pendingCalibrationWarning=entry;
    }
    void pollCalibration()
    {
        if(hardwareEnabled)
        {
            const auto input=calibrationDevice();
            const bool needs=calibrationSettings.completed&&(!input.connected||!tonegrid::calibration::matches(calibrationSettings,input.type,input.name,input.channel));
            if(needs!=calibrationSettings.needsVerification)
            {
                player.setProcessor(nullptr);calibrationSettings.needsVerification=needs;
                if(pedals)pedals->setCalibration(calibrationSettings);configureOutputDsp();attach();
                if(needs)report("Audio input changed. Open GUITAR CALIBRATION to verify your interface and input settings.");
            }
            // Interface setup remains available even if no amplifier has loaded.
            if(!startupPresetPending&&welcomeOffered&&!calibrationOnboardingOffered&&pedals&&isShowing()&&ModalComponentManager::getInstance()->getNumModalComponents()==0)
            {offerCalibrationOnboarding();}
            if(const auto* native=ready()?dynamic_cast<const amp::NamAmp*>(ampSource.get()):nullptr)
            {
                const auto key=native->state().model.sha256+String(outputSettings.ampNormalizationEnabled?":normalized":":raw");
                if(key!=lastCalibrationWarningKey)
                {
                    lastCalibrationWarningKey=key;
                    for(const auto& entry:namCatalog.entries)if(entry.identity==native->state().model){queueCaptureWarning(entry);break;}
                }
            }
            else lastCalibrationWarningKey.clear();
        }
        updateCalibrationStatus();
        if(startupPresetPending||!pendingCalibrationWarning||calibrationWarningDialog||ModalComponentManager::getInstance()->getNumModalComponents()>0||(tone3000Window&&tone3000Window->isVisible()))return;
        const auto entry=*pendingCalibrationWarning;pendingCalibrationWarning.reset();
        // A pending warning may outlive a quick source or mode change.
        const auto* activeAmp=dynamic_cast<const amp::NamAmp*>(ampSource.get());
        if(!activeAmp||!(activeAmp->state().model==entry.identity)
           ||!calibrationWarnings.shouldWarn(entry.calibration,entry.identity.sha256,captureWarningContext(entry)))return;
        if(!calibrationWarnings.acknowledge(entry.identity.sha256))report("Could not persist capture calibration acknowledgement.");
        auto* warning=new tonegrid::calibration::CaptureWarning(entry.calibration,entry.identity.file.replaceCharacter('\\','/').fromLastOccurrenceOf("/",false,false));
        warning->onUse=[safe=Component::SafePointer<RigHost>(this),hash=entry.identity.sha256](bool disable)
        {if(safe){if(disable&&!safe->calibrationWarnings.acknowledge(hash,true))safe->report("Could not save warning preference.");if(safe->calibrationWarningDialog)safe->calibrationWarningDialog->closeButtonPressed();}};
        calibrationWarningDialog=tonegrid::popup::launch("Capture Calibration",*warning,*this,true);
    }
    void offerCalibrationOnboarding()
    {
        calibrationOnboardingOffered=true;
        if(tonegrid::calibration::needsOnboarding(calibrationSettings))audioSettings();
    }
