#pragma once
#include "OutputDspParameters.h"
#include "AmpUi.h"
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/ControlHelp.h"
namespace tonegrid
{
inline juce::String spreadHelp(const juce::String& id)
{
    if(id=="spread-offset")return "Adds a small timing difference between the left and right channels to create stereo width.";
    if(id=="spread-wobble"||id=="spread-wobble-on")return "Adds subtle movement so the stereo image feels less static.";
    if(id=="spread-diffuse")return "Softens and spreads transients to create a smoother, more spacious stereo image.";
    if(id=="spread-crossover"||id=="spread-crossover-on")return "Sets the frequency above which widening becomes stronger, helping keep low frequencies focused.";
    return "Creates a wider, more immersive stereo image after the amp while keeping the core guitar tone focused.";
}
// UI adapter only; the DSP has no dependency on this Component or its callbacks.
class SpreadControls final:public popup::View,public juce::SettableTooltipClient
{
public:
    SpreadControls()
    {
        using namespace juce;
        for(auto* knob:{&offset,&wobble,&crossover}){addAndMakeVisible(knob);knob->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);knob->setTextBoxStyle(Slider::TextBoxBelow,false,100,28);knob->onValueChange=[this,knob]{help.setCopy(spreadHelp(knob->getComponentID()));changed();};}
        ampui::configure(offset,-24,24);offset.setTextValueSuffix(" ms");
        wobble.setRange(0,100,1);wobble.setTextValueSuffix(" %");
        NormalisableRange<double> hz(32.5,520,[](double low,double high,double p){return low*std::pow(high/low,p);},[](double low,double high,double v){return std::log(v/low)/std::log(high/low);});hz.interval=.1;
        crossover.setNormalisableRange(hz);crossover.setTextValueSuffix(" Hz");
        offset.setComponentID("spread-offset");wobble.setComponentID("spread-wobble");crossover.setComponentID("spread-crossover");enabled.setComponentID("spread-enable");
        wobbleOn.setComponentID("spread-wobble-on");crossoverOn.setComponentID("spread-crossover-on");diffuseOn.setComponentID("spread-diffuse");
        for(auto* toggle:std::initializer_list<Button*>{&enabled,&wobbleOn,&crossoverOn,&diffuseOn}){addAndMakeVisible(toggle);toggle->setClickingTogglesState(true);toggle->onClick=[this,toggle]{help.setCopy(spreadHelp(toggle->getComponentID()));changed();};}
        addAndMakeVisible(help);help.setComponentID("spread-help");help.setFont(ui::font(16));help.setJustificationType(Justification::topLeft);help.setCopy(spreadHelp({}));addMouseListener(this,true);
        offset.setTooltip("Signed ADT offset: positive delays right, negative delays left. Active Spread uses amp channel L as its mono source; bypass preserves L/R.");
        wobble.setTooltip("Slow random-walk depth: 100% spans +/-1.2 ms.");crossover.setTooltip("LR4 crossover: low band stays identical left/right.");
        sync({});setSize(700,popupContentHeight(700));
    }
    int popupMinimumWidth()const override{return 700;}
    int popupContentHeight(int width)const override{return juce::jmax(324,help.contentHeight(width/2-48)+72);}
    output::Settings state() const
    {output::Settings s;s.spreadEnabled=enabled.getToggleState();s.offsetMs=(float)offset.getValue();s.wobblePercent=(float)wobble.getValue();s.crossoverHz=(float)crossover.getValue();s.wobbleEnabled=wobbleOn.getToggleState();s.crossoverEnabled=crossoverOn.getToggleState();s.diffuseEnabled=diffuseOn.getToggleState();return s;}
    void sync(const output::Settings& s)
    {
        using namespace juce;enabled.setToggleState(s.spreadEnabled,dontSendNotification);enabled.setButtonText(s.spreadEnabled?"ON":"OFF");
        wobbleOn.setToggleState(s.wobbleEnabled,dontSendNotification);crossoverOn.setToggleState(s.crossoverEnabled,dontSendNotification);diffuseOn.setToggleState(s.diffuseEnabled,dontSendNotification);
        offset.setValue(s.offsetMs,dontSendNotification);wobble.setValue(s.wobblePercent,dontSendNotification);crossover.setValue(s.crossoverHz,dontSendNotification);
    }
    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        g.setColour(popup::mutedInk());g.setFont(ui::font(16,juce::Font::bold));g.drawFittedText(l10n::tr("effects.about"),getWidth()/2+24,0,getWidth()/2-48,32,Justification::centredLeft,2,1.f);
        g.setColour(popup::outline().withAlpha(.6f));g.drawVerticalLine(getWidth()/2,0,(float)getHeight()-18);
        g.setFont(FontOptions("Segoe UI",14.f,Font::plain));g.setColour(popup::ink());
        for(int i=0;i<3;++i)g.drawText(StringArray{"Offset","Wobble","X-Over"}[i],12+i*110,76,106,24,Justification::centred);
    }
    void resized() override
    {
        const auto half=getWidth()/2;enabled.setBounds(12,16,90,38);diffuseOn.setBounds(half-140,16,120,38);
        offset.setBounds(12,104,106,128);wobble.setBounds(122,104,106,128);crossover.setBounds(232,104,106,128);
        wobbleOn.setBounds(122,254,106,32);crossoverOn.setBounds(232,254,106,32);help.setBounds(half+24,48,half-48,getHeight()-72);
    }
    void mouseEnter(const juce::MouseEvent& e) override {updateHelp(e);}
    void mouseMove(const juce::MouseEvent& e) override {updateHelp(e);}
    std::function<void()> onChange;
private:
    void updateHelp(const juce::MouseEvent& e){for(auto* c=e.originalComponent;c&&c!=this;c=c->getParentComponent())if(c->getComponentID().startsWith("spread-")&&c!=&help){help.setCopy(spreadHelp(c->getComponentID()));return;}}
    void changed(){enabled.setButtonText(enabled.getToggleState()?"ON":"OFF");if(onChange)onChange();}
    juce::Slider offset,wobble,crossover;
    ui::ControlHelp help;
    juce::TextButton enabled{"OFF"};juce::ToggleButton wobbleOn{"Wobble"},crossoverOn{"X-over"},diffuseOn{"Diffuse"};
};
}
