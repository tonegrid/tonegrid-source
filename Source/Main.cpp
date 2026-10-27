#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <limits>

using namespace juce;

class EditorWindow final : public DocumentWindow
{
public:
    explicit EditorWindow(AudioProcessorEditor* editor)
        : DocumentWindow("TONEX", Colours::darkgrey, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};

class Probe final : public Component
{
public:
    Probe()
    {
        formats.addFormat(new VST3PluginFormat());
        path.setText("C:\\Program Files\\Common Files\\VST3\\TONEX.vst3", false);
        addAndMakeVisible(path);
        addAndMakeVisible(log);
        log.setMultiLine(true);
        log.setReadOnly(true);
        for (auto* button : { &load, &gui, &save, &restore, &audio, &listen })
            addAndMakeVisible(button);
        load.onClick = [this] { loadPlugin(); };
        gui.onClick = [this] { showEditor(); };
        save.onClick = [this] { chooseState(true); };
        restore.onClick = [this] { chooseState(false); };
        audio.onClick = [this] { showAudioSettings(); };
        listen.setClickingTogglesState(true);
        listen.onClick = [this]
        {
            if (listen.getToggleState() && ! initialiseAudio())
                listen.setToggleState(false, dontSendNotification);
            attachAudio();
        };
        refresh();
        report("Ready. Load TONEX, select a model, then Save State. Audio is initially OFF.");
        setSize(820, 420);
    }

    ~Probe() override
    {
        chooser.reset();
        delete audioDialog.getComponent();
        devices.removeAudioCallback(&player);
        player.setProcessor(nullptr);
        editor.reset();
        plugin.reset();
        devices.closeAudioDevice();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        path.setBounds(area.removeFromTop(30));
        area.removeFromTop(10);
        auto row = area.removeFromTop(32);
        for (auto* button : { &load, &gui, &save, &restore, &audio, &listen })
            button->setBounds(row.removeFromLeft(132).reduced(2, 0));
        area.removeFromTop(12);
        log.setBounds(area);
    }

private:
    AudioPluginFormatManager formats;
    AudioDeviceManager devices;
    AudioProcessorPlayer player;
    std::unique_ptr<AudioPluginInstance> plugin;
    std::unique_ptr<EditorWindow> editor;
    std::unique_ptr<FileChooser> chooser;
    Component::SafePointer<DialogWindow> audioDialog;
    PluginDescription description;
    TextEditor path, log;
    TextButton load { "Load TONEX" }, gui { "Open GUI" }, save { "Save State" },
        restore { "Restore State" }, audio { "Audio settings" }, listen { "Listen" };
    bool audioReady = false;
    const File dataDir = File::getSpecialLocation(File::userApplicationDataDirectory)
                             .getChildFile("ToneGrid-Legacy");

    void report(const String& message)
    {
        const auto line = Time::getCurrentTime().toString(true, true) + "  " + message + "\n";
        log.moveCaretToEnd();
        log.insertTextAtCaret(line);
        if (dataDir.createDirectory().wasOk())
            dataDir.getChildFile("probe.log").appendText(line);
    }

    void refresh()
    {
        const bool ready = plugin != nullptr;
        load.setEnabled(! ready);
        path.setEnabled(! ready);
        gui.setEnabled(ready);
        save.setEnabled(ready && ! chooser);
        restore.setEnabled(ready && ! chooser);
        listen.setEnabled(ready);
    }

    void loadPlugin()
    {
        const File file(path.getText().unquoted());
        if (! file.exists()) { report("Plugin path does not exist."); return; }
        OwnedArray<PluginDescription> types;
        report("Scanning " + file.getFullPathName());
        formats.getFormat(0)->findAllTypesForFile(types, file.getFullPathName());
        PluginDescription* found = nullptr;
        for (auto* type : types)
            if (type->name.containsIgnoreCase("TONEX")) { found = type; break; }
        if (found == nullptr) { report("No TONEX VST3 class found in this path."); return; }
        description = *found;
        String error;
        plugin = formats.createPluginInstance(description, 48000.0, 256, error);
        if (! plugin) { report("Load failed: " + error); return; }
        plugin->enableAllBuses();
        report("Loaded " + description.name + " " + description.version
               + " | " + description.manufacturerName);
        refresh();
        showEditor();
    }

    void showEditor()
    {
        if (! plugin) return;
        if (! editor)
        {
            auto* view = plugin->createEditorIfNeeded();
            if (view == nullptr) { report("Plugin did not provide a GUI."); return; }
            editor = std::make_unique<EditorWindow>(view);
        }
        editor->setVisible(true);
        editor->toFront(true);
    }

    bool initialiseAudio()
    {
        if (audioReady) return true;
        const auto error = devices.initialiseWithDefaultDevices(2, 2);
        if (error.isNotEmpty()) { report("Audio error: " + error); return false; }
        devices.addAudioCallback(&player);
        audioReady = true;
        report("Audio opened. Select your guitar input and outputs in Audio settings.");
        return true;
    }

    void attachAudio()
    {
        player.setProcessor(audioReady && listen.getToggleState() ? plugin.get() : nullptr);
    }

    void showAudioSettings()
    {
        if (audioDialog != nullptr) { audioDialog->toFront(true); return; }
        if (! initialiseAudio()) return;
        DialogWindow::LaunchOptions options;
        options.dialogTitle = "Audio settings (ASIO / Windows audio)";
        options.dialogBackgroundColour = Colours::darkgrey;
        options.useNativeTitleBar = true;
        options.resizable = false;
        auto* selector = new AudioDeviceSelectorComponent(devices, 1, 2, 2, 2, false, false, false, false);
        selector->setSize(560, 440);
        options.content.setOwned(selector);
        audioDialog = options.launchAsync();
    }

