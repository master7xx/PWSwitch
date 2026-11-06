#pragma once
#include "power.h"

namespace power {
struct conflict_error : std::runtime_error { using std::runtime_error::runtime_error; };
struct ValueAccess {
    virtual ~ValueAccess() = default;
    virtual DWORD read(bool ac) = 0;
    virtual void write(bool ac, DWORD value) = 0;
};
void write_pair_if_unchanged(ValueAccess& access, DWORD expected_ac, DWORD expected_dc, DWORD value);
}
