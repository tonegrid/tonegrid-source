#pragma once
namespace tonegrid::core {
// An insert at the device boundary, before input trim, and after guitar DSP.
struct IAudioInsert {
 virtual bool enabled()const noexcept=0;
 virtual void deviceStarted(double rate) noexcept=0;
 virtual void deviceStopped() noexcept=0;
 virtual const float* const* input(const float* const*,int,int)noexcept=0;
 virtual void output(float* const*,int,int)noexcept=0;
 virtual ~IAudioInsert()=default;
};
}
