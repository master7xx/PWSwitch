#include "system_commands.h"
#include <windows.h>
#include <system_error>
#include <vector>

namespace system_commands {
Command power_options() {
    std::vector<wchar_t> buffer(256);
    for (;;) {
        UINT n = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
        if (!n) throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        if (n < buffer.size()) {
            std::wstring path(buffer.data(), n);
            if (!path.empty() && path.back() != L'\\') path += L'\\';
            return {path + L"control.exe", L"/name Microsoft.PowerOptions"};
        }
        if (n > 32767) throw std::runtime_error("Слишком длинный путь к системному каталогу Windows.");
        buffer.resize(static_cast<size_t>(n) + 1);
    }
}
}
