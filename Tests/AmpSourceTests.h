#pragma once
#include "TonexAmpSource.h"
#include "SelfTests.h"

namespace ampTests
{
using namespace juce;
struct Trace
{
    int prepared=0,released=0,processed=0,captured=0,restored=0,editors=0,editorsDestroyed=0,pluginsDestroyed=0;
    bool pluginDestroyedAfterEditor=false;
};
// Synthetic instance, not a new supported plugin/backend.
class Plugin final : public AudioPluginInstance
{
public:
    Plugin(Trace& counters,int inputs=2,int outputs=2,bool editor=true,bool stereo=true)
        : AudioPluginInstance(buses(inputs,outputs)),trace(counters),withEditor(editor),allowStereo(stereo) {}
    ~Plugin() override {++trace.pluginsDestroyed;trace.pluginDestroyedAfterEditor=trace.editors==trace.editorsDestroyed;}
    const String getName() const override {return "Synthetic amp";}
    void fillInPluginDescription(PluginDescription& d) const override
    {d.name=getName();d.pluginFormatName="VST3";d.manufacturerName="Synthetic only";d.uniqueId=123;d.version="test";d.fileOrIdentifier="synthetic.vst3";}
    void prepareToPlay(double,int) override {++trace.prepared;}
    void releaseResources() override {++trace.released;}
    void processBlock(AudioBuffer<float>& buffer,MidiBuffer&) override
    {++trace.processed;for(int c=0;c<buffer.getNumChannels();++c)for(int i=0;i<buffer.getNumSamples();++i)buffer.setSample(c,i,buffer.getSample(c,i)*gain);}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {return allowStereo ? AudioProcessor::isBusesLayoutSupported(layout) : layout.getMainInputChannelSet()!=AudioChannelSet::stereo();}
    double getTailLengthSeconds() const override {return 0;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool hasEditor() const override {return withEditor;}
    AudioProcessorEditor* createEditor() override {return withEditor?new Editor(*this,trace):nullptr;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override {return {};}
    void changeProgramName(int,const String&) override {}
    void getStateInformation(MemoryBlock& state) override {++trace.captured;state.replaceAll(&gain,sizeof(gain));}
    void setStateInformation(const void* data,int bytes) override
    {++trace.restored;if(restoreFailuresRemaining>0){--restoreFailuresRemaining;gain=99;throw std::runtime_error("Synthetic restore failure");}if(bytes==sizeof(gain))std::memcpy(&gain,data,sizeof(gain));}
    float gain=3;
    int restoreFailuresRemaining=0;
private:
    static BusesProperties buses(int inputs,int outputs)
    {
        BusesProperties p;
        if(inputs>0)p=p.withInput("In",AudioChannelSet::canonicalChannelSet(inputs),true);
        if(outputs>0)p=p.withOutput("Out",AudioChannelSet::canonicalChannelSet(outputs),true);
        return p;
    }
    class Editor final : public AudioProcessorEditor
    {
    public:
        Editor(AudioProcessor& processor,Trace& stats):AudioProcessorEditor(processor),trace(stats){++trace.editors;setSize(240,100);}
        ~Editor() override {++trace.editorsDestroyed;}
    private:Trace& trace;
    };
    Trace& trace;
    bool withEditor,allowStereo;
};
inline bool run(String& report)
{
    bool ok=true;int checks=0;
    auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    {
        // No real folder is accessed: missing-path policy is exercised under temp.
        const auto absent=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-missing-amp-"+Uuid().toString()+".vst3");
        auto loaded=amp::loadTonex(absent);
        check(!loaded.source&&loaded.result.getErrorMessage()=="TONEX path does not exist. Use Open Amp to locate TONEX.vst3."
            &&loaded.statusMessage=="TONEX not found - use Open Amp to locate it.","Amp loading preserves missing TONEX path behavior");
    }
    {
        Trace t;Plugin noInput(t,0,2),noOutput(t,2,0);
        check(amp::Vst3AmpHost::configureStereo(noInput,"TONEX").getErrorMessage()=="TONEX has no audio bus.","Amp requires an input bus");
        check(amp::Vst3AmpHost::configureStereo(noOutput,"TONEX").getErrorMessage()=="TONEX has no audio bus.","Amp requires an output bus");
        Plugin refuses(t,1,1,true,false);
        check(amp::Vst3AmpHost::configureStereo(refuses,"TONEX").wasOk()&&refuses.getTotalNumInputChannels()==1,"Mono-input amp negotiates a supported layout");
    }
    Trace t;
    {
        auto plugin=std::make_unique<Plugin>(t);auto* original=plugin.get();
        const auto metadata=plugin->getPluginDescription();
        const auto expectedXml=metadata.createXml();
        amp::Vst3AmpHost source(nullptr,metadata,std::move(plugin),"TONEX");
        check(source.type()==amp::SourceType::Vst3&&source.loaded(),"Amp source owns staged VST3 instance before graph insertion");
        check(source.pluginDescription()->createXml()->isEquivalentTo(expectedXml.get(),false),"Amp metadata preserves format, manufacturer, IDs, version and location");
        MemoryBlock state;source.captureState(state);original->gain=7;source.restoreState(state);
        check(original->gain==3&&t.captured==1&&t.restored==1,"Amp state delegates to original instance before transfer");
        check(amp::Vst3AmpHost::configureStereo(*original,"TONEX").wasOk()
            &&original->getTotalNumInputChannels()==2&&original->getTotalNumOutputChannels()==2,"Amp stereo contract unchanged");
        check(source.openEditor().wasOk()&&source.editorVisible()&&original->getActiveEditor()!=nullptr,"Open Amp opens original processor editor");
        auto* view=original->getActiveEditor();
        auto* window=dynamic_cast<DocumentWindow*>(view?view->getTopLevelComponent():nullptr);
        check(window&&window->getName()=="TONEX - Amp"&&window->getContentComponent()==view,"Editor title and native content ownership preserved");
        if(window)window->closeButtonPressed();
        check(!source.editorVisible()&&original->getActiveEditor()==view&&t.editorsDestroyed==0,"Window close hides and retains plugin editor");
        check(source.openEditor().wasOk()&&source.editorVisible()&&t.editors==1,"Reopening Amp reuses same hidden editor");
        source.closeEditor();
        check(!source.editorVisible()&&original->getActiveEditor()==nullptr&&t.editorsDestroyed==1,"Rig restoration can close editor before state application");

        // Graph ownership/lifecycle remains native JUCE; no extra DSP wrapper.
        {
            rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,64);
            using IO=AudioProcessorGraph::AudioGraphIOProcessor;
            auto input=graph.addNode(std::make_unique<IO>(IO::audioInputNode));
            auto output=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
            // Distinct noncommutative transforms detect wrong amp position.
            auto pedals=graph.addNode(std::make_unique<TestEffect>(2.f,1.f));
            auto ampNode=source.addToGraph(graph);
            check(ampNode&&ampNode->getProcessor()==original&&graph.getNodes().size()==4,"Graph owns exact plugin instance without wrapper or pedal ID");
            check(!source.addToGraph(graph)&&graph.getNodes().size()==4,"Amp cannot be inserted twice");
            check(rig::connectStereoChain(graph,input->nodeID,pedals->nodeID,ampNode->nodeID,output->nodeID),"Amp is separate downstream node after entire pedal chain");
            MemoryBlock again;source.captureState(again);
            check(again==state,"State capture remains available after graph transfer");
            original->gain=9;source.restoreState(state);
            check(original->gain==3&&source.loaded(),"State restoration reaches graph-owned instance");
            check(source.openEditor().wasOk()&&original->getActiveEditor()!=nullptr&&t.editors==2,"Same plugin can reopen editor after transfer/restore");
            graph.prepareToPlay(48000,64);AudioBuffer<float> buffer(2,64);MidiBuffer midi;
            bool correct=true;
            for(int n=0;n<4;++n)
            {
                for(int c=0;c<2;++c)for(int i=0;i<64;++i)buffer.setSample(c,i,.1f*(c+1));
                graph.processBlock(buffer,midi);
                for(int c=0;c<2;++c)for(int i=0;i<64;++i)correct&=std::abs(buffer.getSample(c,i)-((.1f*(c+1)*2.f+1.f)*3.f))<.00001f;
            }
            check(correct&&t.processed==4&&t.prepared>0,"Graph prepares/processes native amp in correct serial order for both channels");
            graph.releaseResources();
            check(t.released>0,"Graph releaseResources still reaches native amp");
            source.unload();
            check(!source.loaded()&&!source.editorVisible()&&t.editorsDestroyed==2&&t.pluginsDestroyed==0,"Unload closes editor and drops source ownership while graph retains plugin");
            check(source.openEditor().failed(),"Unloaded source does not open stale plugin/editor");
            graph.clear();
        }
        check(t.pluginsDestroyed==1&&t.pluginDestroyedAfterEditor,"Graph destruction deletes plugin once, after editor destruction");
        source.unload();check(t.pluginsDestroyed==1,"Repeated source unload is safe");
    }
    check(t.pluginsDestroyed==1,"Source destructor does not double-delete transferred plugin");
    {
        Trace counters;auto plugin=std::make_unique<Plugin>(counters,2,2,false);
        const auto metadata=plugin->getPluginDescription();
        amp::Vst3AmpHost source(nullptr,metadata,std::move(plugin),"TONEX");
        check(source.openEditor().getErrorMessage()=="TONEX did not provide an editor."&&source.loaded(),"Missing editor keeps loaded amp with unchanged error message");
        source.unload();check(counters.pluginsDestroyed==1,"Untransferred plugin is deleted by source unload");
    }
    report+=String(checks)+" amp-source checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";
    return ok;
}
}
