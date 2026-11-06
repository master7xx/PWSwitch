#pragma once
#include "power.h"

namespace undo {
enum class Result { none, restored, conflict, restored_not_applied };
struct ValueChange {
    GUID scheme{}, group{}, setting{};
    bool ac = false, dc = false;
    DWORD before_ac = 0, before_dc = 0, after_ac = 0, after_dc = 0;
};
struct Backend {
    virtual ~Backend() = default;
    virtual GUID active() = 0;
    virtual DWORD read(const GUID& scheme, const GUID& group, const GUID& setting, bool ac) = 0;
    virtual void write(const GUID& scheme, const GUID& group, const GUID& setting, bool ac, DWORD value) = 0;
    virtual void activate(const GUID& scheme) = 0;
};
class Journal {
    enum class Kind { none, scheme, value } kind_ = Kind::none;
    GUID before_scheme_{}, after_scheme_{};
    ValueChange value_{};
    std::wstring description_;
public:
    bool available() const { return kind_ != Kind::none; }
    const std::wstring& description() const { return description_; }
    void clear() { kind_ = Kind::none; description_.clear(); }
    void record_scheme(const GUID& before, const GUID& after, const std::wstring& description);
    void record_value(const ValueChange& value, const std::wstring& description);
    Result restore(Backend& backend);
};
}
