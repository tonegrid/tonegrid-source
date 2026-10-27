#pragma once
#include "Vst3Catalog.h"
#include "PluginLocations.h"
#include <atomic>

namespace vst3
{
struct ScanProgress {std::atomic<int> completed{0},total{0};std::atomic<bool> cancel{false};};
struct ScanResult {Catalog catalog;juce::Result result=juce::Result::ok();};

// Executed on the child application's JUCE message thread. No RigHost/devices.
inline bool scanWorker(const juce::File& plugin,const juce::File& destination)
{
    using namespace juce;Catalog catalog;
    try
    {
        VST3PluginFormat format;OwnedArray<PluginDescription> found;
        format.findAllTypesForFile(found,plugin.getFullPathName());
        for(auto* description:found)catalog.add(*description);
        if(catalog.plugins.empty())catalog.failures.push_back({plugin.getFullPathName(),"No VST3 classes discovered."});
    }
    catch(const std::exception& error){catalog.failures.push_back({plugin.getFullPathName(),String(error.what())});}
    catch(...){catalog.failures.push_back({plugin.getFullPathName(),"Plugin discovery threw an exception."});}
    return rig::writeXml(catalog.xml(),destination);
}

// Coordinator on background thread. Each bundle is discovered in a fresh process;
// crash/timeout cannot destroy the application. No plugin is created in this thread.
inline ScanResult scan(const juce::File& workerExe,const juce::FileSearchPath& paths,
                       ScanProgress& progress,int timeoutMs=30000)
{
    using namespace juce;ScanResult output;
    const auto tempParent=File::getSpecialLocation(File::tempDirectory);
    const auto root=tempParent.getChildFile("ToneGrid-vst3-scan-"+Uuid().toString());
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(tempParent))root.deleteRecursively();}};
    if(root.createDirectory().failed()){output.result=Result::fail("Cannot create VST3 scan temporary directory.");return output;}
    VST3PluginFormat format;auto files=format.searchPathsForPlugins(paths,true,false);files.removeDuplicates(false);files.sort(true);
    progress.total.store(files.size());
    for(int i=0;i<files.size();++i)
    {
        if(progress.cancel.load()){output.result=Result::fail("VST3 scan cancelled.");return output;}
        const auto resultFile=root.getChildFile(String(i)+".xml");ChildProcess child;
        if(!child.start(StringArray{workerExe.getFullPathName(),"--scan-vst3-worker",files[i],resultFile.getFullPathName()},0))
            output.catalog.failures.push_back({files[i],"Cannot launch scan worker."});
        else
        {
            const auto start=Time::getMillisecondCounterHiRes();bool aborted=false;
            while(child.isRunning())
            {
                if(progress.cancel.load()||Time::getMillisecondCounterHiRes()-start>timeoutMs){child.kill();child.waitForProcessToFinish(2000);aborted=true;break;}
                Thread::sleep(20);
            }
            Catalog discovered;const auto parsed=CacheStore(resultFile).read(discovered);
            if(aborted||child.getExitCode()!=0||parsed.failed())
                output.catalog.failures.push_back({files[i],aborted?"Scan timed out/cancelled.":"Scan worker failed/crashed or returned invalid metadata."});
            else {for(const auto& d:discovered.plugins)output.catalog.add(d);for(const auto& f:discovered.failures)output.catalog.failures.push_back(f);}
        }
        progress.completed.store(i+1);
    }
    if(progress.cancel.load()){output.result=Result::fail("VST3 scan cancelled.");return output;}
    if(!files.isEmpty()&&output.catalog.plugins.empty()&&!output.catalog.failures.empty())output.result=Result::fail("All VST3 scan workers failed; previous cache must be kept.");
    output.catalog.sort();return output;
}
}
