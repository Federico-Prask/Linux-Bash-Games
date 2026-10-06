#include "test_helpers.h"
#include "HeroRegistry.h"
#include "Interaction.h"
#include "SkillsExtra.h"
#include "SkillsMou.h"
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace Thks;

namespace {
void setMark(Player& player, const std::string& name, int value) {
    const int old = player.getMark(name);
    player.addMark(name, value - old);
}
}

TEST("targets/ordinary_sha_rejects_out_of_range_target_without_marking_use") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(4, -1, {"zhangfei"});
    const auto players = engine.getPlayers();
    for (const auto& player : players) {
        player->setHero(blankHero());
        clearHand(*player);
    }
    CHECK(engine.calculateDistance(*players[0], *players[2]) > players[0]->getAttackRange());
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "目标规则测试");

    CHECK(!engine.canUseShaOn(*players[0], *players[2], sha));
    CHECK(!engine.useCard(players[0], sha, {players[2]}));
    CHECK_EQ(players[0]->getMark("本回合用过杀"), 0);
}

TEST("targets/jiedao_victims_use_actual_sha_target_legality") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(4, -1, {"", "", "zhugeliang"});
    const auto players = engine.getPlayers();
    for (size_t i = 0; i < players.size(); ++i) {
        if (i != 2) players[i]->setHero(blankHero());
        clearHand(*players[i]);
    }
    auto weapon = makeCard("青龙偃月刀", Suit::SPADE, 5, CardType::EQUIPMENT,
                           CardSubType::WEAPON, ShaElement::NORMAL, 3);
    players[1]->equip(weapon);

    // 空城不能成为【杀】的目标；因此不是持武器角色可选择的借刀目标。
    const auto victims = engine.getJieDaoVictims(*players[1]);
    CHECK(std::find(victims.begin(), victims.end(), players[2]) == victims.end());
    const auto holders = engine.getJieDaoWeaponHolders(*players[0]);
    CHECK(std::find(holders.begin(), holders.end(), players[1]) != holders.end());

    auto jiedao = Card::makeVirtual("借刀杀人", CardType::TRICK,
                                    CardSubType::JIE_DAO_SHA_REN, {}, "目标规则测试");
    CHECK(!engine.useCard(players[0], jiedao, {players[1], players[2]}));
}

TEST("targets/mou_tiaoxin_preflight_keeps_longdan_candidate_check_side_effect_free") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, 0, {"mou_jiangwei", "mou_zhaoyun"});
    const auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto shan = makeCard("闪", Suit::HEART, 7, CardType::BASIC, CardSubType::SHAN);
    players[1]->addHandCard(shan);
    setMark(*players[0], "蓄力", 1);
    engine.setCurrentPlayerForTesting(players[0]);
    engine.setPhase(TurnPhase::PLAY);

    auto skill = players[0]->getHero()->findSkill("谋-挑衅");
    auto active = skill ? std::dynamic_pointer_cast<MouTiaoXinSkill>(skill) : nullptr;
    CHECK(active != nullptr);
    if (!active) return;
    CHECK(active->canActivate(engine, *players[0]));
    CHECK_EQ(players[1]->getMark("龙胆已用"), 0);
    CHECK_EQ(players[1]->getHandCardCount(), 1);
}

TEST("targets/mou_tiaoxin_no_distance_sha_option_is_not_limited_by_owner_range") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(4, 0, {"mou_jiangwei"});
    const auto players = engine.getPlayers();
    for (size_t i = 1; i < players.size(); ++i) players[i]->setHero(blankHero());
    for (const auto& player : players) clearHand(*player);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    players[2]->addHandCard(sha);
    setMark(*players[0], "蓄力", 1);
    engine.setCurrentPlayerForTesting(players[0]);
    engine.setPhase(TurnPhase::PLAY);

    CHECK(engine.calculateDistance(*players[2], *players[0]) > players[2]->getAttackRange());
    CHECK(!engine.canUseShaOn(*players[2], *players[0], sha));
    auto skill = players[0]->getHero()->findSkill("谋-挑衅");
    auto active = skill ? std::dynamic_pointer_cast<MouTiaoXinSkill>(skill) : nullptr;
    CHECK(active != nullptr);
    if (!active) return;
    CHECK(active->canActivate(engine, *players[0]));
}

TEST("targets/final_trick_use_rejects_qianxun_immunity") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, 0, {"", "luxun"});
    const auto players = engine.getPlayers();
    players[0]->setHero(blankHero());
    for (const auto& player : players) clearHand(*player);
    auto lebu = Card::makeVirtual("乐不思蜀", CardType::TRICK,
                                  CardSubType::LE_BU_SI_SHU, {}, "目标规则测试");

    CHECK(!engine.useCard(players[0], lebu, {players[1]}));
}

TEST("targets/shi_jixi_accepts_multiple_far_legal_targets_but_normal_shunshou_does_not") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(4, -1, {""});
    const auto players = engine.getPlayers();
    for (const auto& player : players) {
        player->setHero(blankHero());
        clearHand(*player);
    }
    players[2]->addHandCard(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO));
    players[3]->addHandCard(makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN));
    auto regular = Card::makeVirtual("顺手牵羊", CardType::TRICK,
                                     CardSubType::SHUN_SHOU_QIAN_YANG, {}, "");
    auto jixi = Card::makeVirtual("顺手牵羊", CardType::TRICK,
                                   CardSubType::SHUN_SHOU_QIAN_YANG, {}, "势-急袭");

    CHECK(engine.calculateDistance(*players[0], *players[2]) > 1);
    CHECK(!engine.canUseShunShouOn(*players[0], *players[2], regular));
    CHECK(engine.canUseShunShouOn(*players[0], *players[2], jixi));
    CHECK(!engine.useCard(players[0], regular, {players[2], players[3]}));
    CHECK(engine.useCard(players[0], jixi, {players[2], players[3]}));
    CHECK_EQ(engine.getCardTargets(jixi).size(), size_t(2));
}

TEST("targets/candidate_checks_do_not_record_targets_for_ji_xi") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, 0, {"shi_dengai"});
    const auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto first = makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO);
    auto second = makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN);
    players[1]->addHandCard(first);
    players[1]->addHandCard(second);
    auto guohe = Card::makeVirtual("过河拆桥", CardType::TRICK,
                                   CardSubType::GUO_HE_CHAI_QIAO, {}, "候选检查");

    CHECK(engine.canBeTargeted(players[1], guohe, players[0]));
    auto skill = players[0]->getHero()->findSkill("势-急袭");
    auto jixi = skill ? std::dynamic_pointer_cast<ShiJiXiSkill>(skill) : nullptr;
    CHECK(jixi != nullptr);
    if (!jixi) return;
    {
        std::istringstream script("y\n1\n1\n0\n");
        auto* saved = std::cin.rdbuf(script.rdbuf());
        Interaction::resetInputState();
        jixi->onTurnEnd(engine, *players[0], *players[1]);
        std::cin.rdbuf(saved);
        Interaction::resetInputState();
    }
    CHECK_EQ(players[1]->getHandCardCount(), 2);
}
