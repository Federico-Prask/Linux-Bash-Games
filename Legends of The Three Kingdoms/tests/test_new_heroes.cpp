#include "test_helpers.h"
#include "SkillsNewHeroes.h"
#include "AI.h"
#include "SkillsStd.h"
#include "HeroRegistry.h"
#include "Interaction.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>

using namespace Thks;

namespace {

void setDrawOrder(GameEngine& engine, const std::vector<CardPtr>& top,
                  const std::vector<CardPtr>& bottom) {
    auto drained = engine.getDeck().drawCards(engine.getDeck().getDrawPileSize());
    engine.getDeck().putOnTop(top);
    engine.getDeck().putOnBottom(bottom);
    (void)drained;
}

bool hasSubtype(const std::vector<CardPtr>& cards, std::initializer_list<CardSubType> wanted) {
    return std::any_of(cards.begin(), cards.end(), [&](const CardPtr& card) {
        return card && std::find(wanted.begin(), wanted.end(), card->getSubType()) != wanted.end();
    });
}

std::vector<CardPtr> cardsNotIn(const std::vector<CardPtr>& after, const std::set<const Card*>& before) {
    std::vector<CardPtr> gained;
    for (const auto& card : after)
        if (card && !before.count(card.get())) gained.push_back(card);
    return gained;
}

std::set<const Card*> cardPointers(const std::vector<CardPtr>& cards) {
    std::set<const Card*> result;
    for (const auto& card : cards) if (card) result.insert(card.get());
    return result;
}

struct NewHeroInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit NewHeroInput(const std::string& text)
        : script(text), saved(std::cin.rdbuf(script.rdbuf())) { Interaction::resetInputState(); }
    ~NewHeroInput() { std::cin.rdbuf(saved); Interaction::resetInputState(); }
};

} // namespace

TEST("new_heroes/official_registry_entries_and_skill_names") {
    const HeroInfo* luotong = HeroRegistry::find("luotong");
    CHECK(luotong != nullptr);
    if (luotong) {
        CHECK_EQ(luotong->name, std::string("骆统"));
        CHECK_EQ(luotong->pack, std::string("始计篇·智"));
        CHECK(luotong->country == Country::WU);
        CHECK_EQ(luotong->maxHp, 4);
        CHECK(luotong->gender == Gender::MALE);
        CHECK_EQ(luotong->create()->getSkills().size(), size_t(1));
        CHECK(luotong->create()->findSkill("勤政")->hasTag(SkillTag::LOCK));
    }

    const HeroInfo* xuyou = HeroRegistry::find("xuyou");
    CHECK(xuyou != nullptr);
    if (xuyou) {
        CHECK_EQ(xuyou->name, std::string("许攸"));
        CHECK_EQ(xuyou->pack, std::string("神话再临·阴"));
        CHECK(xuyou->country == Country::QUN);
        CHECK_EQ(xuyou->maxHp, 3);
        CHECK(xuyou->gender == Gender::MALE);
        auto hero = xuyou->create();
        CHECK(hero->findSkill("成略") != nullptr);
        CHECK(hero->findSkill("恃才") != nullptr);
        CHECK(hero->findSkill("寸目") != nullptr);
        CHECK(hero->findSkill("寸目")->hasTag(SkillTag::LOCK));
    }
}

