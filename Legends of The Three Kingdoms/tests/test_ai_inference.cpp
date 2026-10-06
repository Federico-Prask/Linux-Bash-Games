// AI 队友分辨（用户 2026-10-05：“注意 ai 需分辨队友等”）：
// AI 不再直接读未公开的真实身份，而是用**公开信息 + 行为证据**推断阵营。
//   公开信息：主公（选将后亮出/储君继位）、斗地主明牌身份、【择途】明置的身份牌、野心家、
//             阵亡者（死亡时揭示）、储君标记、以及开局公示的身份构成（如 5 人局 1主1忠2反1内）。
//   行为证据：谁对谁用【杀】/有害锦囊、造成伤害（敌对）；谁用【桃】救谁、用【无懈可击】替谁挡锦囊（友好）。
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "AI.h"
#include "Roles.h"
#include <memory>
#include <vector>

using namespace Thks;

namespace {
PlayerPtr byRole(GameEngine& e, Identity role) {
    for (const auto& p : e.getPlayers()) if (p->isAlive() && p->getIdentity() == role) return p;
    return nullptr;
}
} // namespace

TEST("ai/estimate_sides_uses_public_composition_prior") {
    // 5 人局（1主1忠2反1内）：内奸视角 → 主忠方 2、反贼 2
    {
        GameEngine e;
        e.setSeed(200);
        captureLog(e);
        e.initGame(5, -1, {});
        PlayerPtr traitor = byRole(e, Identity::NEI_JIAN);
        int loyal = 0, rebels = 0;
        e.aiEstimateSides(*traitor, loyal, rebels);
        CHECK_EQ(loyal, 2);
        CHECK_EQ(rebels, 2);
    }
    // 8 人局（1主2忠4反1内）：内奸视角 → 主忠方 3、反贼 4
    {
        GameEngine e;
        e.setSeed(201);
        captureLog(e);
        e.initGame(8, -1, {});
        PlayerPtr traitor = byRole(e, Identity::NEI_JIAN);
        int loyal = 0, rebels = 0;
        e.aiEstimateSides(*traitor, loyal, rebels);
        CHECK_EQ(loyal, 3);
        CHECK_EQ(rebels, 4);
        // 主公视角：自己占一个主忠名额 → 其余主忠 2、反贼 4
        PlayerPtr lord = byRole(e, Identity::ZHU_GONG);
        loyal = rebels = 0;
        e.aiEstimateSides(*lord, loyal, rebels);
        CHECK_EQ(loyal, 2);
        CHECK_EQ(rebels, 4);
    }
}

TEST("ai/hostile_act_against_lord_marks_rebel_side") {
    GameEngine e;
    e.setSeed(202);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "zhangjiao", "guanyu", "zhaoyun", "machao"});
    PlayerPtr lord = byRole(e, Identity::ZHU_GONG);
    std::vector<PlayerPtr> rebels;
    for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::FAN_ZEI) rebels.push_back(p);
    CHECK(lord != nullptr);
    CHECK_EQ(rebels.size(), static_cast<size_t>(2));
    if (!lord || rebels.size() < 2) return;
    // 反贼 A 攻击主公（用牌 +1、造成伤害 +2 → 由 useCard/applyDamage 记录）
    e.recordRelation(rebels[0]->getId(), lord->getId(), 3);
    CHECK(e.aiSideEstimate(*rebels[0]) < 0);
    CHECK(!e.aiIsFriend(*lord, *rebels[0]));            // 主公不再把打自己的人当队友
    CHECK(!AIController::isFriend(e, *lord, *rebels[0]));
    // 反贼 B 也打过主公 → 从 B 的视角，A 是队友（靠证据，不是靠读身份）
    e.recordRelation(rebels[1]->getId(), lord->getId(), 3);
    CHECK(e.aiSideEstimate(*rebels[1]) < 0);
    CHECK(AIController::isFriend(e, *rebels[1], *rebels[0]));
    // 而 A 与 B 之间没有任何证据时，A 也不会认 B 是队友
    GameEngine e2;
    e2.setSeed(202);
    captureLog(e2);
    e2.initGame(5, -1, {"caocao", "zhangjiao", "guanyu", "zhaoyun", "machao"});
    PlayerPtr lord2 = byRole(e2, Identity::ZHU_GONG);
    std::vector<PlayerPtr> rebels2;
    for (const auto& p : e2.getPlayers()) if (p->getIdentity() == Identity::FAN_ZEI) rebels2.push_back(p);
    e2.recordRelation(rebels2[0]->getId(), lord2->getId(), 3); // 只有 A 动过手
    CHECK(!AIController::isFriend(e2, *rebels2[0], *rebels2[1]));
}

