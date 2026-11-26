#include "focus_tracking.h"
#include <windows.h>
#include <psapi.h>
#include <vector>

namespace focus_tracking {
namespace {
std::wstring process_path(DWORD pid) {
    // PROCESS_QUERY_LIMITED_INFORMATION works for elevated processes and does
    // not require PROCESS_VM_READ.
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!handle) return {};
    std::wstring result;
    // QueryFullProcessImageNameW does not reliably report the required size when
    // called with a null buffer (it can stay 0), so grow the buffer on failure.
    DWORD size = 512;
    std::vector<wchar_t> buffer(size, L'\0');
    for (;;) {
        DWORD needed = static_cast<DWORD>(buffer.size());
        if (QueryFullProcessImageNameW(handle, 0, buffer.data(), &needed)) {
            result.assign(buffer.data(), needed);
            break;
        }
        DWORD error = GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER || buffer.size() >= 32768) { result.clear(); break; }
        buffer.resize(buffer.size() * 2, L'\0');
    }
    CloseHandle(handle);
    return result;
}
std::wstring window_title(HWND hwnd) {
    int n = GetWindowTextLengthW(hwnd);
    if (n <= 0) return {};
    std::vector<wchar_t> buffer(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(hwnd, buffer.data(), n + 1);
    return buffer.data();
}
std::wstring file_name(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}
}  // namespace

WindowInfo from_hwnd(HWND hwnd) {
    WindowInfo info;
    if (!hwnd || !IsWindow(hwnd)) return info;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return info;
    info.path = process_path(pid);
    if (!info.path.empty()) info.exe = file_name(info.path);
    info.title = window_title(hwnd);
    info.valid = true;
    return info;
}

WindowInfo current_foreground() { return from_hwnd(GetForegroundWindow()); }

std::optional<GUID> Engine::evaluate(const focus_rules::Ruleset& rules) {
    WindowInfo info = backend_->foreground();
    if (!info.valid) return std::nullopt;
    focus_rules::Foreground foreground;
    foreground.path = info.path;
    foreground.exe = info.exe;
    foreground.title = info.title;
    auto target = focus_rules::match(rules, foreground);
    if (!target) return std::nullopt;
    if (IsEqualGUID(*target, backend_->active())) return std::nullopt;
    backend_->activate(*target);
    return target;
}
}  // namespace focus_tracking