TEST("new_heroes/luotong_qinzheng_rewards_on_use_or_response_multiples") {
    GameEngine engine;
    engine.setSeed(31);
    captureLog(engine);
    engine.initGame(2, -1, {"luotong", "guanyu"});
    auto self = engine.getPlayers()[0];
    clearHand(*self);
    auto skill = std::dynamic_pointer_cast<LuoTongQinZhengSkill>(self->getHero()->findSkill("勤政"));
    CHECK(skill != nullptr);
    if (!skill) return;

    auto eventCard = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    for (int count = 1; count <= 15; ++count) {
        const auto beforeCards = self->getHandCards();
        const auto beforePointers = cardPointers(beforeCards);
        skill->onCardPlayed(engine, *self, eventCard);
        const auto gained = cardsNotIn(self->getHandCards(), beforePointers);
        const int expectedRewards = (count % 3 == 0 ? 1 : 0) +
                                    (count % 5 == 0 ? 1 : 0) +
                                    (count % 8 == 0 ? 1 : 0);
        CHECK_EQ(static_cast<int>(gained.size()), expectedRewards);
        CHECK_EQ(skill->getCardsUsedOrPlayed(), count);
        if (count % 3 == 0)
            CHECK(hasSubtype(gained, {CardSubType::SHA, CardSubType::SHAN}));
        if (count % 5 == 0)
            CHECK(hasSubtype(gained, {CardSubType::TAO, CardSubType::JIU}));
        if (count % 8 == 0)
            CHECK(hasSubtype(gained, {CardSubType::WU_ZHONG_SHENG_YOU, CardSubType::JUE_DOU}));
    }

    // 响应时打出也计入；第18张牌再次触发3张牌档位。
    const auto beforeResponse = self->getHandCards();
    const auto beforePointers = cardPointers(beforeResponse);
    skill->onCardResponded(engine, *self, eventCard);
    skill->onCardResponded(engine, *self, eventCard);
    skill->onCardResponded(engine, *self, eventCard);
    const auto responseRewards = cardsNotIn(self->getHandCards(), beforePointers);
    CHECK_EQ(responseRewards.size(), size_t(2)); // 第16张达到8张牌档位，第18张再次达到3张牌档位
    CHECK(hasSubtype(responseRewards, {CardSubType::SHA, CardSubType::SHAN}));
    CHECK(hasSubtype(responseRewards, {CardSubType::WU_ZHONG_SHENG_YOU, CardSubType::JUE_DOU}));
    CHECK_EQ(skill->getCardsUsedOrPlayed(), 18);
}

TEST("new_heroes/xuyou_cunmu_draws_from_bottom_and_leaves_top_for_others") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, -1, {"xuyou", "guanyu"});
    auto xuyou = engine.getPlayers()[0];
    auto ordinary = engine.getPlayers()[1];
    auto top = makeCard("杀", Suit::SPADE, 1, CardType::BASIC, CardSubType::SHA);
    auto bottom1 = makeCard("桃", Suit::HEART, 2, CardType::BASIC, CardSubType::TAO);
    auto bottom2 = makeCard("闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN);
    setDrawOrder(engine, {top}, {bottom1, bottom2});

    engine.drawCards(xuyou, 1, "寸目测试");
    CHECK(xuyou->hasHandCard(bottom1));
    CHECK_EQ(engine.getDeck().peekTop(), top);

    engine.drawCards(ordinary, 1, "普通摸牌测试");
    CHECK(ordinary->hasHandCard(top));

    engine.drawCards(xuyou, 1, "寸目第二次测试");
    CHECK(xuyou->hasHandCard(bottom2));
}

