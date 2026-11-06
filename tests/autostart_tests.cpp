#include "../src/autostart.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

struct Fake final : autostart::Storage {
    autostart::Entry value; bool fail = false, fail_write = false, fail_remove = false;
    autostart::Entry read() override { if (fail) throw std::runtime_error("read failed"); return value; }
    void write(const autostart::Entry& entry) override { if (fail || fail_write) throw std::runtime_error("write failed"); value = entry; }
    void remove() override { if (fail || fail_remove) throw std::runtime_error("remove failed"); value = {}; }
};
void require(bool condition, const char* error) { if (!condition) throw std::runtime_error(error); }
autostart::Entry text(const std::wstring& value) {
    autostart::Entry e; e.exists = true; e.type = REG_SZ; e.data.resize((value.size() + 1) * sizeof(wchar_t));
    std::memcpy(e.data.data(), value.c_str(), e.data.size()); return e;
}
struct TemporaryKey {
    std::wstring path; bool created = false;
    ~TemporaryKey() { if (created) RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); }
};
int main() {
    try {
        Fake f; const std::wstring path = L"C:\\Program Files\\PWSwitch\\PWSwitch.exe";
        auto initial = autostart::inspect(f, path);
        require(initial.state == autostart::State::disabled, "empty entry must be disabled");
        autostart::change(f, initial, path, true);
        require(f.value.data == text(L"\"C:\\Program Files\\PWSwitch\\PWSwitch.exe\"").data, "enabled command must quote exact executable");
        auto enabled = autostart::inspect(f, path);
        require(enabled.state == autostart::State::enabled, "current path must be enabled");
        autostart::change(f, enabled, path, false);
        require(!f.value.exists, "disable must remove entry");
        f.value = text(L"\"C:\\Old\\PWSwitch.exe\"");
        auto moved = autostart::inspect(f, path);
        require(moved.state == autostart::State::moved && moved.target == L"C:\\Old\\PWSwitch.exe", "old path must be visible");
        autostart::change(f, moved, path, true);
        require(autostart::inspect(f, path).state == autostart::State::enabled, "repair must use current executable");
        f.value = text(L"\"C:\\Old\\PWSwitch.exe\""); moved = autostart::inspect(f, path);
        autostart::change(f, moved, path, false);
        require(!f.value.exists, "confirmed old path removal failed");
        f.value = text(L"\"C:\\Other\\app.exe\" --some-option");
        auto foreign = autostart::inspect(f, path);
        require(foreign.state == autostart::State::foreign, "foreign command must be identified");
        auto foreign_value = f.value;
        try { autostart::change(f, foreign, path, true); throw std::runtime_error("foreign overwrite accepted"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) != "foreign overwrite accepted", "foreign overwritten"); }
        require(f.value.data == foreign_value.data, "foreign command changed");
        f.value = text(L"\"C:\\Old\\PWSwitch.exe\""); f.value.type = REG_EXPAND_SZ;
        require(autostart::inspect(f, path).state == autostart::State::foreign, "unexpected registry type must not be owned");
        f.value = text(L"\"C:\\Old\\PWSwitch.exe\""); f.value.data.pop_back();
        require(autostart::inspect(f, path).state == autostart::State::foreign, "malformed data must not be owned");
        f.value = {}; auto stale = autostart::inspect(f, path); f.value = foreign_value;
        try { autostart::change(f, stale, path, true); throw std::runtime_error("concurrent change accepted"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) != "concurrent change accepted", "concurrent change overwritten"); }
        require(f.value.data == foreign_value.data, "concurrent command changed");
        f.value = {}; auto empty = autostart::inspect(f, path);
        std::wstring long_path = L"C:\\" + std::wstring(260, L'a') + L"\\PWSwitch.exe";
        try { autostart::change(f, empty, long_path, true); throw std::runtime_error("long path accepted"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) != "long path accepted", "long path accepted"); }
        require(!f.value.exists, "long path was written");
        f.fail_write = true;
        try { autostart::change(f, empty, path, true); throw std::runtime_error("write failure accepted"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) == "write failed", "write error hidden"); }
        f.fail_write = false;
        f.value = text(L"\"C:\\Program Files\\PWSwitch\\PWSwitch.exe\"");
        enabled = autostart::inspect(f, path); f.fail_remove = true;
        try { autostart::change(f, enabled, path, false); throw std::runtime_error("remove failure accepted"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) == "remove failed", "remove error hidden"); }
        require(f.value.exists, "failed removal changed entry"); f.fail_remove = false;
        f.fail = true;
        try { (void)autostart::inspect(f, path); throw std::runtime_error("read failure accepted"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) == "read failed", "read error hidden"); }
        std::wstring temporary_path = L"Software\\PWSwitch-autostart-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
        {
            TemporaryKey cleanup{temporary_path}; HKEY key = nullptr; DWORD disposition = 0;
            LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, temporary_path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key, &disposition);
            require(status == ERROR_SUCCESS, "temporary registry key creation failed");
            RegCloseKey(key);
            require(disposition == REG_CREATED_NEW_KEY, "temporary registry key already existed");
            cleanup.created = true;
            autostart::RegistryStorage real(HKEY_CURRENT_USER, temporary_path);
            auto before = autostart::inspect(real, path);
            require(before.state == autostart::State::disabled, "new test key is not empty");
            autostart::change(real, before, path, true);
            auto saved = autostart::inspect(real, path);
            require(saved.state == autostart::State::enabled && saved.entry.data == text(L"\"C:\\Program Files\\PWSwitch\\PWSwitch.exe\"").data,
                    "registry write did not preserve quoted command");
            autostart::change(real, saved, path, false);
            require(autostart::inspect(real, path).state == autostart::State::disabled, "registry entry removal failed");
        }
        HKEY check_key = nullptr;
        LSTATUS cleanup_status = RegOpenKeyExW(HKEY_CURRENT_USER, temporary_path.c_str(), 0, KEY_READ, &check_key);
        if (check_key) RegCloseKey(check_key);
        require(cleanup_status == ERROR_FILE_NOT_FOUND, "temporary registry key cleanup failed");
        std::cout << "autostart tests passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
