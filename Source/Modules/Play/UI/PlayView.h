#pragma once
#include "Modules/Play/UI/PedalCard.h"
#include "Modules/Play/UI/PedalScene.h"
#include "Modules/Play/UI/RecordingControls.h"
#include "Modules/Play/UI/LoopControls.h"
namespace tonegrid::play {
class View final : public juce::Component {
public:
    View(recording::SessionRecorder& recorder, looping::Engine& looper)
        : recordingControls(recorder), loopControls(looper) {
        addAndMakeVisible(recordingControls);addAndMakeVisible(loopControls);
    }
    void paint(juce::Graphics& g) override { assets::paintPedalScene(g, getLocalBounds()); }
    void paintOverChildren(juce::Graphics& g) override {
        if(hintOpacity<=0)return;
        const auto actions=actionBounds();
        const std::array<const char*,4> labels{"Play","Record","Download","Looper"};
        g.setFont(ui::font(12));g.setColour(juce::Colours::white.withAlpha(hintOpacity));
        for(size_t i=0;i<actions.size();++i){const auto r=actions[i];g.drawText(labels[i],juce::Rectangle<int>(r.getRight()+8,r.getY(),juce::jmax(0,getWidth()-r.getRight()-10),r.getHeight()),juce::Justification::centredLeft);}
    }
    void setHintOpacity(float opacity){if(hintOpacity!=opacity){hintOpacity=opacity;repaint();}}
    std::array<juce::Rectangle<int>,4> actionBounds()const {
        const auto r=recordingControls.actionBounds();const auto p=recordingControls.getPosition();
        return {r[0].translated(p.x,p.y),r[1].translated(p.x,p.y),r[2].translated(p.x,p.y),loopControls.getBounds().withSizeKeepingCentre(juce::jmin(46,loopControls.getWidth()),juce::jmin(46,loopControls.getHeight()))};
    }
    void resized() override {
        const auto area = getLocalBounds();
        const auto bounds = assets::boardBounds(area);
        for (int i=0;i<board::count;++i) if (cards[size_t(i)]) cards[size_t(i)]->setBounds(bounds[size_t(i)]);
        const auto last=bounds[12];
        const int width=juce::jmax(1,juce::jmin(64,area.getRight()-last.getRight()-12));
        recordingControls.setBounds(last.getRight()+8,last.getCentreY()-118,width,184);
        loopControls.setBounds(last.getRight()+8,last.getCentreY()+74,width,46);
    }
    std::array<std::unique_ptr<PedalCard>,board::count> cards;
    recording::Controls recordingControls;
    looping::Controls loopControls;
private:
    float hintOpacity=0;
};
}
