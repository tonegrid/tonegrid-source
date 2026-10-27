#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Shared/UI/PopupStyle.h"
#include "LevelCalibration.h"
namespace tonegrid
{
class AudioSettingsPanel final:public popup::View
{
public:
    AudioSettingsPanel(juce::AudioDeviceManager& devices,bool initial,calibration::Settings={})
        :deviceManager(devices)
    {
        setComponentID("audio-settings-panel");
        rebuildSelector();
        for(auto* c:std::initializer_list<juce::Component*>{&language,&lite,&help,&calibrationButton,&calibrationHelp,&audioStatus})addAndMakeVisible(c);
        language.setComponentID("settings-language");
        audioStatus.setComponentID("audio-settings-status");audioStatus.setJustificationType(juce::Justification::topLeft);audioStatus.setMinimumHorizontalScale(1.f);audioStatus.setFont(ui::font(14));
        lite.setComponentID("nam-a2-lite");lite.setToggleState(initial,juce::dontSendNotification);
        tonegrid::l10n::text(help,"Use lighter NAM A2 captures to reduce CPU load. Applies to compatible amp and pedal models; other captures keep their normal sound. Remembered after restarting ToneGrid.",juce::dontSendNotification);
        help.setColour(juce::Label::textColourId,popup::mutedInk());help.setJustificationType(juce::Justification::topLeft);
        lite.onClick=[this]{const bool requested=lite.getToggleState();if(onLite&&!onLite(requested))lite.setToggleState(!requested,juce::dontSendNotification);};
        calibrationButton.setComponentID("guitar-calibration-settings");calibrationButton.onClick=[this]{auto action=onSetup;if(action)action();};
        tonegrid::l10n::text(calibrationHelp,"First choose your interface and guitar input above. Then click Guitar Calibration to continue.",juce::dontSendNotification);
        calibrationHelp.setColour(juce::Label::textColourId,popup::mutedInk());calibrationHelp.setJustificationType(juce::Justification::topLeft);
        for(auto* label:{&help,&calibrationHelp}){label->setFont(ui::font(14));label->setMinimumHorizontalScale(1.f);}
        setSize(760,popupContentHeight(760));
    }
    int popupMinimumWidth() const override{return 480;}
    int popupContentHeight(int width) const override{return selector->getHeight()+statusHeight(width)+212+textHeight(help,width-44)+textHeight(calibrationHelp,width-44);}
    void languageChanged()override
    {
        // The JUCE selector reads LocalisedStrings when constructed. Recreate
        // only this UI, on the next message turn, preserving the device manager.
        if(languageRefreshPending)return;languageRefreshPending=true;
        juce::MessageManager::callAsync([safe=juce::Component::SafePointer<AudioSettingsPanel>(this)]{if(safe){safe->languageRefreshPending=false;safe->rebuildSelector();safe->resized();if(auto* parent=safe->getParentComponent())parent->resized();}});
    }
    void setAudioStatus(const juce::String& value,bool error)
    {
        if(audioStatus.getText()==value)return;
        l10n::text(audioStatus,value);audioStatus.setColour(juce::Label::textColourId,error?juce::Colour(0xffe3bd7f):popup::mutedInk());resized();
    }
    void paint(juce::Graphics& g)override{g.setColour(popup::outline());g.drawHorizontalLine(selector->getBottom()+8,20,float(getWidth()-20));}
    void resized()override
    {
        if(layingOut)return;juce::ScopedValueSetter<bool> guard(layingOut,true);
        language.setBounds(22,0,getWidth()-44,68);
        audioStatus.setBounds(22,84,getWidth()-44,juce::jmax(0,statusHeight(getWidth())-4));audioStatus.setVisible(audioStatus.getText().isNotEmpty());
        selector->setBounds(16,88+statusHeight(getWidth()),getWidth()-32,selector->getHeight());selector->resized();
        int y=selector->getBottom()+16;lite.setBounds(22,y,180,32);y+=36;
        const int helpHeight=textHeight(help,getWidth()-44);help.setBounds(22,y,getWidth()-44,helpHeight);y+=helpHeight+16;
        calibrationButton.setBounds(22,y,juce::jmin(340,getWidth()-44),34);y+=42;
        const int calibrationHeight=textHeight(calibrationHelp,getWidth()-44);calibrationHelp.setBounds(22,y,getWidth()-44,calibrationHeight);
        setSize(getWidth(),popupContentHeight(getWidth()));
    }
    void childBoundsChanged(juce::Component* child)override{if(child==selector.get()&&!layingOut)resized();}
    std::function<bool(bool)> onLite;
    std::function<void()> onSetup;
private:
    int statusHeight(int width)const{return audioStatus.getText().isEmpty()?0:textHeight(audioStatus,width-44)+8;}
    static int textHeight(const juce::Label& label,int width)
    {juce::AttributedString text;text.append(label.getText(),label.getFont(),popup::mutedInk());juce::TextLayout layout;layout.createLayout(text,float(juce::jmax(1,width-20)));return juce::jmax(44,juce::roundToInt(layout.getHeight())+26);}
    void rebuildSelector()
    {
        juce::ScopedValueSetter<bool> guard(layingOut,true);
        selector=std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,1,1,1,2,false,false,false,false);
        selector->setComponentID("audio-device-selector");addAndMakeVisible(*selector);
    }
    bool layingOut=false,languageRefreshPending=false;
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    l10n::LanguageChoice language;juce::ToggleButton lite{"NAM A2 Lite"};juce::Label help,calibrationHelp;
    tonegrid::l10n::TextButton calibrationButton{"Guitar Calibration"};juce::Label audioStatus;
};
}
