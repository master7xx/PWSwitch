#include "../src/i18n.h"
#include <cwchar>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
using i18n::Language;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expect_text(const wchar_t* key, const wchar_t* expected) {
    require(std::wstring(i18n::text(key)) == expected, "unexpected translation");
}

void check_language_resolution() {
    require(i18n::resolve(MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT)) == Language::russian, "Russian UI language not resolved");
    require(i18n::resolve(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) == Language::english, "English UI language not resolved");
    require(i18n::resolve(MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH)) == Language::spanish, "Spanish UI language not resolved");
    require(i18n::resolve(MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MEXICAN)) == Language::spanish, "Spanish regional language not resolved");
    require(i18n::resolve(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT)) == Language::english, "unsupported UI language must fall back to English");
}

void check_catalog() {
    const i18n::Entry* entries = i18n::entries();
    const size_t count = i18n::entry_count();
    require(entries && count > 0, "translation catalog is empty");
    std::set<std::wstring> keys;
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = entries[i];
        require(entry.key && *entry.key, "catalog contains an empty key");
        require(entry.english && *entry.english, "catalog contains an empty English translation");
        require(entry.spanish && *entry.spanish, "catalog contains an empty Spanish translation");
        require(keys.insert(entry.key).second, "catalog contains a duplicate key");
        i18n::set_for_testing(Language::russian);
        require(std::wstring(i18n::text(entry.key)) == entry.key, "Russian lookup does not match catalog");
        i18n::set_for_testing(Language::english);
        require(std::wstring(i18n::text(entry.key)) == entry.english, "English lookup does not match catalog");
        i18n::set_for_testing(Language::spanish);
        require(std::wstring(i18n::text(entry.key)) == entry.spanish, "Spanish lookup does not match catalog");
    }
    // Different keys may legitimately have the same translation, e.g. labels
    // used in different contexts. Keys, rather than translated text, are unique.
}

void check_lookup_and_preference() {
    i18n::set_for_testing(Language::russian);
    require(i18n::preference() == Language::russian && i18n::current() == Language::russian, "Russian preference not applied");
    expect_text(L"Применить", L"Применить");
    expect_text(L"О программе…", L"О программе…");
    // The source exception messages are English, so returning the untranslated
    // key is not a complete Russian translation.
    require(std::wstring(i18n::text(L"Windows returned no active scheme")) != L"Windows returned no active scheme", "missing Russian error translation");
    require(std::wstring(i18n::text(L"Active scheme is not available")) != L"Active scheme is not available", "missing Russian unavailable-plan translation");

    i18n::set_for_testing(Language::english);
    require(i18n::preference() == Language::english && i18n::current() == Language::english, "English preference not applied");
    expect_text(L"Применить", L"Apply");
    expect_text(L"О программе…", L"About…");
    expect_text(L"Переключение схем питания Windows из системного трея.", L"Switch Windows power plans from the notification area.");
    i18n::set_for_testing(Language::spanish);
    require(i18n::preference() == Language::spanish && i18n::current() == Language::spanish, "Spanish preference not applied");
    expect_text(L"Применить", L"Aplicar");
    expect_text(L"О программе…", L"Acerca de…");
    expect_text(L"Переключение схем питания Windows из системного трея.", L"Cambie los planes de energía de Windows desde el área de notificación.");
    expect_text(L"unrecognized key", L"unrecognized key");
    require(std::wstring(i18n::text(nullptr)).empty(), "null translation key must be safe");

    bool rejected = false;
    try { i18n::set_for_testing(static_cast<Language>(-1)); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid preference must be rejected");
    require(i18n::preference() == Language::spanish && i18n::current() == Language::spanish, "invalid preference changed the language");
    i18n::set_for_testing(Language::system);
    require(i18n::preference() == Language::system, "system preference was not preserved");
    require(i18n::current() == i18n::resolve(GetUserDefaultUILanguage()), "system preference does not follow Windows UI language");
}

void check_plan_names() {
    const GUID balanced = {0x381b4222,0xf694,0x41f0,{0x96,0x85,0xff,0x5b,0xb2,0x60,0xdf,0x2e}};
    const GUID custom = {0x12345678,0x1234,0x5678,{0x90,0xab,0xcd,0xef,0x12,0x34,0x56,0x78}};
    const std::wstring names[] = {L"fixture-001", L"fixture-002 / prueba", L"", L"  fixture-003  ", L"fixture-004 & <tag>"};
    for (auto language : {Language::russian, Language::english, Language::spanish}) {
        i18n::set_for_testing(language);
        for (const auto& name : names) {
            require(i18n::scheme_name(balanced, name) == name, "built-in GUID must preserve the exact system-provided name");
            require(i18n::scheme_name(custom, name) == name, "custom GUID must preserve the exact system-provided name");
        }
    }
}
}

int main() {
    try {
        check_language_resolution();
        check_catalog();
        check_lookup_and_preference();
        check_plan_names();
        std::cout << "i18n tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "i18n test failed: " << ex.what() << '\n';
        return 1;
    }
}
