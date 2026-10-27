    tonegrid::cabinet::Library cabinetLibrary{appPaths.user().getChildFile("CabinetIR"),appPaths.factoryCabinetIR()};
    tonegrid::cabinet::State cabinetState;
    std::optional<tonegrid::cabinet::Entry> selectedCabinetPreview;
    tonegrid::cabinet::Panel cabinetPanel;
    tonegrid::cabinet::Browser cabinetBrowser;
    Component::SafePointer<DialogWindow> cabinetDialog,cabinetRecommendationDialog;
    std::future<tonegrid::cabinet::Prepared> cabinetLoading;
    std::future<std::unique_ptr<tonegrid::cabinet::Library>> cabinetRefreshing;
    std::unique_ptr<tonegrid::t3k::Tone3000BrowserWindow> cabinetOnlineWindow;
    std::optional<namamp::State> pendingHead;
    bool cabinetInstalled=false;unsigned cabinetSelectionGeneration=0,cabinetLoadGeneration=0;
    bool cabinetBypassed=false;
    String installedCabinetHash;

    void initialiseCabinetUi()
    {
        refreshCabinetLibrary(hardwareEnabled||bundledBootstrapTest);
        if(auto xml=XmlDocument::parse(appPaths.user().getChildFile("CabinetIR/selected.xml")))tonegrid::cabinet::decode(*xml,cabinetState);
        if(auto xml=XmlDocument::parse(appPaths.user().getChildFile("Audio/cabinet-output.xml"));xml&&xml->hasTagName("CabinetOutput"))cabinetBypassed=xml->getBoolAttribute("bypassed");
        else cabinetBypassed=cabinetState.bypassed;
        addAndMakeVisible(cabinetPanel);cabinetPanel.setComponentID("cabinet-panel");
        cabinetPanel.browse.onClick=[this]{showCabinetBrowser();};
        cabinetPanel.lock.onClick=[this]{cabinetState.locked=cabinetPanel.lock.getToggleState();saveCabinetPreference();syncCabinetMode();};
        cabinetPanel.power.onClick=[this]
        {
            const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get());if(!canChooseCabinet()||!native||captureKind(native->state().model.file)!=tonegrid::cabinet::CaptureKind::head)return;cabinetBypassed=!cabinetPanel.power.getToggleState();cabinetState.bypassed=cabinetBypassed;
            XmlElement preference("CabinetOutput");preference.setAttribute("bypassed",cabinetBypassed);
            const auto file=appPaths.user().getChildFile("Audio/cabinet-output.xml");
            if(file.getParentDirectory().createDirectory().failed()||!rig::writeXml(preference,file))report("Could not save the cabinet output preference.");
            saveCabinetPreference();syncCabinetMode();
        };
        namBrowser.onCaptureKind=[this](const String& path,const String& type)
        {
            for(const auto& entry:namCatalog.entries)if(entry.identity.file==path)
            {
                const auto file=namLibrary.folder().getChildFile(".tonegrid-types.json");nlohmann::json types=nlohmann::json::object();
                if(file.existsAsFile()&&file.getSize()<128*1024)try{types=nlohmann::json::parse(file.loadFileAsString().toStdString());}catch(...){}
                if(!types.is_object())types=nlohmann::json::object();types[entry.identity.sha256.toStdString()]=type.toStdString();
                if(!file.replaceWithText(String(types.dump(2))))report("Cannot save the capture type.");else refreshNamLibrary();break;
            }
        };
        cabinetBrowser.onSelect=[this](const auto& state){if(canChooseCabinet())queueCabinet(state);};
        cabinetBrowser.onRefresh=[this]{refreshCabinetLibrary();};
        cabinetBrowser.onGetCaptures=[this]{
            if(auto* dialog=cabinetDialog.getComponent()){dialog->exitModalState(0);delete dialog;}
            cabinetOnlineWindow.reset();cabinetOnlineWindow=std::make_unique<tonegrid::t3k::Tone3000BrowserWindow>(cabinetLibrary.root,appPaths.user().getChildFile("Tone3000Cabinets/WebView2"),
                [safe=Component::SafePointer<RigHost>(this)](const tonegrid::t3k::ImportResult& result){if(safe){safe->cabinetBrowser.setStatus(result.message);safe->report(result.message);if(result.ok)safe->refreshCabinetLibrary();}},
                [safe=Component::SafePointer<RigHost>(this)]{if(safe)safe->showCabinetBrowser();},std::function<void(const String&)>{},"cab");cabinetOnlineWindow->start();
        };
        cabinetPanel.recommended.onClick=[this]{if(!canChooseCabinet())return;if(const auto* native=dynamic_cast<amp::NamAmp*>(ampSource.get()))if(const auto* entry=cabinetLibrary.recommended(native->state().model.sha256)){queueCabinet(entry->state);return;}report("No cabinet recommendation is associated with this capture.");};
    }
    tonegrid::cabinet::CaptureKind captureKind(const String& path)const
    {
        if(namCatalog.browser){auto found=namCatalog.browser->captures.find(path);if(found!=namCatalog.browser->captures.end())return tonegrid::cabinet::kind(found->second.gear);}
        // Newly imported captures may precede their first browser snapshot.
        for(const auto& entry:namCatalog.entries)if(entry.identity.file==path)
            return tonegrid::captures::resolveKind(namLibrary,entry.identity,appPaths.factoryAmps()).kind;
        namamp::Entry entry;
        if(namLibrary.read(namLibrary.folder().getChildFile(path),entry).wasOk())
            return tonegrid::captures::resolveKind(namLibrary,entry.identity,appPaths.factoryAmps()).kind;
        return tonegrid::cabinet::CaptureKind::unknown;
    }
    void saveCabinetPreference()
    {
        const auto destination=appPaths.user().getChildFile("CabinetIR/selected.xml");
        if(destination.getParentDirectory().createDirectory().wasOk())rig::writeXml(tonegrid::cabinet::encode(cabinetState),destination);
    }
    bool canChooseCabinet()const
    {const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get());return native&&!cabinetLoading.valid()&&tonegrid::cabinet::needsIR(captureKind(native->state().model.file));}
    tonegrid::cabinet::Prepared prepareCabinet(const tonegrid::cabinet::State& state)const
    {return tonegrid::cabinet::prepare(cabinetLibrary.root.getChildFile(state.file),cabinetLibrary.root,&state);}
    bool installCabinet(tonegrid::cabinet::Prepared&& prepared,bool reconnect=true)
    {
        if(prepared.result.failed()||!cabinetNode)return false;
        if(reconnect)player.setProcessor(nullptr);
        cabinetState=prepared.state;cabinetState.bypassed=cabinetBypassed;cabinetInstalled=true;installedCabinetHash=cabinetState.sha256;
        cabinetBrowser.selectActive(cabinetState.file);cabinetNode->load(std::move(prepared));cabinetNode->prepareToPlay(48000,256);saveCabinetPreference();
        if(reconnect){syncCabinetMode();attach();}return true;
    }
    void queueCabinet(tonegrid::cabinet::State state)
    {
        if(cabinetLoading.valid())return;state.locked=cabinetState.locked;
        cabinetLoadGeneration=cabinetSelectionGeneration;const auto root=cabinetLibrary.root;
        cabinetLoading=std::async(std::launch::async,[root,state]{return tonegrid::cabinet::prepare(root.getChildFile(state.file),root,&state);});
        syncCabinetMode();
        cabinetBrowser.setSelectionEnabled(false,"Loading cabinet IR...");
    }
    void pollCabinetLoad()
    {
        if(!cabinetLoading.valid()||cabinetLoading.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        auto result=cabinetLoading.get();
        if(cabinetLoadGeneration!=cabinetSelectionGeneration){pendingHead.reset();syncCabinetMode();return;}
        if(result.result.failed()){report(result.result.getErrorMessage()+" Previous sound kept.");namBrowser.setImportStatus(result.result.getErrorMessage()+" Previous sound kept.");pendingHead.reset();syncCabinetMode();return;}
        if(!cabinetNode){pendingHead.reset();return;}
        if(pendingHead)
        {
            const auto wanted=*pendingHead;pendingHead.reset();auto loaded=amp::loadNamAmp(namLibrary,wanted,namLite);
            if(loaded.result.failed()){report(loaded.result.getErrorMessage()+" Previous sound kept.");namBrowser.setImportStatus(loaded.result.getErrorMessage());syncCabinetMode();return;}
            const bool enteringNam=dynamic_cast<amp::NamAmp*>(ampSource.get())==nullptr;
            player.setProcessor(nullptr);
            const auto changed=amp::replace(graph,ampSource,ampStage,pedalStageId,cabinetStageId,std::move(loaded.source),retiredAmps);
            if(changed.failed()){attach();report(changed.getErrorMessage()+" Previous sound kept.");syncAmpUi();return;}
            if(enteringNam){outputSettings.ampNormalizationEnabled=true;outputSettings.ampNormalizationEnhanced=true;}
            installCabinet(std::move(result),false);displayedSource=ampSource->type();syncAmpUi();persistSelection();attach();
            for(const auto& entry:namCatalog.entries)if(entry.identity==wanted.model){queueCaptureWarning(entry);break;}
            report("Active head and cabinet: "+ampName());namBrowser.setImportStatus("Head and cabinet loaded.");
        }
        else if(!installCabinet(std::move(result)))return;
        syncCabinetMode();
    }
    void syncCabinetMode()
    {
        const auto* native=dynamic_cast<const amp::NamAmp*>(ampSource.get());
        const auto kind=native?captureKind(native->state().model.file):tonegrid::cabinet::CaptureKind::unknown;
        const bool head=native&&tonegrid::cabinet::needsIR(kind);
        const auto* entry=cabinetLibrary.find(cabinetState);
        if(!entry&&selectedCabinetPreview&&selectedCabinetPreview->state.file==cabinetState.file&&selectedCabinetPreview->state.sha256==cabinetState.sha256)entry=&*selectedCabinetPreview;
        cabinetState.bypassed=cabinetBypassed;
        if(cabinetNode)cabinetNode->enable(head&&!cabinetBypassed);
        cabinetPanel.update(entry,!native?"Managed by the VST plugin":kind==tonegrid::cabinet::CaptureKind::fullRig?"Cabinet included":head?(cabinetLoading.valid()?"Loading IR...":cabinetBypassed?"Cabinet simulation off":cabinetState.locked?"IR locked across heads":"External cabinet IR"):"Capture type unknown",head&&!cabinetLoading.valid(),cabinetState.locked,head&&!cabinetLoading.valid(),!cabinetBypassed);
        cabinetPanel.power.setEnabled(kind==tonegrid::cabinet::CaptureKind::head&&!cabinetLoading.valid());
        cabinetPanel.setRecommended(head&&!cabinetLoading.valid()&&cabinetLibrary.recommended(native->state().model.sha256)!=nullptr);
        cabinetBrowser.setSelectionEnabled(head&&!cabinetLoading.valid(),cabinetLoading.valid()?"Loading cabinet IR...":head?String{}:kind==tonegrid::cabinet::CaptureKind::fullRig?"Cabinet included in this capture. Load a Head Only capture to use an IR.":"Load a Head Only NAM capture to use a cabinet IR.");
        if(!suppressStartupCabinetChoice&&head&&!cabinetBypassed&&cabinetNode&&!cabinetLoading.valid()&&(!cabinetInstalled||installedCabinetHash!=cabinetState.sha256))
        {
            const auto* chosen=entry?entry:cabinetLibrary.fallback();
            if(chosen)queueCabinet(chosen->state);else report("No cabinet IR is installed. Import a WAV before using this head.");
        }
    }
    bool proposeRecommendedCabinet(const namamp::State& wanted)
    {
        auto path=wanted.model.file;
        if(!namLibrary.folder().getChildFile(path).existsAsFile())
        {namamp::Entry resolved;nlohmann::json payload;if(namLibrary.resolve(wanted.model,resolved,payload).wasOk())path=resolved.identity.file;}
        if(!tonegrid::cabinet::needsIR(captureKind(path)))return false;
        if(cabinetLoading.valid()){report("Wait for the cabinet to finish loading.");return true;}
        if(cabinetInstalled&&!cabinetState.empty()&&installedCabinetHash==cabinetState.sha256)
        {
            if(!cabinetState.locked)if(const auto* suggested=cabinetLibrary.recommended(wanted.model.sha256);suggested&&suggested->state.sha256!=cabinetState.sha256)
            {pendingHead=wanted;queueCabinet(suggested->state);return true;}
            return false;
        }
        if(const auto* previous=cabinetLibrary.find(cabinetState);previous&&previous->state.sha256==cabinetState.sha256)
        {pendingHead=wanted;queueCabinet(cabinetState);return true;}
        const auto* recommended=cabinetLibrary.recommended(wanted.model.sha256);
        const auto* fallback=recommended?recommended:cabinetLibrary.fallback();
        if(!fallback){report("No default cabinet IR is installed. Import a WAV first. Previous amp kept.");return true;}
        if(recommended)
        {
            pendingHead=wanted;auto* prompt=new tonegrid::cabinet::Recommendation(recommended->name);
            cabinetRecommendationDialog=tonegrid::popup::launch("Recommended cabinet",*prompt,*this,true,0,ModalCallbackFunction::create([safe=Component::SafePointer<RigHost>(this),wanted,chosen=recommended->state,generation=cabinetSelectionGeneration](int result){if(!safe||safe->cabinetSelectionGeneration!=generation)return;safe->pendingHead=wanted;if(result==1)safe->queueCabinet(chosen);else if(result==2){if(const auto* def=safe->cabinetLibrary.fallback())safe->queueCabinet(def->state);else safe->pendingHead.reset();}else safe->pendingHead.reset();}));return true;
        }
        pendingHead=wanted;queueCabinet(fallback->state);return true;
    }
    void showCabinetBrowser()
    {
        if(!canChooseCabinet())return;
        if(cabinetDialog){cabinetDialog->toFront(true);return;}
        cabinetBrowser.update(cabinetLibrary,cabinetState,appPaths.user().getChildFile("CabinetIR/favorites.xml"));
        cabinetBrowser.setSize(1240,640);cabinetDialog=tonegrid::popup::launch("Cabinet IR Library",cabinetBrowser,*this,false,400);
    }
    void refreshCabinetLibrary(bool seed=false)
    {
        if(cabinetRefreshing.valid())return;cabinetBrowser.setRefreshing(true);
        cabinetRefreshing=std::async(std::launch::async,[snapshot=std::make_unique<tonegrid::cabinet::Library>(cabinetLibrary),seed]()mutable{snapshot->scan(seed);return std::move(snapshot);});
    }
    void pollCabinetRefresh()
    {
        if(!cabinetRefreshing.valid()||cabinetRefreshing.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try{auto next=cabinetRefreshing.get();cabinetLibrary.publish(std::move(*next));if(cabinetDialog)cabinetBrowser.update(cabinetLibrary,cabinetState,appPaths.user().getChildFile("CabinetIR/favorites.xml"));syncCabinetMode();}
        catch(const std::exception&){cabinetBrowser.setStatus("Cannot refresh cabinet library. Previous catalogue kept.");}
        cabinetBrowser.setRefreshing(false);
    }


public:
    bool cabinetTestReady()const{return ready()&&!loading.valid()&&!namService.busy()&&!cabinetLoading.valid()&&!cabinetRefreshing.valid();}
    bool verifyIRPrototype(const File& evidence,String& results)
    {
        bool ok=true;auto check=[&](bool value,const String& text){ok&=value;results+=String(value?"PASS ":"FAIL ")+text+"\n";};
        evidence.createDirectory();
        const auto oldSize=getBounds();setSize(1360,820);
        results+="INFO footer status="+status.getBounds().toString()+" version="+versionLabel.getBounds().toString()+" log="+latestLog.getBounds().toString()+"\n";
        check(status.getWidth()>=600&&!status.getBounds().intersects(versionLabel.getBounds())&&!checkUpdate.getBounds().intersects(latestLog.getBounds()),"1.1.39 footer preserves 600 pixels of status at 1360 x 820");
        setBounds(oldSize);
        check(appPaths.user().getFileName()=="ToneGrid","Prototype uses its own data folder");
        check(cabinetLibrary.entries.size()>=5,"Five curated cabinet IRs are installed");
        const namamp::Entry *head=nullptr,*full=nullptr;
        for(const auto& entry:namCatalog.entries){const auto kind=captureKind(entry.identity.file);if(!head&&tonegrid::cabinet::needsIR(kind))head=&entry;if(!full&&kind==tonegrid::cabinet::CaptureKind::fullRig)full=&entry;}
        check(head&&full,"Head and full-rig captures coexist in the same catalogue");if(!head||!full)return false;
        for(const auto& e:namCatalog.entries)if(e.identity.file.contains("/Marshall 2555X")&&e.identity.file.contains("CLN_Pristine")){head=&e;break;}
        bool categorized=true,split=true;int heads=0;
        for(const auto& [path,entry]:namCatalog.browser->captures)
        {
            categorized&=!path.startsWith("06 - Head Packs/")&&!path.startsWith("07 - Incoming Heads/");
            if(tonegrid::cabinet::needsIR(tonegrid::cabinet::kind(entry.gear)))
            {++heads;split&=path.contains("Head Only/")||path.contains("Preamp Only/")||path.contains("Power Amp Only/");}
        }
        check(categorized&&split&&heads>=1042,"All heads live in gain categories and remain separate from Full Rig folders");
        check(namCatalog.failures.empty(),"Complete factory library validates after reorganising every Head pack and adding the eight requested packs");
        int verifiedHeads=0;std::set<String> testedHeadPacks;
        for(const auto& entry:namCatalog.entries)if(tonegrid::cabinet::needsIR(captureKind(entry.identity.file)))
        {
            const auto pack=namCatalog.browser->captures.at(entry.identity.file).sourceUrl;if(!testedHeadPacks.insert(pack).second)continue;
            auto loaded=amp::loadNamAmp(namLibrary,{entry.identity,0,0},namLite);bool valid=loaded.result.wasOk();
            if(valid)
            {
                AudioProcessorGraph probe;auto node=loaded.source->addToGraph(probe);valid=node!=nullptr;
                if(node){node->getProcessor()->prepareToPlay(48000,128);AudioBuffer<float> buffer(2,128);MidiBuffer midi;double energy=0;
                    for(int block=0;block<16;++block){for(int i=0;i<128;++i){const float sample=.02f*std::sin(float(block*128+i)*.0144f);buffer.setSample(0,i,sample);buffer.setSample(1,i,sample);}node->getProcessor()->processBlock(buffer,midi);for(int i=0;i<128;++i){const auto v=buffer.getSample(0,i);valid&=std::isfinite(v);energy+=double(v)*v;}}
                    valid&=energy>1e-16;}
            }
            check(valid,"Real head loads and renders finite audio: "+entry.identity.file);++verifiedHeads;
        }
        check(verifiedHeads>=6,"A real Head capture from every installed Head pack was exercised");
        std::set<String> testedCabinetPacks;
        for(const auto& entry:cabinetLibrary.entries)
        {
            if(!testedCabinetPacks.insert(entry.pack.isEmpty()?entry.name:entry.pack).second)continue;
            auto ir=prepareCabinet(entry.state);bool valid=ir.result.wasOk();
            if(valid){tonegrid::cabinet::Node processor;processor.load(std::move(ir));processor.prepareToPlay(48000,128);processor.enable(true);AudioBuffer<float> buffer(2,128);MidiBuffer midi;double energy=0;
                for(int block=0;block<32;++block){buffer.clear();if(block==0){buffer.setSample(0,0,.1f);buffer.setSample(1,0,.1f);}processor.processBlock(buffer,midi);for(int i=0;i<128;++i){const auto v=buffer.getSample(0,i);valid&=std::isfinite(v);energy+=double(v)*v;}}valid&=energy>1e-16;}
            check(valid,"Real cabinet validates and renders: "+entry.name);
        }

        const auto* fallback=cabinetLibrary.fallback();check(fallback!=nullptr,"A declared default IR is available");if(!fallback)return false;
        auto prepared=prepareCabinet(fallback->state);check(prepared.result.wasOk()&&installCabinet(std::move(prepared)),"Install the default IR before loading the head");
        auto loaded=amp::loadNamAmp(namLibrary,{head->identity,0,0},namLite);check(loaded.result.wasOk(),"Load a real curated NAM head");if(!loaded.source)return false;commitSource(std::move(loaded.source));
        {
            const auto moved=nlohmann::json::parse(namLibrary.folder().getChildFile("factory-relocations.json").loadFileAsString().toStdString());
            bool restored=false;
            for(const auto& item:moved.at("moves"))if(item.at("to")==head->identity.file.toStdString())
            {
                auto oldIdentity=head->identity;oldIdentity.file=String(item.at("from").get<std::string>());
                auto prior=amp::loadNamAmp(namLibrary,{oldIdentity,0,0},namLite);
                if(auto* source=dynamic_cast<amp::NamAmp*>(prior.source.get()))restored=source->state().model==head->identity;
                break;
            }
            check(restored,"A saved old Head identity loads the identical real NAM with its current category path");
        }
        if(const auto* suggested=cabinetLibrary.recommended(head->identity.sha256);suggested&&suggested->state.sha256!=cabinetState.sha256)
        {
            cabinetState.locked=false;
            check(proposeRecommendedCabinet({head->identity,0,0})&&cabinetLoading.valid()&&!cabinetRecommendationDialog,"Unlocked cabinet follows an associated head without a repeated popup");
            const auto deadline=Time::getMillisecondCounter()+10000;
            while(cabinetLoading.valid()&&cabinetLoading.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&Time::getMillisecondCounter()<deadline)Thread::sleep(1);
            pollCabinetLoad();check(cabinetState.sha256==suggested->state.sha256,"Associated IR has actually replaced the unlocked default");
            cabinetState.locked=true;
            for(const auto& other:namCatalog.entries)if(const auto* next=cabinetLibrary.recommended(other.identity.sha256);next&&next->state.sha256!=cabinetState.sha256)
            {check(!proposeRecommendedCabinet({other.identity,0,0})&&!cabinetLoading.valid()&&cabinetState.sha256==suggested->state.sha256,"Lock prevents an automatic cabinet change for another associated head");break;}
        }
        {
            const auto previous=cabinetState;queueCabinet(fallback->state);++cabinetSelectionGeneration;
            const auto deadline=Time::getMillisecondCounter()+10000;
            while(cabinetLoading.valid()&&cabinetLoading.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&Time::getMillisecondCounter()<deadline)Thread::sleep(1);
            pollCabinetLoad();check(cabinetState.sha256==previous.sha256&&cabinetState.locked==previous.locked,"An obsolete IR job cannot overwrite a later selection or preset");
        }
        cabinetState.locked=true;syncCabinetMode();
        check(cabinetPanel.browse.isEnabled()&&cabinetPanel.lock.getToggleState(),"Head activates cabinet browsing and remembers the lock");
        const auto clickArtwork=[&](bool fullRig)
        {
            // Exercise JUCE's hit testing and mouse-down/up routing on the actual
            // rack, with a windowless peer; never move the user's cursor.
            const bool wasVisible=isVisible();const auto oldBounds=getBounds();
            namBrowserLifetimeTests::MouseHost host;host.setSize(getWidth(),getHeight());host.addAndMakeVisible(*this);setTopLeftPosition(0,0);
            int amps=0,cabs=0;auto ampCallback=namFolder.onClick,cabCallback=cabinetPanel.browse.onClick;
            namFolder.onClick=[&]{++amps;};cabinetPanel.browse.onClick=[&]{++cabs;};
            const auto ampPoint=ampPanel.getPosition().toFloat()+Point<float>(20,12);
            const auto cabPoint=getLocalPoint(&cabinetPanel,Point<float>(20,12));
            const bool ampHit=getComponentAt(ampPoint)==&namFolder,cabHit=getComponentAt(cabPoint)==&cabinetPanel.browse;
            for(const auto point:{ampPoint,cabPoint}){host.mouse().event(point,false);host.mouse().event(point,true);host.mouse().release();}
            namFolder.onClick=std::move(ampCallback);cabinetPanel.browse.onClick=std::move(cabCallback);
            host.removeChildComponent(this);setBounds(oldBounds);setVisible(wasVisible);
            check(ampHit&&cabHit&&amps==1&&cabs==(fullRig?0:1),fullRig?"Included cabinet artwork ignores real mouse clicks":"Real mouse clicks reach both artwork browser buttons with a Head + IR");
        };
        clickArtwork(false);
        {
            const bool wasVisible=isVisible();const auto bounds=getBounds();namBrowserLifetimeTests::MouseHost host;host.setSize(getWidth(),getHeight());host.addAndMakeVisible(*this);setTopLeftPosition(0,0);
            int ampClicks=0,cabClicks=0;auto a=namFolder.onClick,c=cabinetPanel.browse.onClick;namFolder.onClick=[&]{++ampClicks;};cabinetPanel.browse.onClick=[&]{++cabClicks;};
            bool hit=true;
            for(auto* card:std::initializer_list<tonegrid::libraryui::GearCard*>{&ampPanel,&cabinetPanel})
            {
                for(int i=0;i<30;++i)advanceToolbarHints(card->getBounds().getCentre(),1./30.);
                const auto point=getLocalPoint(&card->load,card->load.getLocalBounds().getCentre()).toFloat();hit&=getComponentAt(point)==&card->load;
                host.mouse().event(point,false);host.mouse().event(point,true);host.mouse().release();
            }
            namFolder.onClick=std::move(a);cabinetPanel.browse.onClick=std::move(c);host.removeChildComponent(this);setBounds(bounds);setVisible(wasVisible);
            check(hit&&ampClicks==1&&cabClicks==1,"Both centered overlay Load buttons receive real mouse clicks and use the existing browsers");
            for(int i=0;i<30;++i)advanceToolbarHints({0,0},1./30.);
        }

        {const auto scans=cabinetLibrary.scans;const auto start=Time::getMillisecondCounterHiRes();showCabinetBrowser();const auto elapsed=Time::getMillisecondCounterHiRes()-start;
         results+="INFO Cabinet browser open: "+String(elapsed,2)+" ms; additional scans: "+String(int(cabinetLibrary.scans-scans))+"\n";
         check(cabinetDialog&&cabinetLibrary.scans==scans&&elapsed<500,"Opening the cabinet browser reuses the catalogue without disk scanning (under 500 ms)");
         if(auto* dialog=cabinetDialog.getComponent()){dialog->exitModalState(0);delete dialog;}}
        check(namBrowser.onlineGear()=="amp_amp-cab","Both amp filters request Head and Amp+Cab in the online selector");
        check(cabinetBrowser.hasBreadcrumb()&&cabinetBrowser.findChildWithID("cabinet-ir-refresh")&&cabinetBrowser.findChildWithID("cabinet-ir-open-folder")&&cabinetBrowser.findChildWithID("cabinet-ir-tone3000"),"Cabinet list provides breadcrumbs, refresh, open-folder and TONE3000 controls");
        const auto retained=cabinetState.sha256;
        name.setText("Prototype Head + IR",false);saveSlot();const auto savedSlot=selected();takeScreenshot(evidence.getChildFile("tonegrid-head.png"));
        loaded=amp::loadNamAmp(namLibrary,{full->identity,0,0},namLite);check(loaded.result.wasOk(),"Load a real complete capture");if(!loaded.source)return false;commitSource(std::move(loaded.source));
        check(!cabinetPanel.browse.isEnabled()&&!cabinetBrowser.canSelect()&&!cabinetPanel.lock.isEnabled()&&!cabinetPanel.recommended.isVisible()&&cabinetState.sha256==retained&&cabinetState.locked,"Amp + Cab disables cabinet browsing and IR replacement while retaining the locked choice");
        cabinetBrowser.onSelect(fallback->state);
        check(!cabinetLoading.valid()&&cabinetState.sha256==retained,"Browsing an included cabinet cannot replace the retained IR");
        clickArtwork(true);
        takeScreenshot(evidence.getChildFile("tonegrid-full-rig.png"));recallSlot(savedSlot);
        check(ampSource&&dynamic_cast<amp::NamAmp*>(ampSource.get())&&static_cast<amp::NamAmp*>(ampSource.get())->state().model==head->identity&&cabinetState.sha256==retained&&cabinetState.locked&&cabinetPanel.browse.isEnabled(),"Recall restores the head, exact IR and lock");
        const auto headFolder=head->identity.file.upToLastOccurrenceOf("/",false,false);
        namBrowser.update(namCatalog,namLibrary.folder(),head->identity.file);namBrowser.setSize(1240,640);namBrowser.setTypes(false,true);namBrowser.navigate(headFolder);
        check(namBrowser.visibleRows()>0&&headFolder.contains("Head Only")&&namBrowser.gridColumns()==1,"Head-only captures appear in a named model folder inside a gain category");
        namBrowser.setTypes(true,false);check(namBrowser.visibleRows()==0,"Head + Cab filter excludes the new DI-only packs");
        namBrowser.setTypes(false,true);namBrowser.navigate(headFolder);
        for(int n=0;n<30;++n)advanceToolbarHints(ampPanel.getBounds().getCentre(),1./30.);
        check(ampPanel.hoverOpacity()>.95f&&normalizeHelp.getTooltip().isNotEmpty(),"Normalization control and explanation appear near the amp");
        const bool previousVisibility=isVisible();setVisible(true);
        const auto* normalizeHit=getComponentAt(getLocalPoint(&normalize,normalize.getLocalBounds().getCentre()));setVisible(previousVisibility);
        check(normalizeHit==&normalize||(normalizeHit&&normalize.isParentOf(normalizeHit)),"Normalization dropdown receives clicks above the amp thumbnail");
        takeScreenshot(evidence.getChildFile("tonegrid-hover-controls.png"));
        for(int n=0;n<30;++n)advanceToolbarHints({0,0},1./30.);
        check(ampPanel.hoverOpacity()<.01f&&cabinetPanel.hoverOpacity()<.01f,"Normalization and lock fade away outside their zones");
        check(cabinetPanel.getHeight()>=110&&ampPanel.getBottom()<sections[0].getBottom(),"Taller amplification stage keeps large artwork inside its bounds");
        check(ampPanel.getY()==cabinetPanel.getY()&&ampPanel.getLocalBounds()==cabinetPanel.getLocalBounds()&&ampPanel.artworkBounds().getX()==cabinetPanel.artworkBounds().getX(),"Amp and cabinet cards share geometry, text insets and artwork layout");
        check(!cabinetBrowser.findChildWithID("cabinet-ir-import")&&!cabinetBrowser.findChildWithID("cabinet-ir-recommended"),"Cabinet browser removes Import WAV and Load recommended actions");
        check(cabinetLibrary.bundle==appPaths.factoryCabinetIR()&&cabinetLibrary.bundle.getFileName()=="FactoryCabinetIR","Factory cabinet resources follow the shared portable resource policy");
        for(int n=0;n<30;++n)advanceToolbarHints(cabinetPanel.getBounds().getCentre(),1./30.);
        check(cabinetPanel.hoverOpacity()>.95f&&ampPanel.hoverOpacity()<.01f&&cabinetPanel.help.getTooltip().isNotEmpty(),"Cabinet overlay fades independently and explains the IR lock");
        takeScreenshot(evidence.getChildFile("tonegrid-cabinet-hover.png"));
        check(cabinetPanel.recommended.isVisible()==(cabinetLibrary.recommended(head->identity.sha256)!=nullptr),"Recommended card button is shown only for an installed head association");
        {tonegrid::cabinet::Panel standalone;standalone.setSize(270,130);standalone.setRecommended(false);check(!standalone.recommended.isVisible(),"No associated cabinet leaves the recommended action hidden");standalone.setRecommended(true);check(standalone.recommended.isVisible(),"Installed association reveals the recommended action");}
        if(const auto* suggested=cabinetLibrary.recommended(head->identity.sha256))
        {
            cabinetPanel.recommended.onClick();const auto deadline=Time::getMillisecondCounter()+10000;
            while(cabinetLoading.valid()&&cabinetLoading.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&Time::getMillisecondCounter()<deadline)Thread::sleep(1);
            pollCabinetLoad();check(cabinetState.sha256==suggested->state.sha256&&cabinetState.locked,"Card recommendation loads the explicitly associated IR and preserves the lock");
        }
        {tonegrid::cabinet::Panel panel;panel.setSize(270,130);panel.advanceHover(true,1./60.);const auto entering=panel.hoverOpacity();panel.advanceHover(false,1./60.);check(entering>0&&entering<1&&panel.hoverOpacity()<entering&&panel.hoverOpacity()>0,"Card fades interpolate in both directions without abrupt opacity changes");}
        for(int n=0;n<30;++n)advanceToolbarHints({0,0},1./30.);
        {
            const auto retainedIR=cabinetState.sha256;const auto retainedAmp=ampName();
            showNamChooser();auto* window=namBrowserDialog.getComponent();auto* button=dynamic_cast<Button*>(namBrowser.findChildWithID("nam-library-ok"));
            const bool opened=window&&window->isVisible();if(button&&button->onClick)button->onClick();
            check(opened&&button&&!window->isVisible(),"Amp browser OK closes the actual library popup");delete namBrowserDialog.getComponent();
            showCabinetBrowser();window=cabinetDialog.getComponent();button=dynamic_cast<Button*>(cabinetBrowser.findChildWithID("cabinet-ir-ok"));
            const bool cabOpened=window&&window->isVisible();if(button&&button->onClick)button->onClick();
            check(cabOpened&&button&&!window->isVisible(),"Cabinet browser OK closes the actual library popup");delete cabinetDialog.getComponent();
            check(ampName()==retainedAmp&&cabinetState.sha256==retainedIR,"Closing either library keeps the active amp and IR");
            check(versionLabel.getText()=="ToneGrid " JUCE_APPLICATION_VERSION_STRING,"App footer identifies the compiled product version");
        }
        {PNGImageFormat format;if(auto stream=evidence.getChildFile("tonegrid-amp-library.png").createOutputStream())format.writeImageToStream(namBrowser.createComponentSnapshot(namBrowser.getLocalBounds(),true,1.f),*stream);}
        cabinetBrowser.update(cabinetLibrary,cabinetState,appPaths.user().getChildFile("CabinetIR/favorites.xml"));cabinetBrowser.setSize(1240,640);check(cabinetBrowser.packCardCount()>=24,"Cabinet browser groups every complete IR pack into compact list rows");
        {PNGImageFormat format;if(auto stream=evidence.getChildFile("tonegrid-cabinet-library.png").createOutputStream())format.writeImageToStream(cabinetBrowser.createComponentSnapshot(cabinetBrowser.getLocalBounds(),true,1.f),*stream);}
        namBrowser.navigate(headFolder);
        if(auto* item=namBrowser.find(head->identity.file))namBrowser.reveal(head->identity.file);
        {PNGImageFormat format;if(auto stream=evidence.getChildFile("tonegrid-amp-captures.png").createOutputStream())format.writeImageToStream(namBrowser.createComponentSnapshot(namBrowser.getLocalBounds(),true,1.f),*stream);}
        for(const auto& e:cabinetLibrary.entries)if(e.source.endsWith("99311")){cabinetBrowser.showPack(e.pack);break;}
        {PNGImageFormat format;if(auto stream=evidence.getChildFile("tonegrid-cabinet-captures.png").createOutputStream())format.writeImageToStream(cabinetBrowser.createComponentSnapshot(cabinetBrowser.getLocalBounds(),true,1.f),*stream);}
        ok=verifyPopupOnboarding(evidence,results)&&ok;
        ok=verifyOptimized140(evidence,results)&&ok;
        return ok;
    }
private:
