#ifndef THKS_CODEX_H
#define THKS_CODEX_H

// =====================================================================
//  图鉴（模态百科）：武将图鉴 + 卡牌图鉴
//
//  - 可在任何人类输入点打开（出牌阶段 / 选将 / 任意询问），热键 t / v；
//  - 纯静态数据展示：只读 HeroRegistry 与临时牌堆，不触碰任何对局状态；
//  - 同步子循环：进入后游戏暂停，退出后原样继续（状态保留）。
// =====================================================================

#include <string>
#include <vector>
#include "Card.h"
#include "HeroRegistry.h"

namespace Thks {

class Codex {
public:
    // 是否为图鉴热键（t/T/v/V；注意 v5 之类带编号的不算）
    static bool isCodexHotkey(const std::string& token);
    // 打开图鉴。currentHeroId 非空且有效时先展示该武将（列表中★标记），否则直接进菜单。
    static void open(const std::string& currentHeroId = "");
    // 武将详情（选将界面复用）
    static void showHeroDetail(const HeroInfo& info);

    struct CardEntry {
        std::string name;
        CardType type = CardType::BASIC;
        int count = 0;
        std::string desc;
    };
    // 卡牌目录：临时标准牌堆去重（保持基本→锦囊→装备分组顺序），纯函数可测试
    static std::vector<CardEntry> buildCardCatalog();
};

} // namespace Thks

#endif // THKS_CODEX_H
