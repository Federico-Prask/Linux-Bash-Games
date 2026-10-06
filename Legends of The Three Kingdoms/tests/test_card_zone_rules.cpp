#include "test_helpers.h"
#include "HeroRegistry.h"
#include "Interaction.h"
#include "SkillsMou.h"
#include "SkillsStd.h"
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace Thks;

namespace {
struct ZoneRuleInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit ZoneRuleInput(const std::string& text)
        : script(text), saved(std::cin.rdbuf(script.rdbuf())) {
        Interaction::resetInputState();
    }
    ~ZoneRuleInput() {
        std::cin.rdbuf(saved);
        Interaction::resetInputState();
    }
};

bool inJudgeZone(const Player& player, const CardPtr& card) {
    const auto& judge = player.getJudgeZone();
    return std::find(judge.begin(), judge.end(), card) != judge.end();
}

bool inDiscardPile(const GameEngine& engine, const CardPtr& card) {
    const auto& discard = engine.getDeck().getDiscardPile();
    return std::find(discard.begin(), discard.end(), card) != discard.end();
}
}

TEST("zones/plain_discard_candidates_are_hand_and_equipment_but_not_judgement") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, -1, {"sunquan", "zhangfei"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);

    auto equipment = makeCard("白银狮子", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::ARMOR);
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    players[0]->equip(equipment);
    players[0]->addJudgeCard(lightning);

    const auto ordinaryDiscardable = players[0]->getHandAndEquipmentCards();
    const auto allZones = players[0]->getAllCards();
    CHECK_EQ(ordinaryDiscardable.size(), size_t(1));
    CHECK(ordinaryDiscardable.front() == equipment);
    CHECK_EQ(allZones.size(), size_t(2));
    CHECK(std::find(allZones.begin(), allZones.end(), lightning) != allZones.end());
}

TEST("zones/standard_zhiheng_discards_equipment_but_not_lightning") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, 0, {"sunquan", "zhangfei"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto equipment = makeCard("白银狮子", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::ARMOR);
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    players[0]->equip(equipment);
    players[0]->addJudgeCard(lightning);
    engine.setCurrentPlayerForTesting(players[0]);
    engine.setPhase(TurnPhase::PLAY);

    auto skill = players[0]->getHero()->findSkill("制衡");
    auto active = skill ? std::dynamic_pointer_cast<ActiveSkill>(skill) : nullptr;
    CHECK(active != nullptr);
    if (!active) return;
    CHECK(active->canActivate(engine, *players[0]));
    {
        ZoneRuleInput input("1 0"); // 选装备；不再选择第二张。
        active->activate(engine, *players[0]);
    }
    CHECK(!players[0]->hasEquipment(equipment));
    CHECK(inJudgeZone(*players[0], lightning));
    CHECK(inDiscardPile(engine, equipment));
    CHECK_EQ(players[0]->getHandCardCount(), 1); // 弃一摸一。
}

TEST("zones/mou_zhiheng_can_use_equipment_but_never_treat_judgement_as_discard") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, 0, {"mou_sunquan", "zhangfei"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    players[0]->addJudgeCard(lightning);
    engine.setCurrentPlayerForTesting(players[0]);
    engine.setPhase(TurnPhase::PLAY);

    auto skill = players[0]->getHero()->findSkill("谋-制衡");
    auto active = skill ? std::dynamic_pointer_cast<ActiveSkill>(skill) : nullptr;
    CHECK(active != nullptr);
    if (!active) return;
    CHECK(!active->canActivate(engine, *players[0]));

    auto equipment = makeCard("防御马", Suit::DIAMOND, 2, CardType::EQUIPMENT,
                              CardSubType::DEFENSIVE_HORSE);
    players[0]->equip(equipment);
    CHECK(active->canActivate(engine, *players[0]));
    {
        ZoneRuleInput input("1");
        active->activate(engine, *players[0]);
    }
    CHECK(!players[0]->hasEquipment(equipment));
    CHECK(inJudgeZone(*players[0], lightning));
    CHECK(inDiscardPile(engine, equipment));
    CHECK_EQ(players[0]->getMark("制衡已用"), 1);
    CHECK_EQ(players[0]->getHandCardCount(), 1);
}

TEST("zones/explicit_area_discard_can_choose_judgement_card") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, 0, {"sunquan", "zhangfei"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    players[1]->addJudgeCard(lightning);
    auto virtualGuohe = Card::makeVirtual("过河拆桥", CardType::TRICK,
                                          CardSubType::GUO_HE_CHAI_QIAO, {}, "区域测试");

    // 通用“选择一张牌”默认不列判定区；明确按牌的“区域”选择则允许判定区。
    CHECK(engine.chooseCardFromPlayer(players[0], players[1], "普通弃牌") == nullptr);
    {
        ZoneRuleInput input("1");
        CHECK(engine.useCard(players[0], virtualGuohe, {players[1]}));
    }
    CHECK(!inJudgeZone(*players[1], lightning));
    CHECK(inDiscardPile(engine, lightning));
}
