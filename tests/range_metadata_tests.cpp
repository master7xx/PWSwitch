#include "../src/power.h"
#include <powrprof.h>
#include <iostream>

namespace {
const GUID video = {0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
const GUID display = {0x3c0bc021,0xc8a8,0x4e07,{0xa9,0x73,0x6b,0x14,0xcb,0xcb,0x2b,0x7e}};
enum class IncrementReply { real, denied, missing, zero } reply = IncrementReply::real;
GUID option_group{}, option_setting{};
bool deny_option_name = false;
enum class PossibleReply { real, empty, denied, zero_size, second_read_error } possible_reply = PossibleReply::real;
DWORD WINAPI read_possible(HKEY root, const GUID* group, const GUID* setting, ULONG* type, ULONG index, UCHAR* buffer, DWORD* bytes) {
    if (IsEqualGUID(*group, option_group) && IsEqualGUID(*setting, option_setting)) {
        if (possible_reply == PossibleReply::empty && index == 0) return ERROR_FILE_NOT_FOUND;
        if (index == 1 && possible_reply == PossibleReply::denied) return ERROR_ACCESS_DENIED;
        if (index == 1 && possible_reply == PossibleReply::zero_size) { *bytes = 0; return ERROR_SUCCESS; }
        if (index == 1 && buffer && possible_reply == PossibleReply::second_read_error) return ERROR_ACCESS_DENIED;
    }
    return PowerReadPossibleValue(root, group, setting, type, index, buffer, bytes);
}
DWORD WINAPI read_option_name(HKEY root, const GUID* group, const GUID* setting, ULONG index, UCHAR* buffer, DWORD* bytes) {
    if (deny_option_name && IsEqualGUID(*group, option_group) && IsEqualGUID(*setting, option_setting) && index == 0)
        return ERROR_ACCESS_DENIED;
    return PowerReadPossibleFriendlyName(root, group, setting, index, buffer, bytes);
}
DWORD WINAPI read_increment(HKEY root, const GUID* group, const GUID* setting, DWORD* value) {
    if (IsEqualGUID(*group, video) && IsEqualGUID(*setting, display)) {
        if (reply == IncrementReply::denied) { *value = 17; return ERROR_ACCESS_DENIED; }
        if (reply == IncrementReply::missing) return ERROR_FILE_NOT_FOUND;
        if (reply == IncrementReply::zero) { *value = 0; return ERROR_SUCCESS; }
    }
    return PowerReadValueIncrement(root, group, setting, value);
}
}

// Replace only the read-only API boundary; exercise production metadata loading.
// Build this file with power_transaction.cpp, without a second power.cpp object.
#define PowerReadValueIncrement read_increment
#define PowerReadPossibleFriendlyName read_option_name
#define PowerReadPossibleValue read_possible
#include "../src/power.cpp"
#undef PowerReadValueIncrement
#undef PowerReadPossibleFriendlyName
#undef PowerReadPossibleValue

power::Setting display_metadata(const GUID& scheme) {
    for (const auto& setting : power::settings(scheme, video))
        if (IsEqualGUID(setting.id, display)) return setting;
    throw std::runtime_error("Display timeout metadata unavailable");
}
int main() {
    try {
        i18n::set_for_testing(i18n::Language::russian);
        const GUID scheme = power::active_scheme();
        bool found_choice = false;
        for (const auto& group : power::groups(scheme)) {
            for (const auto& setting : power::settings(scheme, group.id)) {
                if (setting.options.size() < 2 || setting.options[0].name.empty()) continue;
                option_group = group.id; option_setting = setting.id;
                deny_option_name = true;
                for (const auto& altered : power::settings(scheme, group.id)) {
                    if (!IsEqualGUID(altered.id, option_setting)) continue;
                    if (altered.options.size() < 2 || altered.options[0].name.find(L"название недоступно") == std::wstring::npos ||
                        altered.options[0].name.find(L"0") == std::wstring::npos || altered.options[0].named ||
                        altered.options[1].name != setting.options[1].name || altered.options[1].named != setting.options[1].named)
                        throw std::runtime_error("Unavailable option name must identify the index and preserve other choices");
                    found_choice = true;
                    break;
                }
                deny_option_name = false;
                break;
            }
            if (found_choice) break;
        }
        if (!found_choice) throw std::runtime_error("No enumerated parameter found for option-name test");
        for (auto scenario : {PossibleReply::empty, PossibleReply::denied, PossibleReply::zero_size, PossibleReply::second_read_error}) {
            possible_reply = scenario;
            bool found = false;
            for (const auto& setting : power::settings(scheme, option_group)) {
                if (!IsEqualGUID(setting.id, option_setting)) continue;
                found = true;
                if (scenario == PossibleReply::empty) {
                    if (!setting.options.empty() || setting.description.find(L"Windows не предоставила доступные значения") == std::wstring::npos)
                        throw std::runtime_error("Empty options were not explained");
                } else if (setting.options.size() != 1 || setting.options[0].value != 0 ||
                           setting.description.find(L"список может быть неполным") == std::wstring::npos)
                    throw std::runtime_error("Partial options were not retained and explained");
                break;
            }
            if (!found) throw std::runtime_error("Target enumerated setting disappeared");
        }
        possible_reply = PossibleReply::real;
        const auto baseline = display_metadata(scheme);
        if (!baseline.range || baseline.increment == 0 || !baseline.options.empty())
            throw std::runtime_error("Real display range is not a valid positive-step control case");
        const DWORD ac = power::read_value(scheme, video, display, true);
        const DWORD dc = power::read_value(scheme, video, display, false);
        int failures = 0;
        for (auto scenario : {IncrementReply::denied, IncrementReply::missing, IncrementReply::zero}) {
            reply = scenario;
            const auto setting = display_metadata(scheme);
            if (setting.increment != 0) {
                std::cerr << "FAIL: unavailable range step was replaced with an editable step; scenario=" << static_cast<int>(scenario) << '\n';
                ++failures;
            }
            if (!setting.range || !setting.options.empty() || setting.minimum != baseline.minimum || setting.maximum != baseline.maximum)
                throw std::runtime_error("Unavailable step lost range metadata or enabled enumerated fallback");
            if (setting.description == baseline.description) {
                std::cerr << "FAIL: unavailable step has no user-visible explanation; scenario=" << static_cast<int>(scenario) << '\n';
                ++failures;
            }
            if (power::read_value(scheme, video, display, true) != ac || power::read_value(scheme, video, display, false) != dc)
                throw std::runtime_error("Values changed during read-only metadata test");
        }
        if (!IsEqualGUID(scheme, power::active_scheme())) throw std::runtime_error("Active scheme changed during read-only test");
        if (failures) return 1;
        std::cout << "range metadata tests passed\n";
        return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
