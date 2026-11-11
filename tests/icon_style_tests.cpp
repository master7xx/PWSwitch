#include "../src/icon_style.h"
#include "../src/icon_art.h"
#include "../src/menu_art.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

std::vector<DWORD> icon_pixels(HICON icon, int& width, int& height) {
    ICONINFO info{};
    if (!GetIconInfo(icon, &info)) throw std::runtime_error("icon pixels unavailable");
    BITMAP bitmap{};
    if (!info.hbmColor || !GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap)) {
        if (info.hbmColor) DeleteObject(info.hbmColor);
        if (info.hbmMask) DeleteObject(info.hbmMask);
        throw std::runtime_error("icon bitmap unavailable");
    }
    width = bitmap.bmWidth; height = bitmap.bmHeight;
    BITMAPINFO format{}; format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    format.bmiHeader.biWidth = width; format.bmiHeader.biHeight = -height;
    format.bmiHeader.biPlanes = 1; format.bmiHeader.biBitCount = 32; format.bmiHeader.biCompression = BI_RGB;
    std::vector<DWORD> pixels(static_cast<size_t>(width) * height);
    HDC screen = GetDC(nullptr);
    int rows = screen ? GetDIBits(screen, info.hbmColor, 0, height, pixels.data(), &format, DIB_RGB_COLORS) : 0;
    if (screen) ReleaseDC(nullptr, screen);
    DeleteObject(info.hbmColor); if (info.hbmMask) DeleteObject(info.hbmMask);
    if (rows != height) throw std::runtime_error("cannot read icon bitmap");
    return pixels;
}

