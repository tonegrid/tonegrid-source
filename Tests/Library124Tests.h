#pragma once
#include "NamAmpTests.h"
#include "Content/Amps/NamLibraryService.h"
#include "Shared/UI/Library/NamLibraryBrowser.h"
namespace library124Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"Library124 "+name+"\n";};
    {
        Image fine(Image::RGB,1024,512,false,SoftwareImageType());
        for(int y=0;y<fine.getHeight();++y)for(int x=0;x<fine.getWidth();++x)
            fine.setPixelAt(x,y,(x+y)%2?Colours::white:Colours::black);
        tonegrid::ampui::PreviewImage preview;preview.set(fine);
        bool smooth=true;
        for(auto size:{60,75,90,120,180,300})
        {
            const auto image=preview.forPixels(size,size/2);
            for(int y=2;y<image.getHeight()-2;++y)for(int x=2;x<image.getWidth()-2;++x)
                smooth&=std::abs(int(image.getPixelAt(x,y).getRed())-127)<=2;
        }
        check(smooth,"fine grille texture is filtered without aliasing at small and fractional-DPI sizes");
        auto original=preview.forPixels(1024,512);
        check(original==fine&&original.getPixelAt(12,12)==Colours::black&&original.getPixelAt(13,12)==Colours::white,
              "enlarging after small previews restores original pixels without repeated downscaling");
        auto first=preview.forPixels(300,150);preview.set(fine);
        check(preview.forPixels(300,150)==first,"unchanged artwork and size reuse display cache");
        Image pattern(Image::RGB,1024,512,false,SoftwareImageType());
        {Graphics g(pattern);g.fillAll(Colours::black);g.setColour(Colours::white);g.fillRect(256,0,512,512);}
        preview.set(pattern);bool dpi=true;
        for(float scale:{.75f,1.f,1.25f,1.5f,2.f,3.f})
        {
            Image image(Image::RGB,roundToInt(300*scale),roundToInt(150*scale),true,SoftwareImageType());
            {Graphics g(image);g.addTransform(AffineTransform::scale(scale));preview.draw(g,{0,0,300,150});}
            dpi&=image.getPixelAt(image.getWidth()/2,image.getHeight()/2)==Colours::white
                &&image.getPixelAt(image.getWidth()/8,image.getHeight()/2)==Colours::black;
        }
        check(dpi,"preview keeps geometry and colour at 75/100/125/150/200/300 percent display scale");
        preview.set({});check(!preview.valid()&&!preview.forPixels(60,30).isValid(),"missing or removed image releases preview and retains generic fallback");
    }
    const auto temp=File::getSpecialLocation(File::tempDirectory),profile=temp.getChildFile("ToneGrid-library124-"+Uuid().toString()),root=profile.getChildFile("Amps");
    ScopeGuard cleanup{[&]{if(profile.isAChildOf(temp))profile.deleteRecursively();}};
    auto put=[&](const String& path,const String& data){auto file=root.getChildFile(path);file.getParentDirectory().createDirectory();check(file.replaceWithText(data),"write fixture "+path);return file;};
    auto json=nlohmann::json::parse(namTests::linear().toStdString());json["metadata"]={{"name","Princeton 1964"},{"modeled_by","Embedded Creator"},{"gear_make","Fender"},{"gear_model","Princeton"},{"gear_type","amp_cab"},{"tone_type","clean"},{"description","Detailed clean sound"},{"date",{{"year",2026},{"month",10},{"day",11}}}};
    const auto rich=put("01 - Clean/Princeton/Rich.nam",String(json.dump()));put("Root.nam",namTests::linear());put("01 - Clean/Princeton/Variations/Bare.nam",namTests::linear());put("TO EXCLUDE/Nested/Keep.nam",namTests::linear());put("VOX AC30 - INTERNAL USE ONLY/Keep.nam",namTests::linear());put("Broken.nam","{bad json");root.getChildFile("Future Empty Folder").createDirectory();
    rich.getSiblingFile(".tonegrid-tone3000.json").replaceWithText(R"({"source":"tone3000","creator":"Local Creator","image":"thumbnail.png","models":[{"file":"Rich.nam","name":"Local Princeton"}]})");
    Image artwork(Image::RGB,1536,768,true);{Graphics g(artwork);g.fillAll(Colours::orange);}auto jpeg=rich.getSiblingFile("thumbnail.jpg");{auto stream=jpeg.createOutputStream();check(stream&&JPEGImageFormat().writeImageToStream(artwork,*stream),"local JPEG fixture");}
    namamp::Library library(root);namamp::LibraryService service(library);namamp::Catalog catalog;Result status=Result::ok();
    auto scan=[&]{service.refresh();const auto deadline=Time::getMillisecondCounter()+10000;while(service.busy()&&Time::getMillisecondCounter()<deadline){service.poll(catalog,status);Thread::sleep(1);}check(!service.busy()&&status.wasOk(),"background scan completes");};scan();
    check(catalog.browser&&catalog.browser->captures.size()==6&&catalog.entries.size()==5,"all physical files indexed once, malformed NAM retained without making it loadable");if(!catalog.browser)return false;
    auto info=catalog.browser->captures.at("01 - Clean/Princeton/Rich.nam");
    check(info.name=="Rich"&&info.author=="Local Creator"&&info.make=="Fender"&&info.model=="Princeton"&&info.sampleRate==48000&&info.date=="2026-10-11","filename is the title despite embedded and sidecar names; sidecar author and embedded metadata retained");
    {
        tonegrid::libraryui::Capture lead;lead.path="04 - Lead/Carstens/Variations/Test.nam";lead.author="2dor";
        check(tonegrid::libraryui::captureContext(lead)=="04 - Lead by 2dor","nested and searched captures retain their physical top-level category");
        lead.path="03 - Crunch/Carstens/Test.nam";check(tonegrid::libraryui::captureContext(lead)=="03 - Crunch by 2dor","same amplifier in a different category has distinct context");
        lead.author={};check(tonegrid::libraryui::captureContext(lead)=="03 - Crunch","missing author leaves a clean category label");
        auto utf8=[](auto* raw){return String::fromUTF8(reinterpret_cast<const char*>(raw));};
        const auto emoji=utf8(u8"\U0001F170\uFE0F2\uFE0F\u20E3 ready! \u2728 Full amp \U0001F449\n\U0001F3B8\n\n\u00B112 dB, 4 \u03A9, 80 Hz, \u00A9 Creator; caf\u00E9, \u266F\n\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466 \U0001F1EB\U0001F1F7 \U0001F44D\U0001F3FD");
        check(tonegrid::libraryui::plainDescription(emoji)==utf8(u8"A2 ready!  Full amp\n\n\u00B112 dB, 4 \u03A9, 80 Hz, \u00A9 Creator; caf\u00E9, \u266F"),"emoji sequences, flags and skin modifiers removed while A2, technical units, accents and paragraphs survive");
        check(tonegrid::libraryui::plainDescription(utf8(u8"\U0001F3B8\u2728\U0001F64F")).isEmpty(),"emoji-only description disappears");
        check(tonegrid::libraryui::plainDescription(utf8(u8"\u25B6\uFE0F Start \u25C0\uFE0F Stop \u203C\uFE0F \u2139\uFE0F")).trim()=="Start  Stop","BMP emoji markers and their presentation selectors are also removed");
        auto raw=nlohmann::json::parse(namTests::linear().toStdString());raw["metadata"]={{"name","Wrong title"},{"gear_make","T3K-Null"},{"gear_model","unknown"}};
        auto view=tonegrid::libraryui::basic(rich,root);tonegrid::libraryui::embedded(view,raw);
        check(view.name=="Rich"&&view.make.isEmpty()&&view.model.isEmpty(),"unknown metadata placeholders hidden and embedded title ignored");
        view.author="Display Name";view.creatorUsername="real_creator";
        check(tonegrid::libraryui::authorSearch(view)=="https://www.tone3000.com/search?creators=real_creator","author link searches actual username rather than display name");
        view.creatorUsername={};view.author="A & B";const URL query(tonegrid::libraryui::authorSearch(view));
        check(query.getParameterValues()[0]=="A & B"&&query.getParameterNames()[0]=="q","unknown username falls back to escaped public name search");
        check(tonegrid::libraryui::tonePage("https://www.tone3000.com/tones/real-42").isNotEmpty()
            &&tonegrid::libraryui::tonePage("https://tone3000.com:443/tones/42").isNotEmpty(),"official amp page links accepted");
        for(const auto* url:{"http://tone3000.com/tones/42","https://tone3000.com.evil.test/tones/42","https://evil.test/tones/42","https://user@tone3000.com/tones/42","file:///tones/42","https://tone3000.com:444/tones/42","https://tone3000.com/search?q=42"})
            check(tonegrid::libraryui::tonePage(url).isEmpty(),"untrusted or non-pack source link hidden");
        const auto dir=profile.getChildFile("sidecar-fixture");dir.createDirectory();auto original=dir.getChildFile("First.nam");original.replaceWithText(namTests::linear());
        const auto hash=SHA256(original).toHexString();const String longDescription=String::repeatedString("A complete long description. ",250)+"END OF DESCRIPTION";
        nlohmann::json manifest={{"source","tone3000"},{"creator","Creator Name"},{"creator_username","real_creator"},{"url","https://www.tone3000.com/tones/test-42"},
            {"description",longDescription.toStdString()},{"maker","Fender"},{"model","Deluxe"},{"models",nlohmann::json::array({{{"file","First.nam"},{"name","Ignored"},{"sha256",hash.toStdString()}}})}};
        original.getSiblingFile(".tonegrid-tone3000.json").replaceWithText(String(manifest.dump()));
        const auto renamed=dir.getChildFile("Renamed.nam");check(original.moveFileTo(renamed),"rename metadata fixture");
        auto renamedInfo=tonegrid::libraryui::basic(renamed,dir);tonegrid::libraryui::sidecar(renamedInfo,renamed,hash);
        check(renamedInfo.name=="Renamed"&&renamedInfo.author=="Creator Name"&&renamedInfo.creatorUsername=="real_creator"
            &&renamedInfo.description==longDescription&&renamedInfo.sourceUrl.isNotEmpty()&&renamedInfo.make=="Fender","renaming keeps complete imported provenance via hash and updates filename title");
        auto stranger=tonegrid::libraryui::basic(renamed,dir);tonegrid::libraryui::sidecar(stranger,renamed,String::repeatedString("a",64));
        check(stranger.author.isEmpty()&&stranger.description.isEmpty(),"unrelated capture cannot inherit sibling pack metadata");
        const auto factory=profile.getChildFile("metadata-factory");factory.createDirectory();
        nlohmann::json bundled={{"schemaVersion",1},{"packs",{{"42",{{"maker","Fender"},{"model","Deluxe"},{"author","Verified creator"},{"creator_username","verified"},{"description",longDescription.toStdString()},{"url","https://www.tone3000.com/tones/test-42"}}}}},{"captures",{{hash.toStdString(),"42"}}}};
        factory.getChildFile("capture-info.json").replaceWithText(String(bundled.dump()));tonegrid::libraryui::AmpCaptureInfo verified(factory);
        auto known=tonegrid::libraryui::basic(renamed,dir);verified.apply(known,hash);
        check(known.name=="Renamed"&&known.author=="Verified creator"&&known.description==longDescription,"bundled provenance follows exact NAM bytes while title follows filename");
        auto unknown=tonegrid::libraryui::basic(renamed,dir);verified.apply(unknown,String::repeatedString("b",64));
        check(unknown.make.isEmpty()&&unknown.sourceUrl.isEmpty(),"same filename with different contents cannot inherit verified amp metadata");
        auto fake=std::make_shared<tonegrid::libraryui::Index>();fake->captures.emplace(known.path,known);namamp::Catalog longCatalog;longCatalog.browser=fake;
        tonegrid::NamLibraryBrowser longBrowser;longBrowser.setSize(1240,640);longBrowser.update(longCatalog,dir,known.path);longBrowser.reveal(known.path);
        juce::Component* details=nullptr;std::function<void(Component*)> findDetails=[&](Component* c){if(c->getComponentID()=="nam-library-details")details=c;for(auto* child:c->getChildren())findDetails(child);};findDetails(&longBrowser);
        auto* author=details?dynamic_cast<Button*>(details->findChildWithID("nam-library-author")):nullptr;
        auto* more=details?dynamic_cast<Button*>(details->findChildWithID("nam-library-more-captures")):nullptr;int links=0,selections=0;String opened;
        longBrowser.onSelect=[&](const String&){++selections;};longBrowser.openLink=[&](const URL& u){++links;opened=u.toString(true);};
        if(author&&author->onClick)author->onClick();check(links==1&&opened.contains("creators=verified")&&selections==0,"details author opens creator search without loading the amp");
        if(more&&more->onClick)more->onClick();check(links==2&&opened==known.sourceUrl&&selections==0,"More captures opens exact source amp page without changing audio");
        check(details&&details->getHeight()>1500&&details->getWidth()<=details->getParentComponent()->getWidth()&&more&&more->isVisible()&&longBrowser.createComponentSnapshot(longBrowser.getLocalBounds()).isValid(),"complete long description renders in scrolling details without truncation");
        auto* row=longBrowser.rowComponent(known.path);
        check(row&&tonegrid::libraryui::captureContext(known)=="Library root by Verified creator"&&row->createComponentSnapshot(row->getLocalBounds()).isValid()&&!row->findChildWithID("nam-library-author")&&links==2,"painted capture row shows exact plain category and author with no hyperlink");
        known.author={};known.creatorUsername={};known.sourceUrl={};known.description={};fake->captures[known.path]=known;longBrowser.update(longCatalog,dir,known.path);
        check(author&&!author->isVisible()&&more&&!more->isVisible(),"missing author and amp link are hidden rather than invented");
    }
    check(info.artwork.isValid()&&info.artwork.getPixelData()->createType()->getTypeID()==SoftwareImageType().getTypeID()&&info.artwork.getWidth()==1024&&info.artwork.getHeight()==512,"folder thumbnail decoded off UI and bounded with aspect ratio");
    check(catalog.browser->folderArtwork.at("01 - Clean/Princeton")==info.artwork
        &&catalog.browser->captures.at("01 - Clean/Princeton/Variations/Bare.nam").artwork==info.artwork,
        "folder, capture and nested variations share one high-resolution image");
    check(!catalog.browser->captures.at("Root.nam").artwork.isValid()&&!catalog.browser->captures.at("Broken.nam").error.isEmpty(),"missing artwork and malformed metadata keep safe fallback entries");
    check(catalog.browser->folders.contains("TO EXCLUDE")&&catalog.browser->folders.contains("VOX AC30 - INTERNAL USE ONLY")&&catalog.browser->folders.contains("Future Empty Folder"),"all physical categories and empty folders remain visible");
    check(catalog.browser->count("01 - Clean")==2,"category count is recursive");
    tonegrid::NamLibraryBrowser browser;browser.setFavoritesFile(profile.getChildFile("NAM/favorites.xml"));browser.update(catalog,root,"Root.nam");int loads=0,closes=0;String active="Root.nam";
    browser.onSelect=[&](const String& path){++loads;active=path;browser.selectActive(path);};browser.onClose=[&]{++closes;};
    browser.showAll();check(browser.visibleRows()==6,"All Captures is flat and unique");
    check(browser.reveal(info.path)&&browser.folder()=="01 - Clean/Princeton"&&browser.selected()==info.path&&loads==0,"opening active file navigates to containing directory without loading");
    check(browser.visibleRows()==2&&browser.rowPath(0)=="01 - Clean/Princeton/Variations","mixed folders and files, directories first");
    browser.navigate("01 - Clean");browser.setSize(1240,640);
    auto* folderRow=browser.rowComponent("01 - Clean/Princeton");
    check(folderRow&&folderRow->findChildWithID("nam-library-thumbnail")&&folderRow->findChildWithID("nam-library-thumbnail")->isVisible(),"amp folder row displays its thumbnail");
    browser.reveal(info.path);
    browser.setSearch("Local Creator");check(browser.visibleRows()==1&&browser.rowPath(0)==info.path,"global author search reaches nested capture");browser.setSearch("Fender clean");check(browser.visibleRows()==1,"global make/tone search");browser.setSearch("Variations");check(browser.visibleRows()==2,"global search includes folders and nested bare NAM");
    browser.setSearch("Root");check(browser.visibleRows()==1,"metadata-free filename search");browser.reveal(info.path);
    const auto before=loads;browser.toggleFavorite(info.path);check(browser.isFavorite(info.path)&&loads==before&&browser.selected()==info.path,"star persists without loading or navigation");
    {tonegrid::NamLibraryBrowser restarted;restarted.setFavoritesFile(profile.getChildFile("NAM/favorites.xml"));restarted.update(catalog,root,{});restarted.showFavorites();check(restarted.visibleRows()==1&&restarted.isFavorite(info.path),"favorite survives a new browser session");}
    browser.activate(info.path,false);browser.handleUpdateNowIfNeeded();check(loads==before+1&&closes==0&&active==info.path,"single activation loads and stays open");browser.activate(info.path,true);browser.handleUpdateNowIfNeeded();check(loads==before+2&&closes==1,"double activation uses same callback and requests close");
    check(!browser.reveal("outside/missing.nam")&&browser.visibleRows()==6&&active==info.path,"external/unindexed active path opens All without changing sound");
    scan();check(service.parsedCount()==0&&service.reusedCount()==6,"warm refresh reuses NAM parsing");
    jpeg.deleteFile();scan();browser.update(catalog,root,active);check(!browser.capture(info.path)->artwork.isValid(),"Refresh notices deleted thumbnail without NAM content changes");
    put("01 - Clean/Princeton/New.nam",namTests::linear());scan();browser.update(catalog,root,active);check(browser.totalCaptures()==7&&browser.find("01 - Clean/Princeton/New.nam"),"Refresh discovers new capture");
    const auto favoritePath=info.path;rich.deleteFile();scan();browser.update(catalog,root,active);browser.showFavorites();check(browser.visibleRows()==0&&browser.selected().isEmpty(),"deleted favorite handled safely without substitution");
    const auto bundled=profile.getChildFile("FactoryAmps/Artwork");bundled.createDirectory();
    {auto stream=bundled.getChildFile("test.png").createOutputStream();check(stream&&PNGImageFormat().writeImageToStream(artwork,*stream),"bundled PNG fixture");}
    bundled.getChildFile("manifest.json").replaceWithText(R"({"schemaVersion":1,"folders":{"Princeton":"test.png"},"captures":{"Root.nam":"test.png","Broken.nam":"../outside.png"}})");
    namamp::LibraryService bundledService(library,bundled.getParentDirectory());namamp::Catalog bundledCatalog;bundledService.refresh();
    const auto deadline=Time::getMillisecondCounter()+10000;
    while(bundledService.busy()&&Time::getMillisecondCounter()<deadline){bundledService.poll(bundledCatalog,status);Thread::sleep(1);}
    check(bundledCatalog.browser&&bundledCatalog.browser->captures.at("Root.nam").artwork.isValid()
        &&bundledCatalog.browser->folderArtwork.at("01 - Clean/Princeton").isValid()
        &&!bundledCatalog.browser->captures.at("Broken.nam").artwork.isValid(),"bundled exact filename and folder artwork resolve safely without seeding NAM files");
    // Worker scanning is independent of the processor: deterministic output is
    // checked while a real NAM processor renders, without opening audio hardware.
    namamp::Entry entry;check(library.read(root.getChildFile("Root.nam"),entry).wasOk(),"reference NAM valid");
    AudioProcessorGraph renderingGraph;auto loaded=amp::loadNamAmp(library,{entry.identity,0,0});auto node=loaded.source?loaded.source->addToGraph(renderingGraph):nullptr;auto* processor=node?node->getProcessor():nullptr;
    if(processor)
    {
        processor->prepareToPlay(48000,128);AudioBuffer<float> audio(2,128);MidiBuffer midi;service.refresh();bool finite=true;
        for(int block=0;block<2000;++block){for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)audio.setSample(ch,i,.03f);processor->processBlock(audio,midi);for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)finite&=std::abs(audio.getSample(ch,i)-.03f)<1.e-6f;service.poll(catalog,status);}
        check(finite,"2000 audio blocks remain sample-identical during background indexing");
    }
    else check(false,"reference processor created");
    report+=String(checks)+" Library124 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
