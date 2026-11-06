#pragma once
#include <windows.h>
#include <string>
#include <utility>
#include <vector>

namespace autostart {
struct Entry { bool exists = false; DWORD type = 0; std::vector<BYTE> data; };
struct Storage {
    virtual ~Storage() = default;
    virtual Entry read() = 0;
    virtual void write(const Entry& entry) = 0;
    virtual void remove() = 0;
};
enum class State { disabled, enabled, moved, foreign };
struct Snapshot { State state; Entry entry; std::wstring target; };
class RegistryStorage final : public Storage {
    HKEY root_;
    std::wstring key_path_;
public:
    explicit RegistryStorage(HKEY root = HKEY_CURRENT_USER,
        std::wstring key_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run")
        : root_(root), key_path_(std::move(key_path)) {}
    Entry read() override;
    void write(const Entry& entry) override;
    void remove() override;
};
std::wstring executable_path();
Snapshot inspect(Storage& storage, const std::wstring& executable);
void change(Storage& storage, const Snapshot& expected, const std::wstring& executable, bool enable);
}
