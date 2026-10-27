#pragma once
#include "Shared/UI/UiStyle.h"

namespace tonegrid::captureui
{
inline juce::Font font(){return ui::font(ui::captureSize);}
// Keep the underlying label/ComboBox value intact for menus and accessibility.
// Only painting is curtailed; no fitted text, wrapping, or horizontal font scaling.
inline juce::String curtailed(const juce::String& text,const juce::Font& f,float width)
{
    if(juce::GlyphArrangement::getStringWidth(f,text)<=width)return text;
    const auto dots=juce::String::charToString(0x2026);
    if(juce::GlyphArrangement::getStringWidth(f,dots)>width)return {};
    int low=0,high=text.length();
    while(low<high){const int mid=(low+high+1)/2;if(juce::GlyphArrangement::getStringWidth(f,text.substring(0,mid)+dots)<=width)low=mid;else high=mid-1;}
    return text.substring(0,low).trimEnd()+dots;
}
inline void position(juce::ComboBox& box,juce::Label& label)
{
    label.setComponentID("pedal-preset-text");label.setFont(font());label.setMinimumHorizontalScale(1.f);
    label.setBorderSize({0,0,0,0});label.setJustificationType(juce::Justification::centredLeft);
    label.setBounds(5,0,juce::jmax(0,box.getWidth()-25),box.getHeight());
}
inline void paint(juce::Graphics& g,juce::Label& label)
{
    g.setFont(label.getFont());g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(label.isEnabled()?1.f:ui::disabledOpacity));
    g.drawText(curtailed(label.getText(),label.getFont(),(float)label.getWidth()),label.getLocalBounds(),juce::Justification::centredLeft,false);
}
inline juce::PopupMenu::Options options(juce::ComboBox& box,juce::Label& label,juce::Font popupFont)
{
    int width=box.getWidth();for(int i=0;i<box.getNumItems();++i)width=juce::jmax(width,juce::GlyphArrangement::getStringWidthInt(popupFont,box.getItemText(i))+60);
    if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(box.getScreenBounds()))width=juce::jmin(width,display->userArea.getWidth()-24);
    return juce::PopupMenu::Options().withTargetComponent(&box).withItemThatMustBeVisible(box.getSelectedId()).withInitiallySelectedItem(box.getSelectedId())
        .withMinimumWidth(width).withMaximumNumColumns(1).withStandardItemHeight(26).withTargetScreenArea(label.getScreenBounds());
}
}
