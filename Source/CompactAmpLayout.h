#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Shared/UI/UiStyle.h"

namespace tonegrid::ampui
{
// Keep the validated horizontal grid; remove space vertically instead of
// shrinking every control. The old face used 280 units at the same width.
inline constexpr float faceWidth=2016.f,faceHeight=188.f;
// Fidelity pass: 24% less than the validated compact rack. Minimum keeps controls usable.
inline int compactHeight(int width){return juce::jmax(138,juce::roundToInt(width*178.f/faceWidth));}
inline juce::Rectangle<int> panelBox(juce::Rectangle<int> panel,float x,float y,float w,float h)
{return juce::Rectangle<float>(panel.getX()+x*panel.getWidth()/faceWidth,panel.getY()+y*panel.getHeight()/faceHeight,w*panel.getWidth()/faceWidth,h*panel.getHeight()/faceHeight).toNearestInt();}
inline juce::Rectangle<int> thumbnailBounds(juce::Rectangle<int> p){return panelBox(p,302,42,170,95);}
inline juce::Rectangle<int> captureBounds(juce::Rectangle<int> p){return panelBox(p,490,35,274,77);}
inline juce::Rectangle<int> gainBounds(juce::Rectangle<int> p){return panelBox(p,1064,29,136,151);}
inline juce::Rectangle<int> eqBounds(juce::Rectangle<int> p){return panelBox(p,1200,18,132,150);}
inline juce::Rectangle<int> volumeBounds(juce::Rectangle<int> p){return panelBox(p,1336,29,134,151);}
inline constexpr float postFxStart=1514.f,postFxStep=120.f;
inline juce::Rectangle<int> outputMeterBounds(juce::Rectangle<int> panel)
{return panelBox(panel,1468,38,24,110).withWidth(juce::jmax(12,juce::roundToInt(panel.getWidth()*24.f/faceWidth)));}
inline juce::Rectangle<int> settingsButtonBounds(juce::Rectangle<int> panel,int effect)
{auto r=panelBox(panel,postFxStart+70+effect*postFxStep,87,38,38);const auto side=juce::jmax(24,juce::jmin(r.getWidth(),r.getHeight()));return r.withSizeKeepingCentre(side,side);}
inline juce::Rectangle<int> effectToggleBounds(juce::Rectangle<int> panel,int effect)
{auto r=panelBox(panel,postFxStart+6+effect*postFxStep,89,58,38);return r.withHeight(juce::jmax(24,r.getHeight()));}
inline juce::Rectangle<int> normalizationBounds(juce::Rectangle<int> panel)
{
    const auto folder=panelBox(panel,974,66,60,60);
    auto button=panelBox(panel,774,81,186,30);
    return button.withHeight(ui::buttonHeight).withCentre({button.getCentreX(),folder.getCentreY()});
}
inline juce::Rectangle<int> folderButtonBounds(juce::Rectangle<int> panel)
{const auto r=panelBox(panel,974,66,60,60);return r.withSizeKeepingCentre(r.getWidth(),ui::buttonHeight);}
inline juce::Rectangle<float> folderGlyphBounds(juce::Rectangle<int> button)
{
    const float width=juce::jmin(button.getWidth(),button.getHeight())*.62f;
    return button.toFloat().withSizeKeepingCentre(width,width*20.f/24.f);
}
inline juce::Rectangle<int> vstActionsBounds(juce::Rectangle<int> panel)
{return panelBox(panel,320,96,348,36);}
inline juce::Rectangle<int> vstSelectorBounds(juce::Rectangle<int> panel)
{const auto actions=vstActionsBounds(panel);return panelBox(panel,320,46,348,36).withRight(actions.getRight());}
}
