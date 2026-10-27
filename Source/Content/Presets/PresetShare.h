#pragma once
#include "Shared/UI/PopupStyle.h"
#include "Content/Presets/RigCodec.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace tonegrid::sharing
{
inline constexpr const char* recipient="christian@tonegrid.net";
inline constexpr const char* subject="New preset suggestion";
// Only the saved musical rig and explicit dependency references are shared.
// No device calibration, personal paths, recordings, tokens or NAM binaries.
inline juce::String body(const board::State& state,const board::RigIdentityLookup& identities,const juce::String& references)
{
    const auto xml=board::encode(state,juce::PluginDescription{},identities);
    return "Preset suggestion for review, not automatic publication.\n\nName: "+state.name+"\nToneGrid: "+JUCE_APPLICATION_VERSION_STRING+"\n\nCapture references:\n"+references+
        "\n\nComplete musical settings (ToneGrid preset XML):\n"+xml.toString();
}
inline juce::String mailto(const juce::String& text)
{return juce::String("mailto:")+recipient+"?subject="+juce::URL::addEscapeChars(subject,true)+"&body="+juce::URL::addEscapeChars(text,true);}
class View final:public popup::View
{
public:
    explicit View(juce::String value):text(std::move(value))
    {
        addAndMakeVisible(message);message.setText("ToneGrid opens a draft in your email application; you must send it yourself.\nIf your mail application truncates the message, use Copy all settings and paste them into the email.",juce::dontSendNotification);message.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(content);content.setMultiLine(true);content.setReadOnly(true);content.setScrollbarsShown(true);content.setText(text,false);
        addAndMakeVisible(open);addAndMakeVisible(copy);open.onClick=[this]{openDraft();};copy.onClick=[this]{juce::SystemClipboard::copyTextToClipboard(text);message.setText("All settings copied. Paste them into your email to christian@tonegrid.net. ToneGrid has not sent an email.",juce::dontSendNotification);};setSize(700,440);
    }
    bool openDraft()
    {const bool opened=juce::URL(mailto(text)).launchInDefaultBrowser();if(!opened)message.setText("No email application could be opened. Copy all settings and create an email manually to christian@tonegrid.net, subject: New preset suggestion.",juce::dontSendNotification);return opened;}
    void resized() override{auto r=getLocalBounds().reduced(8);message.setBounds(r.removeFromTop(64));auto buttons=r.removeFromBottom(38);open.setBounds(buttons.removeFromLeft(180));buttons.removeFromLeft(12);copy.setBounds(buttons.removeFromLeft(180));r.removeFromBottom(10);content.setBounds(r);}
private:juce::String text;juce::Label message;juce::TextEditor content;juce::TextButton open{"Open email"},copy{"Copy all settings"};
};
}
