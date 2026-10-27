#pragma once
#include "RigSupport.h"
#include <cmath>

class TestEffect final : public juce::AudioProcessor
{
public:
    TestEffect(float multiplier, float addition)
        : AudioProcessor(BusesProperties().withInput("In", juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true)), gain(multiplier), offset(addition) {}
    const juce::String getName() const override { return "Test effect"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    { for (int c = 0; c < buffer.getNumChannels(); ++c) for (int s = 0; s < buffer.getNumSamples(); ++s) buffer.setSample(c, s, buffer.getSample(c, s) * gain + offset); }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    float gain, offset;
};
inline bool runSelfTests(juce::String& output)
{
    using namespace juce;
    int failures = 0, checks = 0;
    auto expect = [&](bool passed, const String& message) { ++checks; if (! passed) ++failures; output += String(passed ? "PASS " : "FAIL ") + message + "\n"; };
    rig::MidiEdges edges;
    expect(edges.accept({1, 1, 0, 0}).has_value(), "PC 0 accepted");
    expect(edges.accept({1, 16, 127, 0}).has_value(), "PC 127 channel 16 accepted");
    expect(! edges.accept({1, 0, 0, 0}), "Invalid MIDI channel rejected");
    expect(! edges.accept({2, 2, 42, 0}), "CC release does not recall");
    expect(edges.accept({2, 2, 42, 127}).has_value(), "CC press recalls");
    expect(! edges.accept({2, 2, 42, 127}), "Repeated high CC does not recall twice");
    expect(! edges.accept({2, 2, 42, 0}) && edges.accept({2, 2, 42, 64}).has_value(), "CC rearms on release");
    expect(edges.accept({2, 3, 42, 127}).has_value(), "CC channels independent");
    rig::Bindings bindings {};
    expect(rig::assign(bindings, 5, {1, 2, 17}), "Learn PC to slot 6");
    expect(! rig::assign(bindings, 6, {1, 2, 17}), "Duplicate assignment refused");
    expect(rig::matchingSlot(bindings, {1, 1, 17}) == -1, "Wrong channel does not match");
    expect(rig::assign(bindings, 7, {2, 2, 42}), "Learn CC independently");
    TemporaryFile mappingFile;
    expect(rig::writeXml(rig::encodeBindings(bindings), mappingFile.getFile()), "Assignments written to disk");
    auto mappingXml = XmlDocument::parse(mappingFile.getFile()); rig::Bindings restarted {};
    expect(mappingXml && rig::decodeBindings(*mappingXml, restarted) && rig::matchingSlot(restarted, {1, 2, 17}) == 5
        && rig::matchingSlot(restarted, {2, 2, 42}) == 7, "Assignments reload from disk into fresh bank");
    if (mappingXml) mappingXml->getFirstChildElement()->setAttribute("slot", 128);
    expect(mappingXml && ! rig::decodeBindings(*mappingXml, restarted), "Invalid slot rejected");
    PluginDescription description; description.name = "Test TONEX"; description.fileOrIdentifier = "test.vst3";
    description.uniqueId = 42; description.version = "1.0";
    rig::State original; original.name = "Klon + Clean";
    original.blobs[0].append("stomp-data", 10); original.blobs[1].append("amp-data", 8);
    TemporaryFile rigFile;
    expect(rig::writeXml(rig::encodeState(original, description), rigFile.getFile()), "Two plugin states written to one file");
    auto rigXml = XmlDocument::parse(rigFile.getFile()); rig::State restored;
    expect(rigXml && rig::decodeState(*rigXml, description, restored).wasOk() && restored.name == original.name
        && restored.blobs[0] == original.blobs[0] && restored.blobs[1] == original.blobs[1], "Both states and name survive disk round trip");
    auto other = description; other.version = "2.0";
    expect(rigXml && rig::decodeState(*rigXml, other, restored).failed(), "Version mismatch refused");
    if (rigXml) rigXml->getChildByName("Amp")->setAttribute("sha256", "invalid");
    expect(rigXml && rig::decodeState(*rigXml, description, restored).failed() && restored.blobs[0] == original.blobs[0]
        && restored.blobs[1] == original.blobs[1], "Corrupt amp rejected without partial decoded state");
    rig::StereoGraph graph; graph.setPlayConfigDetails(2, 2, 48000, 64);
    using IO = AudioProcessorGraph::AudioGraphIOProcessor;
    const auto input = graph.addNode(std::make_unique<IO>(IO::audioInputNode));
    const auto stomp = graph.addNode(std::make_unique<TestEffect>(2.0f, 0.0f));
    const auto amp = graph.addNode(std::make_unique<TestEffect>(1.0f, 3.0f));
    const auto dest = graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
    const bool connected = rig::connectStereoChain(graph, input->nodeID, stomp->nodeID, amp->nodeID, dest->nodeID);
    expect(connected, "Stereo serial graph connects");
    auto monoLayout = graph.getBusesLayout(); monoLayout.inputBuses.getReference(0) = AudioChannelSet::mono();
    expect(! graph.checkBusesLayoutSupported(monoLayout), "Graph stays stereo for JUCE mono duplication");
    graph.prepareToPlay(48000, 64); AudioBuffer<float> buffer(2, 64); MidiBuffer messages;
    for (int c = 0; c < 2; ++c) for (int s = 0; s < 64; ++s) buffer.setSample(c, s, static_cast<float>(c + 1));
    graph.processBlock(buffer, messages);
    bool orderCorrect = true;
    for (int c = 0; c < 2; ++c) for (int s = 0; s < 64; ++s)
        orderCorrect = orderCorrect && std::abs(buffer.getSample(c, s) - static_cast<float>((c + 1) * 2 + 3)) < 0.0001f;
    expect(orderCorrect, "Audio passes Stomp then Amp on both channels (not reversed or parallel)");
    graph.releaseResources();
    output += "\n" + String(checks) + " checks, " + String(failures) + " failures.\n";
    output += "These checks do not validate the actual TONEX models or the physical MIDI pedal.\n";
    return failures == 0;
}
