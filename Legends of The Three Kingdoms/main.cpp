#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include "Platform.h"
#include "Codex.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "HeroTier.h"
#include "Roles.h"

using namespace Thks;

namespace {

std::string readToken() {
    std::string token;
    if (!(std::cin >> token)) {
        std::cin.clear();
        return "0";
    }
    return token;
}

int parseInt(const std::string& s, int fallback = -1) {
    if (s.empty()) return fallback;
    for (char ch : s) {
        if (!isdigit(static_cast<unsigned char>(ch))) return fallback;
    }
    return std::atoi(s.c_str());
}

// 读 token，热键 t/v 打开图鉴后重读（开局设置用；选将界面另有 v编号命令，单独处理）
std::string readTokenAllowCodex() {
    while (true) {
        std::string token = readToken();
        if (!Codex::isCodexHotkey(token)) return token;
        Codex::open();
    }
}

std::string skillNames(const HeroInfo& info) {
    HeroPtr hero = info.create();
    std::string names;
    for (const auto& s : hero->getSkills()) {
        if (!names.empty()) names += " ";
        names += "【" + s->getName() + "】";
    }
    return names;
}

// 按包分类筛选武将：返回 all 的下标列表（空过滤 = 全部）
// banned 非空时（至尊场）额外剔除弱将，玩家根本选不到被移除的武将。
std::vector<size_t> filterHeroes(const std::vector<std::string>& packFilter,
                                 const std::vector<std::string>& banned = {}) {
    const auto& all = HeroRegistry::all();
    std::vector<size_t> visible;
    for (size_t i = 0; i < all.size(); ++i) {
        if (!banned.empty() &&
            std::find(banned.begin(), banned.end(), all[i].id) != banned.end()) continue;
        if (packFilter.empty() ||
            std::find(packFilter.begin(), packFilter.end(), all[i].pack) != packFilter.end()) {
            visible.push_back(i);
        }
    }
    return visible;
}

void printHeroTable(const std::string& preselectedId, const std::vector<size_t>& visible, const std::string& viewName) {
    const auto& all = HeroRegistry::all();
    std::cout << "\n============================= 武将选择（" << viewName << "）=============================\n";
    for (const auto& pack : HeroRegistry::packs()) {
        bool any = false;
        for (size_t i : visible) {
            if (all[i].pack == pack) { any = true; break; }
        }
        if (!any) continue;
        std::cout << "【" + pack + "】\n";
        for (size_t vi = 0; vi < visible.size(); ++vi) {
            const auto& info = all[visible[vi]];
            if (info.pack != pack) continue;
            std::string mark = (info.id == preselectedId) ? "★" : " ";
            std::cout << "  " << mark << Platform::padRight("(" + std::to_string(vi + 1) + ")", 5) << " "
                      << Platform::padRight(info.name, 10)
                      << Platform::padRight(info.title, 16)
                      << HeroRegistry::countryName(info.country) << "  " << info.maxHp << "体力  " << skillNames(info) << "\n";
        }
    }
    std::cout << "=========================================================================\n";
    if (preselectedId.empty()) {
        std::cout << "当前预选: 无\n";
    } else {
        const HeroInfo* info = HeroRegistry::find(preselectedId);
        std::cout << "当前预选: " << (info ? info->name : preselectedId) << "\n";
    }
    std::cout << "指令: 编号=预选武将 | v编号=查看技能详情 | y=确认 | r=随机 | t=图鉴 | 0=返回扩展包选择\n> ";
}

// 返回选中的武将 id；返回空字符串表示用户取消
// 两级选将：先选扩展包分类（标风火林山 / 界限突破 / 一将成名 / DIY包 / 全部），再在分类内选将
std::string chooseHeroInteractive(const std::vector<std::string>& banned = {}) {
    const auto& all = HeroRegistry::all();
    std::string preselected;
    std::vector<size_t> visible;
    std::string viewName; // 为空表示停留在扩展包选择页
    while (true) {
        if (viewName.empty()) {
            // ---- 第一级：选择扩展包分类（只显示非空分类） ----
            std::vector<PackCategory> cats;
            for (auto& c : HeroRegistry::packCategories()) {
                if (!filterHeroes(c.packs, banned).empty()) cats.push_back(c);
            }
            std::cout << "\n=============================== 选择扩展包 ===============================\n";
            for (size_t i = 0; i < cats.size(); ++i) {
                size_t count = filterHeroes(cats[i].packs, banned).size();
                std::cout << "  [" << (i + 1) << "] " << Platform::padRight(cats[i].name, 16)
                          << "(" << count << " 名武将)\n";
            }
            std::cout << "  [" << (cats.size() + 1) << "] " << Platform::padRight("全部武将", 16)
                      << "(" << filterHeroes({}, banned).size() << " 名武将)\n";
            std::cout << "  [0] 退出\n";
            if (preselected.empty()) {
                std::cout << "当前预选: 无\n";
            } else {
                const HeroInfo* info = HeroRegistry::find(preselected);
                std::cout << "当前预选: " << (info ? info->name : preselected) << "（输入 y 直接确认）\n";
            }
            std::cout << "指令: 编号=进分类 | y=确认预选 | t=图鉴 | 0=退出\n> ";
            std::string token = readToken();
            if (token == "0") return "";
            if (Codex::isCodexHotkey(token)) {
                Codex::open(preselected);
                continue;
            }
            if (token == "y" || token == "Y") {
                if (!preselected.empty()) return preselected;
                std::cout << "请先预选一名武将！\n";
                continue;
            }
            int idx = parseInt(token);
            if (idx >= 1 && idx <= static_cast<int>(cats.size())) {
                const auto& cat = cats[idx - 1];
                auto selected = cat.packs;
                if (!cat.subpacks.empty()) {
                    while (true) {
                        std::cout << "\n--- " << cat.name << " · 选择小包 ---\n";
                        for (size_t j = 0; j < cat.subpacks.size(); ++j)
                            std::cout << "  [" << j + 1 << "] " << cat.subpacks[j] << "（"
                                      << filterHeroes({cat.subpacks[j]}, banned).size() << "名）\n";
                        std::cout << "  [" << cat.subpacks.size() + 1 << "] 全部五包  [0] 返回\n> ";
                        auto sub = readToken();
                        if (Codex::isCodexHotkey(sub)) { Codex::open(preselected); continue; }
                        int n = parseInt(sub);
                        if (n == 0) { selected.clear(); break; }
                        if (n >= 1 && n <= static_cast<int>(cat.subpacks.size())) {
                            selected = {cat.subpacks[n - 1]}; break;
                        }
                        if (n == static_cast<int>(cat.subpacks.size()) + 1) break;
                    }
                }
                if (!selected.empty()) {
                    visible = filterHeroes(selected, banned);
                    viewName = selected.size() == 1 ? selected.front() : cat.name;
                }
            } else if (idx == static_cast<int>(cats.size()) + 1) {
                visible = filterHeroes({}, banned);
                viewName = "全部武将";
            } else {
                std::cout << "无效的输入！\n";
            }
            continue;
        }

        // ---- 第二级：分类内选将 ----
        printHeroTable(preselected, visible, viewName);
        std::string token = readToken();
        if (token == "0") {
            viewName.clear(); // 返回扩展包选择
            continue;
        }
        if (token == "y" || token == "Y") {
            if (preselected.empty()) {
                std::cout << "请先预选一名武将！\n";
                continue;
            }
            return preselected;
        }
        if (token == "r" || token == "R") {
            if (visible.empty()) {
                std::cout << "该分类暂无武将！\n";
                continue;
            }
            return all[visible[static_cast<size_t>(std::rand()) % visible.size()]].id;
        }
        if (Codex::isCodexHotkey(token)) {
            Codex::open(preselected); // t/v 开图鉴（v5 之类带编号的不算，仍走详情）
            continue;
        }
        if (!token.empty() && (token[0] == 'v' || token[0] == 'V' || token[0] == '?')) {
            int idx = parseInt(token.substr(1));
            if (idx >= 1 && idx <= static_cast<int>(visible.size())) {
                Codex::showHeroDetail(all[visible[idx - 1]]);
            } else {
                std::cout << "无效的武将编号！\n";
            }
            continue;
        }
        int idx = parseInt(token);
        if (idx >= 1 && idx <= static_cast<int>(visible.size())) {
            preselected = all[visible[idx - 1]].id;
            Codex::showHeroDetail(all[visible[idx - 1]]);
            std::cout << "已预选 " << all[visible[idx - 1]].name << "，输入 y 确认，或继续选择其他武将。\n";
        } else {
            std::cout << "无效的输入！\n";
        }
    }
}

} // namespace

