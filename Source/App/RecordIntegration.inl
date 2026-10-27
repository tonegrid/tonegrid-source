
    void initialiseRecordModule()
    {
        tonegrid::core::ToneService service;
        service.available=[this]{return ready();};
        service.deviceRate=[this]{if(hardwareEnabled&&!player.getCurrentProcessor())return 0.;auto* device=devices.getCurrentAudioDevice();return device?device->getCurrentSampleRate():hardwareEnabled?0.:48000.;};
        service.processingLatency=[this]{auto* device=devices.getCurrentAudioDevice();const double rate=device?device->getCurrentSampleRate():48000.;return deviceProcessor.getLatencySamples()+(pedals&&outputSettings.pitch.mode!=tonegrid::pitch::Mode::Off?(int)std::llround(pedals->pitch.latencySamples()*rate/48000.):0);};
        service.capture=[this]{return captureRecordTone();};
        service.restore=[this](const String& text){
            if(!ready())return Result::fail("ToneGrid is still preparing its audio engine.");
            auto xml=XmlDocument::parse(text);if(!xml)return Result::fail("Invalid tone snapshot.");
            PluginDescription description;if(auto* plugin=xml->getChildByName("PLUGIN"))if(!description.loadFromXml(*plugin))return Result::fail("Invalid plugin snapshot.");
            board::State state;auto decoded=board::decode(*xml,description,rigIdentities(),state);if(decoded.failed())return decoded;
            ScopedValueSetter<bool> allow(validatedRecall,true);
            return applyRigState(std::move(state),description,-1)?Result::ok():Result::fail("The saved tone could not be restored. Your session audio is intact.");
        };
        service.createRenderer=[this](const String& text,double rate,String& error){
            tonegrid::core::RenderResources resources{appPaths.models(),namLibrary.folder(),cabinetLibrary.root,appPaths.user().getChildFile("NAM/Pedals"),namLite,calibrationSettings,appPaths.factoryAmps()};
            return tonegrid::core::createRenderProcessor(text,rate,resources,error);
        };
        const auto recordings=hardwareEnabled?File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("ToneGrid/Recordings/Active"):appPaths.user().getChildFile("Recordings/Active");
        recordModule=std::make_unique<tonegrid::record::RecordModule>(std::move(service),recordings,appPaths.encoder(),appPaths.user().getChildFile("Record/WebView2"),!hardwareEnabled&&!liveValidationBrowsers);
        addChildComponent(recordModule->view());audioTap.setInsert(&recordModule->audioInsert());
        modules={&playModule,&jamModule,&learnModule,recordModule.get()};
    }
    String captureRecordTone()
    {
        if(!ready())return {};board::State state;state.presetId=currentPresetId;state.extensionXml=currentPresetExtensions;state.name=presetEntry.getButtonText();state.input=pedals->front.get();state.sourceType=ampSource->type();state.genericVst3=state.sourceType==amp::SourceType::Vst3;state.outputDsp=outputSettings;state.outputDsp->expandedEffects=true;
        const bool native=state.sourceType==amp::SourceType::Nam;
        if(!native)player.setProcessor(nullptr);
        try {
            if(native){state.namAmp=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();state.cabinet=cabinetState;}
            else ampSource->captureState(state.amp);
            for(int p=0;p<board::count;++p){state.pedals[size_t(p)]=pedals->get(p);if(board::isNam(p)&&state.pedals[size_t(p)].preset==board::presetCount(p))state.customPedalHashes[size_t(p)]=pedals->identity(p,state.pedals[size_t(p)].preset);}
            if(!native)attach();const auto* description=ampSource->pluginDescription();return board::encode(state,description?*description:PluginDescription{},rigIdentities()).toString();
        }catch(...){if(!native)attach();return {};}
    }
