#pragma once
#include "Shared/UI/PopupStyle.h"
#include "Effects/AmpEq/AmpEqResponse.h"
#include <array>
namespace tonegrid::ampui
{
inline juce::Colour eqBlue(){return juce::Colour(0xff149dff);}
class EqKnobLook final:public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& slider)override
    {
        metal.drawRotarySlider(g,x,y,w,h,position,start,end,slider);using namespace juce;const float side=float(jmin(w,h))-jmin(10.f,jmin(w,h)*.07f);const auto r=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).withSizeKeepingCentre(side,side);Path arc;arc.addCentredArc(r.getCentreX(),r.getCentreY(),side*.52f,side*.52f,0,start,start+position*(end-start),true);g.setColour(eqBlue().withAlpha(slider.isEnabled()?1.f:.3f));g.strokePath(arc,PathStrokeType(2.f));
    }
private:ampui::Look metal;
};
class EqBandTab final:public juce::TextButton
{
public:
    void paintButton(juce::Graphics& g,bool hover,bool)override
    {
        g.setFont(ui::font(12));g.setColour(getToggleState()||hover?ui::ink():ui::metadataInk());g.drawFittedText(getButtonText(),getLocalBounds(),juce::Justification::centred,1,.75f);if(getToggleState()){g.setColour(eqBlue());g.fillRect(8,getHeight()-2,getWidth()-16,2);}if(hasKeyboardFocus(true)){g.setColour(eqBlue());g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),3,1);}
    }
};
class EqButton final:public juce::Button
{
public:EqButton():Button("AMP EQ"){}
private:
    void paintButton(juce::Graphics& g,bool hover,bool down)override
    {
        using namespace juce;const auto r=getLocalBounds().toFloat().reduced(1);g.setColour(ui::panel());g.fillRoundedRectangle(r,5);const float alpha=isEnabled()?1.f:.3f;g.setColour((hover?eqBlue():Colour(0xff49758d)).withAlpha(alpha));g.drawRoundedRectangle(r,5,1);
        Path p;const float y=r.getY()+r.getHeight()*.31f;p.startNewSubPath(r.getX()+r.getWidth()*.18f,y);p.lineTo(r.getX()+r.getWidth()*.31f,y);p.cubicTo(r.getX()+r.getWidth()*.4f,y,r.getX()+r.getWidth()*.4f,y-10,r.getX()+r.getWidth()*.46f,y-7);p.cubicTo(r.getX()+r.getWidth()*.54f,y-4,r.getX()+r.getWidth()*.52f,y+9,r.getX()+r.getWidth()*.6f,y+6);p.lineTo(r.getX()+r.getWidth()*.7f,y);p.lineTo(r.getX()+r.getWidth()*.82f,y);
        g.setColour(eqBlue().withAlpha(alpha*(down?.6f:1.f)));g.strokePath(p,PathStrokeType(1.5f,PathStrokeType::curved,PathStrokeType::rounded));g.setColour(ui::ink().withAlpha(alpha));g.setFont(ui::font(9));g.drawText("AMP EQ",getLocalBounds().withTrimmedTop(getHeight()/2),Justification::centred);
        if(hasKeyboardFocus(true)){g.setColour(eqBlue());g.drawRoundedRectangle(r.reduced(2),3,1);}
    }
};
class AdvancedEqControls final:public popup::View
{
    class Curve final:public juce::Component,public juce::SettableTooltipClient
    {
    public:
        explicit Curve(AdvancedEqControls& value):owner(value){setComponentID("eq-response-curve");setMouseCursor(juce::MouseCursor::CrosshairCursor);setWantsKeyboardFocus(true);}
        juce::Rectangle<float> plot()const{return getLocalBounds().toFloat().reduced(42,14).withTrimmedBottom(14);}
        float xFor(double hz)const{const auto r=plot();return r.getX()+float(std::log(hz/20.)/std::log(1000.))*r.getWidth();}
        float yFor(double db)const{auto r=plot();return r.getCentreY()-float(juce::jlimit(-12.,12.,db))*r.getHeight()/24;}
        double hzFor(float x)const{auto r=plot();return 20*std::pow(1000.,juce::jlimit(0.f,1.f,(x-r.getX())/r.getWidth()));}
        juce::Point<float> point(int band)const{const double f=owner.frequency(band);return {xFor(f),yFor(ampEq::responseDb(owner.state(),f))};}
        void paint(juce::Graphics& g)override
        {
            using namespace juce;auto r=plot();g.setColour(Colour(0xff0a141b));g.fillRoundedRectangle(getLocalBounds().toFloat(),6);g.setColour(ui::outline());g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),6,1);
            for(double f:{20.,30.,50.,100.,200.,300.,500.,1000.,2000.,3000.,5000.,10000.,20000.}){g.setColour(ui::outline().withAlpha(.5f));g.drawVerticalLine(roundToInt(xFor(f)),r.getY(),r.getBottom());}
            for(int db:{-12,-6,0,6,12}){g.setColour(ui::outline().withAlpha(db==0?.9f:.4f));g.drawHorizontalLine(roundToInt(yFor(db)),r.getX(),r.getRight());}
            g.setFont(ui::font(11));g.setColour(ui::metadataInk());for(int db:{-12,0,12})g.drawText(String(db>0?"+":"")+String(db),0,roundToInt(yFor(db))-8,35,16,Justification::centredRight);
            for(double f:{20.,100.,500.,1000.,5000.,20000.})g.drawText(f>=1000?String(f/1000,0)+"k":String(f,0),roundToInt(xFor(f))-18,getHeight()-22,36,18,Justification::centred);
            const auto p=owner.state();Path curve;for(int i=0;i<=480;++i){const double f=20*std::pow(1000.,i/480.);auto point=Point<float>{xFor(f),yFor(ampEq::responseDb(p,f))};if(i==0)curve.startNewSubPath(point);else curve.lineTo(point);}
            Path fill=curve;fill.lineTo(r.getRight(),r.getBottom());fill.lineTo(r.getX(),r.getBottom());fill.closeSubPath();g.setColour(eqBlue().withAlpha(p.enabled?.08f:.025f));g.fillPath(fill);g.setColour(p.enabled?eqBlue():ui::metadataInk());g.strokePath(curve,PathStrokeType(2));
            for(int band=0;band<8;++band){auto centre=point(band);const bool active=band==owner.selectedBand;const float size=active?12.f:8.f;g.setColour(active?Colours::white:eqBlue().withAlpha(.8f));g.fillEllipse(centre.x-size/2,centre.y-size/2,size,size);if(active){g.setColour(eqBlue());g.drawEllipse(centre.x-9,centre.y-9,18,18,1.2f);}}
        }
        void mouseDown(const juce::MouseEvent& e)override
        {float closest=100000;int target=owner.selectedBand;for(int band=0;band<8;++band){const auto distance=point(band).getDistanceFrom(e.position);if(distance<closest){closest=distance;target=band;}}dragging=closest<24;if(dragging)owner.selectBand(target);start=e.position;initialGain=owner.gain(target);grabKeyboardFocus();}
        void mouseDrag(const juce::MouseEvent& e)override
        {
            if(!dragging)return;const int band=owner.selectedBand,gi=owner.gainIndex(band),fi=owner.frequencyIndex(band);
            if(gi>=0){auto& k=owner.knobs[size_t(gi)];k.setValue(juce::jlimit(k.getMinimum(),k.getMaximum(),initialGain+(start.y-e.position.y)*24./plot().getHeight()),juce::sendNotificationSync);}
            if(fi>=0){auto& k=owner.knobs[size_t(fi)];k.setValue(juce::jlimit(k.getMinimum(),k.getMaximum(),hzFor(e.position.x)),juce::sendNotificationSync);}
        }
        void mouseUp(const juce::MouseEvent&)override{dragging=false;}
        bool keyPressed(const juce::KeyPress& key)override
        {
            const int gi=owner.gainIndex(owner.selectedBand),fi=owner.frequencyIndex(owner.selectedBand);
            if((key==juce::KeyPress::upKey||key==juce::KeyPress::downKey)&&gi>=0){owner.knobs[size_t(gi)].setValue(owner.knobs[size_t(gi)].getValue()+(key==juce::KeyPress::upKey?.5:-.5),juce::sendNotificationSync);return true;}
            if((key==juce::KeyPress::leftKey||key==juce::KeyPress::rightKey)&&fi>=0){owner.knobs[size_t(fi)].setValue(owner.knobs[size_t(fi)].getValue()*(key==juce::KeyPress::rightKey?1.02:.98),juce::sendNotificationSync);return true;}return false;
        }
    private:AdvancedEqControls& owner;bool dragging=false;juce::Point<float> start;double initialGain=0;
    };
