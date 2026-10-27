    bool verifyLibrary124(String& result,const File& screenshots,bool restarted)
    {
        using namespace tonegrid;bool ok=true;auto check=[&](bool pass,const String& name){ok&=pass;result+=String(pass?"PASS ":"FAIL ")+"Library124 host "+name+"\n";};
        check(ready()&&ampSource->type()==amp::SourceType::Nam,"isolated NAM host ready");if(!ready())return false;
        const String nested="01 - Clean/Princeton 1964/Princeton.nam";
        if(restarted)
        {
            showNamChooser();check(namBrowser.isFavorite(nested)&&namBrowser.folder()=="01 - Clean/Princeton 1964"&&namBrowser.selected()==nested,"restart restores favorite and focuses actual active capture");
            if(namBrowserDialog)delete namBrowserDialog.getComponent();return ok;
        }
        auto payload=nlohmann::json::parse(namTests::linear(.75f).toStdString());payload["metadata"]={{"name","1964 Fender Princeton Clean"},{"modeled_by","Synthetic test fixture"},{"gear_make","Fender"},{"gear_model","Princeton"},{"gear_type","amp_cab"},{"tone_type","Clean"}};
        const auto file=namLibrary.folder().getChildFile(nested);file.getParentDirectory().createDirectory();file.replaceWithText(String(payload.dump()));
        const auto extra=file.getSiblingFile("Variations/Bright.nam");extra.getParentDirectory().createDirectory();extra.replaceWithText(namTests::linear());
        const auto missing=namLibrary.folder().getChildFile("TO EXCLUDE/Unavailable.nam");missing.getParentDirectory().createDirectory();missing.replaceWithText(namTests::linear());
        {Image image(Image::RGB,1024,683,true,SoftwareImageType());Graphics g(image);g.fillAll(Colours::orange);auto stream=file.getSiblingFile("thumbnail.jpg").createOutputStream();JPEGImageFormat().writeImageToStream(image,*stream);}
        refreshNamLibrary();awaitNamRefreshForTest();const auto* before=ampSource.get();const auto gain=dynamic_cast<amp::NamAmp*>(ampSource.get())->state();const auto effects=tonegrid::output::encode(outputSettings).toString();const auto graphSize=graph.getNumNodes();
        showNamChooser();check(namBrowserDialog&&namBrowser.selected()==gain.model.file,"opens on current active NAM");
        namBrowser.activate(nested,false);namBrowser.handleUpdateNowIfNeeded();auto* active=dynamic_cast<amp::NamAmp*>(ampSource.get());
        check(active&&active->state().model.file==nested&&ampSource.get()!=before&&namBrowserDialog&&namBrowserDialog->isVisible(),"single click loads via canonical loader and keeps real popup open");
        check(active&&active->state().gainDb==gain.gainDb&&active->state().volumeDb==gain.volumeDb&&graph.getNumNodes()==graphSize&&tonegrid::output::encode(outputSettings).toString()==effects,"loading preserves GAIN/VOLUME, output settings and graph topology");
        check(namBrowser.capture(nested)&&namBrowser.capture(nested)->artwork.isValid()&&ampPanel.presentedArtwork()==namBrowser.capture(nested)->artwork,"rack, capture and details share active local folder artwork");
        namBrowser.navigate("01 - Clean");auto* row=namBrowser.rowComponent("01 - Clean/Princeton 1964");auto* picture=row?dynamic_cast<ampui::Thumbnail*>(row->findChildWithID("nam-library-thumbnail")):nullptr;
        check(picture&&!picture->usesFallback(),"amp folder also displays local JPEG");namBrowser.reveal(nested);
        const auto* unchanged=ampSource.get();namBrowser.toggleFavorite(nested);check(ampSource.get()==unchanged&&namBrowser.isFavorite(nested),"favorite never reloads amp");
        delete namBrowserDialog.getComponent();showNamChooser();check(namBrowser.folder()=="01 - Clean/Princeton 1964"&&namBrowser.selected()==nested&&namBrowser.rowComponent(nested),"reopening navigates and scrolls active capture into view");
        for(float scale:{1.f,1.25f,1.5f,2.f})
        {
            const auto image=namBrowserDialog->createComponentSnapshot(namBrowserDialog->getLocalBounds(),true,scale);MemoryOutputStream bytes;
            check(image.isValid()&&PNGImageFormat().writeImageToStream(image,bytes)&&screenshots.getChildFile("library124-"+String(roundToInt(scale*100))+".png").replaceWithData(bytes.getData(),bytes.getDataSize()),"render at "+String(scale*100)+" percent");
        }
        const auto fullBounds=namBrowserDialog->getBounds();namBrowserDialog->setSize(880,580);
        auto* search=namBrowser.popupHeaderAction()->findChildWithID("nam-library-search");
        check(search&&search->isShowing()&&search->getWidth()>100&&namBrowserDialog->getLocalBounds().contains(namBrowserDialog->getLocalArea(search,search->getLocalBounds())),"small monitor keeps global search visible in a stacked header");
        namBrowserDialog->setBounds(fullBounds);
        namBrowser.setSearch("Princeton");check(namBrowser.visibleRows()>=3,"search traverses nested capture and folder context");namBrowser.reveal(nested);
        missing.deleteFile();namBrowser.activate("TO EXCLUDE/Unavailable.nam",true);namBrowser.handleUpdateNowIfNeeded();check(ampSource.get()==unchanged&&namBrowserDialog&&namBrowserDialog->isVisible(),"missing-file double click keeps previous amp and popup visible");
        namBrowser.activate(nested,true);namBrowser.handleUpdateNowIfNeeded();check(ampSource.get()==unchanged&&namBrowserDialog&&!namBrowserDialog->isVisible(),"double click reuses canonical selection and closes only after success");
        if(namBrowserDialog)delete namBrowserDialog.getComponent();return ok;
    }
