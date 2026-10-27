#pragma once
#include "Shared/UI/PopupWindow.h"
#include "BetaWelcomeState.h"
namespace tonegrid::welcome
{
class View final:public popup::View
{
public:
    View()
    {
        setComponentID("beta-welcome");
        close.setComponentID("welcome-close");never.setComponentID("welcome-dismiss");
        settings.setComponentID("welcome-settings");calibrate.setComponentID("welcome-calibrate");
        for(auto* button:{&close,&never,&settings,&calibrate})addAndMakeVisible(button);
        addAndMakeVisible(language);language.setComponentID("welcome-language");
        close.onClick=[this]{finish(false);};never.onClick=[this]{finish(true);};
        settings.onClick=[this]{const auto action=onSettings;if(action)action();};
        calibrate.onClick=[this]{const auto action=onCalibrate;if(action)action();};
        close.addShortcut(juce::KeyPress(juce::KeyPress::escapeKey));
        setSize(640,560);setSize(getWidth(),requiredHeight());
    }
    std::function<void(bool)> onDismiss;
    std::function<void()> onSettings,onCalibrate;
    int popupMinimumWidth() const override{return 380;}
    int popupContentHeight(int width) const override
    {
        int y=78;for(int i=0;i<4;++i)
        {juce::TextLayout measure;measure.createLayout(copy(i),float(width));y+=juce::roundToInt(measure.getHeight())+16;if(i==1||i==2)y+=48;}
        return y+50;
    }
    void resized() override
    {
        language.setBounds(0,0,getWidth(),68);int y=78;
        for(int i=0;i<4;++i)
        {
            layouts[size_t(i)].createLayout(copy(i),float(getWidth()));
            const int height=juce::roundToInt(layouts[size_t(i)].getHeight());
            areas[size_t(i)]={0,y,getWidth(),height};y+=height+16;
            if(i==1||i==2){(i==1?settings:calibrate).setBounds(0,y-4,juce::jmin(240,getWidth()),36);y+=48;}
        }
        y=juce::jmax(y+8,getHeight()-42);
        const int closeWidth=juce::jmin(150,getWidth()/3);never.setBounds(0,y,getWidth()-closeWidth-14,38);close.setBounds(getWidth()-closeWidth,y,closeWidth,38);
    }
    void paint(juce::Graphics& g) override
    {for(size_t i=0;i<layouts.size();++i)layouts[i].draw(g,areas[i].toFloat());}
    int requiredHeight()const{return popupContentHeight(getWidth());}
    l10n::TextButton close{"Close"},never{"Don't show this message again"},settings{"Settings"},calibrate{"Calibrate"};
private:
    static juce::AttributedString copy(int section)
    {
        juce::AttributedString text;text.setLineSpacing(3.f);
        const auto body=[&](const juce::String& s){text.append(l10n::translate(s),ui::font(15.f),popup::mutedInk());};
        const auto heading=[&](const juce::String& s){text.append(l10n::translate(s)+"\n",ui::font(17.f,juce::Font::bold),popup::ink());};
        if(section==0)body("Free, open source and in private beta. Thanks for helping us improve ToneGrid!");
        if(section==1)
        {
            heading("2. Set up your audio");
            body("Select your audio interface and guitar input. ");
#if JUCE_WINDOWS
            body("On Windows, use ASIO with your interface manufacturer's driver. Install it first if needed.");
#elif JUCE_MAC
            body("Use CoreAudio with your interface selected for input and output.");
#else
            body("Use ALSA with your interface selected for input and output.");
#endif
        }
        if(section==2)
        {
            heading("3. Calibrate your guitar input");
            body("Match your interface to ToneGrid's signal chain. Start with input gain at minimum and follow the guide.\n\n"
                 "Need more drive? Try AMP Gain first. Changing interface gain affects every pedal and requires recalibration.");
        }
        if(section==3)
        {
            heading("Keep your beta updated");
            body("Click Check for update at the bottom every 2-3 days.");
        }
        return text;
    }
    void finish(bool suppress){const auto action=onDismiss;if(action)action(suppress);}
    std::array<juce::TextLayout,4> layouts;
    std::array<juce::Rectangle<int>,4> areas;
    l10n::LanguageChoice language{true};
};
}
