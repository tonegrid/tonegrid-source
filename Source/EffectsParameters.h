#pragma once
#include <cmath>
namespace tonegrid
{
namespace pitch
{
enum class Mode { Off, Transpose, Octaver };
struct Parameters { Mode mode=Mode::Off; int semitones=0; float dry=100,down=50,up=0; };
inline bool valid(const Parameters& p) noexcept
{return int(p.mode)>=0&&int(p.mode)<=2&&p.semitones>=-24&&p.semitones<=24&&std::isfinite(p.dry)&&p.dry>=0&&p.dry<=100&&std::isfinite(p.down)&&p.down>=0&&p.down<=100&&std::isfinite(p.up)&&p.up>=0&&p.up<=100;}
}
namespace mod
{
enum class Type { Chorus,Tremolo,Phaser,Flanger };
struct Parameters { bool enabled=false; Type type=Type::Chorus; float rateHz=1,depth=35,level=35; };
inline Parameters voicing(Type type) noexcept
{switch(type){case Type::Tremolo:return {false,type,4,55,100};case Type::Phaser:return {false,type,.6f,55,65};case Type::Flanger:return {false,type,.25f,35,35};default:return {};}}
inline bool valid(const Parameters& p) noexcept
{return int(p.type)>=0&&int(p.type)<=3&&std::isfinite(p.rateHz)&&p.rateHz>=.05f&&p.rateHz<=10&&std::isfinite(p.depth)&&p.depth>=0&&p.depth<=100&&std::isfinite(p.level)&&p.level>=0&&p.level<=100;}
}
namespace delay
{
struct Parameters { bool enabled=false; float timeMs=320,feedback=25,mix=18; };
inline bool valid(const Parameters& p) noexcept
{return std::isfinite(p.timeMs)&&p.timeMs>=1&&p.timeMs<=2000&&std::isfinite(p.feedback)&&p.feedback>=0&&p.feedback<=95&&std::isfinite(p.mix)&&p.mix>=0&&p.mix<=100;}
}
}
