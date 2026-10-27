#pragma once
#include "Effects/Pedalboard/PedalBoard.h"

namespace board
{
// Shared application action for mouse and MIDI. The DSP processor and all
// parameter publication semantics are unchanged; no PedalCard is needed.
inline void togglePedal(Processor& processor, int persistentId)
{
    if(persistentId<0||persistentId>=count)return;
    auto setting=processor.get(persistentId);
    setting.enabled=!setting.enabled;
    processor.set(persistentId,setting);
}
}
