#pragma once
#include "Vst3Loader.h"
namespace vst3
{
struct ValidationResult{juce::PluginDescription description;juce::Result result=juce::Result::ok();int recall=-1;};
inline bool validationWorker(const juce::File& request,const juce::File& destination)
{
    using namespace juce;auto xml=XmlDocument::parse(request);PluginDescription description;
    if(!xml||!xml->getChildByName("PLUGIN")||!description.loadFromXml(*xml->getChildByName("PLUGIN")))return false;
    MemoryBlock state;const MemoryBlock* saved=nullptr;if(auto* data=xml->getChildByName("State")){if(!state.fromBase64Encoding(data->getAllSubText()))return false;saved=&state;}
    auto loaded=load(description,saved);XmlElement result("VstValidation");result.setAttribute("ok",loaded.result.wasOk());result.setAttribute("message",loaded.result.getErrorMessage());
    return rig::writeXml(result,destination);
}
inline ValidationResult validate(const juce::File& executable,juce::PluginDescription description,const juce::MemoryBlock& state,int recall,const std::atomic<bool>& cancel,int timeoutMs=30000)
{
    using namespace juce;ValidationResult out{description,Result::ok(),recall};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-vst-validation-"+Uuid().toString());
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(File::getSpecialLocation(File::tempDirectory)))root.deleteRecursively();}};
    XmlElement request("VstValidation");request.addChildElement(description.createXml().release());if(!state.isEmpty())request.createNewChildElement("State")->addTextElement(state.toBase64Encoding());
    if(!rig::writeXml(request,root.getChildFile("request.xml"))){out.result=Result::fail("Cannot create the plugin validation folder.");return out;}
    ChildProcess child;if(!child.start(StringArray{executable.getFullPathName(),"--validate-vst3-worker",root.getChildFile("request.xml").getFullPathName(),root.getChildFile("result.xml").getFullPathName()},0)){out.result=Result::fail("Cannot start plugin validation. Your current sound is kept.");return out;}
    const auto start=Time::getMillisecondCounterHiRes();
    while(child.isRunning())
    {
        if(cancel.load()||Time::getMillisecondCounterHiRes()-start>timeoutMs){child.kill();child.waitForProcessToFinish(2000);out.result=Result::fail("Plugin validation timed out or was cancelled. Open the plugin's standalone app to check activation, then retry.");return out;}
        Thread::sleep(20);
    }
    auto result=XmlDocument::parse(root.getChildFile("result.xml"));
    if(child.getExitCode()!=0||!result||!result->hasTagName("VstValidation"))out.result=Result::fail("The plugin crashed during validation. Your current sound is kept. Check the plugin's installation or activation.");
    else if(!result->getBoolAttribute("ok"))out.result=Result::fail(result->getStringAttribute("message","Plugin validation failed."));
    return out;
}
}
