#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace amp
{
enum class SourceType { Vst3, Nam }; // Explicit source mode; never a pedal/dropdown ID.

// One downstream stage, independent of pedal identities and positions.
// All control/lifecycle methods run on the message thread. The graph prepares,
// processes and releases the returned native processor directly, as before 3A.
// Before unload: detach the audio player and close the editor; then clear graph.
class AmpSource
{
public:
    virtual ~AmpSource() = default;
    virtual SourceType type() const noexcept = 0;
    virtual bool loaded() const noexcept = 0;
    virtual juce::AudioProcessorGraph::Node::Ptr addToGraph(juce::AudioProcessorGraph&) = 0;
    virtual void captureState(juce::MemoryBlock&) = 0;
    virtual void restoreState(const juce::MemoryBlock&) = 0;
    virtual juce::Result openEditor() = 0;
    virtual bool editorVisible() const noexcept = 0;
    virtual void closeEditor() = 0;
    virtual void unload() = 0;

    // Legacy v3-v6 persistence still requires the original JUCE description.
    // Native NAM returns nullptr; its typed identity/settings are persisted
    // separately. It must not pretend to be a VST3 plugin.
    virtual const juce::PluginDescription* pluginDescription() const noexcept = 0;
};
}
