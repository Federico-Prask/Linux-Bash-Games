#ifndef THKS_CARD_TRACKER_H
#define THKS_CARD_TRACKER_H

#include <string>
#include <vector>
#include "Card.h"

namespace Thks {

class GameEngine;

// 牌堆记牌器：在对局暂停时展示当前牌堆中各实体牌的剩余张数。
// 记牌器不读取角色手牌或牌堆顺序以外的隐私信息；退出后原提示继续。
class CardTracker {
public:
    struct Entry {
        std::string name;
        std::string formattedName;
        CardType type = CardType::BASIC;
        Suit suit = Suit::NONE;
        int rank = 0;
        int count = 0;
    };

    static bool isHotkey(const std::string& token);
    static std::vector<Entry> buildEntries(const Deck& deck);
    static void open(const GameEngine& engine, const std::string& currentHeroId = "");
};

} // namespace Thks

#endif // THKS_CARD_TRACKER_H
