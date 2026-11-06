#define NOMINMAX
#include <windows.h>
#include <string>

namespace {
// Only menu selection is simulated. All editor, language, registry and power
// reads use production code. The sequence cannot select a power command.
BOOL WINAPI language_test_menu(HMENU menu, UINT, int, int, int, HWND, const RECT*) {
    static unsigned call = 0;
    const UINT wanted = call++ == 0 ? 3103 : 3101; // Spanish, then English
    for (int position = 0, count = GetMenuItemCount(menu); position < count; ++position) {
        HMENU submenu = GetSubMenu(menu, position);
        if (!submenu) continue;
        MENUITEMINFOW item{}; item.cbSize = sizeof(item); item.fMask = MIIM_STATE;
        if (GetMenuItemInfoW(submenu, wanted, FALSE, &item) && !(item.fState & (MFS_DISABLED | MFS_GRAYED))) return wanted;
    }
    return 0;
}
}

#define TrackPopupMenu language_test_menu
#define wWinMain production_wWinMain
#include "../src/main.cpp"
#undef wWinMain
#undef TrackPopupMenu

namespace {
struct TestRegistry {
    HKEY real_root = nullptr, isolated_root = nullptr;
    std::wstring path;
    bool created = false, overridden = false;

    bool setup() {
        wchar_t token[33]{};
        if (GetEnvironmentVariableW(L"PWSWITCH_LANGUAGE_TEST_ID", token, 33) != 32) return false;
        for (wchar_t c : std::wstring(token))
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return false;
        if (RegOpenCurrentUser(KEY_READ | KEY_WRITE, &real_root) != ERROR_SUCCESS) return false;
        path = L"Software\\PWSwitch-language-ui-" + std::wstring(token);
        DWORD disposition = 0;
        if (RegCreateKeyExW(real_root, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &isolated_root, &disposition) != ERROR_SUCCESS) return false;
        if (disposition != REG_CREATED_NEW_KEY) return false;
        created = true;
        if (RegSetValueExW(isolated_root, L"TestOwner", 0, REG_SZ, reinterpret_cast<const BYTE*>(token), 33 * sizeof(wchar_t)) != ERROR_SUCCESS) return false;
        if (RegOverridePredefKey(HKEY_CURRENT_USER, isolated_root) != ERROR_SUCCESS) return false;
        overridden = true;
        return true;
    }
    bool cleanup() {
        if (overridden) {
            if (RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS) return false;
            overridden = false;
        }
        if (created) {
            if (RegDeleteTreeW(real_root, path.c_str()) != ERROR_SUCCESS) return false;
            created = false;
        }
        return true;
    }
    ~TestRegistry() {
        cleanup();
        if (isolated_root) RegCloseKey(isolated_root);
        if (real_root) RegCloseKey(real_root);
    }
};
}

int WINAPI wWinMain(HINSTANCE application_instance, HINSTANCE previous, PWSTR command, int show) {
    TestRegistry registry;
    if (!registry.setup()) return 80; // Fail closed before production initialization.
    int result = production_wWinMain(application_instance, previous, command, show);
    if (!registry.cleanup()) return 81;
    return result;
}
