#include "power_transaction.h"

namespace power {
void write_pair_if_unchanged(ValueAccess& access, DWORD expected_ac, DWORD expected_dc, DWORD value) {
    auto conflict = []() { return conflict_error("Значения изменились вне приложения. Откройте меню ещё раз."); };
    if (access.read(true) != expected_ac || access.read(false) != expected_dc) throw conflict();
    bool wrote_ac = false, wrote_dc = false;
    try {
        if (expected_ac != value) {
            if (access.read(true) != expected_ac) throw conflict();
            access.write(true, value); wrote_ac = true;
        }
        if (expected_dc != value) {
            if (access.read(true) != value) throw conflict();
            if (access.read(false) != expected_dc) throw conflict();
            access.write(false, value); wrote_dc = true;
        } else {
            if (access.read(true) != value || access.read(false) != expected_dc) throw conflict();
        }
    } catch (...) {
        if (wrote_ac) {
            try {
                if (access.read(true) != value) throw std::runtime_error("AC value changed during write");
                access.write(true, expected_ac);
            } catch (...) {
                throw partial_write_error("Не удалось завершить изменение или безопасно восстановить значение для сети. Проверьте настройки схемы; автоматическая отмена недоступна.");
            }
        }
        throw;
    }
    if (wrote_ac || wrote_dc) {
        try {
            if (access.read(true) != value || access.read(false) != value)
                throw std::runtime_error("Setting changed during write");
        } catch (...) {
            throw partial_write_error("Значения могли записаться, но не удалось подтвердить результат; активная схема могла не примениться. Проверьте настройки схемы; автоматическая отмена недоступна.");
        }
    }
}
}