TEST("new_heroes/xuyou_chenglue_toggles_only_after_a_valid_paid_use") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, -1, {"xuyou", "guanyu"});
    auto self = engine.getPlayers()[0];
    auto skill = std::dynamic_pointer_cast<XuYouChengLueSkill>(self->getHero()->findSkill("成略"));
    CHECK(skill != nullptr);
    if (!skill) return;

    clearHand(*self);
    CHECK(!skill->canActivate(engine, *self));
    skill->activate(engine, *self);
    CHECK_EQ(skill->getUsesThisTurn(), 0);
    CHECK_EQ(self->getMark("成略阴"), 0);

    engine.setPhase(TurnPhase::PLAY);
    CHECK(!skill->canActivate(engine, *self)); // 摸一张后仍不足弃置两张
    auto firstCost = makeCard("杀", Suit::HEART, 7, CardType::BASIC, CardSubType::SHA);
    auto yinDraw1 = makeCard("杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA);
    auto yinDraw2 = makeCard("闪", Suit::DIAMOND, 9, CardType::BASIC, CardSubType::SHAN);
    auto yinDraw3 = makeCard("桃", Suit::SPADE, 10, CardType::BASIC, CardSubType::TAO);
    auto top = makeCard("决斗", Suit::CLUB, 1, CardType::TRICK, CardSubType::JUE_DOU);
    setDrawOrder(engine, {top}, {yinDraw1, yinDraw2, yinDraw3});

    self->addHandCard(firstCost);
    CHECK(skill->canActivate(engine, *self));
    skill->activate(engine, *self);
    CHECK_EQ(skill->getUsesThisTurn(), 1);
    CHECK_EQ(self->getMark("成略阴"), 1); // 阳成功后转阴
    CHECK_EQ(self->getHandCardCount(), 0);
    CHECK(self->hasChenglueUnlimitedSuit(Suit::HEART));
    CHECK(self->hasChenglueUnlimitedSuit(Suit::CLUB));
    CHECK(!skill->canActivate(engine, *self)); // 出牌阶段限一次

    skill->resetTurnState(); // 模拟下一回合次数重置；转换状态保留
    self->addHandCard(makeCard("酒", Suit::DIAMOND, 11, CardType::BASIC, CardSubType::JIU));
    CHECK(skill->canActivate(engine, *self));
    skill->activate(engine, *self);
    CHECK_EQ(skill->getUsesThisTurn(), 1);
    CHECK_EQ(self->getMark("成略阴"), 0); // 阴成功后转回阳
    CHECK_EQ(self->getHandCardCount(), 2); // 摸两张后弃一张
    CHECK(!skill->canActivate(engine, *self));
}

TEST("new_heroes/xuyou_shicai_only_triggers_once_per_card_type_and_returns_card_to_top") {
    GameEngine engine;
    auto log = captureLog(engine);
    engine.initGame(2, -1, {"xuyou", "zhangfei"});
    auto self = engine.getPlayers()[0];
    auto target = engine.getPlayers()[1];
    clearHand(*self);
    clearHand(*target);
    engine.setPhase(TurnPhase::PLAY);

    auto sha1 = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto basic2 = makeCard("酒", Suit::CLUB, 8, CardType::BASIC, CardSubType::JIU);
    auto top = makeCard("杀", Suit::HEART, 9, CardType::BASIC, CardSubType::SHA);
    auto bottom1 = makeCard("桃", Suit::HEART, 2, CardType::BASIC, CardSubType::TAO);
    auto bottom2 = makeCard("闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN);
    setDrawOrder(engine, {top}, {bottom1, bottom2});
    self->addHandCards({sha1, basic2});

    CHECK(engine.useCard(self, sha1, {target}));
    CHECK(self->hasHandCard(bottom1)); // 恃才摸牌也受寸目影响，从牌堆底摸
    CHECK_EQ(engine.getDeck().peekTop(), sha1);

    CHECK(engine.useCard(self, basic2, {}));
    CHECK_EQ(engine.getDeck().peekTop(), sha1); // 本回合第二张基本牌不再触发恃才
    CHECK_EQ(self->getHandCardCount(), 1);      // 第二张杀消耗后不额外摸牌

    const std::string messages = log->str();
    const std::string marker = "【恃才】将 ";
    const auto first = messages.find(marker);
    CHECK(first != std::string::npos);
    CHECK(messages.find(marker, first == std::string::npos ? 0 : first + marker.size()) == std::string::npos);
}

TEST("new_heroes/xuyou_shicai_can_reclaim_a_card_from_special_or_outside_pile") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, -1, {"xuyou", "guanyu"});
    auto self = engine.getPlayers()[0];

    auto special = makeCard("桃", Suit::HEART, 6, CardType::BASIC, CardSubType::TAO);
    self->addToPile("测试牌堆", special);
    CHECK(engine.canPutUsedCardOnTop(special));
    CHECK(engine.putUsedCardOnTop(self, special));
    CHECK_EQ(self->getPileCount("测试牌堆"), 0);
    CHECK_EQ(engine.getDeck().peekTop(), special);

    auto outside = makeCard("无中生有", Suit::HEART, 7, CardType::TRICK,
                            CardSubType::WU_ZHONG_SHENG_YOU);
    engine.exileZone().push_back(outside);
    CHECK(engine.canPutUsedCardOnTop(outside));
    CHECK(engine.putUsedCardOnTop(self, outside));
    CHECK(std::find(engine.exileZone().begin(), engine.exileZone().end(), outside) ==
          engine.exileZone().end());
    CHECK_EQ(engine.getDeck().peekTop(), outside);
}

