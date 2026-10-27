#pragma once
#include "Content/Presets/RigCodec.h"
#include "MidiBindings.h"
namespace board
{
// Compatibility entry points for existing callers; the codec only queries model
// identities and never reads/writes live parameters or plugin state.
inline XmlElement encode(const State& s, const PluginDescription& description, const Processor& processor)
{
    return encode(s, description, [&processor](int p, int preset) { return processor.identity(p, preset); });
}
inline Result decode(const XmlElement& xml, const PluginDescription& expected, const Processor& processor, State& result)
{
    return decode(xml, expected, [&processor](int p, int preset) { return processor.identity(p, preset); }, result);
}
}
