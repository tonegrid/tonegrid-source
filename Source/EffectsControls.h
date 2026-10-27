#pragma once
#include "EffectsParameters.h"
#include "Effects/Delay/DigitalDelay.h"
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/ControlHelp.h"
namespace tonegrid
{
enum class EffectsPanel { Pitch, Mod, Delay };
inline juce::String effectHelp(EffectsPanel panel,const juce::String& id,mod::Type type,pitch::Mode mode)
{
    if(panel==EffectsPanel::Pitch)
    {
        if(id=="semitones")return "Shifts the entire guitar signal up or down by up to two octaves. The output is fully transposed, with no dry blend.";
        if(id=="dry")return "Sets the original guitar level. It is delayed to match the two octave voices.";
        if(id=="down")return "Sets the level of the voice one octave below your guitar.";
        if(id=="up")return "Sets the level of the voice one octave above your guitar.";
        return mode==pitch::Mode::Off?"Select Transpose or Octaver in the top bar. OFF adds no pitch latency.":"Pitch acts after the noise gate and before the thirteen pedals. Active pitch processing adds a small delay; try chords and strong attacks when checking your sound.";
    }
    if(panel==EffectsPanel::Delay)
    {
        if(id=="time")return "Controls the time between the original note and each repeat.";
        if(id=="feedback")return "Controls how many repeats are generated.";
        if(id=="mix")return "Controls how much delayed signal is blended with the original guitar. 100% gives only the echoes.";
        if(id=="tap")return "Tap repeatedly to set the delay time by feel.";
        return "Creates clean repeating echoes of the guitar signal. Digital Delay follows MOD and comes before Reverb.";
    }
    if(id=="rate")return "Controls how fast the modulation moves.";
    if(id=="depth")return "Controls how strong the modulation effect becomes.";
    if(id=="level")return "Blends in the modulation. Tremolo Level controls the strength of the volume pulse; Chorus, Phaser and Flanger blend their moving sound with your guitar.";
    switch(type)
    {case mod::Type::Tremolo:return "Rhythmically changes the guitar volume to create a smooth pulsing movement.";
    case mod::Type::Phaser:return "Creates sweeping phase cancellations for a smooth, swirling character.";
    case mod::Type::Flanger:return "Uses a very short modulated delay to create a more pronounced sweeping effect.";
    default:return "Adds slightly detuned, moving copies of the guitar to create width and shimmer.";}
}
class EffectsControls final:public popup::View
{
public:
    explicit EffectsControls(EffectsPanel p):panel(p)
    {
        using namespace juce;
        if(panel==EffectsPanel::Mod)
        {
            type.setComponentID("mod-type");type.addItemList({"Chorus","Tremolo","Phaser","Flanger"},1);addAndMakeVisible(type);
            type.onChange=[this]{const bool enabled=modState.enabled;modState=mod::voicing(mod::Type(type.getSelectedId()-1));modState.enabled=enabled;sync(modState);publish();showHelp({});};
        }
        for(size_t i=0;i<knobs.size();++i)
        {
            auto& s=knobs[i];addAndMakeVisible(s);s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::TextBoxBelow,false,100,28);
            s.onValueChange=[this,i]{publish();showHelp(ids[i]);};
        }
        tap.setComponentID("tap");tap.setButtonText("TAP");l10n::tooltip(tap,effectHelp(panel,"tap",modState.type,pitchState.mode));
        if(panel==EffectsPanel::Delay){addAndMakeVisible(tap);tap.onClick=[this]{applyTap(juce::Time::getMillisecondCounterHiRes());};}
        addAndMakeVisible(help);help.setFont(ui::font(16));help.setJustificationType(Justification::topLeft);addMouseListener(this,true);
        if(panel==EffectsPanel::Pitch)sync(pitch::Parameters{});else if(panel==EffectsPanel::Mod)sync(mod::Parameters{});else sync(delay::Parameters{});
        showHelp({});setSize(700,popupContentHeight(700));
    }
    int popupMinimumWidth()const override{return 700;}
    int popupContentHeight(int width)const override{return juce::jmax(324,help.contentHeight(width/2-48)+72);}
    void sync(const pitch::Parameters& p)
    {
        pitchState=p;const bool octave=p.mode==pitch::Mode::Octaver;
        ids=octave?std::array<juce::String,3>{"dry","down","up"}:std::array<juce::String,3>{"semitones","",""};
        names=octave?juce::StringArray{"DRY","DOWN","UP"}:juce::StringArray{"SEMITONES","",""};
        for(int i=0;i<3;++i){knobs[i].setVisible(p.mode!=pitch::Mode::Off&&(octave||i==0));range(knobs[i],octave?0:-24,octave?100:24,1,octave?" %":" st");}
        setValues(octave?std::array<double,3>{p.dry,p.down,p.up}:std::array<double,3>{double(p.semitones),0,0});showHelp({});repaint();
    }
    void sync(const mod::Parameters& p)
    {
        modState=p;ids={"rate","depth","level"};names={"RATE","DEPTH","LEVEL"};type.setSelectedId(int(p.type)+1,juce::dontSendNotification);
        range(knobs[0],.05,10,.01," Hz");knobs[0].setSkewFactorFromMidPoint(1);range(knobs[1],0,100,1," %");range(knobs[2],0,100,1," %");setValues({p.rateHz,p.depth,p.level});repaint();
    }
    void sync(const delay::Parameters& p)
    {
        delayState=p;ids={"time","feedback","mix"};names={"TIME","FEEDBACK","MIX"};range(knobs[0],1,2000,.1," ms");knobs[0].setSkewFactorFromMidPoint(320);range(knobs[1],0,95,1," %");range(knobs[2],0,100,1," %");setValues({p.timeMs,p.feedback,p.mix});repaint();
    }
    pitch::Parameters pitchSettings() const {auto p=pitchState;if(p.mode==pitch::Mode::Octaver){p.dry=float(knobs[0].getValue());p.down=float(knobs[1].getValue());p.up=float(knobs[2].getValue());}else if(p.mode==pitch::Mode::Transpose)p.semitones=int(knobs[0].getValue());return p;}
    mod::Parameters modSettings() const {return {modState.enabled,modState.type,float(knobs[0].getValue()),float(knobs[1].getValue()),float(knobs[2].getValue())};}
    delay::Parameters delaySettings() const {return {delayState.enabled,float(knobs[0].getValue()),float(knobs[1].getValue()),float(knobs[2].getValue())};}
    void applyTap(double timestamp) {if(const auto ms=tapper.tap(timestamp);ms>0)knobs[0].setValue(ms,juce::sendNotificationSync);}
    void paint(juce::Graphics& g) override
    {
        using namespace juce;g.setColour(popup::outline().withAlpha(.6f));g.drawVerticalLine(getWidth()/2,0,float(getHeight()-18));
        g.setColour(popup::mutedInk());g.setFont(ui::font(16,juce::Font::bold));
        g.drawFittedText(l10n::tr("effects.about"),getWidth()/2+24,0,getWidth()/2-48,32,Justification::centredLeft,2,1.f);
        g.setColour(Colours::white);g.setFont(FontOptions("Segoe UI",13.f,Font::plain));
        if(panel==EffectsPanel::Mod)g.drawText("TYPE",20,20,68,28,Justification::centredLeft);
        if(panel==EffectsPanel::Pitch&&pitchState.mode==pitch::Mode::Off)g.drawFittedText(l10n::translate("Select a Pitch mode in the top bar."),20,54,300,70,Justification::centredLeft,3,1.f);
        for(int i=0;i<3;++i)if(knobs[i].isVisible())g.drawText(names[i],20+i*106,74,100,24,Justification::centred);
    }
    void resized() override
    {type.setBounds(94,16,getWidth()/2-114,38);for(int i=0;i<3;++i)knobs[i].setBounds(20+i*106,104,100,120);tap.setBounds(20,getHeight()-68,100,38);help.setBounds(getWidth()/2+24,48,getWidth()/2-48,getHeight()-72);}
    void mouseEnter(const juce::MouseEvent& e) override {hoverHelp(e);}
    void mouseMove(const juce::MouseEvent& e) override {hoverHelp(e);}
    std::function<void()> onChange;
private:
    static void range(juce::Slider& s,double low,double high,double step,const juce::String& suffix){s.setRange(low,high,step);s.setTextValueSuffix(suffix);}
    void setValues(std::array<double,3> v){for(int i=0;i<3;++i){knobs[i].setComponentID(ids[i]);knobs[i].setValue(v[i],juce::dontSendNotification);l10n::tooltip(knobs[i],effectHelp(panel,ids[i],modState.type,pitchState.mode));}}
    void publish(){if(onChange)onChange();}
    void showHelp(const juce::String& id){help.setCopy(effectHelp(panel,id,modState.type,pitchState.mode));}
    void hoverHelp(const juce::MouseEvent& e){for(auto* c=e.originalComponent;c&&c!=this;c=c->getParentComponent())if(c->getComponentID().isNotEmpty()){showHelp(c->getComponentID());return;}}
    EffectsPanel panel;std::array<juce::Slider,3> knobs;std::array<juce::String,3> ids;juce::StringArray names;
    juce::ComboBox type;juce::TextButton tap;ui::ControlHelp help;TapTime tapper;
    pitch::Parameters pitchState;mod::Parameters modState;delay::Parameters delayState;
};
}
