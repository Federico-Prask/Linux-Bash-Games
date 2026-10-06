// 生成 hero.md 的武将表格与按包技能清单（数据来源：HeroRegistry 与各技能 getDescription）。
// 用法：g++ -std=c++17 -Iinclude tools/gen_hero_md.cpp src/*.o -o /tmp/gen_hero_md && /tmp/gen_hero_md > hero.md
#include "HeroRegistry.h"
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

using namespace Thks;

namespace {

std::string countryZh(Country c) {
    switch (c) {
        case Country::WEI: return "魏";
        case Country::SHU: return "蜀";
        case Country::WU: return "吴";
        case Country::QUN: return "群";
        default: return "?";
    }
}

// 展示用包顺序（其余包按字典序追加）
const std::vector<std::string> kPackOrder = {
    "标准包", "风包", "火包", "林包", "山包", "界限突破", "一将成名", "DIY包",
    "始计篇·智", "谋攻篇", "势包", "友", "神话再临·阴", "身份武将",
};

} // namespace

int main() {
    std::map<std::string, std::vector<HeroInfo>> byPack;
    std::vector<HeroInfo> all;
    for (auto& info : HeroRegistry::all()) {
        byPack[info.pack].push_back(info);
        all.push_back(info);
    }

    // ---------- 总表 ----------
    std::cout << "## 武将总表（" << all.size() << " 名）\n\n";
    std::cout << "| # | 扩展包 | 武将 | 称号 | 势力 | 体力 | 技能 |\n";
    std::cout << "| --- | --- | --- | --- | --- | --- | --- |\n";
    int idx = 0;
    for (auto& pack : kPackOrder) {
        auto it = byPack.find(pack);
        if (it == byPack.end()) continue;
        for (auto& info : it->second) {
            auto hero = info.create();
            std::string skills;
            if (hero) {
                bool first = true;
                for (auto& sk : hero->getSkills()) {
                    if (!first) skills += "、";
                    skills += "【" + sk->getName() + "】";
                    first = false;
                }
            }
            std::cout << "| " << ++idx << " | " << pack << " | " << info.name << " | " << info.title
                      << " | " << countryZh(info.country) << " | " << info.maxHp << " | " << skills << " |\n";
        }
    }

    // ---------- 按包技能清单 ----------
    std::cout << "\n## 按包技能清单（技能描述为三国杀移动版官网原文）\n";
    for (auto& pack : kPackOrder) {
        auto it = byPack.find(pack);
        if (it == byPack.end()) continue;
        std::cout << "\n### " << pack << "（" << it->second.size() << " 名）\n";
        for (auto& info : it->second) {
            auto hero = info.create();
            std::cout << "\n#### " << info.name << "（";
            if (!info.title.empty()) std::cout << info.title << "，";
            std::cout << countryZh(info.country) << " " << info.maxHp << "）\n\n";
            if (!hero || hero->getSkills().empty()) {
                std::cout << "- （无技能或技能资料待核对）\n";
                continue;
            }
            for (auto& sk : hero->getSkills()) {
                std::cout << "- 【" << sk->getName() << "】" << sk->getDescription() << "\n";
            }
        }
    }
    return 0;
}
