#include "power.h"
#include "icon_style.h"
#include "icon_art.h"
#include "menu_art.h"
#include "undo.h"
#include "labels.h"
#include "system_commands.h"
#include "autostart.h"
#include "i18n.h"
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dbt.h>
#include <algorithm>
#include <map>
#include <limits>
#include <system_error>
#include <vector>

namespace {
constexpr UINT tray_message = WM_APP + 1;
constexpr UINT commit_scheme_message = WM_APP + 2;
constexpr int ID_SCHEME = 100, ID_LIST = 101, ID_HIDDEN = 102, ID_AC = 103, ID_DC = 104,
    ID_APPLY_AC = 105, ID_APPLY_DC = 106, ID_REFRESH = 107;
constexpr UINT MENU_PLAN = 1000, MENU_DISPLAY = 2000, MENU_SLEEP = 2100, MENU_ADVANCED = 3000,
    MENU_WINDOWS = 3001, MENU_EXIT = 3002, MENU_UNDO = 3003, MENU_AUTOSTART = 3004, MENU_AUTOSTART_REMOVE = 3005,
    MENU_LANGUAGE = 3100;
const DWORD intervals[] = {0, 60, 300, 600, 900, 1800, 3600, 7200};
const wchar_t* interval_labels[] = {L"Никогда", L"1 минута", L"5 минут", L"10 минут", L"15 минут", L"30 минут", L"1 час", L"2 часа"};
const GUID VIDEO = {0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
const GUID DISPLAY = {0x3c0bc021,0xc8a8,0x4e07,{0xa9,0x73,0x6b,0x14,0xcb,0xcb,0x2b,0x7e}};
const GUID SLEEP = {0x238c9fa8,0x0aad,0x41ed,{0x83,0xf4,0x97,0xbe,0x24,0x2c,0x8f,0x20}};
const GUID STANDBY = {0x29f6c1db,0x86da,0x48c5,{0x9f,0xdb,0xf2,0xb6,0x7b,0x1f,0x44,0xda}};
const GUID ACTIVE_SCHEME_EVENT = {0x31f9f286,0x5084,0x42fe,{0xb7,0x20,0x2b,0x02,0x64,0x99,0x37,0x63}};
HINSTANCE instance;
HWND main_window, editor, scheme_box, scheme_label, refresh_button, setting_list, hidden_box, description, ac_label, dc_label, ac_input, dc_input, ac_apply, dc_apply;
HFONT ui_font;
NOTIFYICONDATAW icon{};
UINT taskbar_created;
UINT show_settings_message;
HICON custom_icon;
HPOWERNOTIFY scheme_notification;
GUID icon_scheme{};
std::wstring icon_scheme_name;
bool icon_has_scheme = false;
bool icon_registered = false;
bool icon_unavailable = false;
std::vector<power::Item> menu_plans, editor_plans;
std::vector<power::Setting> editor_settings;
struct NamedSetting { std::wstring group; power::Setting setting; };
std::vector<NamedSetting> cached_settings;
GUID editor_scheme{};
DWORD displayed_ac = 0, displayed_dc = 0;
bool current_readable = false;
bool unreadable_ac_draft = false, unreadable_dc_draft = false;
bool editor_modal = false;
bool tray_interaction = false;
bool show_settings_pending = false;
bool scheme_dropdown_open = false, scheme_selection_deferred = false, scheme_dropdown_canceled = false;
int last_setting_row = -1, last_scheme_index = -1;
bool last_hidden = false;
std::wstring baseline_ac, baseline_dc;
LRESULT baseline_ac_selection = CB_ERR, baseline_dc_selection = CB_ERR;
undo::Journal undo_history;
std::vector<power::Item> localized_plans(const std::vector<power::Item>& plans) {
    auto result = plans;
    for (auto& plan : result) plan.name = i18n::scheme_name(plan.id, plan.name);
    return result;
}
struct UndoCaption {
    enum class Kind { none, scheme, ac, dc, display, sleep } kind = Kind::none;
    std::vector<power::Item> plans;
    GUID first{}, second{};
    std::wstring detail;
};
UndoCaption undo_caption_state;
std::wstring undo_caption_text() {
    auto plans = localized_plans(undo_caption_state.plans);
    auto plan = [&]() { return labels::scheme(plans, undo_caption_state.first, true); };
    switch (undo_caption_state.kind) {
    case UndoCaption::Kind::scheme: return i18n::text(L"схема · ") + plan() + L" ← " + labels::scheme(plans, undo_caption_state.second, true);
    case UndoCaption::Kind::ac: return i18n::text(L"сеть · ") + plan() + undo_caption_state.detail;
    case UndoCaption::Kind::dc: return i18n::text(L"батарея · ") + plan() + undo_caption_state.detail;
    case UndoCaption::Kind::display: return std::wstring(i18n::text(L"экран")) + i18n::text(L", сеть и батарея · ") + plan();
    case UndoCaption::Kind::sleep: return std::wstring(i18n::text(L"сон")) + i18n::text(L", сеть и батарея · ") + plan();
    default: return undo_history.description();
    }
}
struct WindowsUndoBackend : undo::Backend {
    GUID active() override { return power::active_scheme(); }
    DWORD read(const GUID& scheme, const GUID& group, const GUID& setting, bool ac) override { return power::read_value(scheme, group, setting, ac); }
    void write(const GUID& scheme, const GUID& group, const GUID& setting, bool ac, DWORD value) override { power::write_value(scheme, group, setting, ac, value); }
    void activate(const GUID& scheme) override { power::activate(scheme); }
};
int scroll_x = 0, scroll_y = 0;

void refresh_icon(bool force = false) {
    GUID active = power::active_scheme();
    std::wstring name;
    for (const auto& scheme : power::schemes()) if (IsEqualGUID(scheme.id, active)) { name = scheme.name; break; }
    if (name.empty()) name = i18n::text(L"Неизвестная схема");
    if (!force && icon_has_scheme && IsEqualGUID(icon_scheme, active) && icon_scheme_name == name) return;
    auto selected_style = icon_style::style(icon_style::classify(active, name));
    HICON replacement = icon_art::make_icon(selected_style);
    if (!replacement) return;
    NOTIFYICONDATAW update = icon;
    update.hIcon = replacement;
    std::wstring tooltip = L"PWSwitch: " + selected_style.label + L" — " + name;
    wcsncpy_s(update.szTip, tooltip.c_str(), _TRUNCATE);
    update.uFlags = NIF_ICON | NIF_TIP;
    if (!Shell_NotifyIconW(NIM_MODIFY, &update)) { icon_registered = false; DestroyIcon(replacement); return; }
    HICON previous = custom_icon;
    custom_icon = replacement;
    icon.hIcon = replacement;
    wcsncpy_s(icon.szTip, update.szTip, _TRUNCATE);
    icon_scheme = active;
    icon_scheme_name = name;
    icon_has_scheme = true;
    icon_unavailable = false;
    if (previous) DestroyIcon(previous);
}
void refresh_icon_safely() {
    try { refresh_icon(); }
    catch (const std::exception&) {
        icon_has_scheme = false;
        if (icon_unavailable || !icon_registered) return;
        HICON replacement = icon_art::make_icon(icon_style::style(icon_style::Level::unknown));
        if (!replacement) return;
        NOTIFYICONDATAW update = icon;
        update.hIcon = replacement; update.uFlags = NIF_ICON | NIF_TIP;
        wcscpy_s(update.szTip, i18n::text(L"PWSwitch: не удалось определить активную схему"));
        if (!Shell_NotifyIconW(NIM_MODIFY, &update)) { icon_registered = false; DestroyIcon(replacement); return; }
        HICON previous = custom_icon; custom_icon = replacement; icon.hIcon = replacement;
        wcsncpy_s(icon.szTip, update.szTip, _TRUNCATE);
        icon_unavailable = true;
        if (previous) DestroyIcon(previous);
    }
}

std::wstring exception_text(const std::exception& ex) {
    auto system = dynamic_cast<const std::system_error*>(&ex);
    if (system) return power::error_text(static_cast<DWORD>(system->code().value()));
    int n = MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, nullptr, 0);
    if (!n) return i18n::text(L"Неизвестная ошибка");
    std::vector<wchar_t> buffer(n);
    MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, buffer.data(), n);
    return i18n::translate(buffer.data());
}
void report(HWND owner, const std::exception& ex) {
    auto message = exception_text(ex);
    MessageBoxW(owner, message.c_str(), L"PWSwitch", MB_OK | MB_ICONERROR);
}
struct EditorModalGuard {
    bool previous;
    EditorModalGuard() : previous(editor_modal) { editor_modal = true; }
    ~EditorModalGuard() { editor_modal = previous; }
};
int editor_message(HWND owner, const std::wstring& text, UINT flags) {
    EditorModalGuard guard;
    return MessageBoxW(owner, text.c_str(), L"PWSwitch", flags);
}
HWND control(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0, DWORD ex = 0) {
    HWND result = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(result, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    return result;
}
std::wstring window_text(HWND window) {
    int n = GetWindowTextLengthW(window);
    std::vector<wchar_t> buffer(static_cast<size_t>(n) + 1);
    GetWindowTextW(window, buffer.data(), n + 1);
    return buffer.data();
}
std::wstring seconds_text(DWORD value) {
    if (!value) return i18n::text(L"Никогда");
    if (value % 60 == 0) return std::to_wstring(value / 60) + i18n::text(L" мин.");
    return std::to_wstring(value) + i18n::text(L" сек.");
}
bool parse_number(const std::wstring& text, DWORD& value) {
    if (text.empty() || text[0] == L'-' || text[0] == L'+') return false;
    unsigned long long number = 0;
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') return false;
        number = number * 10 + static_cast<unsigned>(c - L'0');
        if (number > MAXDWORD) return false;
    }
    value = static_cast<DWORD>(number); return true;
}
void apply_if_active(const GUID& scheme) {
    if (IsEqualGUID(power::active_scheme(), scheme)) power::activate(scheme);
}
void set_both(const GUID& scheme, const GUID& group, const GUID& id, DWORD value, DWORD old_ac, DWORD old_dc, const std::wstring& action_label, const UndoCaption& caption) {
    try { power::write_both_checked(scheme, group, id, value, old_ac, old_dc); }
    catch (const power::partial_write_error&) { undo_history.clear(); throw; }
    undo::ValueChange change{};
    change.scheme = scheme; change.group = group; change.setting = id; change.ac = change.dc = true;
    change.before_ac = old_ac; change.before_dc = old_dc; change.after_ac = change.after_dc = value;
    undo_history.record_value(change, action_label);
    undo_caption_state = caption;
    try { apply_if_active(scheme); }
    catch (const std::exception& ex) {
        std::wstring message = i18n::text(L"Значения записаны, но применить схему не удалось. Проверьте параметры Windows.\r\n\r\n") + exception_text(ex);
        MessageBoxW(main_window, message.c_str(), L"PWSwitch", MB_OK | MB_ICONWARNING);
    }
}

