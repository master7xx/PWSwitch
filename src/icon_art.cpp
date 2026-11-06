#include "icon_art.h"

namespace icon_art {
HICON make_icon(const icon_style::Style& style) {
    int resource = 106;
    switch (style.argb) {
    case 0xffe2bb3b: resource = 101; break;
    case 0xffa6df30: resource = 102; break;
    case 0xff29a966: resource = 103; break;
    case 0xffed8032: resource = 104; break;
    case 0xffe43838: resource = 105; break;
    }
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(resource),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
}
}
