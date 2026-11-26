#include "focus_ui.h"
#include "focus_tracking.h"
#include "i18n.h"
#include <system_error>
#include <vector>
#include <stdexcept>
#include <windows.h>
#include <commctrl.h>

namespace focus_ui {
namespace {
HINSTANCE instance = nullptr;
HFONT ui_font = nullptr;
std::vector<power::Item> g_schemes;
// Currently open modal dialog (rules or picker). While non-null the tray menu
// is suppressed and tray clicks restore focus to it.
HWND g_modal = nullptr;

constexpr int ID_ENABLE = 201, ID_FALLBACK = 202, ID_RULELIST = 203, ID_RULEPATH = 204, ID_RULETITLE = 205,
    ID_RULESCHEME = 206, ID_RULEADD = 207, ID_RULEEDIT = 208, ID_RULEDEL = 209, ID_PICK = 210, ID_SAVE = 211;
constexpr int ID_PICKLIST = 301, ID_PICKOK = 302;

HWND control(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0) {
    HWND result = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (result && ui_font) SendMessageW(result, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    return result;
}
std::wstring window_text(HWND window) {
    int n = GetWindowTextLengthW(window);
    std::vector<wchar_t> buffer(static_cast<size_t>(n) + 1);
    GetWindowTextW(window, buffer.data(), n + 1);
    return buffer.data();
}
void set_text(HWND window, const std::wstring& text) { SetWindowTextW(window, text.c_str()); }
void popup(HWND owner, const std::wstring& text, UINT flags) { MessageBoxW(owner, text.c_str(), L"PWSwitch", flags); }
int scheme_index(const GUID& id) {
    for (size_t i = 0; i < g_schemes.size(); ++i) if (IsEqualGUID(g_schemes[i].id, id)) return static_cast<int>(i);
    return -1;
}
std::wstring scheme_label(const GUID& id) {
    int i = scheme_index(id);
    return i >= 0 ? g_schemes[static_cast<size_t>(i)].name : L"?";
}
std::wstring rule_summary(const focus_rules::Rule& rule) {
    std::wstring label = rule.path.empty() ? L"(нет exe)" : rule.path;
    label += L"  |  ";
    label += rule.title.empty() ? L"(любой заголовок)" : rule.title;
    label += L"  ->  " + scheme_label(rule.scheme);
    return label;
}

// Center a dialog. The PWSwitch main window is a message-only window at (0,0)
// with zero size, so centering over it would put the dialog in the corner of
// the display. Only center over an owner when it is visible and non-empty;
// otherwise center on the work area.
void center_over(HWND dialog, HWND owner) {
    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    RECT dialog_rect{};
    if (!GetWindowRect(dialog, &dialog_rect)) return;
    int width = dialog_rect.right - dialog_rect.left;
    int height = dialog_rect.bottom - dialog_rect.top;
    int x, y;
    RECT owner_rect{};
    if (owner && IsWindow(owner) && IsWindowVisible(owner) && GetWindowRect(owner, &owner_rect) &&
        (owner_rect.right - owner_rect.left) > 0 && (owner_rect.bottom - owner_rect.top) > 0) {
        x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
        y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    } else {
        x = area.left + ((area.right - area.left) - width) / 2;
        y = area.top + ((area.bottom - area.top) - height) / 2;
    }
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

struct WindowPick { std::wstring path, exe, title; };
std::vector<WindowPick> g_pick_items;
HFONT g_bold_font = nullptr;

BOOL CALLBACK enum_win(HWND hwnd, LPARAM lp) {
    auto* vec = reinterpret_cast<std::vector<WindowPick>*>(lp);
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    if (GetWindowTextLengthW(hwnd) <= 0) return TRUE;
    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    if (wcsncmp(class_name, L"PWSwitch", 8) == 0) return TRUE;
    auto info = focus_tracking::from_hwnd(hwnd);
    if (!info.valid || info.path.empty()) return TRUE;
    vec->push_back({info.path, info.exe, info.title});
    return TRUE;
}

struct PickResult { bool picked = false; std::wstring path, title; };
struct PickState { PickResult* result = nullptr; HWND list = nullptr; };

LRESULT CALLBACK pick_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* state = reinterpret_cast<PickState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        state = reinterpret_cast<PickState*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (msg == WM_CREATE) {
        state->list = control(hwnd, L"LISTBOX", L"", LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
            8, 8, 544, 330, ID_PICKLIST);
        control(hwnd, L"BUTTON", i18n::text(L"Выбрать"), BS_DEFPUSHBUTTON | WS_TABSTOP, 8, 348, 90, 30, ID_PICKOK);
        control(hwnd, L"BUTTON", i18n::text(L"Отмена"), WS_TABSTOP, 104, 348, 90, 30, IDCANCEL);
        return 0;
    }
    if (msg == WM_DRAWITEM) {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (!state || dis->CtlID != ID_PICKLIST || dis->itemID == static_cast<UINT>(-1) ||
            dis->itemData < 0 || static_cast<size_t>(dis->itemData) >= g_pick_items.size()) return TRUE;
        const auto& item = g_pick_items[static_cast<size_t>(dis->itemData)];
        HDC dc = dis->hDC;
        RECT rc = dis->rcItem;
        bool selected = (dis->itemState & ODS_SELECTED) != 0;
        FillRect(dc, &rc, selected ? GetSysColorBrush(COLOR_HIGHLIGHT) : GetSysColorBrush(COLOR_WINDOW));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, selected ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_WINDOWTEXT));
        std::wstring path = item.path;
        std::wstring rest = L"  " + item.exe + L" — " + item.title;
        HGDIOBJ old_font = SelectObject(dc, g_bold_font ? g_bold_font : ui_font);
        SIZE size{};
        std::wstring padded = path + L"  ";
        GetTextExtentPoint32W(dc, padded.c_str(), static_cast<int>(padded.size()), &size);
        RECT text_rc = rc; text_rc.left += 4; text_rc.top += 1;
        DrawTextW(dc, path.c_str(), static_cast<int>(path.size()), &text_rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(dc, ui_font);
        text_rc.left += size.cx;
        DrawTextW(dc, rest.c_str(), static_cast<int>(rest.size()), &text_rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(dc, old_font);
        if (dis->itemState & ODS_FOCUS) { RECT focus = rc; focus.left += 2; DrawFocusRect(dc, &focus); }
        return TRUE;
    }
    if (msg == WM_COMMAND) {
        switch (LOWORD(wp)) {
        case ID_PICKLIST:
            if (HIWORD(wp) == LBN_DBLCLK) SendMessageW(hwnd, WM_COMMAND, ID_PICKOK, 0);
            return 0;
        case ID_PICKOK: {
            LRESULT sel = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR && sel >= 0 && static_cast<size_t>(sel) < g_pick_items.size()) {
                state->result->picked = true;
                state->result->path = g_pick_items[static_cast<size_t>(sel)].path;
                state->result->title = g_pick_items[static_cast<size_t>(sel)].title;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL: DestroyWindow(hwnd); return 0;
        }
    }
    if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
bool pick_window(HWND owner, std::wstring& path, std::wstring& title) {
    std::vector<WindowPick> collected;
    EnumWindows(enum_win, reinterpret_cast<LPARAM>(&collected));
    g_pick_items = std::move(collected);
    if (g_pick_items.empty()) { popup(owner, i18n::text(L"Активные окна не найдены."), MB_OK | MB_ICONINFORMATION); return false; }
    g_bold_font = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    PickState state; PickResult result; state.result = &result;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"PWSwitchPick", i18n::text(L"Выбор активного окна"),
        WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 560, 420, owner, nullptr, instance, &state);
    if (!dialog) {
        if (g_bold_font) { DeleteObject(g_bold_font); g_bold_font = nullptr; }
        std::wstring message = i18n::text(L"Не удалось открыть список окон (код ") +
            std::to_wstring(GetLastError()) + L").";
        popup(owner, message, MB_OK | MB_ICONERROR);
        return false;
    }
    center_over(dialog, owner);
    for (size_t i = 0; i < g_pick_items.size(); ++i) {
        std::wstring entry = g_pick_items[i].path + L"\t" + g_pick_items[i].exe + L" — " + g_pick_items[i].title;
        LRESULT row = SendMessageW(state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.c_str()));
        if (row >= 0) SendMessageW(state.list, LB_SETITEMDATA, row, static_cast<LPARAM>(i));
    }
    SendMessageW(state.list, LB_SETCURSEL, 0, 0);
    ShowWindow(dialog, SW_SHOW); UpdateWindow(dialog);
    g_modal = dialog;
    MSG msg;
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    g_modal = nullptr;
    if (g_bold_font) { DeleteObject(g_bold_font); g_bold_font = nullptr; }
    if (result.picked) { path = result.path; title = result.title; return true; }
    return false;
}

struct RuleDialog {
    focus_rules::Ruleset rules;
    int sel = -1;
    bool saved = false;
    HWND enable, fallback_combo, list, path_edit, title_edit, scheme_combo;
};

void refresh_rule_list(HWND hwnd);
void load_rule_into_fields(HWND hwnd);
void add_rule(HWND hwnd);
void edit_rule(HWND hwnd);
void delete_rule(HWND hwnd);
void save_rules(HWND hwnd);

void refresh_rule_list(HWND hwnd) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    SendMessageW(d->list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < d->rules.rules.size(); ++i) {
        std::wstring label = rule_summary(d->rules.rules[i]);
        LRESULT row = SendMessageW(d->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (row >= 0) SendMessageW(d->list, LB_SETITEMDATA, row, static_cast<LPARAM>(i));
    }
    if (!d->rules.rules.empty() && d->sel < 0) d->sel = 0;
    else if (d->sel >= static_cast<int>(d->rules.rules.size())) d->sel = static_cast<int>(d->rules.rules.size()) - 1;
    if (d->sel >= 0 && d->sel < static_cast<int>(d->rules.rules.size())) {
        SendMessageW(d->list, LB_SETCURSEL, d->sel, 0);
        load_rule_into_fields(hwnd);
    } else { set_text(d->path_edit, L""); set_text(d->title_edit, L""); SendMessageW(d->scheme_combo, CB_SETCURSEL, 0, 0); }
}
void load_rule_into_fields(HWND hwnd) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    LRESULT sel = SendMessageW(d->list, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || sel < 0 || static_cast<size_t>(sel) >= d->rules.rules.size()) return;
    d->sel = static_cast<int>(sel);
    const auto& rule = d->rules.rules[static_cast<size_t>(sel)];
    set_text(d->path_edit, rule.path);
    set_text(d->title_edit, rule.title);
    int idx = scheme_index(rule.scheme);
    SendMessageW(d->scheme_combo, CB_SETCURSEL, idx >= 0 ? idx : 0, 0);
}
void collect_rule_fields(HWND hwnd, focus_rules::Rule& out) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    out.path = window_text(d->path_edit);
    out.title = window_text(d->title_edit);
    LRESULT idx = SendMessageW(d->scheme_combo, CB_GETCURSEL, 0, 0);
    out.scheme = (idx >= 0 && static_cast<size_t>(idx) < g_schemes.size()) ? g_schemes[static_cast<size_t>(idx)].id : GUID{};
}
void add_rule(HWND hwnd) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    focus_rules::Rule rule; collect_rule_fields(hwnd, rule);
    if (rule.path.empty()) { popup(hwnd, i18n::text(L"Укажите исполняемый файл."), MB_OK | MB_ICONWARNING); return; }
    d->rules.rules.push_back(std::move(rule));
    d->sel = static_cast<int>(d->rules.rules.size()) - 1;
    refresh_rule_list(hwnd);
}
void edit_rule(HWND hwnd) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (d->sel < 0 || d->sel >= static_cast<int>(d->rules.rules.size())) return;
    focus_rules::Rule rule; collect_rule_fields(hwnd, rule);
    if (rule.path.empty()) { popup(hwnd, i18n::text(L"Укажите исполняемый файл."), MB_OK | MB_ICONWARNING); return; }
    d->rules.rules[static_cast<size_t>(d->sel)] = std::move(rule);
    refresh_rule_list(hwnd);
}
void delete_rule(HWND hwnd) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (d->sel < 0 || d->sel >= static_cast<int>(d->rules.rules.size())) return;
    d->rules.rules.erase(d->rules.rules.begin() + d->sel);
    d->sel = -1;
    refresh_rule_list(hwnd);
}
void save_rules(HWND hwnd) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    d->rules.enabled = SendMessageW(d->enable, BM_GETCHECK, 0, 0) == BST_CHECKED;
    LRESULT fb = SendMessageW(d->fallback_combo, CB_GETCURSEL, 0, 0);
    d->rules.has_fallback = fb > 0 && static_cast<size_t>(fb) - 1 < g_schemes.size();
    d->rules.fallback = d->rules.has_fallback ? g_schemes[static_cast<size_t>(fb) - 1].id : GUID{};
    focus_rules::save(d->rules);
    d->saved = true;
    DestroyWindow(hwnd);
}

