#pragma once
#include "Platform/BrowserPlatform.h"
#include "Content/Amps/NamAmpLibrary.h"
#include "CabinetProcessor.h"
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <set>

namespace tonegrid::t3k
{
using namespace juce;
inline constexpr const char* clientId="t3k_pub_zbn539j1yH1giZnQWE0VhLuc9izCIPxp";
inline constexpr const char* redirectUri="http://localhost:3001/callback";
inline constexpr const char* api="https://www.tone3000.com/api/v1/";
inline constexpr const char* manifestName=".tonegrid-tone3000.json";
inline bool gearAllowed(const String& requested,const String& actual)
{return requested==actual||(requested=="amp_amp-cab"&&(actual=="amp"||actual=="amp-cab"));}
inline String base64url(const void* data,size_t bytes)
{return Base64::toBase64(data,bytes).replaceCharacter('+','-').replaceCharacter('/','_').removeCharacters("=");}
inline String challengeFor(const String& verifier)
{const auto raw=SHA256(verifier.toRawUTF8(),verifier.getNumBytesAsUTF8()).getRawData();return base64url(raw.getData(),raw.getSize());}
String entropy(size_t count);
inline bool positiveId(const String& value)
{return value.isNotEmpty()&&value.length()<=16&&value.containsOnly("0123456789")&&value.getLargeIntValue()>0;}
inline String escaped(const String& value){return URL::addEscapeChars(value,true);}
inline String form(std::initializer_list<std::pair<String,String>> fields)
{String out;for(const auto& field:fields){if(out.isNotEmpty())out+="&";out+=escaped(field.first)+"="+escaped(field.second);}return out;}
struct Callback
{enum Kind{unrelated,selected,cancelled,failed};Kind kind=unrelated;String code,toneId,error;};
class Tone3000OAuthSession
{
public:
    explicit Tone3000OAuthSession(String gear="amp-cab"):verifier(entropy(64)),state(entropy(32)),requestedGear(std::move(gear)){}
    String authorizationUrl() const
    {
        if(verifier.isEmpty()||state.isEmpty())return {};
        return String(api)+"oauth/authorize?"+form({{"client_id",clientId},{"redirect_uri",redirectUri},{"response_type","code"},
            {"code_challenge",challengeFor(verifier)},{"code_challenge_method","S256"},{"state",state},
            {"prompt","select_tone"},{"gears",requestedGear},{"format",requestedGear=="cab"?"ir":"nam"},{"menubar","true"},{"preview","true"}})+(requestedGear=="cab"?String{}:String("&architecture=2"));
    }
    Callback accept(const String& address)
    {
        if(address.upToFirstOccurrenceOf("?",false,false)!=redirectUri)return {};
        Callback result;result.kind=Callback::failed;
        if(consumed){result.error="This Tone3000 callback has already been used.";return result;}
        consumed=true;
        const URL url(address);const auto names=url.getParameterNames(),values=url.getParameterValues();std::set<String> seen;
        if(address.containsChar('#')){result.error="Invalid Tone3000 callback.";return result;}
        for(const auto& name:names)if(!seen.insert(name).second){result.error="Duplicate Tone3000 callback parameters.";return result;}
        const auto value=[&](const String& key){const auto index=names.indexOf(key);return index<0?String{}:values[index];};
        const auto returnedState=value("state");unsigned difference=static_cast<unsigned>(returnedState.length()^state.length());
        for(int i=0;i<state.length();++i)difference|=static_cast<unsigned>(state[i]^(i<returnedState.length()?returnedState[i]:0));
        if(state.isEmpty()||difference!=0){result.error="Tone3000 authentication state did not match. Please try again.";return result;}
        if(value("canceled")=="true"){result.kind=Callback::cancelled;return result;}
        if(value("error").isNotEmpty()){result.error="Tone3000 login was not authorized. Please try again.";return result;}
        if(!positiveId(value("tone_id"))||value("code").isEmpty()||value("code").length()>4096)
        {result.error="Tone3000 did not return a valid selection.";return result;}
        result.kind=Callback::selected;result.code=value("code");result.toneId=value("tone_id");return result;
    }
    const String& codeVerifier() const{return verifier;}
private:String verifier,state,requestedGear;bool consumed=false;
};
struct Request{String url,method="GET",body,token;size_t maximum=8*1024*1024;bool followRedirects=true;};
struct Response{int status=0;MemoryBlock bytes;String error;};
// Only fixed messages and status numbers reach the UI; never echo OAuth URLs or credentials.
inline String authorizationFailure(const Response& response,const String& browserError)
{
    if(response.status==0)return "Tone3000 could not be reached. Check your connection, then retry or return to your local NAM library.";
    const auto json=JSON::parse(response.bytes.toString());const auto code=json["error"].toString();
    if(code=="invalid_client")return "Tone3000 does not recognize this application's public key. Check the ToneGrid client configuration.";
    if(code=="invalid_redirect_uri"||code=="redirect_uri_mismatch")return "Tone3000 rejected the application's callback address. Check the registered redirect URI.";
    if(response.status==429)return "Tone3000 is receiving too many requests. Wait a moment and retry.";
    if(response.status>=400)return "Tone3000 returned HTTP "+String(response.status)+". Retry or return to your local NAM library.";
    const auto detail=tonegrid::platform::browserErrorDetail(browserError);
    return "Tone3000 is reachable, but the embedded browser could not open the page"+detail+". Retry or return to your local NAM library.";
}
class Transport
{public:virtual ~Transport()=default;virtual Response perform(const Request&,const std::atomic<bool>& cancelled)=0;};
struct Failure{String message;};
inline void require(bool condition,const String& message){if(!condition)throw Failure{message};}
inline var object(std::initializer_list<std::pair<Identifier,var>> fields)
{auto* result=new DynamicObject;for(const auto& field:fields)result->setProperty(field.first,field.second);return var(result);}
inline String textField(const var& value,const Identifier& key)
{const auto field=value[key];return field.isString()?field.toString():String{};}
inline String idField(const var& value,const Identifier& key)
{const auto field=value[key];if(!field.isInt()&&!field.isInt64())return {};const auto id=field.toString();return positiveId(id)?id:String{};}
inline bool safeHttps(const String& address)
{URL url(address);return url.isWellFormed()&&url.getScheme().equalsIgnoreCase("https")&&url.getDomain().isNotEmpty()&&!address.containsAnyOf("\r\n")&& !address.upToFirstOccurrenceOf("/",false,false).containsChar('@')&&!address.fromFirstOccurrenceOf("://",false,false).upToFirstOccurrenceOf("/",false,false).containsChar('@');}
inline bool tone3000Origin(const String& address)
{const URL url(address);return safeHttps(address)&&(url.getDomain().equalsIgnoreCase("www.tone3000.com")||url.getDomain().equalsIgnoreCase("tone3000.com"))&&(url.getPort()==0||url.getPort()==443);}
inline String safeName(const String& original,const String& fallback)
{
    String result;for(auto c:original)if(c>=32&&c!=127&&!String("<>:\"/\\|?*").containsChar(c))result+=String::charToString(c);else result+="_";
    result=result.trim().substring(0,80).trimCharactersAtEnd(" .");if(result.isEmpty()||result=="."||result=="..")result=fallback;
    const auto stem=result.upToFirstOccurrenceOf(".",false,false).toUpperCase();
    if(StringArray{"CON","PRN","AUX","NUL","COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9","LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9"}.contains(stem))result="_"+result;
    return result;
}
inline bool noLinks(const File& path)
{for(auto p=path;p!=p.getParentDirectory();p=p.getParentDirectory())if(p.isSymbolicLink())return false;return true;}
bool commitPack(const File& from,const File& to);
struct Model{String id,name,url;};
struct Pack{String id,title,creator,creatorId,url,license;StringArray images;std::vector<Model> models;String description,maker,model,creatorUsername,gear;};
class Tone3000Client
{
public:
    Tone3000Client(Transport& io,const std::atomic<bool>& stopped,String gear="amp-cab"):transport(io),cancelled(stopped),requestedGear(std::move(gear)){}
    void exchange(const Callback& callback,const String& verifier)
    {
        token.clear();
        require(callback.kind==Callback::selected,"No Tone3000 selection to download.");
        const auto json=fetch({String(api)+"oauth/token","POST",form({{"grant_type","authorization_code"},{"code",callback.code},
            {"code_verifier",verifier},{"redirect_uri",redirectUri},{"client_id",clientId}}),{},1024*1024},"Token exchange");
        token=textField(json,"access_token");require(token.isNotEmpty()&&token.length()<16384&&!token.containsAnyOf("\r\n")&&textField(json,"token_type").equalsIgnoreCase("bearer"),"Tone3000 returned an invalid access token.");
    }
    Pack getPack(const String& id)
    {
        require(positiveId(id),"Invalid Tone3000 tone id.");
        const bool ir=requestedGear=="cab";const auto tone=fetch({String(api)+"tones/"+id+(ir?String{}:String("?architecture=2")),"GET",{},token},"Tone metadata");
        require(idField(tone,"id")==id&&textField(tone,"format")==String(ir?"ir":"nam")&&gearAllowed(requestedGear,textField(tone,"gear")),ir?"Select a cabinet IR pack.":"Select a compatible A2 NAM pack for this stage.");
        Pack pack;pack.id=id;pack.title=textField(tone,"title");pack.gear=textField(tone,"gear");require(pack.title.isNotEmpty(),"Tone3000 pack title is missing.");
        const auto user=tone["user"];pack.creator=textField(user,"display_name");if(pack.creator.isEmpty())pack.creator=textField(user,"username");pack.creatorId=textField(tone,"user_id");
        if(pack.creatorId.isEmpty())pack.creatorId=idField(tone,"user_id");pack.url=textField(tone,"url");pack.license=textField(tone,"license");
        pack.creatorUsername=textField(user,"username");pack.description=textField(tone,"description");
        pack.maker=textField(tone,"gear_make");pack.model=textField(tone,"gear_model");
        if(auto* images=tone["images"].getArray())for(const auto& image:*images)if(image.isString()&&safeHttps(image.toString()))pack.images.add(image.toString());
        std::set<String> ids;int pages=1;
        for(int page=1;page<=pages;++page)
        {
            const auto json=fetch({String(api)+"models?tone_id="+id+(ir?String{}:String("&architecture=2"))+"&page_size=300&page="+String(page),"GET",{},token},"Model metadata");
            require(json["page"].isInt()&&(int)json["page"]==page&&json["total_pages"].isInt(),"Tone3000 pagination is invalid.");
            const int reported=(int)json["total_pages"];require(reported>=1&&reported<=100&&(page==1||pages==reported),"Tone3000 pagination changed or exceeds the import limit.");pages=reported;
            auto* models=json["data"].getArray();require(models!=nullptr,"Tone3000 model list is missing.");
            for(const auto& model:*models)
            {
                require(idField(model,"tone_id")==id,"Tone3000 returned a model from another pack.");
                if(!ir&&textField(model,"architecture_version")!="2")continue;
                Model next{idField(model,"id"),textField(model,"name"),textField(model,"model_url")};
                require(next.id.isNotEmpty()&&next.name.isNotEmpty()&&safeHttps(next.url)&&ids.insert(next.id).second,"Tone3000 returned invalid or duplicate A2 model metadata.");
                pack.models.push_back(std::move(next));require(pack.models.size()<=10000,"Tone3000 pack exceeds the import limit.");
            }
        }
        require(!pack.models.empty(),ir?"This pack has no available cabinet IRs.":"This pack has no available A2 NAM models.");return pack;
    }
    MemoryBlock download(const String& url,size_t limit,const String& context,bool authenticated)
    {return request({url,"GET",{},authenticated?token:String{},limit},context).bytes;}
private:
    Response request(const Request& args,const String& context)
    {
        require(!cancelled.load(),"Import cancelled.");auto reply=transport.perform(args,cancelled);require(!cancelled.load(),"Import cancelled.");
        require(reply.status>=200&&reply.status<300,context+" failed"+(reply.status>0?" (HTTP "+String(reply.status)+").":". Check your connection and try again."));
        require(reply.bytes.getSize()>0&&reply.bytes.getSize()<=args.maximum,context+" returned an empty or oversized response.");return reply;
    }
    var fetch(const Request& args,const String& context)
    {auto reply=request(args,context);var json;auto result=JSON::parse(String::fromUTF8(static_cast<const char*>(reply.bytes.getData()),static_cast<int>(reply.bytes.getSize())),json);require(result.wasOk()&&json.isObject(),context+" returned invalid JSON.");return json;}
    Transport& transport;const std::atomic<bool>& cancelled;String token,requestedGear;
};
// Inspect encoded dimensions before allocating decoded pixels. Only the image
// formats JUCE supports here are accepted, with bounded bytes and pixel count.
inline Image boundedImage(const void* memory,size_t size)
{
    if(!memory||size<10||size>8*1024*1024)return {};
    const auto* p=static_cast<const unsigned char*>(memory);unsigned width=0,height=0;
    auto be16=[&](size_t i){return (unsigned(p[i])<<8)|p[i+1];};
    auto be32=[&](size_t i){return (unsigned(p[i])<<24)|(unsigned(p[i+1])<<16)|(unsigned(p[i+2])<<8)|p[i+3];};
    if(size>=24&&p[0]==137&&p[1]==80&&p[2]==78&&p[3]==71){width=be32(16);height=be32(20);}
    else if(p[0]=='G'&&p[1]=='I'&&p[2]=='F'){width=unsigned(p[6])|(unsigned(p[7])<<8);height=unsigned(p[8])|(unsigned(p[9])<<8);}
    else if(p[0]==255&&p[1]==216)
    {
        size_t i=2;while(i+4<=size)
        {
            if(p[i++]!=255)break;while(i<size&&p[i]==255)++i;if(i>=size)break;const auto marker=p[i++];
            if(marker==0xd9||marker==0xda)break;if(marker==1||(marker>=0xd0&&marker<=0xd7))continue;
            if(i+2>size)break;const auto length=be16(i);if(length<2||i+length>size)break;
            if((marker>=0xc0&&marker<=0xc3)||(marker>=0xc5&&marker<=0xc7)||(marker>=0xc9&&marker<=0xcb)||(marker>=0xcd&&marker<=0xcf))
            {if(length>=7){height=be16(i+3);width=be16(i+5);}break;}
            i+=length;
        }
    }
    if(width==0||height==0||width>8192||height>8192||uint64_t(width)*height>16*1024*1024)return {};
    return ImageFileFormat::loadFrom(memory,size);
}
inline Image boundedImage(const File& file)
{if(!file.existsAsFile()||file.getSize()<=0||file.getSize()>8*1024*1024)return {};MemoryBlock bytes;if(!file.loadFileAsData(bytes))return {};return boundedImage(bytes.getData(),bytes.getSize());}
struct ImportResult{bool ok=false,cancelled=false;String message;File folder;int models=0;std::vector<namamp::Entry> entries;};
class Tone3000DownloadManager
{
public:
    explicit Tone3000DownloadManager(File library,String gear="amp-cab"):amps(std::move(library)),requestedGear(std::move(gear)){}
    ImportResult import(Tone3000Client& client,const Pack& pack,const std::atomic<bool>& cancelled,std::function<void(String)> progress={})
    {
        ImportResult result;File staging;
        try
        {
            const auto container=amps.getChildFile("TONE3000");
            const auto work=amps.getParentDirectory().getParentDirectory().getChildFile("Tone3000/Imports");
            require(noLinks(container)&&noLinks(work),"Tone3000 destination must not contain symbolic links.");
            require(work.createDirectory().wasOk(),"Cannot create the Tone3000 temporary directory.");
            staging=work.getChildFile("import-"+Uuid().toString());require(staging.createDirectory().wasOk(),"Cannot create the Tone3000 staging directory.");
            std::set<String> filenames;Array<var> manifestModels;namamp::Library stagingLibrary(staging);
            for(size_t i=0;i<pack.models.size();++i)
            {
                require(!cancelled.load(),"Import cancelled.");const auto& model=pack.models[i];
                if(progress)progress("Downloading "+String(static_cast<int>(i+1))+" / "+String(static_cast<int>(pack.models.size()))+": "+model.name);
                auto stem=safeName(model.name.endsWithIgnoreCase(".nam")?model.name.dropLastCharacters(4):model.name,"model-"+model.id);
                auto name=stem+".nam";int suffix=0;while(!filenames.insert(name.toLowerCase()).second)name=stem+"-"+model.id+"-"+String(++suffix)+".nam";
                auto data=client.download(model.url,32*1024*1024,"Model "+model.name,true);
                const auto destination=staging.getChildFile(name);require(destination.replaceWithData(data.getData(),data.getSize()),"Cannot write model "+model.name+".");
                namamp::Entry entry;auto valid=stagingLibrary.read(destination,entry);require(valid.wasOk(),"Model "+model.name+" is incompatible with ToneGrid: "+valid.getErrorMessage());
                result.entries.push_back(std::move(entry));
                manifestModels.add(object({{"id",model.id.getLargeIntValue()},{"name",model.name},{"file",name},{"sha256",SHA256(data).toHexString()},{"architecture","2"}}));
            }
            String imageFile,warning;
            if(!pack.images.isEmpty())
            {
                try
                {
                    auto data=client.download(pack.images[0],8*1024*1024,"Pack thumbnail",false);auto image=boundedImage(data.getData(),data.getSize());
                    require(image.isValid()&&image.getWidth()<=8192&&image.getHeight()<=8192,"Invalid Tone3000 thumbnail.");
                    const auto* raw=static_cast<const unsigned char*>(data.getData());
                    imageFile=String("thumbnail")+(data.getSize()>=4&&raw[0]==137&&raw[1]==80?".png":data.getSize()>=2&&raw[0]==255&&raw[1]==216?".jpg":".gif");
                    require(staging.getChildFile(imageFile).replaceWithData(data.getData(),data.getSize()),"Cannot save the thumbnail.");
                }
                catch(const Failure&){require(!cancelled.load(),"Import cancelled.");warning=" Thumbnail unavailable; the generic amp image will be used.";imageFile.clear();}
            }
            auto manifest=object({{"schema",1},{"source","tone3000"},{"tone_id",pack.id.getLargeIntValue()},{"title",pack.title},{"creator",pack.creator},
                {"creator_id",pack.creatorId},{"url",pack.url},{"license",pack.license},{"architecture","2"},{"format","nam"},{"gear",pack.gear.isNotEmpty()?pack.gear:requestedGear},
                {"models",var(manifestModels)},{"image",imageFile},{"description",pack.description},{"maker",pack.maker},{"model",pack.model},
                {"creator_username",pack.creatorUsername},{"downloaded_at",Time::getCurrentTime().toISO8601(true)}});
            require(staging.getChildFile(manifestName).replaceWithText(JSON::toString(manifest,false)),"Cannot save the Tone3000 manifest.");
            require(!cancelled.load(),"Import cancelled.");require(noLinks(container)&&container.createDirectory().wasOk(),"Cannot create the Tone3000 library folder.");
            auto folder=container.getNonexistentChildFile(safeName(pack.title,"Tone-"+pack.id),"",false);
            require(noLinks(folder)&&!folder.exists()&&commitPack(staging,folder),"Cannot commit the Tone3000 pack. Existing files were preserved.");
            const auto prefix=folder.getRelativePathFrom(amps).replaceCharacter('\\','/')+"/";for(auto& entry:result.entries)entry.identity.file=prefix+entry.identity.file;
            staging=File{};result.ok=true;result.folder=folder;result.models=static_cast<int>(pack.models.size());
            result.message="Imported "+String(result.models)+" A2 NAM models from "+pack.title+". Select a model in Local NAM Library."+warning;
        }
        catch(const Failure& failure){result.cancelled=cancelled.load();result.message=result.cancelled?"Tone3000 import cancelled.":failure.message+" No partial pack was added.";}
        catch(const std::exception&){result.message="Tone3000 import failed. No partial pack was added.";}
        if(staging!=File{}&&staging.isAChildOf(amps.getParentDirectory().getParentDirectory().getChildFile("Tone3000/Imports"))&&noLinks(staging))staging.deleteRecursively();
        return result;
    }
private:File amps;String requestedGear;
};
class CabinetDownloadManager
{
public:
    explicit CabinetDownloadManager(File library):root(std::move(library)){}
    ImportResult import(Tone3000Client& client,const Pack& pack,const std::atomic<bool>& cancelled,std::function<void(String)> progress={})
    {
        ImportResult result;File staging;const auto work=root.getParentDirectory().getChildFile("Tone3000/Imports");
        try
        {
            require(pack.gear=="cab","Select a cabinet IR pack.");
            const auto container=root.getChildFile("TONE3000");require(noLinks(container)&&noLinks(work),"Linked cabinet destinations are not supported.");
            require(work.createDirectory().wasOk(),"Cannot create the cabinet import directory.");
            staging=work.getChildFile("cabinet-"+Uuid().toString());require(staging.createDirectory().wasOk(),"Cannot create the cabinet staging directory.");
            std::set<String> names;Array<var> models;size_t total=0;
            for(size_t i=0;i<pack.models.size();++i)
            {
                require(!cancelled.load(),"Import cancelled.");const auto& model=pack.models[i];
                if(progress)progress("Downloading cabinet "+String(int(i+1))+" / "+String(int(pack.models.size()))+": "+model.name);
                auto stem=safeName(model.name.endsWithIgnoreCase(".wav")?model.name.dropLastCharacters(4):model.name,"IR-"+model.id);auto name=stem+".wav";int suffix=0;
                while(!names.insert(name.toLowerCase()).second)name=stem+"-"+model.id+"-"+String(++suffix)+".wav";
                const auto bytes=client.download(model.url,16*1024*1024,"Cabinet "+model.name,true);total+=bytes.getSize();require(total<=512*1024*1024,"This cabinet pack exceeds the 512 MiB import limit.");
                const auto file=staging.getChildFile(name);require(file.replaceWithData(bytes.getData(),bytes.getSize()),"Cannot write cabinet IR.");
                const auto valid=tonegrid::cabinet::prepare(file,staging);require(valid.result.wasOk(),valid.result.getErrorMessage());
                models.add(object({{"id",model.id.getLargeIntValue()},{"file",name},{"name",model.name},{"sha256",SHA256(bytes).toHexString()}}));
            }
            require(!models.isEmpty(),"No cabinet IRs were returned.");String imageFile,warning;
            if(!pack.images.isEmpty())try
            {
                const auto data=client.download(pack.images[0],8*1024*1024,"Cabinet thumbnail",false);require(boundedImage(data.getData(),data.getSize()).isValid(),"Invalid cabinet thumbnail.");
                imageFile="thumbnail.image";require(staging.getChildFile(imageFile).replaceWithData(data.getData(),data.getSize()),"Cannot save cabinet thumbnail.");
            }
            catch(const Failure&){require(!cancelled.load(),"Import cancelled.");imageFile.clear();warning=" Thumbnail unavailable; the cabinet illustration will be used.";}
            const auto metadata=object({{"schema",1},{"title",pack.title},{"creator",pack.creator},{"source",pack.url},{"tone_id",pack.id},{"license",pack.license},{"format","ir"},{"gear","cab"},{"image",imageFile},{"models",var(models)}});
            require(staging.getChildFile(".tonegrid-cabinet.json").replaceWithText(JSON::toString(metadata,false)),"Cannot save cabinet metadata.");
            require(!cancelled.load(),"Import cancelled.");require(noLinks(container)&&container.createDirectory().wasOk(),"Cannot create cabinet library folder.");
            const auto destination=container.getNonexistentChildFile(safeName(pack.title,"Cabinet-"+pack.id),"",false);
            require(!destination.exists()&&noLinks(destination)&&commitPack(staging,destination),"Cannot commit cabinet pack. Existing files were kept.");
            staging=File{};result.ok=true;result.folder=destination;result.models=models.size();result.message="Imported "+String(result.models)+" cabinet IRs from "+pack.title+". Select one in the cabinet library."+warning;
        }
        catch(const Failure& e){result.cancelled=cancelled.load();result.message=result.cancelled?"Cabinet import cancelled.":e.message+" No partial pack was added.";}
        catch(const std::exception&){result.message="Cabinet import failed. No partial pack was added.";}
        if(staging!=File{}&&staging.isAChildOf(work)&&noLinks(staging))staging.deleteRecursively();return result;
    }
private:File root;
};

inline var localManifest(const File& model)
{
    const auto sidecar=model.getSiblingFile(manifestName);if(!sidecar.existsAsFile()||sidecar.getSize()>1024*1024||!noLinks(sidecar))return {};
    const auto json=JSON::parse(sidecar);if(textField(json,"source")!="tone3000")return {};
    bool found=false;if(auto* entries=json["models"].getArray())for(const auto& entry:*entries)if(textField(entry,"file")==model.getFileName())found=true;
    return found?json:var{};
}
inline Image localThumbnail(const File& model)
{
    const auto json=localManifest(model);const auto name=textField(json,"image");if(name.isEmpty()||name.containsAnyOf("/\\:")||name=="."||name=="..")return {};
    const auto image=model.getSiblingFile(name);return image.existsAsFile()&&noLinks(image)&&image.getSize()<=8*1024*1024?boundedImage(image):Image{};
}
}
