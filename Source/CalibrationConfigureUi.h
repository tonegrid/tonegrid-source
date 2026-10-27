#pragma once
#include "Shared/UI/PopupStyle.h"
#include "Content/Calibration/InterfaceProfiles.h"

namespace tonegrid::calibration
{
// Presentation of the existing verified settings; never used by the audio engine.
struct ConfigureCopy
{
    bool minimum=false;
    juce::String input,confirmation,gain,gainConfirmation,details;
};
inline ConfigureCopy configureCopy(const InterfaceProfile* profile)
{
    ConfigureCopy copy;
    copy.input="Plug your guitar into your audio interface's instrument input.\nIf your interface has an INST or Hi-Z option, make sure it's enabled.";
    copy.confirmation="Connected to the guitar input (INST / Hi-Z on, if available).";
    if(!profile)
    {
        copy.minimum=true;
        copy.details="For example: plug into Input 1, then press its INST button if it has one.";
        copy.gain="Start with the input GAIN knob fully left, at minimum.\nIf using an exact dBu value, follow its specified gain setting.";
        copy.gainConfirmation="I have set my interface's input gain as described above.";
        return copy;
    }
    copy.details=profile->settings;
    const auto& settings=profile->settings;
    if(settings.containsIgnoreCase("Front D.I. input")||settings.containsIgnoreCase("Front Hi-Z input")||settings.containsIgnoreCase("Front TRS instrument input"))
    {
        copy.input="Plug your guitar cable into the instrument input shown below.\nThis input is ready for guitar: no INST switch is needed.";
        copy.confirmation="Connected to the instrument input shown above.";
    }
    copy.minimum=settings.containsIgnoreCase("gain at minimum");
    copy.gain=copy.minimum?"Turn the input GAIN knob all the way down (fully left).\nUse the input gain, not the headphone or monitor volume.":"Use the GAIN setting shown above in your interface's control app.\nFor example, 0 dB means no extra input gain.";
    copy.gainConfirmation=copy.minimum?"My interface's GAIN knob is fully down.":"My interface's GAIN matches the required settings above.";
    if(profile->inputInstructions.isNotEmpty())copy.input=profile->inputInstructions;
    if(profile->gainInstructions.isNotEmpty()){copy.gain=profile->gainInstructions;copy.minimum=false;}
    if(profile->gainConfirmation.isNotEmpty())copy.gainConfirmation=profile->gainConfirmation;
    return copy;
}
inline ConfigureCopy conciseConfigureCopy(const InterfaceProfile* profile)
{
    auto copy=configureCopy(profile);
    if(!profile){copy.input="Plug into the instrument input.\nEnable INST / Hi-Z if available.";copy.gain="Start with input GAIN fully left (minimum).\nFor an exact dBu value, use the manufacturer's gain setting.";return copy;}
    const auto& id=profile->id;const auto& s=profile->settings;
    if(profile->inputInstructions.isEmpty())
    {
        juce::String channel;
        if(profile->channels.size()==1)channel=juce::String(profile->channels.front()+1);
        else if(profile->channels.size()==2)channel=juce::String(profile->channels.front()+1)+" or "+juce::String(profile->channels.back()+1);
        else channel=juce::String(profile->channels.front()+1)+" to "+juce::String(profile->channels.back()+1);
        const bool dedicated=s.containsIgnoreCase("D.I.")||s.containsIgnoreCase("Front Hi-Z")||s.containsIgnoreCase("dedicated")||s.containsIgnoreCase("Front TRS");
        copy.input="Use instrument input "+channel+".\n"+(dedicated?"No INST switch is needed.":"Enable INST / Hi-Z if available.");
        if(id=="scarlett-2i2-3")copy.input="Solo: input 2. Other models: input 1 or 2.\nSwitch INST on.";
        if(id=="scarlett-2i2-4")copy.input="Solo: input 1. Other models: input 1 or 2.\nSwitch INST on.";
        if(id=="maudio-air")copy.input="AIR 192|4: input 2. AIR 192|6: input 1 or 2.\nUse the dedicated instrument input.";
        if(id=="maudio-mtrack-solo-duo")copy.input="Solo: input 2. Duo: input 1 or 2.\nSwitch INST on.";
        if(id=="arturia-minifuse2")copy.input="MiniFuse 1: input 1. MiniFuse 2 / 4: input 1 or 2.\nSwitch INST on.";
        if(id=="mackie-onyx-usb")copy.input="Artist: input 2. Producer: input 1 or 2.\nSwitch Hi-Z on.";
        if(id=="ua-volt2")copy.input="Volt 1 / 176: input 1. Volt 2 / 276: input 1 or 2.\nSwitch INST on.";
        if(id=="presonus-quantum-es2")copy.input="ES 2: input 1. ES 4: input 1 or 2.\nUse the instrument input.";
        if(id=="ssl2-original")copy.input="Use input 1 or 2.\nSwitch LINE and HI-Z on.";
    }
    if(profile->gainInstructions.isNotEmpty())return copy;
    if(!copy.minimum)
    {
        if(id=="apogee-boom")copy.gain="Apogee Control: GAIN 0 dB.\nTurn PAD and ECS effects off.";
        else if(id=="rme-babyface-pro-fs")copy.gain="TotalMix: input level +4 dBu and GAIN 0 dB.\nTurn input effects off.";
        else copy.gain=profile->settings;
        return copy;
    }
    copy.gain="Turn input GAIN fully left (minimum).";
    juce::StringArray off;
    for(const auto& label:{"AIR","PAD","Smartgain","Auto Gain","Clip Safe","VINTAGE","76 compressor","4K","high-pass filter","input effects","loopback"})
        if(s.containsIgnoreCase(label)&&s.containsIgnoreCase("off"))off.add(label);
    if(s.containsIgnoreCase("software input boost"))off.add("input boost");
    if(!off.isEmpty())copy.gain+="\nTurn off "+off.joinIntoString(", ")+".";
    if(id.startsWith("ik-axeio"))copy.gain+="\nSelect PASSIVE and PURE; turn Z-TONE fully left.";
    if(id=="steinberg-ur22c")copy.gain+="\nKeep DSP input gain and effects neutral.";
    return copy;
}
class DbuHelpLink final:public juce::Button
{
public:
    DbuHelpLink():Button("What is dBu?")
    {setComponentID("calibration-dbu-help");setMouseCursor(juce::MouseCursor::PointingHandCursor);setWantsKeyboardFocus(true);}
    void paintButton(juce::Graphics& g,bool over,bool) override
    {
        g.setColour(juce::Colour(0xffc4ac7d));g.setFont(juce::FontOptions(14.f,over||hasKeyboardFocus(true)?juce::Font::underlined:juce::Font::plain));
        g.drawText(getButtonText(),getLocalBounds(),juce::Justification::centredLeft);
    }
};
class MinimumGainIllustration final:public juce::Component
{
public:
    MinimumGainIllustration()
    {setComponentID("calibration-minimum-gain");setTitle("Physical GAIN knob at minimum, fully counterclockwise");setInterceptsMouseClicks(false,false);}
    void paint(juce::Graphics& g) override
    {
        using namespace juce;const auto centre=juce::Point<float>(getWidth()*.5f,39.f);const auto knob=juce::Rectangle<float>(centre.x-27,12,54,54);
        g.setColour(Colours::black.withAlpha(.55f));g.fillEllipse(knob.translated(0,2));
        g.setGradientFill(ColourGradient(Colour(0xffaeb5b5),knob.getX(),knob.getY(),Colour(0xff333c40),knob.getRight(),knob.getBottom(),false));g.fillEllipse(knob);
        g.setColour(Colour(0xff252c2e));g.drawEllipse(knob.reduced(.5f),1.f);
        Path arc;arc.addCentredArc(centre.x,centre.y,32,32,0,MathConstants<float>::pi*1.25f,MathConstants<float>::pi*2.75f,true);
        g.setColour(popup::outline());g.strokePath(arc,PathStrokeType(2.f));
        // Minimum is the lower-left end of a conventional 270-degree gain sweep.
        g.setColour(popup::ink());g.drawLine(centre.x-7,centre.y+7,centre.x-19,centre.y+19,2.3f);
        g.setFont(FontOptions(11.f));g.drawText(tonegrid::l10n::translate("MINIMUM"),0,77,getWidth(),18,Justification::centred);
    }
};
}
