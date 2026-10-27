// Methods included inside RigHost solely for isolated CLI regressions and previews.
    bool verifyToneBankWorkflow(String& result,bool bootstrap=false)
    {
        int checks=0;bool ok=true;auto check=[&](bool pass,const String& message){++checks;ok=ok&&pass;result+=(pass?"PASS ":"FAIL ")+String("ToneBank host ")+message+"\n";};
        if(bootstrap){check(!ready()&&pedals&&presetEntry.isEnabled(),"browser available without an installed startup amp or VST3 dependency");loadToneBank(0);check(ready(),"ToneBank initializes existing graph from NAM with no previous AmpSource");}
        check(ready()&&toneBank.presets.size()==101,"ready with offline bank of 101");if(!ready()||toneBank.presets.empty())return false;
        const auto bankHash=SHA256(toneBankCache).toHexString();const auto original=toneBank.presets;
        showPresetBrowser();check(presetDialog&&presetBrowser.isShowing()&&!presetBrowser.toneTabActive(),"entry opens USER PRESETS panel without resizing main window");
        presetBrowser.showToneBank(true);check(presetBrowser.visibleCount()==(int)toneBank.presets.size(),"TONEBANK displays every row");
        presetBrowser.activateRow(0);
        check(ampSource->type()==amp::SourceType::Nam&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model.sha256==toneBank.presets[0].amp.sha256,"row click resolves and loads exact NAM amp");
        check(!rigStore.contains(selected())&&toneBankActive,"row load does not create a User Preset");
        const auto matches=[&](const tonegrid::tonebank::Preset& wanted)
        {
            const auto input=pedals->front.get();bool same=input.gainDb==wanted.state.input.gainDb&&input.thresholdDb==wanted.state.input.thresholdDb&&input.gate==wanted.state.input.gate;
            for(int id=0;id<board::count;++id){const auto a=pedals->get(id),b=wanted.state.pedals[(size_t)id];same=same&&a.preset==b.preset&&a.enabled==b.enabled&&a.input==b.input&&a.mix==b.mix&&a.output==b.output&&a.ratio==b.ratio&&a.tone==b.tone&&a.eqBands==b.eqBands&&a.wahSensitivity==b.wahSensitivity&&a.wahRange==b.wahRange&&a.wahDecay==b.wahDecay;}
            return same&&tonegrid::output::encode(outputSettings).toString()==tonegrid::output::encode(*wanted.state.outputDsp).toString();
        };
        check(matches(toneBank.presets[0]),"input, Pitch, all 13 pedals, 3D, MOD, Delay, Reverb and normalization applied through existing engine");
        auto* native=dynamic_cast<amp::NamAmp*>(ampSource.get());check(native&&native->state().gainDb==toneBank.presets[0].state.namAmp.gainDb&&native->state().volumeDb==toneBank.presets[0].state.namAmp.volumeDb,"amp Gain and Volume applied");
        for(int i=0;i<10;++i)loadToneBank((size_t)(i%2));check(matches(toneBank.presets[1])&&graph.getNodes().size()==9,"rapid repeated selections preserve graph and final complete state");
        toneBank.presets[0].state.outputDsp->pitch={tonegrid::pitch::Mode::Octaver,-7,74,41,22};toneBank.presets[0].state.outputDsp->spreadEnabled=true;toneBank.presets[0].state.outputDsp->offsetMs=-9.5f;
        toneBank.presets[0].state.outputDsp->mod={true,tonegrid::mod::Type::Flanger,3.2f,63,44};toneBank.presets[0].state.outputDsp->delay={true,600,71,29};toneBank.presets[0].state.outputDsp->reverb={true,tonegrid::reverb::Type::Spring,2.7f,37,63,41};
        auto& eq=toneBank.presets[0].state.outputDsp->ampEq;eq.enabled=true;eq.presenceDb=4;eq.depthDb=-2;eq.lowCut=true;eq.lowCutHz=110;eq.highCut=true;eq.highCutHz=9000;toneBank.presets[0].state.pedals[0].mix=.35f;
        loadToneBank(0);check(matches(toneBank.presets[0])&&effectsStateMatches(),"synthetic non-default every-effect state loads and UI controls synchronize");
        name.setText("ToneBank synthetic editable copy",false);saveSlot();const int copySlot=selected();check(rigStore.contains(copySlot)&&!toneBankActive&&SHA256(toneBankCache).toHexString()==bankHash,"Save creates editable User Preset and never writes bank");
        loadToneBank(1);recallSlot(copySlot);check(matches(toneBank.presets[0])&&!toneBankActive,"User Preset recalls after ToneBank with all effects");loadToneBank(1);check(matches(toneBank.presets[1]),"ToneBank recalls after User Preset");
        namGain.setValue(5,sendNotificationSync);namVolume.setValue(-8,sendNotificationSync);outputSettings.ampNormalizationEnabled=false;syncOutputUi();
        native=dynamic_cast<amp::NamAmp*>(ampSource.get());const auto before=native->state();auto* oldSource=ampSource.get();auto* oldProcessor=ampStage->getProcessor();
        toneBank.presets[0]=original[0];toneBank.presets[0].amp.sha256=String::repeatedString("a",64);loadToneBank(0);
        native=dynamic_cast<amp::NamAmp*>(ampSource.get());check(ampSource.get()==oldSource&&ampStage->getProcessor()==oldProcessor&&native&&native->state().model==before.model&&native->state().gainDb==before.gainDb&&native->state().volumeDb==before.volumeDb&&!outputSettings.ampNormalizationEnabled,"missing amp preserves current NAM object, resource, Gain, Volume and normalization exactly");
        check(missingCapturesDialog&&missingCapturesDialog->isShowing()&&missingCapturesDialog->findChildWithID("popup-frame"),"missing amp presents an actual visible popup");
        check(pedals->get(12).enabled==original[0].state.pedals[12].enabled&&pedals->front.get().thresholdDb==original[0].state.input.thresholdDb&&latestLog.getText().contains("Required NAM Amp is missing:"),"missing amp loads remainder and exact non-blocking warning");
        bool vstAvailable=false;
        for(size_t i=0;i<pluginCatalog.plugins.size();++i)if(!pluginCatalog.plugins[i].name.containsIgnoreCase("TONEX")){pluginChoice.setSelectedId((int)i+1,sendNotificationSync);vstAvailable=ampSource->type()==amp::SourceType::Vst3;break;}
        check(vstAvailable,"independent VST3 fixture available");
        if(vstAvailable)
        {
            oldSource=ampSource.get();oldProcessor=ampStage->getProcessor();MemoryBlock beforeBlob,afterBlob;oldSource->captureState(beforeBlob);const auto description=vst3::Identity::of(*ampSource->pluginDescription());outputSettings.ampNormalizationEnabled=false;syncOutputUi();loadToneBank(0);ampSource->captureState(afterBlob);
            check(ampSource.get()==oldSource&&ampStage->getProcessor()==oldProcessor&&ampSource->type()==amp::SourceType::Vst3&&vst3::Identity::of(*ampSource->pluginDescription())==description&&beforeBlob==afterBlob&&!outputSettings.ampNormalizationEnabled,"missing amp preserves current VST3 object, identity, complete blob and normalization exactly");
            check(pedals->get(12).enabled&&latestLog.getText().contains(tonegrid::tonebank::missingAmp(toneBank.presets[0].amp)),"missing amp from VST3 loads remainder and exact warning");
        }
        toneBank.presets[0]=original[0];toneBank.presets[0].state.pedals[1].enabled=true;toneBank.presets[0].state.pedals[2].enabled=true;toneBank.presets[0].captures[1].sha256=String::repeatedString("b",64);toneBank.presets[0].captures[2].sha256=String::repeatedString("c",64);loadToneBank(0);
        check(!pedals->get(1).enabled&&!pedals->get(2).enabled&&pedals->get(12).enabled&&ampSource->type()==amp::SourceType::Nam,"two missing NAM pedals bypass only affected pedals and still load required amp");
        check(pedals->get(1).preset==original[0].state.pedals[1].preset&&pedals->get(2).preset==original[0].state.pedals[2].preset,"missing captures never substitute preset IDs");
        auto* warning=missingCapturesDialog?missingCapturesDialog->getContentComponent():nullptr;check(warning&&missingCapturesDialog->isShowing(),"missing pedals present the same popup");delete missingCapturesDialog.getComponent();
        toneBank.presets=original;loadToneBank(0);check(SHA256(toneBankCache).toHexString()==bankHash,"loading/editing/saving/repeated selection never changes local JSON");
        MemoryOutputStream output;PNGImageFormat().writeImageToStream(presetBrowser.createComponentSnapshot(presetBrowser.getLocalBounds()),output);File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("tonebank-browser.png").replaceWithData(output.getData(),output.getDataSize());
        presetBrowser.showToneBank(false);MemoryOutputStream userOutput;PNGImageFormat().writeImageToStream(presetBrowser.createComponentSnapshot(presetBrowser.getLocalBounds()),userOutput);File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("user-presets-browser.png").replaceWithData(userOutput.getData(),userOutput.getDataSize());
        result+=String(checks)+" ToneBank host checks\n";return ok;
    }

    void openTone3000Preview(){namBrowser.captures.onClick();}
    bool verifyTone3000Workflow(String& result)
    {
        using namespace tonegrid::t3k;bool ok=true;int checks=0;auto check=[&](bool pass,const String& label){++checks;ok&=pass;result+=(pass?"PASS ":"FAIL ")+String("Tone3000 host ")+label+"\n";};
        auto* initial=dynamic_cast<amp::NamAmp*>(ampSource.get());check(initial&&ready()&&!audioReady&&!midi,"temporary NAM host with hardware disabled");if(!initial)return false;
        const auto identity=initial->state();const auto settings=tonegrid::output::encode(outputSettings).toString();const auto nodes=graph.getNumNodes();const auto preference=sourcePreference.loadFileAsString();
        // Make the imported pack start beyond the viewport, as in a large library.
        for(int n=0;n<40;++n){const auto model=namLibrary.folder().getChildFile("Earlier "+String(n).paddedLeft('0',3)+"/Existing.nam");model.getParentDirectory().createDirectory();model.replaceWithText(namTests::linear());}
        refreshNamLibrary();awaitNamRefreshForTest();
        std::atomic<bool> stopped{false};tone3000Tests::Mock io;const auto raw=namTests::linear();MemoryBlock nam(raw.toRawUTF8(),raw.getNumBytesAsUTF8());
        Image art(Image::RGB,20,16,true);Graphics g(art);g.fillAll(Colours::orange);MemoryOutputStream png;PNGImageFormat().writeImageToStream(art,png);
        io.handler=[&](const Request& r)->Response
        {
            if(r.url.endsWith("oauth/token"))return tone3000Tests::json(object({{"access_token","test-token"},{"token_type","bearer"}}));
            if(r.url.contains("tones/42"))return tone3000Tests::json(tone3000Tests::tone());
            if(r.url.contains("models?"))return tone3000Tests::json(object({{"page",1},{"total_pages",1},{"data",var(Array<var>{object({{"id",101},{"tone_id",42},{"name","Z Last.nam"},{"architecture_version","2"},{"model_url","https://www.tone3000.com/api/v1/models/101/download"}}),object({{"id",102},{"tone_id",42},{"name","a First.nam"},{"architecture_version","2"},{"model_url","https://www.tone3000.com/api/v1/models/102/download"}})})}}));
            if(r.url.contains("/download"))return tone3000Tests::bytes(nam);return tone3000Tests::bytes(png.getMemoryBlock());
        };
        Tone3000OAuthSession session;Tone3000Client client(io,stopped);client.exchange(session.accept(tone3000Tests::callbackFor(session)),session.codeVerifier());
        auto pack=client.getPack("42");Tone3000DownloadManager manager(namLibrary.folder());const auto imported=manager.import(client,pack,stopped);tone3000ImportFinished(imported);
        check(imported.ok&&imported.models==2&&namCatalog.entries.size()==43,"complete pack imported and real library refreshed");
        const auto folder=imported.folder.getRelativePathFrom(namLibrary.folder()).replaceCharacter('\\','/');
        const auto relative=namBrowser.firstModelIn(folder);auto file=namLibrary.folder().getChildFile(relative);
        auto* selected=dynamic_cast<amp::NamAmp*>(ampSource.get());
        check(selected&&selected->state().model.file==relative&&relative.endsWith("a First.nam")&&selected->state().gainDb==identity.gainDb&&selected->state().volumeDb==identity.volumeDb&&graph.getNumNodes()==nodes&&tonegrid::output::encode(outputSettings).toString()==settings,"first NAM in display order auto-loads, preserving gain, volume, graph and effects");
        check(sourcePreference.loadFileAsString()!=preference,"auto-loaded NAM is persisted through existing source selection");
        showNamChooser();check(namBrowserDialog&&namBrowserDialog->getName()=="Local NAM Library"&&namBrowser.captures.isVisible(),"real Local NAM Library dialog and clickable CTA");
        auto* leaf=namBrowser.find(relative);
        check(leaf&&leaf->isSelected()&&namBrowser.find("TONE3000")->isOpen()&&namBrowser.find(folder)->isOpen(),"reopened library expands latest pack and highlights auto-loaded NAM");
        check(selected&&!thumbnail.usesFallback()&&thumbnail.hasTone3000Mark(),"automatic load uses existing thumbnail and source mark");
        checkCompactLayout(check);
        check(namGain.findColour(Slider::textBoxOutlineColourId).isTransparent()&&namVolume.findColour(Slider::textBoxOutlineColourId).isTransparent(),"Gain and Volume values have no outlines");
        const auto beforePreset=ampSource.get();const auto beforeSettings=tonegrid::output::encode(outputSettings).toString();
        slot.setSelectedId(6,dontSendNotification);name.setText("UX Test",false);saveSlot();
        const auto savedXml=XmlDocument::parse(rigStore.slotFile(5));auto renamedXml=savedXml?std::make_unique<XmlElement>(*savedXml):nullptr;if(renamedXml)renamedXml->setAttribute("name","Renamed UX Test");
        check(renamePreset(5,"Renamed UX Test").wasOk()&&rigStore.readName(5)=="Renamed UX Test"&&XmlDocument::parse(rigStore.slotFile(5))->isEquivalentTo(renamedXml.get(),false)&&presetEntry.getButtonText().contains("Renamed UX Test"),"rename updates XML, index and current label with every setting intact");
        check(renamePreset(5,"   ").failed()&&rigStore.readName(5)=="Renamed UX Test","blank name is rejected without changing preset");
        check(deletePreset(5).wasOk()&&!rigStore.contains(5)&&!rigStore.nameFile(5).exists()&&presetEntry.getButtonText().contains("(empty)")&&ampSource.get()==beforePreset&&tonegrid::output::encode(outputSettings).toString()==beforeSettings,"delete empties only selected slot and preserves running sound/settings");
        file.getSiblingFile(tonegrid::t3k::textField(tonegrid::t3k::localManifest(file),"image")).deleteFile();populateNam();check(thumbnail.usesFallback()&&thumbnail.hasTone3000Mark(),"missing downloaded thumbnail uses generic amp fallback and retains source mark offline");
        File opened;namBrowser.openDirectory=[&](const File& folder){opened=folder;return true;};namBrowser.openFolder.onClick();check(opened==namLibrary.folder(),"Open Folder retains injected root");
        auto* before=ampSource.get();namBrowser.refresh.onClick();awaitNamRefreshForTest();check(ampSource.get()==before&&namBrowser.find(relative),"Refresh retains manual selection and source");
        const auto repeated=manager.import(client,pack,stopped);tone3000ImportFinished(repeated);
        const auto repeatedFolder=repeated.folder.getRelativePathFrom(namLibrary.folder()).replaceCharacter('\\','/');
        const auto repeatedRelative=namBrowser.firstModelIn(repeatedFolder);
        check(repeated.ok&&repeated.folder!=imported.folder&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model.file==repeatedRelative&&repeatedRelative.startsWith(repeatedFolder+"/"),"repeated download targets the new collision-safe pack, never older folder");
        before=ampSource.get();tonegrid::t3k::ImportResult failed;failed.message="Cancelled test";tone3000ImportFinished(failed);
        check(ampSource.get()==before&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model.file==repeatedRelative,"failed or cancelled import keeps active NAM");
        check(WebBrowserComponent::areOptionsSupported(Tone3000BrowserWindow::browserOptions(bankDir.getParentDirectory().getChildFile("Tone3000/WebView2"))),"installed WebView2 runtime supported without IE fallback");
        tone3000OfflineTest=true;namBrowser.captures.onClick();
        check(!namBrowserDialog&&tone3000Window&&!tone3000Window->isCurrentlyBlockedByAnotherModalComponent(),"CTA closes the library modal before opening an accessible Tone3000 window");
        check(tone3000Window&&tone3000Window->authorizationStarted()&&tone3000Window->isVisible()&&ampSource.get()==before,"one CTA click starts authorization directly without introduction or audio changes");
        check(tone3000Tests::parameter(tone3000TestAuthorization,"client_id")=="t3k_pub_zbn539j1yH1giZnQWE0VhLuc9izCIPxp","actual browser navigation uses the newly confirmed public key");
        if(tone3000Window)
        {
            check(tone3000Window->verifyErrorActions(),"authentication error offers return and Retry creates a fresh secure session");
            tone3000Window->closeButtonPressed();
        }
        check(tone3000Window&&!tone3000Window->isVisible()&&!tone3000Window->busy()&&namBrowserDialog&&namBrowserDialog->isCurrentlyModal(),"cancel returns to the local library without an incomplete import");
        check(ampSource.get()==before&&namBrowser.find(relative)&&namCatalog.entries.size()==45,"return preserves active amp and complete local catalogue");
        namBrowser.captures.onClick();check(!namBrowserDialog&&tone3000Window&&tone3000Window->authorizationStarted()&&tone3000Window->isVisible(),"reopening Tone3000 replaces the library and starts authorization again");
        if(tone3000Window)tone3000Window->closeButtonPressed();
        prepareNamBrowserSnapshot();
        result+=String(checks)+" Tone3000 host checks\n";return ok;
    }
    bool verifyImportedViewport(String& result)
    {
        const auto* source=dynamic_cast<amp::NamAmp*>(ampSource.get());auto* leaf=source?namBrowser.find(source->state().model.file):nullptr;
        const auto row=source?namBrowser.rowFor(source->state().model.file):-1;const auto bounds=namBrowser.list.getRowPosition(row,true);const auto view=namBrowser.list.getLocalBounds();
        const bool visible=leaf&&leaf->isSelected()&&bounds.getHeight()>0&&bounds.getY()>=view.getY()&&bounds.getBottom()<=view.getBottom();
        result+=visible?"PASS Tone3000 host latest pack is scrolled into a long library viewport\n":"FAIL Tone3000 host latest pack viewport position\n";return visible;
    }
    void prepareUx112Preview()
    {
        for(int i=0;i<8;++i){slot.setSelectedId(i+1,dontSendNotification);name.setText(i==0?"Mayer Clean - NAM":i==1?"Pink Floyd - Comfortably Numb Solo":"User preset "+String(i+1),false);saveSlot();}
        outputSettings.spreadEnabled=true;outputSettings.mod.enabled=true;outputSettings.delay.enabled=true;outputSettings.reverb.enabled=true;syncOutputUi();gateToggle.setToggleState(true,dontSendNotification);gateToggle.onClick();
        slot.setSelectedId(1,dontSendNotification);selectSlot();
    }
    bool tone3000NativeBrowserReady() const{return tone3000Window&&tone3000Window->nativeBrowserReady();}
    void takeScreenshot(const File& file) { auto stream=file.createOutputStream(); if(stream) { stream->setPosition(0); stream->truncate(); PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream); } }
    void takeAmpScreenshot(const File& file){auto stream=file.createOutputStream();if(stream){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(createComponentSnapshot(sections[0]),*stream);}}
    void prepareNamBrowserSnapshot()
    {
        showNamChooser();if(const auto* source=dynamic_cast<const amp::NamAmp*>(ampSource.get()))
        {String parent;auto parts=StringArray::fromTokens(source->state().model.file,"/",{});for(int i=0;i<parts.size()-1;++i){parent+=parent.isEmpty()?parts[i]:"/"+parts[i];if(auto* folder=namBrowser.find(parent))folder->setOpen(true);}}
    }
    void takeNamBrowserScreenshot(const File& file)
    {auto stream=file.createOutputStream();if(stream){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(namBrowser.createComponentSnapshot(namBrowser.getLocalBounds()),*stream);}}
    bool verifyNamWorkflow(String& result)
    {
        bool ok=true;int checks=0;auto check=[&](bool good,const String& text){++checks;ok&=good;result+=String(good?"PASS ":"FAIL ")+text+"\n";};
        check(!hardwareEnabled&&!audioReady&&!midi&&ready()&&ampSource->type()==amp::SourceType::Vst3,"NAM UI test starts with existing TONEX and hardware disabled in temporary app-data");
        check(!logVisible&&!log.isVisible()&&showLog.getButtonText()=="Show Log","Full log is closed on launch while status and Show Log remain visible");
        const auto statusHeight=status.getHeight();report("Synthetic compact log update");check(latestLog.getText().contains("Synthetic compact log update")&&latestLog.getRight()<=showLog.getX()&&latestLog.findColour(Label::textColourId).getFloatAlpha()>.49f&&latestLog.findColour(Label::textColourId).getFloatAlpha()<.51f,"Latest log line updates immediately as unboxed single-line text at 50 percent opacity before Show Log");
        showLog.onClick();check(log.isVisible()&&showLog.getButtonText()=="Hide Log"&&status.getHeight()==statusHeight,"Show Log reveals full history without increasing status-bar height");showLog.onClick();check(!log.isVisible()&&status.getHeight()==statusHeight,"Hide Log restores compact presentation with unchanged status height");
        if(!ready()||namCatalog.entries.empty())return false;exerciseUiCleanup(check);auto* engine=pedals;const auto vstCache=pluginCatalog.xml().toString();
        slot.setSelectedId(2,sendNotificationSync);name.setText("Synthetic VST3",false);saveSlot();
        check(rigStore.contains(1)&&XmlDocument::parse(rigStore.slotFile(1))->getIntAttribute("version")==12,"VST3 Save writes format 12 including expanded output DSP before source switch");
        namSourceButton.onClick();
        check(ampSource->type()==amp::SourceType::Nam&&pedals==engine,"Actual source dropdown switches TONEX to real local NAM without recreating pedals");
        if(ampSource->type()!=amp::SourceType::Nam)return false;
        const auto activeModel=dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model.file;
        check(namSourceButton.isVisible()&&vstSourceButton.isVisible()&&namSourceButton.getToggleState()&&!vstSourceButton.getToggleState(),"Two permanent source buttons show mutually exclusive explicit NAM source");
        check(namFilename.getText()==namamp::filename(activeModel)&&namFilename.getText().endsWithIgnoreCase(".nam")&&namFolderName.getText()==namamp::folderLabel(activeModel)&&namFilename.getTooltip()==activeModel,"NAM face shows exact actual filename, immediate folder and full relative-path tooltip");
        check(thumbnail.isVisible()&&thumbnail.usesFallback()&&namFolder.isVisible()&&!namChoice.isVisible()&&!refreshNam.isVisible(),"NAM face has generic amp thumbnail and folder button, without dropdown/arrows/ellipsis/Refresh clutter");
        check(normalize.getSelectedId()==3&&normalize.getTooltip().contains("Partial: original normalization")&&normalize.getTooltip().contains("Full: Partial plus ToneGrid"),"New NAM state has Full normalization selector and required tooltip");
        check(pluginCatalog.xml().toString()==vstCache,"NAM discovery/selection does not alter VST3 catalog/identity metadata");
        check(sections[0].contains(namChoice.getBounds())&&sections[0].contains(namGain.getBounds())&&sections[0].contains(namVolume.getBounds())&&!namGain.getBounds().intersects(namVolume.getBounds()),"NAM dropdown/Refresh/Gain/Volume fit downstream AMP section without overlap");
        check(namGain.getValue()==0&&namVolume.getValue()==0&&namGain.valueToProportionOfLength(0)==.5&&namVolume.valueToProportionOfLength(0)==.5,"NAM Gain and asymmetric Volume both show 0 dB exactly at visual center");
        auto reference=amp::loadNamAmp(namLibrary,dynamic_cast<amp::NamAmp*>(ampSource.get())->state(),namLite);
        if(!reference.source){check(false,"Independent real NAM reference loaded");return false;}
        rig::StereoGraph referenceGraph;auto referenceNode=reference.source->addToGraph(referenceGraph);auto* referenceProcessor=referenceNode->getProcessor();
        graph.setNonRealtime(true);graph.prepareToPlay(48000,128);referenceProcessor->prepareToPlay(48000,128);
        AudioBuffer<float> rendered(2,128),expected(2,128);MidiBuffer testMidi;
        auto fillTest=[&](int block){for(int i=0;i<128;++i){const auto x=.08f*(float)std::sin((block*128+i)*.07);rendered.setSample(0,i,x);rendered.setSample(1,i,-x);expected.setSample(0,i,x);expected.setSample(1,i,-x);}};
        const auto loudness=dynamic_cast<amp::NamAmp*>(ampSource.get())->loudnessDb();
        const auto adjustment=loudness?jlimit(-12.f,12.f,-18.f-*loudness):0.f;SmoothedValue<float> gain;gain.reset(48000,.025);gain.setCurrentAndTargetValue(1);gain.setTargetValue(Decibels::decibelsToGain(adjustment));
        bool matched=true;for(int block=0;block<20;++block){fillTest(block);referenceProcessor->processBlock(expected,testMidi);graph.processBlock(rendered,testMidi);for(int i=0;i<128;++i){const auto correction=gain.getNextValue();for(int channel=0;channel<2;++channel)matched&=std::abs(rendered.getSample(channel,i)-expected.getSample(channel,i)*correction)<1e-6;}}
        check(matched,"Actual host graph with real NAM matches independent inference followed by static metadata gain; Spread bypass is transparent");
        takeScreenshot(File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("nam-ui-neutral.png"));
        takeAmpScreenshot(File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("nam-amp-neutral.png"));
        bool afterPedals=true;for(const auto& card:cards)afterPedals&=card&&card->getBottom()<=sections[0].getY();
        check(afterPedals&&sections[0].getBottom()<status.getY(),"AMP and Spread panels lie after both pedal rows and before Output; MOD/Delay/Reverb follow Spread");
        check(normalize.isVisible()&&(normalize.getSelectedId()!=1)&&sections[0].contains(normalize.getBounds())&&!normalize.getBounds().intersects(namGain.getBounds()),"NAM Normalize defaults ON and fits its section without knob overlap");
        check(spreadToggle.isVisible()&&spreadGear.isVisible()&&spreadControls.getParentComponent()==nullptr,"3D Sound toggle/gear are on the amp face; advanced controls are hidden");spreadGear.onClick();
        check(spreadDialog&&spreadControls.getParentComponent()!=nullptr,"3D Sound gear opens the advanced settings popup");
        spreadToggle.setToggleState(true,dontSendNotification);spreadToggle.onClick();check(outputSettings.spreadEnabled&&spreadControls.state().spreadEnabled,"Main 3D Sound toggle controls existing Spread state and synchronizes popup");
        auto* spreadOffset=dynamic_cast<Slider*>(spreadControls.findChildWithID("spread-offset"));auto* spreadWobble=dynamic_cast<Slider*>(spreadControls.findChildWithID("spread-wobble"));auto* spreadCrossover=dynamic_cast<Slider*>(spreadControls.findChildWithID("spread-crossover"));auto* spreadEnable=dynamic_cast<Button*>(spreadControls.findChildWithID("spread-enable"));
        if(!spreadOffset||!spreadWobble||!spreadCrossover||!spreadEnable){check(false,"Spread UI controls are accessible");return false;}
        spreadEnable->setToggleState(true,dontSendNotification);spreadEnable->onClick();spreadOffset->setValue(-9.5,sendNotificationSync);spreadWobble->setValue(37,sendNotificationSync);spreadCrossover->setValue(260,sendNotificationSync);
        {auto stream=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("3d-sound-settings.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(spreadControls.createComponentSnapshot(spreadControls.getLocalBounds()),*stream);}}
        check(outputSettings.spreadEnabled&&outputSettings.offsetMs==-9.5f&&outputSettings.wobblePercent==37&&outputSettings.crossoverHz==260,"Actual Spread controls publish enabled/offset/wobble/crossover to DSP state");
        normalize.setSelectedId(2,sendNotificationSync);check(outputSettings.ampNormalizationEnabled&&!outputSettings.ampNormalizationEnhanced,"Actual dropdown selects Partial independently from Spread");normalize.setSelectedId(1,sendNotificationSync);check(!outputSettings.ampNormalizationEnabled,"Actual Normalize toggle publishes bypass independently from Spread");normalize.setSelectedId(3,sendNotificationSync);
        double stereoDifference=0;for(int block=20;block<50;++block){fillTest(block);graph.processBlock(rendered,testMidi);for(int i=0;i<128;++i)stereoDifference+=std::abs(rendered.getSample(0,i)-rendered.getSample(1,i));}
        check(stereoDifference>1e-6,"Actual enabled Spread generates stereo after real local NAM through the host graph");graph.releaseResources();
        exerciseReverbUi(check);exerciseEffectsUi(check);
        check(sections[2].contains(guitarIn.getBounds())&&sections[3].contains(gateThreshold.getBounds()),"Header input/gate knobs and dB readouts retain bottom padding inside their sections");
        namGain.setValue(5,sendNotificationSync);namVolume.setValue(-8,sendNotificationSync);guitarIn.setValue(2,sendNotificationSync);
        auto setting=pedals->get(0);setting.enabled=true;pedals->set(0,setting);
        check(dynamic_cast<amp::NamAmp*>(ampSource.get())->state().gainDb==5&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().volumeDb==-8,"Actual NAM knobs update processor atomics without GUI dependence");
        slot.setSelectedId(1,sendNotificationSync);name.setText("Synthetic NAM AMP+CAB",false);saveSlot();
        board::State saved;PluginDescription required;
        check(rigStore.loadForSelection(0,rigIdentities(),saved,required).wasOk()&&saved.sourceType==amp::SourceType::Nam&&saved.namAmp.gainDb==5&&saved.namAmp.volumeDb==-8,"Actual Save handler writes NAM rig v12 with identity/controls/pedals/input");
        check(saved.outputDsp&&saved.outputDsp->ampNormalizationEnabled&&saved.outputDsp->spreadEnabled&&saved.outputDsp->offsetMs==-9.5f&&saved.outputDsp->wobblePercent==37&&saved.outputDsp->wobbleEnabled&&saved.outputDsp->diffuseEnabled,"Actual Save retains independent normalization and all Spread controls");
        check(saved.outputDsp&&saved.outputDsp->reverb.enabled&&saved.outputDsp->reverb.type==tonegrid::reverb::Type::Plate&&saved.outputDsp->reverb.timeSeconds==2.7f&&saved.outputDsp->reverb.preDelayMs==24.5f&&saved.outputDsp->reverb.color==63&&saved.outputDsp->reverb.mixPercent==21,"Actual NAM Save retains all six reverb controls");
        check(saved.outputDsp&&saved.outputDsp->expandedEffects&&saved.outputDsp->pitch.dry==74&&saved.outputDsp->pitch.semitones==-7&&saved.outputDsp->mod.type==tonegrid::mod::Type::Flanger&&saved.outputDsp->mod.rateHz==3.2f&&saved.outputDsp->delay.timeMs==600&&saved.outputDsp->delay.feedback==71,"Actual Save retains every new effect in rig v12");
        vstSourceButton.onClick();
        check(ampSource->type()==amp::SourceType::Vst3&&pedals==engine,"Actual source dropdown switches NAM back to existing VST3 host");
        check(!normalize.isVisible()&&!namGain.isVisible()&&!namVolume.isVisible()&&!namSourceButton.getToggleState()&&vstSourceButton.getToggleState(),"VST mode hides NAM-only controls and preserves permanent source buttons");
        slot.setSelectedId(2,sendNotificationSync);guitarIn.setValue(-3,sendNotificationSync);setting.enabled=false;pedals->set(0,setting);
        slot.setSelectedId(1,sendNotificationSync);
        check(ampSource->type()==amp::SourceType::Nam&&namGain.getValue()==5&&namVolume.getValue()==-8&&pedals->front.get().gainDb==2&&pedals->get(0).enabled,"Actual preset dropdown restores NAM source, knobs and complete unrelated rig state");
        check(outputSettings.spreadEnabled&&outputSettings.offsetMs==-9.5f&&outputSettings.crossoverHz==260&&spreadControls.state().wobblePercent==37,"Preset selection recalls post-amp DSP and synchronizes controls");
        check(outputSettings.reverb.enabled&&reverbToggle.getToggleState()&&reverbControls.state(true).timeSeconds==2.7f,"NAM preset recall restores Reverb and popup controls");
        check(effectsStateMatches()&&pitchControls.pitchSettings().up==22&&modControls.modSettings().level==44&&delayControls.delaySettings().mix==29,"Actual preset recall restores every expanded effect and popup control");
        const auto file=namLibrary.folder().getChildFile(saved.namAmp.model.file),held=file.getSiblingFile(file.getFileName()+".temporarily-missing");
        slot.setSelectedId(2,sendNotificationSync);const auto* previous=ampStage->getProcessor();const auto before=pedals->front.get();const auto rigBefore=rigStore.slotFile(0).loadFileAsString();
        check(file.moveFileTo(held),"Hide NAM only within temporary library for missing-file test");recallMidiRig(0);
        check(ampStage->getProcessor()==previous&&ampSource->type()==amp::SourceType::Vst3&&pedals->front.get().gainDb==before.gainDb&&rigStore.slotFile(0).loadFileAsString()==rigBefore,"Missing NAM recall through same MIDI rig command keeps previous VST3/input and recoverable file");
        held.moveFileTo(file);refreshNam.onClick();recallMidiRig(0);
        check(ampSource->type()==amp::SourceType::Nam&&namGain.getValue()==5&&namVolume.getValue()==-8,"NAM Refresh detects restored file and existing MIDI rig command recalls it");
        const auto edges=graph.getConnections();bool ordered=true;
        for(int channel=0;channel<2;++channel)
        {
            bool ampToNorm=false,normToSpread=false,spreadToMod=false,modToDelay=false,delayToReverb=false,reverbToOut=false;
            for(const auto& edge:edges){auto* from=graph.getNodeForId(edge.source.nodeID)->getProcessor();auto* to=graph.getNodeForId(edge.destination.nodeID)->getProcessor();if(edge.source.channelIndex!=channel||edge.destination.channelIndex!=channel)continue;ampToNorm|=from==ampStage->getProcessor()&&to==normalizationNode;normToSpread|=from==normalizationNode&&to==spreadNode;spreadToMod|=from==spreadNode&&to==modNode;modToDelay|=from==modNode&&to==delayNode;delayToReverb|=from==delayNode&&to==reverbNode;reverbToOut|=from==reverbNode&&edge.destination.nodeID==outputStageId;}
            ordered&=ampToNorm&&normToSpread&&spreadToMod&&modToDelay&&delayToReverb&&reverbToOut;
        }
        check(ordered,"Actual host source switches retain Amp -> Normalize -> Spread -> MOD -> Delay -> Reverb -> Output on both channels");
        auto legacy=saved;legacy.outputDsp.reset();check(rigStore.save(2,legacy,{},rigIdentities()).result.wasOk(),"Synthetic legacy NAM v8 rig saved for compatibility recall");recallSlot(2);
        check(!outputSettings.ampNormalizationEnabled&&!outputSettings.spreadEnabled&&outputSettings.pitch.mode==tonegrid::pitch::Mode::Off&&!outputSettings.mod.enabled&&!outputSettings.delay.enabled&&!outputSettings.reverb.enabled&&!reverbToggle.getToggleState()&&!(normalize.getSelectedId()!=1)&&!spreadControls.state().spreadEnabled,"Actual legacy v8 recall bypasses new DSPs including Reverb and updates their controls");recallSlot(0);
        if(auto preference=XmlDocument::parse(sourcePreference))check(preference->getStringAttribute("type")=="NAM","Startup preference persists explicit NAM source type, not dropdown index");else check(false,"NAM startup preference exists");
        auto missingPedal=XmlDocument::parse(rigStore.slotFile(0));auto* retired=missingPedal->getChildByAttribute("id","10");retired->setAttribute("preset",3);retired->setAttribute("enabled",true);retired->setAttribute("model","retired-blues-original-identity");check(rig::writeXml(*missingPedal,rigStore.slotFile(3)),"Missing-pedal host rig seeded only in temporary bank");recallSlot(3);
        check(ready()&&ampSource->type()==amp::SourceType::Nam&&!pedals->get(10).enabled&&pedals->get(10).preset==3&&pedals->get(0).enabled&&namGain.getValue()==5&&namVolume.getValue()==-8&&latestLog.getText().contains("Missing capture for BLUES OD"),"Actual rig recall bypasses retired Blues only, preserves amp and other pedals, emits non-blocking diagnostic without capture substitution");recallSlot(0);
        normalize.setSelectedId(1,sendNotificationSync);name.setText("Stored normalization OFF",false);slot.setSelectedId(3,dontSendNotification);saveSlot();normalize.setSelectedId(3,sendNotificationSync);recallSlot(2);check(!outputSettings.ampNormalizationEnabled&&normalize.getSelectedId()==1,"Existing rig normalization OFF remains OFF after recall with new-session default Full");recallSlot(0);
        normalize.setSelectedId(2,sendNotificationSync);name.setText("Stored Partial",false);slot.setSelectedId(4,dontSendNotification);saveSlot();normalize.setSelectedId(3,sendNotificationSync);recallSlot(3);
        check(outputSettings.ampNormalizationEnabled&&!outputSettings.ampNormalizationEnhanced&&normalize.getSelectedId()==2,"User preset saves and recalls Partial rather than Full");recallSlot(0);
        if(spreadDialog)delete spreadDialog.getComponent();takeScreenshot(File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("nam-amp-reference.png"));
        checkCompactLayout(check);
        const auto activeBefore=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();auto* sourceBefore=ampSource.get();namFolder.onClick();
        check(namBrowserDialog&&namBrowser.totalCaptures()>0,"Folder button opens the real hierarchical NAM browser dialog");
        const auto newFolder=namLibrary.folder().getChildFile("Tree Test/Deep");newFolder.createDirectory();const auto duplicate=newFolder.getChildFile(namamp::filename(activeBefore.model.file));
        check(namLibrary.folder().getChildFile(activeBefore.model.file).copyFileTo(duplicate),"Browser duplicate-capture fixture created only inside temporary NAM library");
        namBrowser.refresh.onClick();awaitNamRefreshForTest();check(ampSource.get()==sourceBefore&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model==activeBefore.model&&namBrowser.find("Tree Test/Deep"),"Browser Refresh rescans new nested folders while keeping loaded NAM and exact identity unchanged");
        auto* leaf=namBrowser.find("Tree Test/Deep/"+duplicate.getFileName());check(leaf&&!leaf->folder&&leaf->label==namamp::filename(activeBefore.model.file),"Browser displays duplicate filename as a separate nested selectable leaf");
        if(leaf){namBrowser.find("Tree Test")->setOpen(true);namBrowser.find("Tree Test/Deep")->setOpen(true);leaf->setSelected(true,true);namBrowser.handleUpdateNowIfNeeded();}
        check(dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model.file=="Tree Test/Deep/"+duplicate.getFileName()&&namFolderName.getText()=="Deep"&&namFilename.getText()==duplicate.getFileName(),"Actual tree selection reaches unchanged NAM loader using full relative identity and updates amp face");
        // Regression stress: the real selection callback loads/commits NAM and
        // refreshes its tree while JUCE is STILL capturing the pressed row.
        // Windowless OS services keep this deterministic without moving the
        // user's pointer; the actual TreeView, callback, loader and graph run.
        {
            delete namBrowserDialog.getComponent();
            namBrowserLifetimeTests::MouseHost inputHost;inputHost.addAndMakeVisible(namBrowser);namBrowser.setBounds(inputHost.getLocalBounds());
            auto& mouse=inputHost.mouse();ScopeGuard releaseMouse{[&]{mouse.release();}};
            constexpr int loads=100;bool captured=true,retired=true,loaded=true,renderSafe=true;int completed=0;
            const auto graphSize=graph.getNumNodes();
            const auto nested="Tree Test/Deep/"+duplicate.getFileName();
            graph.setNonRealtime(true);graph.prepareToPlay(48000,128);
            AudioBuffer<float> stressAudio(2,128);MidiBuffer stressMidi;
            for(int iteration=0;iteration<loads;++iteration)
            {
                namBrowser.find("Tree Test")->setOpen(true);namBrowser.find("Tree Test/Deep")->setOpen(true);
                const auto relative=iteration%2?nested:activeBefore.model.file;
                Component::SafePointer<Component> row;
                if(!mouse.press(namBrowser,relative,row)){captured=false;mouse.release();break;}
                const auto* previous=ampSource.get();namBrowser.handleUpdateNowIfNeeded();
                retired&=!row||row->getComponentID()=="nam-row:"+relative;
                const auto* current=dynamic_cast<const amp::NamAmp*>(ampSource.get());
                loaded&=current&&ampSource.get()!=previous&&current->state().model.file==relative&&namFilename.getText()==duplicate.getFileName()&&graph.getNumNodes()==graphSize;
                mouse.release();
                double power=0;for(int block=0;block<4;++block)
                {
                    for(int sample=0;sample<128;++sample)for(int channel=0;channel<2;++channel)
                        stressAudio.setSample(channel,sample,.04f*(float)std::sin((block*128+sample)*.07));
                    graph.processBlock(stressAudio,stressMidi);
                    for(int channel=0;channel<2;++channel)for(int sample=0;sample<128;++sample)
                        {const auto value=stressAudio.getSample(channel,sample);renderSafe&=std::isfinite(value);power+=(double)value*value;}
                }
                renderSafe&=power>1.e-12;++completed;
                retiredAmps.collect();
                if(iteration%10==0)namBrowser.refresh.onClick();awaitNamRefreshForTest();
                if(!retired||!loaded)break;
            }
            graph.releaseResources();
            check(captured&&completed==loads,"All 100 actual NAM selections use JUCE mouse-held capture during model commit");
            check(retired&&completed==loads,"All 100 actual NAM commits retain a safe value-owned row or release it");
            check(loaded&&completed==loads,"All 100 held tree selections replace real NAM, preserve graph size and synchronize full relative identity/amp UI");
            check(renderSafe&&completed==loads,"400 audio blocks remain finite and audible through NAM replacement, repaint/Refresh and previous-model retirement");
            const auto* previous=ampSource.get();duplicate.deleteFile();
            // Keep the old catalog long enough to exercise the failed-load path.
            namBrowser.find(activeBefore.model.file)->activate();namBrowser.handleUpdateNowIfNeeded();
            const auto* fallback=ampSource.get();namBrowser.find("Tree Test")->setOpen(true);namBrowser.find("Tree Test/Deep")->setOpen(true);
            Component::SafePointer<Component> failedRow;const bool held=mouse.press(namBrowser,nested,failedRow);namBrowser.handleUpdateNowIfNeeded();mouse.release();
            check(held&&(!failedRow||failedRow->getComponentID()=="nam-row:"+nested)&&ampSource.get()==fallback&&ampSource.get()!=previous&&latestLog.getText().contains("Previous amp kept"),"Missing-file selection while held keeps previous real NAM and safely resynchronizes tree");
            // Restore synthetic duplicate for the existing deletion/Refresh check.
            namLibrary.folder().getChildFile(activeBefore.model.file).copyFileTo(duplicate);namBrowser.refresh.onClick();awaitNamRefreshForTest();
            namBrowser.find(nested)->activate();namBrowser.handleUpdateNowIfNeeded();
        }
        namFolder.onClick();
        File opened;namBrowser.openDirectory=[&](const File& folder){opened=folder;return true;};namBrowser.openFolder.onClick();check(opened==namLibrary.folder(),"Actual browser Open Folder callback targets injected root without opening Explorer during tests");namBrowser.openDirectory=tonegrid::openNamLibraryFolder;
        sourceBefore=ampSource.get();duplicate.deleteFile();namBrowser.refresh.onClick();awaitNamRefreshForTest();check(ampSource.get()==sourceBefore&&!namBrowser.find("Tree Test/Deep/"+duplicate.getFileName())&&namBrowser.selected().isEmpty(),"Refresh after deleting selected temporary file removes its tree leaf without reloading or substituting the active amp");
        delete namBrowserDialog.getComponent();recallSlot(0);
        setSize(1360,820);check(sections[0].contains(namChoice.getBounds())&&sections[0].contains(namGain.getBounds())&&sections[0].contains(namVolume.getBounds())&&sections[0].contains(normalize.getBounds()),"NAM controls also fit minimum window width");checkCompactLayout(check);
        result+=String(checks)+" NAM UI workflow checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
    bool verifyNamRestart(String& result)
    {
        const auto* source=dynamic_cast<const amp::NamAmp*>(ampSource.get());
        const bool ok=ready()&&source&&source->state().gainDb==5&&source->state().volumeDb==-8&&pedals->front.get().gainDb==2&&pedals->get(0).enabled&&!audioReady&&!midi&&displayedSource==amp::SourceType::Nam&&outputSettings.ampNormalizationEnabled&&outputSettings.spreadEnabled&&outputSettings.offsetMs==-9.5f&&outputSettings.crossoverHz==260&&spreadControls.state().wobblePercent==37;
        result+=ok?"PASS Fresh host restores NAM preference and complete saved rig after application-host recreation\n":"FAIL NAM host restart restoration\n";
        const bool reverbOk=outputSettings.reverb.enabled&&outputSettings.reverb.type==tonegrid::reverb::Type::Plate&&outputSettings.reverb.timeSeconds==2.7f&&outputSettings.reverb.preDelayMs==24.5f&&outputSettings.reverb.color==63&&outputSettings.reverb.mixPercent==21&&reverbToggle.getToggleState()&&reverbControls.state(true).timeSeconds==2.7f&&reverbControls.findChildWithID("reverb-candidate")==nullptr;
        result+=reverbOk?"PASS Fresh host restores six Reverb controls; development engine choice is not persisted\n":"FAIL Reverb host restart restoration\n";
        const auto flag=[](bool value){return String(value?1:0);};
        if(!ok)result+="RESTART_DIAGNOSTIC ready="+flag(ready())+" NAM="+flag(source!=nullptr)+" gain="+String(source?source->state().gainDb:999)+" volume="+String(source?source->state().volumeDb:999)+" input="+String(pedals?pedals->front.get().gainDb:999)+" comp="+flag(pedals&&pedals->get(0).enabled)+" hardware="+flag(audioReady||midi!=nullptr)+" displayedNAM="+flag(displayedSource==amp::SourceType::Nam)+" normalize="+flag(outputSettings.ampNormalizationEnabled)+" spread="+flag(outputSettings.spreadEnabled)+" offset="+String(outputSettings.offsetMs)+" crossover="+String(outputSettings.crossoverHz)+" wobble="+String(spreadControls.state().wobblePercent)+" log="+latestLog.getText()+"\n";
        const bool effectsOk=effectsStateMatches()&&pitchMode.getSelectedId()==3&&modToggle.getToggleState()&&delayToggle.getToggleState();
        result+=effectsOk?"PASS Fresh host restores every Pitch/MOD/Delay field and main controls\n":"FAIL Expanded effects host restart restoration\n";
        return ok&&reverbOk&&effectsOk;
    }
    bool effectsStateMatches() const
    {
        const auto& s=outputSettings;
        return s.expandedEffects&&s.pitch.mode==tonegrid::pitch::Mode::Octaver&&s.pitch.semitones==-7&&s.pitch.dry==74&&s.pitch.down==41&&s.pitch.up==22
            &&s.mod.enabled&&s.mod.type==tonegrid::mod::Type::Flanger&&s.mod.rateHz==3.2f&&s.mod.depth==63&&s.mod.level==44
            &&s.delay.enabled&&s.delay.timeMs==600&&s.delay.feedback==71&&s.delay.mix==29;
    }
    template<class Check>void exerciseEffectsUi(Check&& check)
    {
        check(sections[6].contains(pitchMode.getBounds())&&sections[6].contains(pitchGear.getBounds())&&sections[3].getRight()<sections[6].getX()&&sections[6].getRight()<sections[4].getX(),"PITCH dropdown/gear lie between Gate and Audio in existing header");
        pitchMode.setSelectedId(2,sendNotificationSync);pitchGear.onClick();
        check(pitchDialog&&pitchControls.getParentComponent()!=nullptr,"Pitch gear opens non-owned mode-specific two-column popup");
        dynamic_cast<Slider*>(pitchControls.findChildWithID("semitones"))->setValue(-7,sendNotificationSync);
        pitchMode.setSelectedId(3,sendNotificationSync);
        for(auto v:{std::pair<const char*,double>{"dry",74},{"down",41},{"up",22}})dynamic_cast<Slider*>(pitchControls.findChildWithID(v.first))->setValue(v.second,sendNotificationSync);
        modGear.onClick();modToggle.setToggleState(true,dontSendNotification);modToggle.onClick();
        dynamic_cast<ComboBox*>(modControls.findChildWithID("mod-type"))->setSelectedId(4,sendNotificationSync);
        for(auto v:{std::pair<const char*,double>{"rate",3.2},{"depth",63},{"level",44}})dynamic_cast<Slider*>(modControls.findChildWithID(v.first))->setValue(v.second,sendNotificationSync);
        delayGear.onClick();delayToggle.setToggleState(true,dontSendNotification);delayToggle.onClick();
        for(auto v:{std::pair<const char*,double>{"time",789.5},{"feedback",71},{"mix",29}})dynamic_cast<Slider*>(delayControls.findChildWithID(v.first))->setValue(v.second,sendNotificationSync);
        delayControls.applyTap(0);delayControls.applyTap(600);
        check(modDialog&&delayDialog&&effectsStateMatches(),"Actual mode/type/knobs/toggles/TAP publish every new effect field");
        spreadControls.onChange();check(effectsStateMatches(),"Spread changes preserve independent Pitch/MOD/Delay controls");
        graph.setNonRealtime(true);graph.prepareToPlay(48000,96);AudioBuffer<float> b(2,96);MidiBuffer m;bool finite=true;double energy=0;
        for(int k=0;k<600;++k){for(int c=0;c<2;++c)for(int i=0;i<96;++i)b.setSample(c,i,.06f*float(std::sin((k*96+i)*.071)));graph.processBlock(b,m);for(int c=0;c<2;++c)for(int i=0;i<96;++i){auto x=b.getSample(c,i);finite&=std::isfinite(x);energy+=x*x;}}
        graph.releaseResources();check(finite&&energy>1e-5,"Real amp graph with Octaver/MOD/Delay/Reverb enabled renders 600 finite audible blocks");
        delete pitchDialog.getComponent();delete modDialog.getComponent();delete delayDialog.getComponent();
        check(pitchControls.getParentComponent()==nullptr&&modControls.getParentComponent()==nullptr&&delayControls.getParentComponent()==nullptr,"All three non-owned new popups close safely and preserve reusable controls");
    }
    template<class Check>void exerciseReverbUi(Check&& check)
    {
        using tonegrid::reverb::Type;
        check(reverbToggle.isVisible()&&reverbGear.isVisible()&&reverbGear.getBounds()==tonegrid::ampui::settingsButtonBounds(sections[0],3)&&sections[0].contains(reverbToggle.getBounds()),"Reverb compact toggle/gear fit unchanged AMP face");
        reverbGear.onClick();check(reverbDialog&&reverbControls.getParentComponent()!=nullptr,"Reverb gear opens dedicated two-column settings popup");
        auto* type=dynamic_cast<ComboBox*>(reverbControls.findChildWithID("reverb-type"));auto* candidate=dynamic_cast<ComboBox*>(reverbControls.findChildWithID("reverb-candidate"));
        auto* time=dynamic_cast<Slider*>(reverbControls.findChildWithID("reverb-time"));auto* delay=dynamic_cast<Slider*>(reverbControls.findChildWithID("reverb-predelay"));auto* color=dynamic_cast<Slider*>(reverbControls.findChildWithID("reverb-color"));auto* mix=dynamic_cast<Slider*>(reverbControls.findChildWithID("reverb-mix"));
        if(!type||candidate||!time||!delay||!color||!mix){check(false,"Reverb popup controls accessible");return;}
        type->setSelectedId(2,sendNotificationSync);time->setValue(2.7,sendNotificationSync);delay->setValue(24.5,sendNotificationSync);color->setValue(63,sendNotificationSync);mix->setValue(21,sendNotificationSync);
        reverbToggle.setToggleState(true,dontSendNotification);reverbToggle.onClick();
        check(outputSettings.reverb.enabled&&outputSettings.reverb.type==Type::Plate&&outputSettings.reverb.timeSeconds==2.7f&&outputSettings.reverb.preDelayMs==24.5f&&outputSettings.reverb.color==63&&outputSettings.reverb.mixPercent==21,"Actual four knobs/Type/toggle publish all musical Reverb controls");
        check(candidate==nullptr,"Production Reverb popup has no development A/B selector");
        const auto reverb=outputSettings.reverb;spreadControls.onChange();check(outputSettings.reverb.enabled==reverb.enabled&&outputSettings.reverb.timeSeconds==reverb.timeSeconds,"Spread popup changes preserve independent Reverb state and candidate");
        reverbToggle.setToggleState(false,dontSendNotification);reverbToggle.onClick();check(!outputSettings.reverb.enabled&&outputSettings.reverb.timeSeconds==2.7f,"Main Reverb bypass preserves settings without reconstructing engine");reverbToggle.setToggleState(true,dontSendNotification);reverbToggle.onClick();
        graph.setNonRealtime(true);graph.prepareToPlay(48000,96);AudioBuffer<float> b(2,96);MidiBuffer midi;bool finite=true;
        for(int block=0;block<100;++block){for(int i=0;i<96;++i){const auto x=.06f*(float)std::sin((block*96+i)*.07);b.setSample(0,i,x);b.setSample(1,i,x);}graph.processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<96;++i)finite&=std::isfinite(b.getSample(c,i));}
        check(finite,"Real source graph with Reverb enabled renders 100 finite blocks");
        type->setSelectedId(4,sendNotificationSync);check(outputSettings.reverb.type==Type::Spring&&candidate==nullptr&&outputSettings.reverb.timeSeconds==2.7f,"Actual Spring popup selection publishes independent core without resetting knobs");
        finite=true;for(int block=0;block<100;++block){for(int i=0;i<96;++i){const auto x=.06f*(float)std::sin((block*96+i)*.07);b.setSample(0,i,x);b.setSample(1,i,x);}graph.processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<96;++i)finite&=std::isfinite(b.getSample(c,i));}
        check(finite,"Real amp source graph with Spring selected renders 100 finite blocks");
        auto streamSpring=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("spring-settings.png").createOutputStream();if(streamSpring){streamSpring->setPosition(0);streamSpring->truncate();PNGImageFormat().writeImageToStream(reverbControls.createComponentSnapshot(reverbControls.getLocalBounds()),*streamSpring);}
        type->setSelectedId(2,sendNotificationSync);check(outputSettings.reverb.type==Type::Plate&&candidate==nullptr,"Actual Spring-to-Plate selection restores previous development engine and all controls");graph.releaseResources();
        auto stream=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("reverb-settings.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(reverbControls.createComponentSnapshot(reverbControls.getLocalBounds()),*stream);}
        delete reverbDialog.getComponent();check(reverbControls.getParentComponent()==nullptr,"Non-owned Reverb popup closes safely, reusable controls stay alive");
    }
    template<class Check>void exerciseUiCleanup(Check&& check)
    {
        const int originalWidth=getWidth(),originalHeight=getHeight();
        for(int width:{1360,1660,2240})
        {
            setSize(width,originalHeight);const auto metrics=tonegrid::ui::toolbarMetrics(width-28-tonegrid::ui::logoWidth-tonegrid::ui::logoGap);bool fits=true;for(int i=1;i<6;++i)fits&=getLocalBounds().contains(sections[(size_t)i]);
            check(fits&&slot.getWidth()<=tonegrid::ui::presetComboMax&&midiDevice.getWidth()<=tonegrid::ui::midiComboMax&&save.getWidth()==metrics.save&&learn.getWidth()==metrics.learn&&rescan.getWidth()==metrics.refresh,"Actual toolbar stays bounded with compact widths at "+String(width));
            check(slot.getRight()<save.getX()&&save.getRight()<learn.getX()&&sections[1].contains(learn.getBounds())&&learn.isVisible(),"Actual Preset | Save | MIDI Learn order preserved at "+String(width));
            check(sections[1].getY()==14&&sections[1].getHeight()==tonegrid::ui::headerHeight&&sections[1].getX()>=14+tonegrid::ui::logoWidth&&sections[2].contains(guitarIn.getBounds())&&sections[2].contains(meter.getBounds())&&sections[3].contains(gateToggle.getBounds())&&sections[3].contains(gateThreshold.getBounds()),"All header controls fit alongside the logo in the single recovered-height band at "+String(width));
            check(sections[4].contains(audio.getBounds())&&sections[5].contains(midiDevice.getBounds())&&sections[5].contains(rescan.getBounds()),"Actual Audio/MIDI actions stay within compact groups at "+String(width));
            if(width==2240)takeScreenshot(File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("ui-cleanup-wide.png"));
        }
        setSize(originalWidth,originalHeight);
        check(bool(save.onClick)&&bool(learn.onClick)&&bool(midiDevice.onChange)&&bool(rescan.onClick)&&bool(audio.onClick),"Top Save/MIDI Learn/input/Refresh/Audio original callbacks remain connected");
        const auto input=pedals->front.get();guitarIn.setValue(3.5,sendNotificationSync);gateThreshold.setValue(-55,sendNotificationSync);gateToggle.setToggleState(!input.gate,dontSendNotification);gateToggle.onClick();
        check(pedals->front.get().gainDb==3.5f&&pedals->front.get().thresholdDb==-55&&pedals->front.get().gate!=input.gate,"Actual Guitar IN/Noise Gate controls retain existing gain/threshold/toggle actions");
        guitarIn.setValue(input.gainDb,sendNotificationSync);gateThreshold.setValue(input.thresholdDb,sendNotificationSync);gateToggle.setToggleState(input.gate,dontSendNotification);gateToggle.onClick();
        const bool native=displayedSource==amp::SourceType::Nam;
        check(namSourceButton.isVisible()&&vstSourceButton.isVisible()&&namSourceButton.getBottom()<vstSourceButton.getY()&&namSourceButton.getToggleState()!=vstSourceButton.getToggleState(),"Source choices remain permanent, vertically stacked and mutually exclusive");
        check(!native||(normalize.getWidth()<=tonegrid::ui::normalizationMax&&normalize.isVisible()&&namGain.valueToProportionOfLength(0)==.5&&namVolume.valueToProportionOfLength(0)==.5),"NAM normalization stays compact and Gain/Volume zero stays at noon");
        check(spreadToggle.isVisible()&&spreadGear.isVisible()&&reverbToggle.isVisible()&&reverbGear.isVisible()&&bool(spreadGear.onClick)&&bool(reverbGear.onClick),"Existing PostFX toggles and settings actions remain exposed, MOD and Delay have active compact controls");
    }
    template<class Check>void checkCompactLayout(Check&& check)
    {
        const auto width=sections[0].getWidth(),oldHeight=roundToInt(width*280.f/2016);
        check(sections[0].getHeight()==tonegrid::ampui::compactHeight(width),"AMP height uses the fidelity reduction with usable minimum at window "+String(getWidth())+" x "+String(getHeight()));
        const auto oldArea=getHeight()-28-44-90-12-(logVisible?88:32)-8-oldHeight-10;
        const auto oldCardHeight=jmin((oldArea-12)/2,roundToInt((width/7-4)*288.f/224));
        check(cards[0]&&cards[0]->getHeight()>oldCardHeight&&cards[7]->getHeight()==cards[0]->getHeight(),"Both pedal rows regain usable height without changes to card artwork or controls");
        auto gear=tonegrid::ampui::settingsButtonBounds(sections[0],0);bool geometry=spreadGear.getBounds()==gear;
        for(int i=0;i<4;++i){const auto r=tonegrid::ampui::settingsButtonBounds(sections[0],i);geometry&=r.getWidth()==r.getHeight()&&r.getWidth()==gear.getWidth();}
        check(geometry&&gear.getWidth()>=24,"Real 3D/MOD/Delay/Reverb gears use equal square geometry");
        if(displayedSource==amp::SourceType::Nam)
            check(normalize.getBounds()==tonegrid::ampui::normalizationBounds(sections[0])&&normalize.getBounds().getCentreY()==namFolder.getBounds().getCentreY()&&normalize.getRight()<namFolder.getX()&&namFilename.getRight()<normalize.getX()&&namFolder.getBounds()==tonegrid::ampui::folderButtonBounds(sections[0])&&normalize.getHeight()==namFolder.getHeight()&&normalize.getHeight()==tonegrid::ui::buttonHeight&&captureHeading.getBounds().getUnion(namFolderName.getBounds()).getCentreY()==namFolder.getBounds().getCentreY()&&bool(namFolder.onClick),"Actual Normalize and unchanged Folder action share a clear horizontal band at "+String(getWidth()));
    }
    bool verifyVst3Workflow(String& result)
    {
        bool ok=true;int checks=0;auto check=[&](bool value,const String& label){++checks;ok&=value;result+=String(value?"PASS ":"FAIL ")+label+"\n";};
        check(!hardwareEnabled&&!audioReady&&!midi,"UI test uses synthetic app-data with ASIO/MIDI disabled");
        check(ready(),"UI test automatically loads default TONEX and thirteen pedals");if(!ready())return false;exerciseUiCleanup(check);
        check(sections[0].contains(pluginChoice.getBounds())&&sections[0].contains(ampGui.getBounds())&&sections[0].contains(scanPlugins.getBounds())
              &&!pluginChoice.getBounds().intersects(ampGui.getBounds())&&!ampGui.getBounds().intersects(scanPlugins.getBounds()),"AMP dropdown/Open/Scan fit downstream AMP section without overlap");
        check(pluginChoice.getRight()==scanPlugins.getRight()&&pluginChoice.getWidth()==tonegrid::ampui::vstActionsBounds(sections[0]).getWidth(),"Real VST dropdown is compact and aligned with Rescan action area's right edge");
        const auto tonex=*ampSource->pluginDescription();auto* pedalEngine=pedals;
        PluginDescription alternate;bool available=false;
        for(const auto& preferred:{"Genome","BYOD","AmpliTube"})
        {
            for(const auto& d:pluginCatalog.plugins)if(d.name.containsIgnoreCase(preferred))
            {auto loaded=vst3::load(d);if(loaded.source){alternate=*loaded.source->pluginDescription();available=true;break;}}
            if(available)break;
        }
        if(!available){result+="SKIP cross-plugin UI workflow: no compatible optional VST3 available.\n";return ok;}
        auto choose=[&](const PluginDescription& description)
        {
            for(size_t index=0;index<pluginCatalog.plugins.size();++index)
                if(vst3::Identity::of(pluginCatalog.plugins[index])==vst3::Identity::of(description))
                {pluginChoice.setSelectedId((int)index+1,sendNotificationSync);return;}
        };
        choose(alternate);
        check(vst3::Identity::of(*ampSource->pluginDescription())==vst3::Identity::of(alternate)&&pedals==pedalEngine,
              "Real dropdown callback switches to "+alternate.name+" without recreating pedal engine");
        exerciseReverbUi(check);exerciseEffectsUi(check);
        name.setText("Synthetic UI selected VST3",false);saveSlot();
        board::State saved;PluginDescription wanted;
        check(rigStore.loadForSelection(0,rigIdentities(),saved,wanted).wasOk()&&saved.genericVst3&&vst3::Identity::of(wanted)==vst3::Identity::of(alternate),
              "Actual Save handler writes selected VST3 rig v12 in temporary slot");
        check(saved.outputDsp&&!saved.outputDsp->spreadEnabled&&!normalize.isVisible(),"VST3 rig retains output settings while NAM-only Normalize control is hidden");
        check(saved.outputDsp&&saved.outputDsp->reverb.enabled&&saved.outputDsp->reverb.timeSeconds==2.7f,"Actual VST3 Save preserves independent Reverb controls");
        check(saved.outputDsp&&saved.outputDsp->expandedEffects&&saved.outputDsp->pitch.dry==74&&saved.outputDsp->pitch.semitones==-7&&saved.outputDsp->mod.type==tonegrid::mod::Type::Flanger&&saved.outputDsp->mod.rateHz==3.2f&&saved.outputDsp->delay.timeMs==600&&saved.outputDsp->delay.feedback==71,"Actual Save retains every new effect in rig v12");
        choose(tonex);check(vst3::Identity::of(*ampSource->pluginDescription())==vst3::Identity::of(tonex),"Actual dropdown switches back to TONEX");
        slot.setSelectedId(2,sendNotificationSync);slot.setSelectedId(1,sendNotificationSync);
        check(vst3::Identity::of(*ampSource->pluginDescription())==vst3::Identity::of(alternate)&&name.getText()==saved.name,
              "Actual preset dropdown restores another plugin and complete named rig");
        check(outputSettings.reverb.enabled&&reverbToggle.getToggleState()&&reverbControls.state(true).timeSeconds==2.7f,"VST3 preset recall restores Reverb and synchronizes popup");
        check(effectsStateMatches()&&pitchControls.pitchSettings().up==22&&modControls.modSettings().level==44&&delayControls.delaySettings().mix==29,"Actual preset recall restores every expanded effect and popup control");
        vst3::Catalog selected;check(selectionStore.read(selected).wasOk()&&selected.plugins.size()==1
              &&vst3::Identity::of(selected.plugins[0])==vst3::Identity::of(alternate),"Successful recall persists stable startup selection");
        auto missing=wanted;missing.uniqueId=7654321;missing.deprecatedUid=7654322;missing.fileOrIdentifier=bankDir.getChildFile("missing-test.vst3").getFullPathName();
        check(rigStore.save(1,saved,missing,rigIdentities()).result.wasOk(),"Create recoverable synthetic rig with missing plugin");
        auto* before=ampStage->getProcessor();const auto gain=pedals->front.get().gainDb;
        recallSlot(1);
        check(ampStage->getProcessor()==before&&pedals==pedalEngine&&pedals->front.get().gainDb==gain,
              "Missing-plugin recall through actual host preserves active amp and pedal state");
        check(rigStore.contains(1),"Missing-plugin rig remains on disk without rewriting/substitution");
        setSize(1360,820);
        check(sections[0].contains(pluginChoice.getBounds())&&sections[0].contains(scanPlugins.getBounds()),"VST3 controls also fit minimum window width");
        check(pluginChoice.getRight()==scanPlugins.getRight(),"Compact VST alignment holds at minimum window size");
        result+=String(checks)+" isolated UI workflow checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
    }
    bool verifyWorkflow(const File& file)
    {
        String result;bool ok=true;
        auto check=[&](bool value,const String& label){result+=(value?"PASS ":"FAIL ")+label+"\n";ok=ok&&value;};
        check(ready()&&ampSource->pluginDescription(),"VST3 amp and captures loaded automatically for legacy workflow test");
        check(player.getCurrentProcessor()==&graph,"Audio attached automatically at 48 kHz");
        if(ready()&&ampSource->pluginDescription())
        {
            guitarIn.setValue(6,sendNotificationSync);gateThreshold.setValue(-55,sendNotificationSync);gateToggle.setToggleState(true,dontSendNotification);gateToggle.onClick();
            auto input=pedals->front.get();check(input.gainDb==6&&input.thresholdDb==-55&&input.gate,"Header controls update real input gain and gate");
            int placeholders=0;for(int v=0;v<board::count;++v)if(PedalCard::engineFor(v)<0)++placeholders;check(placeholders==0,"All thirteen pedal slots have active engines");
            bool routing=true,ui=true;
            for(int v=0;v<board::count;++v)
            {
                const int p=PedalCard::engineFor(v);routing=routing&&p==board::order[(size_t)v];if(p<board::legacyPedalCount)continue;
                auto& card=*cards[(size_t)v];const auto before=pedals->get(p);
                if(auto* power=dynamic_cast<TextButton*>(card.findChildWithID("power"))){power->onClick();ui=ui&&pedals->get(p).enabled!=before.enabled;}else ui=false;
                int sliders=0;for(auto* child:card.getChildren())
                {
                    if(auto* combo=dynamic_cast<ComboBox*>(child)){ui=ui&&combo->getNumItems()==8;combo->setSelectedId(8,sendNotificationSync);ui=ui&&pedals->get(p).preset==7;}
                    if(auto* knob=dynamic_cast<Slider*>(child))if(knob->isVisible()){knob->setValue(p==board::id(board::PedalId::AutoWah)?(sliders==2?333:42):p==board::id(board::PedalId::GraphicEq)?3:2,sendNotificationSync);++sliders;}
                }
                const auto after=pedals->get(p);
                if(p==board::id(board::PedalId::AutoWah))ui=ui&&sliders==3&&after.wahSensitivity==42&&after.wahRange==42&&after.wahDecay==333;
                else if(p==board::id(board::PedalId::GraphicEq)){ui=ui&&sliders==7;for(float db:after.eqBands)ui=ui&&db==3;}
                else ui=ui&&after.input==2&&after.tone==2&&after.output==2;
                pedals->set(p,before);card.sync();
            }
            check(routing,"Visual pedal order matches all thirteen audio stages");
            check(ui,"New pedal toggles, eight presets, Wah knobs, NAM Tone and seven EQ faders update engine");
            int tested=0;const int original=selected();
            for(int i=0;i<rig::slotCount && tested<2;++i)
            {
                if(!rigStore.contains(i))continue;
                board::State expected;
                if(rigStore.load(i,*ampSource->pluginDescription(),rigIdentities(),expected).failed())continue;
                // Change a real control before selecting the saved slot through the ComboBox callback.
                auto changed=pedals->get(0);changed.enabled=!expected.pedals[0].enabled;pedals->set(0,changed);
                slot.setSelectedId(0,dontSendNotification);slot.setSelectedId(i+1,sendNotificationSync);
                bool same=true;
                for(int p=0;p<board::count;++p){const auto actual=pedals->get(p);const auto wanted=expected.pedals[(size_t)p];same=same&&actual.enabled==wanted.enabled&&actual.preset==wanted.preset&&std::abs(actual.input-wanted.input)<.001f&&std::abs(actual.mix-wanted.mix)<.001f&&std::abs(actual.tone-wanted.tone)<.001f&&std::abs(actual.output-wanted.output)<.001f&&actual.eqBands==wanted.eqBands&&actual.wahSensitivity==wanted.wahSensitivity&&actual.wahRange==wanted.wahRange&&actual.wahDecay==wanted.wahDecay;}
                check(same && name.getText()==expected.name,"Selecting saved slot "+String(i+1)+" restores the rig");++tested;
            }
            check(tested==2,"Two existing saved rigs available for selection test");
            for(int i=rig::slotCount-1;i>=0;--i)if(!rigStore.contains(i))
            {
                const auto before=pedals->get(0);slot.setSelectedId(i+1,sendNotificationSync);const auto after=pedals->get(0);
                check(before.enabled==after.enabled&&before.preset==after.preset&&before.input==after.input,"Empty slot preserves current sound");break;
            }
            if(rigStore.contains(original)){slot.setSelectedId(original+1,sendNotificationSync);}
            check(player.getCurrentProcessor()==&graph,"Audio stays active after rig selection");
        }
        result+=ok?"ALL PASSED\n":"FAILED\n";file.replaceWithText(result);return ok;
    }
    bool verifyRecording114Workflow(String& result)
    {
        using namespace tonegrid::recording;bool ok=true;int checks=0;
        auto check=[&](bool value,const String& label){++checks;ok&=value;result+=String(value?"PASS ":"FAIL ")+"1.1.14 host "+label+"\n";};
        auto await=[&](Mode wanted){for(int i=0;i<500;++i){if(recorder.state()==wanted&&!recorder.requestPending())return true;Thread::sleep(2);}return false;};
        auto screenshot=[&](const String& suffix){recordingControls.sync();takeScreenshot(File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("ux114-"+suffix+".png"));};
        prepareUx112Preview();recordingControls.setAudioAvailable(true);screenshot("empty");
        check(recordingControls.getNumChildComponents()==4&&!recorder.available(),"three transport icons and one timer start without a take");
        check(recorder.record(48000)&&await(Mode::recording),"host recorder starts on its worker");
        AudioBuffer<float> signal(2,128);for(int block=0;block<100;++block){for(int i=0;i<128;++i){signal.setSample(0,i,.2f*float(std::sin((block*128+i)*.1)));signal.setSample(1,i,.1f*float(std::cos((block*128+i)*.1)));}recorder.process(signal.getArrayOfWritePointers(),2,128);Thread::sleep(1);}
        screenshot("recording");check(recorder.stop()&&await(Mode::ready),"host finalizes the new take");screenshot("ready");
        check(recorder.play(48000)&&await(Mode::playing),"host starts playback of the last take");screenshot("playing");
        check(recorder.stop()&&await(Mode::ready),"host stops playback and retains take");
        for(int width:{1660,1360})
        {
            setSize(width,940);const auto boxes=tonegrid::assets::boardBounds(pedalArea);
            check(recordingControls.getX()>boxes[12].getRight()&&recordingControls.getRight()<=pedalArea.getRight()&&recordingControls.getWidth()>=36,"transport fits beside Clean Boost at width "+String(width));
            screenshot("ready-"+String(width));
        }
        setSize(1660,940);result+=String(checks)+" recording host checks\n";return ok;
    }
    static Component* popup115Child(Component& parent,const String& id)
    {
        if(parent.getComponentID()==id)return &parent;
        for(auto* child:parent.getChildren())if(auto* match=popup115Child(*child,id))return match;
        return nullptr;
    }
    void capturePopup115(Component& component,const String& label,bool composite=false)
    {
        const auto exe=File::getSpecialLocation(File::currentExecutableFile);
        auto image=component.createComponentSnapshot(component.getLocalBounds());
        if(composite)
        {
            auto scene=createComponentSnapshot(getLocalBounds());Graphics g(scene);
            g.drawImageAt(image,(scene.getWidth()-image.getWidth())/2,(scene.getHeight()-image.getHeight())/2);image=scene;
        }
        MemoryOutputStream output;PNGImageFormat().writeImageToStream(image,output);
        exe.getSiblingFile("ux115-"+label+".png").replaceWithData(output.getData(),output.getDataSize());
    }
    bool verifyPopup115Workflow(String& result)
    {
        bool ok=true;auto check=[&](bool value,const String& text){ok&=value;result+=String(value?"PASS ":"FAIL ")+"1.1.15 host "+text+"\n";};
        const auto* source=ampSource.get();const auto settings=tonegrid::output::encode(outputSettings).toString();
        const auto midi=bankDir.getChildFile("midi.xml").loadFileAsString();
        auto inspect=[&](DialogWindow* window,Component& view,const String& label)
        {
            check(window&&!window->isUsingNativeTitleBar()&&window->getContentComponent()->getComponentID()=="popup-frame"&&view.isShowing(),label+" uses common window and remains attached");
            if(!window)return;
            auto* title=dynamic_cast<Label*>(popup115Child(*window,"popup-title"));
            check(title&&title->getText()==window->getName().toUpperCase()&&title->getFont().getHeight()==22.f
                  &&popup115Child(*window,"popup-brand-mark"),label+" has one large header title beside the pictogram");
            capturePopup115(*window,label);capturePopup115(*window,label+"-scene",true);
            if(auto* close=dynamic_cast<Button*>(popup115Child(*window,"popup-close")))close->onClick();
            check(!window->isVisible(),label+" closes through its header");
            delete window;
            check(!view.getParentComponent(),label+" persistent view survives close and detaches");
        };
        showPresetBrowser();presetBrowser.showToneBank(false);presetBrowser.focusUser(0);
        inspect(presetDialog.getComponent(),presetBrowser,"user-presets");
        showPresetBrowser();presetBrowser.showToneBank(true);inspect(presetDialog.getComponent(),presetBrowser,"tonebank");presetBrowser.showToneBank(false);
        showNamChooser();inspect(namBrowserDialog.getComponent(),namBrowser,"nam-library");
        showReverbSettings();inspect(reverbDialog.getComponent(),reverbControls,"reverb");
        showSpreadSettings();inspect(spreadDialog.getComponent(),spreadControls,"3d-sound");
        showEffectsSettings(tonegrid::EffectsPanel::Mod);inspect(modDialog.getComponent(),modControls,"mod");
        showEffectsSettings(tonegrid::EffectsPanel::Delay);inspect(delayDialog.getComponent(),delayControls,"delay");
        showEffectsSettings(tonegrid::EffectsPanel::Pitch);inspect(pitchDialog.getComponent(),pitchControls,"pitch");
        showNamChooser();if(namBrowserDialog)static_cast<Component*>(namBrowserDialog.getComponent())->keyPressed(KeyPress(KeyPress::escapeKey));
        check(namBrowserDialog&&!namBrowserDialog->isVisible(),"Escape closes the common library window");delete namBrowserDialog.getComponent();
        audioSettings();if(audioDialog)capturePopup115(*audioDialog,"audio-settings");
        check(audioDialog&&popup115Child(*audioDialog,"nam-a2-lite")&&audioDialog->getContentComponent()->getComponentID()=="popup-frame","Audio Settings retains device selector and Lite preference in common frame");delete audioDialog.getComponent();
        check(ampSource.get()==source&&tonegrid::output::encode(outputSettings).toString()==settings&&bankDir.getChildFile("midi.xml").loadFileAsString()==midi,"opening and closing all restyled windows preserves amp, effects and MIDI data");
        return ok;
    }
    bool verifyPopup115PromptStep(int step,String& result,Component::SafePointer<Component>& dismissed)
    {
        bool ok=true;auto check=[&](bool value,const String& text){ok&=value;result+=String(value?"PASS ":"FAIL ")+"1.1.15 host "+text+"\n";};
        constexpr int index=rig::slotCount-1;
        auto finish=[&](const String& text,bool cancel,const String& label)
        {
            auto* window=ModalComponentManager::getInstance()->getModalComponent(0);
            check(window&&popup115Child(*window,"popup-frame"),label+" opens a styled prompt");if(!window)return;
            dismissed=window;
            if(auto* editor=dynamic_cast<TextEditor*>(popup115Child(*window,"preset-name")))
            {check(editor->hasKeyboardFocus(false),label+" starts with name input focused");editor->setText(text);}
            capturePopup115(*window,label);
            if(auto* button=dynamic_cast<Button*>(popup115Child(*window,cancel?"prompt-cancel":"prompt-confirm")))button->onClick();else check(false,label+" action missing");
        };
        switch(step)
        {
            case 0:
                check(!rigStore.contains(index),"prompt test uses an empty temporary slot");slot.setSelectedId(index+1,dontSendNotification);askSave();finish("Popup save regression",false,"save-preset");break;
            case 1:
                check(rigStore.contains(index)&&rigStore.readName(index)=="Popup save regression","Save callback writes the confirmed name");
                askRenamePreset(index);finish("Must not be written",true,"rename-cancel");break;
            case 2:
                check(rigStore.readName(index)=="Popup save regression","Cancel rename keeps existing name");
                askRenamePreset(index);finish("Popup renamed regression",false,"rename-preset");break;
            case 3:
                check(rigStore.readName(index)=="Popup renamed regression","Rename callback changes the intended slot");
                askDeletePreset(index);finish({},true,"delete-cancel");break;
            case 4:
                check(rigStore.contains(index),"Cancel delete retains the saved preset");askDeletePreset(index);finish({},false,"delete-preset");break;
            case 5:
                check(!rigStore.contains(index),"Delete callback removes only the confirmed temporary preset");
                slot.setSelectedId(1,dontSendNotification);reloadSlots(0);break;
        }
        return ok;
    }
    bool verifyRelease113Workflow(String& result)
    {
        bool ok=true;int checks=0;auto check=[&](bool value,const String& text){++checks;ok&=value;result+=String(value?"PASS ":"FAIL ")+"1.1.13 host "+text+"\n";};
        const auto before=ampSource.get();const auto settings=tonegrid::output::encode(outputSettings).toString();
        slot.setSelectedId(7,dontSendNotification);name.setText("Movable preset",false);saveSlot();
        auto assignments=midiController.assignments();assignments[6]={2,3,66};midiController.restoreAssignments(board::encodeBindings(assignments));storeMidiBindings(assignments);
        const auto bytes=rigStore.slotFile(6).loadFileAsString();const auto midi=bankDir.getChildFile("midi.xml").loadFileAsString();
        movePreset(6,1);
        check(selected()==7&&!rigStore.contains(6)&&rigStore.slotFile(7).loadFileAsString()==bytes&&ampSource.get()==before&&tonegrid::output::encode(outputSettings).toString()==settings&&bankDir.getChildFile("midi.xml").loadFileAsString()==midi&&midiController.assignments()==assignments,"moving active preset follows its name without reloading audio or moving MIDI");
        movePreset(7,-1);deletePreset(6);
        check(setNamLite(true)&&namLite&&tonegrid::namquality::read(qualityPreference)&&ampSource.get()==before&&dynamic_cast<amp::NamAmp*>(ampSource.get())->state().model.file.isNotEmpty(),"Lite applies to running compatible pedals, persists, and retains the unsupported amp object and neural history");
        check(setNamLite(false)&&!namLite&&!tonegrid::namquality::read(qualityPreference)&&tonegrid::output::encode(outputSettings).toString()==settings,"return to Full retains all effect controls");
        showTuner();check(tunerDialog&&tunerEngine.active()&&audioTap.isMuted()&&!tunerDialog->isUsingNativeTitleBar(),"header tuner opens a custom muted input popup without hardware in tests");
        auto* tunerTitle=dynamic_cast<Label*>(tunerControls.findChildWithID("popup-title"));
        check(tunerTitle&&tunerTitle->getText()=="TUNER"&&tunerTitle->getFont().getHeight()==22.f,
              "tuner shares the large title beside its pictogram without changing tuning controls");
        const auto exe=File::getSpecialLocation(File::currentExecutableFile);auto capture=[&](Component& c,const String& label){MemoryOutputStream out;PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),out);exe.getSiblingFile("ux113-"+label+".png").replaceWithData(out.getData(),out.getDataSize());};
        capture(tunerControls,"tuner");
        capture(*tunerDialog,"tuner-window");
        auto reading=[](double midi){return tonegrid::tuning::Reading{440*std::pow(2.,(midi-69)/12),.99,-15};};
        for(const auto& sample:std::array<std::pair<double,const char*>,3>{{{31.035,"tuner-in-tune"},{57-.25,"tuner-flat"},{57+.25,"tuner-sharp"}}})
        {
            for(int frame=0;frame<90;++frame)tunerControls.updateReading(reading(sample.first),1./60);
            capture(tunerControls,sample.second);
        }
        tunerControls.setSize(420,330);capture(tunerControls,"tuner-small");tunerControls.setSize(560,440);
        auto* close=dynamic_cast<Button*>(tunerControls.findChildWithID("tuner-close"));
        if(close)close->onClick();
        check(close&&tunerDialog&&!tunerDialog->isVisible(),"tuner close icon hides the modal window");
        delete tunerDialog.getComponent();tunerControls.close();
        check(!audioTap.isMuted()&&!tunerEngine.active(),"tuner close restores sound and stops analysis");
        showTuner();if(tunerDialog)static_cast<Component*>(tunerDialog.getComponent())->keyPressed(KeyPress(KeyPress::escapeKey));
        check(tunerDialog&&!tunerDialog->isVisible(),"Escape closes the custom tuner");
        delete tunerDialog.getComponent();tunerControls.close();
        prepareUx112Preview();takeScreenshot(exe.getSiblingFile("ux113-main.png"));setSize(1360,940);takeScreenshot(exe.getSiblingFile("ux113-main-1360.png"));setSize(1660,940);
        showNamChooser();capture(namBrowser,"library");delete namBrowserDialog.getComponent();
        showPresetBrowser();capture(presetBrowser,"presets");delete presetDialog.getComponent();
        showReverbSettings();capture(reverbControls,"reverb");delete reverbDialog.getComponent();
        audioSettings();if(audioDialog&&audioDialog->getContentComponent())capture(*audioDialog->getContentComponent(),"audio");delete audioDialog.getComponent();
        check(versionLabel.getText()==String("ToneGrid ")+JUCE_APPLICATION_VERSION_STRING,"footer derives the public ToneGrid version from the build");
        check(checkUpdate.getComponentID()=="check-update"&&checkUpdate.isVisible()&&checkUpdate.getButtonText()=="Check for update","footer offers a manual version check");
        setSize(1360,820);
        result+="INFO footer status="+status.getBounds().toString()+" version="+versionLabel.getBounds().toString()+" update="+checkUpdate.getBounds().toString()+" log="+latestLog.getBounds().toString()+"\n";
        check(checkUpdate.getX()>=versionLabel.getRight()&&!status.getBounds().intersects(versionLabel.getBounds())&&!checkUpdate.getBounds().intersects(latestLog.getBounds())&&status.getWidth()>=600,"footer version, update button and audio status fit the minimum window");
        check(!onlineUpdates.busy(),"isolated host performs no automatic website requests");setSize(1660,940);
        result+=String(checks)+" release 1.1.13 host checks\n";return ok;
    }
