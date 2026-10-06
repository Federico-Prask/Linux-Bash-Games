// Codex.cpp — 图鉴（模态百科）实现
//
// 设计约束：
//  - 只读 HeroRegistry（静态登记）与函数内临时牌堆，不引用 GameEngine，
//    不触碰任何对局状态；退出后游戏原样继续（状态保留）。
//  - 所有输入经 Interaction::readToken，EOF 时逐层 unwind，不死循环。
#include "Codex.h"
#include "Interaction.h"
#include "Platform.h"
#include <iostream>
#include <algorithm>

namespace Thks {

namespace {

// 武将技能名一览（单行）
std::string skillNames(const HeroInfo& info) {
    HeroPtr hero = info.create();
    std::string names;
    for (const auto& s : hero->getSkills()) {
        if (!names.empty()) names += " ";
        names += "【" + s->getName() + "】";
    }
    return names;
}

// 按包过滤：返回 all 的下标列表（空过滤 = 全部）
std::vector<size_t> filterHeroes(const std::vector<std::string>& packFilter) {
    const auto& all = HeroRegistry::all();
    std::vector<size_t> visible;
    for (size_t i = 0; i < all.size(); ++i) {
        if (packFilter.empty() ||
            std::find(packFilter.begin(), packFilter.end(), all[i].pack) != packFilter.end()) {
            visible.push_back(i);
        }
    }
    return visible;
}

std::string cardTypeName(CardType t) {
    switch (t) {
        case CardType::BASIC: return "基本牌";
        case CardType::TRICK: return "锦囊牌";
        case CardType::EQUIPMENT: return "装备牌";
        default: return "未知";
    }
}

// 卡牌图鉴流程；返回 false 表示输入已关闭（上层直接 unwind）
bool cardFlow() {
    auto catalog = Codex::buildCardCatalog();
    auto countKind = [&](CardType t) {
        int kinds = 0, total = 0;
        for (const auto& e : catalog) {
            if (e.type == t) { kinds++; total += e.count; }
        }
        return std::make_pair(kinds, total);
    };
    while (true) {
        auto basic = countKind(CardType::BASIC);
        auto trick = countKind(CardType::TRICK);
        auto equip = countKind(CardType::EQUIPMENT);
        std::cout << "\n=============================== 卡牌图鉴 ===============================\n";
        std::cout << "  [1] 基本牌（" << basic.first << "种" << basic.second << "张）\n";
        std::cout << "  [2] 锦囊牌（" << trick.first << "种" << trick.second << "张）\n";
        std::cout << "  [3] 装备牌（" << equip.first << "种" << equip.second << "张）\n";
        std::cout << "  [0] 返回\n> " << std::flush;
        std::string token = Interaction::readToken();
        if (token == "0" || Interaction::inputClosed()) return !Interaction::inputClosed();
        int idx = Interaction::parseInt(token);
        if (idx < 1 || idx > 3) {
            std::cout << "无效的输入！\n";
            continue;
        }
        CardType want = (idx == 1) ? CardType::BASIC : (idx == 2 ? CardType::TRICK : CardType::EQUIPMENT);
        std::cout << "\n----- " << cardTypeName(want) << " -----\n";
        for (const auto& e : catalog) {
            if (e.type != want) continue;
            std::cout << "  " << Platform::padRight(e.name, 12) << "×" << e.count;
            if (!e.desc.empty()) std::cout << " —— " << e.desc;
            std::cout << "\n";
        }
    }
}

// 武将图鉴流程（包分类 → 武将列表 → 详情）；返回 false 表示输入已关闭
bool heroFlow(const std::string& currentHeroId) {
    const auto& all = HeroRegistry::all();
    while (true) {
        // ---- 包分类 ----
        std::vector<PackCategory> cats;
        for (auto& c : HeroRegistry::packCategories()) {
            if (!filterHeroes(c.packs).empty()) cats.push_back(c);
        }
        std::cout << "\n=============================== 武将图鉴 ===============================\n";
        for (size_t i = 0; i < cats.size(); ++i) {
            std::cout << "  [" << (i + 1) << "] " << Platform::padRight(cats[i].name, 16)
                      << "（" << filterHeroes(cats[i].packs).size() << " 名）\n";
        }
        std::cout << "  [" << (cats.size() + 1) << "] " << Platform::padRight("全部武将", 16)
                  << "（" << all.size() << " 名）\n";
        std::cout << "  [0] 返回\n> " << std::flush;
        std::string token = Interaction::readToken();
        if (token == "0" || Interaction::inputClosed()) return !Interaction::inputClosed();
        int idx = Interaction::parseInt(token);
        std::vector<size_t> visible;
        std::string viewName;
        if (idx >= 1 && idx <= static_cast<int>(cats.size())) {
            const auto& cat = cats[idx - 1];
            auto selected = cat.packs;
            if (!cat.subpacks.empty()) {
                while (true) {
                    std::cout << "\n--- " << cat.name << " · 选择小包 ---\n";
                    for (size_t j = 0; j < cat.subpacks.size(); ++j)
                        std::cout << "  [" << j + 1 << "] " << cat.subpacks[j] << "（"
                                  << filterHeroes({cat.subpacks[j]}).size() << "名）\n";
                    std::cout << "  [" << cat.subpacks.size() + 1 << "] 全部五包  [0] 返回\n> " << std::flush;
                    int n = Interaction::parseInt(Interaction::readToken());
                    if (Interaction::inputClosed()) return false;
                    if (n == 0) { selected.clear(); break; }
                    if (n >= 1 && n <= static_cast<int>(cat.subpacks.size())) {
                        selected = {cat.subpacks[n - 1]}; break;
                    }
                    if (n == static_cast<int>(cat.subpacks.size()) + 1) break;
                }
            }
            if (selected.empty()) continue;
            visible = filterHeroes(selected);
            if (selected.size() > 1) {
                std::stable_sort(visible.begin(),visible.end(),[&](size_t a,size_t b) {
                    auto order=[&](const std::string& pack){
                        auto it=std::find(selected.begin(),selected.end(),pack);
                        return std::distance(selected.begin(),it);
                    };
                    return order(all[a].pack)<order(all[b].pack);
                });
            }
            viewName = selected.size() == 1 ? selected.front() : cat.name;
        } else if (idx == static_cast<int>(cats.size()) + 1) {
            visible = filterHeroes({});
            viewName = "全部武将";
        } else {
            std::cout << "无效的输入！\n";
            continue;
        }
        // ---- 武将列表 ----
        while (true) {
            std::cout << "\n----- 武将（" << viewName << "，" << visible.size() << "名，★=你的武将）-----\n";
            for (size_t vi = 0; vi < visible.size(); ++vi) {
                const auto& info = all[visible[vi]];
                std::string mark = (info.id == currentHeroId) ? "★" : " ";
                std::cout << "  " << mark << Platform::padRight("(" + std::to_string(vi + 1) + ")", 5) << " "
                          << Platform::padRight(info.name, 10)
                          << Platform::padRight(info.title, 16)
                          << HeroRegistry::countryName(info.country) << "  " << info.maxHp << "体力  "
                          << skillNames(info) << "\n";
            }
            std::cout << "指令: 编号=查看详情 | 0=返回\n> " << std::flush;
            token = Interaction::readToken();
            if (token == "0" || Interaction::inputClosed()) break;
            int h = Interaction::parseInt(token);
            if (h < 1 || h > static_cast<int>(visible.size())) {
                std::cout << "无效的输入！\n";
                continue;
            }
            Codex::showHeroDetail(all[visible[h - 1]]);
            std::cout << "指令: 0=返回列表\n> " << std::flush;
            Interaction::readToken(); // 任意输入返回（EOF 则 unwind）
            if (Interaction::inputClosed()) return false;
        }
        if (Interaction::inputClosed()) return false;
    }
}

bool menuLoop(const std::string& currentHeroId) {
    while (true) {
        std::cout << "\n================================= 图鉴 =================================\n";
        std::cout << "  [1] 武将图鉴（" << HeroRegistry::all().size() << "名）\n";
        std::cout << "  [2] 卡牌图鉴（" << Codex::buildCardCatalog().size() << "种）\n";
        std::cout << "  [0] 返回游戏\n> " << std::flush;
        std::string token = Interaction::readToken();
        if (token == "0" || Interaction::inputClosed()) return !Interaction::inputClosed();
        int idx = Interaction::parseInt(token);
        if (idx == 1) {
            if (!heroFlow(currentHeroId)) return false;
        } else if (idx == 2) {
            if (!cardFlow()) return false;
        } else {
            std::cout << "无效的输入！\n";
        }
    }
}

} // namespace

bool Codex::isCodexHotkey(const std::string& token) {
    return token == "t" || token == "T" || token == "v" || token == "V";
}

void Codex::showHeroDetail(const HeroInfo& info) {
    HeroPtr hero = info.create();
    std::cout << "\n-------------------------------------------------------------------------\n";
    std::cout << hero->getName() + "·" + hero->getTitle()
              << "  [" << hero->getPack() << "]  " << hero->getCountryString() << "势力  "
              << hero->getGenderString() << "  " << hero->getMaxHp() << " 体力\n";
    std::cout << hero->getSkillSummary();
    std::cout << "-------------------------------------------------------------------------\n";
}

std::vector<Codex::CardEntry> Codex::buildCardCatalog() {
    Deck deck; // 临时牌堆：与对局牌堆无关
    deck.initStandardDeck();
    auto all = deck.peekTopCards(deck.getDrawPileSize());
    std::reverse(all.begin(), all.end()); // 牌堆初始化顺序：基本→锦囊→装备
    std::vector<CardEntry> entries;
    for (const auto& c : all) {
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&](const CardEntry& e) { return e.name == c->getName(); });
        if (it != entries.end()) {
            it->count++;
            continue;
        }
        CardEntry e;
        e.name = c->getName();
        e.type = c->getType();
        e.count = 1;
        e.desc = c->getDescription();
        if (e.desc.empty()) {
            // 基本牌在牌堆中无描述，图鉴补默认规则文本（纯展示）
            if (e.name == "杀") e.desc = "指定攻击范围内一名角色，对方须打出【闪】，否则受到1点伤害";
            else if (e.name == "火杀") e.desc = "【杀】，造成火焰伤害";
            else if (e.name == "雷杀") e.desc = "【杀】，造成雷电伤害";
            else if (e.name == "闪") e.desc = "抵消一张【杀】的效果";
            else if (e.name == "桃") e.desc = "回复1点体力；濒死时可救治";
            else if (e.name == "酒") e.desc = "本回合你使用的下一张【杀】伤害+1";
        }
        entries.push_back(e);
    }
    return entries;
}

void Codex::open(const std::string& currentHeroId) {
    std::cout << "\n=================== 图鉴（游戏暂停中，退出后原样继续）===================\n";
    const HeroInfo* cur = currentHeroId.empty() ? nullptr : HeroRegistry::find(currentHeroId);
    if (cur) {
        // 有当前武将时先展示之（代替旧"查看技能说明"），再决定去向
        showHeroDetail(*cur);
        while (true) {
            std::cout << "指令: 0=返回游戏 | 1=武将图鉴 | 2=卡牌图鉴\n> " << std::flush;
            std::string token = Interaction::readToken();
            if (token == "0" || Interaction::inputClosed()) {
                std::cout << "已退出图鉴，回到游戏。\n";
                return;
            }
            if (token == "1") {
                heroFlow(currentHeroId);
                break;
            }
            if (token == "2") {
                cardFlow();
                break;
            }
            std::cout << "无效的输入！\n";
        }
    }
    menuLoop(currentHeroId);
    std::cout << "已退出图鉴，回到游戏。\n";
}

} // namespace Thks
