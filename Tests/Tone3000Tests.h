#pragma once
#include "Tone3000.h"
#include "Content/Amps/FactoryAmps.h"
#include "NamAmpTests.h"
#include "Shared/UI/Library/NamLibraryBrowser.h"
namespace tone3000Tests
{
using namespace juce;using namespace tonegrid::t3k;
inline String parameter(const String& address,const String& key)
{const URL url(address);const auto index=url.getParameterNames().indexOf(key);return index<0?String{}:url.getParameterValues()[index];}
inline String callbackFor(const Tone3000OAuthSession& session,String extra="code=test-code&tone_id=42")
{return String(redirectUri)+"?state="+parameter(session.authorizationUrl(),"state")+"&"+extra;}
class Mock final:public Transport
{
public:
    std::vector<Request> calls;std::function<Response(const Request&)> handler;
    Response perform(const Request& r,const std::atomic<bool>&) override{calls.push_back(r);return handler(r);}
};
inline Response bytes(const MemoryBlock& data,int status=200){return {status,data,{}};}
inline Response json(const var& value,int status=200){const auto text=JSON::toString(value,true);return bytes(MemoryBlock(text.toRawUTF8(),text.getNumBytesAsUTF8()),status);}
inline var tone(String format="nam",String gear="amp-cab")
{return object({{"id",42},{"title","Deluxe / Studio"},{"format",format},{"gear",gear},{"license","cc-by"},{"url","https://www.tone3000.com/tones/42"},
    {"description","Full original Tone3000 description"},{"gear_make","Fender"},{"gear_model","Deluxe"},
    {"user",object({{"username","creator"},{"display_name","Creator Name"}})},{"user_id","creator-id"},{"images",var(Array<var>{"https://images.example/42.png"})}});}
inline var model(int id,String generation="2",int toneId=42)
{return object({{"id",id},{"tone_id",toneId},{"name","CON.nam"},{"architecture_version",generation},{"model_url","https://www.tone3000.com/api/v1/models/"+String(id)+"/download"}});}
inline bool run(String& report)
{
    bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("Tone3000 ")+name+"\n";};
    check(challengeFor("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk")=="E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM","RFC 7636 S256 test vector");
    Tone3000OAuthSession auth,second;auto address=auth.authorizationUrl();
    check(auth.codeVerifier().length()==86&&auth.codeVerifier().containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"),"secure verifier length and alphabet");
    check(parameter(address,"state").length()==43&&parameter(address,"state")!=parameter(second.authorizationUrl(),"state")&&auth.codeVerifier()!=second.codeVerifier(),"fresh secure state and verifier for every session");
    check(parameter(address,"client_id")==clientId&&parameter(address,"redirect_uri")==redirectUri&&parameter(address,"response_type")=="code"&&parameter(address,"code_challenge_method")=="S256","publishable client, exact callback and PKCE authorization");
    check(parameter(address,"client_id")=="t3k_pub_zbn539j1yH1giZnQWE0VhLuc9izCIPxp","client matches the independently confirmed user key, including digit 5");
    {Tone3000OAuthSession heads("amp_amp-cab"),cabs("cab");
     check(parameter(heads.authorizationUrl(),"gears")=="amp_amp-cab"&&parameter(heads.authorizationUrl(),"format")=="nam"&&parameter(heads.authorizationUrl(),"architecture")=="2","combined amplifier selector allows Head and Amp+Cab");
     check(parameter(cabs.authorizationUrl(),"gears")=="cab"&&parameter(cabs.authorizationUrl(),"format")=="ir"&&parameter(cabs.authorizationUrl(),"architecture").isEmpty(),"cabinet selector requests IR cabinets without NAM architecture filtering");}
    check(authorizationFailure(json(object({{"error","invalid_client"}}),400),{}).contains("public key"),"HTTP 400 invalid_client is a client configuration error, not a connection failure");
    check(authorizationFailure(json(object({{"error","redirect_uri_mismatch"}}),400),{}).contains("callback"),"rejected callback identifies redirect configuration");
    check(authorizationFailure({},{}).contains("connection"),"unreachable server offers connection guidance");
    check(authorizationFailure({429,{},{}},{}).contains("Wait")&&authorizationFailure({503,{},{}},{}).contains("HTTP 503"),"rate limiting and server failures keep their real meaning");
    #if JUCE_WINDOWS
    const String browserCode="WebView2 code 14";
#else
    const String browserCode="browser code 14";
#endif
    check(authorizationFailure({307,{},{}},"Error code: 14").contains(browserCode),"reachable authorization distinguishes an embedded browser failure using the native platform name");
    check(!authorizationFailure(json(object({{"error","other"},{"error_description","secret-test-token"}}),400),"https://example.com?code=secret-test-code").contains("secret-test"),"diagnostic text never echoes credentials or arbitrary server descriptions");
    check(parameter(address,"prompt")=="select_tone"&&parameter(address,"gears")=="amp-cab"&&parameter(address,"format")=="nam"&&parameter(address,"architecture")=="2"&&parameter(address,"menubar")=="true"&&parameter(address,"preview")=="true","exact Select filters");
    check(!address.contains("secret")&&!address.contains("legacy")&&!address.contains(auth.codeVerifier()),"authorization contains no client secret or verifier");
    check(auth.accept("https://example.com/callback?code=bad").kind==Callback::unrelated,"ignore other navigation without consuming OAuth session");
    const auto accepted=auth.accept(callbackFor(auth));check(accepted.kind==Callback::selected&&accepted.toneId=="42"&&accepted.code=="test-code","validated callback selection");
    check(auth.accept(callbackFor(auth)).kind==Callback::failed,"callback replay rejected");
    for(const auto* bad:{"", "state=wrong", "code=a&tone_id=-2", "code=a&tone_id=42&state=duplicate"})
    {Tone3000OAuthSession session;String callback=callbackFor(session,bad);check(session.accept(callback).kind==Callback::failed,String("reject invalid callback ")+bad);}
    {Tone3000OAuthSession session;check(session.accept(callbackFor(session,"canceled=true")).kind==Callback::cancelled,"documented Select cancellation");}
    {Tone3000OAuthSession session;check(session.accept(callbackFor(session,"error=access_denied")).kind==Callback::failed,"login refusal reported");}
    {Tone3000OAuthSession session;check(session.accept(callbackFor(session).replace("localhost:3001","localhost:3002")).kind==Callback::unrelated,"exact callback port enforced");}
    {Tone3000OAuthSession session;check(session.accept(callbackFor(session)+"#state=wrong").kind==Callback::failed,"callback fragment rejected");}
    for(const auto* name:{"CON", "aux.txt", "COM1.nam", "LPT9", "...", "../escape", "a/b:c*?", "A. "})
    {auto sanitized=safeName(name,"capture");check(sanitized.isNotEmpty()&&!sanitized.containsAnyOf("/\\<>:\"|?*")&&!sanitized.endsWithChar('.')&&!sanitized.endsWithChar(' '),String("portable filename ")+name);}
    check(safeName("CON","fallback")=="_CON"&&safeName("","fallback")=="fallback"&&safeName(String::repeatedString("a",200),"x").length()==80,"reserved, empty and bounded names");
    check(safeHttps("https://www.tone3000.com/api/v1/models/1")&&!safeHttps("http://example.com/a")&&!safeHttps("https://user:pass@example.com/a")&&!safeHttps("file:///a"),"HTTPS download URL boundary");
    check(tone3000Origin("https://www.tone3000.com/api/models/1/download")&&tone3000Origin("https://tone3000.com:443/file")&&!tone3000Origin("https://www.tone3000.com.evil.example/file")&&!tone3000Origin("https://images.example/file")&&!tone3000Origin("https://www.tone3000.com:8443/file"),"bearer restricted to official HTTPS origins, including model download paths outside v1");
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-t3k-test-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};const auto library=namamp::libraryForApplicationData(root);
    const auto collision=root.getChildFile("commit-existing"),source=root.getChildFile("commit-staging");root.createDirectory();source.createDirectory();collision.replaceWithText("user-file");
    check(!commitPack(source,collision)&&source.isDirectory()&&collision.loadFileAsString()=="user-file","atomic commit refuses existing user file without deleting it");
    MemoryBlock nam;const auto namText=namTests::linear();nam.append(namText.toRawUTF8(),namText.getNumBytesAsUTF8());
    Image image(Image::RGB,12,10,true);Graphics g(image);g.fillAll(Colours::orange);MemoryOutputStream encoded;PNGImageFormat().writeImageToStream(image,encoded);
    std::atomic<bool> cancelled{false};Mock io;
    const auto normal=[&](const Request& r)->Response
    {
        if(r.url.endsWith("oauth/token"))return json(object({{"access_token","mock-bearer"},{"token_type","bearer"},{"expires_in",3600}}));
        if(r.url.contains("tones/42"))return json(tone());
        if(r.url.contains("models?")){const int page=parameter(r.url,"page").getIntValue();return json(object({{"page",page},{"total_pages",2},{"data",var(page==1?Array<var>{model(101),model(999,"1")}:Array<var>{model(102),model(998,"custom")})}}));}
        if(r.url.contains("/download"))return bytes(nam);
        if(r.url=="https://images.example/42.png")return bytes(encoded.getMemoryBlock());return bytes({},404);
    };
    io.handler=normal;Tone3000Client client(io,cancelled);client.exchange(accepted,second.codeVerifier());const auto pack=client.getPack("42");
    check(pack.models.size()==2&&pack.models[0].id=="101"&&pack.models[1].id=="102"&&pack.creator=="Creator Name","all pages, A2 only, creator metadata");
    check(io.calls[0].method=="POST"&&io.calls[0].token.isEmpty()&&parameter("https://example.com/?"+io.calls[0].body,"grant_type")=="authorization_code"&&parameter("https://example.com/?"+io.calls[0].body,"code_verifier")==second.codeVerifier(),"PKCE token exchange form without secret");
    check(io.calls[1].token=="mock-bearer"&&parameter(io.calls[2].url,"architecture")=="2"&&parameter(io.calls[3].url,"page")=="2","bearer metadata and paginated A2 request");
    Tone3000DownloadManager downloader(library);const auto imported=downloader.import(client,pack,cancelled);
    check(imported.ok&&imported.models==2&&imported.folder.getParentDirectory()==library.getChildFile("TONE3000")&&imported.folder.getFileName()=="Deluxe _ Studio","pack storage without creator subfolder");
    const auto manifest=JSON::parse(imported.folder.getChildFile(manifestName));auto* entries=manifest["models"].getArray();
    check(textField(manifest,"source")=="tone3000"&&idField(manifest,"tone_id")=="42"&&textField(manifest,"license")=="cc-by"&&textField(manifest,"creator")=="Creator Name"&&textField(manifest,"downloaded_at").isNotEmpty()&&entries&&entries->size()==2,"sidecar source, ids, creator, license, timestamp, model list");
    check(pack.creatorUsername=="creator"&&pack.description=="Full original Tone3000 description"&&pack.maker=="Fender"&&pack.model=="Deluxe"
        &&textField(manifest,"creator_username")=="creator"&&textField(manifest,"description")==pack.description&&textField(manifest,"maker")=="Fender"&&textField(manifest,"model")=="Deluxe", "import preserves full pack description, creator username and available gear fields in local manifest");
    bool exact=true;StringArray files;for(const auto& file:imported.folder.findChildFiles(File::findFiles,false,"*.nam")){MemoryBlock content;file.loadFileAsData(content);exact&=content==nam;files.add(file.getFileName());}
    check(exact&&files.size()==2&&files[0]!=files[1],"every model bytes preserved and duplicate names separated");
    check(io.calls.size()==7&&io.calls[4].token=="mock-bearer"&&io.calls[5].token=="mock-bearer"&&io.calls[6].token.isEmpty(),"download ALL A2 models plus optional unauthenticated image");
    Mock pedalIo;pedalIo.handler=[&](const Request& r){return r.url.contains("tones/42")?json(tone("nam","pedal")):normal(r);};
    Tone3000Client pedalClient(pedalIo,cancelled,"pedal");pedalClient.exchange(accepted,second.codeVerifier());const auto pedalPack=pedalClient.getPack("42");
    Tone3000DownloadManager pedalDownloader(root.getChildFile("Pedal downloads"),"pedal");const auto pedalImport=pedalDownloader.import(pedalClient,pedalPack,cancelled);
    check(pedalImport.ok&&pedalImport.models==2&&pedalImport.entries.size()==2&&JSON::parse(pedalImport.folder.getChildFile(manifestName))["gear"].toString()=="pedal","pedal pack downloads every A2 variation without a single-capture chooser");
    const auto selected=imported.folder.getChildFile(files[0]);check(localThumbnail(selected).isValid(),"local shared thumbnail loads offline for a listed model");
    check(!localThumbnail(imported.folder.getChildFile("unlisted.nam")).isValid(),"manifest image not assigned to unrelated model");
    namamp::Library local(library);namamp::Catalog catalog;check(local.refresh(catalog).wasOk()&&catalog.entries.size()==2&&catalog.failures.empty(),"existing recursive library sees complete valid imported models");
    tonegrid::NamLibraryBrowser browser;browser.update(catalog,library,{});check(browser.find("TONE3000/"+imported.folder.getFileName()+"/"+files[0])&&browser.selected().isEmpty(),"nested Tone3000 folder tree with no auto-selection");
    int clicks=0;browser.onGetCaptures=[&]{++clicks;};browser.captures.onClick();check(clicks==1&&browser.captures.isVisible()&&browser.captures.getParentComponent()==browser.popupHeaderAction(),"Local NAM Library header/CTA route above unchanged tree");
    check(browser.captures.createComponentSnapshot(browser.captures.getLocalBounds()).isValid()&&ImageCache::getFromMemory(ToneGridApplicationAssets::ToneGrid512_png,ToneGridApplicationAssets::ToneGrid512_pngSize).getWidth()==512,"logo CTA and supplied native icon resource render");
    auto again=downloader.import(client,pack,cancelled);check(again.ok&&again.folder!=imported.folder&&selected.existsAsFile(),"re-import uses another safe pack folder without overwrite");
    const auto before=library.findChildFiles(File::findFiles,true,"*.nam").size();
    const auto failure=[&](const std::function<Response(const Request&)>& handler,const String& name)
    {io.handler=handler;auto result=downloader.import(client,pack,cancelled);check(!result.ok&&library.findChildFiles(File::findFiles,true,"*.nam").size()==before&&root.getChildFile("ToneGrid/Tone3000/Imports").findChildFiles(File::findDirectories,false,"*").isEmpty(),name);};
    failure([&](const Request& r){return r.url.contains("/102/")?bytes({},503):normal(r);},"failed second model rolls back entire pack and cleans staging");
    failure([&](const Request& r){return r.url.contains("/102/")?bytes(MemoryBlock("bad",3)):normal(r);},"invalid NAM rejected before library commit");
    failure([&](const Request& r){if(r.url.contains("/102/"))cancelled=true;return normal(r);},"cancel during model download rolls back all files");cancelled=false;
    io.handler=[&](const Request& r){return r.url.startsWith("https://images")?bytes({},404):normal(r);};
    const auto noImage=downloader.import(client,pack,cancelled);check(noImage.ok&&noImage.message.contains("Thumbnail unavailable")&&!localThumbnail(noImage.folder.getChildFile(files[0])).isValid(),"optional image failure keeps complete pack with explicit fallback notice");
    auto invalidManifest=JSON::parse(imported.folder.getChildFile(manifestName));invalidManifest.getDynamicObject()->setProperty("image","../escape.png");imported.folder.getChildFile(manifestName).replaceWithText(JSON::toString(invalidManifest));check(!localThumbnail(selected).isValid(),"thumbnail path traversal rejected");
    const auto rejectMetadata=[&](var payload,const String& name)
    {io.handler=[&](const Request& r){return r.url.contains("tones/42")?json(payload):normal(r);};bool rejected=false;try{client.getPack("42");}catch(const Failure&){rejected=true;}check(rejected,name);};
    rejectMetadata(tone("ir"),"IR tone rejected");rejectMetadata(tone("nam","pedal"),"pedal tone rejected");
    io.handler=[&](const Request& r){return r.url.contains("models?")?json(object({{"page",1},{"total_pages",1},{"data",var(Array<var>{model(101,"2",43)})}})):normal(r);};
    bool rejected=false;try{client.getPack("42");}catch(const Failure&){rejected=true;}check(rejected,"model from a different tone rejected");
    io.handler=[&](const Request& r){return r.url.contains("models?")?json(object({{"page",1},{"total_pages",1},{"data",var(Array<var>{model(101,"1")})}})):normal(r);};
    rejected=false;try{client.getPack("42");}catch(const Failure&){rejected=true;}check(rejected,"A1-only pack rejected");
    {
        Mock mixed;bool isHead=true;mixed.handler=[&](const Request& r){return r.url.contains("tones/42")?json(tone("nam",isHead?"amp":"amp-cab")):normal(r);};
        Tone3000Client mixedClient(mixed,cancelled,"amp_amp-cab");mixedClient.exchange(accepted,second.codeVerifier());
        check(mixedClient.getPack("42").gear=="amp","combined amplifier client accepts amp heads and preserves actual gear");isHead=false;
        check(mixedClient.getPack("42").gear=="amp-cab","combined amplifier client accepts complete captures");
        WavAudioFormat wav;const auto probe=root.getChildFile("probe.wav");auto stream=probe.createOutputStream();
        std::unique_ptr<AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));AudioBuffer<float> impulse(1,128);impulse.clear();impulse.setSample(0,0,1.f);if(writer){writer->writeFromAudioSampleBuffer(impulse,0,128);writer.reset();}
        MemoryBlock wavBytes;probe.loadFileAsData(wavBytes);Mock cabIo;
        const auto irReply=[&](const Request& r)->Response{
            if(r.url.contains("tones/42"))return json(tone("ir","cab"));
            if(r.url.contains("models?"))return json(object({{"page",1},{"total_pages",1},{"data",var(Array<var>{model(201,"")})}}));
            if(r.url.contains("/download"))return bytes(wavBytes);return normal(r);
        };cabIo.handler=irReply;Tone3000Client cabClient(cabIo,cancelled,"cab");cabClient.exchange(accepted,second.codeVerifier());const auto cabPack=cabClient.getPack("42");
        const auto cabinetRoot=root.getChildFile("ToneGrid/CabinetIR");CabinetDownloadManager cabDownload(cabinetRoot);const auto result=cabDownload.import(cabClient,cabPack,cancelled);
        check(result.ok&&result.models==1&&result.folder.getChildFile(".tonegrid-cabinet.json").existsAsFile(),"cabinet OAuth metadata and WAV import commit a complete private pack");
        const auto irFiles=result.folder.findChildFiles(File::findFiles,false,"*.wav");MemoryBlock importedWav;if(!irFiles.isEmpty())irFiles[0].loadFileAsData(importedWav);
        check(importedWav==wavBytes&&result.entries.empty(),"IR imports preserve WAV bytes and never enter the NAM model list");
        bool noArchitecture=true;for(const auto& request:cabIo.calls)if(request.url.contains("tones/42")||request.url.contains("models?"))noArchitecture&=!request.url.contains("architecture=");check(noArchitecture,"IR metadata requests never add NAM architecture filters");
        cabIo.handler=[&](const Request& r){return r.url.contains("/download")?bytes(nam):irReply(r);};const auto bad=cabDownload.import(cabClient,cabPack,cancelled);
        check(!bad.ok&&cabinetRoot.findChildFiles(File::findFiles,true,"*.wav").size()==1,"malformed cabinet download rolls back staging and preserves existing IRs");
    }
    io.handler=[](const Request&){return bytes({},401);};rejected=false;try{client.exchange(accepted,second.codeVerifier());}catch(const Failure&){rejected=true;}check(rejected,"token refusal is nonfatal and prevents import");
    io.handler=[](const Request&){return bytes({},0);};rejected=false;try{client.getPack("42");}catch(const Failure&){rejected=true;}check(rejected,"offline metadata failure is nonfatal");
    check(library.findChildFiles(File::findFiles,true,"*.nam").size()==before+2,"error paths preserved all existing imported NAM files");
    {
        const auto shipped=root.getChildFile("artwork-factory");shipped.createDirectory();shipped.getChildFile("test.nam").replaceWithData(nam.getData(),nam.getSize());
        shipped.getChildFile("thumbnail.png").replaceWithData(encoded.getMemoryBlock().getData(),encoded.getDataSize());
        const String oldText="previous-shipped-thumbnail";const auto oldHash=SHA256(oldText.toRawUTF8(),oldText.getNumBytesAsUTF8()).toHexString();
        const auto inventory=object({{"schemaVersion",2},{"ampCount",1},{"amps",var(Array<var>{object({{"filename","test.nam"},{"sha256",SHA256(nam).toHexString()}})})},{"assets",var(Array<var>{object({{"filename","thumbnail.png"},{"sha256",SHA256(encoded.getMemoryBlock()).toHexString()},{"previousSha256",var(Array<var>{oldHash})}})})}});
        shipped.getChildFile("manifest.json").replaceWithText(JSON::toString(inventory,false));
        for(int mode=0;mode<3;++mode){const auto target=root.getChildFile("artwork-case-"+String(mode)+"/Amps");target.createDirectory();const auto image=target.getChildFile("thumbnail.png");
            if(mode<2)image.replaceWithText(mode==0?oldText:String("personal-artwork"));const auto result=tonegrid::factory::seed(shipped,target,cancelled);
            check(result.wasOk()&&(mode==1?image.loadFileAsString()=="personal-artwork":SHA256(image).toHexString()==SHA256(encoded.getMemoryBlock()).toHexString()),mode==0?"known former factory thumbnail is replaced by exact prior hash":mode==1?"personal thumbnail edit survives the artwork update":"missing factory thumbnail is installed on first launch");}
    }
    report+=String(checks)+" Tone3000 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
