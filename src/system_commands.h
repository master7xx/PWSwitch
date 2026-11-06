#pragma once
#include <string>

namespace system_commands {
struct Command { std::wstring executable, arguments; };
Command power_options();
}
