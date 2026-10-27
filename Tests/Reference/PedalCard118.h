#pragma once
#include "MidiBindings.h"
#include "Effects/Pedalboard/PedalCommands.h"
#include "Shared/UI/UiStyle.h"
#include <BinaryData.h>
namespace pilotReferenceUi
{
namespace ui=tonegrid::ui;
class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        using namespace juce;
        setColour(ComboBox::backgroundColourId,ui::background());setColour(ComboBox::outlineColourId,ui::outline());
        setColour(TextButton::buttonColourId,ui::background());setColour(TextButton::textColourOffId,Colours::white);
        setColour(PopupMenu::backgroundColourId,Colour(0xff10222c));setColour(TextEditor::backgroundColourId,Colour(0xff091b23));
        setColour(TextEditor::outlineColourId,ui::outline());setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
    }
    juce::Font getComboBoxFont(juce::ComboBox& c)override{return juce::FontOptions("Segoe UI",c.getComponentID()=="pedal-preset"?juce::jlimit(10.f,12.f,c.getHeight()*.43f):12.f,juce::Font::plain);}
    juce::Font getTextButtonFont(juce::TextButton& b,int)override{return juce::FontOptions("Segoe UI",b.getComponentID()=="pedal-midi"?juce::jlimit(9.f,11.f,b.getHeight()*.44f):12.f,juce::Font::plain);}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider)override
    {
        using namespace juce;auto r=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).withSizeKeepingCentre((float)jmin(w,h)-6,(float)jmin(w,h)-6);
        g.setColour(Colours::black.withAlpha(.7f));g.fillEllipse(r.translated(1,3));g.setGradientFill(ColourGradient(Colour(0xffb7c0c4),r.getX(),r.getY(),Colour(0xff131a1d),r.getRight(),r.getBottom(),false));g.fillEllipse(r);
        r=r.reduced(2);g.setGradientFill(ColourGradient(Colour(0xff343e43),r.getX(),r.getY(),Colour(0xff020609),r.getRight(),r.getBottom(),false));g.fillEllipse(r);
        Path needle;needle.addRoundedRectangle(-1,-r.getHeight()*.45f,2,r.getHeight()*.34f,1);g.setColour(slider.isEnabled()?Colours::white:Colours::grey);g.fillPath(needle,AffineTransform::rotation(start+pos*(end-start)).translated(r.getCentreX(),r.getCentreY()));
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float,float,const juce::Slider::SliderStyle,juce::Slider&)override
    {
        using namespace juce;const float cx=x+w*.5f;g.setColour(Colour(0xff061219));g.fillRoundedRectangle(cx-3,(float)y,6,(float)h,2);g.setColour(Colour(0xff4e879b));g.drawHorizontalLine(y+h/2,cx-6,cx+6);
        auto r=juce::Rectangle<float>(cx-7,pos-4,14,8);g.setGradientFill(ColourGradient(Colour(0xffbacbd1),r.getX(),r.getY(),Colour(0xff26343c),r.getX(),r.getBottom(),false));g.fillRoundedRectangle(r,2);g.setColour(Colour(0xff07171f));g.drawRoundedRectangle(r,2,1);g.setColour(Colours::white);g.drawHorizontalLine(roundToInt(pos),cx-4,cx+4);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& c,bool hover,bool down)override
    {
        using namespace juce;auto r=b.getLocalBounds().toFloat().reduced(1);
        if(b.getComponentID()=="gate")
        {const bool on=b.getToggleState();g.setColour(on?Colour(0xff00b8fa):Colour(0xff35444d));g.fillRoundedRectangle(r,r.getHeight()/2);g.setColour(Colours::white);auto ball=r.reduced(3).withWidth(r.getHeight()-6);if(on)ball.setX(r.getRight()-r.getHeight()+3);g.fillEllipse(ball);return;}
        if(b.getComponentID()=="power")
        {ui::paintFootswitch(g,r,down,hover||b.hasKeyboardFocus(true));return;}
        g.setGradientFill(ColourGradient(hover?c.brighter(.12f):c.brighter(.03f),0,0,c.darker(.12f),0,r.getBottom(),false));g.fillRoundedRectangle(r,4);g.setColour(b.isEnabled()?ui::outline():ui::outline().withAlpha(.4f));g.drawRoundedRectangle(r,4,1);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool over,bool down)override
    {
        if(b.getComponentID()=="power"||b.getComponentID()=="gate")return;
        if(b.getComponentID()=="save")
        {using namespace juce;g.setColour(b.isEnabled()?Colours::white:Colours::grey);auto y=(float)b.getHeight()/2-7;g.drawRoundedRectangle(9,y,13,14,1,1.2f);g.drawRect(12.f,y+1,6.f,4.f,1.f);g.drawRect(12.f,y+9,7.f,5.f,1.f);g.setFont(getTextButtonFont(b,b.getHeight()));g.drawText("Save",27,0,b.getWidth()-30,b.getHeight(),Justification::centred);return;}
        LookAndFeel_V4::drawButtonText(g,b,over,down);
    }
};
class Meter : public juce::Component
{
public:
    void update(float peak){level=juce::jmax(peak,level*.82f);if(peak>=1)clip=30;else if(clip>0)--clip;repaint();}
    void paint(juce::Graphics& g)override
    {using namespace juce;for(int i=0;i<8;++i){auto r=juce::Rectangle<float>(2.f,(float)(7-i)*getHeight()/8+1,(float)getWidth()-4,(float)getHeight()/8-3);const bool on=Decibels::gainToDecibels(level,-100.f)>=-42.f+i*6.f || (i==7&&clip>0);auto c=i<5?Colour(0xff30f17d):i<7?Colours::orange:Colours::red;g.setColour(on?c:c.withAlpha(.15f));g.fillRoundedRectangle(r,2);}}
private:float level=0;int clip=0;
};
class LearnButton : public juce::TextButton
{
public:
    using TextButton::TextButton;
    std::function<void()> onClear;
    void mouseDown(const juce::MouseEvent& e)override{context=e.mods.isPopupMenu();if(context){if(onClear)onClear();}else TextButton::mouseDown(e);}
    void mouseUp(const juce::MouseEvent& e)override{if(!context)TextButton::mouseUp(e);context=false;}
private:bool context=false;
};
}
class PilotReferenceCard final : public juce::Component
{
public:
    using CardLook=pilotReferenceUi::Look;
    static int engineFor(int visual){return board::order[(size_t)visual];}
    PilotReferenceCard(int visual,board::Processor& engine):id(visual),pedal(engineFor(visual)),processor(engine)
    {
        using namespace juce;setLookAndFeel(&look);
        int skinSize=0;
        const auto* skinData=BinaryData::getNamedResource(board::definition(pedal).skinResource,skinSize);
        skin=ImageCache::getFromMemory(skinData,skinSize);
        jackSkin=ImageCache::getFromMemory(BinaryData::compbody_png,BinaryData::compbody_pngSize);
        for(auto* c:std::initializer_list<Component*>{&title,&power,&preset,&learn})addAndMakeVisible(c);

        title.setComponentID("pedal-title");preset.setComponentID("pedal-preset");learn.setComponentID("pedal-midi");
        title.setText(String(board::definition(pedal).displayName),dontSendNotification);title.setJustificationType(Justification::centred);title.setColour(Label::textColourId,(id==1||id==5||id==9||id==11)?Colours::black:Colours::white);
        const StringArray labels=pedal==board::id(board::PedalId::AutoWah)?StringArray{"Sens","Range","Decay"}:pedal==board::id(board::PedalId::GraphicEq)?StringArray{"Low","Mid","High"}:pedal==board::id(board::PedalId::StudioComp)?StringArray{"Thr","Ratio","Level"}:pedal==board::id(board::PedalId::CleanBoost)?StringArray{"Boost","Mix","Level"}:StringArray{"Input trim","Tone","Level"};
        for(int k=0;k<3;++k)
        {
            auto& knob=knobs[(size_t)k];auto& label=knobLabels[(size_t)k];addAndMakeVisible(knob);addAndMakeVisible(label);label.setText(labels[k],dontSendNotification);label.setJustificationType(Justification::centred);
            knob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);knob.setTextBoxStyle(Slider::NoTextBox,false,0,0);knob.setPopupDisplayEnabled(true,true,this);knob.setTooltip(labels[k]);
            const bool namStyle=board::isNam(pedal);
            knob.setRange(k==0?(pedal==board::id(board::PedalId::StudioComp)?-48:namStyle?-12:-18):k==1?(pedal==board::id(board::PedalId::StudioComp)?1:namStyle?-6:0):-24,k==0?(pedal==board::id(board::PedalId::StudioComp)?0:namStyle?12:18):k==1?(pedal==board::id(board::PedalId::StudioComp)?20:namStyle?6:100):24,k==1&&!namStyle&&pedal!=board::id(board::PedalId::StudioComp)?1:.1);
            if(namStyle&&k==1){knob.setDoubleClickReturnValue(true,0);knob.setTooltip("Post-NAM Tone: tilt at 1 kHz, -6 to +6 dB. Double-click for neutral.");}
            if(pedal==board::id(board::PedalId::AutoWah)){knob.setRange(k==2?50:0,k==2?800:100,1);knob.setTooltip(k==0?"Envelope sensitivity":k==1?"Wah sweep range":"Envelope release (ms)");}
            knob.setTextValueSuffix(pedal==board::id(board::PedalId::AutoWah)?(k==2?" ms":"%"):k==1?(pedal==board::id(board::PedalId::StudioComp)?":1":namStyle?" dB":"%") : " dB");knob.setEnabled(pedal>=0);
            knob.onValueChange=[this]{if(pedal<0)return;auto s=processor.get(pedal);if(pedal==board::id(board::PedalId::AutoWah)){s.wahSensitivity=(float)knobs[0].getValue();s.wahRange=(float)knobs[1].getValue();s.wahDecay=(float)knobs[2].getValue();processor.set(pedal,s);return;}s.input=(float)knobs[0].getValue();if(pedal==board::id(board::PedalId::StudioComp))s.ratio=(float)knobs[1].getValue();else if(board::isNam(pedal))s.tone=(float)knobs[1].getValue();else s.mix=(float)knobs[1].getValue()/100;s.output=(float)knobs[2].getValue();processor.set(pedal,s);};
        }
        if(pedal==board::id(board::PedalId::GraphicEq))
        {
            for(auto& knob:knobs)knob.setVisible(false);for(auto& label:knobLabels)label.setVisible(false);
            const StringArray hz{"100","200","400","800","1.6k","3.2k","6.4k"};
            for(int b=0;b<7;++b){auto& f=eqFaders[(size_t)b];auto& l=eqLabels[(size_t)b];addAndMakeVisible(f);addAndMakeVisible(l);f.setSliderStyle(Slider::LinearVertical);f.setTextBoxStyle(Slider::NoTextBox,false,0,0);f.setRange(-12,12,.1);f.setDoubleClickReturnValue(true,0);f.setTextValueSuffix(" dB");f.setPopupDisplayEnabled(true,true,this);f.setTooltip(hz[b]+" Hz / -12 to +12 dB. Double-click for flat.");f.setTitle("EQ "+hz[b]);f.onValueChange=[this,b]{auto state=processor.get(pedal);state.eqBands[(size_t)b]=(float)eqFaders[(size_t)b].getValue();processor.set(pedal,state);};l.setText(hz[b],dontSendNotification);l.setJustificationType(Justification::centred);}
        }
        if(pedal>=0)for(int n=0;n<board::presetCount(pedal);++n)preset.addItem(processor.label(pedal,n),n+1);
        preset.setTextWhenNothingSelected("");preset.setEnabled(pedal>=0);
        preset.onChange=[this]{if(pedal<0)return;auto s=board::defaults(pedal,preset.getSelectedId()-1);s.enabled=processor.get(pedal).enabled;processor.set(pedal,s);sync();};
        power.setComponentID("power");power.setTitle(String(board::definition(pedal).displayName)+" bypass");power.setEnabled(pedal>=0);power.onClick=[this]{toggle();};
        learn.onClick=[this]{if(onLearn)onLearn();};learn.onClear=[this]{if(onClear)onClear();};learn.setTooltip("Learn a MIDI button. Right-click to clear the assignment.");
        sync();
    }
    ~PilotReferenceCard()override{setLookAndFeel(nullptr);}
    void toggle(){if(pedal<0)return;board::togglePedal(processor,pedal);sync();}
    void sync()
    {
        using namespace juce;if(pedal<0){learn.setEnabled(false);repaint();return;}
        const auto s=processor.get(pedal);
        preset.setSelectedId(preset.indexOfItemId(s.preset+1)>=0?s.preset+1:0,dontSendNotification);knobs[0].setValue(s.input,dontSendNotification);knobs[1].setValue(pedal==board::id(board::PedalId::StudioComp)?s.ratio:board::isNam(pedal)?s.tone:s.mix*100,dontSendNotification);knobs[2].setValue(s.output,dontSendNotification);
        if(pedal==board::id(board::PedalId::AutoWah)){knobs[0].setValue(s.wahSensitivity,dontSendNotification);knobs[1].setValue(s.wahRange,dontSendNotification);knobs[2].setValue(s.wahDecay,dontSendNotification);}
        if(pedal==board::id(board::PedalId::GraphicEq))for(int b=0;b<7;++b)eqFaders[(size_t)b].setValue(s.eqBands[(size_t)b],dontSendNotification);
        power.setToggleState(s.enabled,dontSendNotification);power.setTooltip(s.enabled?"ON - bypass":"OFF - enable");preset.setTooltip(processor.details(pedal,s.preset));
        String information=String(board::definition(pedal).displayName)+"\nType: "+(board::isNam(pedal)?"NAM Capture":"Algorithmic");
        if(board::isNam(pedal))information+="\n"+processor.modelFile(pedal,s.preset);
        information+="\nPreset: "+processor.label(pedal,s.preset);
        const auto details=processor.details(pedal,s.preset);if(details.isNotEmpty())information+="\n"+details;
        title.setTooltip(information);repaint();
    }
    void midi(const rig::Binding& b,bool learning,bool available)
    {
        if(pedal<0)return;learn.setEnabled(available);
        learn.setButtonText(learning?"Cancel Learn":b.valid()?b.text():"MIDI Learn");
        learn.setTooltip((learning?"Press a controller button. Click to cancel.":b.valid()?b.text()+". Click to learn a new assignment.":"Learn a MIDI button.")+juce::String(" Right-click to clear the assignment."));
    }
    void paint(juce::Graphics& g)override
    {
        using namespace juce;g.drawImage(skin,face,RectanglePlacement::stretchToFit);
        auto led=box(105,19,14,14).toFloat();const bool on=pedal>=0&&processor.get(pedal).enabled;
        g.setColour(Colours::black);g.fillEllipse(led.expanded(1));if(on){g.setColour(Colour(0xffff3b32).withAlpha(.25f));g.fillEllipse(led.expanded(4));}g.setColour(on?Colour(0xffff382e):Colour(0xff56272a));g.fillEllipse(led);if(on){g.setColour(Colours::white);g.fillEllipse(led.reduced(3));}
        // Reuse the photographed metallic sockets from the previously approved enclosure.
        const std::array<int,13> left{13,15,16,17,8,9,12,11,8,8,8,8,8},right{210,208,208,207,216,215,212,213,216,216,216,216,216};
        for(int side=0;side<2;++side)
        {
            const int x=side==0?jmax(0,left[(size_t)id]-11):right[(size_t)id]-1;
            const int width=side==0?left[(size_t)id]-x+1:jmin(11,224-x);
            auto jack=box((float)x,94,(float)width,32);Graphics::ScopedSaveState saved(g);Path outline;outline.addRoundedRectangle(jack.toFloat(),(float)width*.2f);g.reduceClipRegion(outline);
            g.drawImage(jackSkin,jack.getX(),jack.getY(),jack.getWidth(),jack.getHeight(),side==0?169:813,725,44,130);
        }
    }
    void resized()override
    {
        using namespace juce;face=RectanglePlacement(RectanglePlacement::centred).appliedTo(juce::Rectangle<float>(0,0,224,288),getLocalBounds().toFloat());
        const float scale=face.getWidth()/224;title.setFont(FontOptions("Segoe UI",17*scale,Font::bold));title.setBounds(box(24,34,176,22));
        for(int k=0;k<3;++k){knobs[(size_t)k].setBounds(box(26+k*57,61,52,49));knobLabels[(size_t)k].setFont(FontOptions("Segoe UI",11*scale,Font::plain));knobLabels[(size_t)k].setBounds(box(21+k*60,111,60,15));}
        for(int b=0;b<7;++b){eqFaders[(size_t)b].setBounds(box(25+b*25,62,24,45));eqLabels[(size_t)b].setBounds(box(24+b*25,109,26,13));eqLabels[(size_t)b].setFont(FontOptions("Segoe UI",8*scale,Font::plain));}
        // Compact controls sit above a real stomp switch, matching the approved face.
        preset.setBounds(box((224-tonegrid::ui::pedalControlWidth)/2,158,tonegrid::ui::pedalControlWidth,29));
        learn.setBounds(box((224-tonegrid::ui::pedalMidiWidth)/2,194,tonegrid::ui::pedalMidiWidth,25));
        power.setBounds(box(88,226,48,48));
    }

    std::function<void()> onLearn,onClear;
private:
    juce::Rectangle<int> box(float x,float y,float w,float h)const{return juce::Rectangle<float>(face.getX()+x*face.getWidth()/224,face.getY()+y*face.getHeight()/288,w*face.getWidth()/224,h*face.getHeight()/288).toNearestInt();}
    CardLook look;int id,pedal;board::Processor& processor;juce::Image skin,jackSkin;juce::Rectangle<float> face;

    std::array<juce::Slider,7> eqFaders;std::array<juce::Label,7> eqLabels;
    juce::Label title;std::array<juce::Label,3> knobLabels;std::array<juce::Slider,3> knobs;juce::ComboBox preset;juce::TextButton power;pilotReferenceUi::LearnButton learn{"MIDI Learn"};
};
