// Included inside RigHost; exercise real components under the production wrapper.
    bool verifyWindow125(String& report,const File& folder)
    {
        bool ok=true;const auto check=[&](bool pass,const String& message){ok&=pass;report+=String(pass?"PASS ":"FAIL ")+message+"\n";};
        auto* wrapper=getParentComponent();auto* window=findParentComponentOfClass<DocumentWindow>();
        check(wrapper&&window,"production window owns the live scalable board");if(!wrapper||!window)return false;
        const auto before=tonegrid::output::encode(outputSettings).toString();
        for(const auto size:std::initializer_list<Point<int>>{{1660,940},{1360,820},{1280,690},{960,540},{640,360},{320,200},{240,180},{2400,1500}})
        {
            window->setBoundsConstrained(window->getBounds().withSize(size.x,size.y));
#if JUCE_MAC
            // AppKit constrains the native frame to the laptop work area. Test
            // the requested client size when it fits, and the exact available
            // client size otherwise, accounting for the native title bar.
            const auto* display=Desktop::getInstance().getDisplays().getDisplayForRect(window->getScreenBounds());
            const auto nativeFrame=window->getPeer()->getFrameSizeIfPresent();
            const auto frame=nativeFrame?*nativeFrame:BorderSize<int>{};
            const int expectedWidth=display?jmin(size.x,display->userArea.getWidth()-frame.getLeftAndRight()):size.x;
            const int expectedHeight=display?jmin(size.y,display->userArea.getHeight()-frame.getTopAndBottom()):size.y;
            check(display&&window->getWidth()==expectedWidth&&window->getHeight()==expectedHeight,"native resize fits requested size or Mac work area "+String(size.x)+"x"+String(size.y));
            if(window->getWidth()!=expectedWidth||window->getHeight()!=expectedHeight)report+="WINDOW_DIAGNOSTIC actual="+window->getScreenBounds().toString()+" expected-size="+String(expectedWidth)+"x"+String(expectedHeight)+" work-area="+(display?display->userArea.toString():"none")+" native-top="+String(frame.getTop())+" native-bottom="+String(frame.getBottom())+"\n";
#else
            check(window->getWidth()==size.x&&window->getHeight()==size.y,"native resize accepted "+String(size.x)+"x"+String(size.y));
#endif
            // Reveal the new hover overlay before testing Normalize hit targets.
            for(int n=0;n<30;++n)advanceToolbarHints(ampPanel.getBounds().getCentre(),1./30.);
            for(auto* control:std::initializer_list<Component*>{&audio,&guitarCalibration,&checkUpdate,&sendDiagnostic,&namGain,&namVolume,&normalize,&presetEntry})
            {
                const auto visible=wrapper->getLocalArea(control,control->getLocalBounds());
                const auto centre=visible.getCentre();auto* hit=wrapper->getComponentAt(centre);
                check(wrapper->getLocalBounds().expanded(1).contains(visible)&&hit&&(hit==control||control->isParentOf(hit)),"visible and clickable "+control->getName()+" at "+String(size.x));
            }
            for(auto& card:cards)check(card&&wrapper->getLocalBounds().expanded(1).contains(wrapper->getLocalArea(card.get(),card->getLocalBounds())),"all pedal cards fit at "+String(size.x));
            if(size.x==1660||size.x==1360)check(getTransform().isIdentity()==(wrapper->getWidth()>=1360&&wrapper->getHeight()>=820),"artwork retains native rendering when the actual client area fits the design minimum");
            if(size.x==1280||size.x==960||size.x==640)
            {
                const auto image=wrapper->createComponentSnapshot(wrapper->getLocalBounds(),true,1.5f);MemoryOutputStream png;
                check(PNGImageFormat().writeImageToStream(image,png)&&folder.getChildFile("window-"+String(size.x)+".png").replaceWithData(png.getData(),png.getDataSize()),"HiDPI snapshot "+String(size.x));
            }
        }
        window->setSize(640,360);
        audio.onClick();
        if(audioDialog)
        {
            check(std::abs(Component::getApproximateScaleFactorForComponent(audioDialog.getComponent())-Component::getApproximateScaleFactorForComponent(window))<.01f,"audio settings retains readable desktop scale when board is tiny");
            delete audioDialog.getComponent();
        }
        else check(false,"Audio Settings opens from the compact board");
        showGuitarCalibration();
        if(calibrationDialog)
        {
            check(std::abs(Component::getApproximateScaleFactorForComponent(calibrationDialog.getComponent())-Component::getApproximateScaleFactorForComponent(window))<.01f,"calibration retains readable desktop scale");
            delete calibrationDialog.getComponent();
        }
        else check(false,"Guitar Calibration opens from the compact board");
        check(before==tonegrid::output::encode(outputSettings).toString(),"resizing preserves sound settings");
        for(const auto area:std::initializer_list<Rectangle<int>>{{0,0,1366,728},{0,0,1280,680},{1920,0,1024,640},{-1280,0,1280,720},{0,0,853,480}})
            check(area.contains(tonegrid::ui::ScalableBoard::initialBounds(area)),"startup fits work area "+area.toString());
        {
            tonegrid::t3k::Tone3000BrowserWindow browser(folder.getChildFile("unused-library"),folder.getChildFile("unused-browser"),{});
            browser.setBoundsConstrained(browser.getBounds().withSize(480,320));
            check(browser.isResizable()&&browser.getWidth()==480&&browser.getHeight()==320,"Tone3000 browser resizes below previous minimum");
            check(browser.isUsingNativeTitleBar()&&!browser.authorizationStarted(),"Tone3000 retains native Close; no sign-in or network access in this test");
        }
        return ok;
    }
