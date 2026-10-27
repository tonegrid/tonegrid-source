#pragma once
#include "Shared/UI/PopupStyle.h"
#include "NamCalibrationMetadata.h"
#include <set>

namespace tonegrid::calibration
{
struct WarningContext
{
    bool finalAmpStage=false;
    bool outputNormalizationEnabled=false;
};
inline bool requiresCaptureWarning(const Metadata& info,WarningContext context={})
{
    if(info.invalid||(info.input&&!validLevel(*info.input))||(info.output&&!validLevel(*info.output)))return true;
    if(info.complete())return false;
    // Absent output is safe only for a normalized, final AMP+CAB. Invalid data
    // and unknown input remain warnings; upstream models need an output reference.
    if(!info.invalid&&info.input&&!info.output&&info.gearType.equalsIgnoreCase("amp_cab")
       &&context.finalAmpStage&&context.outputNormalizationEnabled)return false;
    return true;
}
class WarningStore
{
public:
    explicit WarningStore(juce::File preference):file(std::move(preference))
    {
        if(file.getSize()>1024*1024)return;
        if(auto xml=juce::XmlDocument::parse(file);xml&&xml->hasTagName("CalibrationWarnings"))
        {
            disabled=xml->getBoolAttribute("disabled");
            for(auto* item:xml->getChildIterator())if(validIdentity(item->getStringAttribute("sha256")))seen.insert(item->getStringAttribute("sha256"));
        }
    }
    bool shouldWarn(const Metadata& info,const juce::String& hash,WarningContext context={}) const
    {return !disabled&&requiresCaptureWarning(info,context)&&validIdentity(hash)&&!seen.contains(hash);}
    bool acknowledge(const juce::String& hash,bool disable=false)
    {
        if(validIdentity(hash))seen.insert(hash);disabled=disable||disabled;
        juce::XmlElement xml("CalibrationWarnings");xml.setAttribute("disabled",disabled);
        for(const auto& identity:seen)xml.createNewChildElement("Capture")->setAttribute("sha256",identity);
        if(file.getParentDirectory().createDirectory().failed())return false;
        juce::TemporaryFile temp(file);return temp.getFile().replaceWithText(xml.toString())&&temp.overwriteTargetFileWithTemporary();
    }
    bool globallyDisabled() const{return disabled;}
private:
    static bool validIdentity(const juce::String& s){return s.length()==64&&s.containsOnly("0123456789abcdef");}
    juce::File file;std::set<juce::String> seen;bool disabled=false;
};
class CalibrationLink final:public juce::Button
{
public:
    CalibrationLink():Button(juce::String::fromUTF8("Learn more about NAM calibration \xe2\x86\x97"))
    {setMouseCursor(juce::MouseCursor::PointingHandCursor);setWantsKeyboardFocus(true);}
    void paintButton(juce::Graphics& g,bool over,bool) override
    {
        g.setColour(juce::Colour(0xffc4ac7d));g.setFont(juce::FontOptions(14.f,over||hasKeyboardFocus(true)?juce::Font::underlined:juce::Font::plain));
        g.drawText(getButtonText(),getLocalBounds(),juce::Justification::centredLeft);
    }
};
class CaptureWarning final:public popup::View
{
public:
    CaptureWarning(const Metadata& info,const juce::String& filename)
    {
        using namespace juce;
        for(auto* child:std::initializer_list<Component*>{&name,&status,&message,&learn,&use,&disable})addAndMakeVisible(child);
        name.setComponentID("capture-calibration-filename");status.setComponentID("capture-calibration-status");
        message.setComponentID("capture-calibration-message");learn.setComponentID("capture-calibration-learn");use.setComponentID("capture-calibration-use");disable.setComponentID("capture-calibration-suppress");
        name.setFont(FontOptions(17.f,Font::bold));name.setMinimumHorizontalScale(1.f);name.setJustificationType(Justification::topLeft);name.setText(filename,dontSendNotification);name.setTooltip(filename);
        status.setFont(FontOptions(14.f));status.setColour(Label::textColourId,Colour(0xffd3b77f));status.setText(info.invalid?"Invalid calibration information":info.status()==CaptureStatus::partial?"Partially calibrated":"Uncalibrated",dontSendNotification);
        message.setMultiLine(true,true);message.setReadOnly(true);message.setScrollbarsShown(true);message.setCaretVisible(false);message.setFont(FontOptions(14.f));
        message.setColour(TextEditor::backgroundColourId,Colours::transparentBlack);message.setColour(TextEditor::outlineColourId,Colours::transparentBlack);message.setColour(TextEditor::focusedOutlineColourId,Colours::transparentBlack);message.setColour(TextEditor::textColourId,popup::ink());
        const auto copy=String(info.invalid?"This capture's calibration information cannot be used reliably. ToneGrid won't guess the missing signal levels, so you may need to adjust the gain to get the sound you want.":info.status()==CaptureStatus::partial?"This capture includes only part of its calibration information. ToneGrid can't automatically match all its original signal levels, so you may need to adjust the gain to get the sound you want.":"This capture doesn't include complete calibration information. ToneGrid can't automatically match all its original signal levels, so you may need to adjust the gain to get the sound you want.")+String::fromUTF8("\n\nThis doesn't mean the capture sounds bad \xe2\x80\x94 it can still sound great.");
        message.setText(copy,false);
        nameHeight=textHeight(filename,FontOptions(17.f,Font::bold),536)+6;
        messageHeight=textHeight(copy,FontOptions(14.f),520)+14;
        learn.onClick=[this,details=info.description()]
        {
            message.setText(details+"\n\nCalibration connects real guitar voltages to the digital levels expected by each NAM.\n\nYour interface profile supplies the first reference. Calibrated pedals convert back to that reference after processing. Their boosts and attenuation are preserved.\n\nA missing output reference interrupts guaranteed calibration of the next input. Unknown values stay unknown; ToneGrid does not guess them from loudness.\n\nAMP GAIN intentionally changes distortion. AMP VOLUME and LEVEL NORMALIZATION adjust playback after the amp. VST3 plugins have their own conventions.\n\nChoose GUITAR CALIBRATION to check your interface settings. Existing Input Trim, pedal controls and AMP GAIN remain available when a capture needs a manual adjustment.",false);message.setCaretPosition(0);
        };
        use.onClick=[this]{if(onUse)onUse(disable.getToggleState());};use.addShortcut(KeyPress(KeyPress::returnKey));
        learn.setExplicitFocusOrder(1);disable.setExplicitFocusOrder(2);use.setExplicitFocusOrder(3);setFocusContainerType(Component::FocusContainerType::keyboardFocusContainer);
        setSize(560,16+nameHeight+30+messageHeight+34+12+36+40);
    }
    bool popupBranding() const override{return false;}
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xffd3b77f));const float y=float(status.getBounds().getCentreY());juce::Path p;p.addTriangle(14,y-6,8,y+5,20,y+5);g.strokePath(p,juce::PathStrokeType(1.2f));g.drawLine(14,y-3,14,y,1.f);g.fillEllipse(13.4f,y+2,1.2f,1.2f);
        g.setColour(popup::mutedInk().withAlpha(.2f));g.drawHorizontalLine(disable.getY()-6,8.f,float(getWidth()-8));
    }
    void resized() override
    {auto area=getLocalBounds().reduced(8);name.setBounds(area.removeFromTop(nameHeight));status.setBounds(area.removeFromTop(30).withTrimmedLeft(22));message.setBounds(area.removeFromTop(messageHeight));learn.setBounds(area.removeFromTop(34));area.removeFromTop(12);disable.setBounds(area.removeFromTop(36));use.setBounds(area.removeFromTop(40).withWidth(140).reduced(0,4));}
    std::function<void(bool)> onUse;
private:
    static int textHeight(const juce::String& text,juce::Font font,int width)
    {juce::AttributedString attributed;attributed.append(text,font);juce::TextLayout layout;layout.createLayout(attributed,float(width));return juce::roundToInt(std::ceil(layout.getHeight()));}
    int nameHeight=30,messageHeight=140;juce::Label name,status;juce::TextEditor message;CalibrationLink learn;juce::TextButton use{"OK, Continue"};juce::ToggleButton disable{"Don't show this again (all captures)"};
};
}
