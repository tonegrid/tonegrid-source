#pragma once
#include "Shared/UI/PopupStyle.h"
namespace tonegrid::content {
class SettingsView final : public popup::View {
public:
    SettingsView() {
        setComponentID("editable-content-settings");
        for (auto* c : std::initializer_list<juce::Component*>{&help,&status,&open,&reload}) addAndMakeVisible(c);
        help.setText("Customize interface profiles, JAM tracks, LEARN lessons and ToneBank without rebuilding ToneGrid.\n\nOpen the folder to create editable JSON copies. Save your edits, then click Reload content. Removing an override restores the shipped catalogue.\n\nYour presets, recordings and current guitar sound are kept. Interface changes apply when you next open Guitar Calibration.",juce::dontSendNotification);
        help.setFont(ui::font(15));help.setMinimumHorizontalScale(1);help.setColour(juce::Label::textColourId,ui::ink());help.setJustificationType(juce::Justification::topLeft);
        status.setFont(ui::font(14));status.setMinimumHorizontalScale(1);status.setColour(juce::Label::textColourId,ui::secondaryInk());
        status.setJustificationType(juce::Justification::topLeft);
        open.setComponentID("editable-content-open");reload.setComponentID("editable-content-reload");
        open.setTooltip("Create missing editable copies and show their folder. Existing personal files are never replaced.");
        reload.setTooltip("Validate saved content before loading it. An invalid file keeps the previous catalogue and shows an error.");
        open.onClick=[this]{if(onOpen)onOpen();};reload.onClick=[this]{if(onReload)onReload();};
        setSize(620,340);
    }
    void resized() override {
        auto area=getLocalBounds().reduced(16);help.setBounds(area.removeFromTop(210));area.removeFromTop(12);
        auto buttons=area.removeFromTop(36);open.setBounds(buttons.removeFromLeft(225));buttons.removeFromLeft(12);reload.setBounds(buttons.removeFromLeft(180));
        area.removeFromTop(12);status.setBounds(area);
    }
    void message(const juce::String& value,bool busy=false){status.setText(value,juce::dontSendNotification);reload.setEnabled(!busy);}
    std::function<void()> onOpen,onReload;
private:
    juce::Label help,status;
    juce::TextButton open{"Open content folder"},reload{"Reload content"};
};
}
