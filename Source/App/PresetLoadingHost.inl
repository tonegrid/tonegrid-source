    struct PreparedPreset
    {
        board::State state;PluginDescription plugin;int slot=-1;std::uint64_t revision=0;
        bool toneBank=false;String title,button;
        std::unique_ptr<amp::AmpSource> amp;
        std::optional<tonegrid::cabinet::Prepared> cabinet;
        std::optional<tonegrid::cabinet::Entry> cabinetPreview;
        std::vector<tonegrid::userpedals::Prepared> custom;
        Result result=Result::ok();
    };
    tonegrid::core::PresetLoadQueue presetRequests;
    std::future<PreparedPreset> presetPreparing;
    unsigned presetPreparationsStarted=0,presetPreparationsPublished=0;
    void queueUserPreset(int index)
    {
        if(index<0||index>=board::RigStore::maximumPresets)return;
        ++cabinetSelectionGeneration;pendingHead.reset();
        presetRequests.request(index,Time::getMillisecondCounterHiRes());
        presetBrowser.warning("Loading your last selected preset...");
    }
    void queueTonePreset(size_t index)
    {if(index>=toneBank.presets.size())return;++cabinetSelectionGeneration;pendingHead.reset();presetRequests.request(-int(index)-1,Time::getMillisecondCounterHiRes());presetBrowser.warning("Loading your last selected tone...");}
    void pollPresetPreparation()
    {
        if(presetPreparing.valid())
        {
            if(presetPreparing.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
            auto prepared=presetPreparing.get();
            if(presetRequests.current(prepared.revision))
            {
                if(prepared.result.failed())
                {report(prepared.result.getErrorMessage()+" Current sound kept.");showMissingCaptures(prepared.title,{prepared.result.getErrorMessage()});}
                else
                {
                    if(applyRigState(std::move(prepared.state),prepared.plugin,prepared.slot,std::move(prepared.amp),std::move(prepared.cabinet),std::move(prepared.custom)))
                    {
                        selectedCabinetPreview=std::move(prepared.cabinetPreview);syncCabinetMode();
                        ++presetPreparationsPublished;
                        if(prepared.toneBank){toneBankActive=true;currentPresetId=Uuid().toString();currentPresetExtensions.clear();for(int i=0;i<rigStore.capacity();++i)if(!rigStore.contains(i)){slot.setSelectedId(i+1,dontSendNotification);break;}presetEntry.setButtonText(prepared.button);name.setText(prepared.title,false);active.setText("ToneBank: "+prepared.title,dontSendNotification);report("Loaded ToneBank: "+prepared.title);}
                        presetBrowser.warning("Loaded: "+prepared.title+". Use Save to keep your own copy.");
                    }
                    else presetBrowser.warning("Could not load this preset. Current sound kept.");
                }
            }
        }
        if(!ready())return;
        auto request=presetRequests.take(Time::getMillisecondCounterHiRes());if(!request)return;
        if(request->slot>=0&&!rigStore.contains(request->slot))
        {slot.setSelectedId(request->slot+1,dontSendNotification);selectSlot();active.setText("Empty slot - current sound kept. Name it and Save Rig.",dontSendNotification);return;}
        PreparedPreset prepared;prepared.slot=request->slot;prepared.revision=request->revision;
        if(request->slot<0)
        {
            const auto index=size_t(-request->slot-1);if(index>=toneBank.presets.size())return;
            const auto& preset=toneBank.presets[index];auto catalog=namCatalog;
            if(!tonegrid::tonebank::resolveAmp(preset.amp,catalog))for(const auto& relative:{preset.amp.file,String("Factory/")+preset.amp.file})
            {namamp::Entry entry;if(namLibrary.read(namLibrary.folder().getChildFile(relative),entry).wasOk()&&entry.identity.sha256==preset.amp.sha256){catalog.entries.push_back(std::move(entry));break;}}
            auto plan=tonegrid::tonebank::prepare(preset,catalog,*pedals,appPaths.models());
            if(!plan.warnings.isEmpty()){showMissingCaptures(preset.title(),plan.warnings);return;}
            prepared.state=std::move(plan.state);prepared.slot=-2;prepared.toneBank=true;prepared.title=preset.title();prepared.button=preset.artist+" / "+preset.song;
        }
        else
        {
            prepared.result=rigStore.loadForSelection(request->slot,rigIdentities(),prepared.state,prepared.plugin);
            prepared.title=rigStore.readName(request->slot);
            if(prepared.result.failed()){showMissingCaptures(prepared.title,{prepared.result.getErrorMessage()});report(prepared.result.getErrorMessage());return;}
        }
        // Third-party plugins retain their native message-thread lifecycle.
        if(prepared.state.sourceType!=amp::SourceType::Nam){applyRigState(std::move(prepared.state),prepared.plugin,prepared.slot);return;}
        ++presetPreparationsStarted;
        presetPreparing=prepareNamPresetAsync(std::move(prepared));
    }
    std::future<PreparedPreset> prepareNamPresetAsync(PreparedPreset prepared)
    {
        const auto library=namLibrary;const auto cabinetRoot=cabinetLibrary.root,cabinetBundle=cabinetLibrary.bundle;const bool lite=namLite;
        std::vector<tonegrid::userpedals::Entry> personal(static_cast<size_t>(board::count));
        for(int p=0;p<board::count;++p)if(prepared.state.customPedalHashes[size_t(p)].isNotEmpty())
        {const auto hash=prepared.state.customPedalHashes[size_t(p)];personal[size_t(p)]=userPedals.entry(p,hash);personal[size_t(p)].location=userPedals.file(p,hash);}
        return std::async(std::launch::async,[value=std::move(prepared),library,cabinetRoot,cabinetBundle,lite,personal=std::move(personal)]()mutable
        {
            try
            {
                if(value.state.cabinet&&!value.state.cabinet->empty())
                {auto cab=tonegrid::cabinet::prepare(cabinetRoot.getChildFile(value.state.cabinet->file),cabinetRoot,&*value.state.cabinet);if(cab.result.failed()){value.result=cab.result;return std::move(value);}value.cabinet.emplace(std::move(cab));value.cabinetPreview=tonegrid::cabinet::Library(cabinetRoot,cabinetBundle).previewSelected(value.cabinet->state);}
                value.custom.resize(size_t(board::count));
                for(int p=0;p<board::count;++p)if(value.state.customPedalHashes[size_t(p)].isNotEmpty())
                {auto model=tonegrid::userpedals::prepare(personal[size_t(p)].location,lite,value.state.customPedalHashes[size_t(p)]);if(model.result.failed()){value.result=model.result;return std::move(value);}model.model.label=personal[size_t(p)].label;value.custom[size_t(p)]=std::move(model);}
                auto loaded=amp::loadNamAmp(library,value.state.namAmp,lite);if(loaded.result.failed()){value.result=loaded.result;return std::move(value);}value.amp=std::move(loaded.source);
            }
            catch(const std::exception& e){value.result=Result::fail("Cannot prepare this preset: "+String(e.what()));}
            catch(...){value.result=Result::fail("Cannot prepare this preset.");}
            return std::move(value);
        });
    }
