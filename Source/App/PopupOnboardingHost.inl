// Included inside RigHost; operates only in the disposable IR validation profile.
    bool verifyPopupOnboarding(const File& evidence,String& results)
    {
        bool ok=true;
        const auto check=[&](bool pass,const String& text){ok&=pass;results+=String(pass?"PASS ":"FAIL ")+text+"\n";};
        const auto snapshot=[&](Component& component,const String& name)
        {
            PNGImageFormat format;MemoryOutputStream data;
            return format.writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()),data)
                &&evidence.getChildFile(name+".png").replaceWithData(data.getData(),data.getDataSize());
        };
        const auto find=[](Component& root,const String& id)->Component*
        {
            std::function<Component*(Component&)> visit=[&](Component& component)->Component*
            {if(component.getComponentID()==id)return &component;for(auto* child:component.getChildren())if(auto* match=visit(*child))return match;return nullptr;};
            return visit(root);
        };
        const auto oldBounds=getBounds();const bool oldVisible=isVisible(),oldOffered=welcomeOffered,oldOnboarding=calibrationOnboardingOffered;
        const auto oldSettings=calibrationSettings;
        namBrowserLifetimeTests::MouseHost host;host.setSize(getWidth(),getHeight());host.addAndMakeVisible(*this);setTopLeftPosition(0,0);
        welcomeOffered=false;pollWelcome();
        auto* welcome=welcomeDialog.getComponent();auto* view=welcome?dynamic_cast<tonegrid::welcome::View*>(find(*welcome,"beta-welcome")):nullptr;
        check(welcome&&view&&welcome->isVisible(),"Welcome opens with direct Settings and Calibrate actions");
        for(int i=0;i<30;++i)modalBackdrop.advance(1./60.);
        check(modalBackdrop.isVisible()&&modalBackdrop.getAlpha()==1,"Open Welcome softly dims the main interface");
        check(audio.getX()<guitarCalibration.getX()&&audio.getY()==guitarCalibration.getY(),"Audio header places Settings before Calibration");
        if(view)
        {
            check(snapshot(*welcome->getContentComponent(),"tonegrid-welcome"),"Render the shorter Welcome and both setup buttons");
            view->settings.onClick();
            check(audioDialog&&audioDialog->isVisible()&&welcome->isVisible()&&Component::getCurrentlyModalComponent()==audioDialog.getComponent(),"Welcome Settings opens above Welcome without closing it");
            if(audioDialog)
            {
                check(snapshot(*audioDialog->getContentComponent(),"tonegrid-welcome-settings"),"Render Audio Settings above retained Welcome");
                auto* setup=find(*audioDialog,"guitar-calibration-settings");if(auto* button=dynamic_cast<Button*>(setup))button->onClick();
                check(calibrationDialog&&audioDialog->isVisible()&&welcome->isVisible()&&Component::getCurrentlyModalComponent()==calibrationDialog.getComponent(),"Settings can open Calibration as a third dialog layer");
                if(calibrationDialog){calibrationDialog->closeButtonPressed();delete calibrationDialog.getComponent();}
                check(audioDialog->isVisible()&&welcome->isVisible()&&Component::getCurrentlyModalComponent()==audioDialog.getComponent(),"Closing Calibration returns to Audio Settings");
                audioDialog->closeButtonPressed();delete audioDialog.getComponent();
            }
            check(welcome->isVisible()&&Component::getCurrentlyModalComponent()==welcome,"Closing Settings returns to Welcome");
            view->calibrate.onClick();
            check(calibrationDialog&&calibrationDialog->isVisible()&&welcome->isVisible()&&Component::getCurrentlyModalComponent()==calibrationDialog.getComponent(),"Welcome Calibrate opens directly above Welcome");
            if(calibrationDialog)
            {
                check(snapshot(*calibrationDialog->getContentComponent(),"tonegrid-welcome-calibration"),"Render the direct guitar calibration tutorial");
                calibrationDialog->closeButtonPressed();delete calibrationDialog.getComponent();
            }
            check(calibrationSettings.completed==oldSettings.completed&&calibrationSettings.referenceDbU==oldSettings.referenceDbU
                  ||(!std::isfinite(calibrationSettings.referenceDbU)&&!std::isfinite(oldSettings.referenceDbU)&&calibrationSettings.completed==oldSettings.completed),"Cancelling setup dialogs preserves the current input calibration");
            check(snapshot(*this,"tonegrid-popup-backdrop"),"Render the main board with its single 50 percent shade");
        }
        if(welcome){welcome->closeButtonPressed();delete welcomeDialog.getComponent();}
        for(int i=0;i<30;++i)modalBackdrop.advance(1./60.);
        check(!modalBackdrop.isVisible()&&!modalBackdrop.hasDialogs(),"Closing the final popup restores the main interface");
        calibrationSettings={};updateCalibrationStatus();
        for(int i=0;i<30;++i)guitarCalibration.advance(false,1./60.,.75);
        check(guitarCalibration.needsAttention()&&guitarCalibration.getAlpha()==1,"Incomplete calibration remains visible with the new blinking indicator");
        check(snapshot(*this,"tonegrid-calibration-incomplete"),"Render the new Settings and Calibration order and larger orange dot");
        calibrationSettings.completed=true;calibrationSettings.referenceDbU=12;calibrationSettings.deviceType="Test";calibrationSettings.deviceName="Test";calibrationSettings.channel=0;updateCalibrationStatus();
        for(int i=0;i<30;++i)guitarCalibration.advance(false,1./60.,0);
        check(!guitarCalibration.needsAttention()&&guitarCalibration.getAlpha()==0,"Completed calibration hides after leaving the audio region");
        for(int i=0;i<30;++i)guitarCalibration.advance(true,1./60.,0);
        check(guitarCalibration.getAlpha()==1,"Completed calibration returns when entering the audio region");
        check(snapshot(*this,"tonegrid-calibration-complete"),"Render the completed calibration check on hover");
        calibrationSettings.needsVerification=true;updateCalibrationStatus();
        check(guitarCalibration.needsAttention(),"Changed input requiring calibration verification keeps the action visible");
        calibrationSettings=oldSettings;updateCalibrationStatus();welcomeOffered=oldOffered;calibrationOnboardingOffered=oldOnboarding;
        host.removeChildComponent(this);setBounds(oldBounds);setVisible(oldVisible);return ok;
    }
