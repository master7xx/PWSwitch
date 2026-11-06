#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <stdexcept>

namespace power {
struct partial_write_error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Item { GUID id; std::wstring name; };
struct Option { DWORD value; std::wstring name; bool named = true; };
struct Setting {
    GUID group, id;
    std::wstring name, description, units;
    bool hidden = false;
    bool range = false;
    DWORD minimum = 0, maximum = 0, increment = 1;
    std::vector<Option> options;
};
GUID active_scheme();
std::vector<Item> schemes();
std::vector<Item> groups(const GUID& scheme);
std::vector<Setting> settings(const GUID& scheme, const GUID& group);
DWORD read_value(const GUID& scheme, const GUID& group, const GUID& setting, bool ac);
void write_value(const GUID& scheme, const GUID& group, const GUID& setting, bool ac, DWORD value);
void write_both(const GUID& scheme, const GUID& group, const GUID& setting, DWORD value);
void write_both_checked(const GUID& scheme, const GUID& group, const GUID& setting, DWORD value, DWORD expected_ac, DWORD expected_dc);
void activate(const GUID& scheme);
std::wstring error_text(DWORD error);
}
