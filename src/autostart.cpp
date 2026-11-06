#include "autostart.h"
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace autostart {
namespace {
constexpr wchar_t value_name[] = L"PWSwitch";
void check(LSTATUS code) { if (code != ERROR_SUCCESS) throw std::system_error(static_cast<int>(code), std::system_category()); }
struct Key { HKEY value = nullptr; ~Key() { if (value) RegCloseKey(value); } };
bool equal(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
bool same(const Entry& a, const Entry& b) { return a.exists == b.exists && (!a.exists || (a.type == b.type && a.data == b.data)); }
std::wstring entry_text(const Entry& entry) {
    if (!entry.exists || entry.type != REG_SZ || entry.data.size() < 2 * sizeof(wchar_t) || entry.data.size() % sizeof(wchar_t)) return {};
    std::wstring value(entry.data.size() / sizeof(wchar_t), L'\0');
    std::memcpy(&value[0], entry.data.data(), entry.data.size());
    if (value.back() != L'\0') return {};
    value.pop_back();
    if (value.find(L'\0') != std::wstring::npos) return {};
    return value;
}
std::wstring quoted_target(const std::wstring& value) {
    if (value.size() < 3 || value.front() != L'"' || value.back() != L'"') return {};
    std::wstring path = value.substr(1, value.size() - 2);
    if (path.find(L'"') != std::wstring::npos) return {};
    bool absolute = (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) ||
                    (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\');
    return absolute ? path : std::wstring();
}
Entry make_entry(const std::wstring& path) {
    if (path.empty() || path.find(L'"') != std::wstring::npos || path.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Некорректный путь к программе.");
    std::wstring command = L"\"" + path + L"\"";
    if (command.size() > 260) throw std::runtime_error("Путь к программе слишком длинный для автозапуска Windows (более 260 символов вместе с кавычками).");
    Entry result; result.exists = true; result.type = REG_SZ;
    result.data.resize((command.size() + 1) * sizeof(wchar_t));
    std::memcpy(result.data.data(), command.c_str(), result.data.size());
    return result;
}
}
Entry RegistryStorage::read() {
    Key key; LSTATUS code = RegOpenKeyExW(root_, key_path_.c_str(), 0, KEY_QUERY_VALUE, &key.value);
    if (code == ERROR_FILE_NOT_FOUND) return {};
    check(code);
    for (int attempt = 0; attempt < 3; ++attempt) {
        DWORD type = 0, size = 0; code = RegQueryValueExW(key.value, value_name, nullptr, &type, nullptr, &size);
        if (code == ERROR_FILE_NOT_FOUND) return {};
        check(code);
        if (size > 1024 * 1024) throw std::runtime_error("Запись автозапуска слишком велика; она оставлена без изменений.");
        Entry result; result.exists = true; result.type = type; result.data.resize(size);
        DWORD actual = size;
        code = RegQueryValueExW(key.value, value_name, nullptr, &result.type, result.data.empty() ? nullptr : result.data.data(), &actual);
        if (code == ERROR_MORE_DATA || (code == ERROR_SUCCESS && actual > size)) continue;
        if (code == ERROR_FILE_NOT_FOUND) return {};
        check(code); result.data.resize(actual); return result;
    }
    throw std::runtime_error("Запись автозапуска меняется. Откройте меню снова.");
}
void RegistryStorage::write(const Entry& entry) {
    Key key; DWORD disposition = 0;
    check(RegCreateKeyExW(root_, key_path_.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key.value, &disposition));
    check(RegSetValueExW(key.value, value_name, 0, entry.type, entry.data.data(), static_cast<DWORD>(entry.data.size())));
}
void RegistryStorage::remove() {
    Key key; LSTATUS code = RegOpenKeyExW(root_, key_path_.c_str(), 0, KEY_SET_VALUE, &key.value);
    if (code == ERROR_FILE_NOT_FOUND) return;
    check(code); code = RegDeleteValueW(key.value, value_name);
    if (code != ERROR_FILE_NOT_FOUND) check(code);
}
std::wstring executable_path() {
    std::vector<wchar_t> buffer(512);
    while (buffer.size() <= 32768) {
        DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!n) throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        if (n < buffer.size()) return std::wstring(buffer.data(), n);
        buffer.resize(buffer.size() * 2);
    }
    throw std::runtime_error("Не удалось определить путь к программе.");
}
Snapshot inspect(Storage& storage, const std::wstring& executable) {
    Entry entry = storage.read();
    if (!entry.exists) return {State::disabled, std::move(entry), {}};
    std::wstring target = quoted_target(entry_text(entry));
    if (!target.empty() && equal(target, executable)) return {State::enabled, std::move(entry), std::move(target)};
    size_t slash = target.find_last_of(L"\\/");
    if (!target.empty() && slash != std::wstring::npos && equal(target.substr(slash + 1), L"PWSwitch.exe"))
        return {State::moved, std::move(entry), std::move(target)};
    return {State::foreign, std::move(entry), {}};
}
void change(Storage& storage, const Snapshot& expected, const std::wstring& executable, bool enable) {
    if (expected.state == State::foreign) throw std::runtime_error("Запись автозапуска занята другой программой или имеет неизвестный формат.");
    Entry new_value;
    if (enable) new_value = make_entry(executable);
    Entry current = storage.read();
    if (!same(current, expected.entry)) throw std::runtime_error("Запись автозапуска изменилась. Откройте меню снова.");
    if (enable) storage.write(new_value); else if (current.exists) storage.remove();
}
}
