#include "RigSupport.h"
#include "SelfTests.h"
#include <atomic>
using namespace juce;
class RigEditor final : public DocumentWindow
{
public:
    RigEditor(const String& title, AudioProcessorEditor* view) : DocumentWindow(title, Colours::darkgrey, closeButton)
    {
        setUsingNativeTitleBar(true); setContentOwned(view, true);
        setResizable(view->isResizable(), false); centreWithSize(getWidth(), getHeight()); setVisible(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};
class RigHost final : public Component, private MidiInputCallback, private Timer
{
public:
    RigHost()
    {
        formats.addFormat(new VST3PluginFormat());
        path.setText("C:\\Program Files\\Common Files\\VST3\\TONEX.vst3", false);
        title.setText("Guitar  >  TONEX Stomp  >  TONEX Amp  >  Output", dontSendNotification);
        name.setTextToShowWhenEmpty("Preset name (e.g. Klon + clean amp)", Colours::grey);
        log.setMultiLine(true); log.setReadOnly(true);
        for (Component* c : std::initializer_list<Component*> { &path, &title, &load, &stompGui, &ampGui, &audio, &listen,
            &slot, &name, &save, &recall, &midiDevice, &rescan, &learn, &clear, &assignment, &active, &log }) addAndMakeVisible(c);
        load.onClick = [this] { loadRig(); };
        stompGui.onClick = [this] { showEditor(0); }; ampGui.onClick = [this] { showEditor(1); };
        audio.onClick = [this] { audioSettings(); };
        listen.setClickingTogglesState(true);
        listen.onClick = [this] { if (listen.getToggleState() && ! initialiseAudio()) listen.setToggleState(false, dontSendNotification); attach(); };
        slot.onChange = [this] { learningSlot = -1; selectSlot(); };
        save.onClick = [this] { saveSlot(); }; recall.onClick = [this] { recallSlot(selectedSlot()); };
        rescan.onClick = [this] { scanMidi(); }; midiDevice.onChange = [this] { openMidi(); };
        learn.onClick = [this]
        {
            learningSlot = learningSlot >= 0 ? -1 : selectedSlot();
            if (learningSlot >= 0) report("MIDI Learn: press a pedal button (PC or CC >= 64).");
            refresh();
        };
        clear.onClick = [this] { auto next = bindings; next[static_cast<size_t>(selectedSlot())] = {}; if (storeBindings(next)) { learningSlot = -1; refresh(); } };
        if (auto xml = XmlDocument::parse(bankDir.getChildFile("midi-bindings.xml")))
            if (! rig::decodeBindings(*xml, bindings)) report("Invalid MIDI assignments file; assignments not loaded.");
        reloadSlots(0); scanMidi(); refresh();
        active.setText("No rig recalled yet. Listen is OFF.", dontSendNotification);
        startTimerHz(30); setSize(1000, 530);
        report("Load both instances, configure Stomp and Amp, name a slot and Save Rig. Then use MIDI Learn.");
    }
    ~RigHost() override
    {
        stopTimer(); if (midi) midi->stop(); midi.reset(); delete audioDialog.getComponent();
        devices.removeAudioCallback(&player); player.setProcessor(nullptr);
        for (auto& editor : editors) editor.reset();
        graph.clear(); devices.closeAudioDevice();
    }
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12);
        title.setBounds(bounds.removeFromTop(28)); path.setBounds(bounds.removeFromTop(28)); bounds.removeFromTop(8);
        auto row = bounds.removeFromTop(34);
        for (auto* b : { &load, &stompGui, &ampGui, &audio, &listen }) b->setBounds(row.removeFromLeft(194).reduced(2, 0));
        bounds.removeFromTop(12); row = bounds.removeFromTop(32);
        slot.setBounds(row.removeFromLeft(245).reduced(2, 0)); name.setBounds(row.removeFromLeft(335).reduced(2, 0));
        save.setBounds(row.removeFromLeft(194).reduced(2, 0)); recall.setBounds(row.reduced(2, 0));
        active.setBounds(bounds.removeFromTop(28)); bounds.removeFromTop(8); row = bounds.removeFromTop(32);
        midiDevice.setBounds(row.removeFromLeft(390).reduced(2, 0));
        for (auto* b : { &rescan, &learn, &clear }) b->setBounds(row.removeFromLeft(194).reduced(2, 0));
        assignment.setBounds(bounds.removeFromTop(28)); bounds.removeFromTop(8); log.setBounds(bounds);
    }
private:
    AudioPluginFormatManager formats;
    AudioDeviceManager devices;
    rig::StereoGraph graph;
    AudioProcessorPlayer player;
    std::array<AudioPluginInstance*, 2> plugins {};
    std::array<std::unique_ptr<RigEditor>, 2> editors;
    PluginDescription description;
    Component::SafePointer<DialogWindow> audioDialog;
    std::unique_ptr<MidiInput> midi;
    Array<MidiDeviceInfo> midiDevices;
    AbstractFifo fifo { 256 };
    std::array<rig::Event, 256> events {};
    std::atomic<bool> overflow { false };
    rig::MidiEdges edges;
    rig::Bindings bindings {};
    int learningSlot = -1;
    bool audioReady = false;
    Label title, assignment, active;
    TextEditor path, name, log;
    ComboBox slot, midiDevice;
    TextButton load { "Load Stomp + Amp" }, stompGui { "Open Stomp" }, ampGui { "Open Amp" },
        audio { "Audio settings" }, listen { "Listen" }, save { "Save Rig" }, recall { "Recall Rig" },
        rescan { "Refresh MIDI inputs" }, learn { "MIDI Learn" }, clear { "Clear assignment" };
    const File bankDir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("ToneGrid-Legacy").getChildFile("Rigs-v2");
    String lastAudioState;
    bool ready() const { return plugins[0] && plugins[1]; }
    int selectedSlot() const { return jmax(0, slot.getSelectedId() - 1); }
    File slotFile(int index) const { return bankDir.getChildFile("slot-" + String(index + 1).paddedLeft('0', 3) + ".tonexrig"); }
    File nameFile(int index) const { return bankDir.getChildFile("slot-" + String(index + 1) + ".name"); }
    void report(const String& text)
    {
        const auto line = Time::getCurrentTime().toString(true, true) + "  " + text + "\n";
        log.moveCaretToEnd(); log.insertTextAtCaret(line);
        if (bankDir.createDirectory().wasOk()) bankDir.getChildFile("rig.log").appendText(line);
    }
    void refresh()
    {
        load.setEnabled(! ready()); path.setEnabled(! ready()); stompGui.setEnabled(ready()); ampGui.setEnabled(ready());
        listen.setEnabled(ready()); save.setEnabled(ready()); const bool saved = slotFile(selectedSlot()).existsAsFile();
        recall.setEnabled(ready() && saved); learn.setEnabled(midi != nullptr && ready() && saved);
        learn.setButtonText(learningSlot >= 0 ? "Cancel MIDI Learn" : "MIDI Learn");
        clear.setEnabled(bindings[static_cast<size_t>(selectedSlot())].valid());
        assignment.setText(learningSlot >= 0 ? "Waiting for a pedal button for slot " + String(learningSlot + 1)
            : bindings[static_cast<size_t>(selectedSlot())].text(), dontSendNotification);
    }
    void reloadSlots(int selected)
    {
        slot.clear(dontSendNotification);
        for (int i = 0; i < rig::slotCount; ++i)
        {
            String label = "(empty)";
            if (slotFile(i).existsAsFile()) { label = nameFile(i).loadFileAsString().trim(); if (label.isEmpty()) label = "(saved)"; }
            slot.addItem(String(i + 1).paddedLeft('0', 3) + " - " + label, i + 1);
        }
        slot.setSelectedId(selected + 1, dontSendNotification); selectSlot();
    }
    void selectSlot() { name.setText(nameFile(selectedSlot()).loadFileAsString(), false); refresh(); }
    void loadRig()
    {
        if (ready()) return;
        OwnedArray<PluginDescription> types; const File file(path.getText().unquoted());
        if (! file.exists()) { report("TONEX path does not exist."); return; }
        formats.getFormat(0)->findAllTypesForFile(types, file.getFullPathName());
        PluginDescription* found = nullptr;
        for (auto* candidate : types) if (candidate->name.containsIgnoreCase("TONEX")) { found = candidate; break; }
        if (! found) { report("No TONEX VST3 found."); return; }
        description = *found; std::array<std::unique_ptr<AudioPluginInstance>, 2> instances;
        for (auto& instance : instances)
        {
            String error; instance = formats.createPluginInstance(description, 48000.0, 256, error);
            if (! instance) { report("Load failed: " + error); return; }
            auto layout = instance->getBusesLayout();
            for (auto& bus : layout.inputBuses) bus = AudioChannelSet::disabled();
            for (auto& bus : layout.outputBuses) bus = AudioChannelSet::disabled();
            if (layout.inputBuses.isEmpty() || layout.outputBuses.isEmpty()) { report("TONEX has no audio bus."); return; }
            layout.inputBuses.getReference(0) = AudioChannelSet::stereo(); layout.outputBuses.getReference(0) = AudioChannelSet::stereo();
            if (! instance->setBusesLayout(layout)) { report("TONEX refused stereo input/output layout."); return; }
        }
        graph.setPlayConfigDetails(2, 2, 48000.0, 256); using IO = AudioProcessorGraph::AudioGraphIOProcessor;
        const auto input = graph.addNode(std::make_unique<IO>(IO::audioInputNode));
        const auto output = graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
        plugins = { instances[0].get(), instances[1].get() };
        const auto stomp = graph.addNode(std::move(instances[0])); const auto amp = graph.addNode(std::move(instances[1]));
        if (! input || ! output || ! stomp || ! amp || ! rig::connectStereoChain(graph, input->nodeID, stomp->nodeID, amp->nodeID, output->nodeID))
        { plugins = {}; graph.clear(); report("Could not connect the serial audio chain."); return; }
        refresh(); report("Loaded two TONEX instances " + description.version + ": input -> Stomp -> Amp -> output."); showEditor(0);
    }
    void showEditor(int index)
    {
        if (! ready()) return; auto& editor = editors[static_cast<size_t>(index)];
        if (! editor)
        {
            auto* view = plugins[static_cast<size_t>(index)]->createEditorIfNeeded();
            if (! view) { report("TONEX did not provide an editor."); return; }
            editor = std::make_unique<RigEditor>(index == 0 ? "TONEX 1 - STOMP" : "TONEX 2 - AMP", view);
        }
        editor->setVisible(true); editor->toFront(true);
    }
    bool initialiseAudio()
    {
        if (audioReady) return true; auto xml = XmlDocument::parse(bankDir.getChildFile("audio.xml"));
        const auto error = devices.initialise(2, 2, xml.get(), true);
        if (error.isNotEmpty()) { report("Audio error: " + error); return false; }
        devices.addAudioCallback(&player); audioReady = true; return true;
    }
    void attach() { player.setProcessor(audioReady && listen.getToggleState() && ready() ? &graph : nullptr); }
    void audioSettings()
    {
        if (audioDialog) { audioDialog->toFront(true); return; } if (! initialiseAudio()) return;
        DialogWindow::LaunchOptions options; options.dialogTitle = "Audio settings - ASIO / Windows audio";
        options.dialogBackgroundColour = Colours::darkgrey; options.useNativeTitleBar = true;
        auto* selector = new AudioDeviceSelectorComponent(devices, 1, 2, 2, 2, false, false, false, false);
        selector->setSize(560, 440); options.content.setOwned(selector); audioDialog = options.launchAsync();
    }
    void saveSlot()
    {
        if (! ready()) return; const int index = selectedSlot(); rig::State state; state.name = name.getText().trim();
        if (state.name.isEmpty()) { report("Enter a preset name before saving."); return; }
        player.setProcessor(nullptr);
        for (int i = 0; i < 2; ++i) plugins[static_cast<size_t>(i)]->getStateInformation(state.blobs[static_cast<size_t>(i)]);
        attach();
        for (const auto& blob : state.blobs) if (blob.isEmpty() || blob.getSize() > 45 * 1024 * 1024) { report("Save failed: empty or oversized plugin state."); return; }
        if (! rig::writeXml(rig::encodeState(state, description), slotFile(index))) { report("Could not save rig."); return; }
        if (! nameFile(index).replaceWithText(state.name)) report("Rig saved, but its name index could not be written.");
        active.setText("Saved slot " + String(index + 1) + ": " + state.name, dontSendNotification);
        reloadSlots(index); report("Saved both instances to slot " + String(index + 1) + ". Save Rig replaces the selected slot.");
    }
    void recallSlot(int index)
    {
        if (! ready() || index < 0 || index >= rig::slotCount) return; const File file = slotFile(index);
        if (file.getSize() <= 0 || file.getSize() > 128 * 1024 * 1024) { report("Rig missing or larger than 128 MiB."); return; }
        auto xml = XmlDocument::parse(file); if (! xml) { report("Invalid rig XML."); return; }
        rig::State state; const auto result = rig::decodeState(*xml, description, state);
        if (result.failed()) { report(result.getErrorMessage()); return; }
        player.setProcessor(nullptr); std::array<bool, 2> visible {};
        for (int i = 0; i < 2; ++i) { const auto n = static_cast<size_t>(i); visible[n] = editors[n] && editors[n]->isVisible(); editors[n].reset(); }
        for (int i = 0; i < 2; ++i) { const auto n = static_cast<size_t>(i); plugins[n]->setStateInformation(state.blobs[n].getData(), static_cast<int>(state.blobs[n].getSize())); }
        attach(); for (int i = 0; i < 2; ++i) if (visible[static_cast<size_t>(i)]) showEditor(i);
        learningSlot = -1; slot.setSelectedId(index + 1, dontSendNotification); name.setText(state.name, false);
        active.setText("Recalled slot " + String(index + 1) + ": " + state.name, dontSendNotification);
        refresh(); report("Restored Stomp + Amp from slot " + String(index + 1) + ": " + state.name);
    }
    bool storeBindings(const rig::Bindings& next)
    {
        if (! rig::writeXml(rig::encodeBindings(next), bankDir.getChildFile("midi-bindings.xml"))) { report("Could not save MIDI assignments."); return false; }
        bindings = next; return true;
    }
    void scanMidi()
    {
        const auto previous = midi ? midi->getIdentifier() : bankDir.getChildFile("midi-input.txt").loadFileAsString();
        if (midi) midi->stop(); midi.reset(); fifo.reset(); edges = {}; learningSlot = -1;
        midiDevices = MidiInput::getAvailableDevices(); midiDevice.clear(dontSendNotification); midiDevice.addItem("MIDI input: Off", 1);
        int selected = 1;
        for (int i = 0; i < midiDevices.size(); ++i) { midiDevice.addItem(midiDevices[i].name, i + 2); if (midiDevices[i].identifier == previous) selected = i + 2; }
        midiDevice.setSelectedId(selected, dontSendNotification); openMidi(false);
    }
    void openMidi(bool persistSelection = true)
    {
        if (midi) midi->stop(); midi.reset(); fifo.reset(); edges = {}; learningSlot = -1;
        const int index = midiDevice.getSelectedId() - 2;
        if (isPositiveAndBelow(index, midiDevices.size()))
        {
            midi = MidiInput::openDevice(midiDevices[index].identifier, this);
            if (midi) { midi->start(); report("MIDI input: " + midiDevices[index].name); }
            else report("Could not open MIDI input. Check other apps and refresh the list.");
        }
        // Keep the selected identifier even if the controller is temporarily unavailable.
        if (persistSelection && bankDir.createDirectory().wasOk())
            bankDir.getChildFile("midi-input.txt").replaceWithText(isPositiveAndBelow(index, midiDevices.size()) ? midiDevices[index].identifier : String());
        refresh();
    }
    void handleIncomingMidiMessage(MidiInput*, const MidiMessage& message) override
    {
        rig::Event event;
        if (message.isProgramChange()) event = { 1, message.getChannel(), message.getProgramChangeNumber(), 0 };
        else if (message.isController()) event = { 2, message.getChannel(), message.getControllerNumber(), message.getControllerValue() };
        else return;
        int start1, size1, start2, size2; fifo.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0) { events[static_cast<size_t>(start1)] = event; fifo.finishedWrite(1); } else overflow.store(true);
    }
    void timerCallback() override
    {
        if (overflow.exchange(false)) report("MIDI queue full: some messages were dropped.");
        int pendingRecall = -1;
        for (int count = 0; count < 255; ++count)
        {
            int start1, size1, start2, size2; fifo.prepareToRead(1, start1, size1, start2, size2); if (size1 == 0) break;
            const auto event = events[static_cast<size_t>(start1)]; fifo.finishedRead(1); const auto binding = edges.accept(event); if (! binding) continue;
            if (learningSlot >= 0)
            {
                auto next = bindings;
                if (! rig::assign(next, learningSlot, *binding)) report("This MIDI button belongs to another slot. Clear its assignment first.");
                else if (storeBindings(next)) { report("Learned " + binding->text() + " for slot " + String(learningSlot + 1)); learningSlot = -1; refresh(); }
                pendingRecall = -1; break;
            }
            const int mapped = rig::matchingSlot(bindings, *binding); if (mapped >= 0) pendingRecall = mapped;
        }
        // Restoring plugin state runs on the message thread; coalesce bursts to the last mapped slot.
        if (pendingRecall >= 0) recallSlot(pendingRecall);
        if (audioReady && ! audioDialog)
            if (auto xml = devices.createStateXml())
            { const String current = xml->toString(); if (current != lastAudioState && rig::writeXml(*xml, bankDir.getChildFile("audio.xml"))) lastAudioState = current; }
    }
};
class MainWindow final : public DocumentWindow
{
public:
    MainWindow() : DocumentWindow("TONEX Rig 0.2.0 - Stomp + Amp / MIDI", Colours::darkgrey, allButtons)
    { setUsingNativeTitleBar(true); setContentOwned(new RigHost(), true); centreWithSize(getWidth(), getHeight()); setVisible(true); }
    void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
};
class Application final : public JUCEApplication
{
public:
    const String getApplicationName() override { return "TONEX State Probe"; }
    const String getApplicationVersion() override { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override { const auto args = getCommandLineParameters().trim(); return args == "--self-test" || args == "--diagnose-audio"; }
    void initialise(const String& commandLine) override
    {
        if (commandLine.trim() == "--self-test")
        {
            String results; const bool passed = runSelfTests(results);
            const bool written = File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("self-tests.txt").replaceWithText(results);
            setApplicationReturnValue(passed && written ? 0 : 1); quit(); return;
        }
        if (commandLine.trim() == "--diagnose-audio")
        {
            AudioDeviceManager manager; String report = "TONEX Rig 0.2.0 - audio and MIDI enumeration\n";
            for (auto* type : manager.getAvailableDeviceTypes()) { type->scanForDevices(); report += "\n[" + type->getTypeName() + "]\n" + type->getDeviceNames().joinIntoString(", ") + "\n"; }
            report += "\n[MIDI inputs]\n"; for (const auto& device : MidiInput::getAvailableDevices()) report += device.name + "\n";
            setApplicationReturnValue(File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("audio-devices.txt").replaceWithText(report) ? 0 : 1);
            quit(); return;
        }
        window = std::make_unique<MainWindow>();
    }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<MainWindow> window;
};
START_JUCE_APPLICATION(Application)
