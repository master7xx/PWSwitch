#include "../src/power.h"
#include <powrprof.h>

namespace {
DWORD WINAPI unavailable_display_increment(HKEY root, const GUID* group, const GUID* setting, DWORD* value) {
    const GUID video = {0x7516b95f,0xf776,0x4464,{0x8c,0x53,0x06,0x16,0x7f,0x40,0xcc,0x99}};
    const GUID display = {0x3c0bc021,0xc8a8,0x4e07,{0xa9,0x73,0x6b,0x14,0xcb,0xcb,0x2b,0x7e}};
    if (IsEqualGUID(*group, video) && IsEqualGUID(*setting, display)) {
        *value = 0;
        return std::wstring(GetCommandLineW()).find(L"--test-zero-step") != std::wstring::npos ? ERROR_SUCCESS : ERROR_ACCESS_DENIED;
    }
    return PowerReadValueIncrement(root, group, setting, value);
}
}

// Test executable only. All controls and event handlers are production code.
// Compile this instead of main.cpp and power.cpp; link the other normal sources.
#define PowerReadValueIncrement unavailable_display_increment
#include "../src/power.cpp"
#undef PowerReadValueIncrement
#include "../src/main.cpp"
