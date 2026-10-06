#include "HeroRegistry.h"
#include <iostream>
#include <fstream>
using namespace Thks;
int main() {
    std::ofstream out("/tmp/skills_dump.tsv");
    for (auto& info : HeroRegistry::all()) {
        auto h = info.create();
        if (!h) continue;
        for (auto& sk : h->getSkills()) {
            out << info.id << "\t" << info.pack << "\t" << info.name << "\t"
                << sk->getName() << "\t" << sk->getDescription() << "\n";
        }
    }
    out.close();
    std::cout << "done\n";
    return 0;
}
