    bool verifyBetaWelcome125(String& report,const File& folder)
    {
        bool ok=true;int checks=0;const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("beta welcome ")+label+"\n";};
        check(ready()&&normalize.getSelectedId()==3&&outputSettings.ampNormalizationEnhanced,"fresh session selects Full normalization");
        check(!tonegrid::welcome::dismissed(appPaths.betaWelcome()),"new profile shows welcome by default");
        const auto original=tonegrid::output::encode(outputSettings).toString();
        DocumentWindow testWindow("ToneGrid beta welcome test",Colours::black,DocumentWindow::closeButton);testWindow.setUsingNativeTitleBar(true);testWindow.setContentNonOwned(this,false);testWindow.centreWithSize(1360,820);testWindow.setVisible(true);
        pollWelcome();auto* view=welcomeDialog?dynamic_cast<tonegrid::welcome::View*>(welcomeDialog->getContentComponent()->findChildWithID("beta-welcome")):nullptr;
        // The common frame hosts its content through a viewport.
        const auto findView=[&]() -> tonegrid::welcome::View*
        {
            if(!welcomeDialog)return nullptr;
            std::function<tonegrid::welcome::View*(Component*)> find=[&](Component* c)->tonegrid::welcome::View*{if(auto* found=dynamic_cast<tonegrid::welcome::View*>(c))return found;for(auto* child:c->getChildren())if(auto* found=find(child))return found;return nullptr;};
            return find(welcomeDialog->getContentComponent());
        };
        view=findView();check(view&&welcomeDialog->isCurrentlyModal(),"startup displays branded modal welcome before calibration");
        if(view)
        {
            check(view->close.getButtonText()=="Close"&&view->never.getButtonText()=="Don't show this message again"&&view->getLocalBounds().contains(view->close.getBounds())&&view->getLocalBounds().contains(view->never.getBounds()),"both requested actions remain visible inside scrollable content");
            MemoryOutputStream image;PNGImageFormat().writeImageToStream(welcomeDialog->getContentComponent()->createComponentSnapshot(welcomeDialog->getContentComponent()->getLocalBounds()),image);folder.getChildFile("beta-welcome125.png").replaceWithData(image.getData(),image.getDataSize());
            view->close.onClick();check(!tonegrid::welcome::dismissed(appPaths.betaWelcome()),"Close does not suppress future launches");
        }
        delete welcomeDialog.getComponent();welcomeOffered=false;pollWelcome();view=findView();check(view!=nullptr,"welcome remains eligible on a subsequent launch after Close");
        if(view){view->never.onClick();check(tonegrid::welcome::dismissed(appPaths.betaWelcome()),"Don't show again persists suppression");}
        delete welcomeDialog.getComponent();welcomeOffered=tonegrid::welcome::dismissed(appPaths.betaWelcome());pollWelcome();check(!welcomeDialog,"suppressed welcome is not opened again");
        check(tonegrid::output::encode(outputSettings).toString()==original,"welcome actions leave musical settings untouched");
        offerCalibrationOnboarding();
        check(audioDialog&&audioDialog->isVisible()&&!calibrationDialog,"fresh onboarding opens Audio Settings before Guitar Calibration");
        if(audioDialog)
        {
            std::function<Component*(Component*)> findButton=[&](Component* component)->Component*{if(component->getComponentID()=="guitar-calibration-settings")return component;for(auto* child:component->getChildren())if(auto* found=findButton(child))return found;return nullptr;};
            auto* next=dynamic_cast<TextButton*>(findButton(audioDialog->getContentComponent()));
            if(next)next->onClick();
            check(next&&calibrationDialog&&calibrationDialog->isVisible()&&audioDialog&&audioDialog->isVisible(),"Audio Settings calibration button opens the wizard above retained Settings");
            delete calibrationDialog.getComponent();delete audioDialog.getComponent();
        }
        report+=String(checks)+" beta welcome host checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
