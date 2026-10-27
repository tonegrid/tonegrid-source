    bool verifyRelease130(String& report,const File& folder)
    {
        bool ok=true;auto check=[&](bool pass,const String& name){ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};check(ready(),"synthetic NAM ready");if(!ready())return false;
        setSize(1660,940);const auto area=pedalArea;const auto before=tonegrid::output::encode(outputSettings).toString();const auto* source=ampSource.get();const auto nodes=graph.getNumNodes();
        check(playTab.getBottom()<=tonegrid::ui::windowInset+tonegrid::ui::headerHeight&&jamTab.getBounds().getY()==playTab.getY(),"tabs fit original 80-pixel header");
        auto snapshot=[&](Component& c,const String& name){MemoryOutputStream png;check(PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds(),true,1.5f),png)&&folder.getChildFile(name+".png").replaceWithData(png.getData(),png.getDataSize()),"render "+name);};snapshot(*this,"release130-play");
        jamTab.onClick();check(jamActive&&jamView.isVisible()&&pedalArea==area&&!cards[0]->isVisible()&&namGain.isVisible(),"JAM replaces board without reducing PLAY space; amp controls remain");check(jamView.trackCount()==900,"JAM prepopulated in real host");snapshot(*this,"release130-jam");jamView.playTrackForTest(0);check(jamView.playerRequested()&&jamView.playerBounds().getHeight()==200,"inline player opens at 200 pixels in real host");snapshot(*this,"release131-inline-player");playTab.onClick();check(!jamView.playerRequested(),"leaving JAM stops backing playback");jamTab.onClick();
        for(int i=0;i<20;++i){playTab.onClick();jamTab.onClick();}playTab.onClick();check(!jamActive&&cards[0]->isVisible()&&!jamView.isVisible()&&pedalArea==area&&graph.getNumNodes()==nodes&&ampSource.get()==source&&tonegrid::output::encode(outputSettings).toString()==before,"repeated tab changes preserve audio graph, source and sound");
        ampEqGear.onClick();check(ampEqDialog!=nullptr,"new AMP EQ opens");if(ampEqDialog)
        {
            for(int width:{1040,700,480}){ampEqDialog->setSize(width,650);ampEqControls.selectBand(6);auto* viewport=ampEqControls.findParentComponentOfClass<Viewport>();if(width==1040&&viewport)for(int i:{3,4})check(viewport->getViewArea().contains(ampEqControls.findChildWithID("amp-eq-"+String(i))->getBounds()),"cut filter visible without scrolling in normal-size EQ");snapshot(*ampEqDialog.getComponent(),"release130-eq-window-"+String(width));}
            auto* presence=dynamic_cast<Slider*>(ampEqControls.findChildWithID("amp-eq-9"));if(presence)presence->setValue(3,sendNotificationSync);check(outputSettings.ampEq.presenceDb==3,"new EQ publishes to real host DSP");delete ampEqDialog.getComponent();
        }
        return ok;
    }
