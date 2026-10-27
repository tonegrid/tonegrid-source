#pragma once
#include <array>
#include <cstddef>

namespace board
{
// Persistent XML/catalog IDs. Never infer identity from a visual/table position.
enum class PedalId : int
{
    StudioComp = 0,
    TubeScreamer = 1,
    HorseOd = 2,
    OrangeDist = 3,
    TransparentOd = 4,
    VintageFuzz = 5,
    CleanBoost = 6,
    ClassicFuzz = 7,
    TrebleBoost = 8,
    AutoWah = 9,
    BluesOd = 10,
    FilterDist = 11,
    GraphicEq = 12
};

constexpr int id(PedalId pedal) noexcept { return static_cast<int>(pedal); }

enum class PedalFamily { Nam, Compressor, Boost, AutoWah, GraphicEq };

struct PedalDefinition
{
    PedalId persistentId;
    const char* name;        // Existing engine/log name, intentionally preserved.
    const char* displayName; // Existing card title.
    PedalFamily family;
    const char* skinResource; // JUCE BinaryData resource name; resolved only by UI.
};

// The one fixed audio/visual order. Resource identity is explicit, not positional.
inline constexpr std::array definitions{
    PedalDefinition{PedalId::ClassicFuzz,   "CLASSIC FUZZ", "CLASSIC FUZZ",   PedalFamily::Nam,        "skin01_png"},
    PedalDefinition{PedalId::TrebleBoost,   "TREBLE BOOST", "TREBLE BOOST",   PedalFamily::Nam,        "skin02_png"},
    PedalDefinition{PedalId::AutoWah,       "AUTO WAH",     "AUTO WAH",       PedalFamily::AutoWah,    "skin03_png"},
    PedalDefinition{PedalId::StudioComp,    "COMP",         "STUDIO COMP",    PedalFamily::Compressor, "skin04_png"},
    PedalDefinition{PedalId::TransparentOd, "TIMMY",        "TRANSPARENT OD", PedalFamily::Nam,        "skin05_png"},
    PedalDefinition{PedalId::HorseOd,       "CENTURA",      "HORSE OD",       PedalFamily::Nam,        "skin06_png"},
    PedalDefinition{PedalId::TubeScreamer,  "TS9",          "808 OD",         PedalFamily::Nam,        "skin07_png"},
    PedalDefinition{PedalId::BluesOd,       "BLUES OD",     "BLUES OD",       PedalFamily::Nam,        "skin08_png"},
    PedalDefinition{PedalId::FilterDist,    "FILTER DIST",  "FILTER DIST",    PedalFamily::Nam,        "skin09_png"},
    PedalDefinition{PedalId::OrangeDist,    "DS-1",         "ORANGE DIST",    PedalFamily::Nam,        "skin10_png"},
    PedalDefinition{PedalId::VintageFuzz,   "BIG MUFF",     "VINTAGE FUZZ",   PedalFamily::Nam,        "skin11_png"},
    PedalDefinition{PedalId::GraphicEq,     "GRAPHIC EQ",   "GRAPHIC EQ",     PedalFamily::GraphicEq,  "skin12_png"},
    PedalDefinition{PedalId::CleanBoost,    "BOOST",        "CLEAN BOOST",    PedalFamily::Boost,      "skin13_png"}
};

inline constexpr int count = static_cast<int>(definitions.size());
// Formats 3/4/5 contain exactly the original IDs 0 through 6.
inline constexpr int legacyPedalCount = id(PedalId::CleanBoost) + 1;

constexpr bool validDefinitions() noexcept
{
    std::array<bool, count> seen{};
    for (const auto& d : definitions)
    {
        const int value = id(d.persistentId);
        if (value < 0 || value >= count || seen[static_cast<std::size_t>(value)]) return false;
        seen[static_cast<std::size_t>(value)] = true;
    }
    return true;
}
static_assert(validDefinitions(), "Existing fixed storage requires unique, dense historical IDs");

// Compile-time projections, not independently maintained definitions. The runtime
// storage remains indexed by explicit historical ID for this milestone.
inline constexpr auto order = []
{
    std::array<int, count> result{};
    for (std::size_t i = 0; i < definitions.size(); ++i) result[i] = id(definitions[i].persistentId);
    return result;
}();

inline constexpr auto definitionRowsById = []
{
    std::array<std::size_t, count> result{};
    for (std::size_t row = 0; row < definitions.size(); ++row)
        result[static_cast<std::size_t>(id(definitions[row].persistentId))] = row;
    return result;
}();

// Caller must supply a validated historical ID, as for existing processor arrays.
constexpr const PedalDefinition& definition(int persistentId) noexcept
{
    return definitions[definitionRowsById[static_cast<std::size_t>(persistentId)]];
}

constexpr bool isNam(int persistentId) noexcept
{
    return persistentId >= 0 && persistentId < count && definition(persistentId).family == PedalFamily::Nam;
}
}
