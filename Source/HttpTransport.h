#pragma once
#include "Tone3000.h"
#include <mutex>
namespace tonegrid::t3k
{
// All HTTP work is performed by a worker, never by the audio callback.
class HttpTransport final:public Transport
{
public:
    void cancel()
    {std::shared_ptr<WebInputStream> stream;{std::lock_guard<std::mutex> guard(streamMutex);stream=activeStream;}if(stream)stream->cancel();}
    Response perform(const Request& request,const std::atomic<bool>& cancelled) override
    {
        Response result;String address=request.url;
        for(int redirect=0;redirect<=5&&!cancelled.load();++redirect)
        {
            if(!safeHttps(address)){result.error="HTTPS required.";return result;}
            // Remain below the documented default 100 requests/minute.
            while(Time::getMillisecondCounterHiRes()<nextRequest&&!cancelled.load())Thread::sleep(20);
            if(cancelled.load())return result;nextRequest=Time::getMillisecondCounterHiRes()+700.0;
            StringPairArray headers;String extra;
            // A bearer token must never be forwarded to an image/CDN/third-party origin.
            if(tone3000Origin(address)&&request.token.isNotEmpty())extra="Authorization: Bearer "+request.token+"\r\n";
            if(request.method=="POST")extra+="Content-Type: application/x-www-form-urlencoded\r\n";
            URL url(address);if(request.method=="POST")url=url.withPOSTData(request.body);
            auto input=std::make_shared<WebInputStream>(url,request.method=="POST");
            input->withCustomRequestCommand(request.method).withConnectionTimeout(10000).withExtraHeaders(extra).withNumRedirectsToFollow(0);
            {std::lock_guard<std::mutex> guard(streamMutex);activeStream=input;}
            if(cancelled.load()){cancel();result.status=0;return result;}
            const bool connected=input->connect(nullptr);result.status=input->getStatusCode();headers=input->getResponseHeaders();
            if(!connected){result.error=cancelled.load()?"Cancelled.":"Connection failed.";return result;}
            if(result.status>=300&&result.status<400&&request.method=="GET")
            {
                if(!request.followRedirects)return result;
                String location;for(int i=0;i<headers.size();++i)if(headers.getAllKeys()[i].equalsIgnoreCase("Location"))location=headers.getAllValues()[i];
                if(location.startsWith("/"))location="https://"+url.getDomain()+location;
                if(!safeHttps(location)){result.error="Invalid download redirect.";return result;}address=location;continue;
            }
            if(!input)return result;
            const auto length=input->getTotalLength();if(length>static_cast<int64>(request.maximum)){result.status=0;result.error="Response too large.";return result;}
            char block[32768];MemoryOutputStream output;
            while(!input->isExhausted()&&!cancelled.load())
            {
                const auto bytes=input->read(block,sizeof(block));if(bytes<=0)break;
                if(output.getDataSize()+static_cast<size_t>(bytes)>request.maximum){result.status=0;return result;}output.write(block,static_cast<size_t>(bytes));
            }
            const auto* web=dynamic_cast<WebInputStream*>(input.get());
            if(cancelled.load()||(web&&web->isError())||(length>=0&&static_cast<int64>(output.getDataSize())!=length)){result.status=0;return result;}
            result.bytes=output.getMemoryBlock();return result;
        }
        result.status=0;return result;
    }
private:double nextRequest=0;std::mutex streamMutex;std::shared_ptr<WebInputStream> activeStream;
};
}