int main() {
    try {
        const GUID custom = {0x12345678,0x1234,0x1234,{0x80,1,2,3,4,5,6,7}};
        const GUID balanced = {0x381b4222,0xf694,0x41f0,{0x96,0x85,0xff,0x5b,0xb2,0x60,0xdf,0x2e}};
        const GUID high = {0x8c5e7fda,0xe8bf,0x4a96,{0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c}};
        const GUID saver = {0xa1841308,0x3541,0x4fab,{0xbc,0x81,0xf7,0x15,0x56,0xf2,0x0b,0x4a}};
        auto expect = [](icon_style::Level actual, icon_style::Level wanted) {
            if (actual != wanted) throw std::runtime_error("unexpected scheme color category");
        };
        expect(icon_style::classify(balanced, L"fixture-standard-01"), icon_style::Level::balanced);
        expect(icon_style::classify(high, L"fixture-standard-02"), icon_style::Level::performance);
        expect(icon_style::classify(saver, L"fixture-standard-03"), icon_style::Level::saving);
        struct NameCase { const wchar_t* name; icon_style::Level expected; };
        const NameCase cases[] = {
            {L"fixture-ru / производ / максим", icon_style::Level::ultimate},
            {L"fixture-ru / производ / normal", icon_style::Level::performance},
            {L"fixture-ru / баланс / normal", icon_style::Level::balanced},
            {L"fixture-ru / эконом / normal", icon_style::Level::saving},
            {L"fixture-ru / очень / эконом", icon_style::Level::maximum_saving},
            {L"fixture-en / ULTIMATE / PERFORMANCE", icon_style::Level::ultimate},
            {L"fixture-en / Extreme / powersav", icon_style::Level::maximum_saving},
            {L"fixture-eco / eco / normal", icon_style::Level::saving},
            {L"fixture-ru / эко / normal", icon_style::Level::saving},
            {L"fixture-ru / не эконом / normal", icon_style::Level::unknown},
            {L"fixture / second / normal", icon_style::Level::unknown},
            {L"fixture / performance / recovery", icon_style::Level::performance},
            {L"fixture / neutral / normal", icon_style::Level::unknown},
            {L"fixture-ru / эконом / производ", icon_style::Level::unknown},
            {L"fixture-es / ahorro / normal", icon_style::Level::saving},
            {L"fixture-es / máximo / ahorro", icon_style::Level::maximum_saving},
            {L"fixture-es / MAXIMO / AHORRO", icon_style::Level::maximum_saving},
            {L"fixture-es / economizador / normal", icon_style::Level::saving},
            {L"fixture-es / rendimiento / alto", icon_style::Level::performance},
            {L"fixture-es / MÁXIMO / RENDIMIENTO", icon_style::Level::ultimate},
            {L"fixture-es / maximo / rendimiento", icon_style::Level::ultimate},
            {L"fixture-es / equilibrado / normal", icon_style::Level::balanced},
            {L"fixture-es / equilibrada / normal", icon_style::Level::balanced},
            {L"fixture-es / ahorro / rendimiento", icon_style::Level::unknown},
            {L"fixture-es / equilibrado / ahorro", icon_style::Level::unknown},
            {L"fixture-mixed / powersav / rendimiento", icon_style::Level::unknown},
            {L"fixture-es / sin / ahorro", icon_style::Level::unknown},
            {L"fixture-es / no / máximo / rendimiento", icon_style::Level::unknown},
            {L"fixture-es / nunca / ahorro", icon_style::Level::unknown},
            {L"fixture-es / ni / ahorro / rendimiento", icon_style::Level::unknown},
            {L"fixture-es / no / equilibrado", icon_style::Level::unknown},
            {L"fixture-es / sin / eco", icon_style::Level::unknown},
            {L"fixture-mixed / no / performance", icon_style::Level::unknown},
            {L"fixture-es / desequilibrado / normal", icon_style::Level::unknown},
            {L"fixture-es / desahorro / normal", icon_style::Level::unknown},
            {L"fixture-es / sincronizado / ahorro", icon_style::Level::saving},
            {L"fixture-es / novedad / rendimiento", icon_style::Level::performance},
            {L"fixture-es / normal / unit", icon_style::Level::unknown},
        };
        for (const auto& item : cases) {
            if (icon_style::classify(custom, item.name) != item.expected)
                throw std::runtime_error("Synthetic power plan name was misclassified");
        }
        auto lime = icon_style::style(icon_style::Level::maximum_saving);
        auto red = icon_style::style(icon_style::Level::ultimate);
        {
            HICON tray = icon_art::make_icon(icon_style::style(icon_style::Level::balanced));
            if (!tray) throw std::runtime_error("balanced tray icon unavailable");
            int width = 0, height = 0;
            auto tray_pixels = icon_pixels(tray, width, height);
            DestroyIcon(tray);
            HICON application = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, width, height, 0));
            if (!application) throw std::runtime_error("application icon resource unavailable");
            int app_width = 0, app_height = 0;
            auto application_pixels = icon_pixels(application, app_width, app_height);
            DestroyIcon(application);
            if (width != app_width || height != app_height || tray_pixels != application_pixels)
                throw std::runtime_error("balanced tray icon differs from application icon");
        }
        if (lime.argb == red.argb || lime.label.empty() || red.label.empty()) throw std::runtime_error("styles must be distinct and labelled");
        for (const auto& style : {lime, icon_style::style(icon_style::Level::saving),
                icon_style::style(icon_style::Level::balanced), icon_style::style(icon_style::Level::performance),
                red, icon_style::style(icon_style::Level::unknown)}) {
            HICON icon = icon_art::make_icon(style);
            if (!icon) throw std::runtime_error("icon creation failed");
            int width = 0, height = 0;
            auto pixels = icon_pixels(icon, width, height);
            DestroyIcon(icon);
            if (width < 16 || height < 16 || std::find(pixels.begin(), pixels.end(), style.argb) == pixels.end())
                throw std::runtime_error("rendered icon does not contain expected scheme color");
            if (std::none_of(pixels.begin(), pixels.end(), [](DWORD pixel) { return (pixel >> 24) == 0; }))
                throw std::runtime_error("icon transparency missing");
        }
        std::vector<std::vector<DWORD>> menu_images;
        for (const auto& kind : {menu_art::Kind::scheme, menu_art::Kind::display, menu_art::Kind::sleep,
                menu_art::Kind::settings, menu_art::Kind::windows, menu_art::Kind::exit,
                menu_art::Kind::clock, menu_art::Kind::never, menu_art::Kind::custom, menu_art::Kind::undo, menu_art::Kind::about}) {
            HBITMAP bitmap = menu_art::make_bitmap(kind, lime);
            if (!bitmap) throw std::runtime_error("menu icon creation failed");
            DIBSECTION dib{};
            if (GetObjectW(bitmap, sizeof(dib), &dib) != sizeof(dib) || !dib.dsBm.bmBits) {
                DeleteObject(bitmap); throw std::runtime_error("menu icon pixels unavailable");
            }
            const auto* pixels = static_cast<const DWORD*>(dib.dsBm.bmBits);
            std::vector<DWORD> image(pixels, pixels + 16 * 16);
            DeleteObject(bitmap);
            size_t visible = 0;
            for (DWORD c : image) {
                unsigned a = c >> 24;
                if (a) ++visible;
                if (((c >> 16) & 255) > a || ((c >> 8) & 255) > a || (c & 255) > a)
                    throw std::runtime_error("menu icon alpha is not premultiplied");
            }
            if (visible < 8 || visible >= image.size()) throw std::runtime_error("menu icon shape is empty or lacks transparency");
            for (const auto& previous : menu_images) if (previous == image) throw std::runtime_error("distinct commands have identical icons");
            menu_images.push_back(std::move(image));
        }
        std::cout << "icon style tests passed\n";
        return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
