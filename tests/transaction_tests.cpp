#include "../src/power_transaction.h"
#include <iostream>
#include <stdexcept>

struct FakeAccess : power::ValueAccess {
    DWORD ac=600, dc=180;
    bool change_dc_after_ac=false, change_ac_after_ac=false, change_dc_after_dc=false, fail_dc=false;
    int ac_reads=0, change_ac_at_read=0, fail_ac_at_read=0;
    DWORD read(bool is_ac) override {
        if (is_ac) {
            ++ac_reads;
            if (ac_reads == fail_ac_at_read) throw std::runtime_error("AC read failed");
            if (ac_reads == change_ac_at_read) ac=1300;
        }
        return is_ac ? ac : dc;
    }
    void write(bool is_ac, DWORD value) override {
        if (!is_ac && fail_dc) throw std::runtime_error("DC write failed");
        (is_ac ? ac : dc) = value;
        if (is_ac && value == 300 && change_dc_after_ac) dc=1200;
        if (is_ac && value == 300 && change_ac_after_ac) ac=1300;
        if (!is_ac && value == 300 && change_dc_after_dc) dc=1200;
    }
};
int main() {
    try {
        FakeAccess a;
        power::write_pair_if_unchanged(a,600,180,300);
        if (a.ac != 300 || a.dc != 300) throw std::runtime_error("pair write failed");
        a.ac=600; a.dc=181;
        try { power::write_pair_if_unchanged(a,600,180,300); throw std::runtime_error("initial conflict ignored"); }
        catch (const power::conflict_error&) {}
        if (a.ac != 600 || a.dc != 181) throw std::runtime_error("initial external change overwritten");
        a.ac=600; a.dc=180; a.change_dc_after_ac=true;
        try { power::write_pair_if_unchanged(a,600,180,300); throw std::runtime_error("late DC conflict ignored"); }
        catch (const power::conflict_error&) {}
        if (a.ac != 600 || a.dc != 1200) throw std::runtime_error("late external change overwritten");
        a.ac=600; a.dc=300;
        try { power::write_pair_if_unchanged(a,600,300,300); throw std::runtime_error("concurrent unchanged DC conflict ignored"); }
        catch (const power::conflict_error&) {}
        if (a.ac != 600 || a.dc != 1200) throw std::runtime_error("AC was not rolled back after concurrent DC change");
        {
            FakeAccess changed_source;
            changed_source.ac=600; changed_source.dc=300; changed_source.change_ac_after_ac=true;
            try { power::write_pair_if_unchanged(changed_source,600,300,300); throw std::runtime_error("changed AC reported as successful when DC was already set"); }
            catch (const power::partial_write_error&) {}
            if (changed_source.ac != 1300 || changed_source.dc != 300) throw std::runtime_error("external AC value was overwritten");
        }
        {
            FakeAccess changed_source;
            changed_source.change_dc_after_dc=true;
            try { power::write_pair_if_unchanged(changed_source,600,180,300); throw std::runtime_error("changed DC reported as successful after write"); }
            catch (const power::partial_write_error&) {}
            if (changed_source.ac != 300 || changed_source.dc != 1200) throw std::runtime_error("external DC value was overwritten");
        }
        {
            FakeAccess unreadable;
            unreadable.ac=300; unreadable.fail_ac_at_read=3;
            try { power::write_pair_if_unchanged(unreadable,300,180,300); throw std::runtime_error("final read failure reported as successful"); }
            catch (const power::partial_write_error&) {}
            if (unreadable.ac != 300 || unreadable.dc != 300) throw std::runtime_error("final read failure unexpectedly changed values");
        }
        a.ac=600; a.dc=180; a.change_ac_after_ac=true;
        try { power::write_pair_if_unchanged(a,600,180,300); throw std::runtime_error("partial failure ignored"); }
        catch (const power::partial_write_error&) {}
        if (a.ac != 1300 || a.dc != 1200) throw std::runtime_error("changed AC was overwritten during rollback");
        a.change_dc_after_ac=false; a.change_ac_after_ac=false; a.ac=600; a.dc=180; a.fail_dc=true;
        try { power::write_pair_if_unchanged(a,600,180,300); throw std::runtime_error("DC failure ignored"); }
        catch (const power::partial_write_error&) { throw; }
        catch (const std::runtime_error& ex) { if (std::string(ex.what()) != "DC write failed") throw; }
        if (a.ac != 600 || a.dc != 180) throw std::runtime_error("failed pair write was not rolled back");
        a.ac=300; a.dc=180; a.fail_dc=false; a.ac_reads=0; a.change_ac_at_read=2;
        try { power::write_pair_if_unchanged(a,300,180,300); throw std::runtime_error("concurrent unchanged-source conflict ignored"); }
        catch (const power::conflict_error&) {}
        if (a.ac != 1300 || a.dc != 180) throw std::runtime_error("other source changed after concurrent AC change");
        std::cout << "transaction tests passed\n"; return 0;
    } catch (const std::exception& ex) { std::cerr << "transaction test failed: " << ex.what() << '\n'; return 1; }
}
