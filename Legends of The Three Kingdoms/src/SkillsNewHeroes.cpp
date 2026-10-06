#include "SkillsNewHeroes.h"
#include "GameEngine.h"
#include "Player.h"
#include "AI.h"
#include <algorithm>
#include <string>
#include <vector>

namespace Thks {

namespace {

std::string who(const Player& player) {
    return player.getName() + "（" + (player.getHero() ? player.getHero()->getName() : "无武将") + "）";
}

} // namespace

// ------------------------------ 骆统：勤政 ------------------------------
LuoTongQinZhengSkill::LuoTongQinZhengSkill()
    : StateSkill("勤政",
                 "锁定技，你每使用或打出三张牌时，你随机获得一张【杀】或【闪】；每使用或打出五张牌时，你随机获得一张【桃】或【酒】；每使用或打出八张牌时，你随机获得一张【无中生有】或【决斗】。",
                 SkillTag::LOCK) {}

void LuoTongQinZhengSkill::obtainRandomMatching(GameEngine& engine, Player& self,
                                                const std::vector<CardSubType>& subtypes,
                                                const std::string& reason) {
    auto card = engine.getDeck().drawRandomMatching(
        [&subtypes](const CardPtr& candidate) {
            return candidate && std::find(subtypes.begin(), subtypes.end(), candidate->getSubType()) != subtypes.end();
        }, engine.getRng());
    if (!card) {
        engine.logMessage("  【勤政】" + reason + "：牌堆中没有符合条件的牌。");
        return;
    }
    auto owner = engine.getPlayerById(self.getId());
    if (!owner) return;
    owner->addHandCard(card);
    engine.logMessage("  [" + who(self) + "] 发动【勤政】，随机获得 " + card->getFormattedName() + "。");
    engine.notifyCardsObtained(owner, 1);
}

void LuoTongQinZhengSkill::onCardPlayed(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || !self.isAlive()) return;
    ++cardsUsedOrPlayed;
    if (cardsUsedOrPlayed % 3 == 0)
        obtainRandomMatching(engine, self, {CardSubType::SHA, CardSubType::SHAN}, "第3张牌奖励");
    if (cardsUsedOrPlayed % 5 == 0)
        obtainRandomMatching(engine, self, {CardSubType::TAO, CardSubType::JIU}, "第5张牌奖励");
    if (cardsUsedOrPlayed % 8 == 0)
        obtainRandomMatching(engine, self, {CardSubType::WU_ZHONG_SHENG_YOU, CardSubType::JUE_DOU}, "第8张牌奖励");
}

// ------------------------------ 许攸：成略 ------------------------------
const std::string& XuYouChengLueSkill::yinMarkName() {
    static const std::string mark = "成略阴";
    return mark;
}

XuYouChengLueSkill::XuYouChengLueSkill()
    : ActiveSkill("成略",
                 "转换技，出牌阶段限一次，阳：你可以摸一张牌，然后弃置两张手牌。\xC2\xA0阴：你可以摸两张牌，然后弃置一张手牌。\xC2\xA0若如此做，直到本回合结束，你使用与弃置牌相同花色的牌无距离和次数限制。",
                 1, SkillTag::SWITCH) {}

bool XuYouChengLueSkill::canActivate(GameEngine& engine, Player& self) {
    if (!self.isAlive() || !hasUsesLeft() || engine.getCurrentPhase() != TurnPhase::PLAY ||
        !engine.isPlayerTurn(self)) return false;
    const bool yin = self.getMark(yinMarkName()) > 0;
    const int drawCount = yin ? 2 : 1;
    const int discardCount = yin ? 1 : 2;
    const int availableToDraw = engine.getDeck().getDrawPileSize() + engine.getDeck().getDiscardPileSize();
    const int possibleDraws = std::min(drawCount, availableToDraw);
    // 预先验证摸牌后能支付费用，避免因手牌不足而空发。
    return self.getHandCardCount() + possibleDraws >= discardCount;
}

bool XuYouChengLueSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

void XuYouChengLueSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    auto me = engine.getPlayerById(self.getId());
    if (!me) return;

    const bool yin = self.getMark(yinMarkName()) > 0;
    const int drawCount = yin ? 2 : 1;
    const int discardCount = yin ? 1 : 2;
    engine.logMessage("  [" + who(self) + "] 发动【成略】（" + (yin ? "阴" : "阳") + "）。");
    engine.drawCards(me, drawCount, "成略");

    // 摸牌触发的其他效果可能改变存活/手牌状态；再次验证费用后才继续结算。
    if (!self.isAlive() || self.getHandCardCount() < discardCount) return;
    auto costs = engine.chooseCards(me, self.getHandCards(), discardCount,
                                    "【成略】弃置手牌");
    if (static_cast<int>(costs.size()) != discardCount) return;
    for (const auto& card : costs) if (!self.hasHandCard(card)) return;

    std::vector<Suit> suits;
    suits.reserve(costs.size());
    for (const auto& card : costs) suits.push_back(engine.effectiveSuit(self, card));

    // 成功选择并完成发动后才消耗次数、翻转转换状态并赋予花色效果。
    markUsed();
    for (const auto& card : costs) engine.discardCardOf(me, card, "成略");
    if (!self.isAlive()) return;
    for (Suit suit : suits) self.addChenglueUnlimitedSuit(suit);
    self.addMark(yinMarkName(), yin ? -1 : 1);

    engine.logMessage("  【成略】本回合与所弃置牌花色相同的牌无距离和次数限制。");
}

// ------------------------------ 许攸：恃才 ------------------------------
XuYouShiCaiSkill::XuYouShiCaiSkill()
    : TriggerSkill("恃才",
                   "当你使用一张牌结算结束后，若此牌与你本回合使用的牌类型均不同（包括装备牌），你可以将此牌置于牌堆顶，然后摸一张牌。") {}

void XuYouShiCaiSkill::onCardUsed(GameEngine&, Player&, CardPtr card, bool firstUseOfTypeThisTurn) {
    if (card && firstUseOfTypeThisTurn) firstUseCardsPendingResolution.insert(card.get());
}

void XuYouShiCaiSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || firstUseCardsPendingResolution.erase(card.get()) == 0 || !self.isAlive()) return;
    if (!engine.canPutUsedCardOnTop(card)) return;

    auto me = engine.getPlayerById(self.getId());
    if (!me) return;
    const bool aiWants = card->getType() != CardType::EQUIPMENT;
    if (!engine.askConfirm(me, "【恃才】此牌为你本回合首次使用的此类型牌，是否将其置于牌堆顶并摸一张牌？", aiWants)) return;
    if (engine.putUsedCardOnTop(me, card)) engine.drawCards(me, 1, "恃才");
}

void XuYouShiCaiSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool starting) {
    if (starting) firstUseCardsPendingResolution.clear();
}

// ------------------------------ 许攸：寸目 ------------------------------
XuYouCunMuSkill::XuYouCunMuSkill()
    : StateSkill("寸目", "锁定技，当你摸牌时，改为从牌堆底摸牌。", SkillTag::LOCK) {}

} // namespace Thks
