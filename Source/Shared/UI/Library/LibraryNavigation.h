#pragma once
#include "Content/Amps/NamLibraryIndex.h"
#include <set>
namespace tonegrid::libraryui {
enum class BrowseMode { all, favorites, folder };
struct NavigationItem {
    juce::String path, title, searchable;
    bool favorite = false;
    int sourceIndex = -1;
};
struct NavigationRow { juce::String path; int entry = -1, count = 1; bool pack = false; };
// Value-only navigation; amp/cab adapters provide metadata and loading policy.
inline std::vector<NavigationRow> visibleRows(const std::set<juce::String>& folders,
    const std::vector<NavigationItem>& items, BrowseMode mode, const juce::String& current,
    const juce::String& search)
{
    std::vector<NavigationRow> rows;
    const auto query = search.trim();
    std::map<juce::String, juce::String> titles;
    for (const auto& item : items) titles.emplace(item.path, item.title);
    const auto words = juce::StringArray::fromTokens(query, " ", "\"");
    const auto matches = [&](const juce::String& value) {
        for (const auto& word : words) if (!value.containsIgnoreCase(word)) return false;
        return true;
    };
    const auto addFolder = [&](const juce::String& path) {
        int count = 0;
        for (const auto& item : items) if (item.path.startsWith(path + "/")) ++count;
        if (count > 0) rows.push_back({path, -1, count, true});
    };
    if (query.isNotEmpty()) {
        for (const auto& path : folders) if (matches(path)) addFolder(path);
        for (const auto& item : items) if (matches(item.searchable)) rows.push_back({item.path, item.sourceIndex, 1, false});
    } else if (mode == BrowseMode::all) {
        std::set<juce::String> packs;
        for (const auto& item : items) {
            const auto parent = parentOf(item.path);
            if (parent.isEmpty()) rows.push_back({item.path, item.sourceIndex, 1, false});
            else packs.insert(parent);
        }
        for (const auto& path : packs) addFolder(path);
    } else {
        if (mode == BrowseMode::folder)
            for (const auto& path : folders) if (parentOf(path) == current) addFolder(path);
        for (const auto& item : items)
            if ((mode == BrowseMode::favorites && item.favorite)
                || (mode == BrowseMode::folder && parentOf(item.path) == current))
                rows.push_back({item.path, item.sourceIndex, 1, false});
    }
    const auto title = [&](const NavigationRow& row) {
        if (!row.pack) if (auto found = titles.find(row.path); found != titles.end()) return found->second;
        return leafOf(row.path);
    };
    std::sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
        if (a.pack != b.pack) return a.pack;
        const auto comparison = title(a).compareIgnoreCase(title(b));
        return comparison ? comparison < 0 : a.path < b.path;
    });
    return rows;
}
}
