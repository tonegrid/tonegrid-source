#pragma once
#include "Shared/UI/PopupWindow.h"
#include "DiagnosticReport.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace tonegrid::diagnostics
{
class View final:public popup::View
{
public:
    explicit View(juce::String report):text(std::move(report))
    {
        setComponentID("diagnostic-report");
        for(auto* c:std::initializer_list<juce::Component*>{&message,&content,&open,&copy})addAndMakeVisible(c);
        message.setJustificationType(juce::Justification::topLeft);message.setMinimumHorizontalScale(1.f);
        message.setText("Review the report and send the prepared email yourself. If your mail app leaves out details, use Copy Diagnostic and paste the complete report. No recordings or captures are included.",juce::dontSendNotification);
        content.setComponentID("diagnostic-text");content.setMultiLine(true);content.setReadOnly(true);content.setScrollbarsShown(true);content.setText(text,false);
        open.setComponentID("diagnostic-open-email");copy.setComponentID("diagnostic-copy");
        open.onClick=[this]{openDraft();};copy.onClick=[this]{juce::SystemClipboard::copyTextToClipboard(text);message.setText("Complete report copied. Paste it into your email to "+juce::String(recipient)+". You must send the email yourself.",juce::dontSendNotification);};
        setSize(680,440);
    }
    int popupMinimumWidth()const override{return 420;}
    bool popupLocalised()const override{return false;}
    void resized()override
    {
        auto r=getLocalBounds().reduced(8);message.setBounds(r.removeFromTop(90));auto buttons=r.removeFromBottom(38);
        open.setBounds(buttons.removeFromLeft(160));buttons.removeFromLeft(12);copy.setBounds(buttons.removeFromLeft(180));r.removeFromBottom(10);content.setBounds(r);
    }
    bool openDraft()
    {
        const bool opened=launchEmail(mailto(text));
        if(!opened)message.setText("Your email app could not be opened. Use Copy Diagnostic, then email the report to "+juce::String(recipient)+". Subject: "+subject(),juce::dontSendNotification);
        return opened;
    }
    const juce::String& report()const{return text;}
    std::function<bool(const juce::String&)> launchEmail=[](const juce::String& uri){return juce::URL(uri).launchInDefaultBrowser();};
private:
    juce::String text;juce::Label message;juce::TextEditor content;
    juce::TextButton open{"Open Email"},copy{"Copy Diagnostic"};
};
}
