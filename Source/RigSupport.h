#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <array>
#include <optional>

namespace rig
{
using namespace juce;
constexpr int slotCount = 128;
// Keep two processing channels when the device supplies one guitar input.
// AudioProcessorPlayer then duplicates that mono input to both channels.
class StereoGraph final : public AudioProcessorGraph
{
public:
    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    { return layout.getMainInputChannelSet() == AudioChannelSet::stereo() && layout.getMainOutputChannelSet() == AudioChannelSet::stereo(); }
};
struct Binding
{
    int kind = 0; // 1 = Program Change, 2 = CC rising above 63
    int channel = 0, number = 0;
    bool valid() const { return (kind == 1 || kind == 2) && channel >= 1 && channel <= 16 && number >= 0 && number < 128; }
    bool operator==(const Binding& b) const { return kind == b.kind && channel == b.channel && number == b.number; }
    String text() const { return valid() ? String(kind == 1 ? "PC " : "CC ") + String(number) + " / channel " + String(channel) : "No MIDI assignment"; }
};
struct Event { int kind = 0, channel = 0, number = 0, value = 0; };
class MidiEdges
{
public:
    std::optional<Binding> accept(const Event& e)
    {
        Binding binding { e.kind, e.channel, e.number };
        if (! binding.valid()) return {};
        if (e.kind == 1) return binding;
        auto& wasHigh = high[static_cast<size_t>(e.channel - 1)][static_cast<size_t>(e.number)];
        const bool isHigh = e.value >= 64;
        const bool rising = isHigh && ! wasHigh;
        wasHigh = isHigh;
        return rising ? std::optional<Binding>(binding) : std::nullopt;
    }
private:
    std::array<std::array<bool, 128>, 16> high {};
};
using Bindings = std::array<Binding, slotCount>;
inline int matchingSlot(const Bindings& bindings, const Binding& binding)
{
    for (int i = 0; i < slotCount; ++i)
        if (bindings[static_cast<size_t>(i)].valid() && bindings[static_cast<size_t>(i)] == binding) return i;
    return -1;
}
inline bool assign(Bindings& bindings, int slot, const Binding& binding)
{
    if (slot < 0 || slot >= slotCount || ! binding.valid()) return false;
    const int other = matchingSlot(bindings, binding);
    if (other >= 0 && other != slot) return false;
    bindings[static_cast<size_t>(slot)] = binding;
    return true;
}
inline bool writeXml(const XmlElement& xml, const File& file)
{
    if (file.getParentDirectory().createDirectory().failed()) return false;
    TemporaryFile temporary(file);
    return xml.writeTo(temporary.getFile()) && temporary.overwriteTargetFileWithTemporary();
}
inline XmlElement encodeBindings(const Bindings& bindings)
{
    XmlElement root("MidiBindings");
    root.setAttribute("version", 1);
    for (int i = 0; i < slotCount; ++i)
    {
        const auto& b = bindings[static_cast<size_t>(i)];
        if (! b.valid()) continue;
        auto* item = root.createNewChildElement("Binding");
        item->setAttribute("slot", i);
        item->setAttribute("kind", b.kind);
        item->setAttribute("channel", b.channel);
        item->setAttribute("number", b.number);
    }
    return root;
}
inline bool decodeBindings(const XmlElement& root, Bindings& result)
{
    if (! root.hasTagName("MidiBindings") || root.getIntAttribute("version") != 1) return false;
    Bindings candidate {};
    for (auto* item : root.getChildIterator())
    {
        const int slot = item->getIntAttribute("slot", -1);
        Binding b { item->getIntAttribute("kind"), item->getIntAttribute("channel"), item->getIntAttribute("number", -1) };
        if (! item->hasTagName("Binding") || slot < 0 || slot >= slotCount
            || candidate[static_cast<size_t>(slot)].valid() || ! assign(candidate, slot, b)) return false;
    }
    result = candidate;
    return true;
}
struct State
{
    String name;
    std::array<MemoryBlock, 2> blobs;
};
inline XmlElement encodeState(const State& state, const PluginDescription& description)
{
    XmlElement root("TonexRig");
    root.setAttribute("version", 2);
    root.setAttribute("name", state.name);
    root.addChildElement(description.createXml().release());
    for (int i = 0; i < 2; ++i)
    {
        const auto& blob = state.blobs[static_cast<size_t>(i)];
        auto* child = root.createNewChildElement(i == 0 ? "Stomp" : "Amp");
        child->setAttribute("bytes", static_cast<int>(blob.getSize()));
        child->setAttribute("sha256", SHA256(blob).toHexString());
        child->addTextElement(blob.toBase64Encoding());
    }
    return root;
}
inline Result decodeState(const XmlElement& root, const PluginDescription& expected, State& result)
{
    if (! root.hasTagName("TonexRig") || root.getIntAttribute("version") != 2)
        return Result::fail("Not a two-instance rig preset (version 2 required).");
    PluginDescription saved;
    auto* identity = root.getChildByName("PLUGIN");
    if (! identity || ! saved.loadFromXml(*identity) || ! saved.isDuplicateOf(expected) || saved.version != expected.version)
        return Result::fail("TONEX identity, location or version differs from the saved rig.");
    State candidate;
    candidate.name = root.getStringAttribute("name");
    for (int i = 0; i < 2; ++i)
    {
        auto* child = root.getChildByName(i == 0 ? "Stomp" : "Amp");
        auto& blob = candidate.blobs[static_cast<size_t>(i)];
        if (! child || ! blob.fromBase64Encoding(child->getAllSubText()) || blob.isEmpty()
            || blob.getSize() > 45 * 1024 * 1024 || static_cast<int>(blob.getSize()) != child->getIntAttribute("bytes")
            || SHA256(blob).toHexString() != child->getStringAttribute("sha256"))
            return Result::fail("Missing or corrupt state for " + String(i == 0 ? "Stomp" : "Amp") + ". Neither instance was changed.");
    }
    result = std::move(candidate);
    return Result::ok();
}
inline bool connectStereoChain(AudioProcessorGraph& graph, AudioProcessorGraph::NodeID input,
                               AudioProcessorGraph::NodeID stomp, AudioProcessorGraph::NodeID amp,
                               AudioProcessorGraph::NodeID output)
{
    for (int channel = 0; channel < 2; ++channel)
        if (! graph.addConnection({ { input, channel }, { stomp, channel } })
            || ! graph.addConnection({ { stomp, channel }, { amp, channel } })
            || ! graph.addConnection({ { amp, channel }, { output, channel } })) return false;
    return true;
}
}
