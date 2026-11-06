#include "../src/labels.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        const GUID a={0x12345678,1,2,{1,2,3,4,5,6,7,8}};
        const GUID b={0x87654321,1,2,{1,2,3,4,5,6,7,8}};
        const GUID c={0x12345678,1,2,{1,2,3,4,5,6,7,9}};
        std::vector<power::Item> plans={{a,L"Копия"},{b,L"Копия"},{c,L"Другая"}};
        if (labels::scheme(plans,a,false)==labels::scheme(plans,b,false)) throw std::runtime_error("duplicate names are indistinguishable");
        if (labels::scheme(plans,c,false)!=L"Другая") throw std::runtime_error("unique name unnecessarily changed");
        plans={{a,L"Длинная одинаковая схема один"},{b,L"Длинная одинаковая схема два"}};
        if (labels::scheme(plans,a,true)==labels::scheme(plans,b,true)) throw std::runtime_error("truncated names are indistinguishable");
        plans={{a,L"Копия"},{c,L"Копия"}};
        if (labels::scheme(plans,a,true)==labels::scheme(plans,c,true)) throw std::runtime_error("identical GUID prefix is ambiguous");
        plans={{a,L"Work"},{b,L"Work"},{c,L"Work [12345678]"}};
        if (labels::scheme(plans,a,false)==labels::scheme(plans,c,false)) throw std::runtime_error("generated suffix collides with another real name");
        std::cout << "label tests passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
