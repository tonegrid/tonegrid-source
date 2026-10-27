#pragma once
#include "Content/Presets/RigState.h"

namespace board::migrations
{
// These are the existing v3-v6 compatibility rules, not a new file format.
// Historical schema sizes, independent of future board additions.
inline int pedalCount(int version) { return version >= 6 ? 13 : 7; }

inline State initialState()
{
    State candidate;
    for (int p = 0; p < count; ++p) candidate.pedals[(size_t)p] = defaults(p, recommendedPreset(p));
    return candidate; // Missing v3 Input retains the InputSettings defaults.
}

inline Result legacyNam(const XmlElement& item, int version, int p, Setting& setting)
{
    if (isNam(p) && version < 5)
    {
        const auto oldMix = item.getDoubleAttribute("mix", 999);
        if (!std::isfinite(setting.input) || setting.input < -18 || setting.input > 18
            || !std::isfinite(oldMix) || oldMix < 0 || oldMix > 1)
            return Result::fail("Invalid legacy NAM settings.");
        setting.input = jlimit(-12.f, 12.f, setting.input);
    }
    // The codec sets NAM mix to 1 and pre-v5 tone to 0 before this migration.
    return Result::ok();
}

inline float defaultRatio(int p, int preset)
{
    return p == id(PedalId::StudioComp) && preset >= 0 && preset < presets
        ? compressors[(size_t)preset].ratio : 2.f;
}

inline bool isLegacyWah(int p, const String& model)
{
    return p == id(PedalId::AutoWah) && model == "auto-wah-v1";
}
// Factory identities map to the nearest supplied Gain/Tone clock settings.
inline int bluesPreset(const String& hash) noexcept
{
    struct Mapping{const char* hash;int index;};static const Mapping mappings[]{
        {"fbcaec4625b7e7cb1ce0e7ac6799782173948a733edd731b742c29c03cef33a2",4},
        {"a008c831d9ae1913b46376fcfbe9f0dabf581892f4086c2b07f045fc57b7c78a",5},
        {"9491ca45e9ca7774542919acfb249c0920cf881acc1c69f64f379dba699432a6",7},
        {"1daaed1d55e4cd321ca3dabd67404f2c3eafff7785b6fb64c5a99e606f383578",8},
        {"7105ec3160dcffc1bda1194d4efc60be1ff427d41669580368c527c8f7a1587f",7},
        {"7f227ef568e283793d66b0650a9a0f2e14c0f23a24dee3077777e2de8da4c2e7",8},
        {"d2ca82812d53d2cd47e768c0a230f0e653d62217bf10f0f36dd0192f180d52f7",10},
        {"1e57e165afabd23ff94bf62f78c952b5cc9a69f5ce0874266954cdc9e8e52d16",11},
        {"97d64d1169fb73d665e410e57487cadd5c40adaa90a9164f8963ffca68d0fc72",13},
        {"d9bbfc23600a03d870373ea7aa2a4b5ac069d5f78d7012be097cfa2543212f9d",14},
        {"b38c769b8d24dc066587a825661d6eb1c05b2c8328d6682351704adf9e19f4f9",13},
        {"9016e3b294e318b17858771c56211de6a87db11cf55ec707afaa00b983c57b1f",14},
        {"489dde909808877408a1ca2de3955be43978bf98c6dee5797dd5ce7c7d583747",16},
        {"484c84c1def1a566f123ac66613ce69bb08adc27b4b039f8e3d64db7bdf28fe0",17},
    };
    for(const auto& item:mappings)if(hash==item.hash)return item.index;
    return -1;
}
// Stable identities migrate retained Wampler captures and the explicitly replaced DS-1 variants.
inline int orangePreset(const String& hash) noexcept
{
    struct Mapping{const char* hash;int index;};static const Mapping mappings[]{
        {"905f6d565094b63f6c33bee1f5159b64e61142f1f8e75fb7fda31aeefe9a40fe",0},
        {"88f7057da94b6306c561f649743703f8cbfe5d18024a18be046db8ebfa3054c9",2},
        {"4f5e84f86b7d5b0b958f4bdfae5455ea79eb05bac5e2af87605727982be304a2",4},
        {"988c100a6a1a1efc9cff5715e624f7a543c59331b54b204126786673bb3761c4",0},
        {"238bcd53c0b4f3412759c8a654001a3e1df38f4bdf64a83c0a710b52c0e043ff",1},
        {"8018b901a7d0744250615537ded839d62b52b7a91c24b7429ed75f34e8ec373b",2},
        {"69d69f0563c2079914a377cc466a8fedc3543b8def29035bddb7208ddfb8d352",3},
        {"bbc8fb6da57e2eef05bf021e7c0564a5b747bbcbe30297a624cc9a8bcc97614b",4},
        {"3001576f0de3d63a9e71f4e3a46d691b4db784ab7943b78ce78d254dd64c8f91",5},
    };
    for(const auto& entry:mappings)if(hash==entry.hash)return entry.index;
    return -1;
}
inline bool withdrawnOrange(const String& hash) noexcept
{return hash=="905f6d565094b63f6c33bee1f5159b64e61142f1f8e75fb7fda31aeefe9a40fe"||hash=="88f7057da94b6306c561f649743703f8cbfe5d18024a18be046db8ebfa3054c9"||hash=="4f5e84f86b7d5b0b958f4bdfae5455ea79eb05bac5e2af87605727982be304a2";}

}
