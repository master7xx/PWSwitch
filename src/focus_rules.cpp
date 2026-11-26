#include "focus_rules.h"
#include <system_error>
#include <windows.h>

namespace focus_rules {
namespace {
constexpr wchar_t subkey[] = L"Software\\PWSwitch\\Rules";
void check(LSTATUS code) { if (code != ERROR_SUCCESS) throw std::system_error(static_cast<int>(code), std::system_category()); }
struct Key { HKEY value = nullptr; ~Key() { if (value) RegCloseKey(value); } };

// Case-insensitive substring match, ordinal (Windows-style) comparison.
bool contains(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        if (CompareStringOrdinal(haystack.c_str() + i, static_cast<int>(needle.size()),
                needle.c_str(), static_cast<int>(needle.size()), TRUE) == CSTR_EQUAL)
            return true;
    }
    return false;
}

std::wstring exe_name(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

// Build a stable registry value name for a rule index (0..n-1):
// "Rule0", "Rule1", ... Each value is REG_SZ with fields separated by '\t'.
std::wstring value_name(size_t index) { return L"Rule" + std::to_wstring(index); }

std::wstring guid_string(const GUID& id) {
    wchar_t text[40] = {};
    swprintf_s(text, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", id.Data1, id.Data2, id.Data3,
        id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3], id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    return text;
}
bool parse_guid(const std::wstring& text, GUID& out) {
    // Accept "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" or bare form.
    std::wstring s = text;
    if (s.size() >= 2 && s.front() == L'{' && s.back() == L'}') s = s.substr(1, s.size() - 2);
    unsigned d1 = 0, d2 = 0, d3 = 0;
    int b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, b7 = 0;
    if (swscanf_s(s.c_str(), L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            &d1, &d2, &d3, &b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7) != 11) return false;
    out.Data1 = d1; out.Data2 = static_cast<unsigned short>(d2); out.Data3 = static_cast<unsigned short>(d3);
    out.Data4[0] = static_cast<unsigned char>(b0); out.Data4[1] = static_cast<unsigned char>(b1);
    out.Data4[2] = static_cast<unsigned char>(b2); out.Data4[3] = static_cast<unsigned char>(b3);
    out.Data4[4] = static_cast<unsigned char>(b4); out.Data4[5] = static_cast<unsigned char>(b5);
    out.Data4[6] = static_cast<unsigned char>(b6); out.Data4[7] = static_cast<unsigned char>(b7);
    return true;
}

// Cap a stored rule's serialized size to avoid unbounded registry values.
enum { kMaxRuleBytes = 16 * 1024 };
}  // namespace

bool is_match(const Rule& rule, const Foreground& foreground) {
    if (rule.path.empty()) return false;
    bool path_ok =
        // Full-path rule: match either the exact path or the trailing exe name.
        contains(foreground.path, rule.path) ||
        contains(foreground.exe, exe_name(rule.path));
    if (!path_ok) return false;
    return contains(foreground.title, rule.title);
}

std::optional<GUID> match(const Ruleset& rules, const Foreground& foreground) {
    if (!rules.enabled) return std::nullopt;
    for (const auto& rule : rules.rules) {
        if (is_match(rule, foreground)) return rule.scheme;
    }
    if (rules.has_fallback) return rules.fallback;
    return std::nullopt;
}

Ruleset load() {
    Ruleset result;
    Key key; LSTATUS code = RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &key.value);
    if (code == ERROR_FILE_NOT_FOUND) return result;
    check(code);
    // enabled / has_fallback as DWORD REG_DWORD (0 or 1).
    auto read_dword = [&](const wchar_t* name, DWORD& out) -> bool {
        DWORD type = 0, bytes = sizeof(out);
        LSTATUS st = RegQueryValueExW(key.value, name, nullptr, &type, reinterpret_cast<BYTE*>(&out), &bytes);
        if (st == ERROR_FILE_NOT_FOUND) return false;
        check(st);
        if (type != REG_DWORD || bytes != sizeof(out)) return false;
        return true;
    };
    DWORD enabled = 0, has_fallback = 0;
    read_dword(L"Enabled", enabled); result.enabled = enabled != 0;
    read_dword(L"HasFallback", has_fallback); result.has_fallback = has_fallback != 0;

