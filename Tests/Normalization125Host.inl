    bool verifyNormalization125(String& report,const File& folder,bool restarted)
    {
        bool ok=true;int checks=0;const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("normalization host ")+label+"\n";};
        check(ready()&&dynamic_cast<amp::NamAmp*>(ampSource.get()),"isolated NAM host without audio hardware");if(!ready())return false;
        for(int p=0;p<board::count;++p){auto state=pedals->get(p);state.enabled=false;pedals->set(p,state);}pedals->front.set({0,-80,false});
        graph.prepareToPlay(48000,128);AudioBuffer<float> buffer(2,128);MidiBuffer midi;
        const auto render=[&]{for(int block=0;block<24;++block){for(int c=0;c<2;++c)for(int i=0;i<128;++i)buffer.setSample(c,i,.05f);graph.processBlock(buffer,midi);}return buffer.getSample(0,127);};
        const auto matches=[&](float db){return std::abs(render()-.05f*Decibels::decibelsToGain(db))<1e-5f;};
        if(restarted){check(normalize.getSelectedId()==2&&!outputSettings.ampNormalizationEnhanced&&matches(6),"Partial survives application restart with original NAM correction only");}
        else
        {
            check(normalize.getSelectedId()==3&&matches(12),"legacy ON loads as Full and applies NAM plus measured gains");
            normalize.setSelectedId(2,sendNotificationSync);check(matches(6),"Partial dropdown applies NAM loudness only");
            normalize.setSelectedId(1,sendNotificationSync);check(matches(0),"OFF dropdown returns to unity");
            normalize.setSelectedId(3,sendNotificationSync);namChoice.setSelectedId(2,sendNotificationSync);graph.prepareToPlay(48000,128);
            check(matches(0)&&normalize.getTooltip().contains("Full currently behaves like Partial"),"unmeasured capture clears preceding trim and explains fallback");
            namChoice.setSelectedId(1,sendNotificationSync);graph.prepareToPlay(48000,128);normalize.setSelectedId(2,sendNotificationSync);
            slot.setSelectedId(20,dontSendNotification);name.setText("Partial test",false);saveSlot();normalize.setSelectedId(3,sendNotificationSync);recallSlot(19);
            check(normalize.getSelectedId()==2&&!outputSettings.ampNormalizationEnhanced&&matches(6),"user preset saves and recalls Partial");
            for(const int width:{1360,1660,1920})
            {
                setSize(width,940);
                // Normalize now belongs to the card's hover overlay. Its bounds
                // are local to actions; the full-card browse button is behind it
                // intentionally. Compare the actual controls in board coordinates.
                const auto controlBounds=getLocalArea(&normalize,normalize.getLocalBounds());
                const auto helpBounds=getLocalArea(&normalizeHelp,normalizeHelp.getLocalBounds());
                check(sections[0].contains(controlBounds)&&ampPanel.getBounds().contains(controlBounds)
                    &&ampPanel.getBounds().contains(helpBounds)&&!controlBounds.intersects(helpBounds),
                    "dropdown and help fit amp card without overlap at "+String(width));
                takeAmpScreenshot(folder.getChildFile("normalization125-"+String(width)+".png"));
            }
            persistSelection();
        }
        report+=String(checks)+" normalization host checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