TEST("ai/helping_lord_marks_loyal_side") {
    GameEngine e;
    e.setSeed(203);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "huatuo", "guanyu", "zhaoyun", "machao"});
    PlayerPtr lord = byRole(e, Identity::ZHU_GONG);
    PlayerPtr loyal = byRole(e, Identity::ZHONG_CHEN);
    PlayerPtr rebel = byRole(e, Identity::FAN_ZEI);
    CHECK(lord != nullptr && loyal != nullptr && rebel != nullptr);
    if (!lord || !loyal || !rebel) return;
    // 忠臣用【桃】救主公（-3）→ 强烈的主忠方证据
    e.recordRelation(loyal->getId(), lord->getId(), -3);
    CHECK(e.aiSideEstimate(*loyal) > 0);
    CHECK(e.aiIsFriend(*loyal, *lord));   // 忠臣视角：主公是队友（公开身份）
    CHECK(e.aiIsFriend(*lord, *loyal));   // 主公视角：救过我的人是队友
    // 反贼打过主公 → 不是队友
    e.recordRelation(rebel->getId(), lord->getId(), +2);
    CHECK(!e.aiIsFriend(*lord, *rebel));
    CHECK(!e.aiIsFriend(*loyal, *rebel));
}

TEST("ai/card_usage_is_recorded_as_evidence") {
    GameEngine e;
    e.setSeed(204);
    captureLog(e);
    e.initGame(3, -1, {"caocao", "huatuo", "guanyu"});
    auto ps = e.getPlayers();
    for (auto& p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    // 华佗对曹操使用【桃】→ 记录友好证据
    ps[0]->setHp(1);
    auto tao = std::make_shared<Card>(970, "桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO,
                                      ShaElement::NORMAL, 1, "");
    ps[1]->addHandCard(tao);
    CHECK(e.useCard(ps[1], tao, {ps[0]}));
    CHECK(e.relationOf(ps[1]->getId(), ps[0]->getId()) < 0);
    CHECK(e.aiSideEstimate(*ps[1]) > 0 || ps[0]->getIdentity() == Identity::ZHU_GONG);
    // 再用【杀】攻击 → 敌对证据累加
    e.setCurrentPlayerForTesting(ps[2]);
    auto sha = std::make_shared<Card>(971, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA,
                                      ShaElement::NORMAL, 1, "");
    ps[2]->addHandCard(sha);
    int before = e.relationOf(ps[2]->getId(), ps[0]->getId());
    CHECK(e.useCard(ps[2], sha, {ps[0]}));
    CHECK(e.relationOf(ps[2]->getId(), ps[0]->getId()) > before);
}

TEST("ai/dead_players_identities_become_public_evidence") {
    GameEngine e;
    e.setSeed(205);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "huatuo", "guanyu", "zhaoyun", "machao"});
    auto ps = e.getPlayers();
    PlayerPtr rebel = byRole(e, Identity::FAN_ZEI);
    CHECK(rebel != nullptr);
    if (!rebel) return;
    PlayerPtr other = byRole(e, Identity::ZHONG_CHEN);
    // 反贼阵亡后身份公开 → 成为“反贼方锚点”：谁打过他就偏向主忠方
    e.killPlayer(rebel);
    CHECK(!rebel->isAlive());
    e.recordRelation(other->getId(), rebel->getId(), +2);
    CHECK(e.aiSideEstimate(*other) > 0);
}

