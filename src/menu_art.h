#pragma once
#include "icon_style.h"

namespace menu_art {
enum class Kind { scheme, display, sleep, settings, windows, exit, clock, never, custom, undo, about };
// The caller owns the returned bitmap. Pixels use premultiplied alpha for native menus.
HBITMAP make_bitmap(Kind kind, const icon_style::Style& scheme, int size = 16);
}
