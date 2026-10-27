#pragma once
#include "MidiBindings.h"
#include <atomic>

namespace board
{
// Single MIDI input producer, message-thread consumer. The 256-cell JUCE FIFO
// has 255 usable entries, exactly as in RigHost before extraction.
class MidiEventQueue
{
public:
    void push(const rig::Event& event) noexcept
    {
        int a,b,c,d; fifo.prepareToWrite(1,a,b,c,d);
        if (b>0) { events[(size_t)a]=event; fifo.finishedWrite(1); }
        else overflow.store(true);
    }
    void push(const MidiMessage& message) noexcept
    {
        if(message.isProgramChange())push(rig::Event{1,message.getChannel(),message.getProgramChangeNumber(),0});
        else if(message.isController())push(rig::Event{2,message.getChannel(),message.getControllerNumber(),message.getControllerValue()});
    }
    bool pop(rig::Event& event) noexcept
    {
        int a,b,c,d; fifo.prepareToRead(1,a,b,c,d);
        if(b==0)return false;
        event=events[(size_t)a];fifo.finishedRead(1);return true;
    }
    bool takeOverflow() noexcept { return overflow.exchange(false); }
    // Call only after stopping the input. Preserve the pending overflow warning,
    // which historically survived device refresh/switch even when the FIFO reset.
    void resetAfterInputStopped() noexcept { fifo.reset(); }
private:
    AbstractFifo fifo{256};
    std::array<rig::Event,256> events{};
    std::atomic<bool> overflow{false};
};

class MidiController
{
public:
    // Synchronous message-thread operations, not an event bus. The host persists
    // assignments before the controller publishes them, as before milestone 2B.
    struct Actions
    {
        virtual ~Actions() = default;
        virtual bool storeMidiBindings(const Bindings&) = 0;
        virtual void recallMidiRig(int slot) = 0;
        virtual void toggleMidiPedal(int persistentId) = 0;
        virtual void midiStateChanged() = 0;
        virtual void reportMidi(const String&) = 0;
    };

    // The only callback-thread entry point. No allocation, IO, UI or dispatch.
    void enqueue(const MidiMessage& message) noexcept { queue.push(message); }
    const Bindings& assignments() const { return bindings; }
    int learningTarget() const { return learning; }
    bool restoreAssignments(const XmlElement& xml) { return decodeBindings(xml,bindings); }
    void cancelLearn() { learning=-1; }
    void resetAfterInputStopped() { queue.resetAfterInputStopped();edges={};cancelLearn(); }

    static String targetName(int target)
    {
        return target<rig::slotCount ? "rig "+String(target+1)
            : String(definition(pedalForMidiTarget(target)).name);
    }

    void startLearn(int target, Actions& actions)
    {
        if(target<0||target>=targets)return;
        learning=learning==target?-1:target;
        if(learning>=0)actions.reportMidi("MIDI Learn for "+targetName(target)+": press a controller button (PC or CC 64-127).");
        actions.midiStateChanged();
    }
    void clearBinding(int target, Actions& actions)
    {
        if(target<0||target>=targets)return;
        auto next=bindings;next[(size_t)target]={};
        if(actions.storeMidiBindings(next)) { bindings=next;cancelLearn();actions.midiStateChanged(); }
    }

    // Called at the existing 30 Hz timer position. Dispatch each command before
    // reading the next event: recall then toggle must observe the recalled state.
    void drain(Actions& actions)
    {
        if(queue.takeOverflow()) { edges={};actions.reportMidi("MIDI queue full; release and press your controller again."); }
        for(int n=0;n<255;++n)
        {
            rig::Event event;if(!queue.pop(event))break;
            auto bind=edges.accept(event);if(!bind)continue;
            if(learning>=0)
            {
                auto next=bindings;
                if(!board::assign(next,learning,*bind))
                    actions.reportMidi("This MIDI message is already assigned to "+targetName(board::match(bindings,*bind))+". Clear that assignment first.");
                else if(actions.storeMidiBindings(next))
                {
                    bindings=next;actions.reportMidi("Learned "+bind->text()+" for "+targetName(learning));
                    cancelLearn();actions.midiStateChanged();
                }
                continue; // Learning never also recalls/toggles the learned target.
            }
            const int target=board::match(bindings,*bind);if(target<0)continue;
            if(target<rig::slotCount)actions.recallMidiRig(target);
            else actions.toggleMidiPedal(pedalForMidiTarget(target));
        }
    }
private:
    MidiEventQueue queue;
    rig::MidiEdges edges;
    Bindings bindings{};
    int learning=-1;
};
}