public:
    inline static constexpr int bodyWidth=940,collapsedHeight=532,expandedHeight=532;
    AdvancedEqControls():curve(*this)
    {
        const std::array<double,13> minimum{-12,-12,-12,20,2000,80,-12,.3,400,-12,.3,-6,40},maximum{12,12,12,500,20000,2000,12,4,12000,12,4,6,250},neutral{0,0,0,80,8000,250,0,1,3000,0,1,0,100};
        // Legacy slider identities and saved parameter meanings remain unchanged.
        for(size_t i=0;i<knobs.size();++i)
        {
            auto& k=knobs[i];addAndMakeVisible(k);k.setTitle(names[int(i)]);k.setComponentID("amp-eq-"+juce::String(int(i)));k.setSliderStyle(isMain(i)?juce::Slider::RotaryHorizontalVerticalDrag:juce::Slider::LinearHorizontal);k.setTextBoxStyle(isMain(i)?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,isMain(i)?94:88,24);k.setRange(minimum[i],maximum[i],isQ(i)?.01:isFrequency(i)?1:.1);k.setDoubleClickReturnValue(true,neutral[i]);k.setTextValueSuffix(isQ(i)?"":isFrequency(i)?" Hz":" dB");k.setNumDecimalPlacesToDisplay(isQ(i)?2:isFrequency(i)?0:1);if(isFrequency(i))k.setSkewFactorFromMidPoint(std::sqrt(minimum[i]*maximum[i]));k.setTooltip(names[int(i)]+". Double-click to reset.");k.onValueChange=[this]{publish();};
        }
        for(size_t i:mainIndices)knobs[i].setLookAndFeel(&knobLook);
        addAndMakeVisible(curve);curve.setTooltip("Select a band, then drag its point. Arrow keys adjust gain and frequency. The curve shows the actual combined EQ response.");
        for(auto* b:{&enabled,&lowCut,&highCut}){addAndMakeVisible(b);b->setLookAndFeel(&switchLook);b->setClickingTogglesState(true);b->onClick=[this]{publish();};b->setColour(juce::TextButton::buttonOnColourId,eqBlue());}
        enabled.setComponentID("eq-enabled-toggle");lowCut.setComponentID("eq-low-cut-toggle");highCut.setComponentID("eq-high-cut-toggle");enabled.setTitle("EQ on/off");lowCut.setTitle("Low Cut on/off");highCut.setTitle("High Cut on/off");lowCut.setTooltip("Reduce deep rumble. 12 dB/octave.");highCut.setTooltip("Soften harsh highs. 12 dB/octave.");
        addAndMakeVisible(reset);reset.setComponentID("eq-reset");reset.onClick=[this]{sync({});publish();};
        for(size_t i=0;i<bands.size();++i){auto& b=bands[i];addAndMakeVisible(b);l10n::button(b,bandNames[int(i)]);b.setComponentID("eq-band-"+juce::String(int(i)));b.setClickingTogglesState(true);b.onClick=[this,i]{selectBand(int(i));};}
        sync({});selectBand(3);updateTooltips();setSize(bodyWidth,popupContentHeight(bodyWidth));
    }
    ~AdvancedEqControls()override{for(auto* b:{&enabled,&lowCut,&highCut})b->setLookAndFeel(nullptr);for(auto& k:knobs)k.setLookAndFeel(nullptr);}
    void languageChanged()override{updateTooltips();}
    int popupMinimumWidth()const override{return 560;}
    int popupContentHeight(int width)const override{return width<760?680:collapsedHeight;}
    float popupCornerRadius()const override{return 18.f;}
    float popupBorderWidth()const override{return 1.f;}
    ampEq::Parameters state()const noexcept
    {
        ampEq::Parameters p{float(knobs[0].getValue()),float(knobs[1].getValue()),float(knobs[2].getValue())};p.enabled=enabled.getToggleState();p.lowCut=lowCut.getToggleState();p.highCut=highCut.getToggleState();p.lowCutHz=float(knobs[3].getValue());p.highCutHz=float(knobs[4].getValue());p.lowMidHz=float(knobs[5].getValue());p.lowMidDb=float(knobs[6].getValue());p.lowMidQ=float(knobs[7].getValue());p.presenceHz=float(knobs[8].getValue());p.presenceDb=float(knobs[9].getValue());p.presenceQ=float(knobs[10].getValue());p.depthDb=float(knobs[11].getValue());p.depthHz=float(knobs[12].getValue());return p;
    }
    void sync(const ampEq::Parameters& p)
    {
        const std::array<float,13> v{p.bassDb,p.midDb,p.trebleDb,p.lowCutHz,p.highCutHz,p.lowMidHz,p.lowMidDb,p.lowMidQ,p.presenceHz,p.presenceDb,p.presenceQ,p.depthDb,p.depthHz};for(size_t i=0;i<v.size();++i)knobs[i].setValue(v[i],juce::dontSendNotification);enabled.setToggleState(p.enabled,juce::dontSendNotification);lowCut.setToggleState(p.lowCut,juce::dontSendNotification);highCut.setToggleState(p.highCut,juce::dontSendNotification);curve.repaint();repaint();
    }
    // Compatibility for existing callers; this no longer opens an expanding area.
    bool advancedOpen()const noexcept{return false;}
    void setAdvancedOpen(bool){selectBand(3);}
    void selectBand(int band)
    {
        selectedBand=juce::jlimit(0,7,band);for(size_t i=0;i<bands.size();++i)bands[i].setToggleState(int(i)==selectedBand,juce::dontSendNotification);
        for(size_t i:{size_t(5),size_t(6),size_t(7),size_t(8),size_t(10),size_t(12)})knobs[i].setVisible((selectedBand==3&&(i==5||i==6||i==7))||(selectedBand==6&&(i==8||i==10))||(selectedBand==2&&i==12));resized();curve.repaint();repaint();
    }
    int selected()const{return selectedBand;}
    void resized()override
    {
        const int w=getWidth()-24;enabled.setBounds(12,4,46,26);reset.setBounds(getWidth()-98,2,86,30);curve.setBounds(12,34,w,140);const int bw=w/8;for(int i=0;i<8;++i)bands[size_t(i)].setBounds(12+i*bw,180,bw-3,24);
        detail={12,210,w,52};const int column=(w-102)/3;for(int i=0;i<3;++i)detailSlots[size_t(i)]={110+i*column,234,column-12,24};
        knobs[5].setBounds(detailSlots[0]);knobs[6].setBounds(detailSlots[1]);knobs[7].setBounds(detailSlots[2]);knobs[8].setBounds(detailSlots[0]);knobs[10].setBounds(detailSlots[1]);knobs[12].setBounds(detailSlots[0]);
        const int columns=getWidth()<760?3:5,cell=w/columns;for(size_t n=0;n<mainIndices.size();++n)knobs[mainIndices[n]].setBounds(12+int(n)%columns*cell+(cell-100)/2,290+int(n)/columns*148,100,110);
        const int y=columns==3?562:414,fw=(w-14)/2;filterBounds={juce::Rectangle<int>(12,y,fw,70),{26+fw,y,fw,70}};
        for(int n=0;n<2;++n){auto box=filterBounds[size_t(n)];auto& toggle=n==0?lowCut:highCut;toggle.setBounds(box.getRight()-53,box.getY()+7,42,22);knobs[size_t(n+3)].setBounds(box.getX()+10,box.getY()+36,box.getWidth()-20,28);}
    }
    void paint(juce::Graphics& g)override
    {
        using namespace juce;g.setColour(ui::ink());g.setFont(ui::font(15));g.drawText(enabled.getToggleState()?"EQ ON":"EQ OFF",70,2,110,28,Justification::centredLeft);g.setFont(ui::font(12));g.setColour(ui::metadataInk());g.drawText(l10n::translate("Select a band to fine-tune"),200,4,240,26,Justification::centredLeft);
        g.setColour(ui::outline());g.drawRoundedRectangle(detail.toFloat(),5,1);g.setColour(ui::secondaryInk());g.setFont(ui::font(12));g.drawText(l10n::translate(bandNames[selectedBand]),detail.withWidth(94),Justification::centred);
        if(selectedBand==3){label(g,"Frequency",detailSlots[0].withY(212).withHeight(20));label(g,"Gain",detailSlots[1].withY(212).withHeight(20));label(g,"Width (Q)",detailSlots[2].withY(212).withHeight(20));}
        else if(selectedBand==6){label(g,"Frequency",detailSlots[0].withY(212).withHeight(20));label(g,"Width (Q)",detailSlots[1].withY(212).withHeight(20));}
        else if(selectedBand==2)label(g,"Frequency",detailSlots[0].withY(212).withHeight(20));
        else {const String text=selectedBand==0||selectedBand==7?"Frequency and On/Off are always available below.":l10n::format("eq.fixed_frequency",{{"frequency",String(frequency(selectedBand),0)}});g.drawText(l10n::translate(text),detail.withTrimmedLeft(110).reduced(6),Justification::centredLeft,true);}
        for(size_t i:mainIndices){auto b=knobs[i].getBounds();g.setColour(ui::ink());g.setFont(ui::font(17));g.drawText(l10n::translate(i==9?"Presence":i==11?"Depth":names[int(i)]),b.withY(b.getY()-26).withHeight(24),Justification::centred);}
        for(int n=0;n<2;++n){const auto box=filterBounds[size_t(n)];g.setColour(ui::outline());g.drawRoundedRectangle(box.toFloat(),6,1);g.setColour(ui::ink());g.setFont(ui::font(14));g.drawText(l10n::translate(n==0?"Low Cut":"High Cut").toUpperCase(),box.reduced(10,5).removeFromTop(23).withTrimmedRight(55),Justification::centredLeft);}
        g.setColour(ui::metadataInk());g.setFont(ui::font(11));g.drawFittedText(l10n::translate("Low Cut: reduce rumble. High Cut: soften harsh highs. Double-click any control to reset."),12,getHeight()-36,getWidth()-24,32,Justification::centredLeft,2,1.f);
    }
    std::function<void(ampEq::Parameters)> onChange;std::function<void(int)> onBodyHeight;
