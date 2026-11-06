#include "labels.h"
#include "i18n.h"

namespace labels {
namespace {
std::wstring compact_name(const std::wstring& name) { return name.size() > 22 ? name.substr(0, 21) + L"…" : name; }
bool same_name(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
std::wstring id_text(const GUID& id, bool full) {
    wchar_t buffer[40]{};
    if (full) swprintf_s(buffer, L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", id.Data1, id.Data2, id.Data3,
        id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3], id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    else swprintf_s(buffer, L"%08lX", id.Data1);
    return buffer;
}
}
std::vector<std::wstring> all(const std::vector<power::Item>& plans, bool compact) {
    std::vector<std::wstring> bases, result;
    bases.reserve(plans.size()); result.reserve(plans.size());
    for (const auto& plan : plans) {
        auto name = i18n::scheme_name(plan.id, plan.name);
        bases.push_back(compact ? compact_name(name) : name);
    }
    for (size_t i = 0; i < plans.size(); ++i) {
        bool duplicate = false;
        for (size_t j = 0; j < plans.size(); ++j) if (i != j && same_name(bases[i], bases[j])) { duplicate = true; break; }
        std::wstring candidate = bases[i];
        if (duplicate) candidate += L" [" + id_text(plans[i].id, false) + L"]";
        auto collides = [&](const std::wstring& text) {
            for (size_t j = 0; j < bases.size(); ++j) if (i != j && same_name(text, bases[j])) return true;
            for (const auto& used : result) if (same_name(text, used)) return true;
            return false;
        };
        if (collides(candidate)) {
            std::wstring full = bases[i] + L" [" + id_text(plans[i].id, true) + L"]";
            candidate = full;
            for (unsigned suffix = 2; collides(candidate); ++suffix) candidate = full + L" #" + std::to_wstring(suffix);
        }
        result.push_back(std::move(candidate));
    }
    return result;
}
std::wstring scheme(const std::vector<power::Item>& plans, const GUID& id, bool compact) {
    auto rendered = all(plans, compact);
    for (size_t i = 0; i < plans.size(); ++i) if (IsEqualGUID(plans[i].id, id)) return rendered[i];
    return std::wstring(i18n::text(L"Схема [")) + id_text(id, true) + L"]";
}
}
