#include "../src/i18n.h"
#include <windows.h>
#include <sddl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
using i18n::Language;

void check(LSTATUS code, const char* message) {
    if (code != ERROR_SUCCESS) throw std::system_error(static_cast<int>(code), std::system_category(), message);
}
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

struct Key {
    HKEY value = nullptr;
    ~Key() { if (value) RegCloseKey(value); }
    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;
    Key() = default;
};

// Run this executable as a separate process: the predefined-key override is
// process-wide. The test root is unique and an existing root is never reused.
struct IsolatedCurrentUser {
    Key real_root, isolated_root, restricted_key;
    std::wstring path;
    std::vector<BYTE> original_dacl;
    bool created = false, overridden = false, restricted = false;

    void setup() {
        check(RegOpenCurrentUser(KEY_READ | KEY_WRITE, &real_root.value), "open real current-user root");
        path = L"Software\\PWSwitch-i18n-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
        DWORD disposition = 0;
        check(RegCreateKeyExW(real_root.value, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &isolated_root.value, &disposition), "create isolated root");
        if (disposition != REG_CREATED_NEW_KEY) throw std::runtime_error("isolated root already exists; refusing to use it");
        created = true;
        check(RegOverridePredefKey(HKEY_CURRENT_USER, isolated_root.value), "override current user for this process");
        overridden = true;
    }
    void deny_value_writes() {
        check(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, READ_CONTROL | WRITE_DAC, &restricted_key.value), "open isolated application key");
        DWORD size = 0;
        LSTATUS status = RegGetKeySecurity(restricted_key.value, DACL_SECURITY_INFORMATION, nullptr, &size);
        if (status != ERROR_INSUFFICIENT_BUFFER || !size) throw std::runtime_error("cannot size isolated DACL");
        original_dacl.resize(size);
        check(RegGetKeySecurity(restricted_key.value, DACL_SECURITY_INFORMATION, reinterpret_cast<PSECURITY_DESCRIPTOR>(original_dacl.data()), &size), "read isolated DACL");
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        // Deny only KEY_SET_VALUE; allow reading and restoring the ACL. This is
        // applied to the isolated child, never to the real application key.
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(D;;0x00000002;;;WD)(A;;KA;;;WD)", SDDL_REVISION_1, &descriptor, nullptr))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "build isolated DACL");
        status = RegSetKeySecurity(restricted_key.value, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor);
        LocalFree(descriptor);
        check(status, "restrict isolated application key");
        restricted = true;
    }
    void cleanup() {
        if (restricted) {
            check(RegSetKeySecurity(restricted_key.value, DACL_SECURITY_INFORMATION, reinterpret_cast<PSECURITY_DESCRIPTOR>(original_dacl.data())), "restore isolated DACL");
            restricted = false;
        }
        if (overridden) {
            check(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr), "restore predefined HKCU");
            overridden = false;
        }
        if (created) {
            check(RegDeleteTreeW(real_root.value, path.c_str()), "remove isolated registry tree");
            created = false;
        }
    }
    ~IsolatedCurrentUser() {
        if (restricted && RegSetKeySecurity(restricted_key.value, DACL_SECURITY_INFORMATION, reinterpret_cast<PSECURITY_DESCRIPTOR>(original_dacl.data())) != ERROR_SUCCESS)
            std::cerr << "Could not restore isolated test key DACL\n";
        if (overridden && RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS)
            std::cerr << "Could not restore predefined HKCU for this test process\n";
        if (created && RegDeleteTreeW(real_root.value, path.c_str()) != ERROR_SUCCESS)
            std::cerr << "Could not remove isolated test registry tree\n";
    }
};

void write_value(DWORD type, const void* data, DWORD bytes) {
    Key key;
    check(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, KEY_SET_VALUE, &key.value), "open isolated language key");
    check(RegSetValueExW(key.value, L"Language", 0, type, static_cast<const BYTE*>(data), bytes), "write isolated language value");
}
void expect_value(const wchar_t* name, const wchar_t* expected) {
    Key key;
    check(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, KEY_QUERY_VALUE, &key.value), "read isolated application key");
    wchar_t actual[32]{}; DWORD type = 0, bytes = sizeof(actual);
    check(RegQueryValueExW(key.value, name, nullptr, &type, reinterpret_cast<BYTE*>(actual), &bytes), "read isolated value");
    require(type == REG_SZ && std::wstring(actual) == expected, "unexpected persisted value");
}
void expect_system() {
    require(i18n::preference() == Language::system, "invalid or missing setting did not use system preference");
    require(i18n::current() == i18n::resolve(GetUserDefaultUILanguage()), "system preference did not resolve from UI language");
}
}

int main() {
    try {
        IsolatedCurrentUser registry;
        registry.setup();
        i18n::initialize();
        expect_system();
        i18n::select(Language::english);
        expect_value(L"Language", L"en");
        i18n::set_for_testing(Language::spanish);
        i18n::initialize();
        require(i18n::preference() == Language::english && i18n::current() == Language::english, "English preference did not survive reload");
        i18n::select(Language::spanish);
        expect_value(L"Language", L"es");
        i18n::set_for_testing(Language::english);
        i18n::initialize();
        require(i18n::preference() == Language::spanish && i18n::current() == Language::spanish, "Spanish preference did not survive reload");

        Key key;
        check(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PWSwitch", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key.value), "open isolated application key");
        const wchar_t neighbor[] = L"keep";
        check(RegSetValueExW(key.value, L"Neighbor", 0, REG_SZ, reinterpret_cast<const BYTE*>(neighbor), sizeof(neighbor)), "write neighbor value");
        i18n::select(Language::system);
        DWORD type = 0, bytes = 0;
        require(RegQueryValueExW(key.value, L"Language", nullptr, &type, nullptr, &bytes) == ERROR_FILE_NOT_FOUND, "system choice did not remove language value");
        expect_value(L"Neighbor", L"keep");
        expect_system();

        const DWORD wrong_type = 7;
        write_value(REG_DWORD, &wrong_type, sizeof(wrong_type));
        i18n::initialize(); expect_system();
        const wchar_t unknown[] = L"zz";
        write_value(REG_SZ, unknown, sizeof(unknown));
        i18n::initialize(); expect_system();
        const wchar_t malformed[] = {L'e', L'n'};
        write_value(REG_SZ, malformed, sizeof(malformed));
        i18n::initialize(); expect_system();

        i18n::select(Language::english);
        registry.deny_value_writes();
        bool denied = false;
        try { i18n::select(Language::spanish); }
        catch (const std::system_error& ex) { denied = ex.code().value() == ERROR_ACCESS_DENIED; }
        require(denied, "restricted isolated key did not reject preference write");
        require(i18n::preference() == Language::english && i18n::current() == Language::english, "failed write changed the current language");
        expect_value(L"Language", L"en");
        expect_value(L"Neighbor", L"keep");
        registry.cleanup();
        std::cout << "isolated i18n registry tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "isolated i18n registry test failed: " << ex.what() << '\n';
        return 1;
    }
}
