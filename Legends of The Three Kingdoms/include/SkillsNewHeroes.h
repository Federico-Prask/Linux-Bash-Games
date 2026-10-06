#ifndef THKS_SKILLS_NEW_HEROES_H
#define THKS_SKILLS_NEW_HEROES_H

#include "Hero.h"
#include <set>

namespace Thks {

// 骆统（移动版·始计篇智包）：跨回合累计使用/打出的牌数，达到相应倍数时随机检索。
class LuoTongQinZhengSkill : public StateSkill {
    int cardsUsedOrPlayed = 0;
    void obtainRandomMatching(GameEngine& engine, Player& self,
                              const std::vector<CardSubType>& subtypes, const std::string& reason);
public:
    LuoTongQinZhengSkill();
    void onCardPlayed(GameEngine& engine, Player& self, CardPtr card) override;
    int getCardsUsedOrPlayed() const { return cardsUsedOrPlayed; }
};

// 许攸（移动版·神话再临·阴）：转换技；本回合被弃置花色的牌无距离/次数限制。
class XuYouChengLueSkill : public ActiveSkill {
    static const std::string& yinMarkName();
public:
    XuYouChengLueSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
};

// 恃才：按牌型记录本回合首次使用，并在该牌结算后决定是否控顶摸牌。
class XuYouShiCaiSkill : public TriggerSkill {
    std::set<const Card*> firstUseCardsPendingResolution;
public:
    XuYouShiCaiSkill();
    void onCardUsed(GameEngine& engine, Player& self, CardPtr card,
                    bool firstUseOfTypeThisTurn) override;
    void onCardResolved(GameEngine& engine, Player& self, CardPtr card) override;
    void onTurnBoundary(GameEngine& engine, Player& self, Player& currentPlayer,
                        bool starting) override;
};

// 寸目：所有摸牌均从牌堆底进行。
class XuYouCunMuSkill : public StateSkill {
public:
    XuYouCunMuSkill();
    bool drawsFromBottom() const override { return true; }
};

} // namespace Thks

#endif // THKS_SKILLS_NEW_H
