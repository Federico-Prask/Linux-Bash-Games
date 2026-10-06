// 输出武将登记表（id \t 名称 \t 扩展包 \t 势力 \t 体力），供 tools/gen_hero_tier.py 使用。
// 用法：g++ -std=c++17 -Iinclude tools/dump_heroes.cpp src/*.o -o /tmp/dump_heroes && /tmp/dump_heroes > /tmp/heroes.tsv
#include "HeroRegistry.h"
#include <iostream>
using namespace Thks;
int main() {
    for (const auto& info : HeroRegistry::all()) {
        std::cout << info.id << "\t" << info.name << "\t" << info.pack << "\t"
                  << HeroRegistry::countryName(info.country) << "\t" << info.maxHp << "\n";
    }
    return 0;
}
