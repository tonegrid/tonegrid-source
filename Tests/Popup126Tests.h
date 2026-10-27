#pragma once
#include "Shared/UI/PopupWindow.h"
#include "AudioSettingsPanel.h"
#include "BetaWelcome.h"
#include "Effects/AmpEq/AdvancedEqControls.h"
#include "GuitarCalibrationWizard.h"
#include "Shared/UI/Library/NamLibraryBrowser.h"
#include "RawInputTestDevice.h"
#include "TunerControls.h"

namespace popup126Tests
{
class DeviceType final:public juce::AudioIODeviceType
{
public:
    DeviceType():AudioIODeviceType("Test"){}
    void scanForDevices()override{}
    juce::StringArray getDeviceNames(bool)const override{return {"Synthetic input"};}
    int getDefaultDeviceIndex(bool)const override{return 0;}
    int getIndexOfDevice(juce::AudioIODevice*,bool)const override{return 0;}
    bool hasSeparateInputsAndOutputs()const override{return false;}
    juce::AudioIODevice* createDevice(const juce::String&,const juce::String&)override{return new RawInputTestDevice();}
};
inline bool run(const juce::File& output,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+label+"\n";};
    output.createDirectory();Component owner;owner.setBounds(0,0,640,360);
    const auto snapshot=[&](Component& c,const String& name){MemoryOutputStream png;check(PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds(),true,1.5f),png)&&output.getChildFile(name+".png").replaceWithData(png.getData(),png.getDataSize()),"HiDPI snapshot "+name);};
    const auto exercise=[&](tonegrid::popup::View& body,const String& name,int minimumHeight=0)
    {
        tonegrid::popup::Window window(name,body,owner,false,minimumHeight);window.setVisible(true);
        check(window.isResizable(),name+" is resizable");
        auto* frame=dynamic_cast<tonegrid::popup::Frame*>(window.getContentComponent());
        auto* viewport=body.findParentComponentOfClass<Viewport>();
        check(frame&&viewport,name+" has scrollable body");if(!frame||!viewport)return;
        for(const auto size:std::initializer_list<Point<int>>{{800,540},{600,400},{480,360},{360,340}})
        {
            window.setBoundsConstrained(window.getBounds().withSize(size.x,size.y));
            check(window.getWidth()==size.x&&window.getHeight()==size.y,name+" resize "+String(size.x)+"x"+String(size.y));
            auto* close=frame->findChildWithID("popup-close");const auto before=close?close->getBounds():juce::Rectangle<int>{};
            check(close&&close->getWidth()==24&&frame->getLocalBounds().contains(before)&&frame->getComponentAt(before.getCentre())==close,name+" Close remains visible and clickable");
            auto* border=dynamic_cast<tonegrid::popup::ResizeBorder*>(frame->findChildWithID("popup-resize-border"));
            check(border&&frame->getComponentAt(border->getBounds().getBottomRight()-Point<int>(3,3))==border,name+" visible corner accepts resize input");
            check(border&&frame->getComponentAt(border->getBounds().getBottomRight()-Point<int>(10,10))==border,name+" drawn grip accepts resize input");
            viewport->setViewPosition(body.getWidth(),body.getHeight());
            check(viewport->getViewArea().getBottom()>=body.getHeight()-1,name+" last controls can be reached");
            check(viewport->getViewArea().getRight()>=body.getWidth()-1,name+" rightmost controls can be reached");
            check(close&&close->getBounds()==before,name+" scrolling keeps header fixed");
            check(body.getTransform().isIdentity(),name+" text remains at desktop size");
            viewport->setViewPosition(0,0);
            if(size.x==600)snapshot(*frame,name+"-600");
        }
        window.closeButtonPressed();check(!window.isVisible(),name+" closes normally");
    };
    AudioDeviceManager devices;
    while(!devices.getAvailableDeviceTypes().isEmpty())devices.removeAudioDeviceType(devices.getAvailableDeviceTypes()[0]);
    devices.addAudioDeviceType(std::make_unique<DeviceType>());
    check(devices.initialise(1,2,nullptr,true).isEmpty(),"synthetic audio interface opens without hardware");
    {
        tonegrid::AudioSettingsPanel panel(devices,false);exercise(panel,"Audio Settings");
        auto* selector=panel.findChildWithID("audio-device-selector");auto* lite=panel.findChildWithID("nam-a2-lite");
        auto* calibration=dynamic_cast<TextButton*>(panel.findChildWithID("guitar-calibration-settings"));
        check(selector&&lite&&lite->getY()-selector->getBottom()<=20,"Audio Settings removes fixed empty gap");
        check(calibration&&panel.getLocalBounds().contains(calibration->getBounds()),"Audio Settings calibration button fits body");
        bool continued=false;panel.onSetup=[&]{continued=true;};if(calibration)calibration->onClick();check(continued,"calibration action survives resizing");
        bool requested=false;panel.onLite=[&](bool value){requested=value;return true;};auto* toggle=dynamic_cast<ToggleButton*>(lite);if(toggle){toggle->setToggleState(true,dontSendNotification);toggle->onClick();}check(requested,"NAM Lite action survives resizing");
    }
    {tonegrid::welcome::View view;exercise(view,"Welcome");check(view.getLocalBounds().contains(view.never.getBounds())&&view.getLocalBounds().contains(view.close.getBounds()),"welcome reflows with both actions reachable");}
    {tonegrid::ampui::AdvancedEqControls view;view.setAdvancedOpen(true);const auto state=view.state();exercise(view,"NAM EQ");check(view.state().bassDb==state.bassDb&&view.state().enabled==state.enabled,"EQ resizing keeps parameters");}
    {tonegrid::NamLibraryBrowser view;exercise(view,"Local NAM Library",440);auto* header=view.popupHeaderAction();check(header->getParentComponent()==&view,"library header reattaches after popup closes");}
    {tonegrid::calibration::GuitarWizard view(tonegrid::calibration::Settings{});exercise(view,"Guitar Calibration");}
    {
        tonegrid::tuning::Engine engine;AudioProcessorPlayer player;tonegrid::tuning::AudioTap tap(player,engine);
        tonegrid::tuning::Controls controls(engine,tap);tonegrid::tuning::Window window(controls,owner);
        window.setBoundsConstrained(window.getBounds().withSize(360,280));
        auto* close=controls.findChildWithID("tuner-close");
        check(window.isResizable()&&window.getWidth()==360&&window.getHeight()==280,"tuner resizes on small screens");
        check(close&&close->getWidth()==24&&controls.getLocalBounds().contains(close->getBounds()),"tuner Close retains fixed usable size");
        snapshot(controls,"Tuner-360");window.closeButtonPressed();
    }
    for(const auto area:std::initializer_list<juce::Rectangle<int>>{{0,0,1366,728},{0,0,853,440},{-1280,0,1280,680},{1920,0,1024,640}})
    {
        const auto fit=tonegrid::popup::fittedBounds(area,{1324,900},{area.getRight()+200,area.getBottom()+100});
        check(area.contains(fit)&&fit.getWidth()<=area.getWidth()*.91&&fit.getHeight()<=area.getHeight()*.91,"popup fits 90 percent work area "+area.toString());
    }
    report+=String(checks)+" responsive popup checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
