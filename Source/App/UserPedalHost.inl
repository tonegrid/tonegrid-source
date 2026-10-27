// Included inside RigHost: control-thread orchestration only.
    void refreshPersonalPedals()
    {
        if(!pedals)return;
        for(int v=0;v<board::count;++v)if(cards[size_t(v)])
        {const int p=PedalCard::engineFor(v);if(!board::isNam(p))continue;std::vector<std::pair<String,String>> labels;for(const auto& entry:userPedals.entries(p))labels.emplace_back(entry.hash,entry.label);cards[size_t(v)]->setPersonalCaptures(std::move(labels));}
    }
    void addPersonalPedal(int p,bool tone3000)
    {
        if(!pedals||!board::isNam(p))return;
        if(tone3000)
        {
            if(pedalTone3000Window&&pedalTone3000Window->isVisible()){pedalTone3000Window->toFront(true);report("Finish the current pedal import first.");return;}
            pedalTone3000Window.reset();const auto root=appPaths.user().getChildFile("NAM/PedalDownloads");
            pedalTone3000Window=std::make_unique<tonegrid::t3k::Tone3000BrowserWindow>(root,appPaths.browserProfile(),
                [safe=Component::SafePointer<RigHost>(this),p,root](const tonegrid::t3k::ImportResult& result)
                {
                    if(!safe)return;if(!result.ok){safe->report(result.message);return;}
                    if(safe->userLibraryOperation.valid()){safe->report("Finish the current library operation first, then import the pack again.");return;}
                    std::vector<std::pair<File,String>> files;
                    for(const auto& entry:result.entries){const auto file=root.getChildFile(entry.identity.file);const auto manifest=tonegrid::t3k::localManifest(file);auto url=manifest["url"].toString();if(url.isEmpty())url="https://www.tone3000.com/tones/"+manifest["tone_id"].toString();files.emplace_back(file,url);}
                    const bool lite=safe->namLite;const auto* library=&safe->userPedals;
                    safe->userLibraryOperation=std::async(std::launch::async,[library,p,files=std::move(files),lite]{return library->importPack(p,files,lite);});
                    safe->librarySuccess="Pedal pack added to "+board::pedalName(p)+". Choose a capture from My captures to play it.";safe->report("Adding every capture in the selected pedal pack...");
                },
                std::function<void()>{},std::function<void(const String&)>{},"pedal");
            pedalTone3000Window->start();return;
        }
        const auto folder=userPedals.folder(p);
        if(userPedals.ensureFolder(p).failed()||!folder.startAsProcess()){report("Cannot open the personal pedal folder: "+folder.getFullPathName());return;}
        report("Add or remove .nam files in this pedal folder. My captures refreshes automatically; calibration is checked when you select one.");
    }
    void deletePersonalPedal(int p,const String& hash)
    {
        if(pedalDeleteDialog||userLibraryOperation.valid()){report("Finish the current library operation first.");return;}
        const auto entry=userPedals.entry(p,hash);
        auto* prompt=new tonegrid::popup::Prompt("Delete \""+entry.label+"\" from this pedal's personal library?\n\nPresets using it will keep its reference and bypass the missing pedal. Factory captures are kept.","Delete capture");
        pedalDeleteDialog=tonegrid::popup::launch("Delete personal capture",*prompt,*this,true,0,ModalCallbackFunction::create(
            [safe=Component::SafePointer<RigHost>(this),p,hash](int result)
            {if(!safe||result!=1)return;const auto* library=&safe->userPedals;safe->userLibraryOperation=std::async(std::launch::async,[library,p,hash]{return library->remove(p,hash);});safe->librarySuccess="Personal capture deleted.";}));
    }
    void checkMissingPersonalPedals()
    {
        if(!pedals)return;
        for(int p=0;p<board::count;++p)if(board::isNam(p))
        {
            auto setting=pedals->get(p);if(setting.preset!=board::presetCount(p))continue;
            const auto hash=pedals->identity(p,setting.preset);
            if(pedals->available(p,setting.preset)&&!userPedals.file(p,hash).existsAsFile()){pedals->markCustomMissing(p);report("Personal capture removed or changed: "+board::pedalName(p)+" bypassed. Select another capture explicitly.");}
        }
        for(auto& card:cards)if(card)card->sync();updateCalibrationStatus();
    }
    void pollPersonalLibrary()
    {
        if(userLibraryOperation.valid()&&userLibraryOperation.wait_for(std::chrono::seconds(0))==std::future_status::ready)
        {const auto result=userLibraryOperation.get();refreshPersonalPedals();checkMissingPersonalPedals();report(result.wasOk()?librarySuccess:result.getErrorMessage());}
        if(userLibraryScan.valid()&&userLibraryScan.wait_for(std::chrono::seconds(0))==std::future_status::ready)
        {
            if(userLibraryScan.get()){refreshPersonalPedals();checkMissingPersonalPedals();const auto errors=userPedals.errors();if(errors.isNotEmpty())report("Some personal NAM files are incompatible and were excluded from My captures:\n"+errors);}
        }
        if(pedals&&!userLibraryScan.valid()&&!userLibraryOperation.valid()&&++userLibraryTicks>=60)
        {userLibraryTicks=0;const auto* library=&userPedals;userLibraryScan=std::async(std::launch::async,[library]{return library->refresh();});}
    }
    void loadPersonalPedal(int p,File file,String hash,String label,String url,bool importing)
    {
        if(userPedalLoading.valid()||pedalImportDialog){report("Finish the current capture import first.");return;}
        pendingUserPedal=p;pendingUserLabel=std::move(label);pendingUserUrl=std::move(url);pendingUserImport=importing;
        const bool lite=namLite;
        userPedalLoading=std::async(std::launch::async,[file,hash,lite]{return tonegrid::userpedals::prepare(file,lite,hash);});
        report("Loading personal capture for "+board::pedalName(p)+". Current sound kept while loading.");
    }
    void acceptPersonalPedal(int p,const std::shared_ptr<tonegrid::userpedals::Prepared>& prepared,const String& url,bool importing)
    {
        if(!pedals)return;
        if(!importing&&(!prepared->source.existsAsFile()||SHA256(prepared->source).toHexString()!=prepared->model.hash)){report("Capture was removed or changed while loading. Current sound kept.");return;}
        if(importing){const auto stored=userPedals.store(p,*prepared,url);if(stored.failed()){report(stored.getErrorMessage());return;}}
        player.setProcessor(nullptr);
        try{tonegrid::namquality::apply(*prepared->model.dsp,namLite);prepared->model.dsp->prewarm();pedals->installCustom(p,std::move(prepared->model));auto setting=board::defaults(p,board::presetCount(p));setting.enabled=pedals->get(p).enabled;pedals->set(p,setting);}
        catch(const std::exception& e){attach();report("Personal capture activation failed: "+String(e.what()));return;}
        attach();refreshPersonalPedals();report("Personal capture selected for "+board::pedalName(p)+". Save to retain it in a User Preset.");updateCalibrationStatus();
    }
    void pollPersonalPedal()
    {
        if(!userPedalLoading.valid()||userPedalLoading.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        auto prepared=std::make_shared<tonegrid::userpedals::Prepared>(userPedalLoading.get());
        if(prepared->result.failed()||!prepared->model.dsp){report(prepared->result.getErrorMessage());return;}
        prepared->model.label=pendingUserLabel;const auto p=pendingUserPedal;const auto url=pendingUserUrl;const bool importing=pendingUserImport;
        if(!prepared->model.calibration.complete())
        {
            auto* prompt=new tonegrid::popup::Prompt("This pedal is not fully calibrated. Automatic matching of input and output levels is incomplete; you may need to adjust Input Trim and Level.\n\n"+prepared->model.calibration.description()+"\n\nUse this capture anyway?","Use capture");
            pedalImportDialog=tonegrid::popup::launch("Pedal calibration warning",*prompt,*this,true,0,ModalCallbackFunction::create(
                [safe=Component::SafePointer<RigHost>(this),p,prepared,url,importing](int result)
                {if(!safe||result!=1)return;safe->acceptPersonalPedal(p,prepared,url,importing);}));
        }
        else acceptPersonalPedal(p,prepared,url,importing);
    }
    std::vector<tonegrid::userpedals::Prepared> preparePersonalRecall(board::State& state)
    {
        std::vector<tonegrid::userpedals::Prepared> result(static_cast<size_t>(board::count));
        for(int p=0;p<board::count;++p)if(state.customPedalHashes[size_t(p)].isNotEmpty())
        {
            const auto hash=state.customPedalHashes[size_t(p)];const auto entry=userPedals.entry(p,hash);
            result[size_t(p)]=tonegrid::userpedals::prepare(userPedals.file(p,hash),namLite,hash);auto& item=result[size_t(p)];
            if(item.result.failed()||!item.model.dsp){item.model={};item.model.hash=hash;item.model.label=entry.label;item.model.file="User/"+hash+".nam";state.pedals[size_t(p)].enabled=false;state.missingCaptureDiagnostics.add("Personal capture missing or incompatible for "+board::pedalName(p)+". Pedal bypassed: "+hash.substring(0,8));}
            else item.model.label=entry.label;
        }
        return result;
    }
