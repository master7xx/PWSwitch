#include "../src/system_commands.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    try {
        wchar_t system[MAX_PATH + 1]{};
        UINT n=GetSystemDirectoryW(system, MAX_PATH + 1);
        if (!n || n > MAX_PATH) throw std::runtime_error("cannot locate system directory");
        std::wstring expected(system, n);
        if (!expected.empty() && expected.back() != L'\\') expected += L'\\';
        expected += L"control.exe";
        auto command = system_commands::power_options();
        if (CompareStringOrdinal(command.executable.c_str(), -1, expected.c_str(), -1, TRUE) != CSTR_EQUAL)
            throw std::runtime_error("Control Panel executable is not in the Windows system directory");
        if (command.arguments != L"/name Microsoft.PowerOptions") throw std::runtime_error("unexpected Control Panel target");
        DWORD attributes=GetFileAttributesW(command.executable.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("system control.exe is unavailable");
        auto previous = std::vector<wchar_t>(32768);
        DWORD length=GetCurrentDirectoryW(static_cast<DWORD>(previous.size()), previous.data());
        if (!length || length >= previous.size()) throw std::runtime_error("cannot read current directory");
        wchar_t temporary[MAX_PATH+1]{}; DWORD temp=GetTempPathW(MAX_PATH+1, temporary);
        if (!temp || temp > MAX_PATH || !SetCurrentDirectoryW(temporary)) throw std::runtime_error("cannot change test working directory");
        auto other = system_commands::power_options();
        SetCurrentDirectoryW(previous.data());
        if (CompareStringOrdinal(other.executable.c_str(),-1,expected.c_str(),-1,TRUE) != CSTR_EQUAL)
            throw std::runtime_error("current directory changed the executable target");
        std::cout << "system command tests passed\n"; return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
