#pragma once
#include "Shared/UI/PopupStyle.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace tonegrid
{
class MissingCapturesView final:public popup::View
{
public:
    MissingCapturesView(const juce::String& preset,const juce::StringArray& missing)
    {
        setComponentID("missing-captures-view");
        addAndMakeVisible(details);details.setComponentID("missing-captures-details");
        details.setMultiLine(true);details.setReadOnly(true);details.setScrollbarsShown(true);
        details.setFont(ui::font(ui::controlSize));details.setColour(juce::TextEditor::backgroundColourId,popup::field());
        details.setColour(juce::TextEditor::textColourId,popup::ink());
        details.setText(l10n::format("missing.description",{{"preset",preset},{"captures",missing.joinIntoString("\n\n")}}),false);
        addAndMakeVisible(updateStatus);updateStatus.setComponentID("missing-captures-update-status");
        tonegrid::l10n::text(updateStatus,"ToneGrid update available: Checking...",juce::dontSendNotification);updateStatus.setColour(juce::Label::textColourId,popup::ink());
        addAndMakeVisible(update);update.setComponentID("missing-captures-update");update.setEnabled(false);
        update.setColour(juce::TextButton::buttonColourId,ui::tabBlue());
        update.onClick=[]{juce::URL("https://www.tonegrid.net/download").launchInDefaultBrowser();};
        tonegrid::l10n::tooltip(update,"Open the download page for the latest ToneGrid. The complete package includes its Factory amps and cabinets.");
        addAndMakeVisible(search);search.setComponentID("missing-captures-search");
        search.onClick=[]{juce::URL("https://www.tone3000.com/").launchInDefaultBrowser();};
        addAndMakeVisible(close);close.setComponentID("missing-captures-close");close.onClick=[this]{if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->closeButtonPressed();};
        setSize(660,390);
    }
    void setUpdateResult(bool available,const juce::String& version,bool failed)
    {tonegrid::l10n::text(updateStatus,failed?"ToneGrid update available: Unable to check":available?l10n::format("missing.yes_update",{{"version",version}}):l10n::translate("ToneGrid update available: No"),juce::dontSendNotification);update.setEnabled(available&&!failed);update.setVisible(available&&!failed);}
    void checking(){tonegrid::l10n::text(updateStatus,"ToneGrid update available: Checking...",juce::dontSendNotification);update.setVisible(true);update.setEnabled(false);}
    std::vector<juce::Component*> popupFooterActions() override{return {&close,&update,&search};}
    int popupMinimumWidth()const override{return 280;}
    void resized()override{auto r=getLocalBounds().reduced(8);updateStatus.setBounds(r.removeFromBottom(32));details.setBounds(r);}
private:
    juce::TextEditor details;
    juce::Label updateStatus;
    tonegrid::l10n::TextButton search{"Find on Tone3000"},close{"Close"},update{"Update ToneGrid"};
};
}
