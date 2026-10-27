// Included inside RigHost. Synthetic captures and an isolated profile only.
    bool verifyRelease122(String& result,const File& folder,bool restarted)
    {
        bool ok=true;auto check=[&](bool pass,const String& name){ok&=pass;result+=String(pass?"PASS ":"FAIL ")+"122 host "+name+"\n";};
        check(ready()&&ampSource->type()==amp::SourceType::Nam,"NAM graph ready");if(!ready())return false;
        const auto savedFile=appPaths.user().getChildFile("personal-test.nam");
        if(restarted)
        {
            const auto hash=SHA256(savedFile).toHexString();
            recallSlot(selected());
            check(pedals->get(3).preset==board::presetCount(3)&&pedals->identity(3,board::presetCount(3))==hash&&pedals->available(3,board::presetCount(3)),"personal identity resolves to the same capture after full restart");
            check(outputSettings.ampEq.lowCut&&outputSettings.ampEq.lowMidDb==4,"advanced EQ survives full restart and preset recall");
            check(looper.state()==tonegrid::looping::Mode::empty,"looper is empty after full restart");
            const auto installed=userPedals.file(3,hash);check(installed.isAChildOf(appPaths.user())&&installed.deleteFile(),"remove isolated test capture");
            recallSlot(selected());
            check(!pedals->get(3).enabled&&pedals->get(3).preset==board::presetCount(3)&&pedals->identity(3,board::presetCount(3))==hash,"missing personal capture bypasses without substituting a factory sound");
            return ok;
        }
        auto json=nlohmann::json::parse(namTests::linear().toStdString());json["metadata"]={{"input_level_dbu",11.5},{"output_level_dbu",11.5}};
        check(savedFile.replaceWithText(json.dump()),"synthetic calibrated capture written to isolated profile");
        auto imported=std::make_shared<tonegrid::userpedals::Prepared>(tonegrid::userpedals::prepare(savedFile,namLite));
        check(imported->result.wasOk()&&imported->model.dsp,"personal import prepared");if(!imported->model.dsp)return false;
        const auto identity=imported->model.hash;acceptPersonalPedal(3,imported,"https://www.tone3000.com/tones/test",true);
        check(pedals->get(3).preset==board::presetCount(3)&&pedals->identity(3,board::presetCount(3))==identity,"personal import activates reserved capture without moving factory indices");
        auto setting=pedals->get(3);setting.enabled=true;setting.output=3;pedals->set(3,setting);
        outputSettings.ampEq.enabled=true;outputSettings.ampEq.lowCut=true;outputSettings.ampEq.lowMidDb=4;configureOutputDsp();syncOutputUi();name.setText("122 personal & advanced EQ",dontSendNotification);saveSlot();
        check(rigStore.contains(selected()),"personal User Preset saved");
        pedals->set(3,board::defaults(3,0));outputSettings.ampEq={};configureOutputDsp();recallSlot(selected());
        check(pedals->get(3).enabled&&pedals->get(3).output==3&&pedals->identity(3,board::presetCount(3))==identity&&outputSettings.ampEq.lowCut,"saved personal capture and intentional gain restored");
        for(int p=0;p<board::count;++p){auto value=pedals->get(p);value.enabled=false;pedals->set(p,value);}pedals->front.set({0,-70,false});graph.prepareToPlay(48000,128);
        AudioBuffer<float> dc(2,128);MidiBuffer events;for(int n=0;n<150;++n){for(int c=0;c<2;++c)for(int i=0;i<128;++i)dc.setSample(c,i,.02f);graph.processBlock(dc,events);}
        check(std::abs(dc.getSample(0,127))<1.e-5f,"advanced low cut reaches the actual NAM graph");recallSlot(selected());
        auto warningJson=json;warningJson["metadata"].erase("output_level_dbu");const auto partial=appPaths.user().getChildFile("partial-test.nam");partial.replaceWithText(warningJson.dump());
        std::promise<tonegrid::userpedals::Prepared> promise;userPedalLoading=promise.get_future();promise.set_value(tonegrid::userpedals::prepare(partial,namLite));pendingUserPedal=3;pendingUserLabel="Partial test";pendingUserUrl={};pendingUserImport=true;pollPersonalPedal();
        check(pedalImportDialog&&pedalImportDialog->isVisible()&&pedals->identity(3,board::presetCount(3))==identity,"incomplete calibration opens warning before changing the current sound");
        if(pedalImportDialog){auto stream=folder.getChildFile("release122-calibration-warning.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&PNGImageFormat().writeImageToStream(pedalImportDialog->createComponentSnapshot(pedalImportDialog->getLocalBounds()),*stream),"calibration warning screenshot");pedalImportDialog->exitModalState(0);delete pedalImportDialog.getComponent();}
        check(pedals->identity(3,board::presetCount(3))==identity,"canceling incomplete calibration keeps the selected capture");
        std::promise<tonegrid::userpedals::Prepared> again;userPedalLoading=again.get_future();again.set_value(tonegrid::userpedals::prepare(partial,namLite));pollPersonalPedal();
        check(pedalImportDialog&&pedalImportDialog->isVisible(),"calibration warning repeats on every uncalibrated selection without an ignore preference");
        if(pedalImportDialog){pedalImportDialog->exitModalState(0);delete pedalImportDialog.getComponent();}
        deletePersonalPedal(3,identity);check(pedalDeleteDialog&&pedalDeleteDialog->isVisible(),"personal capture trash asks for deletion confirmation");
        if(pedalDeleteDialog){pedalDeleteDialog->exitModalState(0);delete pedalDeleteDialog.getComponent();}
        check(userPedals.file(3,identity).existsAsFile(),"canceling deletion keeps the personal capture and its sound");
        ampEqGear.onClick();check(ampEqDialog&&ampEqDialog->isVisible(),"EQ icon opens the actual popup");
        if(ampEqDialog){auto stream=folder.getChildFile("release122-eq-popup.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&PNGImageFormat().writeImageToStream(ampEqDialog->createComponentSnapshot(ampEqDialog->getLocalBounds()),*stream),"EQ popup screenshot");delete ampEqDialog.getComponent();}
        // Rendering the real custom menu content exercises personal row/trash bounds
        // without synthesizing OS mouse input or opening the external folder manager.
        for(auto& card:cards)if(card&&PedalCard::engineFor(int(&card-&cards[0]))==3)
        {
            auto* combo=dynamic_cast<ComboBox*>(card->findChildWithID("pedal-preset"));
            for(PopupMenu::MenuItemIterator it(*combo->getRootMenu());it.next();)if(auto* row=it.getItem().customComponent.get())
            {row->setSize(460,30);auto stream=folder.getChildFile("release122-personal-row.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&PNGImageFormat().writeImageToStream(row->createComponentSnapshot(row->getLocalBounds()),*stream),"personal capture row and delete button screenshot");}
        }
        for(int width:{1360,1660,2240}){setSize(width,940);check(loopControls.getBounds().getBottom()<=sections[0].getY()&&!ampEqGear.getBounds().intersects(namGain.getBounds())&&!ampEqGear.getBounds().intersects(namVolume.getBounds()),"rack and looper layout at "+String(width));takeAmpScreenshot(folder.getChildFile("release122-rack-"+String(width)+".png"));}
        setSize(1660,940);
        {auto stream=folder.getChildFile("release122-full-ui.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream),"complete pedalboard screenshot");}
        showPresetBrowser();if(presetDialog){auto stream=folder.getChildFile("release122-user-presets.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&PNGImageFormat().writeImageToStream(presetDialog->createComponentSnapshot(presetDialog->getLocalBounds()),*stream),"User Preset share icons screenshot");delete presetDialog.getComponent();}
        looper.prepare(48000);AudioBuffer<float> b(2,128);for(int c=0;c<2;++c)for(int i=0;i<128;++i)b.setSample(c,i,.02f);looper.advance();for(int i=0;i<80;++i)looper.process(b.getArrayOfWritePointers(),2,128);looper.advance();looper.process(b.getArrayOfWritePointers(),2,128);
        const auto duration=looper.seconds();recallSlot(selected());check(looper.state()==tonegrid::looping::Mode::playing&&looper.seconds()==duration,"preset recall preserves independent recorded loop");
        looper.clear();b.clear();for(int i=0;i<4;++i)looper.process(b.getArrayOfWritePointers(),2,128);
        sharePreset(selected());check(shareConfirmDialog&&shareConfirmDialog->isVisible()&&!shareDialog,"sharing asks before opening an email");
        if(shareConfirmDialog){auto stream=folder.getChildFile("release122-share-confirm.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&PNGImageFormat().writeImageToStream(shareConfirmDialog->createComponentSnapshot(shareConfirmDialog->getLocalBounds()),*stream),"share confirmation screenshot");shareConfirmDialog->exitModalState(0);delete shareConfirmDialog.getComponent();}
        check(!shareDialog,"NO does not launch an email");
        const auto installed=userPedals.file(3,identity);check(installed.deleteFile(),"delete the active personal capture manually");userPedals.refresh();refreshPersonalPedals();checkMissingPersonalPedals();
        check(!pedals->get(3).enabled&&!pedals->available(3,board::presetCount(3))&&pedals->identity(3,board::presetCount(3))==identity,"folder deletion bypasses active capture without losing its preset identity");
        auto retry=pedals->get(3);retry.enabled=true;pedals->set(3,retry);check(!pedals->get(3).enabled,"power and MIDI cannot reactivate a deleted capture still held for the audio fade");
        check(savedFile.copyFileTo(installed),"restore isolated capture for restart test");userPedals.refresh();recallSlot(selected());
        return ok;
    }
