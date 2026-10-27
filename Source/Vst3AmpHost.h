#pragma once
#include "AmpSource.h"
#include "Vst3State.h"
#include "MonoVstBridge.h"

namespace amp
{
// Control facade for one native JUCE VST3 instance, not an AudioProcessor wrapper.
// Owns the instance until transfer; then retains its graph node. The graph keeps
// owning/processing the instance. unload drops this reference before graph.clear.
// Keep this facade (and its format manager) alive until the graph is destroyed.
class Vst3AmpHost final : public AmpSource
{
public:
    Vst3AmpHost(std::unique_ptr<juce::AudioPluginFormatManager> manager,
                const juce::PluginDescription& metadata,
                std::unique_ptr<juce::AudioPluginInstance> plugin,juce::String editorName,bool extendedState=false)
        : formats(std::move(manager)), description(metadata), pending(std::move(plugin)), original(pending.get()), displayName(std::move(editorName)),extended(extendedState) {}
    ~Vst3AmpHost() override { unload(); }

    SourceType type() const noexcept override { return SourceType::Vst3; }
    bool loaded() const noexcept override { return pending != nullptr || node != nullptr; }
    const juce::PluginDescription* pluginDescription() const noexcept override { return &description; }

    static juce::Result configureStereo(juce::AudioPluginInstance& instance, const juce::String& name)
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        auto layout=instance.getBusesLayout();
        for(auto& bus:layout.inputBuses)bus=juce::AudioChannelSet::disabled();
        for(auto& bus:layout.outputBuses)bus=juce::AudioChannelSet::disabled();
        if(layout.inputBuses.isEmpty()||layout.outputBuses.isEmpty())return juce::Result::fail(name+" has no audio bus.");
        for(const auto channels:{std::pair{2,2},std::pair{1,2},std::pair{1,1},std::pair{2,1}})
        {
            layout.inputBuses.getReference(0)=juce::AudioChannelSet::canonicalChannelSet(channels.first);
            layout.outputBuses.getReference(0)=juce::AudioChannelSet::canonicalChannelSet(channels.second);
            if(instance.setBusesLayout(layout))return juce::Result::ok();
        }
        return juce::Result::fail(name+" refused mono and stereo layouts.");
    }

    juce::AudioProcessorGraph::Node::Ptr addToGraph(juce::AudioProcessorGraph& graph) override
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        if(!pending||node||pending->getTotalNumInputChannels()<1||pending->getTotalNumInputChannels()>2||pending->getTotalNumOutputChannels()<1||pending->getTotalNumOutputChannels()>2)return nullptr;
        if(pending->getTotalNumInputChannels()==2&&pending->getTotalNumOutputChannels()==2)node=graph.addNode(std::move(pending));
        else node=graph.addNode(std::make_unique<MonoVstBridge>(std::move(pending)));
        return node;
    }
    void captureState(juce::MemoryBlock& state) override
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        if(auto* plugin=instance())vst3::captureState(*plugin,state,extended);
    }
    void restoreState(const juce::MemoryBlock& state) override
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        if(auto* plugin=instance())vst3::restoreState(*plugin,state);
    }
    juce::Result openEditor() override
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        auto* plugin=instance();if(!plugin)return juce::Result::fail("No amp source loaded.");
        if(!editor)
        {
            auto* view=plugin->createEditorIfNeeded();
            if(!view)return juce::Result::fail(displayName+" did not provide an editor.");
            editor=std::make_unique<EditorWindow>(displayName+" - Amp",view);
        }
        editor->setVisible(true);editor->toFront(true);return juce::Result::ok();
    }
    bool editorVisible() const noexcept override { return editor && editor->isVisible(); }
    void closeEditor() override
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        editor.reset();
    }
    void unload() override
    {
        closeEditor();pending.reset();node=nullptr;
    }

private:
    class EditorWindow final : public juce::DocumentWindow
    {
    public:
        EditorWindow(const juce::String& title,juce::AudioProcessorEditor* view)
            : DocumentWindow(title,juce::Colours::darkgrey,closeButton)
        {
            setUsingNativeTitleBar(true);setContentOwned(view,true);
            setResizable(view->isResizable(),false);centreWithSize(getWidth(),getHeight());
            if(const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds()))
            {
                const auto available=display->userArea.reduced(12);
                if(getWidth()>available.getWidth()||getHeight()>available.getHeight())
                {
                    if(view->isResizable())view->setSize(juce::jmin(view->getWidth(),available.getWidth()-16),juce::jmin(view->getHeight(),available.getHeight()-48));
                    else view->setScaleFactor(juce::jmin(1.f,juce::jmin(float(available.getWidth()-16)/view->getWidth(),float(available.getHeight()-48)/view->getHeight())));
                    centreWithSize(getContentComponent()->getBoundsInParent().getWidth(),getContentComponent()->getBoundsInParent().getHeight()+32);
                }
                setBounds(getBounds().constrainedWithin(display->userArea));
            }
            setVisible(true);
        }
        void closeButtonPressed() override { setVisible(false); } // Hide, retain instance/editor.
    };
    juce::AudioPluginInstance* instance() const noexcept
    {
        return loaded()?original:nullptr;
    }
    // Manager is destroyed last, after editor, node and any untransferred plugin.
    std::unique_ptr<juce::AudioPluginFormatManager> formats;
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> pending;
    juce::AudioPluginInstance* original=nullptr;
    const juce::String displayName;
    const bool extended;
    juce::AudioProcessorGraph::Node::Ptr node;
    std::unique_ptr<EditorWindow> editor;
};
}