    wchar_t fallback_text[64] = {};
    DWORD ftype = 0, fbytes = sizeof(fallback_text);
    code = RegQueryValueExW(key.value, L"Fallback", nullptr, &ftype, reinterpret_cast<BYTE*>(fallback_text), &fbytes);
    if (code == ERROR_SUCCESS && ftype == REG_SZ && fbytes >= sizeof(wchar_t) &&
        fbytes <= sizeof(fallback_text) && fbytes % sizeof(wchar_t) == 0 &&
        fallback_text[fbytes / sizeof(wchar_t) - 1] == L'\0' && parse_guid(fallback_text, result.fallback))
        result.has_fallback = has_fallback != 0;

    // Fallback plan is stored as its GUID; a rule is the same shape.
    for (DWORD i = 0; i < 256; ++i) {
        wchar_t name[32] = {};
        swprintf_s(name, L"Rule%lu", i);
        wchar_t text[2048] = {};
        DWORD type = 0, bytes = sizeof(text);
        LSTATUS st = RegQueryValueExW(key.value, name, nullptr, &type, reinterpret_cast<BYTE*>(text), &bytes);
        if (st == ERROR_FILE_NOT_FOUND) break;
        if (st == ERROR_MORE_DATA) continue;
        check(st);
        if (type != REG_SZ || bytes < sizeof(wchar_t) || bytes > sizeof(text) || bytes % sizeof(wchar_t)) continue;
        std::wstring value(text, bytes / sizeof(wchar_t) - 1);
        // Fields: guid<TAB>path<TAB>title
        size_t t1 = value.find(L'\t'), t2 = value.find(L'\t', t1 == std::wstring::npos ? 0 : t1 + 1);
        if (t1 == std::wstring::npos || t2 == std::wstring::npos) continue;
        Rule rule{};
        if (!parse_guid(value.substr(0, t1), rule.scheme)) continue;
        rule.path = value.substr(t1 + 1, t2 - t1 - 1);
        rule.title = value.substr(t2 + 1);
        result.rules.push_back(std::move(rule));
    }
    return result;
}

void save(const Ruleset& rules) {
    Key key; DWORD disposition = 0;
    check(RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key.value, &disposition));
    // Write flags. Delete legacy values first so stale RuleN entries vanish.
    for (DWORD i = 0; i < 256; ++i) {
        wchar_t name[32] = {};
        swprintf_s(name, L"Rule%lu", i);
        RegDeleteValueW(key.value, name);
    }
    RegDeleteValueW(key.value, L"Fallback");
    DWORD enabled = rules.enabled ? 1 : 0, has_fallback = rules.has_fallback ? 1 : 0;
    check(RegSetValueExW(key.value, L"Enabled", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled)));
    check(RegSetValueExW(key.value, L"HasFallback", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&has_fallback), sizeof(has_fallback)));
    if (rules.has_fallback) {
        const wchar_t* text = guid_string(rules.fallback).c_str();
        check(RegSetValueExW(key.value, L"Fallback", 0, REG_SZ, reinterpret_cast<const BYTE*>(text),
            (static_cast<DWORD>(std::wcslen(text)) + 1) * sizeof(wchar_t)));
    }
    for (size_t i = 0; i < rules.rules.size() && i < 256; ++i) {
        const auto& rule = rules.rules[i];
        std::wstring value = guid_string(rule.scheme) + L"\t" + rule.path + L"\t" + rule.title;
        if (value.size() > kMaxRuleBytes / sizeof(wchar_t)) continue;
        wchar_t name[32] = {};
        swprintf_s(name, L"Rule%zu", i);
        check(RegSetValueExW(key.value, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
            (static_cast<DWORD>(value.size()) + 1) * sizeof(wchar_t)));
    }
}

bool default_fallback(const std::vector<power::Item>& schemes, GUID& out) {
    // Prefer the plan whose localized-friendly name is close to "economical"/
    // "saving". Built-in saving plans are named in the OS language; we check a
    // few common substrings case-insensitively. Otherwise pick the first plan.
    if (schemes.empty()) return false;
    static const wchar_t* hints[] = {L"экономи", L"эконом", L"saving", L"saver",
        L"economy", L"power saver", L"ahorro", L"ahorrar"};
    for (const auto& plan : schemes) {
        for (const wchar_t* hint : hints) {
            if (contains(plan.name, hint)) { out = plan.id; return true; }
        }
    }
    out = schemes.front().id;
    return true;
}
}  // namespace focus_rules
