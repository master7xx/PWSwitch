#include "../src/focus_rules.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        const GUID a = {0x11111111,0x1111,0x1111,{1,1,1,1,1,1,1,1}};
        const GUID b = {0x22222222,0x2222,0x2222,{2,2,2,2,2,2,2,2}};
        focus_rules::Foreground fg;
        fg.path = L"D:\\games\\WoW\\Wow.exe";
        fg.exe = L"Wow.exe";
        fg.title = L"World of Warcraft";

        focus_rules::Rule rule;
        rule.scheme = a;
        rule.path = L"D:\\games\\WoW\\Wow.exe";
        rule.title = L"World of";
        if (!focus_rules::is_match(rule, fg)) throw std::runtime_error("full-path + title should match");

        // Exe-name rule should match a different install path.
        rule.path = L"Wow.exe";
        if (!focus_rules::is_match(rule, fg)) throw std::runtime_error("exe-name should match");

        // Mismatched title should not match.
        rule.title = L"Steam";
        if (focus_rules::is_match(rule, fg)) throw std::runtime_error("wrong title should not match");

        // Empty title means any title.
        rule.title.clear();
        if (!focus_rules::is_match(rule, fg)) throw std::runtime_error("empty title should match any");

        // Disabled ruleset returns nullopt, even when a rule would match.
        focus_rules::Ruleset rules;
        rules.enabled = false;
        rules.rules.push_back(rule);
        if (focus_rules::match(rules, fg)) throw std::runtime_error("disabled ruleset should not match");

        // Enabled + first-match wins: two rules that both match, earliest wins.
        rules.enabled = true;
        rules.rules.insert(rules.rules.begin(), {b, L"Wow.exe", L""});
        auto got = focus_rules::match(rules, fg);
        if (!got || !IsEqualGUID(*got, b)) throw std::runtime_error("first matching rule should win");

        // No match -> fallback.
        rules.rules.clear();
        rules.rules.push_back({b, L"Steam.exe", L""});
        rules.has_fallback = true;
        rules.fallback = a;
        got = focus_rules::match(rules, fg);
        if (!got || !IsEqualGUID(*got, a)) throw std::runtime_error("fallback should be used when no rule matches");

        // No match and no fallback -> nullopt.
        rules.has_fallback = false;
        if (focus_rules::match(rules, fg)) throw std::runtime_error("no fallback should yield nullopt");

        std::cout << "focus_rules tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