LRESULT CALLBACK rules_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* d = reinterpret_cast<RuleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        d = reinterpret_cast<RuleDialog*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));
    }
    try {
        if (msg == WM_CREATE) {
            d->enable = control(hwnd, L"BUTTON", i18n::text(L"Включить авто-переключение по фокусу окна"), BS_AUTOCHECKBOX | WS_TABSTOP, 14, 14, 520, 24, ID_ENABLE);
            control(hwnd, L"STATIC", i18n::text(L"Резервный план:"), 0, 14, 46, 130, 22);
            d->fallback_combo = control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 150, 43, 384, 260, ID_FALLBACK);
            control(hwnd, L"STATIC", i18n::text(L"Правила:"), 0, 14, 76, 130, 22);
            d->list = control(hwnd, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, 14, 100, 520, 150, ID_RULELIST);
            control(hwnd, L"STATIC", i18n::text(L"Исполняемый файл (путь):"), 0, 14, 258, 200, 22);
            d->path_edit = control(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 220, 258, 314, 26, ID_RULEPATH);
            control(hwnd, L"BUTTON", i18n::text(L"Выбрать окно…"), WS_TABSTOP, 14, 258, 200, 26, ID_PICK);
            control(hwnd, L"STATIC", i18n::text(L"Заголовок окна:"), 0, 14, 290, 200, 22);
            d->title_edit = control(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 220, 290, 314, 26, ID_RULETITLE);
            control(hwnd, L"STATIC", i18n::text(L"План питания:"), 0, 14, 330, 130, 22);
            d->scheme_combo = control(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 150, 328, 384, 240, ID_RULESCHEME);
            control(hwnd, L"BUTTON", i18n::text(L"Добавить"), WS_TABSTOP, 14, 372, 150, 30, ID_RULEADD);
            control(hwnd, L"BUTTON", i18n::text(L"Удалить"), WS_TABSTOP, 174, 372, 150, 30, ID_RULEDEL);
            control(hwnd, L"BUTTON", i18n::text(L"Сохранить"), WS_TABSTOP, 334, 372, 150, 30, ID_SAVE);

            for (const auto& plan : g_schemes) {
                auto name = plan.name;
                SendMessageW(d->scheme_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
                SendMessageW(d->fallback_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
            }
            SendMessageW(d->scheme_combo, CB_SETCURSEL, 0, 0);
            std::wstring placeholder = i18n::text(L"— не переключать —");
            SendMessageW(d->fallback_combo, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(placeholder.c_str()));
            int fallback_index = 0;
            if (d->rules.has_fallback) {
                int found = scheme_index(d->rules.fallback);
                if (found >= 0) fallback_index = found + 1;
            }
            SendMessageW(d->fallback_combo, CB_SETCURSEL, fallback_index, 0);
            SendMessageW(d->enable, BM_SETCHECK, d->rules.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
            refresh_rule_list(hwnd);
            return 0;
        }
        if (msg == WM_COMMAND) {
            switch (LOWORD(wp)) {
            case ID_PICK: {
                std::wstring path, title;
                if (pick_window(hwnd, path, title)) { set_text(d->path_edit, path); set_text(d->title_edit, title); }
                return 0;
            }
            case ID_RULELIST:
                if (HIWORD(wp) == LBN_SELCHANGE) { load_rule_into_fields(hwnd); return 0; }
                return 0;
            case ID_RULEADD: add_rule(hwnd); return 0;
            case ID_RULEEDIT: edit_rule(hwnd); return 0;
            case ID_RULEDEL: delete_rule(hwnd); return 0;
            case ID_SAVE: save_rules(hwnd); return 0;
            case IDCANCEL: DestroyWindow(hwnd); return 0;
            }
        }
        if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    } catch (const std::exception& ex) {
        int n = MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, nullptr, 0);
        std::wstring message = n ? std::wstring(n, L'\0') : L"";
        if (n) MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, &message[0], n);
        popup(hwnd, message, MB_OK | MB_ICONERROR);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}  // namespace

