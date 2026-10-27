// Hardware-off test commands. Every file is under the host's injected Temp root.
    std::unique_ptr<tonegrid::record::RecordModule> record141NoDevice;String record141OfflineCaptured;
    String record141Frozen,record141FrozenHash,record141Solo,record141SoloHash;
    int record141Stage=0;String record141Take,record141TakeHash,record141BackingHash;int record141Checks=0;bool record141Ok=true;
    int tickRecord141(const File& evidence,String& report)
    {
        auto check=[&](bool pass,const String& text){++record141Checks;record141Ok&=pass;report+=String(pass?"PASS ":"FAIL ")+text+"\n";};
        if(!cabinetTestReady())return 0;
        auto& controller=recordModule->recordController();auto& view=recordModule->recordView();auto& audio=controller.audio();
        const auto folder=appPaths.user().getChildFile("Recordings/Active");
        const auto feed=[&](int blocks){AudioBuffer<float> raw(2,512),buffer(2,512);MidiBuffer midi;for(int block=0;block<blocks;++block){for(int i=0;i<512;++i){raw.setSample(0,i,(.008f+.022f*std::abs(std::sin(float(block)*.017f)))*std::sin(float(block*512+i)*.037f));raw.setSample(1,i,-.8f);}const auto* input=audio.input(raw.getArrayOfReadPointers(),2,512);buffer.copyFrom(0,0,input[0],512);buffer.copyFrom(1,0,input[1],512);graph.processBlock(buffer,midi);audio.output(buffer.getArrayOfWritePointers(),2,512);if(block%4==0)Thread::sleep(1);}};
        switch(record141Stage)
        {
            case 0:
                check(modules.size()==4&&modules[0]==&playModule&&modules[1]==&jamModule&&modules[2]==&learnModule&&modules[3]==recordModule.get(),"All four main tabs use the common Core module contract");
                graph.prepareToPlay(48000,512);audio.deviceStarted(48000);setMode(3);++record141Stage;return 0;
            case 1:
                if(controller.isBusy())return 0;
                check(recordActive&&recordTab.getToggleState()&&view.isVisible()&&!jamView.isVisible()&&!learnView.isVisible(),"RECORD navigation activates its independent workspace and hides other tab views");
                evidence.createDirectory();takeScreenshot(evidence.getChildFile("record-empty.png"));
                view.youtubeURL.setText("https://youtu.be/dQw4w9WgXcQ",false);view.loadYoutube.onClick();
                check(view.youtube.hasPlayer()&&controller.model().videoId=="dQw4w9WgXcQ","YouTube reference controller loads a validated visible reference without any audio graph route");
                view.setBusy(false,false,false);
                check(view.removeYoutube.isEnabled(),"Loading a YouTube reference refreshes view state so its actual Remove button is enabled while idle");
                controller.exportAudio(folder.getChildFile("youtube-only.mp3"),true);check(!folder.getChildFile("youtube-only.mp3").exists()&&view.status.getText().contains("YouTube policy"),"YouTube-only mix request is rejected with the explicit local-backing policy message");
                view.youtube.setAutomaticFake(false);view.record.onClick();++record141Stage;return 0;
            case 2:
                if(controller.isBusy())return 0;
                check(audio.state()==tonegrid::core::RecordAudioEngine::recording,"Record button starts the DI writer through the module controller");
                if(audio.state()!=tonegrid::core::RecordAudioEngine::recording)return -1;
                check(audio.isSyncHeld(),"The actual Record controller waits for YouTube playback readiness before capture");feed(4);check(audio.position()==0,"An armed take does not advance while the video is still loading");
                {tonegrid::record::VideoStatus value;value.state=1;value.reason="started";view.youtube.statusForTest(value);}view.youtube.setAutomaticFake(true);
                feed(3072);view.stop.onClick();++record141Stage;return 0;
            case 3:
                if(controller.isBusy())return 0;
                check(controller.model().clips.size()==1&&controller.model().clips[0].length==1572864,"Stop commits the exact audio sample interval into a single Guitar track");
                if(controller.model().clips.empty())return -1;
                {view.goStart.onClick();view.timeline.zoomIn.onClick();view.timeline.panTo(5);const bool moved=view.timeline.firstVisibleSeconds()>0;view.goStart.onClick();check(moved&&view.timeline.firstVisibleSeconds()==0&&audio.position()==0,"Go to Start restores the visible song start after navigating away with an unchanged zero cursor");view.timeline.fitSong.onClick();}
                record141Take=controller.model().clips[0].file;record141TakeHash=SHA256(folder.getChildFile(record141Take)).toHexString();
                check(folder.getChildFile("session.json").existsAsFile()&&controller.model().tone.contains("PedalRig"),"Autosaved session uses the canonical ToneGrid rig snapshot");
                namGain.setValue(6,sendNotificationSync);view.timeline.onSeek(0);view.play.onClick();++record141Stage;return 0;
            case 4:
                if(controller.isBusy())return 0;
                check(audio.state()==tonegrid::core::RecordAudioEngine::playing,"Play routes the stored DI back into current guitar processing");feed(8);
                check(SHA256(folder.getChildFile(record141Take)).toHexString()==record141TakeHash,"Changing the live tone and re-amping never rewrites the original DI");
                {tonegrid::record::VideoStatus value;value.state=2;value.seconds=double(audio.position())/controller.model().rate;value.reason="state";view.youtube.statusForTest(value);}check(audio.state()==tonegrid::core::RecordAudioEngine::stopped,"Pausing inside YouTube stops the actual module's audio transport");++record141Stage;return 0;
            case 5:
                if(controller.isBusy())return 0;view.timeline.onDelete(512,1024);++record141Stage;return 0;
            case 6:
                if(controller.isBusy())return 0;
                check(controller.model().clips.size()==2&&controller.model().clips[1].start==1024,"Guitar selection delete leaves a silent hole without ripple");
                view.timeline.onSeek(2048);view.record.onClick();++record141Stage;return 0;
            case 7:
                if(controller.isBusy())return 0;feed(8);view.stop.onClick();++record141Stage;return 0;
            case 8:
                if(controller.isBusy())return 0;
                check(controller.model().clips.size()==4&&controller.model().clips[2].start==2048&&controller.model().clips[2].length==4096&&controller.model().clips[3].start==6144,"Punch replaces only the new interval and resumes the previous take after Stop");
                check(SHA256(folder.getChildFile(record141Take)).toHexString()==record141TakeHash,"Delete and punch keep original take files intact");
                takeScreenshot(evidence.getChildFile("record-takes.png"));setSize(1360,820);takeScreenshot(evidence.getChildFile("record-small.png"));setSize(1660,940);
                setMode(0);setMode(1);setMode(2);setMode(0);++record141Stage;return 0;
            case 9:
                if(controller.isBusy())return 0;
                check(!audio.enabled()&&audio.state()==tonegrid::core::RecordAudioEngine::stopped&&!recordActive&&!view.youtube.hasPlayer(),"PLAY/JAM/LEARN adapters deactivate RECORD without hidden audio transport");
                recordModule->shutdown();audioTap.setInsert(nullptr);recordModule.reset();namGain.setValue(-6,sendNotificationSync);initialiseRecordModule();setMode(3);setMode(0);++record141Stage;return 0;
            case 10:
                if(controller.isBusy())return 0;
                check(std::abs(namGain.getValue()+6)<.001,"Leaving RECORD during asynchronous recovery does not overwrite the active PLAY tone");setMode(3);
                check(controller.model().clips.size()==4&&std::abs(namGain.getValue()-6)<.001,"Recreating RECORD restores the saved session and its canonical ToneGrid tone");
                view.timeline.onSeek(4096);view.record.onClick();++record141Stage;return 0;
            case 11:
                if(controller.isBusy())return 0;feed(4);setMode(0);
                check(!audio.enabled()&&audio.state()==tonegrid::core::RecordAudioEngine::stopped,"Switching tabs while recording stops monitoring/capture immediately");++record141Stage;return 0;
            case 12:
                if(controller.isBusy())return 0;
                check(controller.model().pendingTake.isEmpty()&&controller.model().clips.size()>=4,"Tab deactivation finalizes and autosaves the current take safely");
                check(SHA256(folder.getChildFile(record141Take)).toHexString()==record141TakeHash,"Lifecycle and recovery preserve all original source bytes");
                setMode(3);
                {
                    const auto source=appPaths.user().getChildFile("record-reference-source.wav");WavAudioFormat wav;auto stream=source.createOutputStream();std::unique_ptr<AudioFormatWriter> writer(stream?wav.createWriterFor(stream.get(),44100,2,32,{},0):nullptr);if(!writer)return -1;stream.release();AudioBuffer<float> b(2,44100);for(int c=0;c<2;++c)for(int i=0;i<44100;++i)b.setSample(c,i,(.02f+.05f*std::abs(std::sin(float(i)*.00007f)))*std::sin(float(i)*.031f));for(int second=0;second<30;++second)writer->writeFromAudioSampleBuffer(b,0,b.getNumSamples());writer.reset();record141BackingHash=SHA256(source).toHexString();controller.importBacking(source);
                }
                ++record141Stage;return 0;
            case 13:
                if(controller.isBusy())return 0;
                check(controller.model().backing.isNotEmpty()&&folder.getChildFile(controller.model().backing).existsAsFile()&&SHA256(appPaths.user().getChildFile("record-reference-source.wav")).toHexString()==record141BackingHash,"Local backing import copies into the self-contained session and preserves the user's original file");
                controller.exportAudio(folder.getChildFile("native-mix.mp3"),true);++record141Stage;return 0;
            case 14:
                if(controller.isBusy())return 0;
                {
                    AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> r(formats.createReaderFor(folder.getChildFile("native-mix.mp3")));check(r&&r->numChannels==2&&r->lengthInSamples>=1572864,"The actual module exports processed guitar plus its local backing as a valid stereo MP3");
                }
                check(SHA256(folder.getChildFile(record141Take)).toHexString()==record141TakeHash&&SHA256(appPaths.user().getChildFile("record-reference-source.wav")).toHexString()==record141BackingHash,"Full controller export leaves original DI and local backing bytes intact");
                takeScreenshot(evidence.getChildFile("record-final.png"));
                {
                    const auto copyFolder=folder.getSiblingFile("NoDevice");copyFolder.createDirectory();const auto saved=controller.model();StringArray names;for(const auto& c:saved.clips)names.addIfNotAlreadyThere(c.file);if(saved.backing.isNotEmpty())names.addIfNotAlreadyThere(saved.backing);for(const auto& name:names)if(!folder.getChildFile(name).copyFileTo(copyFolder.getChildFile(name)))return -1;
                    if(tonegrid::record::RecordSessionStore(copyFolder).save(saved).failed())return -1;
                    tonegrid::core::ToneService service;service.available=[] {return true;};service.deviceRate=[] {return 0.;};service.capture=[this]{return captureRecordTone();};service.restore=[](const String&){return Result::ok();};
                    service.createRenderer=[this](const String& text,double rate,String& error){record141OfflineCaptured=text;tonegrid::core::RenderResources r{appPaths.models(),namLibrary.folder(),cabinetLibrary.root,appPaths.user().getChildFile("NAM/Pedals"),namLite,calibrationSettings};return tonegrid::core::createRenderProcessor(text,rate,r,error);};
                    record141NoDevice=std::make_unique<tonegrid::record::RecordModule>(std::move(service),copyFolder,appPaths.encoder(),copyFolder.getChildFile("web"),true);record141NoDevice->view().setSize(1660,650);record141NoDevice->activated();
                }
                ++record141Stage;return 0;
            case 15:
                if(record141NoDevice->recordController().isBusy())return 0;
                record141NoDevice->recordView().play.onClick();check(record141NoDevice->recordController().audio().state()==tonegrid::core::RecordAudioEngine::stopped&&record141NoDevice->recordView().status.getText().contains("Audio Settings"),"Playback without an output device is rejected clearly without advancing the transport");
                namGain.setValue(9,sendNotificationSync);record141NoDevice->recordController().exportAudio(folder.getSiblingFile("NoDevice").getChildFile("offline-current-tone.mp3"),false);++record141Stage;return 0;
            case 16:
                if(record141NoDevice->recordController().isBusy())return 0;
                {
                    auto tone=XmlDocument::parse(record141OfflineCaptured);const auto* source=tone?tone->getChildByName("AmpSource"):nullptr;AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(folder.getSiblingFile("NoDevice").getChildFile("offline-current-tone.mp3")));check(source&&source->getDoubleAttribute("gainDb")==9.&&reader&&reader->lengthInSamples>=1572864,"Export works with no audio device and renders the CURRENT tone rather than the older saved snapshot");
                }
                record141NoDevice->shutdown();record141NoDevice.reset();
                check(!view.keyPressed(KeyPress(KeyPress::spaceKey),&view.youtubeURL),"Space in the YouTube text field remains ordinary text input");
                view.timeline.onSeek(607248);check(std::abs(view.youtube.requestedPosition()-12.651)<.000001,"Clicking 12.651 seconds on the timeline sends the fractional YouTube seek request");
                view.removeYoutube.onClick();check(controller.model().videoId.isEmpty()&&view.youtubeURL.getText().isEmpty()&&!view.youtube.hasPlayer()&&controller.model().clips.size()>=4,"Remove clears and stops YouTube without changing guitar takes");
                view.secondHeader.onClick();record141Stage=160;return 0;
            case 160:
                {
                    auto* prompt=Component::getCurrentlyModalComponent();if(!prompt)return 0;check(dynamic_cast<tonegrid::popup::Window*>(prompt)&&modalBackdrop.hasDialogs(),"Render confirmation uses the shared ToneGrid popup and animated dimming backdrop");evidence.createDirectory();takeScreenshot(evidence.getChildFile("record-render-backdrop.png"));if(auto* window=dynamic_cast<DialogWindow*>(prompt)){MemoryOutputStream image;PNGImageFormat().writeImageToStream(window->createComponentSnapshot(window->getLocalBounds()),image);evidence.getChildFile("record-render-prompt.png").replaceWithData(image.getData(),image.getDataSize());}
                    check(controller.model().editableTrack==0&&controller.model().second.clips.empty(),"Switching away from an editable recorded guitar offers Render and switch instead of silently changing its sound");prompt->exitModalState(0);record141Stage=161;return 0;
                }
            case 161:
                if(Component::getCurrentlyModalComponent()!=nullptr)return 0;
                controller.renderTrack(0);record141Stage=17;return 0;
            case 17:
                if(controller.isBusy())return 0;
                record141Frozen=controller.model().rendered;
                check(record141Frozen.isNotEmpty()&&folder.getChildFile(record141Frozen).existsAsFile()&&controller.model().editableTrack==-1,"Render freezes Guitar 1 into a separate playback file while retaining all original dry edits");
                if(record141Frozen.isEmpty())return -1;
                record141FrozenHash=SHA256(folder.getChildFile(record141Frozen)).toHexString();namGain.setValue(-3,sendNotificationSync);controller.selectTrack(1);++record141Stage;return 0;
            case 18:
                if(controller.isBusy())return 0;
                check(controller.model().activeTrack==1&&controller.model().editableTrack==1&&view.armSecond.getToggleState()&&!view.armFirst.getToggleState(),"The red recording selector arms only Guitar 2 after Guitar 1 is rendered");
                view.timeline.onSeek(0);view.record.onClick();++record141Stage;return 0;
            case 19:
                if(controller.isBusy())return 0;
                check(audio.state()==tonegrid::core::RecordAudioEngine::recording,"The second guitar records through the same raw-DI capture point");feed(128);view.stop.onClick();++record141Stage;return 0;
            case 20:
                if(controller.isBusy())return 0;
                check(controller.model().second.clips.size()==1&&controller.model().second.clips[0].length==65536,"Guitar 2 commits its own take and sample extent independently of Guitar 1");
                if(controller.model().second.clips.empty())return -1;
                record141Solo=controller.model().second.clips[0].file;record141SoloHash=SHA256(folder.getChildFile(record141Solo)).toHexString();
                check(SHA256(folder.getChildFile(record141Frozen)).toHexString()==record141FrozenHash&&SHA256(folder.getChildFile(record141Take)).toHexString()==record141TakeHash,"Choosing the solo tone and recording Guitar 2 leaves rendered rhythm and original Guitar 1 bytes unchanged");
                view.timeline.onSeek(0);check(view.keyPressed(KeyPress(KeyPress::spaceKey),&view.timeline),"Space on the timeline is handled as Play/Pause");++record141Stage;return 0;
            case 21:
                if(controller.isBusy())return 0;feed(4);view.keyPressed(KeyPress(KeyPress::spaceKey),&view.timeline);
                check(audio.state()==tonegrid::core::RecordAudioEngine::stopped&&audio.position()==2048,"Space pauses playback at its exact current sample rather than rewinding");++record141Stage;return 0;
            case 22:
                if(controller.isBusy())return 0;view.keyPressed(KeyPress(KeyPress::spaceKey),&view.timeline);++record141Stage;return 0;
            case 23:
                if(controller.isBusy())return 0;
                check(audio.state()==tonegrid::core::RecordAudioEngine::playing&&audio.position()==2048,"Space resumes playback from the retained cursor");feed(4);view.stop.onClick();++record141Stage;return 0;
            case 24:
                if(controller.isBusy())return 0;controller.renderTrack(1);++record141Stage;return 0;
            case 25:
                if(controller.isBusy())return 0;
                check(controller.model().editableTrack==-1&&controller.model().rendered.isNotEmpty()&&controller.model().second.rendered.isNotEmpty(),"Both guitar tracks can remain rendered with independent saved tones");
                controller.exportAudio(folder.getChildFile("both-rendered.mp3"),true);++record141Stage;return 0;
            case 26:
                if(controller.isBusy())return 0;
                {AudioFormatManager f;f.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(f.createReaderFor(folder.getChildFile("both-rendered.mp3")));check(reader&&reader->lengthInSamples>=1572864,"Export all works when both guitars are rendered without requiring a live guitar processor");}
                view.firstHeader.onClick();record141Stage=260;return 0;
            case 260:
                if(controller.isBusy())return 0;
                check(controller.model().activeTrack==0&&controller.model().editableTrack==-1&&controller.model().rendered.isNotEmpty(),"Clicking the Guitar 1 panel selects its rendered sound without unfreezing it");view.renderFirst.onClick();record141Stage=27;return 0;
            case 27:
                if(controller.isBusy())return 0;
                check(controller.model().editableTrack==0&&controller.model().rendered.isEmpty()&&controller.model().second.rendered.isNotEmpty()&&std::abs(namGain.getValue()-9)<.001,"Unfreeze restores Guitar 1's saved tone while Guitar 2 keeps its rendered sound");
                view.timeline.setSelection(48000,72000);controller.exportAudio(folder.getChildFile("selected-mix.mp3"),true,true);++record141Stage;return 0;
            case 28:
                if(controller.isBusy())return 0;
                {AudioFormatManager f;f.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(f.createReaderFor(folder.getChildFile("selected-mix.mp3")));check(reader&&reader->lengthInSamples>=24000&&reader->lengthInSamples<30000,"Export time selection produces the chosen half-second mix, allowing MP3 encoder padding");}
                check(SHA256(folder.getChildFile(record141Solo)).toHexString()==record141SoloHash&&SHA256(folder.getChildFile(record141Take)).toHexString()==record141TakeHash,"Freeze, unfreeze and range export preserve both original dry recordings");
                check(view.guitarVolume.getX()==view.secondVolume.getX()&&view.secondVolume.getX()==view.backingVolume.getX()&&view.guitarVolume.getWidth()==view.backingVolume.getWidth(),"All three audio tracks share the same bottom-row volume placement");
                check(view.guitarHelp.getTooltip().contains("original recording")&&view.secondHelp.getTooltip().contains("Render")&&view.youtubeHelp.getTooltip().contains("never recorded")&&tonegrid::record::accent()==tonegrid::ui::tabBlue(),"Beginner help explains the original recording, Render and YouTube boundaries, using ToneGrid blue");
                takeScreenshot(evidence.getChildFile("record-two-guitars.png"));setMode(0);++record141Stage;return 0;
            case 29:
                if(controller.isBusy())return 0;
                {tonegrid::record::RecordSession saved;check(tonegrid::record::RecordSessionStore(folder).load(saved).wasOk()&&saved.editableTrack==0&&saved.second.rendered.isNotEmpty()&&saved.second.clips.size()==1&&saved.videoId.isEmpty(),"Autosave restores both guitars, edited/rendered states and the removed YouTube reference");}
                report+=String(record141Checks)+" Record141 host checks; "+(record141Ok?"ALL PASSED":"FAILURES")+"\n";return record141Ok?1:-1;
        }
        return -1;
    }