TEST("new_heroes/mantian_exchanges_cards_with_one_or_two_distinct_other_targets") {
    GameEngine engine;
    engine.setSeed(73);
    captureLog(engine);
    engine.initGame(3, -1, {"zhangfei", "guanyu", "zhaoyun"});
    auto players=engine.getPlayers();
    for (const auto& player : players) clearHand(*player);

    auto mantian=makeCard("瞒天过海",Suit::SPADE,5,CardType::TRICK,CardSubType::MANTIAN_GUOHAI);
    auto gift1=makeCard("酒",Suit::CLUB,3,CardType::BASIC,CardSubType::JIU);
    auto gift2=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    auto targetCard1=makeCard("桃",Suit::HEART,3,CardType::BASIC,CardSubType::TAO);
    auto targetCard2=makeCard("无中生有",Suit::HEART,7,CardType::TRICK,CardSubType::WU_ZHONG_SHENG_YOU);
    players[0]->addHandCards({mantian,gift1,gift2});
    players[1]->addHandCard(targetCard1);
    players[2]->addHandCard(targetCard2);

    CHECK(engine.useCard(players[0],mantian,{players[1],players[2]}));
    CHECK(!players[0]->hasHandCard(mantian));
    CHECK(players[0]->hasHandCard(targetCard1));
    CHECK(players[0]->hasHandCard(targetCard2));
    CHECK(players[1]->hasHandCard(gift1));
    CHECK(players[2]->hasHandCard(gift2));
    CHECK(!players[1]->hasHandCard(targetCard1));
    CHECK(!players[2]->hasHandCard(targetCard2));
}

TEST("new_heroes/mantian_rejects_empty_self_duplicate_and_excess_targets_without_consuming") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(4,-1,{"zhangfei","guanyu","zhaoyun","machao"});
    auto players=engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto mantian=makeCard("瞒天过海",Suit::SPADE,5,CardType::TRICK,CardSubType::MANTIAN_GUOHAI);
    players[0]->addHandCard(mantian);
    for (int i=1;i<4;++i)
        players[i]->addHandCard(makeCard("杀",Suit::HEART,i+1,CardType::BASIC,CardSubType::SHA));

    CHECK(!engine.useCard(players[0],mantian,{}));
    CHECK(players[0]->hasHandCard(mantian));
    CHECK(!engine.useCard(players[0],mantian,{players[0]}));
    CHECK(players[0]->hasHandCard(mantian));
    CHECK(!engine.useCard(players[0],mantian,{players[1],players[1]}));
    CHECK(players[0]->hasHandCard(mantian));
    CHECK(!engine.useCard(players[0],mantian,{players[1],players[2],players[3]}));
    CHECK(players[0]->hasHandCard(mantian));

    // 不依赖董昭【妙略】：实体瞒天过海本身始终不计入任何持有者的手牌上限。
    const int baseLimit=players[0]->getHandLimit();
    auto second=Card::makeVirtual("瞒天过海",CardType::TRICK,CardSubType::MANTIAN_GUOHAI,{},"测试");
    players[0]->addHandCard(second);
    CHECK_EQ(engine.calculateHandLimit(players[0]),baseLimit+2); // 一实体牌 + 一虚拟牌
}

