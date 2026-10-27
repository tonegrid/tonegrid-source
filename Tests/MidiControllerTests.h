#pragma once
#include "MidiController.h"
#include "Effects/Pedalboard/PedalCommands.h"
#include <functional>
#include <thread>
#include <vector>

// No PedalCard, Component, RigHost, device or filesystem is needed by this suite.
namespace midiTests
{
using namespace juce;
using Controller = board::MidiController;
struct Sink final : Controller::Actions
{
    board::Processor engine;
    board::Bindings persisted{};
    std::vector<std::pair<char,int>> commands;
    StringArray messages;
    int saves=0, refreshes=0;
    bool writable=true;
    std::function<void()> duringCommand;
    bool storeMidiBindings(const board::Bindings& next) override
    {
        ++saves;
        if(!writable){reportMidi("Could not save MIDI assignments.");return false;}
        persisted=next;return true;
    }
    void recallMidiRig(int slot) override
    {
        commands.emplace_back('R',slot);
        auto s=engine.get(board::id(board::PedalId::StudioComp));s.enabled=false;
        engine.set(board::id(board::PedalId::StudioComp),s);
        if(duringCommand)duringCommand();
    }
    void toggleMidiPedal(int p) override
    {
        commands.emplace_back('P',p);board::togglePedal(engine,p);
        if(duringCommand)duringCommand();
    }
    void midiStateChanged() override { ++refreshes; }
    void reportMidi(const String& message) override { messages.add(message); }
};
inline void pc(Controller& controller,int number,int channel=1)
{ controller.enqueue(MidiMessage::programChange(channel,number)); }
inline void cc(Controller& controller,int number,int value,int channel=1)
{ controller.enqueue(MidiMessage::controllerEvent(channel,number,value)); }
inline bool install(Controller& controller, std::initializer_list<std::pair<int,rig::Binding>> entries)
{
    board::Bindings bindings{};
    for(const auto& entry:entries)if(!board::assign(bindings,entry.first,entry.second))return false;
    return controller.restoreAssignments(board::encodeBindings(bindings));
}
inline bool sameBindings(const board::Bindings& a,const board::Bindings& b)
{ const auto expected=board::encodeBindings(b);return board::encodeBindings(a).isEquivalentTo(&expected,false); }

inline bool run(String& report)
{
    bool ok=true;int checks=0;
    auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    constexpr int comp=board::id(board::PedalId::StudioComp);
    const int compTarget=board::pedalMidiTarget(comp);
    {
        Controller c;Sink s;
        check(install(c,{{0,{1,1,12}},{127,{1,16,127}},{compTarget,{2,2,19}}}),"MIDI synthetic assignments installed");
        pc(c,0);pc(c,12,2);c.enqueue(MidiMessage::noteOn(1,60,(uint8)100));
        c.enqueue(MidiMessage::pitchWheel(1,8192));c.drain(s);
        check(s.commands.empty(),"Unmapped PC, wrong channel, notes and pitch bend do nothing");
        pc(c,12);pc(c,12);pc(c,127,16);c.drain(s);
        check(s.commands==std::vector<std::pair<char,int>>{{'R',0},{'R',0},{'R',127}},"PC recalls mapped first/last slots; repeated PC is not debounced");
        check(s.saves==0&&s.refreshes==0,"Playback does not write assignments or trigger Learn refresh");
        s.commands.clear();cc(c,19,0,2);cc(c,19,63,2);cc(c,19,64,2);cc(c,19,127,2);cc(c,19,64,2);c.drain(s);
        check(s.commands.size()==1&&s.commands[0]==std::make_pair('P',comp)&&s.engine.get(comp).enabled,"CC 64 rising edge toggles engine without any card");
        cc(c,19,127,1);cc(c,19,63,2);cc(c,19,0,2);c.drain(s);
        check(s.commands.size()==1,"Wrong CC channel and release do not toggle");
        cc(c,19,64,2);c.drain(s);
        check(s.commands.size()==2&&!s.engine.get(comp).enabled,"CC release rearms next press");
        check(c.assignments()[(size_t)compTarget]==rig::Binding{2,2,19},"Playback leaves global assignment unchanged");
    }
    {
        Controller c;Sink s;
        check(install(c,{{0,{2,1,10}},{1,{2,2,10}},{2,{2,1,11}},{compTarget,{1,3,20}}}),"CC rig and PC pedal mappings supported as before");
        cc(c,10,127,1);cc(c,10,127,2);cc(c,11,127,1);pc(c,20,3);c.drain(s);
        check(s.commands==std::vector<std::pair<char,int>>{{'R',0},{'R',1},{'R',2},{'P',comp}},"CC edges independent by number/channel; PC can toggle pedal");
    }
    {
        Controller c;Sink s;
        c.startLearn(compTarget,s);
        check(c.learningTarget()==compTarget&&s.messages[s.messages.size()-1]=="MIDI Learn for COMP: press a controller button (PC or CC 64-127)."&&s.refreshes==1,"Learn starts with unchanged message and UI notification");
        cc(c,8,0);cc(c,8,63);c.drain(s);
        check(c.learningTarget()==compTarget&&s.saves==0,"CC release cannot complete Learn");
        cc(c,8,64,4);c.drain(s);
        check(c.learningTarget()==-1&&c.assignments()[(size_t)compTarget]==rig::Binding{2,4,8}
            &&s.saves==1&&s.commands.empty()&&s.refreshes==2,"CC Learn commits once without executing pedal action");
        check(s.messages[s.messages.size()-1]=="Learned CC 8 / channel 4 for COMP"&&sameBindings(c.assignments(),s.persisted),"Learn success text and persisted mapping preserved");
        cc(c,8,127,4);c.drain(s);check(s.commands.empty(),"Held learned CC does not immediately retrigger");
        cc(c,8,0,4);cc(c,8,127,4);c.drain(s);check(s.commands.size()==1,"Learned CC executes after release/press");
        c.startLearn(compTarget,s);pc(c,26,7);c.drain(s);
        check(c.assignments()[(size_t)compTarget]==rig::Binding{1,7,26}&&s.saves==2,"Learn replacement removes old binding and saves new PC");
        s.commands.clear();cc(c,8,0,4);cc(c,8,127,4);pc(c,26,7);c.drain(s);
        check(s.commands==std::vector<std::pair<char,int>>{{'P',comp}},"Replaced message no longer controls target");
        c.startLearn(42,s);c.startLearn(42,s);
        check(c.learningTarget()==-1,"Pressing Learn twice cancels it");
        c.startLearn(42,s);c.startLearn(compTarget,s);
        check(c.learningTarget()==compTarget,"Learn can switch to another target");
        c.cancelLearn();check(c.learningTarget()==-1,"Slot selection can cancel Learn without resetting assignments");
    }
    {
        Controller c;Sink s;install(c,{{0,{1,1,3}},{compTarget,{1,1,4}}});
        c.startLearn(compTarget,s);pc(c,3);c.drain(s);
        check(c.learningTarget()==compTarget&&s.saves==0&&s.commands.empty()
            &&c.assignments()[(size_t)compTarget]==rig::Binding{1,1,4},"Conflict preserves previous binding and keeps Learn active");
        check(s.messages[s.messages.size()-1]=="This MIDI message is already assigned to rig 1. Clear that assignment first.","Conflict message retains human slot numbering");
        pc(c,4);c.drain(s);
        check(c.learningTarget()==-1&&s.saves==1&&s.commands.empty(),"Learning same binding on same target is permitted");
        c.startLearn(0,s);pc(c,4);c.drain(s);
        check(s.messages[s.messages.size()-1]=="This MIDI message is already assigned to COMP. Clear that assignment first.","Pedal conflict uses canonical historical name");
        s.writable=false;pc(c,5);c.drain(s);
        check(c.learningTarget()==0&&c.assignments()[0]==rig::Binding{1,1,3}
            &&s.messages[s.messages.size()-1]=="Could not save MIDI assignments.","Failed Learn save does not publish candidate or cancel Learn");
        s.writable=true;pc(c,5);c.drain(s);
        check(c.assignments()[0]==rig::Binding{1,1,5}&&c.learningTarget()==-1,"PC Learn retry commits after persistence recovers");
        c.startLearn(7,s);s.writable=false;const int refreshes=s.refreshes;c.clearBinding(0,s);
        check(c.assignments()[0].valid()&&c.learningTarget()==7&&s.refreshes==refreshes,"Failed Clear keeps assignment and Learn unchanged");
        s.writable=true;c.clearBinding(0,s);
        check(!c.assignments()[0].valid()&&c.learningTarget()==-1&&s.refreshes==refreshes+1,"Successful Clear commits and cancels any active Learn");
        const int saves=s.saves;c.clearBinding(0,s);check(s.saves==saves+1,"Clearing empty target still persists as before");
    }
    {
        Controller c;Sink s;
        cc(c,7,127);c.drain(s);c.startLearn(9,s);cc(c,7,127);c.drain(s);
        check(c.learningTarget()==9&&s.saves==0,"Starting Learn does not reset previously held CC edges");
        cc(c,7,0);cc(c,7,127);s.writable=false;c.drain(s);const int saves=s.saves;
        cc(c,7,127);c.drain(s);check(c.learningTarget()==9&&s.saves==saves,"Failed CC Learn still consumes rising edge");
        s.writable=true;cc(c,7,0);cc(c,7,127);c.drain(s);
        check(c.learningTarget()==-1&&c.assignments()[9]==rig::Binding{2,1,7},"Release/press permits retry after failed CC Learn");
        c.startLearn(8,s);c.resetAfterInputStopped();
        check(c.learningTarget()==-1&&c.assignments()[9].valid(),"Device reset cancels Learn but retains assignments");
        cc(c,7,127);c.drain(s);check(s.commands==std::vector<std::pair<char,int>>{{'R',9}},"Device reset clears held edges");
    }
    {
        Controller c;Sink s;install(c,{{0,{1,1,0}},{compTarget,{2,1,20}}});
        pc(c,0);cc(c,20,127);c.drain(s);
        check(s.commands==std::vector<std::pair<char,int>>{{'R',0},{'P',comp}}&&s.engine.get(comp).enabled,"FIFO recall then toggle applies synchronously to recalled state");
        s.commands.clear();cc(c,20,0);cc(c,20,127);pc(c,0);c.drain(s);
        check(s.commands==std::vector<std::pair<char,int>>{{'P',comp},{'R',0}}&&!s.engine.get(comp).enabled,"FIFO toggle then recall preserves opposite action order");
        c.startLearn(1,s);pc(c,1);pc(c,1);c.drain(s);
        check(s.commands.back()==std::make_pair('R',1)&&s.saves==1,"One burst can learn then execute the next identical PC");
    }
    {
        Controller c;Sink s;board::Bindings mappings{};
        for(const auto& definition:board::definitions)
        {
            const int p=board::id(definition.persistentId);
            board::assign(mappings,board::pedalMidiTarget(p),{2,3,60+p});
            auto setting=board::defaults(p,3);setting.enabled=false;s.engine.set(p,setting);
        }
        check(c.restoreAssignments(board::encodeBindings(mappings)),"All canonical historical pedal targets loaded");
        for(const auto& definition:board::definitions)
        {
            const int p=board::id(definition.persistentId);const auto before=s.engine.get(p);
            cc(c,60+p,127,3);c.drain(s);const auto after=s.engine.get(p);
            check(s.commands.back()==std::make_pair('P',p)&&after.enabled&&after.preset==before.preset
                &&after.input==before.input&&after.mix==before.mix&&after.output==before.output&&after.ratio==before.ratio
                &&after.tone==before.tone&&after.wahSensitivity==before.wahSensitivity&&after.wahRange==before.wahRange
                &&after.wahDecay==before.wahDecay&&after.eqBands==before.eqBands,
                  "Headless command toggles only enabled for historical ID "+String(p));
        }
        const auto before=c.assignments();auto invalid=board::encodeBindings(before);
        invalid.addChildElement(new XmlElement(*invalid.getFirstChildElement()));
        check(!c.restoreAssignments(invalid)&&sameBindings(before,c.assignments()),"Invalid global assignment XML rejected without mutation");
        const auto encoded=board::encodeBindings(c.assignments());
        check(encoded.hasTagName("PedalMidi")&&encoded.getIntAttribute("version")==1&&encoded.getNumChildElements()==13,"Existing global PedalMidi v1 format retained");
    }
    {
        board::MidiEventQueue q;rig::Event event;
        q.push(MidiMessage::noteOff(1,60));check(!q.pop(event)&&!q.takeOverflow(),"Queue ignores unrelated MIDI messages");
        for(int i=0;i<255;++i)q.push(rig::Event{1,1,i,0});
        check(!q.takeOverflow(),"FIFO accepts exactly 255 outstanding events");
        q.push(rig::Event{1,1,999,0});check(q.takeOverflow()&&!q.takeOverflow(),"Full FIFO drops incoming event and latches overflow once");
        bool ordered=true;for(int i=0;i<255;++i)ordered&=q.pop(event)&&event.number==i;
        check(ordered&&!q.pop(event),"Overflow retains queued events in arrival order");
        for(int cycle=0;cycle<12;++cycle)
        {
            for(int i=0;i<173;++i)q.push(rig::Event{2,2,i,cycle});
            for(int i=0;i<173;++i)ordered&=q.pop(event)&&event.number==i&&event.value==cycle;
        }
        check(ordered&&!q.takeOverflow()&&!q.pop(event),"FIFO wraparound preserves repeated bursts");
        for(int i=0;i<256;++i)q.push(rig::Event{1,1,i,0});q.resetAfterInputStopped();
        check(!q.pop(event)&&q.takeOverflow(),"Device reset clears pending events but preserves overflow warning");
    }
    {
        Controller c;Sink s;install(c,{{0,{1,1,0}},{compTarget,{2,1,7}}});
        cc(c,7,127);c.drain(s);s.commands.clear();
        for(int i=0;i<254;++i)pc(c,0);cc(c,7,127);cc(c,7,0);c.drain(s);
        check(s.commands.size()==255&&s.commands.back()==std::make_pair('P',comp),"Overflow resets edges before consuming retained queue, including held CC");
        check(s.messages.size()==1&&s.messages[0]=="MIDI queue full; release and press your controller again.","Overflow message unchanged and emitted once per drain");
        const auto size=s.commands.size();cc(c,7,127);c.drain(s);
        check(s.commands.size()==size,"Dropped CC release leaves high state until release is received");
        cc(c,7,0);cc(c,7,127);c.drain(s);check(s.commands.size()==size+1,"Overflow recovery preserves release/press behavior");
        c.startLearn(10,s);for(int i=0;i<256;++i)pc(c,31);c.drain(s);
        check(c.assignments()[10]==rig::Binding{1,1,31}&&c.learningTarget()==-1,
              "Overflow retains Learn; first retained message learns and subsequent PCs execute");
    }
    {
        Controller c;Sink s;install(c,{{0,{1,1,0}}});
        // Commands may synchronously allow the producer to add more events.
        // The consumer still processes at most 255 per timer tick.
        s.duringCommand=[&]{pc(c,0);};pc(c,0);c.drain(s);
        check(s.commands.size()==255,"Drain remains bounded when events arrive during dispatch");
        s.duringCommand={};c.drain(s);check(s.commands.size()==256,"Events beyond drain budget remain for next tick");
        pc(c,0);c.startLearn(12,s);c.resetAfterInputStopped();c.drain(s);
        check(s.commands.size()==256&&c.learningTarget()==-1,"Device switch discards queued actions and cancels Learn");
        for(int i=0;i<256;++i)pc(c,0);c.resetAfterInputStopped();c.drain(s);
        check(s.commands.size()==256&&s.messages[s.messages.size()-1]=="MIDI queue full; release and press your controller again.","Overflow warning survives device reset as historically");
    }
    {
        // Exercise actual single-producer/single-consumer publication, bounded
        // below capacity in acknowledged bursts so this is not a timing test.
        board::MidiEventQueue q;std::atomic<int> acknowledged{0};std::atomic<bool> failed{false};
        std::thread producer([&]{for(int batch=0;batch<40;++batch){while(acknowledged.load()!=batch)std::this_thread::yield();
            for(int i=0;i<100;++i)q.push(rig::Event{1,1,batch*100+i,0});}});
        rig::Event event;
        for(int batch=0;batch<40;++batch){for(int i=0;i<100;++i){while(!q.pop(event))std::this_thread::yield();
            if(event.number!=batch*100+i)failed.store(true);}acknowledged.store(batch+1);}
        producer.join();
        check(!failed.load()&&!q.takeOverflow()&&!q.pop(event),"SPSC concurrent queue preserves 4000 ordered events");
    }
    report+=String(checks)+" MIDI controller checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";
    return ok;
}
}
