#include "power.h"
#include "i18n.h"
#include "power_transaction.h"
#include <powrprof.h>
#include <system_error>

namespace power {
namespace {
void check(DWORD code) {
    if (code != ERROR_SUCCESS) throw std::system_error(static_cast<int>(code), std::system_category());
}
std::wstring guid_string(const GUID& id) {
    wchar_t text[40] = {};
    swprintf_s(text, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", id.Data1, id.Data2, id.Data3,
        id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3], id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    return text;
}
using TextFn = DWORD (WINAPI*)(HKEY, const GUID*, const GUID*, const GUID*, UCHAR*, DWORD*);
std::wstring text(TextFn fn, const GUID* scheme, const GUID* group, const GUID* setting) {
    DWORD bytes = 0;
    DWORD result = fn(nullptr, scheme, group, setting, nullptr, &bytes);
    if ((result != ERROR_MORE_DATA && result != ERROR_SUCCESS) || bytes == 0) return {};
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    result = fn(nullptr, scheme, group, setting, reinterpret_cast<UCHAR*>(buffer.data()), &bytes);
    return result == ERROR_SUCCESS ? std::wstring(buffer.data()) : std::wstring();
}
std::wstring name(const GUID* scheme, const GUID* group, const GUID* setting, const GUID& fallback) {
    auto result = text(PowerReadFriendlyName, scheme, group, setting);
    return result.empty() ? guid_string(fallback) : result;
}
std::vector<GUID> enumerate(const GUID* scheme, const GUID* group, POWER_DATA_ACCESSOR kind) {
    std::vector<GUID> result;
    for (ULONG i = 0;; ++i) {
        GUID id{}; DWORD size = sizeof(id);
        DWORD code = PowerEnumerate(nullptr, scheme, group, kind, i, reinterpret_cast<UCHAR*>(&id), &size);
        if (code == ERROR_NO_MORE_ITEMS) break;
        check(code);
        result.push_back(id);
    }
    return result;
}
}
GUID active_scheme() {
    GUID* result = nullptr;
    check(PowerGetActiveScheme(nullptr, &result));
    if (!result) throw std::runtime_error("Windows returned no active scheme");
    GUID copy = *result; LocalFree(result); return copy;
}
std::vector<Item> schemes() {
    std::vector<Item> out;
    for (const auto& id : enumerate(nullptr, nullptr, ACCESS_SCHEME)) out.push_back({id, name(&id, nullptr, nullptr, id)});
    return out;
}
std::vector<Item> groups(const GUID& scheme) {
    std::vector<Item> out;
    for (const auto& id : enumerate(&scheme, nullptr, ACCESS_SUBGROUP)) out.push_back({id, name(&scheme, &id, nullptr, id)});
    return out;
}
std::vector<Setting> settings(const GUID& scheme, const GUID& group) {
    std::vector<Setting> out;
    for (const auto& id : enumerate(&scheme, &group, ACCESS_INDIVIDUAL_SETTING)) {
        Setting s{}; s.group = group; s.id = id;
        s.name = name(&scheme, &group, &id, id);
        s.description = text(PowerReadDescription, &scheme, &group, &id);
        s.hidden = (PowerReadSettingAttributes(&group, &id) & POWER_ATTRIBUTE_HIDE) != 0;
        DWORD bytes = 0;
        DWORD units_code = PowerReadValueUnitsSpecifier(nullptr, &group, &id, nullptr, &bytes);
        if ((units_code == ERROR_MORE_DATA || units_code == ERROR_SUCCESS) && bytes) {
            std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
            if (PowerReadValueUnitsSpecifier(nullptr, &group, &id, reinterpret_cast<UCHAR*>(buffer.data()), &bytes) == ERROR_SUCCESS) s.units = buffer.data();
        }
        s.range = PowerReadValueMin(nullptr, &group, &id, &s.minimum) == ERROR_SUCCESS &&
                  PowerReadValueMax(nullptr, &group, &id, &s.maximum) == ERROR_SUCCESS;
        if (s.range) {
            DWORD increment = 0;
            DWORD increment_code = PowerReadValueIncrement(nullptr, &group, &id, &increment);
            if (increment_code == ERROR_SUCCESS && increment != 0) s.increment = increment;
            else {
                s.increment = 0;
                s.description += i18n::text(L"\r\nРедактирование недоступно: Windows не предоставила достоверный шаг диапазона. Текущие значения доступны для просмотра.");
                if (increment_code != ERROR_SUCCESS) s.description += L"\r\n" + error_text(increment_code);
            }
        }
        bool incomplete_options = false;
        DWORD i = 0;
        for (; !s.range && i < 1024; ++i) {
            DWORD type = 0, size = 0;
            DWORD code = PowerReadPossibleValue(nullptr, &group, &id, &type, i, nullptr, &size);
            if (code != ERROR_SUCCESS && code != ERROR_MORE_DATA) {
                incomplete_options = code != ERROR_FILE_NOT_FOUND;
                break;
            }
            if (size == 0 || size > 1024 * 1024) { incomplete_options = true; break; }
            std::vector<UCHAR> possible_value(size);
            const DWORD allocated_size = size;
            code = PowerReadPossibleValue(nullptr, &group, &id, &type, i, possible_value.data(), &size);
            if (code != ERROR_SUCCESS || size == 0 || size > allocated_size) { incomplete_options = true; break; }
            DWORD string_bytes = 0; std::wstring label;
            DWORD name_code = PowerReadPossibleFriendlyName(nullptr, &group, &id, i, nullptr, &string_bytes);
            if ((name_code == ERROR_MORE_DATA || name_code == ERROR_SUCCESS) && string_bytes) {
                std::vector<wchar_t> buffer(string_bytes / sizeof(wchar_t) + 1, L'\0');
                if (PowerReadPossibleFriendlyName(nullptr, &group, &id, i, reinterpret_cast<UCHAR*>(buffer.data()), &string_bytes) == ERROR_SUCCESS) label = buffer.data();
            }
            bool named = !label.empty();
            if (!named) label = std::wstring(i18n::text(L"Индекс ")) + std::to_wstring(i) + i18n::text(L" — название недоступно");
            s.options.push_back({i, label, named});
        }
        if (!s.range && s.options.empty())
            s.description += i18n::text(L"\r\nWindows не предоставила доступные значения. Редактирование недоступно.");
        else if (!s.range && (incomplete_options || i == 1024))
            s.description += i18n::text(L"\r\nWindows не предоставила все сведения: список может быть неполным.");
        out.push_back(std::move(s));
    }
    return out;
}
DWORD read_value(const GUID& scheme, const GUID& group, const GUID& setting, bool ac) {
    DWORD value = 0;
    check(ac ? PowerReadACValueIndex(nullptr, &scheme, &group, &setting, &value) : PowerReadDCValueIndex(nullptr, &scheme, &group, &setting, &value));
    return value;
}
void write_value(const GUID& scheme, const GUID& group, const GUID& setting, bool ac, DWORD value) {
    check(ac ? PowerWriteACValueIndex(nullptr, &scheme, &group, &setting, value) : PowerWriteDCValueIndex(nullptr, &scheme, &group, &setting, value));
}
void write_both(const GUID& scheme, const GUID& group, const GUID& setting, DWORD value) {
    DWORD old_ac = read_value(scheme, group, setting, true), old_dc = read_value(scheme, group, setting, false);
    write_both_checked(scheme, group, setting, value, old_ac, old_dc);
}
void write_both_checked(const GUID& scheme, const GUID& group, const GUID& setting, DWORD value, DWORD expected_ac, DWORD expected_dc) {
    struct Access final : ValueAccess {
        const GUID &scheme, &group, &setting;
        Access(const GUID& s, const GUID& g, const GUID& v) : scheme(s), group(g), setting(v) {}
        DWORD read(bool ac) override { return read_value(scheme, group, setting, ac); }
        void write(bool ac, DWORD n) override { write_value(scheme, group, setting, ac, n); }
    } access(scheme, group, setting);
    write_pair_if_unchanged(access, expected_ac, expected_dc, value);
}
void activate(const GUID& scheme) { check(PowerSetActiveScheme(nullptr, &scheme)); }
std::wstring error_text(DWORD error) {
    wchar_t* text = nullptr;
    DWORD language = i18n::current() == i18n::Language::russian ? MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT) :
        i18n::current() == i18n::Language::spanish ? MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN) : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, language, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    if (!n && language != MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
        n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring result = n && text ? std::wstring(text, n) : std::wstring(i18n::text(L"Ошибка ")) + std::to_wstring(error);
    if (text) LocalFree(text);
    return result;
}
}