struct Prompt { HWND edit; std::wstring initial; bool accepted = false; DWORD value = 0; };
LRESULT CALLBACK prompt_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto p = reinterpret_cast<Prompt*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        p = reinterpret_cast<Prompt*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p));
    }
    if (msg == WM_CREATE) {
        control(hwnd, L"STATIC", i18n::text(L"Время в минутах (0 — никогда):"), 0, 18, 15, 270, 24);
        p->edit = control(hwnd, L"EDIT", p->initial.c_str(), WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 18, 43, 270, 26, 11);
        control(hwnd, L"BUTTON", i18n::text(L"Применить"), BS_DEFPUSHBUTTON | WS_TABSTOP, 99, 82, 90, 30, IDOK);
        control(hwnd, L"BUTTON", i18n::text(L"Отмена"), WS_TABSTOP, 198, 82, 90, 30, IDCANCEL);
        SetFocus(p->edit); SendMessageW(p->edit, EM_SETSEL, 0, -1); return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        DWORD minutes;
        if (!parse_number(window_text(p->edit), minutes) || minutes > MAXDWORD / 60) {
            MessageBoxW(hwnd, i18n::text(L"Введите целое число минут от 0 до 71582788."), L"PWSwitch", MB_OK | MB_ICONWARNING); return 0;
        }
        p->accepted = true; p->value = minutes * 60; DestroyWindow(hwnd); return 0;
    }
    if ((msg == WM_COMMAND && LOWORD(wp) == IDCANCEL) || msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
bool ask_minutes(DWORD& result, bool has_current, DWORD current_seconds) {
    Prompt p;
    if (has_current && current_seconds % 60 == 0) p.initial = std::to_wstring(current_seconds / 60);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"PWSwitchPrompt", i18n::text(L"Своё время"), WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 324, 160, main_window, nullptr, instance, &p);
    if (!dialog) return false;
    EnableWindow(main_window, FALSE); ShowWindow(dialog, SW_SHOW);
    MSG msg;
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(main_window, TRUE); SetForegroundWindow(main_window);
    result = p.value; return p.accepted;
}

