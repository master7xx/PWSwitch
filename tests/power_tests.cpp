#include "../src/power.h"
#include <windows.h>
#include <powrprof.h>
#include <iostream>

int main() {
    try {
        const auto active = power::active_scheme();
        const auto plans = power::schemes();
        bool found = false;
        for (const auto& p : plans) if (IsEqualGUID(p.id, active)) found = true;
        if (!found || plans.empty()) throw std::runtime_error("active scheme missing");
        const auto groups = power::groups(active);
        if (groups.empty()) throw std::runtime_error("no settings groups");
        size_t count = 0, hidden = 0, choices = 0, nonsequential = 0, distinct_payloads = 0;
        for (const auto& group : groups) {
            auto entries = power::settings(active, group.id);
            count += entries.size();
            for (const auto& entry : entries) {
                if (entry.hidden) ++hidden;
                if (!entry.options.empty()) ++choices;
                for (size_t i = 0; i < entry.options.size(); ++i) {
                    if (entry.options[i].value != i) { ++nonsequential; break; }
                    DWORD raw = 0, type = 0, size = sizeof(raw);
                    if (PowerReadPossibleValue(nullptr, &entry.group, &entry.id, &type, static_cast<ULONG>(i),
                        reinterpret_cast<UCHAR*>(&raw), &size) == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(raw) && raw != i) {
                        ++distinct_payloads;
                        if (entry.options[i].value != i) throw std::runtime_error("choice index confused with payload");
                    }
                }
            }
        }
        if (count == 0) throw std::runtime_error("no settings enumerated");
        if (nonsequential != 0) throw std::runtime_error("choice uses payload instead of setting index");
        std::cout << "settings: " << count << ", hidden: " << hidden << ", choices: " << choices << ", distinct payloads: " << distinct_payloads << '\n';
        const GUID video = {0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
        const GUID display = {0x3c0bc021,0xc8a8,0x4e07,{0xa9,0x73,0x6b,0x14,0xcb,0xcb,0x2b,0x7e}};
        auto video_settings = power::settings(active, video);
        bool found_display = false;
        for (const auto& setting : video_settings) {
            if (IsEqualGUID(setting.id, display)) {
                found_display = true;
                std::cout << "display options=" << setting.options.size() << " range=" << setting.range << " increment=" << setting.increment << '\n';
                if (!setting.options.empty()) throw std::runtime_error("numeric range misidentified as enumerated choice");
                if (setting.name.empty() || !setting.range || setting.minimum != 0 || setting.increment == 0)
                    throw std::runtime_error("display metadata missing");
            }
        }
        if (!found_display) throw std::runtime_error("display setting missing");
        const GUID no_group = {0xfea3413e,0x7e05,0x4911,{0x9a,0x71,0x70,0x03,0x31,0xf1,0xc2,0x94}};
        const GUID console_lock = {0x0e796bdb,0x100d,0x47d6,{0xa2,0xd5,0xf7,0xd2,0xda,0xa5,0x1f,0x51}};
        bool found_lock = false;
        for (const auto& setting : power::settings(active, no_group)) {
            if (!IsEqualGUID(setting.id, console_lock)) continue;
            found_lock = true;
            std::cout << "lock options=" << setting.options.size() << " range=" << setting.range << "\n";
            if (setting.options.size() != 2 || setting.options[0].name == L"0" || setting.options[1].name == L"1")
                throw std::runtime_error("localized option labels missing");
        }
        if (!found_lock) std::cout << "console lock unavailable on this system; label check skipped\n";
        auto old = power::read_value(active, video, display, true);
        GUID* duplicate = nullptr;
        DWORD result = PowerDuplicateScheme(nullptr, &active, &duplicate);
        if (result != ERROR_SUCCESS || !duplicate) throw std::runtime_error("duplicate scheme failed");
        GUID copy = *duplicate;
        LocalFree(duplicate);
        try {
            power::write_value(copy, video, display, true, 1234);
            if (power::read_value(copy, video, display, true) != 1234) throw std::runtime_error("write/read mismatch");
            power::write_both(copy, video, display, 4321);
            if (power::read_value(copy, video, display, true) != 4321 || power::read_value(copy, video, display, false) != 4321)
                throw std::runtime_error("both-source write mismatch");
            const GUID pci = {0x501a4d13,0x42af,0x4429,{0x9f,0xd1,0xa8,0x21,0x8c,0x26,0x8e,0x20}};
            const GUID aspm = {0xee12f906,0xd277,0x404b,{0xb6,0xda,0xe5,0xfa,0x1a,0x57,0x6d,0xf5}};
            for (const auto& setting : power::settings(copy, pci)) if (IsEqualGUID(setting.id, aspm) && setting.options.size() >= 2) {
                DWORD before = power::read_value(copy, pci, aspm, true);
                DWORD new_index = before == 0 ? 1 : 0;
                power::write_value(copy, pci, aspm, true, new_index);
                if (power::read_value(copy, pci, aspm, true) != new_index)
                    throw std::runtime_error("enumerated setting round trip failed");
                break;
            }
            if (power::read_value(active, video, display, true) != old) throw std::runtime_error("original modified");
        } catch (...) {
            PowerDeleteScheme(nullptr, &copy);
            throw;
        }
        if (PowerDeleteScheme(nullptr, &copy) != ERROR_SUCCESS) throw std::runtime_error("cleanup failed");
        if (!IsEqualGUID(active, power::active_scheme())) throw std::runtime_error("active scheme changed");
        std::cout << "power tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "power tests failed: " << e.what() << '\n';
        return 1;
    }
}
