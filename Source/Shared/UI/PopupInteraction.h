#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Shared/UI/UiMotion138.h"
#include <algorithm>
#include <vector>

namespace tonegrid::popup
{
// One shade belongs to the board, irrespective of the number of dialog layers.
class Backdrop final : public juce::Component, private juce::ComponentListener, private juce::Timer
{
public:
    explicit Backdrop(juce::Component& board) : root(board)
    {
        setComponentID("popup-backdrop");setWantsKeyboardFocus(false);
        root.addChildComponent(*this);root.addComponentListener(this);
        setBounds(root.getLocalBounds());setAlpha(0);
    }
    ~Backdrop() override
    {
        root.removeComponentListener(this);
        for(auto& window:windows)if(window)window->removeComponentListener(this);
    }
    juce::Component& board() const {return root;}
    void attach(juce::Component& window)
    {windows.emplace_back(&window);window.addComponentListener(this);refresh();}
    void detach(juce::Component& window)
    {
        window.removeComponentListener(this);
        windows.erase(std::remove_if(windows.begin(),windows.end(),[&](const auto& p){return !p||p==&window;}),windows.end());refresh();
    }
    bool hasDialogs() const
    {for(const auto& window:windows)if(window&&window->isVisible())return true;return false;}
    void advance(double dt)
    {
        const auto next=fade.advance(hasDialogs(),dt);setAlpha(next);
        if(next==0&&!hasDialogs())setVisible(false);
        if(next==(hasDialogs()?1.f:0.f))stopTimer();
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colours::black.withAlpha(.5f));}
private:
    void refresh()
    {
        const bool active=hasDialogs();setInterceptsMouseClicks(active,false);
        if(active){setVisible(true);front();}
        last=juce::Time::getMillisecondCounterHiRes();startTimerHz(60);
    }
    void front(){if(!fronting&&isVisible()){juce::ScopedValueSetter<bool> guard(fronting,true);toFront(false);}}
    void componentChildrenChanged(juce::Component& c) override {if(&c==&root)front();}
    void componentMovedOrResized(juce::Component& c,bool,bool) override {if(&c==&root)setBounds(root.getLocalBounds());}
    void componentVisibilityChanged(juce::Component& c) override {if(&c!=&root)refresh();}
    void componentBeingDeleted(juce::Component& c) override {if(&c!=&root)detach(c);}
    void timerCallback() override
    {const auto now=juce::Time::getMillisecondCounterHiRes();advance((now-last)/1000.);last=now;}
    juce::Component& root;
    std::vector<juce::Component::SafePointer<juce::Component>> windows;
    motion::CardFade fade;double last=0;bool fronting=false;
};

class DismissibleDialog : public juce::DialogWindow
{
public:
    DismissibleDialog(const juce::String& title,juce::Component& owner,bool desktop=true)
        : DialogWindow(title,juce::Colours::transparentBlack,true,desktop,
                       juce::Component::getApproximateScaleFactorForComponent(owner.getTopLevelComponent()))
    {
        juce::Component* source=&owner;
        auto* parent=dynamic_cast<DismissibleDialog*>(&owner);
        if(!parent)parent=owner.findParentComponentOfClass<DismissibleDialog>();
        if(parent)source=parent->board;
        for(auto* candidate=source;candidate;candidate=candidate->getParentComponent())
            for(auto* child:candidate->getChildren())if(auto* shade=dynamic_cast<Backdrop*>(child))
            {backdrop=shade;board=&shade->board();shade->attach(*this);return;}
        board=source;
    }
    ~DismissibleDialog() override {if(backdrop)backdrop->detach(*this);}
    void closeButtonPressed() override {exitModalState(0);setVisible(false);}
    void inputAttemptWhenModal() override
    {
        // JUCE also calls this during blocked keyboard focus traversal.
        // Only a pointer press is an outside-click dismissal.
        bool pressed=false;
        for(const auto& source:juce::Desktop::getInstance().getMouseSources())pressed|=source.isDragging();
        if(pressed)dismissOutside();
    }
    void dismissOutside()
    {
        if(closePending)return;closePending=true;
        // Closing inside JUCE's mouse-down routing would replay the same click
        // into the newly unblocked control. Defer until that event has finished.
        const juce::Component::SafePointer<DismissibleDialog> safe(this);
        juce::MessageManager::callAsync([safe]
        {
            if(!safe)return;safe->closePending=false;
            if(safe->isVisible()&&juce::Component::getCurrentlyModalComponent()==safe.getComponent())safe->closeButtonPressed();
        });
    }
private:
    juce::Component::SafePointer<Backdrop> backdrop;
    juce::Component* board=nullptr;bool closePending=false;
};
}
