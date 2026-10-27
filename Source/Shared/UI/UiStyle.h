#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "Shared/Localization/LanguageUI.h"
namespace tonegrid::ui
{
// Shared UI-only dimensions. Extra window width is space, not widget growth.
inline constexpr int buttonHeight=32,buttonPadding=10,gap=8,sectionGap=20,sectionPadding=12;
inline constexpr int presetComboMax=260,midiComboMax=200,saveWidth=68,learnWidth=96,refreshWidth=112,audioWidth=116;
inline constexpr int normalizationMax=210;
inline constexpr float pedalControlWidth=112.f,pedalMidiWidth=104.f;
// Application chrome tokens. The photographed pedal art has its own immutable palette.
inline constexpr float stroke=1.f,controlRadius=5.f,panelRadius=12.f;
inline constexpr float disabledOpacity=.38f,separatorOpacity=.45f;
inline constexpr float primarySize=16.f,sectionSize=12.f,controlSize=14.f,microSize=12.f,statusSize=13.f,captureSize=12.f;
inline constexpr int regularWeight=juce::Font::plain,emphasisWeight=juce::Font::bold;
inline juce::Colour ink(){return juce::Colour(0xffe7e9e6);}
inline juce::Colour secondaryInk(){return juce::Colour(0xffb7c0c3);}
inline juce::Colour metadataInk(){return juce::Colour(0xff8e9a9f);}
inline juce::Colour panel(){return juce::Colour(0xff10181d);}
inline juce::Colour field(){return juce::Colour(0xff182228);}
inline juce::Colour selected(){return juce::Colour(0xff293941);}
inline juce::Colour accent(){return juce::Colour(0xff627f91);}
inline juce::Colour tabBlue(){return juce::Colour(0xff149dff);}
inline juce::Font font(float height=controlSize,int weight=regularWeight)
{
    // No bundled font dependency. JUCE resolves the native sans-serif on macOS/Linux.
#if JUCE_WINDOWS
    return juce::FontOptions("Segoe UI",height,weight);
#else
    return juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),height,weight);
#endif
}
inline juce::Font sectionFont(){auto f=font(sectionSize);f.setExtraKerningFactor(.035f);return f;}
inline void paintControl(juce::Graphics& g,juce::Rectangle<float> r,bool active,bool hover,bool down,bool enabled,bool focus=false)
{
    r=r.reduced(.5f);const float alpha=enabled?1.f:disabledOpacity;
    auto fill=active?selected():field();if(hover&&enabled)fill=fill.brighter(.06f);if(down)fill=fill.darker(.08f);
    g.setGradientFill(juce::ColourGradient(fill.brighter(.055f).withMultipliedAlpha(alpha),r.getX(),r.getY(),fill.darker(.14f).withMultipliedAlpha(alpha),r.getX(),r.getBottom(),false));g.fillRoundedRectangle(r,controlRadius);
    g.setColour((active||focus?accent():juce::Colour(0xff465158)).withAlpha(alpha*(hover||focus?1.f:.9f)));
    g.drawRoundedRectangle(r,controlRadius,stroke);
}
inline void paintChevron(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour colour)
{
    const auto x=r.getRight()-15.f,y=r.getCentreY();juce::Path p;
    p.startNewSubPath(x-4,y-2);p.lineTo(x,y+2);p.lineTo(x+4,y-2);
    g.setColour(colour);g.strokePath(p,juce::PathStrokeType(1.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
}
inline juce::Colour separator(){return juce::Colour(0xff454c51);}
inline juce::Colour outline(){return juce::Colour(0xff3d454a);}
inline juce::Colour background(){return juce::Colour(0xff151e24);}
inline juce::Colour switchOn(){return juce::Colour(0xffb0b5b8);}
inline juce::Colour switchOff(){return juce::Colour(0xff343d42);}
// One shared header band; narrower windows use compact widths, never stretched controls.
inline constexpr int windowInset=8,footerInset=6,boardTopGap=4;
inline constexpr int headerHeight=80,logoWidth=180,logoGap=14;
struct ToolbarMetrics
{
    int preset,midi,save,learn,refresh,audio,padding,controlGap,sectionSpacing,guitar,gate,calibration,midiGap;
};
inline ToolbarMetrics toolbarMetrics(int width)
{
    if(width>=1370)return {205,140,saveWidth,78,72,60,sectionPadding,gap,12,140,164,136,gap};
    return {170,80,58,72,32,52,6,6,12,94,106,128,0};
}
inline std::array<juce::Rectangle<int>,6> toolbarSections(juce::Rectangle<int> row)
{
    const auto m=toolbarMetrics(row.getWidth());
    const std::array<int,6> widths{m.preset+m.save+m.learn+2*m.controlGap+2*m.padding,m.guitar,m.gate,112,m.calibration+m.audio+buttonHeight+2*m.controlGap+2*m.padding,m.midi+m.refresh+m.midiGap+2*m.padding};
    int used=0;for(int w:widths)used+=w;
    const int spacing=juce::jlimit(m.sectionSpacing,48,(row.getWidth()-used)/5);
    std::array<juce::Rectangle<int>,6> sections{};
    for(size_t i=0;i<sections.size();++i){sections[i]=row.removeFromLeft(widths[i]);row.removeFromLeft(spacing);}
    return sections;
}
inline juce::Rectangle<int> compactWidth(juce::Rectangle<int> r,int maximum)
{return r.withWidth(juce::jmin(r.getWidth(),maximum));}
inline void paintFootswitch(juce::Graphics& g,juce::Rectangle<float> r,bool down,bool hover)
{
    using namespace juce;r=r.withSizeKeepingCentre(jmin(r.getWidth(),r.getHeight()),jmin(r.getWidth(),r.getHeight())).reduced(3);
    g.setColour(Colours::black.withAlpha(.65f));g.fillEllipse(r.translated(1,3));
    g.setGradientFill(ColourGradient(Colour(0xffe1e4e3),r.getX(),r.getY(),Colour(0xff343a3c),r.getRight(),r.getBottom(),false));g.fillEllipse(r);
    g.setColour(Colour(0xff161b1d));g.drawEllipse(r.reduced(1),1.2f);
    auto washer=r.reduced(4);g.setGradientFill(ColourGradient(Colour(0xff5b6468),washer.getX(),washer.getY(),Colour(0xffced1cf),washer.getRight(),washer.getBottom(),false));g.fillEllipse(washer);
    auto cap=washer.reduced(3).translated(0,down?1.f:-1.f);g.setColour(Colours::black.withAlpha(.7f));g.fillEllipse(cap.translated(0,2));
    g.setGradientFill(ColourGradient(Colour(0xfff5f5ef),cap.getX(),cap.getY(),Colour(0xff656f73),cap.getRight(),cap.getBottom(),false));g.fillEllipse(cap);
    for(int i=0;i<20;++i){float x=cap.getX()+cap.getWidth()*i/20.f;g.setColour(Colours::white.withAlpha(.07f));g.drawLine(x,cap.getY()+cap.getHeight()*.2f,x,cap.getBottom()-cap.getHeight()*.2f,.5f);}
    g.setColour(Colour(0xffe6e9e7).withAlpha(.65f));g.drawEllipse(cap.reduced(1),.8f);
    if(hover){g.setColour(Colours::white.withAlpha(.4f));g.drawEllipse(r,1);}
}
}
