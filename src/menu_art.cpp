#include "menu_art.h"
#include <algorithm>
#include <cmath>

namespace menu_art {
namespace {
double sq(double x) { return x * x; }
double dist(double x, double y, double cx, double cy) { return std::sqrt(sq(x - cx) + sq(y - cy)); }
bool line(double x, double y, double ax, double ay, double bx, double by, double width = 1.35) {
    double dx = bx - ax, dy = by - ay;
    double t = std::max(0.0, std::min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)));
    return dist(x, y, ax + t * dx, ay + t * dy) <= width / 2;
}
bool ring(double x, double y, double cx, double cy, double radius, double width = 1.35) {
    return std::abs(dist(x, y, cx, cy) - radius) <= width / 2;
}
bool rect(double x, double y, double l, double t, double r, double b) {
    return x >= l && x <= r && y >= t && y <= b;
}
DWORD foreground() {
    COLORREF c = GetSysColor(COLOR_MENUTEXT);
    return 0xff000000 | (static_cast<DWORD>(GetRValue(c)) << 16) | (static_cast<DWORD>(GetGValue(c)) << 8) | GetBValue(c);
}
DWORD sample(Kind kind, const icon_style::Style& scheme, double x, double y, DWORD ink) {
    bool on = false;
    switch (kind) {
    case Kind::scheme: {
        double radius = dist(x, y, 8, 8);
        if (radius > 7) return 0;
        if (radius > 6.25) return 0xff35434a;
        // A small lightning mark corresponds to the tray icon.
        bool bolt = (y >= 3 && y <= 8.2 && x >= 8.8 - (y - 3) * .46 && x <= 10.6 - (y - 3) * .24) ||
                    (y >= 7 && y <= 13 && x >= 5.3 + (13 - y) * .25 && x <= 7.1 + (13 - y) * .48);
        return bolt ? (scheme.dark_bolt ? 0xff13272a : 0xfff8fbff) : scheme.argb;
    }
    case Kind::display:
        on = line(x,y,3,4,13,4) || line(x,y,3,4,3,10) || line(x,y,13,4,13,10) || line(x,y,3,10,13,10) ||
             line(x,y,8,10.5,8,12.7) || line(x,y,5.5,13,10.5,13); break;
    case Kind::sleep:
        on = dist(x,y,7.1,8) <= 5 && dist(x,y,9.7,5.5) >= 4.7; break;
    case Kind::settings: {
        double d = dist(x,y,8,8);
        on = (d >= 2.4 && d <= 4.4) ||
             ((rect(x,y,7.3,1.9,8.7,4.1) || rect(x,y,7.3,11.9,8.7,14.1) ||
               rect(x,y,1.9,7.3,4.1,8.7) || rect(x,y,11.9,7.3,14.1,8.7) ||
               line(x,y,3.7,3.7,5.2,5.2) || line(x,y,10.8,10.8,12.3,12.3) ||
               line(x,y,3.7,12.3,5.2,10.8) || line(x,y,10.8,5.2,12.3,3.7)) && d >= 3.2); break;
    }
    case Kind::windows:
        on = rect(x,y,2.5,3,7.1,7.3) || rect(x,y,8.4,3,13,7.3) || rect(x,y,2.5,8.7,7.1,13) || rect(x,y,8.4,8.7,13,13); break;
    case Kind::exit:
        on = line(x,y,7.3,3,3.4,3) || line(x,y,3.4,3,3.4,13) || line(x,y,3.4,13,7.3,13) ||
             line(x,y,6,8,13,8) || line(x,y,10.5,5.5,13,8) || line(x,y,10.5,10.5,13,8); break;
    case Kind::clock:
        on = ring(x,y,8,8,5.4) || line(x,y,8,4.8,8,8) || line(x,y,8,8,10.3,9.4); break;
    case Kind::never:
        on = ring(x,y,5.4,8,2.4) || ring(x,y,10.6,8,2.4) || line(x,y,7,6.3,9,9.7) || line(x,y,7,9.7,9,6.3); break;
    case Kind::custom:
        on = (line(x,y,4,12,11.9,4.1,2.2) && !line(x,y,11.1,3.2,12.8,4.9,.7)) ||
             line(x,y,3.7,12.3,3,13.4,1.2); break;
    case Kind::undo:
        on = (ring(x,y,8.8,8.3,4.5) && (x >= 6 || y >= 8)) ||
             line(x,y,3.1,4.8,3.1,8.2) || line(x,y,3.1,8.2,6.5,8.2) || line(x,y,3.1,8.2,5.5,5.8); break;
    case Kind::about:
        on = ring(x,y,8,8,5.7) || dist(x,y,8,5.1) <= .8 || line(x,y,8,7.2,8,11.3,1.5); break;
    }
    return on ? ink : 0;
}
}
HBITMAP make_bitmap(Kind kind, const icon_style::Style& scheme, int size) {
    if (size <= 0 || size > 256) return nullptr;
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); return nullptr; }
    DWORD* pixels = static_cast<DWORD*>(bits);
    DWORD ink = foreground();
    constexpr int sub = 4, samples = sub * sub;
    for (int py = 0; py < size; ++py) for (int px = 0; px < size; ++px) {
        unsigned a = 0, r = 0, g = 0, b = 0;
        for (int sy = 0; sy < sub; ++sy) for (int sx = 0; sx < sub; ++sx) {
            double x = (px + (sx + 0.5) / sub) * 16 / size;
            double y = (py + (sy + 0.5) / sub) * 16 / size;
            DWORD c = sample(kind, scheme, x, y, ink);
            if (c) { a += 255; r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255; }
        }
        pixels[py * size + px] = ((a / samples) << 24) | ((r / samples) << 16) | ((g / samples) << 8) | (b / samples);
    }
    return bitmap;
}
}
