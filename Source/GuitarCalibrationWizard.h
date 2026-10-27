#pragma once
#include "Shared/UI/PopupStyle.h"
#include "Content/Calibration/InterfaceProfiles.h"
#include "CalibrationStatus.h"
#include "RawInputVerification.h"
#include "CalibrationConfigureUi.h"

namespace tonegrid::calibration
{
struct DeviceInput
{
    juce::String type,name; juce::StringArray channels; int channel=-1; bool connected=false;
    bool sameAs(const DeviceInput& other) const
    { return type==other.type && name==other.name && channel==other.channel && connected==other.connected; }
};
class GuitarWizard final : public popup::View, private juce::Timer
{
public:
    explicit GuitarWizard(Settings settings, const juce::File& profileFile = {}) : draft(std::move(settings)), profiles(interfaceProfiles(profileFile))
    {
        using namespace juce;
        for (auto* c:std::initializer_list<Component*>{&progress,&headline,&explanation,&summary,&profile,&input,&reference,&referenceLabel,&conditions,&source,&meterText,&signalStatus,&automatic,&back,&next,&feedback,&inputTitle,&inputExplanation,&inputReason,&gainTitle,&gainExplanation,&gainReason,&calibrationValue,&calibrationExplanation,&dbuHelp,&gainIllustration}) addAndMakeVisible(c);
        for (auto* label:{&progress,&headline,&explanation,&summary,&referenceLabel,&conditions,&meterText,&signalStatus,&feedback,&inputTitle,&inputExplanation,&inputReason,&gainTitle,&gainExplanation,&gainReason,&calibrationValue,&calibrationExplanation})
        {
            label->setJustificationType(Justification::topLeft); label->setColour(Label::textColourId,popup::ink());
            label->setFont(FontOptions(14.f)); label->setMinimumHorizontalScale(1.f);
        }
        headline.setFont(FontOptions(21.f)); progress.setColour(Label::textColourId,popup::mutedInk());
        conditions.setColour(Label::textColourId,popup::mutedInk()); feedback.setFont(FontOptions(13.f));
        profile.setComponentID("calibration-profile"); tonegrid::l10n::item(profile,"OTHER INTERFACE / MANUAL",1);
        int chosen=1;
        for (size_t i=0;i<profiles.size();++i) { profile.addItem(profiles[i].name,int(i)+2); if (profiles[i].matchesId(draft.profileId)) chosen=int(i)+2; }
        profile.setSelectedId(chosen,dontSendNotification); profile.onChange=[this]{chooseProfile();};
        input.setComponentID("calibration-input-channel"); input.onChange=[this]
        {
            if (onChannel && input.getSelectedId()>0) onChannel(input.getSelectedId()-1);
            device={}; verification={}; tick();
        };
        reference.setComponentID("calibration-manual-reference"); reference.setInputRestrictions(12,"+-0123456789.eE");
        tonegrid::l10n::placeholder(reference,"Enter manufacturer's dBu value",popup::mutedInk()); reference.onTextChange=[this]{updateCalibrationCopy();validate();};
        if (valid(draft) && draft.profileId=="manual") manualReference=number(draft.referenceDbU);
        automatic.setToggleState(draft.enabled,dontSendNotification); automatic.setComponentID("calibration-automatic");
        automatic.setClickingTogglesState(true);automatic.onClick=[this]{updateVerifyCopy();};meterText.setComponentID("calibration-raw-meter");
        signalStatus.setComponentID("calibration-input-status"); summary.setComponentID("calibration-summary");
        explanation.setComponentID("calibration-explanation"); feedback.setComponentID("calibration-feedback");
        tonegrid::l10n::text(inputTitle,"1. Plug in your guitar",dontSendNotification);inputTitle.setFont(FontOptions(15.f,Font::bold));
        inputExplanation.setComponentID("calibration-input-explanation");conditions.setComponentID("calibration-required-settings");
        tonegrid::l10n::text(inputReason,"This setting is designed for electric guitars. Without it, your signal may sound weaker, duller, or respond differently to amps and pedals.",dontSendNotification);inputReason.setColour(Label::textColourId,popup::mutedInk());
        tonegrid::l10n::text(gainTitle,"2. Set the input gain",dontSendNotification);gainTitle.setFont(FontOptions(15.f,Font::bold));
        gainExplanation.setComponentID("calibration-gain-explanation");tonegrid::l10n::text(gainReason,"Keep it there while using ToneGrid to preserve accurate NAM calibration.",dontSendNotification);gainReason.setColour(Label::textColourId,popup::mutedInk());
        calibrationValue.setComponentID("calibration-value-explanation");calibrationExplanation.setComponentID("calibration-dbu-explanation");
        dbuHelp.onClick=[this]{showDbuHelp=!showDbuHelp;tonegrid::l10n::button(dbuHelp,showDbuHelp?"Hide explanation":"What is dBu?");updateCalibrationCopy();resizeConfigure();};
        back.onClick=[this]{if (step>0) { --step; showStep(); }};
        next.onClick=[this]{advance();}; next.setComponentID("calibration-next");back.setComponentID("calibration-back");
        int focus=1;
        for (auto* c:std::initializer_list<Component*>{&profile,&input,&reference,&dbuHelp,&source,&automatic,&back,&next}) c->setExplicitFocusOrder(focus++);
        setFocusContainerType(Component::FocusContainerType::keyboardFocusContainer); next.addShortcut(KeyPress(KeyPress::returnKey));
        source.onClick=[this]{const auto index=profile.getSelectedId()-2; if (index>=0 && index<int(profiles.size())) URL(profiles[size_t(index)].source).launchInDefaultBrowser();};
        setSize(600,440); chooseProfile(); showStep(); startTimerHz(20);
    }
    ~GuitarWizard() override { stopTimer(); }
    std::function<DeviceInput()> currentDevice;
    std::function<RawReading()> reading;
    std::function<bool(int)> onChannel;
    std::function<bool(Settings)> onSave;
    std::function<void(int)> onBodyHeight;
    std::function<double()> now=[] { return juce::Time::getMillisecondCounterHiRes()*.001; };
    int currentStep() const { return step; }
    int popupMinimumWidth()const override{return 240;}
    int popupContentHeight(int width)const override{return step==1?configureHeight(width):440;}
    std::vector<juce::Component*> popupFooterActions()override{return {&back,&next};}
    void refresh() { tick(); revealFocusedControl(); }
    void advance()
    {
        validate(); if (!next.isEnabled()) return;
        if (step<2)
        {
            ++step;
            if (step==2) { verification={}; displayPeak=heldPeak=0; peakUntil=0; }
            showStep(); return;
        }
        const auto value=parseReference(reference.getText()); if (!value) return;
        auto confirmed=draft; confirmed.referenceDbU=*value; confirmed.enabled=automatic.getToggleState();
        confirmed.profileId=profile.getSelectedId()==1?"manual":profiles[size_t(profile.getSelectedId()-2)].id;
        confirmed.deviceType=device.type; confirmed.deviceName=device.name; confirmed.channel=device.channel;
        confirmed.completed=true; confirmed.needsVerification=false;
        confirmed.signalVerified=verification.safe(device.connected,now());
        if (onSave && onSave(confirmed))
        {
            saved=true; tonegrid::l10n::text(progress,"3 / 3     SETTINGS SAVED",juce::dontSendNotification);
            tonegrid::l10n::text(headline,"Calibration Saved",juce::dontSendNotification);
            auto copy=juce::String("Your settings are saved. You're ready to play.");
            if (!confirmed.enabled) copy+="\nCalibration is switched off.";
            explanation.setText(l10n::tr("calibration.saved_copy")+(!confirmed.enabled?"\n"+l10n::tr("calibration.saved_off"):juce::String{}),juce::dontSendNotification);
            summary.setText("Interface: "+profile.getText()+"\nInput: "+juce::String(device.channel+1)+" - "+device.name+"\nInput reference: "+number(*value)+" dBu\nAutomatic calibration: "+(confirmed.enabled?"ON":"OFF"),juce::dontSendNotification);
            tonegrid::l10n::text(feedback,confirmed.signalVerified?"Input signal checked.":"Saved without a signal check.",juce::dontSendNotification);
            feedback.setColour(juce::Label::textColourId,popup::mutedInk()); tonegrid::l10n::button(next,"DONE");
            next.onClick=[safe=juce::Component::SafePointer<GuitarWizard>(this)]{if (safe) if (auto* window=safe->findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed();};
            for (auto* c:std::initializer_list<juce::Component*>{&automatic,&meterText,&signalStatus,&back}) c->setVisible(false);
            resized(); repaint();
        }
        else tonegrid::l10n::text(feedback,"Could not save. Your previous settings were kept.",juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override
    {
        if(step==1)
        {
            const int y=configureTop();
            g.setColour(popup::outline().withAlpha(.5f));g.drawHorizontalLine(y-8,10.f,float(getWidth()-10));return;
        }
        if (step!=2 || saved) return;
        const auto bar=juce::Rectangle<float>(10,268,float(getWidth()-20),12);
        g.setColour(popup::field()); g.fillRoundedRectangle(bar,4);
        const float db=juce::Decibels::gainToDecibels(displayPeak,-100.f);
        g.setColour(verification.status(now())==InputStatus::clipping?juce::Colour(0xffd8b87b):juce::Colour(0xff889ca4));
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*juce::jlimit(0.f,1.f,(db+80)/80)),4);
        const float peakX=bar.getX()+bar.getWidth()*juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(heldPeak,-100.f)+80)/80);
        g.setColour(popup::ink()); g.drawVerticalLine(juce::roundToInt(peakX),bar.getY(),bar.getBottom());
        if (verification.status(now())==InputStatus::detected)
        { juce::Path check; check.startNewSubPath(11,330); check.lineTo(15,334); check.lineTo(23,324); g.setColour(juce::Colour(0xff8ab49f)); g.strokePath(check,juce::PathStrokeType(1.6f)); }
    }
    void resized() override
    {
        const int w=getWidth()-20; progress.setBounds(10,2,w,22); headline.setBounds(10,34,w,52);
        if(step==1)
        {
            headline.setBounds(10,34,w,28);profile.setBounds(10,70,w,32);input.setBounds(10,110,w,30);
            referenceLabel.setBounds(10,148,w-132,32);reference.setBounds(getWidth()-130,148,120,32);
            const int estimateExtra=getWidth()<460?44:28;calibrationValue.setBounds(10,184,w,estimateExtra);const int helpY=186+(hasEstimate()?estimateExtra:0);
            dbuHelp.setBounds(10,helpY,120,26);source.setBounds(getWidth()-130,helpY,120,26);
            calibrationExplanation.setBounds(10,helpY+32,w,getWidth()<460?64:44);
            const int y=configureTop(),inputHeight=getWidth()<460?80:56;
            inputTitle.setBounds(10,y,w,24);inputExplanation.setBounds(10,y+28,w,inputHeight);
            const int gainY=y+inputHeight+38;gainTitle.setBounds(10,gainY,w,24);const bool drawKnob=gainAtMinimum&&getWidth()>=460;
            const int textWidth=w-(drawKnob?110:0);gainExplanation.setBounds(10,gainY+28,textWidth,getWidth()<460?96:64);gainIllustration.setBounds(getWidth()-110,gainY,100,100);gainIllustration.setVisible(drawKnob);
            feedback.setBounds(10,gainY+(getWidth()<460?130:98),w,getWidth()<460?46:24);
            if(back.getParentComponent()==this){back.setBounds(10,getHeight()-40,112,34);next.setBounds(getWidth()-216,getHeight()-40,206,34);}return;
        }
        explanation.setBounds(10,92,w,step==0?160:step==2?70:45);
        profile.setBounds(10,145,w,34); input.setBounds(10,189,w,30);
        referenceLabel.setBounds(10,230,310,24); reference.setBounds(330,225,w-320,34);
        conditions.setBounds(10,269,w-110,76); source.setBounds(getWidth()-110,269,100,30);
        summary.setBounds(10,186,w,saved?116:70); meterText.setBounds(10,286,w,25);
        signalStatus.setBounds(30,318,w-20,28); automatic.setBounds(10,352,juce::jmin(220,w),32);
        feedback.setBounds(10,saved?322:392,w,saved?90:40);
        if(back.getParentComponent()==this){back.setBounds(10,getHeight()-40,112,34);next.setBounds(getWidth()-216,getHeight()-40,206,34);}
    }
    void focusOfChildComponentChanged(juce::Component::FocusChangeType) override
    { revealFocusedControl(); }
private:
    void revealFocusedControl()
    {
        if(step!=1)return;
        auto* focused=juce::Component::getCurrentlyFocusedComponent();auto* viewport=findParentComponentOfClass<juce::Viewport>();
        if(!focused||!isParentOf(focused)||!viewport)return;
        const auto bounds=getLocalArea(focused,focused->getLocalBounds());const auto visible=viewport->getViewArea();
        if(bounds.getY()<visible.getY())viewport->setViewPosition(visible.getX(),juce::jmax(0,bounds.getY()-8));
        else if(bounds.getBottom()>visible.getBottom())viewport->setViewPosition(visible.getX(),bounds.getBottom()-visible.getHeight()+8);
    }
    Settings draft; std::vector<InterfaceProfile> profiles; DeviceInput device; Verification verification;
    int step=0,previousProfile=0; bool saved=false; float displayPeak=0,heldPeak=0; double peakUntil=0;
    juce::String manualReference="12";
    bool gainAtMinimum=false,showDbuHelp=false;
    juce::Label progress,headline,explanation,summary,referenceLabel,conditions,meterText,signalStatus,feedback;
    juce::Label inputTitle,inputExplanation,inputReason,gainTitle,gainExplanation,gainReason,calibrationValue,calibrationExplanation;
    DbuHelpLink dbuHelp;MinimumGainIllustration gainIllustration;
    juce::ComboBox profile,input; juce::TextEditor reference;
    tonegrid::l10n::TextButton source{"Official specs"},back{"BACK"},next{"NEXT"};
    tonegrid::l10n::TextButton automatic{"Calibration: ON"};
    static juce::String number(float value)
    { return (value>=0?"+":"")+juce::String(value,4).trimCharactersAtEnd("0").trimCharactersAtEnd("."); }
    void updateCalibrationCopy()
    {
        const int index=profile.getSelectedId()-2;
        tonegrid::l10n::text(calibrationValue,index<0?"Estimate. Enter an exact value if known.":"Starting estimate: "+number(profiles[size_t(index)].reference)+" dBu.",juce::dontSendNotification);calibrationValue.setVisible(step==1&&hasEstimate());
        tonegrid::l10n::text(calibrationExplanation,"dBu describes your interface's input sensitivity. It helps ToneGrid make the pedals and amps respond correctly. It is not a volume target to reach while playing.",juce::dontSendNotification);
        calibrationExplanation.setVisible(step==1&&showDbuHelp);
    }
    bool hasEstimate()const{const int i=profile.getSelectedId()-2;return i<0||profiles[size_t(i)].estimated;}
    int configureTop()const{return 224+(hasEstimate()?(getWidth()<460?44:28):0)+(showDbuHelp?(getWidth()<460?72:52):0);}
    int configureHeight(int width)const{return 442+(hasEstimate()?(width<460?44:28):0)+(showDbuHelp?(width<460?72:52):0)+(width<460?80:0);}
    void resizeConfigure(){if(step==1){setSize(getWidth(),configureHeight(getWidth()));if(onBodyHeight)onBodyHeight(getHeight());resized();repaint();}}
    void chooseProfile()
    {
        if (previousProfile==1) manualReference=reference.getText();
        const int index=profile.getSelectedId()-2; reference.setReadOnly(index>=0);
        source.setVisible(step==1 && index>=0);
        if (index>=0 && index<int(profiles.size()))
        {
            const auto& p=profiles[size_t(index)]; reference.setText(number(p.reference),false);
            tonegrid::l10n::tooltip(conditions,p.notes+"\n"+p.source);
        }
        else
        {
            reference.setText(manualReference,false);
            tonegrid::l10n::tooltip(conditions,"Use the manufacturer's instrument-input reference, not a microphone or line-output specification.");
        }
        const auto copy=configureCopy(index>=0&&index<int(profiles.size())?&profiles[size_t(index)]:nullptr);
        const auto concise=conciseConfigureCopy(index>=0&&index<int(profiles.size())?&profiles[size_t(index)]:nullptr);
        gainAtMinimum=copy.minimum;inputExplanation.setText(l10n::translate(concise.input),juce::dontSendNotification);
        conditions.setText(copy.details,juce::dontSendNotification);conditions.setVisible(false);source.setTooltip(copy.details);gainExplanation.setText(l10n::translate(concise.gain),juce::dontSendNotification);
        gainIllustration.setVisible(step==1&&gainAtMinimum);showDbuHelp=false;tonegrid::l10n::button(dbuHelp,"What is dBu?");updateCalibrationCopy();resizeConfigure();resized();repaint();
        previousProfile=profile.getSelectedId();profile.setTooltip(profile.getText());validate();
    }
    void showStep()
    {
        using namespace juce;
        tonegrid::l10n::text(progress,String(step+1)+" / 3     "+l10n::translate(step==0?"UNDERSTAND":step==1?"CONFIGURE":"VERIFY"),dontSendNotification);
        for (auto* c:std::initializer_list<Component*>{&profile,&input,&reference,&referenceLabel,&source,&inputTitle,&inputExplanation,&inputReason,&gainTitle,&gainExplanation,&gainReason,&calibrationValue,&calibrationExplanation,&dbuHelp}) c->setVisible(step==1);
        conditions.setVisible(false);
        explanation.setVisible(step!=1);gainIllustration.setVisible(step==1&&gainAtMinimum);
        inputReason.setVisible(false);gainReason.setVisible(false);calibrationExplanation.setVisible(step==1&&showDbuHelp);
        source.setVisible(step==1 && profile.getSelectedId()>1);
        for (auto* c:std::initializer_list<Component*>{&automatic,&meterText,&signalStatus,&summary}) c->setVisible(step==2);
        back.setVisible(step>0); tonegrid::l10n::button(next,step==2?"SAVE CALIBRATION":"NEXT"); feedback.setText({},dontSendNotification);
        if (step==0)
        {
            tonegrid::l10n::text(headline,"Get your guitar ready.",dontSendNotification);
            tonegrid::l10n::text(explanation,"Choose your audio interface and follow two simple steps.\n\nThen play a few notes to check your input.",dontSendNotification);
        }
        if (step==1)
        {
            tonegrid::l10n::text(headline,"Select your audio interface",dontSendNotification);
            tonegrid::l10n::text(referenceLabel,"Input level (dBu)",dontSendNotification);updateCalibrationCopy();
        }
        if (step==2) updateVerifyCopy();setSize(getWidth(),step==1?configureHeight(getWidth()):440);if(onBodyHeight)onBodyHeight(getHeight());resized();tick();
        if(step==1&&isShowing())profile.grabKeyboardFocus();
    }
    void updateVerifyCopy()
    {
        tonegrid::l10n::text(headline,"Play a few notes.",juce::dontSendNotification);
        tonegrid::l10n::text(explanation,"ToneGrid checks your guitar input. You can leave your pedals and amp on.",juce::dontSendNotification);
        tonegrid::l10n::button(automatic,automatic.getToggleState()?"Calibration: ON":"Calibration: OFF");
        const auto value=parseReference(reference.getText());
        summary.setText(profile.getText()+"\nInput "+juce::String(device.channel+1)+": "+device.name+"\nReference: "+(value?number(*value)+" dBu":"Not available")+" | Automatic calibration "+(automatic.getToggleState()?"ON":"OFF"),juce::dontSendNotification);
        if (!automatic.getToggleState()) tonegrid::l10n::tooltip(summary,"ToneGrid will not automatically compensate NAM calibration levels. Captures may respond differently from their original hardware."); else summary.setTooltip({});
    }
    void validate()
    {
        if (saved) return;
        bool configured=parseReference(reference.getText()).has_value() && device.connected && device.channel>=0;
        const int index=profile.getSelectedId()-2;
        if (index>=0 && index<int(profiles.size())) configured=configured && profiles[size_t(index)].supports(device.channel);
        const double time=now();
        const bool verified=verification.safe(device.connected,time);
        const bool manualSave=verification.manualAvailable(time);
        next.setEnabled(step==0 || (configured && (step==1 || verified || manualSave)));
    }
    void tick()
    {
        if (saved) return;
        if (currentDevice)
        {
            const auto current=currentDevice();
            if (!current.sameAs(device))
            {
                device=current; verification={};
                input.clear(juce::dontSendNotification);
                for (int i=0;i<device.channels.size();++i) tonegrid::l10n::item(input,"Input "+juce::String(i+1)+": "+device.channels[i],i+1);
                input.setSelectedId(device.channel+1,juce::dontSendNotification);
                if (step==2) { step=1; showStep(); return; }
            }
        }
        // Reuse and drain the existing pre-graph peak tap on every screen.
        const auto raw=reading?reading():RawReading{};
        if (step==2)
        {
            const double time=now(); verification.update(raw,time);
            const float peak=std::isfinite(raw.peak)?std::abs(raw.peak):2.f;
            displayPeak=juce::jmax(peak,displayPeak*.85f);
            if (peak>=heldPeak || time>=peakUntil) { heldPeak=peak; peakUntil=time+2; }
            const auto db=juce::Decibels::gainToDecibels(displayPeak,-100.f),heldDb=juce::Decibels::gainToDecibels(heldPeak,-100.f);
            meterText.setText(raw.connected?"Input level: "+juce::String(db,1)+" dBFS":"Input unavailable",juce::dontSendNotification);juce::ignoreUnused(heldDb);
            const auto state=verification.status(time);
            signalStatus.setText(state==InputStatus::detected?"Signal detected":state==InputStatus::clipping?"Input too loud":state==InputStatus::unavailable?"Input unavailable":"Waiting for your guitar",juce::dontSendNotification);
            signalStatus.setColour(juce::Label::textColourId,state==InputStatus::detected?juce::Colour(0xff8ab49f):state==InputStatus::clipping?juce::Colour(0xffd8b87b):popup::mutedInk());
            feedback.setColour(juce::Label::textColourId,state==InputStatus::clipping?juce::Colour(0xffd8b87b):popup::mutedInk());
            tonegrid::l10n::text(feedback,state==InputStatus::unavailable?"Select your interface in Audio Settings.":state==InputStatus::clipping?"Check the input GAIN setting for your interface.":state==InputStatus::detected?"Looks good. Save your calibration.":verification.manualAvailable(time)?"No signal detected yet. You can still save these settings.":"Play a few notes when you're ready.",juce::dontSendNotification);
            repaint();
        }
        else if (step==1)
        {
            const int index=profile.getSelectedId()-2;
            tonegrid::l10n::text(feedback,!device.connected?"Connect your interface and select it in Audio Settings.":index>=0 && !profiles[size_t(index)].supports(device.channel)?"Choose the instrument input channel specified by this profile.":"",juce::dontSendNotification);
        }
        validate();
    }
    void timerCallback() override { refresh(); }
};
}
