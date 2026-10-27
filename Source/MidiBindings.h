#pragma once
#include "RigSupport.h"
#include "Effects/Pedalboard/PedalDefinitions.h"

namespace board
{
using namespace juce;
constexpr int targets = rig::slotCount + static_cast<int>(definitions.size());
// MIDI targets retain their historical offset from rig slots, not visual positions.
constexpr int pedalMidiTarget(int persistentId) { return rig::slotCount + persistentId; }
constexpr int pedalForMidiTarget(int target) { return target - rig::slotCount; }
using Bindings = std::array<rig::Binding,targets>;
inline int match(const Bindings& bindings, const rig::Binding& b)
{
    if (! b.valid()) return -1;
    for (int i=0; i<targets; ++i) if (bindings[(size_t)i].valid() && bindings[(size_t)i] == b) return i;
    return -1;
}
inline bool assign(Bindings& bindings, int target, const rig::Binding& b)
{
    if (target < 0 || target >= targets || ! b.valid()) return false;
    const int previous = match(bindings,b); if (previous >= 0 && previous != target) return false;
    bindings[(size_t)target] = b; return true;
}
inline XmlElement encodeBindings(const Bindings& bindings)
{
    XmlElement xml("PedalMidi"); xml.setAttribute("version", 1);
    for (int i=0; i<targets; ++i)
    {
        const auto& b = bindings[(size_t)i]; if (! b.valid()) continue;
        auto* e = xml.createNewChildElement("Binding"); e->setAttribute("target",i); e->setAttribute("kind",b.kind);
        e->setAttribute("channel",b.channel); e->setAttribute("number",b.number);
    }
    return xml;
}
inline bool decodeBindings(const XmlElement& xml, Bindings& result)
{
    if (! xml.hasTagName("PedalMidi") || xml.getIntAttribute("version") != 1) return false;
    Bindings candidate {};
    for (auto* e : xml.getChildIterator())
    {
        const int i = e->getIntAttribute("target",-1);
        rig::Binding b {e->getIntAttribute("kind"),e->getIntAttribute("channel"),e->getIntAttribute("number",-1)};
        if (! e->hasTagName("Binding") || i<0 || i>=targets || candidate[(size_t)i].valid() || ! assign(candidate,i,b)) return false;
    }
    result = candidate; return true;
}
}