void update_inputs();
std::vector<NamedSetting> fetch_settings(const GUID& scheme) {
    std::vector<NamedSetting> fresh;
    for (const auto& group : power::groups(scheme))
        for (auto& setting : power::settings(scheme, group.id)) fresh.push_back({group.name, std::move(setting)});
    bool exists = false;
    for (const auto& plan : power::schemes()) if (IsEqualGUID(plan.id, scheme)) { exists = true; break; }
    if (!exists) throw std::runtime_error("Выбранная схема больше недоступна. Обновите список схем.");
    return fresh;
}
void load_settings(bool reload = true) {
    if (reload) cached_settings = fetch_settings(editor_scheme);
    current_readable = false;
    EnableWindow(ac_apply, FALSE); EnableWindow(dc_apply, FALSE);
    editor_settings.clear();
    SendMessageW(setting_list, LB_RESETCONTENT, 0, 0);
    bool show_hidden = SendMessageW(hidden_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
    int horizontal_extent = 0;
    HDC dc = GetDC(setting_list);
    HGDIOBJ old_font = dc ? SelectObject(dc, ui_font) : nullptr;
    for (const auto& item : cached_settings) {
            const auto& setting = item.setting;
            if (setting.hidden && !show_hidden) continue;
            std::wstring label = item.group + L"  ›  " + setting.name;
            if (setting.hidden) label = L"◆ " + label;
            if (dc) { SIZE size{}; if (GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size)) horizontal_extent = std::max(horizontal_extent, static_cast<int>(size.cx) + 12); }
            int row = static_cast<int>(SendMessageW(setting_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
            if (row >= 0) {
                SendMessageW(setting_list, LB_SETITEMDATA, row, static_cast<LPARAM>(editor_settings.size()));
                editor_settings.push_back(setting);
            }
    }
    if (dc) { SelectObject(dc, old_font); ReleaseDC(setting_list, dc); }
    SendMessageW(setting_list, LB_SETHORIZONTALEXTENT, horizontal_extent, 0);
    if (!editor_settings.empty()) SendMessageW(setting_list, LB_SETCURSEL, 0, 0);
    update_inputs();
}
void populate_scheme_box(const std::vector<std::wstring>& names, int selected) {
    auto fail = [&]() {
        SendMessageW(scheme_box, CB_RESETCONTENT, 0, 0);
        EnableWindow(scheme_box, FALSE); EnableWindow(setting_list, FALSE); EnableWindow(hidden_box, FALSE);
        current_readable = false;
        EnableWindow(ac_apply, FALSE); EnableWindow(dc_apply, FALSE);
        throw std::runtime_error("Не удалось заполнить список схем. Обновите окно настроек.");
    };
    SendMessageW(scheme_box, CB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < names.size(); ++i)
        if (SendMessageW(scheme_box, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(names[i].c_str())) != static_cast<LRESULT>(i)) fail();
    if (SendMessageW(scheme_box, CB_SETCURSEL, selected, 0) != selected) fail();
    EnableWindow(scheme_box, TRUE); EnableWindow(setting_list, TRUE); EnableWindow(hidden_box, TRUE);
}
void load_schemes() {
    GUID previous = editor_scheme;
    bool had_previous = !IsEqualGUID(previous, GUID{});
    auto fresh_plans = power::schemes();
    GUID active = power::active_scheme();
    int selected = -1;
    for (size_t i = 0; i < fresh_plans.size(); ++i) {
        const auto& plan = fresh_plans[i];
        if (IsEqualGUID(plan.id, had_previous ? previous : active)) selected = static_cast<int>(i);
    }
    if (selected < 0) for (size_t i = 0; i < fresh_plans.size(); ++i) if (IsEqualGUID(fresh_plans[i].id, active)) selected = static_cast<int>(i);
    if (selected < 0) throw std::runtime_error("Active scheme is not available");
    GUID selected_scheme = fresh_plans[selected].id;
    auto fresh_settings = fetch_settings(selected_scheme);
    editor_plans = std::move(fresh_plans);
    editor_scheme = selected_scheme;
    cached_settings = std::move(fresh_settings);
    auto plan_labels = labels::all(localized_plans(editor_plans), false);
    populate_scheme_box(plan_labels, selected);
    last_scheme_index = selected;
    load_settings(false);
}
const power::Setting* selected_setting() {
    LRESULT item = SendMessageW(setting_list, LB_GETCURSEL, 0, 0);
    if (item == LB_ERR) return nullptr;
    LRESULT index = SendMessageW(setting_list, LB_GETITEMDATA, item, 0);
    if (index < 0 || static_cast<size_t>(index) >= editor_settings.size()) return nullptr;
    return &editor_settings[static_cast<size_t>(index)];
}
struct InputListError : std::runtime_error { using std::runtime_error::runtime_error; };
void fill_input(HWND input, const power::Setting& setting, DWORD value) {
    SendMessageW(input, CB_RESETCONTENT, 0, 0);
    if (!setting.options.empty()) {
        int selected = -1;
        for (size_t i = 0; i < setting.options.size(); ++i) {
            const auto& opt = setting.options[i];
            LRESULT row = SendMessageW(input, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(opt.name.c_str()));
            if (row != static_cast<LRESULT>(i))
                throw InputListError("Не удалось заполнить список значений. Изменение параметра недоступно; попробуйте обновить список.");
            if (opt.value == value) selected = static_cast<int>(i);
        }
        if (selected >= 0) SendMessageW(input, CB_SETCURSEL, selected, 0);
        else SetWindowTextW(input, (i18n::text(L"Текущее: ") + std::to_wstring(value)).c_str());
    } else SetWindowTextW(input, std::to_wstring(value).c_str());
}
void update_inputs() {
    current_readable = false;
    unreadable_ac_draft = unreadable_dc_draft = false;
    const auto* setting = selected_setting();
    last_setting_row = static_cast<int>(SendMessageW(setting_list, LB_GETCURSEL, 0, 0));
    baseline_ac.clear(); baseline_dc.clear();
    baseline_ac_selection = baseline_dc_selection = CB_ERR;
    SetWindowTextW(description, L"");
    SetWindowTextW(ac_input, L""); SetWindowTextW(dc_input, L"");
    if (!setting) { EnableWindow(ac_apply, FALSE); EnableWindow(dc_apply, FALSE); return; }
    std::wstring info = setting->description;
    if (setting->hidden) info = i18n::text(L"◆ Windows обычно не показывает этот параметр.\r\n") + info;
    if (setting->range) {
        info += i18n::text(L"\r\nДиапазон: ") + std::to_wstring(setting->minimum) + L"–" + std::to_wstring(setting->maximum);
        if (setting->increment) info += i18n::text(L", шаг: ") + std::to_wstring(setting->increment);
        info += L" " + setting->units;
    }
    SetWindowTextW(description, info.c_str());
    try {
        displayed_ac = power::read_value(editor_scheme, setting->group, setting->id, true);
        displayed_dc = power::read_value(editor_scheme, setting->group, setting->id, false);
        fill_input(ac_input, *setting, displayed_ac); fill_input(dc_input, *setting, displayed_dc);
        baseline_ac = window_text(ac_input); baseline_dc = window_text(dc_input);
        baseline_ac_selection = SendMessageW(ac_input, CB_GETCURSEL, 0, 0);
        baseline_dc_selection = SendMessageW(dc_input, CB_GETCURSEL, 0, 0);
        current_readable = true;
    } catch (const InputListError& ex) { SetWindowTextW(description, (info + L"\r\n" + exception_text(ex)).c_str()); }
      catch (const std::exception& ex) { SetWindowTextW(description, (info + i18n::text(L"\r\nНе удалось прочитать значения: ") + exception_text(ex)).c_str()); }
    bool named_option = std::any_of(setting->options.begin(), setting->options.end(), [](const power::Option& option) { return option.named; });
    bool editable = current_readable && (named_option || (setting->range && setting->increment != 0));
    EnableWindow(ac_apply, editable); EnableWindow(dc_apply, editable);
}
bool has_pending_edits() {
    return unreadable_ac_draft || unreadable_dc_draft || window_text(ac_input) != baseline_ac || window_text(dc_input) != baseline_dc ||
        SendMessageW(ac_input, CB_GETCURSEL, 0, 0) != baseline_ac_selection ||
        SendMessageW(dc_input, CB_GETCURSEL, 0, 0) != baseline_dc_selection;
}
bool discard_pending_edits(HWND owner) {
    if (!has_pending_edits()) return true;
    return editor_message(owner, i18n::text(L"Есть неприменённые изменения. Отбросить их?"),
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDYES;
}
struct EditorDraft {
    bool ac = false, dirty = false;
    DWORD displayed = 0;
    LRESULT selection = CB_ERR, baseline_selection = CB_ERR;
    std::wstring text, baseline;
};
EditorDraft capture_draft(bool ac) {
    HWND input = ac ? ac_input : dc_input;
    EditorDraft draft{}; draft.ac = ac;
    draft.text = window_text(input);
    draft.baseline = ac ? baseline_ac : baseline_dc;
    draft.displayed = ac ? displayed_ac : displayed_dc;
    draft.selection = SendMessageW(input, CB_GETCURSEL, 0, 0);
    draft.baseline_selection = ac ? baseline_ac_selection : baseline_dc_selection;
    draft.dirty = draft.text != draft.baseline || draft.selection != draft.baseline_selection;
    return draft;
}
void restore_draft(const EditorDraft& draft, bool preserve_baseline) {
    if (!draft.dirty) return;
    HWND input = draft.ac ? ac_input : dc_input;
    if (draft.selection >= 0) SendMessageW(input, CB_SETCURSEL, draft.selection, 0);
    else SetWindowTextW(input, draft.text.c_str());
    if (!current_readable) {
        if (draft.ac) unreadable_ac_draft = true; else unreadable_dc_draft = true;
    }
    if (preserve_baseline || !current_readable) {
        if (draft.ac) { baseline_ac = draft.baseline; baseline_ac_selection = draft.baseline_selection; displayed_ac = draft.displayed; }
        else { baseline_dc = draft.baseline; baseline_dc_selection = draft.baseline_selection; displayed_dc = draft.displayed; }
    }
}
bool input_value(HWND input, const power::Setting& setting, DWORD& value) {
    if (!setting.options.empty()) {
        LRESULT index = SendMessageW(input, CB_GETCURSEL, 0, 0);
        if (index < 0 || static_cast<size_t>(index) >= setting.options.size() || !setting.options[static_cast<size_t>(index)].named) return false;
        value = setting.options[static_cast<size_t>(index)].value; return true;
    }
    if (!setting.range || !parse_number(window_text(input), value)) return false;
    return value >= setting.minimum && value <= setting.maximum && setting.increment && (value - setting.minimum) % setting.increment == 0;
}
void apply_editor(bool ac) {
    const auto* setting = selected_setting(); if (!setting || !current_readable) return;
    DWORD value;
    if (!input_value(ac ? ac_input : dc_input, *setting, value)) {
        editor_message(editor, i18n::text(L"Выберите вариант с доступным названием или введите число в указанном диапазоне с указанным шагом."), MB_OK | MB_ICONWARNING); return;
    }
    DWORD now = power::read_value(editor_scheme, setting->group, setting->id, ac);
    if (now != (ac ? displayed_ac : displayed_dc)) {
        auto ac_draft = capture_draft(true), dc_draft = capture_draft(false);
        if (ac) ac_draft.dirty = true; else dc_draft.dirty = true;
        std::wstring current = std::to_wstring(now);
        for (const auto& option : setting->options) if (option.value == now) { current = option.name; break; }
        if (setting->options.empty() && !setting->units.empty()) current += L" " + setting->units;
        update_inputs();
        restore_draft(ac_draft, !ac); restore_draft(dc_draft, ac);
        std::wstring message = i18n::text(L"Значение изменилось вне приложения. Текущее значение: ") + current +
            i18n::text(L".\r\nЧерновики сохранены. Проверьте значение и при необходимости нажмите «Применить» ещё раз.");
        editor_message(editor, message, MB_OK | MB_ICONWARNING);
        return;
    }
    if (now == value) {
        if (ac) { baseline_ac = window_text(ac_input); baseline_ac_selection = SendMessageW(ac_input, CB_GETCURSEL, 0, 0); }
        else { baseline_dc = window_text(dc_input); baseline_dc_selection = SendMessageW(dc_input, CB_GETCURSEL, 0, 0); }
        return;
    }
    auto other_draft = capture_draft(!ac);
    power::write_value(editor_scheme, setting->group, setting->id, ac, value);
    undo::ValueChange change{}; change.scheme = editor_scheme; change.group = setting->group; change.setting = setting->id;
    change.ac = ac; change.dc = !ac;
    if (ac) { change.before_ac = now; change.after_ac = value; }
    else { change.before_dc = now; change.after_dc = value; }
    std::wstring group_name;
    for (const auto& item : cached_settings) if (IsEqualGUID(item.setting.group, setting->group) && IsEqualGUID(item.setting.id, setting->id)) { group_name = item.group; break; }
    std::wstring action_label = std::wstring(ac ? i18n::text(L"сеть · ") : i18n::text(L"батарея · ")) + labels::scheme(localized_plans(editor_plans), editor_scheme, true) + L" · " + group_name + L" › " + setting->name;
    undo_history.record_value(change, action_label);
    undo_caption_state = {ac ? UndoCaption::Kind::ac : UndoCaption::Kind::dc, editor_plans, editor_scheme, {}, L" · " + group_name + L" › " + setting->name};
    std::wstring apply_error;
    try { apply_if_active(editor_scheme); }
    catch (const std::exception& ex) { apply_error = exception_text(ex); }
    update_inputs();
    restore_draft(other_draft, true);
    if (!apply_error.empty()) {
        std::wstring message = i18n::text(L"Значение записано, но применить схему не удалось. Данные обновлены.\r\n\r\n") + apply_error;
        editor_message(editor, message, MB_OK | MB_ICONWARNING);
    }
}
void update_scrollbars(HWND hwnd) {
    RECT client{}; GetClientRect(hwnd, &client);
    int old_x = scroll_x, old_y = scroll_y;
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0; info.nMax = 769; info.nPage = static_cast<UINT>(std::max(0L, client.right)); info.nPos = scroll_x;
    SetScrollInfo(hwnd, SB_HORZ, &info, TRUE); info.fMask = SIF_POS; GetScrollInfo(hwnd, SB_HORZ, &info); scroll_x = info.nPos;
    info = {sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0; info.nMax = 494; info.nPage = static_cast<UINT>(std::max(0L, client.bottom)); info.nPos = scroll_y;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE); info.fMask = SIF_POS; GetScrollInfo(hwnd, SB_VERT, &info); scroll_y = info.nPos;
    if (old_x != scroll_x || old_y != scroll_y) ScrollWindowEx(hwnd, old_x - scroll_x, old_y - scroll_y, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN | SW_INVALIDATE);
}
void scroll_editor(HWND hwnd, int bar, int action) {
    SCROLLINFO info{sizeof(info), SIF_ALL};
    if (!GetScrollInfo(hwnd, bar, &info)) return;
    int previous = info.nPos;
    switch (action) {
    case SB_LINEUP: info.nPos -= 30; break;
    case SB_LINEDOWN: info.nPos += 30; break;
    case SB_PAGEUP: info.nPos -= static_cast<int>(info.nPage); break;
    case SB_PAGEDOWN: info.nPos += static_cast<int>(info.nPage); break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION: info.nPos = info.nTrackPos; break;
    case SB_TOP: info.nPos = info.nMin; break;
    case SB_BOTTOM: info.nPos = info.nMax; break;
    default: return;
    }
    info.fMask = SIF_POS; SetScrollInfo(hwnd, bar, &info, TRUE); GetScrollInfo(hwnd, bar, &info);
    int delta = previous - info.nPos;
    if (bar == SB_HORZ) scroll_x = info.nPos; else scroll_y = info.nPos;
    ScrollWindowEx(hwnd, bar == SB_HORZ ? delta : 0, bar == SB_VERT ? delta : 0, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN | SW_INVALIDATE);
}
void reveal_editor_focus(HWND hwnd) {
    HWND focused = GetFocus();
    if (!focused || focused == hwnd) return;
    while (GetParent(focused) && GetParent(focused) != hwnd) focused = GetParent(focused);
    if (GetParent(focused) != hwnd) return;
    RECT bounds{}, client{};
    if (!GetWindowRect(focused, &bounds) || !GetClientRect(hwnd, &client)) return;
    MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<POINT*>(&bounds), 2);
    auto offset = [](int start, int end, int size) {
        if (start < 0) return start;
        if (end > size && end - start <= size) return end - size;
        return 0;
    };
    int old_x = scroll_x, old_y = scroll_y;
    SCROLLINFO info{sizeof(info), SIF_POS};
    info.nPos = scroll_x + offset(bounds.left, bounds.right, client.right);
    SetScrollInfo(hwnd, SB_HORZ, &info, TRUE); GetScrollInfo(hwnd, SB_HORZ, &info); scroll_x = info.nPos;
    info.nPos = scroll_y + offset(bounds.top, bounds.bottom, client.bottom);
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE); GetScrollInfo(hwnd, SB_VERT, &info); scroll_y = info.nPos;
    if (old_x != scroll_x || old_y != scroll_y)
        ScrollWindowEx(hwnd, old_x - scroll_x, old_y - scroll_y, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN | SW_INVALIDATE);
}
void commit_scheme_selection(HWND hwnd) {
    LRESULT i = SendMessageW(scheme_box, CB_GETCURSEL, 0, 0);
    if (i < 0 || static_cast<size_t>(i) >= editor_plans.size() || i == last_scheme_index) return;
    if (!discard_pending_edits(hwnd)) { SendMessageW(scheme_box, CB_SETCURSEL, last_scheme_index, 0); return; }
    try {
        GUID next = editor_plans[static_cast<size_t>(i)].id;
        auto fresh = fetch_settings(next);
        editor_scheme = next; cached_settings = std::move(fresh); last_scheme_index = static_cast<int>(i);
        load_settings(false);
    } catch (...) { SendMessageW(scheme_box, CB_SETCURSEL, last_scheme_index, 0); throw; }
}
LRESULT CALLBACK editor_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    try {
        if (msg == WM_CREATE) {
            scroll_x = scroll_y = 0;
            last_setting_row = last_scheme_index = -1; last_hidden = false;
            scheme_dropdown_open = scheme_selection_deferred = scheme_dropdown_canceled = false;
            scheme_label = control(hwnd, L"STATIC", i18n::text(L"Схема питания:"), 0, 14, 17, 120, 22);
            scheme_box = control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 132, 12, 520, 350, ID_SCHEME);
            refresh_button = control(hwnd, L"BUTTON", i18n::text(L"Обновить"), WS_TABSTOP, 662, 11, 95, 28, ID_REFRESH);
            hidden_box = control(hwnd, L"BUTTON", i18n::text(L"Показывать скрытые параметры Windows  (◆ — скрыт Windows)"), BS_AUTOCHECKBOX | WS_TABSTOP, 14, 48, 600, 25, ID_HIDDEN);
            setting_list = control(hwnd, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, 14, 80, 743, 240, ID_LIST, WS_EX_CLIENTEDGE);
            description = control(hwnd, L"EDIT", L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, 14, 330, 743, 80, 0, WS_EX_CLIENTEDGE);
            ac_label = control(hwnd, L"STATIC", i18n::text(L"От сети:"), 0, 14, 423, 100, 24);
            ac_input = control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, 118, 419, 480, 300, ID_AC);
            ac_apply = control(hwnd, L"BUTTON", i18n::text(L"Применить"), WS_TABSTOP, 611, 418, 146, 28, ID_APPLY_AC);
            dc_label = control(hwnd, L"STATIC", i18n::text(L"От батареи:"), 0, 14, 459, 100, 24);
            dc_input = control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, 118, 455, 480, 300, ID_DC);
            dc_apply = control(hwnd, L"BUTTON", i18n::text(L"Применить"), WS_TABSTOP, 611, 454, 146, 28, ID_APPLY_DC);
            load_schemes(); update_scrollbars(hwnd); return 0;
        }
        if (msg == WM_SIZE) { update_scrollbars(hwnd); return 0; }
        if (msg == commit_scheme_message) {
            if (scheme_selection_deferred) {
                scheme_selection_deferred = false;
                if (scheme_dropdown_canceled || editor_modal) SendMessageW(scheme_box, CB_SETCURSEL, last_scheme_index, 0);
                else commit_scheme_selection(hwnd);
                scheme_dropdown_canceled = false;
            }
            return 0;
        }
        if (msg == WM_HSCROLL || msg == WM_VSCROLL) {
            if (!lp) scroll_editor(hwnd, msg == WM_HSCROLL ? SB_HORZ : SB_VERT, LOWORD(wp));
            return 0;
        }
        if (msg == WM_COMMAND) {
            switch (LOWORD(wp)) {
            case ID_SCHEME: {
                UINT notification = HIWORD(wp);
                if (notification == CBN_DROPDOWN) { scheme_dropdown_open = true; scheme_dropdown_canceled = false; }
                else if (notification == CBN_SELENDCANCEL && scheme_dropdown_open) scheme_dropdown_canceled = true;
                else if (notification == CBN_CLOSEUP && scheme_dropdown_open) {
                    scheme_dropdown_open = false; scheme_selection_deferred = true;
                    PostMessageW(hwnd, commit_scheme_message, 0, 0);
                } else if (notification == CBN_SELCHANGE && !scheme_dropdown_open && !scheme_selection_deferred) commit_scheme_selection(hwnd);
                return 0;
            }
            case ID_LIST:
                if (HIWORD(wp) == LBN_SELCHANGE) {
                    int next = static_cast<int>(SendMessageW(setting_list, LB_GETCURSEL, 0, 0));
                    if (next == last_setting_row) return 0;
                    if (!discard_pending_edits(hwnd)) { SendMessageW(setting_list, LB_SETCURSEL, last_setting_row, 0); return 0; }
                    update_inputs();
                }
                return 0;
            case ID_HIDDEN: {
                bool next = SendMessageW(hidden_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (next == last_hidden) return 0;
                if (!discard_pending_edits(hwnd)) { SendMessageW(hidden_box, BM_SETCHECK, last_hidden ? BST_CHECKED : BST_UNCHECKED, 0); return 0; }
                last_hidden = next; load_settings(false); return 0;
            }
            case ID_REFRESH: if (discard_pending_edits(hwnd)) load_schemes(); return 0;
            case ID_APPLY_AC: apply_editor(true); return 0;
            case ID_APPLY_DC: apply_editor(false); return 0;
            case IDCANCEL: SendMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
            }
        }
        if (msg == WM_CLOSE) { if (discard_pending_edits(hwnd)) DestroyWindow(hwnd); return 0; }
        if (msg == WM_DESTROY) { editor = nullptr; return 0; }
    } catch (const std::exception& ex) { editor_message(hwnd, exception_text(ex), MB_OK | MB_ICONERROR); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void show_editor() {
    if (editor && IsWindow(editor)) { ShowWindow(editor, SW_RESTORE); SetForegroundWindow(editor); return; }
    editor_scheme = GUID{};
    RECT area{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    int width = std::min(790L, area.right - area.left), height = std::min(540L, area.bottom - area.top);
    editor = CreateWindowExW(0, L"PWSwitchEditor", i18n::text(L"PWSwitch — расширенные настройки"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_VSCROLL | WS_HSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, nullptr);
    if (!editor) throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
    ShowWindow(editor, SW_SHOW); UpdateWindow(editor);
}
void refresh_editor_language() {
    if (!editor || !IsWindow(editor)) return;
    const auto* selected = selected_setting();
    GUID group{}, setting{};
    bool had_selection = selected != nullptr;
    if (selected) { group = selected->group; setting = selected->id; }
    SetWindowTextW(editor, i18n::text(L"PWSwitch — расширенные настройки"));
    SetWindowTextW(scheme_label, i18n::text(L"Схема питания:"));
    SetWindowTextW(refresh_button, i18n::text(L"Обновить"));
    SetWindowTextW(hidden_box, i18n::text(L"Показывать скрытые параметры Windows  (◆ — скрыт Windows)"));
    SetWindowTextW(ac_label, i18n::text(L"От сети:"));
    SetWindowTextW(dc_label, i18n::text(L"От батареи:"));
    SetWindowTextW(ac_apply, i18n::text(L"Применить"));
    SetWindowTextW(dc_apply, i18n::text(L"Применить"));
    try {
        auto plan_labels = labels::all(localized_plans(editor_plans), false);
        populate_scheme_box(plan_labels, last_scheme_index);
        auto fresh = fetch_settings(editor_scheme);
        cached_settings = std::move(fresh);
        load_settings(false);
        if (had_selection) {
            for (size_t i = 0; i < editor_settings.size(); ++i) {
                if (!IsEqualGUID(editor_settings[i].group, group) || !IsEqualGUID(editor_settings[i].id, setting)) continue;
                for (LRESULT row = 0, count = SendMessageW(setting_list, LB_GETCOUNT, 0, 0); row < count; ++row) {
                    if (SendMessageW(setting_list, LB_GETITEMDATA, row, 0) != static_cast<LRESULT>(i)) continue;
                    SendMessageW(setting_list, LB_SETCURSEL, row, 0); update_inputs(); break;
                }
                break;
            }
        }
    } catch (const std::exception& ex) {
        if (IsWindowEnabled(scheme_box)) update_inputs();
        editor_message(editor, exception_text(ex), MB_OK | MB_ICONERROR);
    }
}
void select_language(i18n::Language language) {
    if (language == i18n::preference()) return;
    if (editor && IsWindow(editor) && !discard_pending_edits(editor)) return;
    i18n::select(language);
    icon_has_scheme = false; icon_unavailable = false;
    refresh_icon_safely();
    refresh_editor_language();
}
struct TimeoutSnapshot { bool valid = false; DWORD ac = 0, dc = 0; };
class MenuImages {
    std::map<unsigned long long, HBITMAP> images_;
public:
    ~MenuImages() { for (const auto& item : images_) if (item.second) DeleteObject(item.second); }
    void attach(HMENU menu, UINT item, bool by_position, menu_art::Kind kind, const icon_style::Style& style = icon_style::style(icon_style::Level::unknown)) {
        auto key = (static_cast<unsigned long long>(kind) << 32) | (kind == menu_art::Kind::scheme ? style.argb : 0);
        auto found = images_.find(key);
        if (found == images_.end()) {
            HBITMAP bitmap = menu_art::make_bitmap(kind, style);
            try { found = images_.emplace(key, bitmap).first; }
            catch (...) { if (bitmap) DeleteObject(bitmap); throw; }
        }
        if (!found->second) return;
        MENUITEMINFOW info{}; info.cbSize = sizeof(info); info.fMask = MIIM_BITMAP; info.hbmpItem = found->second;
        SetMenuItemInfoW(menu, item, by_position, &info);
    }
};
TimeoutSnapshot append_timeout_menu(HMENU menu, MenuImages& images, menu_art::Kind kind, const wchar_t* label, const GUID& group, const GUID& id, UINT base, const GUID& active) {
    DWORD ac, dc;
    try { ac = power::read_value(active, group, id, true); dc = power::read_value(active, group, id, false); }
    catch (const std::exception&) {
        std::wstring unavailable = std::wstring(label) + i18n::text(L": недоступно");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, unavailable.c_str());
        images.attach(menu, static_cast<UINT>(GetMenuItemCount(menu) - 1), true, kind);
        return {};
    }
    HMENU sub = CreatePopupMenu();
    if (!sub) throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
    std::wstring title = std::wstring(label) + L": " + (ac == dc ? seconds_text(ac) : i18n::text(L"сеть ") + seconds_text(ac) + i18n::text(L", батарея ") + seconds_text(dc));
    AppendMenuW(sub, MF_STRING | MF_GRAYED, 0, i18n::text(L"Применяется для сети и батареи"));
    for (UINT i = 0; i < _countof(intervals); ++i) {
        bool checked = ac == dc && ac == intervals[i];
        std::wstring caption = i18n::text(interval_labels[i]);
        if (checked) caption += L"\t✓";
        AppendMenuW(sub, MF_STRING | (checked ? MF_CHECKED : 0), base + i, caption.c_str());
        images.attach(sub, base + i, false, i == 0 ? menu_art::Kind::never : menu_art::Kind::clock);
    }
    AppendMenuW(sub, MF_SEPARATOR, 0, nullptr); AppendMenuW(sub, MF_STRING, base + 99, i18n::text(L"Своё время…"));
    images.attach(sub, base + 99, false, menu_art::Kind::custom);
    if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), title.c_str())) {
        DWORD code = GetLastError(); DestroyMenu(sub); throw std::system_error(static_cast<int>(code), std::system_category());
    }
    images.attach(menu, static_cast<UINT>(GetMenuItemCount(menu) - 1), true, kind);
    return {true, ac, dc};
}
void tray_menu() {
    MenuImages images;
    HMENU menu = CreatePopupMenu();
    try {
        GUID active = power::active_scheme(); menu_plans = power::schemes();
        auto plan_labels = labels::all(localized_plans(menu_plans), false);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, i18n::text(L"Схемы питания"));
        for (size_t i = 0; i < menu_plans.size() && i < 900; ++i) {
            bool checked = IsEqualGUID(menu_plans[i].id, active);
            std::wstring caption = plan_labels[i];
            if (checked) caption += L"\t✓";
            AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), MENU_PLAN + static_cast<UINT>(i), caption.c_str());
            images.attach(menu, MENU_PLAN + static_cast<UINT>(i), false, menu_art::Kind::scheme,
                icon_style::style(icon_style::classify(menu_plans[i].id, menu_plans[i].name)));
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        auto display_snapshot = append_timeout_menu(menu, images, menu_art::Kind::display, i18n::text(L"Экран"), VIDEO, DISPLAY, MENU_DISPLAY, active);
        auto sleep_snapshot = append_timeout_menu(menu, images, menu_art::Kind::sleep, i18n::text(L"Сон"), SLEEP, STANDBY, MENU_SLEEP, active);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, MENU_ADVANCED, i18n::text(L"Расширенные настройки…"));
        images.attach(menu, MENU_ADVANCED, false, menu_art::Kind::settings);
        AppendMenuW(menu, MF_STRING, MENU_WINDOWS, i18n::text(L"Электропитание Windows…"));
        images.attach(menu, MENU_WINDOWS, false, menu_art::Kind::windows);
        autostart::RegistryStorage autostart_storage;
        autostart::Snapshot autostart_snapshot{autostart::State::foreign, {}, {}};
        std::wstring autostart_executable, autostart_error;
        try {
            autostart_executable = autostart::executable_path();
            autostart_snapshot = autostart::inspect(autostart_storage, autostart_executable);
        } catch (const std::exception& ex) { autostart_error = exception_text(ex); }
        if (!autostart_error.empty()) AppendMenuW(menu, MF_STRING, MENU_AUTOSTART, i18n::text(L"Автозапуск: не удалось прочитать…"));
        else if (autostart_snapshot.state == autostart::State::foreign) AppendMenuW(menu, MF_STRING, MENU_AUTOSTART, i18n::text(L"Автозапуск: запись занята…"));
        else if (autostart_snapshot.state == autostart::State::moved) {
            AppendMenuW(menu, MF_STRING, MENU_AUTOSTART, i18n::text(L"Обновить путь в записи автозапуска…"));
            AppendMenuW(menu, MF_STRING, MENU_AUTOSTART_REMOVE, i18n::text(L"Удалить запись автозапуска с другим путём…"));
        } else if (autostart_snapshot.state == autostart::State::enabled)
            AppendMenuW(menu, MF_STRING, MENU_AUTOSTART, i18n::text(L"Выключить автозапуск"));
        else AppendMenuW(menu, MF_STRING, MENU_AUTOSTART, i18n::text(L"Включить автозапуск"));
        images.attach(menu, MENU_AUTOSTART, false, menu_art::Kind::settings);
        if (autostart_snapshot.state == autostart::State::moved && autostart_error.empty())
            images.attach(menu, MENU_AUTOSTART_REMOVE, false, menu_art::Kind::settings);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, i18n::text(L"Windows может отключить автозапуск в настройках"));
        HMENU languages = CreatePopupMenu();
        if (!languages) throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        const i18n::Language language_values[] = {i18n::Language::system, i18n::Language::english, i18n::Language::russian, i18n::Language::spanish};
        const wchar_t* language_labels[] = {i18n::text(L"Язык Windows"), L"English", L"Русский", L"Español"};
        for (UINT i = 0; i < _countof(language_values); ++i)
            AppendMenuW(languages, MF_STRING | (language_values[i] == i18n::preference() ? MF_CHECKED : 0), MENU_LANGUAGE + i, language_labels[i]);
        if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(languages), i18n::text(L"Язык"))) {
            DWORD code = GetLastError(); DestroyMenu(languages); throw std::system_error(static_cast<int>(code), std::system_category());
        }
        images.attach(menu, static_cast<UINT>(GetMenuItemCount(menu) - 1), true, menu_art::Kind::settings);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        std::wstring undo_caption = i18n::text(L"Отменить последнее изменение");
        if (undo_history.available()) {
            std::wstring action_label = undo_caption_text();
            undo_caption = i18n::text(L"Отменить: ") + action_label;
        }
        AppendMenuW(menu, MF_STRING | (undo_history.available() ? 0 : MF_GRAYED), MENU_UNDO, undo_caption.c_str());
        images.attach(menu, MENU_UNDO, false, menu_art::Kind::undo);
        AppendMenuW(menu, MF_STRING, MENU_EXIT, i18n::text(L"Выход"));
        images.attach(menu, MENU_EXIT, false, menu_art::Kind::exit);
        POINT point; GetCursorPos(&point); SetForegroundWindow(main_window);
        UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, main_window, nullptr);
        PostMessageW(main_window, WM_NULL, 0, 0);
        DestroyMenu(menu); menu = nullptr;
        if (chosen >= MENU_PLAN && chosen < MENU_PLAN + std::min<size_t>(menu_plans.size(), 900)) {
            GUID target = menu_plans[chosen - MENU_PLAN].id;
            GUID before = power::active_scheme();
            if (!IsEqualGUID(before, target)) {
                power::activate(target);
                undo_history.record_scheme(before, target, i18n::text(L"схема · ") + labels::scheme(localized_plans(menu_plans), before, true) + L" ← " + labels::scheme(localized_plans(menu_plans), target, true));
                undo_caption_state = {UndoCaption::Kind::scheme, menu_plans, before, target, {}};
                refresh_icon_safely();
            }
        }
        else if ((chosen >= MENU_DISPLAY && chosen <= MENU_DISPLAY + 99) || (chosen >= MENU_SLEEP && chosen <= MENU_SLEEP + 99)) {
            bool display = chosen < MENU_SLEEP; UINT offset = chosen - (display ? MENU_DISPLAY : MENU_SLEEP);
            const auto& snapshot = display ? display_snapshot : sleep_snapshot;
            DWORD seconds = 0;
            if (offset == 99) { if (!ask_minutes(seconds, snapshot.valid && snapshot.ac == snapshot.dc, snapshot.ac)) return; }
            else if (offset < _countof(intervals)) seconds = intervals[offset]; else return;
            if (!IsEqualGUID(active, power::active_scheme())) { MessageBoxW(main_window, i18n::text(L"Активная схема изменилась. Откройте меню ещё раз."), L"PWSwitch", MB_OK | MB_ICONWARNING); return; }
            if (!snapshot.valid || power::read_value(active, display ? VIDEO : SLEEP, display ? DISPLAY : STANDBY, true) != snapshot.ac ||
                power::read_value(active, display ? VIDEO : SLEEP, display ? DISPLAY : STANDBY, false) != snapshot.dc) {
                MessageBoxW(main_window, i18n::text(L"Значения изменились вне приложения. Откройте меню ещё раз."), L"PWSwitch", MB_OK | MB_ICONWARNING); return;
            }
            if (snapshot.ac == seconds && snapshot.dc == seconds) return;
            std::wstring action_label = std::wstring(display ? i18n::text(L"экран") : i18n::text(L"сон")) + i18n::text(L", сеть и батарея · ") + labels::scheme(localized_plans(menu_plans), active, true);
            set_both(active, display ? VIDEO : SLEEP, display ? DISPLAY : STANDBY, seconds,
                snapshot.ac, snapshot.dc, action_label,
                {display ? UndoCaption::Kind::display : UndoCaption::Kind::sleep, menu_plans, active, {}, {}});
        } else if (chosen == MENU_ADVANCED) show_editor();
        else if (chosen == MENU_WINDOWS) {
            auto command = system_commands::power_options();
            auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(main_window, L"open", command.executable.c_str(), command.arguments.c_str(), nullptr, SW_SHOWNORMAL));
            if (result <= 32) {
                std::wstring message = i18n::text(L"Не удалось открыть электропитание Windows (код ") + std::to_wstring(result) + L").";
                MessageBoxW(main_window, message.c_str(), L"PWSwitch", MB_OK | MB_ICONERROR);
            }
        }
        else if (chosen == MENU_AUTOSTART || chosen == MENU_AUTOSTART_REMOVE) {
            if (!autostart_error.empty()) MessageBoxW(main_window, autostart_error.c_str(), i18n::text(L"PWSwitch — автозапуск"), MB_OK | MB_ICONERROR);
            else if (autostart_snapshot.state == autostart::State::foreign)
                MessageBoxW(main_window, i18n::text(L"Запись PWSwitch занята другой программой или имеет неизвестный формат. Она оставлена без изменений."), i18n::text(L"PWSwitch — автозапуск"), MB_OK | MB_ICONWARNING);
            else {
                bool enable = chosen != MENU_AUTOSTART_REMOVE && autostart_snapshot.state != autostart::State::enabled;
                if (autostart_snapshot.state == autostart::State::moved) {
                    std::wstring question = i18n::text(L"Запись указывает на другой файл:\r\n") + autostart_snapshot.target +
                        (enable ? i18n::text(L"\r\n\r\nЗаменить её путём к текущей программе? Это может быть другая копия.") : i18n::text(L"\r\n\r\nУдалить эту запись? Это может быть другая копия."));
                    if (MessageBoxW(main_window, question.c_str(), i18n::text(L"PWSwitch — автозапуск"), MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return;
                }
                autostart::change(autostart_storage, autostart_snapshot, autostart_executable, enable);
                if (enable) MessageBoxW(main_window, i18n::text(L"Запись добавлена. Windows может отключить её в настройках автозапуска."), i18n::text(L"PWSwitch — автозапуск"), MB_OK | MB_ICONINFORMATION);
            }
        }
        else if (chosen >= MENU_LANGUAGE && chosen < MENU_LANGUAGE + _countof(language_values))
            select_language(language_values[chosen - MENU_LANGUAGE]);
        else if (chosen == MENU_UNDO && undo_history.available()) {
            WindowsUndoBackend backend;
            undo::Result result = undo_history.restore(backend);
            if (result == undo::Result::conflict) {
                MessageBoxW(main_window, i18n::text(L"Схема или параметр изменились вне приложения. Отмена не выполнена, чтобы сохранить новые настройки."), L"PWSwitch", MB_OK | MB_ICONWARNING);
            } else if (result == undo::Result::restored_not_applied) {
                MessageBoxW(main_window, i18n::text(L"Предыдущие значения восстановлены, но применить схему не удалось. Проверьте параметры Windows."), L"PWSwitch", MB_OK | MB_ICONWARNING);
            }
            refresh_icon_safely();
            if (editor && IsWindow(editor) && !has_pending_edits()) update_inputs();
        }
        else if (chosen == MENU_EXIT && (!editor || !IsWindow(editor) || discard_pending_edits(editor))) DestroyWindow(main_window);
    } catch (...) { if (menu) DestroyMenu(menu); throw; }
}
LRESULT CALLBACK main_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    try {
        if (show_settings_message && msg == show_settings_message) {
            if (editor_modal) { if (editor) SetForegroundWindow(GetLastActivePopup(editor)); }
            else if (tray_interaction) { show_settings_pending = true; EndMenu(); }
            else show_editor();
            return 0;
        }
        if (taskbar_created && msg == taskbar_created) {
            icon_registered = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
            if (!icon_registered) icon_registered = Shell_NotifyIconW(NIM_MODIFY, &icon) != FALSE;
            if (icon_registered) refresh_icon_safely();
            return 0;
        }
        if (msg == WM_TIMER && wp == 1) {
            if (!icon_registered) {
                icon_registered = Shell_NotifyIconW(NIM_MODIFY, &icon) != FALSE;
                if (!icon_registered) icon_registered = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
            }
            if (icon_registered) refresh_icon_safely();
            return 0;
        }
        if (msg == WM_POWERBROADCAST && wp == PBT_POWERSETTINGCHANGE && lp) {
            auto setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lp);
            if (IsEqualGUID(setting->PowerSetting, ACTIVE_SCHEME_EVENT)) {
                refresh_icon_safely();
            }
            return TRUE;
        }
        if (msg == tray_message && (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)) {
            if (editor_modal) { if (editor) SetForegroundWindow(GetLastActivePopup(editor)); return 0; }
            if (tray_interaction) return 0;
            struct InteractionGuard { InteractionGuard() { tray_interaction = true; } ~InteractionGuard() { tray_interaction = false; } };
            { InteractionGuard guard; tray_menu(); }
            if (show_settings_pending && IsWindow(hwnd)) { show_settings_pending = false; show_editor(); }
            return 0;
        }
        if (msg == WM_DESTROY) { KillTimer(hwnd, 1); if (scheme_notification) UnregisterPowerSettingNotification(scheme_notification); Shell_NotifyIconW(NIM_DELETE, &icon); if (custom_icon) { DestroyIcon(custom_icon); custom_icon = nullptr; } if (editor) DestroyWindow(editor); PostQuitMessage(0); return 0; }
    } catch (const std::exception& ex) { report(hwnd, ex); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}
