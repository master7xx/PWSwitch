#pragma once
#include "power.h"

namespace icon_style {
enum class Level { maximum_saving, saving, balanced, performance, ultimate, unknown };
struct Style { DWORD argb; bool dark_bolt; std::wstring label; };
Level classify(const GUID& scheme, const std::wstring& name);
Style style(Level level);
}
