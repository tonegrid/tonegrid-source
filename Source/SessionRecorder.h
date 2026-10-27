#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <mutex>

namespace tonegrid::recording
{
enum class Mode { empty, preparing, recording, finishing, ready, playing, exporting, closed };

// One ephemeral stereo take. Only the worker opens files, allocates buffers or encodes.
// The audio callback exchanges samples through one preallocated SPSC FIFO.
class SessionRecorder final : private juce::Thread
{
public:
    static constexpr double maximumSeconds=600;
    explicit SessionRecorder(juce::File encoder, double limit=maximumSeconds);
    ~SessionRecorder() override;
    Mode state() const noexcept { return mode.load(); }
    bool available() const noexcept { return hasTake.load(); }
    bool requestPending() const noexcept { return pending.load(); }
    double seconds() const noexcept { return captured.load()/takeRate.load(); }
    bool record(double rate);
    bool play(double rate);
    bool stop();
    bool exportMp3(const juce::File& target);
    juce::String takeMessage();
    void deviceStopped() noexcept { finishRequested.store(true); }
    void process(float* const* output,int channels,int samples,bool muted=false) noexcept;
    // Diagnostics read only after finalization, never while the worker is writing.
    juce::File completedTake() const;
private:
    enum class Action { none, record, play, stop, exportFile };
    struct Command { Action action=Action::none;double rate=48000;juce::File target; };
    bool request(Command command);
    void run() override;
    void beginRecording(double rate);
    void finishRecording();
    void beginPlayback(double rate);
    void fillPlayback();
    void endPlayback();
    void encode(const juce::File& destination);
    bool drainRecording();
    void suspendAudio(Mode next);
    void message(const juce::String& text);
    Mode restingState() const noexcept { return hasTake.load()?Mode::ready:Mode::empty; }

    static constexpr int capacity=65536,blockSize=8192;
    juce::AbstractFifo fifo{capacity};
    juce::AudioBuffer<float> ring{2,capacity},scratch{2,blockSize};
    const juce::File encoderFile,sessionFolder;
    const double limitSeconds;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::ResamplingAudioSource> resampler;
    juce::File take;
    std::atomic<Mode> mode{Mode::empty};
    std::atomic<bool> hasTake{false},pending{false},finishRequested{false},overflow{false};
    std::atomic<int> readers{0};
    std::atomic<juce::int64> captured{0},played{0};
    std::atomic<double> takeRate{48000};
    juce::int64 captureLimit=0,playLimit=0,produced=0;
    bool storageFailed=false;
    std::mutex controlMutex;Command command;juce::String notification;
};
}