    void chooseState(bool writing)
    {
        if (! plugin || chooser) return;
        if (dataDir.createDirectory().failed()) { report("Cannot create state directory."); return; }
        chooser = std::make_unique<FileChooser>(writing ? "Save TONEX state" : "Restore TONEX state",
                    dataDir.getChildFile("test-A.tonexstate"), "*.tonexstate");
        refresh();
        const auto browserFlags = writing ? FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                                       | FileBrowserComponent::warnAboutOverwriting
                                   : FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(browserFlags, [safe = Component::SafePointer<Probe>(this), writing](const FileChooser& fc)
        {
            if (! safe) return;
            const auto file = fc.getResult();
            if (file != File())
            {
                // Detach on the message thread before reading/writing plugin state.
                // AudioProcessorPlayer synchronises this with its audio callback.
                safe->player.setProcessor(nullptr);
                if (writing) safe->saveState(file);
                else safe->restoreState(file);
                safe->attachAudio();
            }
            // Defer destruction until the FileChooser callback has returned.
            MessageManager::callAsync([safe]
            {
                if (safe) { safe->chooser.reset(); safe->refresh(); }
            });
        });
    }

    void saveState(const File& file)
    {
        MemoryBlock state;
        plugin->getStateInformation(state);
        if (state.isEmpty() || state.getSize() > 90 * 1024 * 1024)
        { report("Save failed: plugin state is empty or exceeds 90 MiB."); return; }
        XmlElement root("TonexStateProbe");
        root.setAttribute("formatVersion", 1);
        root.setAttribute("savedAt", Time::getCurrentTime().toISO8601(true));
        root.setAttribute("bytes", static_cast<int>(state.getSize()));
        root.setAttribute("sha256", SHA256(state).toHexString());
        root.addChildElement(description.createXml().release());
        root.createNewChildElement("State")->addTextElement(state.toBase64Encoding());
        auto* params = root.createNewChildElement("Parameters");
        const auto& parameters = plugin->getParameters();
        for (int i = 0; i < parameters.size(); ++i)
        {
            auto* p = parameters[i];
            auto* entry = params->createNewChildElement("Parameter");
            entry->setAttribute("index", i);
            entry->setAttribute("name", p->getName(128));
            entry->setAttribute("value", static_cast<double>(p->getValue()));
        }
        // TemporaryFile is placed beside the destination: failed writes preserve the old state.
        TemporaryFile temporary(file);
        if (! root.writeTo(temporary.getFile()) || ! temporary.overwriteTargetFileWithTemporary())
        { report("Save failed: cannot write/replace " + file.getFullPathName()); return; }
        report("Saved " + String(static_cast<int64>(state.getSize())) + " bytes to " + file.getFullPathName());
    }

    void restoreState(const File& file)
    {
        // The wrapper is only intended for locally saved states.
        if (file.getSize() <= 0 || file.getSize() > 128 * 1024 * 1024)
        { report("Restore rejected: missing/empty file or file larger than 128 MiB."); return; }
        auto root = XmlDocument::parse(file);
        if (! root || ! root->hasTagName("TonexStateProbe") || root->getIntAttribute("formatVersion") != 1)
        { report("Restore rejected: invalid state file/version."); return; }
        PluginDescription savedDescription;
        auto* identity = root->getChildByName("PLUGIN");
        if (identity == nullptr || ! savedDescription.loadFromXml(*identity)
            || ! savedDescription.isDuplicateOf(description))
        { report("Restore rejected: state belongs to a different plugin."); return; }
        if (savedDescription.version != description.version)
        { report("Restore rejected: TONEX version differs from the saved version."); return; }
        MemoryBlock state;
        auto* encoded = root->getChildByName("State");
        if (encoded == nullptr || ! state.fromBase64Encoding(encoded->getAllSubText()) || state.isEmpty()
            || state.getSize() > static_cast<size_t>(std::numeric_limits<int>::max())
            || static_cast<int>(state.getSize()) != root->getIntAttribute("bytes")
            || SHA256(state).toHexString() != root->getStringAttribute("sha256"))
        { report("Restore rejected: corrupt or incomplete state data."); return; }
        editor.reset();
        plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        showEditor();
        report("State submitted to TONEX: " + file.getFullPathName()
               + ". Check model name, settings and sound; this is NOT an automatic pass.");
    }
};

class MainWindow final : public DocumentWindow
{
public:
    MainWindow() : DocumentWindow("TONEX State Probe 0.1.1 - ASIO", Colours::darkgrey, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new Probe(), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class Application final : public JUCEApplication
{
public:
    const String getApplicationName() override { return "TONEX State Probe"; }
    const String getApplicationVersion() override { return "0.1.1"; }
    bool moreThanOneInstanceAllowed() override { return getCommandLineParameters().trim() == "--diagnose-audio"; }
    void initialise(const String& commandLine) override
    {
        if (commandLine.trim() == "--diagnose-audio")
        {
            // Enumerate driver names without opening an audio device or loading TONEX.
            AudioDeviceManager manager;
            String report = "TONEX State Probe 0.1.1 - audio device enumeration\n";
            for (auto* type : manager.getAvailableDeviceTypes())
            {
                type->scanForDevices();
                report += "\n[" + type->getTypeName() + "]\n";
                report += "Inputs: " + type->getDeviceNames(true).joinIntoString(", ") + "\n";
                report += "Outputs: " + type->getDeviceNames(false).joinIntoString(", ") + "\n";
            }
            const auto file = File::getSpecialLocation(File::currentExecutableFile)
                                  .getSiblingFile("audio-devices.txt");
            setApplicationReturnValue(file.replaceWithText(report) ? 0 : 1);
            quit();
            return;
        }
        window = std::make_unique<MainWindow>();
    }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(Application)
