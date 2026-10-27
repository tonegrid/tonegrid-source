// Included inside RigHost. Real component rendering, isolated profile, no audio device.
    bool verifyUi123(String& result,const File& folder)
    {
        bool ok=true;auto check=[&](bool pass,const String& text){ok&=pass;result+=String(pass?"PASS ":"FAIL ")+"UI123 "+text+"\n";};
        check(ready()&&ampSource->type()==amp::SourceType::Nam,"isolated NAM host ready");if(!ready())return false;
        const auto outputBefore=tonegrid::output::encode(outputSettings).toString();
        const auto ampBefore=namamp::encode(dynamic_cast<amp::NamAmp*>(ampSource.get())->state()).toString();
        const auto midiBefore=board::encodeBindings(midiController.assignments()).toString();
        // Exercise the production dropdown-to-popup route without an audio/MIDI device.
        if(cards[0])
        {
            auto* choice=dynamic_cast<ComboBox*>(cards[0]->findChildWithID("pedal-preset"));
            const int pedal=PedalCard::engineFor(0),target=board::pedalMidiTarget(pedal);
            const auto stateBefore=pedals->get(pedal);const int selectedCapture=choice?choice->getSelectedId():0;
            if(choice)choice->setSelectedId(PedalCard::midiLearnItem,sendNotificationSync);
            check(pedalMidiDialog&&pedalMidiView&&choice&&choice->getSelectedId()==selectedCapture,"LEARN MIDI dropdown opens the real popup without changing the capture");
            if(pedalMidiView)
            {
                auto* message=dynamic_cast<Label*>(pedalMidiView->findChildWithID("pedal-midi-status"));
                check(message&&message->getText().contains("First select a MIDI input"),"missing MIDI hardware shows setup guidance");
                pedalMidiView->setSize(280,260);
                bool fit=true;for(auto* child:pedalMidiView->getChildren())fit&=pedalMidiView->getLocalBounds().contains(child->getBounds());
                check(fit,"MIDI popup actions fit a narrow window");
                midiController.startLearn(target,*this);
                midiController.enqueue(MidiMessage::programChange(3,25));midiController.drain(*this);
                check(midiController.assignments()[size_t(target)].valid()&&midiController.learningTarget()<0&&pedals->get(pedal).enabled==stateBefore.enabled,"popup learns through the existing controller without toggling the pedal");
                pedalMidiView->onClear();
                check(!midiController.assignments()[size_t(target)].valid(),"popup clears the saved pedal assignment");
                midiController.startLearn(target,*this);pedalMidiView->onClose();
                check(midiController.learningTarget()<0,"popup Cancel immediately stops learning");
                delete pedalMidiDialog.getComponent();pedalMidiTarget=-1;
            }
        }
        auto screenshot=[&](Component& component,const String& name,float scale=1.f)
        {
            const auto bounds=component.getLocalBounds();const auto image=component.createComponentSnapshot(bounds,true,scale);
            check(image.isValid()&&std::abs(image.getWidth()-bounds.getWidth()*scale)<=1&&std::abs(image.getHeight()-bounds.getHeight()*scale)<=1,"raster dimensions "+name);
            MemoryOutputStream stream;check(PNGImageFormat().writeImageToStream(image,stream)&&folder.getChildFile(name+".png").replaceWithData(stream.getData(),stream.getDataSize()),"capture "+name);
        };
        // Same client size as the user's 1697x927 reference, excluding its 27px OS title bar.
        setSize(1697,900);
        const auto savedInput=pedals->front.get();
        guitarIn.setValue(7,sendNotificationSync);gateThreshold.setValue(-70,sendNotificationSync);
        for(auto* slider:{&guitarIn,&gateThreshold})
        {
            const auto point=slider->getLocalBounds().getCentre().toFloat();const auto now=Time::getCurrentTime();
            MouseEvent event(Desktop::getInstance().getMainMouseSource(),point,ModifierKeys::leftButtonModifier,1,0,0,0,0,slider,slider,now,point,now,2,false);
            slider->mouseDoubleClick(event);
        }
        check(guitarIn.getValue()==0&&gateThreshold.getValue()==-50,"1.1.38 actual double clicks return both toolbar knobs to centre");
        check(pedals->front.get().gainDb==0&&pedals->front.get().thresholdDb==-50,"double-click resets reach the existing audio input controls");
        pedals->front.set(savedInput);syncInput();
        for(int i=0;i<20;++i){guitarIn.advance(false,1./30.);gateThreshold.advance(false,1./30.);}
        for(int i=0;i<20;++i)advanceToolbarHints({-100,-100},1./30.);
        check(save.getAlpha()==0&&learn.getAlpha()==0&&rescan.getAlpha()==0,"secondary toolbar buttons start hidden outside their regions");
        for(int i=0;i<20;++i)advanceToolbarHints(sections[1].getCentre(),1./30.);
        check(save.getAlpha()==1&&learn.getAlpha()==1&&rescan.getAlpha()==0,"preset region reveals Save and MIDI Learn independently");
        screenshot(*this,"ui138-preset-actions");
        for(int i=0;i<20;++i)advanceToolbarHints(sections[5].getCentre(),1./30.);
        check(save.getAlpha()==0&&learn.getAlpha()==0&&rescan.getAlpha()==1,"MIDI region reveals Refresh while preset actions fade away");
        screenshot(*this,"ui138-midi-refresh");
        for(int i=0;i<20;++i)advanceToolbarHints({-100,-100},1./30.);
        screenshot(*this,"ui138-values-hidden");
        recordingHintOpacity=1;playModule.contentView().setHintOpacity(1);screenshot(*this,"ui138-recording-labels");recordingHintOpacity=0;playModule.contentView().setHintOpacity(0);
        for(int i=0;i<20;++i){guitarIn.advance(true,1./30.);gateThreshold.advance(true,1./30.);}
        check(guitarIn.readoutOpacity()==1&&gateThreshold.readoutOpacity()==1,"both production readouts fade in when the mouse approaches");
        screenshot(*this,"ui138-values-visible");
        for(int i=0;i<20;++i){guitarIn.advance(false,1./30.);gateThreshold.advance(false,1./30.);}
        check(guitarIn.readoutOpacity()==0&&gateThreshold.readoutOpacity()==0,"both production readouts fade out after leaving");
        const auto* comp=cards[3]?cards[3]->findChildWithID("pedal-preset"):nullptr;
        const auto* wah=cards[2]?cards[2]->findChildWithID("pedal-preset"):nullptr;
        check(comp&&wah&&std::abs(comp->getBoundsInParent().getY()-wah->getBoundsInParent().getY())<=1,"Studio Comp selector shares the top-row selector height");
        for(float scale:{1.f,1.25f,1.5f,2.f})screenshot(*this,"ui123-reference-"+String(roundToInt(scale*100)),scale);
        for(int width:{1360,1660,2240})
        {
            setSize(width,940);
            check(!namGain.getBounds().intersects(ampEqGear.getBounds())&&!namVolume.getBounds().intersects(ampEqGear.getBounds())&&loopControls.getBottom()<=sections[0].getY(),"rack controls remain separate at "+String(width));
            const auto actions=recordingActionBounds();
            bool hintsFit=true;for(const auto& action:actions)hintsFit&=pedalArea.getRight()-action.getRight()-10>=GlyphArrangement::getStringWidthInt(tonegrid::ui::font(12),"Download");
            check(hintsFit,"recording labels fit to the right of all four icons at "+String(width));
            const auto layout=tonegrid::assets::boardBounds(pedalArea);
            for(int i=0;i<board::count;++i)check(cards[size_t(i)]&&getLocalArea(cards[size_t(i)]->getParentComponent(),cards[size_t(i)]->getBounds())==layout[size_t(i)],"unchanged host-coordinate artwork grid "+String(width)+" pedal "+String(i));
            for(auto* c:std::initializer_list<Component*>{&presetEntry,&save,&learn,&pitchMode,&tunerButton,&guitarCalibration,&audio,&midiDevice,&rescan})
                check(c->getHeight()==tonegrid::ui::buttonHeight&&getLocalBounds().contains(c->getBounds()),"toolbar dimensions "+String(width)+" "+c->getComponentID());
            screenshot(*this,"ui123-width-"+String(width));
        }
        check(namGain.getMinimum()==-24&&namGain.getMaximum()==24&&namVolume.getMinimum()==-24&&namVolume.getMaximum()==24,"amplifier Gain and Volume share the requested -24/+24 dB range");
        check(outputBefore==tonegrid::output::encode(outputSettings).toString()&&ampBefore==namamp::encode(dynamic_cast<amp::NamAmp*>(ampSource.get())->state()).toString()&&midiBefore==board::encodeBindings(midiController.assignments()).toString(),"rendering and resizing preserve output, amplifier and MIDI state");
        setSize(1697,900);ampEqGear.onClick();if(ampEqDialog){screenshot(*ampEqDialog,"ui123-eq");delete ampEqDialog.getComponent();}else check(false,"EQ popup opens");
        showPresetBrowser();if(presetDialog){screenshot(*presetDialog,"ui123-presets");delete presetDialog.getComponent();}else check(false,"preset popup opens");
        const auto nativeState=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();
        static ampTests::Trace trace;auto plugin=std::make_unique<ampTests::Plugin>(trace);PluginDescription description;plugin->fillInPluginDescription(description);
        commitSource(std::make_unique<amp::Vst3AmpHost>(std::make_unique<AudioPluginFormatManager>(),description,std::move(plugin),"Synthetic UI123"));
        check(!ampEqGear.isVisible()&&!namGain.isVisible()&&!namVolume.isVisible()&&pluginChoice.isVisible(),"existing VST3 visibility retained");screenshot(*this,"ui123-vst3");
        auto loaded=amp::loadNamAmp(namLibrary,nativeState,namLite);check(loaded.result.wasOk(),"restore isolated NAM source");if(loaded.source)commitSource(std::move(loaded.source));
        return ok;
    }
