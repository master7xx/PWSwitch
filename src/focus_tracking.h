#pragma once
#include "focus_rules.h"
#include "power.h"
#include <string>

namespace focus_tracking {
// Describes the currently focused window. Exposed so the engine can be tested
// with a fake foreground and fake power backend.
struct WindowInfo {
    std::wstring path;   // full executable path, e.g. D:\...\WoW.exe
    std::wstring exe;    // file name only, e.g. "WoW.exe"
    std::wstring title;  // window title
    bool valid = false;  // false when no foreground window could be identified
};

// Thin abstraction over the parts of Windows used by the engine, so unit tests
// can substitute fakes.
struct Backend {
    virtual ~Backend() = default;
    virtual WindowInfo foreground() = 0;
    virtual GUID active() = 0;
    virtual void activate(const GUID& scheme) = 0;
};

// Real implementation on the current foreground window using Win32 APIs.
WindowInfo current_foreground();

// Real Backend that reads the foreground and activates the active power plan.
class Engine final {
public:
    explicit Engine(Backend& backend) : backend_(&backend) {}

    // Re-evaluate the foreground against the rules and switch the power plan if
    // the matched plan differs from the currently active one. Returns the plan
    // that was activated via the backend, or nullopt when nothing switched.
    std::optional<GUID> evaluate(const focus_rules::Ruleset& rules);

private:
    Backend* backend_;
};

// Build a descriptor from a foreground window handle. Handles the common
// cases (UWP-style hosts, elevated processes) without throwing for a missing
// window. Exposed for tests that want to validate the Win32 path.
WindowInfo from_hwnd(HWND hwnd);
}
