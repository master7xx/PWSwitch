#include "icon_style.h"
#include "i18n.h"

namespace icon_style {
namespace {
const GUID balanced = {0x381b4222,0xf694,0x41f0,{0x96,0x85,0xff,0x5b,0xb2,0x60,0xdf,0x2e}};
const GUID performance = {0x8c5e7fda,0xe8bf,0x4a96,{0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c}};
const GUID saving = {0xa1841308,0x3541,0x4fab,{0xbc,0x81,0xf7,0x15,0x56,0xf2,0x0b,0x4a}};
const GUID ultimate = {0xe9a42b02,0xd5df,0x448d,{0xaa,0x00,0x03,0xf1,0x47,0x49,0xeb,0x61}};
bool contains(const std::wstring& text, const wchar_t* part) { return text.find(part) != std::wstring::npos; }
bool contains_word(const std::wstring& text, const wchar_t* word) {
    std::wstring token(word);
    auto is_word = [](wchar_t c) {
        WORD kind = 0;
        return c == L'_' || (GetStringTypeW(CT_CTYPE1, &c, 1, &kind) && (kind & (C1_ALPHA | C1_DIGIT)) != 0);
    };
    for (size_t at = text.find(token); at != std::wstring::npos; at = text.find(token, at + 1)) {
        size_t end = at + token.size();
        if ((at == 0 || !is_word(text[at - 1])) && (end == text.size() || !is_word(text[end]))) return true;
    }
    return false;
}
std::wstring lower(const std::wstring& text) {
    if (text.empty()) return {};
    int count = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr, 0);
    if (!count) return text;
    std::wstring result(static_cast<size_t>(count), L'\0');
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(), static_cast<int>(text.size()), &result[0], count, nullptr, nullptr, 0)) return text;
    return result;
}
}
Level classify(const GUID& scheme, const std::wstring& name) {
    if (IsEqualGUID(scheme, balanced)) return Level::balanced;
    if (IsEqualGUID(scheme, performance)) return Level::performance;
    if (IsEqualGUID(scheme, saving)) return Level::saving;
    if (IsEqualGUID(scheme, ultimate)) return Level::ultimate;
    auto s = lower(name);
    if (contains(s, L"не эконом") || contains(s, L"без эконом") || contains(s, L"not power sav") || contains(s, L"no power sav")) return Level::unknown;
    bool spanish_saver = contains_word(s, L"ahorro") || contains_word(s, L"ahorrador") || contains_word(s, L"economizador");
    bool spanish_fast = contains_word(s, L"rendimiento");
    bool spanish_middle = contains_word(s, L"equilibrado") || contains_word(s, L"equilibrada");
    // A name is only a hint. Do not guess the intended category when it negates it.
    if (contains_word(s, L"no") || contains_word(s, L"sin") || contains_word(s, L"nunca") || contains_word(s, L"ni")) return Level::unknown;
    bool saver = contains(s, L"эконом") || contains(s, L"энергосбереж") || contains(s, L"энергосбер") ||
                 contains(s, L"power sav") || contains(s, L"powersav") || contains(s, L"energy sav") ||
                 contains(s, L"battery sav") || contains_word(s, L"eco") || contains_word(s, L"эко") || spanish_saver;
    bool fast = contains(s, L"производ") || contains(s, L"performance") || contains(s, L"perfomance") || spanish_fast;
    bool middle = contains(s, L"баланс") || contains(s, L"balanced") || contains(s, L"сбаланс") || spanish_middle;
    bool extreme = contains(s, L"очень") || contains(s, L"максим") || contains(s, L"супер") || contains(s, L"ультра") ||
                   contains(s, L"extreme") || contains(s, L"ultimate") || contains(s, L"maximum") || contains(s, L"ultra") || contains_word(s, L"very") || contains_word(s, L"max") ||
                   contains_word(s, L"máximo") || contains_word(s, L"máxima") || contains_word(s, L"maximo") || contains_word(s, L"maxima");
    if (static_cast<int>(saver) + static_cast<int>(fast) + static_cast<int>(middle) != 1) return Level::unknown;
    if (saver) return extreme ? Level::maximum_saving : Level::saving;
    if (fast) return extreme ? Level::ultimate : Level::performance;
    return Level::balanced;
}
Style style(Level level) {
    switch (level) {
    case Level::maximum_saving: return {0xffa6df30, true, i18n::text(L"Тип: сбережение +")};
    case Level::saving: return {0xff29a966, true, i18n::text(L"Тип: сбережение")};
    case Level::balanced: return {0xffe2bb3b, true, i18n::text(L"Тип: компромисс")};
    case Level::performance: return {0xffed8032, true, i18n::text(L"Тип: быстродействие")};
    case Level::ultimate: return {0xffe43838, false, i18n::text(L"Тип: быстродействие +")};
    default: return {0xff607d8b, false, i18n::text(L"Тип: неизвестен")};
    }
}
}
