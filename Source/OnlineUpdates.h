#pragma once
#include "HttpTransport.h"
#include "Content/Presets/ToneBank.h"
#include <future>

namespace tonegrid::updates
{
inline constexpr const char* versionUrl="https://www.tonegrid.net/lastversion.txt";
// Older applications keep /tonebank.json (100-entry transition bank).
// 1.1.36+ uses a stable modern endpoint so its growing bank is not replaced by it.
inline constexpr const char* bankUrl="https://www.tonegrid.net/tonebank-modern.json";
inline constexpr const char* downloadUrl="https://www.tonegrid.net/download";
inline constexpr juce::int64 checkInterval=72LL*60*60*1000;

struct Version
{
    std::array<int,3> parts{};
    auto operator<=>(const Version&) const=default;
    static std::optional<Version> parse(juce::String text)
    {
        text=text.trim();if(text.startsWithChar(0xfeff))text=text.substring(1).trim();
        const auto tokens=juce::StringArray::fromTokens(text,".",{});
        if(tokens.size()!=3||text.length()>32)return {};
        Version value;
        for(int i=0;i<3;++i)
        {
            if(tokens[i].isEmpty()||tokens[i].length()>6||!tokens[i].containsOnly("0123456789"))return {};
            value.parts[(size_t)i]=tokens[i].getIntValue();
        }
        return value;
    }
};
inline bool due(juce::int64 last,juce::int64 now)
{return last<=0||last>now||now-last>=checkInterval;}

struct Schedule
{
    juce::int64 application=0,bank=0;
    static Schedule read(const juce::File& file)
    {
        Schedule result;
        if(file.getSize()>4096)return result;
        if(auto xml=juce::XmlDocument::parse(file);xml&&xml->hasTagName("OnlineChecks")&&xml->getIntAttribute("version")==1)
        {result.application=xml->getStringAttribute("application").getLargeIntValue();result.bank=xml->getStringAttribute("bank").getLargeIntValue();}
        return result;
    }
    juce::Result write(const juce::File& file) const
    {
        if(file.getParentDirectory().createDirectory().failed())return juce::Result::fail("Cannot save online check dates.");
        juce::XmlElement xml("OnlineChecks");xml.setAttribute("version",1);xml.setAttribute("application",juce::String(application));xml.setAttribute("bank",juce::String(bank));
        juce::TemporaryFile temporary(file);
        if(!xml.writeTo(temporary.getFile())||!temporary.overwriteTargetFileWithTemporary())return juce::Result::fail("Cannot save online check dates.");
        return juce::Result::ok();
    }
};
struct Result
{
    Schedule schedule;
    bool checkedApplication=false,checkedBank=false,newVersion=false,bankChanged=false;
    juce::String latestVersion;
    juce::Result applicationStatus=juce::Result::ok(),bankStatus=juce::Result::ok(),scheduleStatus=juce::Result::ok();
    tonebank::Bank bank;
};

// Runs only on a worker. The previous bank survives HTTP, schema and compatibility
// failures. A bank refresh never recalls a preset or changes the audio graph.
inline Result fetch(t3k::Transport& transport,const std::atomic<bool>& cancelled,const juce::File& preferences,
                    const juce::File& cache,const juce::String& current,Schedule dates,bool application,bool bank,juce::int64 now)
{
    Result out;out.schedule=dates;out.checkedApplication=application;out.checkedBank=bank;
    if(application)out.schedule.application=now;if(bank)out.schedule.bank=now;
    // Persist attempts as well as successes: an offline launch must not retry daily.
    out.scheduleStatus=out.schedule.write(preferences);
    const auto request=[&](const char* address,size_t maximum)
    {return transport.perform({address,"GET",{},{},maximum,false},cancelled);};
    if(application&&!cancelled.load())
    {
        try
        {
            const auto reply=request(versionUrl,128);const auto text=reply.bytes.toString().trim();
            const auto latest=Version::parse(text),installed=Version::parse(current);
            if(cancelled.load()||reply.status!=200||reply.bytes.getSize()==0||reply.bytes.getSize()>128||!latest||!installed)
                out.applicationStatus=juce::Result::fail("Unable to check for updates. Please try again later.");
            else{out.latestVersion=text;out.newVersion=*latest>*installed;}
        }
        catch(...){out.applicationStatus=juce::Result::fail("Unable to check for updates. Please try again later.");}
    }
    if(bank&&!cancelled.load())
    {
        try
        {
            const auto reply=request(bankUrl,8*1024*1024);
            if(reply.status!=200||reply.bytes.getSize()==0||reply.bytes.getSize()>8*1024*1024)
                out.bankStatus=juce::Result::fail("ToneBank update unavailable. Your local bank has been kept.");
            else
            {
                const auto raw=reply.bytes.toString();out.bankStatus=tonebank::parse(raw,out.bank);
                if(out.bankStatus.wasOk())
                {
                    const auto metadata=tonebank::Json::parse(raw.toStdString());
                    const auto target=Version::parse(juce::String(metadata.at("targetToneGridVersion").get<std::string>()));
                    const auto installed=Version::parse(current);
                    if(!target||!installed||*target>*installed)out.bankStatus=juce::Result::fail("This ToneBank requires a newer ToneGrid version. Your local bank has been kept.");
                    else if(!tonebank::supportedLibrary(out.bank.libraryRevision))out.bankStatus=juce::Result::fail("This ToneBank uses another capture library. Your local bank has been kept.");
                    else if(cancelled.load())out.bankStatus=juce::Result::fail("ToneBank update cancelled.");
                    else if(!cache.existsAsFile()||juce::SHA256(cache)!=juce::SHA256(reply.bytes))
                    {out.bankStatus=tonebank::install(cache,raw);out.bankChanged=out.bankStatus.wasOk();}
                }
            }
        }
        catch(...){out.bankStatus=juce::Result::fail("ToneBank update failed. Your local bank has been kept.");}
    }
    return out;
}

class Service final
{
public:
    Service(juce::File preference,juce::File bankCache,juce::String version)
        :preferences(std::move(preference)),cache(std::move(bankCache)),current(std::move(version)),dates(Schedule::read(preferences)){}
    ~Service(){stop();}
    void stop(){cancelled=true;transport.cancel();if(work.valid())work.wait();}
    bool busy() const{return work.valid();}
    bool start(bool application,bool bank,bool force=false,juce::int64 now=juce::Time::currentTimeMillis())
    {
        if(busy()||cancelled.load())return false;
        application=application&&(force||due(dates.application,now));bank=bank&&(force||due(dates.bank,now));
        if(!application&&!bank)return false;
        work=std::async(std::launch::async,[this,application,bank,now]
        {return fetch(transport,cancelled,preferences,cache,current,dates,application,bank,now);});return true;
    }
    bool poll(Result& result)
    {
        if(!busy()||work.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        result=work.get();dates=result.schedule;return true;
    }
private:
    const juce::File preferences,cache;const juce::String current;Schedule dates;
    t3k::HttpTransport transport;std::atomic<bool> cancelled{false};std::future<Result> work;
};
}
