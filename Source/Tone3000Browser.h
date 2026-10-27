#pragma once
#include "HttpTransport.h"
#include "Shared/UI/PopupWindow.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <mutex>
#include <thread>

namespace tonegrid::t3k
{
class Tone3000BrowserWindow final:public DocumentWindow,private Timer
{
    struct Job
    {
        std::atomic<bool> cancelled{false},done{false};std::mutex lock;String progress;ImportResult result;bool diagnostic=false;HttpTransport transport;
        void status(String value){std::lock_guard<std::mutex> guard(lock);progress=std::move(value);}
        std::unique_ptr<Tone3000Client> client;
    };
    class Browser final:public WebBrowserComponent
    {
    public:
        Browser(Tone3000BrowserWindow& parent,const Options& options):WebBrowserComponent(options),owner(parent){}
        bool pageAboutToLoad(const String& address) override{return owner.navigation(address);}
        void pageFinishedLoading(const String&) override{owner.browserReady=true;}
        void windowCloseRequest() override{owner.closeButtonPressed();}
        void newWindowAttemptingToLoad(const String& address) override{if(safeHttps(address))tonegrid::platform::navigateEncodedBrowserUrl(*this,address);}
        bool pageLoadHadNetworkError(const String& message) override{owner.diagnoseNavigationError(message);return false;}
    private:Tone3000BrowserWindow& owner;
    };
public:
    Tone3000BrowserWindow(File library,File profile,std::function<void(const ImportResult&)> completed,
                         std::function<void()> closed={},std::function<void(const String&)> testNavigation={},String gear="amp-cab")
        :DocumentWindow(gear=="cab"?"Tone3000 - Cabinet IRs":gear=="pedal"?"Tone3000 - A2 NAM Pedals":"Tone3000 - A2 NAM Amps",Colour(0xff071017),closeButton),amps(std::move(library)),profileFolder(std::move(profile)),onComplete(std::move(completed)),onClosed(std::move(closed)),navigationOverride(std::move(testNavigation)),requestedGear(std::move(gear))
    {
        setUsingNativeTitleBar(true);setResizable(true,false);setResizeLimits(360,260,32768,32768);
        setContentNonOwned(&content,false);content.addAndMakeVisible(status);content.addAndMakeVisible(cancel);
        status.setJustificationType(Justification::centredLeft);status.setColour(Label::textColourId,Colour(0xffdbe8ee));
        status.setMinimumHorizontalScale(1.f);cancel.onClick=[this]{closeButtonPressed();};
        content.addChildComponent(retry);retry.onClick=[this]{start();};
        if(const auto* display=Desktop::getInstance().getDisplays().getPrimaryDisplay())setBounds(popup::fittedBounds(display->userArea,{1000,760},display->userArea.getCentre()));
        else centreWithSize(800,500);
    }
    ~Tone3000BrowserWindow() override
    {onClosed={};onComplete={};session.reset();stopTimer();if(job){job->cancelled=true;job->transport.cancel();}if(worker.joinable())worker.join();browser.reset();clearContentComponent();}
    static WebBrowserComponent::Options browserOptions(const File& profile)
    {return tonegrid::platform::browserOptions(profile);}
    bool busy() const{return job&&!job->done.load();}
    bool authorizationStarted() const{return session!=nullptr&&browser!=nullptr;}
    bool nativeBrowserReady() const{return browserReady;}
    bool verifyErrorActions()
    {
        if(!session||!browser||!navigationOverride)return false;
        const auto previous=session?session->authorizationUrl():String{};
        const String refused="{\"error\":\"invalid_client\"}";
        error(authorizationFailure({400,MemoryBlock(refused.toRawUTF8(),refused.getNumBytesAsUTF8()),{}},{}));
        const bool offered=retry.isVisible()&&cancel.getButtonText()=="Back to NAM Library"&&!browser->isVisible();
        retry.onClick();return offered&&authorizationStarted()&&!retry.isVisible()&&session->authorizationUrl()!=previous;
    }
    void start()
    {
        if(busy())
        {
            setVisible(true);toFront(true);
            if(job->cancelled.load()){restartWhenReady=true;status.setText("Finishing the previous cancellation...",dontSendNotification);layout();}
            return;
        }
        stopTimer();if(worker.joinable())worker.join();
        restartWhenReady=false;session.reset();if(browser)browser->setVisible(false);retry.setVisible(false);
        job.reset();session=std::make_unique<Tone3000OAuthSession>(requestedGear);cancel.setButtonText("Cancel");
        const auto options=browserOptions(profileFolder);
        setVisible(true);toFront(true);
        if(!WebBrowserComponent::areOptionsSupported(options)){error(tonegrid::platform::browserUnavailableMessage());return;}
        if(session->authorizationUrl().isEmpty()){error("Cannot generate secure Tone3000 authentication. Please try again.");return;}
        openSelect();
    }
    void closeButtonPressed() override
    {const bool wasVisible=isVisible();restartWhenReady=false;if(job){job->cancelled=true;job->transport.cancel();}session.reset();parkBrowser();setVisible(false);if(wasVisible&&onClosed)onClosed();}
    void resized() override{DocumentWindow::resized();layout();}
private:
    // Own navigation explicitly: JUCE's hidden-page goBack() competes with a fresh
    // OAuth session. The blank page also stops preview audio and background activity.
    void parkBrowser(){if(browser){browser->stop();browser->goToURL("about:blank");browser->setVisible(false);}}
    void layout(){auto r=content.getLocalBounds();auto bar=r.removeFromBottom(76).reduced(12);cancel.setBounds(bar.removeFromRight(cancel.getButtonText()=="Cancel"?90:190));bar.removeFromRight(10);if(retry.isVisible()){retry.setBounds(bar.removeFromRight(90));bar.removeFromRight(10);}status.setBounds(bar);if(browser)browser->setBounds(r);}
    void openSelect()
    {
        if(!session)return;
#if JUCE_WINDOWS
        if(!noLinks(profileFolder)||profileFolder.createDirectory().failed()){error("Cannot create the Tone3000 browser profile. The local library remains available.");return;}
#endif
        // Reuse the native controller on Retry/reopen; it may still be initializing
        // asynchronously, and its cookies/profile belong to this same window.
        if(!browser)browser=std::make_unique<Browser>(*this,browserOptions(profileFolder));content.addAndMakeVisible(*browser);
        status.setText(requestedGear=="cab"?"Select a cabinet impulse-response pack.":requestedGear=="pedal"?"Select an A2 NAM pedal pack. All captures will be added; calibration is checked when selected.":requestedGear=="amp"?"Select an A2 NAM amp head pack, without cabinet.":"Select an A2 NAM Amp+Cab or Amp Head pack.",dontSendNotification);layout();
        if(navigationOverride){navigationOverride(session->authorizationUrl());browser->goToURL("about:blank");}else if(!tonegrid::platform::navigateEncodedBrowserUrl(*browser,session->authorizationUrl())){error("Cannot initialize secure Tone3000 navigation. Please retry.");return;}
    }
    void diagnoseNavigationError(const String& browserError)
    {
        if(!session||busy()||!isVisible())return;
        if(worker.joinable())worker.join();const auto address=session->authorizationUrl();session.reset();
        parkBrowser();retry.setVisible(false);job=std::make_shared<Job>();job->diagnostic=true;
        job->status("Checking the Tone3000 connection...");const auto work=job;
        worker=std::thread([work,address,browserError]
        {
            try{const auto response=work->transport.perform({address,"GET",{},{},64*1024,false},work->cancelled);work->result.message=authorizationFailure(response,browserError);}
            catch(const std::exception&){work->result.message="Unable to check Tone3000. Retry or return to your local NAM library.";}
            work->done=true;
        });startTimer(100);
    }
    bool navigation(const String& address)
    {
        if(address=="about:blank")return true;
        if(!session)return false;auto callback=session->accept(address);
        if(callback.kind==Callback::unrelated)return safeHttps(address)||address=="about:blank";
        if(callback.kind==Callback::cancelled){closeButtonPressed();return false;}
        if(callback.kind==Callback::failed){error(callback.error);session.reset();return false;}
        const auto verifier=session->codeVerifier();session.reset();parkBrowser();job=std::make_shared<Job>();job->status("Preparing the selected Tone3000 pack...");const auto work=job;const auto root=amps;
        const auto gear=requestedGear;
        worker=std::thread([work,root,callback,verifier,gear]
        {
            try
            {
                work->client=std::make_unique<Tone3000Client>(work->transport,work->cancelled,gear);work->client->exchange(callback,verifier);auto pack=work->client->getPack(callback.toneId);
                if(gear=="cab"){CabinetDownloadManager downloads(root);work->result=downloads.import(*work->client,pack,work->cancelled,[work](String message){work->status(std::move(message));});}
                else {Tone3000DownloadManager downloads(root,gear);work->result=downloads.import(*work->client,pack,work->cancelled,[work](String message){work->status(std::move(message));});}
            }
            catch(const Failure& failure){work->result.cancelled=work->cancelled.load();work->result.message=work->result.cancelled?"Tone3000 import cancelled.":failure.message;}
            catch(const std::exception&){work->result.message="Tone3000 import failed. Please try again.";}
            work->done=true;
        });startTimer(100);return false;
    }
    void error(const String& message){parkBrowser();status.setText(message,dontSendNotification);cancel.setButtonText(requestedGear=="cab"?"Back to Cabinet Library":"Back to NAM Library");retry.setVisible(true);layout();}
    void timerCallback() override
    {
        if(!job)return;if(!job->done.load()){std::lock_guard<std::mutex> guard(job->lock);status.setText(job->progress,dontSendNotification);return;}
        stopTimer();if(worker.joinable())worker.join();
        const auto result=job->result;const bool diagnostic=job->diagnostic,cancelled=job->cancelled.load();job.reset();
        if(restartWhenReady&&isVisible()){if(!diagnostic&&onComplete)onComplete(result);start();return;}
        if(diagnostic){if(!cancelled&&isVisible())error(result.message);return;}
        if(onComplete)onComplete(result);
        if(result.ok||result.cancelled)closeButtonPressed();else if(isVisible())error(result.message);
    }
    Component content;Label status;TextButton cancel{"Cancel"},retry{"Retry"};File amps,profileFolder;
    std::function<void(const ImportResult&)> onComplete;std::function<void()> onClosed;std::function<void(const String&)> navigationOverride;
    std::unique_ptr<Tone3000OAuthSession> session;std::unique_ptr<Browser> browser;std::shared_ptr<Job> job;
    std::thread worker;bool restartWhenReady=false,browserReady=false;
    String requestedGear;
};
}