TEST("ai/no_ground_truth_leak_for_hidden_roles") {
    // 关键回归：AI 不再“开挂”读真实身份。
    // 5 人局里两名反贼互相是队友，但**没有任何行为证据时**，一方不会把另一方当队友。
    GameEngine e;
    e.setSeed(206);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "huatuo", "guanyu", "zhaoyun", "machao"});
    std::vector<PlayerPtr> rebels;
    for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::FAN_ZEI) rebels.push_back(p);
    CHECK_EQ(rebels.size(), static_cast<size_t>(2));
    if (rebels.size() < 2) return;
    CHECK_EQ(e.relationOf(rebels[0]->getId(), rebels[1]->getId()), 0);
    CHECK(!AIController::isFriend(e, *rebels[0], *rebels[1])); // 无证据 → 不认队友
    // 但真实身份版（仅测试/对照用）仍然是队友
    CHECK(AIController::isFriend(*rebels[0], *rebels[1]));
    // 一旦其中一人打了主公，另一人（反贼视角）就能从证据认出他
    PlayerPtr lord = byRole(e, Identity::ZHU_GONG);
    e.recordRelation(rebels[0]->getId(), lord->getId(), +3);
    CHECK(e.aiSideEstimate(*rebels[0]) < 0);
    CHECK(AIController::isFriend(e, *rebels[1], *rebels[0]));
}

TEST("ai/traitor_and_ambitor_have_no_teammates") {
    GameEngine e;
    e.setSeed(207);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "huatuo", "guanyu", "zhaoyun", "machao"});
    PlayerPtr traitor = byRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = byRole(e, Identity::ZHU_GONG);
    CHECK(traitor != nullptr && lord != nullptr);
    if (!traitor || !lord) return;
    e.recordRelation(traitor->getId(), lord->getId(), -3); // 就算救过主公
    CHECK(!AIController::isFriend(e, *traitor, *lord));     // 内奸也没有队友
    traitor->setIdentity(Identity::YE_XIN_JIA);
    CHECK(!AIController::isFriend(e, *traitor, *lord));     // 野心家同样没有队友
}

TEST("ai/doudizhu_roles_remain_public") {
    GameEngine e;
    e.setSeed(208);
    captureLog(e);
    e.initDoudizhuGame(-1, {"caocao", "huatuo", "guanyu"}, 0, false);
    auto ps = e.getPlayers();
    CHECK(!AIController::isFriend(e, *ps[0], *ps[1]));  // 地主 vs 农民
    CHECK(AIController::isFriend(e, *ps[1], *ps[2]));   // 农民互为队友（明牌）
    CHECK(AIController::isFriend(e, *ps[0], *ps[0]));
}

TEST("ai/consistency_rule_prevents_unwinnable_stalemate") {
    // 对局未结束就说明胜负未分：若按推断“所有存活者都是队友”，则证据最弱的一名视为非队友，
    // 否则谁都不出手，对局会打到回合上限（实测 seed=934 的 7 人至尊场曾因此僵死）。
    GameEngine e;
    e.setSeed(209);
    captureLog(e);
    e.initGame(2, -1, {"caocao", "huatuo"});
    auto ps = e.getPlayers();
    ps[0]->setIdentity(Identity::ZHU_GONG);
    ps[1]->setIdentity(Identity::ZHONG_CHEN);
    ps[1]->addMark("储君", 1);
    e.recordRelation(ps[1]->getId(), ps[0]->getId(), -3);
    CHECK(e.aiSideEstimate(*ps[1]) > 0);
    CHECK(!e.aiIsFriend(*ps[0], *ps[1])); // 2 人局：唯一的对手必须可打
    CHECK(!e.isGameOver());
}
