#pragma once
#include <cstdint>
#include <optional>
namespace tonegrid::core
{
// One pending choice, not a queue of expensive loads. Audio never accesses it.
class PresetLoadQueue
{
public:
    struct Request {int slot;std::uint64_t revision;};
    void request(int slot,double now){pending=Request{slot,++generation};due=now+150.;}
    std::optional<Request> take(double now)
    {if(!pending||now<due)return {};auto value=pending;pending.reset();return value;}
    bool current(std::uint64_t revision)const{return generation==revision;}
    bool idle()const{return !pending.has_value();}
    void cancel(){++generation;pending.reset();}
private:
    std::uint64_t generation=0;double due=0;std::optional<Request> pending;
};
}
