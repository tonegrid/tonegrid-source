    bool verifyToneBank133(String& report,const File& folder)
    {
        bool ok=true;int checks=0;const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("ToneBank133 host ")+label+"\n";};
        const auto original=toneBank;
        check(toneBank.presets.size()>100&&!appPaths.resources().getChildFile("ToneBank/transition-legacy.json").existsAsFile(),"running app ships only the current modern ToneBank");
        toneBank=original;auto& target=toneBank.presets[0];auto& fx=*target.state.outputDsp;
        fx.ampEq.enabled=true;fx.ampEq.lowCut=true;fx.ampEq.highCut=true;fx.ampEq.lowCutHz=120;fx.ampEq.highCutHz=8500;
        fx.ampEq.presenceDb=3.5f;fx.ampEq.presenceHz=3900;fx.ampEq.depthDb=-2.5f;fx.ampEq.depthHz=90;target.state.pedals[0].mix=.35f;
        loadToneBank(0);const auto expected=tonegrid::output::encode(outputSettings).toString();
        check(outputSettings.ampEq.presenceDb==3.5f&&outputSettings.ampEq.depthDb==-2.5f&&outputSettings.ampEq.lowCutHz==120&&outputSettings.ampEq.highCutHz==8500&&std::abs(pedals->get(0).mix-.35f)<.0001f,"ToneBank applies extended EQ and compressor mix to the actual host");
        saveSlot();const int saved=selected();loadToneBank(1);recallSlot(saved);
        check(tonegrid::output::encode(outputSettings).toString()==expected&&std::abs(pedals->get(0).mix-.35f)<.0001f,"Save, switch and recall preserve every added musical setting");
        board::State savedState;PluginDescription none;check(rigStore.loadForSelection(saved,rigIdentities(),savedState,none).wasOk()&&savedState.outputDsp&&tonegrid::output::encode(*savedState.outputDsp).toString()==expected,"saved disk preset contains the full settings");
        const auto suggestion=tonegrid::sharing::body(savedState,rigIdentities(),"Synthetic references");check(suggestion.contains("presenceDb")&&suggestion.contains("depthDb")&&suggestion.contains("lowCutHz")&&suggestion.contains("highCutHz"),"Share serializes the same extended settings without opening email");
        const auto growthIndex=toneBank.presets.size();auto extra=target;extra.rank=(int)growthIndex+1;extra.id="101-host-growth";extra.song="Growth test";toneBank.presets.push_back(extra);presetBrowser.setBank(toneBank);presetBrowser.showToneBank(true);
        check(presetBrowser.visibleCount()==(int)growthIndex+1,"running host browser displays the grown bank");presetBrowser.activateRow((int)growthIndex);check(!presetRequests.idle(),"last row above 100 queues its correct preset");loadToneBank(growthIndex);check(name.getText().contains("Growth test"),"decoded grown-bank sound applies correctly");
        auto* current=ampSource.get();const auto nodeCount=graph.getNodes().size();
        toneBank.presets[growthIndex].amp.sha256=String::repeatedString("a",64);for(int p:{1,2}){toneBank.presets[growthIndex].state.pedals[size_t(p)].enabled=true;toneBank.presets[growthIndex].captures[size_t(p)].sha256=String::repeatedString("b",64);}
        loadToneBank(growthIndex);check(missingCapturesDialog&&missingCapturesDialog->isShowing()&&ampSource.get()==current&&graph.getNodes().size()==nodeCount&&!pedals->get(1).enabled&&!pedals->get(2).enabled,"one visible popup aggregates a missing amp and two pedals; current amp and graph remain safe");
        const std::function<Component*(Component*,const String&)> find=[&](Component* root,const String& id)->Component*
        {if(!root)return nullptr;if(root->getComponentID()==id)return root;for(int i=0;i<root->getNumChildComponents();++i)if(auto* child=find(root->getChildComponent(i),id))return child;return nullptr;};
        auto* details=dynamic_cast<TextEditor*>(find(missingCapturesDialog.getComponent(),"missing-captures-details"));
        check(details&&details->getText().contains(toneBank.presets[growthIndex].amp.file)&&details->getText().contains("808 OD")&&details->getText().contains("HORSE OD")&&details->getText().contains("Your current sound has been kept")
            &&find(missingCapturesDialog.getComponent(),"missing-captures-update-status"),"popup names the exact amp and pedals, keeps the sound and explains application-update recovery");
        for(const auto size:std::initializer_list<Point<int>>{{700,530},{360,340}})
        {
            if(!missingCapturesDialog)break;missingCapturesDialog->setSize(size.x,size.y);
            auto* close=find(missingCapturesDialog.getComponent(),"missing-captures-close");auto* search=find(missingCapturesDialog.getComponent(),"missing-captures-search");
            bool visible=close&&search&&close->isShowing()&&search->isShowing();if(visible)for(auto* button:{close,search})visible&=missingCapturesDialog->getLocalBounds().contains(missingCapturesDialog->getLocalArea(button,button->getLocalBounds()));
            check(visible,"Close and Tone3000 actions remain visible at "+String(size.x)+"x"+String(size.y));
            MemoryOutputStream png;PNGImageFormat().writeImageToStream(missingCapturesDialog->createComponentSnapshot(missingCapturesDialog->getLocalBounds()),png);folder.getChildFile("missing-captures133-"+String(size.x)+".png").replaceWithData(png.getData(),png.getDataSize());
        }
        delete missingCapturesDialog.getComponent();toneBank=original;loadToneBank(0);
        auto user=original.presets[0].state;user.name="Unavailable user amp test";user.namAmp=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();user.namAmp.model.sha256=String::repeatedString("e",64);
        const auto written=rigStore.save(18,user,none,rigIdentities());current=ampSource.get();recallSlot(18);
        check(written.result.wasOk()&&missingCapturesDialog&&missingCapturesDialog->isShowing()&&ampSource.get()==current,"User Preset missing amp also opens the popup and keeps the current rig");delete missingCapturesDialog.getComponent();
        user.namAmp=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();user.name="Unavailable personal pedal test";user.pedals[1].preset=board::presetCount(1);user.pedals[1].enabled=true;user.customPedalHashes[1]=String::repeatedString("d",64);
        const auto custom=rigStore.save(19,user,none,rigIdentities());recallSlot(19);
        check(custom.result.wasOk()&&missingCapturesDialog&&missingCapturesDialog->isShowing()&&!pedals->get(1).enabled,"missing personal pedal is bypassed with a visible popup on User Preset recall");delete missingCapturesDialog.getComponent();
        toneBank=original;presetBrowser.setBank(toneBank);report+=String(checks)+" ToneBank133 host checks\n";return ok;
    }
