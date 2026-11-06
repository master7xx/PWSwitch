#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include <vector>
#include "../src/power.h"
#include "../src/i18n.h"

namespace {
LRESULT selected_option = 0;
HWND fake_ac = reinterpret_cast<HWND>(1), fake_dc = reinterpret_cast<HWND>(2), fake_list = reinterpret_cast<HWND>(3),
     fake_description = reinterpret_cast<HWND>(4), fake_ac_apply = reinterpret_cast<HWND>(5), fake_dc_apply = reinterpret_cast<HWND>(6);
bool fail_first_insert = false, failed_insert = false;
bool ac_enabled = true, dc_enabled = true;
int writes = 0;
int language_changes = 0;
std::wstring texts[7];
std::vector<std::wstring> ac_choices, dc_choices;
size_t id(HWND window) { return reinterpret_cast<size_t>(window); }
LRESULT WINAPI fake_send_message(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == LB_GETCURSEL || message == LB_GETITEMDATA) return 0;
    if (message == CB_GETCURSEL) return selected_option;
    if (message == CB_RESETCONTENT) {
        (window == fake_ac ? ac_choices : dc_choices).clear(); return 0;
    }
    if (message == CB_ADDSTRING) {
        if (window == fake_ac && fail_first_insert && !failed_insert) { failed_insert = true; return CB_ERRSPACE; }
        auto& choices = window == fake_ac ? ac_choices : dc_choices;
        choices.emplace_back(reinterpret_cast<const wchar_t*>(lp));
        return static_cast<LRESULT>(choices.size() - 1);
    }
    if (message == CB_SETCURSEL) { selected_option = static_cast<LRESULT>(wp); return selected_option; }
    return 0;
}
BOOL WINAPI fake_enable_window(HWND window, BOOL enabled) {
    if (window == fake_ac_apply) ac_enabled = enabled != FALSE;
    if (window == fake_dc_apply) dc_enabled = enabled != FALSE;
    return TRUE;
}
BOOL WINAPI fake_set_text(HWND window, LPCWSTR text) { if (id(window) < 7) texts[id(window)] = text ? text : L""; return TRUE; }
int WINAPI fake_text_length(HWND window) { return id(window) < 7 ? static_cast<int>(texts[id(window)].size()) : 0; }
int WINAPI fake_get_text(HWND window, LPWSTR text, int count) {
    if (count <= 0) return 0;
    std::wstring value = id(window) < 7 ? texts[id(window)] : L"";
    int n = std::min(static_cast<int>(value.size()), count - 1);
    std::copy_n(value.c_str(), n, text); text[n] = L'\0'; return n;
}
BOOL WINAPI fake_is_window(HWND window) { return window != nullptr; }
int WINAPI fake_message_box(HWND, LPCWSTR, LPCWSTR, UINT) { return IDNO; }
}
namespace power {
DWORD fake_read_value(const GUID&, const GUID&, const GUID&, bool) { return 42; }
void fake_write_value(const GUID&, const GUID&, const GUID&, bool, DWORD) { ++writes; }
}
namespace i18n {
void test_select(Language language) { ++language_changes; set_for_testing(language); }
}

// Test the production editor validation without creating windows or changing settings.
#define SendMessageW fake_send_message
#define EnableWindow fake_enable_window
#define SetWindowTextW fake_set_text
#define GetWindowTextLengthW fake_text_length
#define GetWindowTextW fake_get_text
#define read_value fake_read_value
#define write_value fake_write_value
#define IsWindow fake_is_window
#define MessageBoxW fake_message_box
#define select test_select
#include "../src/main.cpp"
#undef SendMessageW
#undef EnableWindow
#undef SetWindowTextW
#undef GetWindowTextLengthW
#undef GetWindowTextW
#undef read_value
#undef write_value
#undef IsWindow
#undef MessageBoxW
#undef select

int main() {
    i18n::set_for_testing(i18n::Language::russian);
    power::Setting setting{};
    setting.options.push_back({0, L"Индекс 0 — название недоступно", false});
    setting.options.push_back({1, L"Известный вариант", true});
    DWORD value = 99;
    selected_option = 0;
    if (input_value(nullptr, setting, value)) return 1;
    selected_option = 1;
    if (!input_value(nullptr, setting, value) || value != 1) return 2;
    setting.options = {{0, L"First choice", true}, {1, L"Second choice", true}};
    editor_settings = {setting};
    setting_list = fake_list; description = fake_description;
    ac_input = fake_ac; dc_input = fake_dc; ac_apply = fake_ac_apply; dc_apply = fake_dc_apply;
    fail_first_insert = true; selected_option = CB_ERR;
    update_inputs();
    if (current_readable || ac_enabled || dc_enabled) return 3;
    if (texts[id(fake_description)].find(L"Не удалось заполнить список значений") == std::wstring::npos) return 4;
    selected_option = 0;
    apply_editor(true);
    if (writes != 0) return 5;
    editor = reinterpret_cast<HWND>(7);
    texts[id(fake_ac)] = L"unsaved";
    language_changes = 0;
    select_language(i18n::Language::english);
    if (language_changes != 0 || i18n::preference() != i18n::Language::russian || texts[id(fake_ac)] != L"unsaved") return 6;
    const GUID balanced = {0x381b4222,0xf694,0x41f0,{0x96,0x85,0xff,0x5b,0xb2,0x60,0xdf,0x2e}};
    undo_caption_state = {UndoCaption::Kind::display, {{balanced, L"fixture-plan-017"}}, balanced, {}, {}};
    i18n::set_for_testing(i18n::Language::english);
    if (undo_caption_text().find(L"fixture-plan-017") == std::wstring::npos || undo_caption_text().find(L"display") == std::wstring::npos) return 7;
    i18n::set_for_testing(i18n::Language::spanish);
    if (undo_caption_text().find(L"fixture-plan-017") == std::wstring::npos || undo_caption_text().find(L"pantalla") == std::wstring::npos) return 8;
    return 0;
}
