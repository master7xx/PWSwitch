#pragma once
#include <windows.h>
#include <string>
#include <cstddef>

namespace i18n {
enum class Language { system, english, russian, spanish };
struct Entry { const wchar_t* key; const wchar_t* english; const wchar_t* spanish; };
Language resolve(LANGID windows_language);
Language current();
Language preference();
// Read/write only HKCU\Software\PWSwitch\Language. Invalid values use Windows language.
void initialize();
// Persist before changing state; failures throw and leave the current language intact.
void select(Language language);
void set_for_testing(Language language);
const wchar_t* text(const wchar_t* key);
std::wstring translate(const std::wstring& key);
const Entry* entries();
size_t entry_count();
// Preserve the exact name supplied by Windows for every GUID and UI language.
std::wstring scheme_name(const GUID& id, const std::wstring& name);
}
