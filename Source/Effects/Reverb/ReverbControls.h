#pragma once
#include "Effects/Reverb/ReverbParameters.h"
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/ControlHelp.h"
namespace tonegrid
{
inline juce::String reverbHelp(const juce::String& id,reverb::Type type)
{
    if(id=="reverb-type")
    {
        if(type==reverb::Type::Room)return "Shorter, natural reflections that make the guitar feel like it is playing in a real space.";
        if(type==reverb::Type::Plate)return "Smooth, dense and musical reverb with a classic studio character.";
        if(type==reverb::Type::Spring)return "Simulates the lively metallic character of a traditional spring reverb tank, with the splash and drip associated with classic guitar amplifiers.";
        return "Creates a wider and deeper space with a longer, more atmospheric tail.";
    }
    if(id=="reverb-time"&&type==reverb::Type::Spring)return "Controls how long the spring tail rings. Start with a moderate time to keep chords clear.";
    if(id=="reverb-color"&&type==reverb::Type::Spring)return "Moves the spring tone from warm and mellow to brighter and more present.";
    if(id=="reverb-time")return "Controls how long the reverb tail takes to fade away. Seconds are an approximate musical target, not a measured RT60.";
    if(id=="reverb-predelay")return "Adds a short gap before the reverb begins, helping the original guitar attack stay clear.";
    if(id=="reverb-color")return "Adjusts the tonal character of the reverb from darker and warmer to brighter and more present.";
    if(id=="reverb-mix")return "Controls how much reverb is blended with the dry guitar signal. Start at 10-25% for guitar.";
    return "Adds a sense of space and depth after the amplifier. Room, Plate and Large share the existing engine; Spring uses a dedicated spring tank.";
}
class ReverbControls final:public popup::View
{
public:
    ReverbControls()
    {
        using namespace juce;
        type.setComponentID("reverb-type");type.addItem("Room",1);type.addItem("Plate",2);type.addItem("Large",3);type.addItem("Spring",4);
        addAndMakeVisible(type);type.onChange=[this]{updateTooltips();if(onChange)onChange();};
        time.setComponentID("reverb-time");delay.setComponentID("reverb-predelay");color.setComponentID("reverb-color");mix.setComponentID("reverb-mix");
        time.setRange(reverb::minimumTime,reverb::maximumTime,.01);time.setSkewFactorFromMidPoint(2);time.setTextValueSuffix(" s");delay.setRange(0,reverb::maximumPreDelay,.1);delay.setTextValueSuffix(" ms");
        color.setRange(0,reverb::maximumPercent,1);mix.setRange(0,reverb::maximumPercent,1);color.setTextValueSuffix(" %");mix.setTextValueSuffix(" %");
        for(auto* knob:{&time,&delay,&color,&mix}){addAndMakeVisible(knob);knob->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);knob->setTextBoxStyle(Slider::TextBoxBelow,false,110,28);knob->onValueChange=[this]{if(onChange)onChange();};}
        addAndMakeVisible(help);help.setFont(ui::font(16));help.setJustificationType(Justification::topLeft);
        help.setCopy("Time\nHow long the reverb tail rings.\n\nPre-delay\nThe gap before the reverb starts, keeping the guitar attack clear.\n\nColor\nFrom dark and warm to bright and present.\n\nMix\nThe amount of reverb blended with your dry guitar.");
        sync({});setSize(700,popupContentHeight(700));
    }
    int popupMinimumWidth()const override{return 700;}
    int popupContentHeight(int width)const override{return juce::jmax(404,help.contentHeight(width/2-48)+60);}
    reverb::Parameters state(bool enabled) const
    {return {enabled,reverb::Type(type.getSelectedId()-1),float(time.getValue()),float(delay.getValue()),float(color.getValue()),float(mix.getValue())};}
    void sync(const reverb::Parameters& p)
    {
        using namespace juce;type.setSelectedId(int(p.type)+1,dontSendNotification);
        time.setValue(p.timeSeconds,dontSendNotification);delay.setValue(p.preDelayMs,dontSendNotification);color.setValue(p.color,dontSendNotification);mix.setValue(p.mixPercent,dontSendNotification);
        updateTooltips();
    }
    void paint(juce::Graphics& g) override
    {
        using namespace juce;g.setColour(popup::outline().withAlpha(.6f));g.drawVerticalLine(getWidth()/2,0,float(getHeight()-18));g.setColour(popup::mutedInk());g.setFont(ui::font(16,juce::Font::bold));g.drawFittedText(l10n::tr("effects.about"),getWidth()/2+24,0,getWidth()/2-48,32,Justification::centredLeft,2,1.f);
        g.setFont(FontOptions("Segoe UI",13.f,Font::plain));g.setColour(Colours::white);g.drawText("Type",20,20,68,26,Justification::centredLeft);
        for(int i=0;i<4;++i)g.drawText(juce::StringArray{"Time","Pre-delay","Color","Mix"}[i],20+(i%2)*160,68+(i/2)*160,140,24,Justification::centred);
    }
    void resized() override
    {
        const auto half=getWidth()/2;type.setBounds(94,16,half-114,38);
        time.setBounds(20,98,140,120);delay.setBounds(180,98,140,120);color.setBounds(20,258,140,120);mix.setBounds(180,258,140,120);
        help.setBounds(half+24,48,half-48,getHeight()-60);
    }
    std::function<void()> onChange;
private:
    void updateTooltips(){const auto selected=reverb::Type(type.getSelectedId()-1);l10n::tooltip(type,reverbHelp(type.getComponentID(),selected));for(auto* knob:{&time,&delay,&color,&mix})l10n::tooltip(*knob,reverbHelp(knob->getComponentID(),selected));}
    juce::ComboBox type;juce::Slider time,delay,color,mix;ui::ControlHelp help;
};
}