TEST("new_heroes/mantian_human_can_choose_one_target_and_choose_the_returned_hand_card") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3,0,{"zhangfei","guanyu","zhaoyun"});
    auto players=engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto mantian=makeCard("瞒天过海",Suit::DIAMOND,5,CardType::TRICK,CardSubType::MANTIAN_GUOHAI);
    auto gift=makeCard("酒",Suit::CLUB,3,CardType::BASIC,CardSubType::JIU);
    auto untouched=makeCard("闪",Suit::SPADE,6,CardType::BASIC,CardSubType::SHAN);
    auto targetCard=makeCard("无中生有",Suit::HEART,7,CardType::TRICK,CardSubType::WU_ZHONG_SHENG_YOU);
    players[0]->addHandCards({mantian,gift});
    players[1]->addHandCard(untouched);
    players[2]->addHandCard(targetCard);

    // 直接指定一名其他角色，选手牌区，并把自己的第一张手牌交还。
    NewHeroInput input("1 1");
    CHECK(engine.useCard(players[0],mantian,{players[2]}));
    CHECK(players[0]->hasHandCard(targetCard));
    CHECK(players[1]->hasHandCard(untouched));
    CHECK(players[2]->hasHandCard(gift));
    CHECK(!players[2]->hasHandCard(targetCard));
}

TEST("targeting/standard_tuxi_may_choose_self_when_text_says_role_not_other_role") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3,0,{"zhangliao","guanyu","machao"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    auto ownCard=makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN);
    auto otherCard=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    players[0]->addHandCard(ownCard);
    players[1]->addHandCard(otherCard);
    int draw=2;
    {
        NewHeroInput input("y 1 0"); // 选择自己后结束；本人的牌原样返回。
        players[0]->getHero()->findSkill("突袭")->onDrawCards(engine,*players[0],draw);
    }
    CHECK_EQ(draw,0);
    CHECK(players[0]->hasHandCard(ownCard));
    CHECK(players[1]->hasHandCard(otherCard));
}

TEST("new_heroes/mantian_ai_selects_only_legal_enemy_targets") {
    GameEngine engine;
    engine.setSeed(91);
    captureLog(engine);
    engine.initGame(3,-1,{"zhangfei","guanyu","zhaoyun"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    auto mantian=makeCard("瞒天过海",Suit::SPADE,5,CardType::TRICK,CardSubType::MANTIAN_GUOHAI);
    players[0]->addHandCard(mantian);
    for(int i=1;i<3;++i)
        players[i]->addHandCard(makeCard("杀",Suit::HEART,i+2,CardType::BASIC,CardSubType::SHA));

    auto decision=AIController::makePlayDecision(engine,*players[0]);
    CHECK(decision.cardToPlay==mantian);
    CHECK(!decision.targets.empty());
    CHECK(decision.targets.size()<=2);
    std::set<int> targetIds;
    for(const auto& target:decision.targets) {
        CHECK(target!=players[0]);
        CHECK(!target->getAllCards().empty());
        CHECK(!AIController::isFriend(*players[0],*target));
        CHECK(targetIds.insert(target->getId()).second);
        CHECK(engine.canBeTargeted(target,mantian,players[0]));
    }
}

TEST("new_heroes/mantian_nullification_cancels_only_that_targets_two_part_exchange") {
    GameEngine engine;
    engine.setSeed(92);
    captureLog(engine);
    engine.initGame(3,-1,{"zhangfei","guanyu","zhaoyun"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    auto mantian=makeCard("瞒天过海",Suit::HEART,5,CardType::TRICK,CardSubType::MANTIAN_GUOHAI);
    auto gift=makeCard("酒",Suit::CLUB,3,CardType::BASIC,CardSubType::JIU);
    auto protectedCard=makeCard("桃",Suit::HEART,7,CardType::BASIC,CardSubType::TAO);
    auto exposedCard=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    auto wuxie=makeCard("无懈可击",Suit::SPADE,11,CardType::TRICK,CardSubType::WU_XIE_KE_JI);
    players[0]->addHandCards({mantian,gift});
    players[1]->addHandCards({protectedCard,wuxie});
    players[2]->addHandCard(exposedCard);

    CHECK(engine.useCard(players[0],mantian,{players[1],players[2]}));
    CHECK(players[1]->hasHandCard(protectedCard));
    CHECK(!players[1]->hasHandCard(wuxie));
    CHECK(players[0]->hasHandCard(exposedCard));
    CHECK(players[2]->hasHandCard(gift));
    CHECK(!players[2]->hasHandCard(exposedCard));
}
