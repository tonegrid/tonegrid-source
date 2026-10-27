#pragma once
#include <array>
#include <atomic>
#include <type_traits>
namespace tonegrid
{
// One control-thread producer, one audio-thread consumer. Three owned slots avoid
// torn settings without locks, allocations, or retry loops on the audio thread.
template<class T> class ParameterMailbox final
{
public:
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    void publish(const T& value) noexcept
    {slots[back]=value;back=middle.exchange(back|dirty,std::memory_order_acq_rel)&mask;}
    T read() noexcept
    {if(middle.load(std::memory_order_acquire)&dirty)front=middle.exchange(front,std::memory_order_acq_rel)&mask;return slots[front];}
private:
    static constexpr unsigned dirty=4,mask=3;
    std::array<T,3> slots{};std::atomic<unsigned> middle{1};unsigned front=0,back=2;
};
}