int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR, int) {
    instance = hinst;
    try { i18n::initialize(); } catch (const std::exception& ex) { report(nullptr, ex); }
    show_settings_message = RegisterWindowMessageW(L"PWSwitch.ShowSettings");
    if (!show_settings_message) { MessageBoxW(nullptr, i18n::text(L"Не удалось подготовить запуск PWSwitch."), L"PWSwitch", MB_OK | MB_ICONERROR); return 1; }
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\PWSwitch.SingleInstance");
    if (!mutex) { MessageBoxW(nullptr, i18n::text(L"Не удалось проверить, запущен ли PWSwitch."), L"PWSwitch", MB_OK | MB_ICONERROR); return 1; }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = nullptr;
        for (int attempt = 0; attempt < 40 && !existing; ++attempt) {
            existing = FindWindowW(L"PWSwitchMain", nullptr);
            if (!existing) Sleep(50);
        }
        bool sent = existing && PostMessageW(existing, show_settings_message, 0, 0);
        CloseHandle(mutex);
        if (!sent) { MessageBoxW(nullptr, i18n::text(L"PWSwitch уже запущен, но открыть его окно не удалось."), L"PWSwitch", MB_OK | MB_ICONWARNING); return 1; }
        return 0;
    }
    ui_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    WNDCLASSW wc{}; wc.hInstance = instance; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpfnWndProc = main_proc; wc.lpszClassName = L"PWSwitchMain"; RegisterClassW(&wc);
    wc.lpfnWndProc = editor_proc; wc.lpszClassName = L"PWSwitchEditor"; RegisterClassW(&wc);
    wc.lpfnWndProc = prompt_proc; wc.lpszClassName = L"PWSwitchPrompt"; RegisterClassW(&wc);
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    main_window = CreateWindowW(L"PWSwitchMain", L"PWSwitch", 0, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!main_window) { CloseHandle(mutex); return 1; }
    icon.cbSize = sizeof(icon); icon.hWnd = main_window; icon.uID = 1; icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    custom_icon = icon_art::make_icon(icon_style::style(icon_style::Level::unknown));
    icon.uCallbackMessage = tray_message; icon.hIcon = custom_icon ? custom_icon : LoadIconW(nullptr, IDI_APPLICATION); wcscpy_s(icon.szTip, i18n::text(L"PWSwitch — схемы питания"));
    if (!Shell_NotifyIconW(NIM_ADD, &icon)) { MessageBoxW(nullptr, i18n::text(L"Не удалось создать значок в трее."), L"PWSwitch", MB_OK | MB_ICONERROR); DestroyWindow(main_window); CloseHandle(mutex); return 1; }
    icon_registered = true;
    refresh_icon_safely();
    scheme_notification = RegisterPowerSettingNotification(main_window, &ACTIVE_SCHEME_EVENT, DEVICE_NOTIFY_WINDOW_HANDLE);
    UINT_PTR timer = SetTimer(main_window, 1, 5000, nullptr);
    if (!scheme_notification && !timer) MessageBoxW(nullptr, i18n::text(L"Windows не разрешила отслеживать смену схемы. Значок обновится при переключении схемы через приложение."), L"PWSwitch", MB_OK | MB_ICONWARNING);
    else if (!timer) MessageBoxW(nullptr, i18n::text(L"Не удалось включить резервное обновление значка. Если он пропадёт после перезапуска проводника, перезапустите PWSwitch."), L"PWSwitch", MB_OK | MB_ICONWARNING);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool open_settings = false;
    if (argv) {
        for (int i = 1; i < argc; ++i) if (std::wstring(argv[i]) == L"--settings") open_settings = true;
        LocalFree(argv);
    }
    if (open_settings) {
        try { show_editor(); } catch (const std::exception& ex) { report(main_window, ex); }
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (editor && IsDialogMessageW(editor, &msg)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) reveal_editor_focus(editor);
            continue;
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    CloseHandle(mutex); return 0;
}
