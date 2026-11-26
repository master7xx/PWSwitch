#include "../src/focus_tracking.h"
#include <iostream>
#include <stdexcept>

namespace {
struct FakeBackend final : focus_tracking::Backend {
    focus_tracking::WindowInfo info;
    GUID active_scheme{};
    GUID activated{};
    bool did_activate = false;
    focus_tracking::WindowInfo foreground() override { return info; }
    GUID active() override { return active_scheme; }
    void activate(const GUID& scheme) override { activated = scheme; did_activate = true; }
};
}

int main() {
    try {
        const GUID plan_a = {0x11111111,0x1111,0x1111,{1,1,1,1,1,1,1,1}};
        const GUID plan_b = {0x22222222,0x2222,0x2222,{2,2,2,2,2,2,2,2}};
        FakeBackend backend;
        focus_tracking::Engine engine(backend);

        focus_rules::Ruleset rules;
        rules.enabled = true;
        rules.rules.push_back({plan_a, L"D:\\games\\WoW\\Wow.exe", L"World of"});

        backend.info = {L"D:\\games\\WoW\\Wow.exe", L"Wow.exe", L"World of Warcraft", true};
        backend.active_scheme = GUID{};  // different from match
        auto target = engine.evaluate(rules);
        if (!target || !IsEqualGUID(*target, plan_a)) throw std::runtime_error("should activate matched plan");
        if (!backend.did_activate || !IsEqualGUID(backend.activated, plan_a)) throw std::runtime_error("backend should have activated plan_a");

        // Same plan already active -> no activation.
        backend.did_activate = false;
        backend.active_scheme = plan_a;
        if (engine.evaluate(rules)) throw std::runtime_error("should not activate when already active");
        if (backend.did_activate) throw std::runtime_error("must not call activate when already active");

        // Disabled ruleset -> no activation.
        backend.did_activate = false;
        backend.active_scheme = GUID{};
        rules.enabled = false;
        if (engine.evaluate(rules)) throw std::runtime_error("disabled ruleset should not activate");
        if (backend.did_activate) throw std::runtime_error("must not call activate when disabled");

        // Invalid foreground -> no activation.
        rules.enabled = true;
        backend.info.valid = false;
        if (engine.evaluate(rules)) throw std::runtime_error("invalid foreground should not activate");
        if (backend.did_activate) throw std::runtime_error("must not call activate for invalid foreground");

        std::cout << "focus_tracking tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
