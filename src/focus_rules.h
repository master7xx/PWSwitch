#pragma once
#include "power.h"
#include <optional>
#include <string>
#include <vector>

namespace focus_rules {
struct Rule {
    GUID scheme{};            // plan to activate when this rule matches
    std::wstring path;        // full executable path (preferred) or process name
    std::wstring title;       // window title substring (empty = any title)
};
struct Ruleset {
    bool enabled = false;
    bool has_fallback = false;
    GUID fallback{};          // plan used when no rule matches (only if has_fallback)
    std::vector<Rule> rules;  // first match wins, top to bottom
};

// Foreground window description used for matching.
struct Foreground {
    std::wstring path;   // full executable path
    std::wstring exe;    // file name only, e.g. "wow.exe"
    std::wstring title;  // window title
};

// Match foreground info against rules. Returns the plan to activate, or nullopt
// when nothing matches AND no fallback is configured.
std::optional<GUID> match(const Ruleset& rules, const Foreground& foreground);
bool is_match(const Rule& rule, const Foreground& foreground);

// Load from / save to HKCU\Software\PWSwitch\Rules. Missing or malformed data
// yields an empty, disabled ruleset. Any error is reported as std::exception.
Ruleset load();
void save(const Ruleset& rules);

// Pick the "economical / saving" plan as the default fallback if one exists
// among the provided schemes; otherwise the first scheme. Returns false when
// the list is empty.
bool default_fallback(const std::vector<power::Item>& schemes, GUID& out);
}
