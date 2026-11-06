#include "../src/undo.h"
#include <iostream>
#include <stdexcept>

namespace {
const GUID A = {1,2,3,{1,2,3,4,5,6,7,8}}, B = {2,2,3,{1,2,3,4,5,6,7,8}}, C = {3,2,3,{1,2,3,4,5,6,7,8}};
void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
struct Fake : undo::Backend {
    GUID active_plan = B; DWORD ac = 300, dc = 300;
    bool fail_dc_once = false, fail_activate = false, change_ac_after_write = false, change_dc_after_write = false;
    int reads = 0, change_dc_at_read = 0, change_ac_at_read = 0, fail_at_read = 0;
    GUID active() override { return active_plan; }
    DWORD read(const GUID&, const GUID&, const GUID&, bool is_ac) override {
        ++reads; if (reads == fail_at_read) throw std::runtime_error("read failure");
        if (change_dc_at_read && reads == change_dc_at_read) dc = 1200;
        if (change_ac_at_read && reads == change_ac_at_read) ac = 1300;
        return is_ac ? ac : dc;
    }
    void write(const GUID&, const GUID&, const GUID&, bool is_ac, DWORD n) override {
        if (!is_ac && fail_dc_once) { fail_dc_once = false; throw std::runtime_error("write failure"); }
        (is_ac ? ac : dc) = n;
        if (is_ac && change_ac_after_write) ac = 1300;
        if (!is_ac && change_dc_after_write) dc = 1200;
    }
    void activate(const GUID& id) override { if (fail_activate) throw std::runtime_error("activation failure"); active_plan = id; }
};
undo::ValueChange both() {
    undo::ValueChange c{}; c.scheme = B; c.group = A; c.setting = C;
    c.ac = c.dc = true; c.before_ac = 600; c.before_dc = 180; c.after_ac = c.after_dc = 300; return c;
}
}
int main() {
    try {
        Fake f; undo::Journal j;
        j.record_scheme(A, B, L"scheme"); f.active_plan = C;
        require(j.restore(f) == undo::Result::conflict && IsEqualGUID(f.active_plan,C) && j.available(), "external scheme change overwritten");
        f.active_plan = B;
        require(j.restore(f) == undo::Result::restored && IsEqualGUID(f.active_plan,A) && !j.available(), "scheme undo failed");
        f.active_plan = B; f.ac = 300; f.dc = 301; j.record_value(both(), L"timeout");
        require(j.restore(f) == undo::Result::conflict && f.ac == 300 && f.dc == 301 && j.available(), "external timeout change overwritten");
        f.dc = 300;
        require(j.restore(f) == undo::Result::restored && f.ac == 600 && f.dc == 180 && !j.available(), "distinct AC/DC values not restored");
        auto one = both(); one.ac = false; f.ac = 42; f.dc = 300; j.record_value(one, L"dc");
        require(j.restore(f) == undo::Result::restored && f.ac == 42 && f.dc == 180, "unaffected power source changed");
        f.ac = f.dc = 300; f.fail_dc_once = true; j.record_value(both(), L"timeout");
        try { (void)j.restore(f); throw std::runtime_error("write failure was ignored"); } catch (const std::runtime_error& e) { require(std::string(e.what()) == "write failure", "wrong failure"); }
        require(f.ac == 300 && f.dc == 300 && j.available(), "failed undo did not roll back");
        f.reads = 0; f.change_dc_at_read = 4;
        require(j.restore(f) == undo::Result::conflict && f.ac == 300 && f.dc == 1200 && j.available(), "undo overwrote a concurrent DC change");
        f.change_dc_at_read = 0; f.dc = 300;
        f.fail_activate = true;
        require(j.restore(f) == undo::Result::restored_not_applied && f.ac == 600 && f.dc == 180 && !j.available(), "apply failure did not report restored values");
        f.fail_activate = false; f.reads = 0; f.ac = f.dc = 300; f.change_ac_at_read = 4; j.record_value(both(), L"timeout");
        try { (void)j.restore(f); throw std::runtime_error("concurrent AC change ignored"); }
        catch (const std::runtime_error& e) { require(std::string(e.what()) != "concurrent AC change ignored", "concurrent AC change ignored"); }
        require(f.ac == 1300 && f.dc == 300 && !j.available(), "partial undo overwrote external change or retained stale undo");
        {
            Fake external; undo::Journal single;
            auto ac_only = both(); ac_only.dc = false;
            external.change_ac_after_write = true;
            single.record_value(ac_only, L"ac");
            try { (void)single.restore(external); throw std::runtime_error("single-source undo reported success after external change"); }
            catch (const std::runtime_error& e) { require(std::string(e.what()) != "single-source undo reported success after external change", "single-source undo reported success after external change"); }
            require(external.ac == 1300 && external.dc == 300 && !single.available(), "single-source conflict overwrote external value or retained stale undo");
        }
        {
            Fake external; undo::Journal pair;
            external.change_dc_after_write = true;
            pair.record_value(both(), L"pair");
            try { (void)pair.restore(external); throw std::runtime_error("pair undo reported success after external DC change"); }
            catch (const std::runtime_error& e) { require(std::string(e.what()) != "pair undo reported success after external DC change", "pair undo reported success after external DC change"); }
            require(external.ac == 600 && external.dc == 1200 && !pair.available(), "pair conflict overwrote external values or retained stale undo");
        }
        {
            Fake unreadable; undo::Journal pair;
            unreadable.fail_at_read = 6;
            pair.record_value(both(), L"pair");
            try { (void)pair.restore(unreadable); throw std::runtime_error("final undo read failure reported as successful"); }
            catch (const std::runtime_error& e) { require(std::string(e.what()) != "final undo read failure reported as successful", "final undo read failure reported as successful"); }
            require(unreadable.ac == 600 && unreadable.dc == 180 && !pair.available(), "final undo read failure retained stale journal or changed values");
        }
        std::cout << "undo tests passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "undo test failed: " << e.what() << '\n'; return 1; }
}
