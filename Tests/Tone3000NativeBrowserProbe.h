#pragma once
#include "Tone3000.h"

// Intercept the native navigation policy before any request reaches the server.
// This exercises the actual WKWebView URL boundary, without login or user data.
class Tone3000NativeBrowserProbe final : public juce::Component, private juce::Timer
{
    class Window final : public juce::DocumentWindow
    {
    public:
        Window():DocumentWindow("ToneGrid - native OAuth URL check",juce::Colours::black,0)
        {setUsingNativeTitleBar(true);}
    };
    class Browser final : public juce::WebBrowserComponent
    {
    public:
        explicit Browser(Tone3000NativeBrowserProbe& p)
            :WebBrowserComponent(tonegrid::platform::browserOptions({})),owner(p){}
        bool pageAboutToLoad(const juce::String& address) override
        {owner.inspect(address);return false;}
        bool pageLoadHadNetworkError(const juce::String&) override{return false;}
    private:Tone3000NativeBrowserProbe& owner;
    };
public:
    Tone3000NativeBrowserProbe(juce::File output,std::function<void(bool)> complete)
        :destination(std::move(output)),finished(std::move(complete)),browser(*this)
    {
        addAndMakeVisible(browser);setSize(640,400);browser.setBounds(getLocalBounds());
        window.setContentNonOwned(this,true);window.centreWithSize(640,430);window.setVisible(true);
        startTimer(100);
    }
    ~Tone3000NativeBrowserProbe() override{stopTimer();window.clearContentComponent();}
    void resized() override{browser.setBounds(getLocalBounds());}
private:
    static juce::String parameter(const juce::String& address,const juce::String& key)
    {
        const juce::URL url(address);const auto index=url.getParameterNames().indexOf(key);
        return index<0?juce::String{}:url.getParameterValues()[index];
    }
    static juce::String opaqueValue()
    {return juce::String::fromUTF8("a+b% &x=1/#\xc3\xa9");}
    void check(bool value,const juce::String& label)
    {passed&=value;report+=(value?"PASS ":"FAIL ")+label+"\n";}
    void timerCallback() override
    {
        if(pending)
        {
            pending=false;attempts=0;
            if(++stage==5){finish();return;}
            tonegrid::t3k::Tone3000OAuthSession session(stage==2?"pedal":stage==4?"cab":"amp_amp-cab");
            expected=session.authorizationUrl();
            if(stage==3)expected+="&opaque="+tonegrid::t3k::escaped(opaqueValue());
            if(stage==0)browser.goToURL(expected);
            else if(!tonegrid::platform::navigateEncodedBrowserUrl(browser,expected))
            {check(false,"native browser request was created");finish();}
            return;
        }
        if(++attempts>200){check(false,"native navigation callback arrived before timeout");finish();}
    }
    void inspect(const juce::String& address)
    {
        if(done||pending||!address.startsWith(tonegrid::t3k::api))return;
        using namespace tonegrid::t3k;
        if(stage==0)
        {
#if JUCE_MAC
            check(parameter(address,"redirect_uri")==escaped(redirectUri),
                  "negative control reproduces JUCE 8.0.6 double encoding on Mac");
#else
            check(parameter(address,"redirect_uri")==redirectUri,"native baseline preserves callback");
#endif
        }
        else
        {
            check(parameter(address,"redirect_uri")==redirectUri,
                  stage==2?"pedal callback reaches WKWebView encoded exactly once":stage==4?"cabinet callback reaches WKWebView encoded exactly once":"head/amp callback reaches WKWebView encoded exactly once");
            bool intact=true;
            for(const auto* key:{"client_id","state","code_challenge","code_challenge_method","gears","format","architecture"})
                intact&=parameter(address,key)==parameter(expected,key);
            check(intact,"PKCE, state and selection filters survive native navigation");
            if(stage==3){report+="Synthetic opaque expected="+opaqueValue()+" actual="+parameter(address,"opaque")+"\n";check(parameter(address,"opaque")==opaqueValue(),
                              "escaped plus, percent, separators, fragment and Unicode stay in one value");}
        }
        pending=true;
    }
    void finish()
    {
        if(done)return;done=true;stopTimer();window.setVisible(false);
        report+="No request sent, no sign-in, no token exchange or audio device.\n";
        destination.getParentDirectory().createDirectory();
        const bool written=destination.replaceWithText(report);if(finished)finished(passed&&written);
    }
    juce::File destination;std::function<void(bool)> finished;Window window;Browser browser;
    juce::String expected,report;int stage=-1,attempts=0;bool pending=true,passed=true,done=false;
};