int main() {
    // 跨平台初始化
    Platform::init();
    // 随机源与引擎保持一致：设置了 THKS_SEED 时，选将界面的 r（随机）也可复现。
    {
        const char* seedEnv = std::getenv("THKS_SEED");
        unsigned menuSeed = (seedEnv && *seedEnv) ? static_cast<unsigned>(std::strtoul(seedEnv, nullptr, 10))
                                                  : static_cast<unsigned>(std::time(nullptr));
        std::srand(menuSeed);
    }

    std::cout << "=========================================" << std::endl;
    std::cout << "          三国杀 (thks) 命令行版         " << std::endl;
    std::cout << "       C++17 跨平台游戏核心 (Kernel)     " << std::endl;
    std::cout << "=========================================\n" << std::endl;

    // ---- 0. 玩法 ----
    std::cout << "请选择玩法:\n";
    std::cout << "  [1] 身份模式（军争）\n";
    std::cout << "  [2] 斗地主（1 地主 vs 2 农民；亮将/抢地主/选将/专属技能）\n";
    std::cout << "请输入选项 (默认 1): ";
    std::string modeToken = readTokenAllowCodex();
    int playMode = parseInt(modeToken, 1);

    // ---- 0.5 场次：普通场 / 至尊场 ----
    // 用户 2026-10-06：「弱将门槛高一点，每个模式各禁 30 个以上；两个模式的禁止池可以不一样；
    // 玩家随机也不用禁止将池（随机候选/换将都排除），但**点将可以用全将**。」
    const size_t heroTotal = HeroRegistry::all().size();
    const std::vector<std::string> banIdentity = HeroTier::bannedInZhizun(HeroTier::Mode::IDENTITY);
    const std::vector<std::string> banDoudizhu = HeroTier::bannedInZhizun(HeroTier::Mode::DOUDIZHU);
    std::cout << "\n请选择场次:\n";
    std::cout << "  [1] 普通场（全部 " << heroTotal << " 名武将）\n";
    std::cout << "  [2] 至尊场（身份场禁止池 " << banIdentity.size() << " 名 / 斗地主禁止池 "
              << banDoudizhu.size() << " 名；随机池按当前模式剔除，点将可用全将）\n";
    std::cout << "      名单与依据见 docs/zhizun_field_rules.md\n";
    std::cout << "请输入选项 (默认 1): ";
    bool zhizun = parseInt(readTokenAllowCodex(), 1) == 2;
    // 点将（自选武将）可用全将 → 选择列表不排除任何武将。
    const std::vector<std::string> banned;
    if (zhizun) {
        const std::vector<std::string>& show =
            (playMode == 2) ? banDoudizhu : banIdentity;
        std::cout << "至尊场已开启（本局：" << (playMode == 2 ? "斗地主禁止池" : "身份场禁止池") << " "
                  << show.size() << " 名，随机池已剔除；点将可用全将） → ";
        for (size_t i = 0; i < show.size(); ++i) {
            const HeroInfo* info = HeroRegistry::find(show[i]);
            std::cout << (info ? info->name : show[i]) << (i + 1 == show.size() ? "\n" : "、");
        }
    }

    // ---- 1. 人数 ----
    if (playMode == 2) {
        std::cout << "\n斗地主固定 3 人局。\n";
        std::cout << "\n请选择武将分配方式:\n";
        std::cout << "  [1] 自选武将（可预览技能，其余角色随机）\n";
        std::cout << "  [2] 全部随机分配\n";
        std::cout << "  [3] 全电脑对战演示（观战）\n";
        std::cout << "请输入选项 (默认 1): ";
        std::string mtoken = readTokenAllowCodex();
        int mmode = parseInt(mtoken, 1);
        std::vector<std::string> mHeroIds;
        int mHumanIndex = (mmode == 3) ? -1 : 0;
        if (mmode != 2 && mmode != 3) {
            std::string chosen = chooseHeroInteractive(banned);
            if (chosen.empty()) { std::cout << "游戏已退出。" << std::endl; return 0; }
            mHeroIds.push_back(chosen);
        }
        GameEngine engine;
        engine.setZhizunField(zhizun);
        if (mmode == 3) engine.setAiDelayMs(0);
        // fullFlow：亮 3 将 → 抢地主（不叫/1/2/3 分）→ 地主追加 2 将 → 选将（地主 5 选 1、农民 3 选 1，
        // 位置可换将 2 次）→ 分发专属技能（飞扬/跋扈/共苦）。自选武将（mmode==1）作为你的候选之一保留。
        engine.initDoudizhuGame(mHumanIndex, mHeroIds, -1, true);
        engine.startGame();
        return 0;
    }
    std::cout << "请选择游戏人数 (" << Roles::MIN_PLAYERS << "-" << Roles::MAX_PLAYERS << "):\n";
    for (int n = Roles::MIN_PLAYERS; n <= Roles::MAX_PLAYERS; ++n) {
        std::cout << "  [" << n << "] " << n << " 人局 (" << Roles::describeComposition(n) << ")\n";
    }
    std::cout << "  [0] 退出\n";
    std::cout << "请输入人数 (默认 4，t=图鉴): ";
    std::string token = readTokenAllowCodex();
    int choice = parseInt(token, 4);
    if (choice == 0) {
        std::cout << "游戏已退出。" << std::endl;
        return 0;
    }
    if (choice < Roles::MIN_PLAYERS || choice > Roles::MAX_PLAYERS) {
        std::cout << "无效的人数，使用默认 4 人局。" << std::endl;
        choice = 4;
    }
    int totalPlayers = choice;

    // ---- 2. 身份：指定或随机（用户 2026-10-05 裁定：玩家也可以指定或随机身份了）----
    int preferredRole = -1; // -1 随机；0 主公 1 忠臣 2 反贼 3 内奸
    {
        Roles::RoleConfig cfg = Roles::configFor(totalPlayers);
        const char* roleNames[4] = {"主公", "忠臣", "反贼", "内奸"};
        const int roleCounts[4] = {cfg.lords, cfg.loyalists, cfg.rebels, cfg.traitors};
        std::cout << "\n请选择你的身份（" << totalPlayers << " 人局构成："
                  << Roles::describeComposition(totalPlayers) << "）:\n";
        std::cout << "  [1] 随机分配（默认）\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "  [" << (i + 2) << "] " << roleNames[i]
                      << (roleCounts[i] > 0 ? "" : "（本人数局没有该身份）") << "\n";
        }
        std::cout << "请输入选项 (默认 1，t=图鉴): ";
        int r = parseInt(readTokenAllowCodex(), 1);
        if (r >= 2 && r <= 5) {
            if (roleCounts[r - 2] > 0) preferredRole = r - 2;
            else std::cout << "本人数局没有该身份，改为随机分配。\n";
        }
    }

    // ---- 3. 武将分配方式 ----
    std::cout << "\n请选择武将分配方式:\n";
    std::cout << "  [1] 自选武将（作为你选将候选之一，可预览技能）\n";
    std::cout << "  [2] 全部随机（进入选将：主公 5 选 1、其余 3 选 1）\n";
    std::cout << "  [3] 全电脑对战演示（观战，AI 也走选将）\n";
    std::cout << "请输入选项 (默认 1，t=图鉴): ";
    token = readTokenAllowCodex();
    int mode = parseInt(token, 1);

    std::vector<std::string> heroIds;
    int humanIndex = 0;
    if (mode == 3) {
        humanIndex = -1;
        preferredRole = -1; // 观战：身份全随机
    } else if (mode == 2) {
        // 全随机：交给引擎
    } else {
        std::string chosen = chooseHeroInteractive(banned);
        if (chosen.empty()) {
            std::cout << "游戏已退出。" << std::endl;
            return 0;
        }
        heroIds.push_back(chosen);
    }

    GameEngine engine;
    if (mode == 3) engine.setAiDelayMs(0);
    engine.setZhizunField(zhizun);
    engine.setIdentityDraft(true);          // 身份场选将：主公先选完并亮出，别人再选针对或辅助
    engine.setPreferredIdentity(preferredRole);
    engine.initGame(totalPlayers, humanIndex, heroIds);
    engine.startGame();

    return 0;
}
