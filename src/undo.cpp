#include "undo.h"

namespace undo {
void Journal::record_scheme(const GUID& before, const GUID& after, const std::wstring& description) {
    if (IsEqualGUID(before, after)) return;
    before_scheme_ = before; after_scheme_ = after; description_ = description; kind_ = Kind::scheme;
}
void Journal::record_value(const ValueChange& change, const std::wstring& description) {
    ValueChange value = change;
    value.ac = value.ac && value.before_ac != value.after_ac;
    value.dc = value.dc && value.before_dc != value.after_dc;
    if (!value.ac && !value.dc) return;
    value_ = value; description_ = description; kind_ = Kind::value;
}
Result Journal::restore(Backend& backend) {
    if (kind_ == Kind::none) return Result::none;
    if (kind_ == Kind::scheme) {
        if (!IsEqualGUID(backend.active(), after_scheme_)) return Result::conflict;
        backend.activate(before_scheme_);
        kind_ = Kind::none; description_.clear();
        return Result::restored;
    }
    const auto change = value_;
    if ((change.ac && backend.read(change.scheme, change.group, change.setting, true) != change.after_ac) ||
        (change.dc && backend.read(change.scheme, change.group, change.setting, false) != change.after_dc)) return Result::conflict;
    bool wrote_ac = false;
    auto rollback_ac = [&]() {
        if (!wrote_ac) return;
        try {
            if (backend.read(change.scheme, change.group, change.setting, true) != change.before_ac)
                throw std::runtime_error("Setting changed while undoing");
            backend.write(change.scheme, change.group, change.setting, true, change.after_ac);
        } catch (...) {
            clear();
            throw std::runtime_error("Не удалось полностью отменить изменение или восстановить предыдущее состояние. Проверьте настройки схемы.");
        }
    };
    struct ConcurrentChange {};
    try {
        if (change.ac) {
            if (backend.read(change.scheme, change.group, change.setting, true) != change.after_ac) throw ConcurrentChange{};
            backend.write(change.scheme, change.group, change.setting, true, change.before_ac); wrote_ac = true;
        }
        if (change.dc) {
            if (change.ac && backend.read(change.scheme, change.group, change.setting, true) != change.before_ac) throw ConcurrentChange{};
            if (backend.read(change.scheme, change.group, change.setting, false) != change.after_dc) throw ConcurrentChange{};
            backend.write(change.scheme, change.group, change.setting, false, change.before_dc);
        }
    } catch (const ConcurrentChange&) {
        rollback_ac(); return Result::conflict;
    } catch (...) {
        rollback_ac(); throw;
    }
    try {
        if ((change.ac && backend.read(change.scheme, change.group, change.setting, true) != change.before_ac) ||
            (change.dc && backend.read(change.scheme, change.group, change.setting, false) != change.before_dc))
            throw std::runtime_error("Setting changed while undoing");
    } catch (...) {
        clear();
        throw std::runtime_error("Значения могли измениться, но не удалось подтвердить отмену; активная схема могла не примениться. Проверьте настройки схемы; автоматическая отмена недоступна.");
    }
    kind_ = Kind::none; description_.clear();
    try {
        if (IsEqualGUID(backend.active(), change.scheme)) backend.activate(change.scheme);
    } catch (...) { return Result::restored_not_applied; }
    return Result::restored;
}
}
