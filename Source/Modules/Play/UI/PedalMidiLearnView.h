#pragma once
#include "Shared/UI/PopupWindow.h"
#include "MidiBindings.h"

namespace tonegrid
{
class PedalMidiLearnView final:public popup::View
{
public:
    explicit PedalMidiLearnView(const juce::String& pedal)
    {
        using namespace juce;
        setComponentID("pedal-midi-learn-view");
        for(auto* component:std::initializer_list<Component*>{&heading,&status,&again,&clear,&close})addAndMakeVisible(component);
        heading.setText(pedal,dontSendNotification);heading.setFont(ui::font(20.f));heading.setColour(Label::textColourId,popup::ink());
        status.setFont(ui::font(15.f));status.setColour(Label::textColourId,popup::mutedInk());status.setJustificationType(Justification::topLeft);
        status.setComponentID("pedal-midi-status");again.setComponentID("pedal-midi-relearn");clear.setComponentID("pedal-midi-clear");close.setComponentID("pedal-midi-close");
        again.onClick=[this]{if(onLearn)onLearn();};clear.onClick=[this]{if(onClear)onClear();};close.onClick=[this]{if(onClose)onClose();};
        setSize(460,210);
    }
    void update(const rig::Binding& binding,bool learning,bool available)
    {
        using namespace juce;
        const auto text=learning?String("Press a button on your MIDI controller.\nToneGrid is listening...")
            :binding.valid()?String("Assigned: ")+binding.text()+"\nYou can close this window or learn another button."
            :available?String("Click Learn, then press a button on your MIDI controller.")
            :String("First select a MIDI input at the top of ToneGrid.\nThen reopen LEARN MIDI for this pedal.");
        status.setText(text,dontSendNotification);again.setButtonText(learning?"Listening...":"Learn");again.setEnabled(available&&!learning);
        clear.setEnabled(binding.valid());close.setButtonText(learning?"Cancel":"Close");
    }
    void feedback(const juce::String& text){status.setText(text,juce::dontSendNotification);}
    int popupMinimumWidth()const override{return 240;}
    int popupContentHeight(int width)const override{return width<400?260:210;}
    void resized()override
    {
        auto area=getLocalBounds();heading.setBounds(area.removeFromTop(32));area.removeFromTop(10);status.setBounds(area.removeFromTop(100));
        auto buttons=getLocalBounds().removeFromBottom(getWidth()<400?84:38);
        if(getWidth()<400){auto top=buttons.removeFromTop(38);again.setBounds(top.removeFromLeft((top.getWidth()-8)/2));top.removeFromLeft(8);clear.setBounds(top);buttons.removeFromTop(8);close.setBounds(buttons);}
        else{close.setBounds(buttons.removeFromRight(100));buttons.removeFromRight(8);clear.setBounds(buttons.removeFromRight(150));buttons.removeFromRight(8);again.setBounds(buttons.removeFromRight(110));}
    }
    std::function<void()> onLearn,onClear,onClose;
private:
    juce::Label heading,status;juce::TextButton again{"Learn"},clear{"Clear assignment"},close{"Close"};
};
}