void init(HINSTANCE hinst, HFONT font) {
    instance = hinst;
    ui_font = font;
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpfnWndProc = rules_proc;
    wc.lpszClassName = L"PWSwitchRules";
    RegisterClassW(&wc);
    wc.lpfnWndProc = pick_proc;
    wc.lpszClassName = L"PWSwitchPick";
    RegisterClassW(&wc);
}
bool show_rules(HWND owner, const std::vector<power::Item>& schemes) {
    g_schemes = schemes;
    RuleDialog state;
    state.rules = focus_rules::load();
    // Default the fallback plan to the economical/saving plan when none is set.
    // Do NOT force enabled here: the checkbox must reflect and preserve the
    // user's saved state so it can be switched off and stay off.
    if (!state.rules.has_fallback) {
        GUID fallback{};
        if (focus_rules::default_fallback(g_schemes, fallback)) {
            state.rules.has_fallback = true;
            state.rules.fallback = fallback;
        }
    }
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"PWSwitchRules", i18n::text(L"PWSwitch — авто-переключение"),
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 560, 540, owner, nullptr, instance, &state);
    if (!dialog) {
        std::wstring message = i18n::text(L"Не удалось открыть окно правил (код ") +
            std::to_wstring(GetLastError()) + L").";
        MessageBoxW(owner, message.c_str(), L"PWSwitch", MB_OK | MB_ICONERROR);
        return false;
    }
    center_over(dialog, owner);
    ShowWindow(dialog, SW_SHOW); UpdateWindow(dialog);
    g_modal = dialog;
    MSG msg;
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    g_modal = nullptr;
    return state.saved;
}

HWND current_modal() { return g_modal; }
}