private:
    void updateTooltips()
    {
        l10n::tooltip(curve,"Select a band, then drag its point. Arrow keys adjust gain and frequency. The curve shows the actual combined EQ response.");
        l10n::tooltip(lowCut,"Reduce deep rumble. 12 dB/octave.");l10n::tooltip(highCut,"Soften harsh highs. 12 dB/octave.");
        for(size_t i=0;i<knobs.size();++i){auto title=names[int(i)];for(const auto& suffix:juce::StringArray{" frequency"," gain"," width"})if(title.endsWith(suffix)){title=l10n::translate(title.dropLastCharacters(suffix.length()))+" — "+l10n::translate(suffix.trim().substring(0,1).toUpperCase()+suffix.trim().substring(1));break;}if(title==names[int(i)])title=l10n::translate(title);knobs[i].setTooltip(l10n::format("eq.control_tip",{{"name",title}}));}
    }
    static bool isMain(size_t i){return i<3||i==9||i==11;}static bool isQ(size_t i){return i==7||i==10;}static bool isFrequency(size_t i){return i==3||i==4||i==5||i==8||i==12;}
    static int gainIndex(int band){static constexpr int ids[]{-1,0,11,6,1,2,9,-1};return ids[band];}static int frequencyIndex(int band){static constexpr int ids[]{3,-1,12,5,-1,-1,8,4};return ids[band];}
    double gain(int band)const{const int i=gainIndex(band);return i>=0?knobs[size_t(i)].getValue():0;}double frequency(int band)const{const int i=frequencyIndex(band);return i>=0?knobs[size_t(i)].getValue():band==1?150:band==4?425:1800;}
    void publish(){curve.repaint();repaint();if(onChange)onChange(state());}
    static void label(juce::Graphics& g,const juce::String& text,juce::Rectangle<int> r){g.setColour(ui::metadataInk());g.setFont(ui::font(11));g.drawText(l10n::translate(text),r,juce::Justification::centredLeft);}
    inline static constexpr std::array<size_t,5> mainIndices{0,1,2,9,11};
    const juce::StringArray names{"Bass","Mid","Treble","Low Cut","High Cut","Low Mid frequency","Low Mid gain","Low Mid width","Presence frequency","Presence gain","Presence width","Depth","Depth frequency"},bandNames{"Low Cut","Bass","Depth","Low Mid","Mid","Treble","Presence","High Cut"};
    ampui::Look switchLook;EqKnobLook knobLook;std::array<juce::Slider,13> knobs;Curve curve;juce::TextButton enabled,lowCut,highCut;l10n::TextButton reset{"Reset"};std::array<EqBandTab,8> bands;int selectedBand=3;juce::Rectangle<int> detail;std::array<juce::Rectangle<int>,3> detailSlots;std::array<juce::Rectangle<int>,2> filterBounds;
};
}
