#pragma once
#include "power.h"

namespace labels {
// Compact labels are used in the undo menu; full labels are used in scheme lists.
std::vector<std::wstring> all(const std::vector<power::Item>& plans, bool compact);
std::wstring scheme(const std::vector<power::Item>& plans, const GUID& id, bool compact);
}
