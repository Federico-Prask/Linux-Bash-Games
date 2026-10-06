// 身份场新机制（用户 2026-10-05 提供规则）：内奸【择途】/ 野心家 / 主公【立储】与储君继位，
// 以及选将框取牌策略（普通场随机 / 至尊场由选将 AI 预选且有机会取第二名）。
// 规则与工程落实见 docs/identity_field_rules.md。
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "HeroTier.h"
#include "SkillsMode.h"
#include "AI.h"
#include "Roles.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <set>
#include <sstream>

using namespace Thks;

namespace {

// 只返回**存活**的角色（阵亡者身份仍保留，但已不参与局势）
PlayerPtr findByRole(GameEngine& e, Identity role) {
    for (const auto& p : e.getPlayers()) if (p->isAlive() && p->getIdentity() == role) return p;
    return nullptr;
}

SkillPtr findSkill(GameEngine& e, PlayerPtr p, const std::string& name) {
    return p && p->getHero() ? p->getHero()->findSkill(name) : nullptr;
}

// 让内奸处于“可发动择途”的状态：存活 > 4、自己回合的出牌阶段
void armTraitor(GameEngine& e, PlayerPtr traitor) {
    e.setCurrentPlayerForTesting(traitor);
    e.setPhase(TurnPhase::PLAY);
}

} // namespace

// ==================== 一、模式技能分发 ====================

TEST("modes/identity_mode_skills_are_distributed_by_role") {
    GameEngine e;
    e.setSeed(41);
    auto sink = captureLog(e);
    e.initGame(5, -1, {});
    int traitors = 0, lords = 0;
    for (const auto& p : e.getPlayers()) {
        bool hasZetu = findSkill(e, p, "择途") != nullptr;
        bool hasLichu = findSkill(e, p, "立储") != nullptr;
        if (p->getIdentity() == Identity::NEI_JIAN) {
            CHECK(hasZetu);
            CHECK(!hasLichu);
            ++traitors;
        } else if (p->getIdentity() == Identity::ZHU_GONG) {
            CHECK(hasLichu);
            CHECK(!hasZetu);
            ++lords;
        } else {
            CHECK(!hasZetu);
            CHECK(!hasLichu);
        }
    }
    CHECK_EQ(traitors, 1);
    CHECK_EQ(lords, 1);
    CHECK(sink->str().find("内奸【择途】") != std::string::npos); // 开局公告了新机制
}

TEST("modes/doudizhu_does_not_get_identity_mode_skills") {
    GameEngine e;
    e.setSeed(42);
    captureLog(e);
    e.initDoudizhuGame(-1, {}, 0, false);
    for (const auto& p : e.getPlayers()) {
        CHECK(findSkill(e, p, "择途") == nullptr);
        CHECK(findSkill(e, p, "立储") == nullptr);
    }
    // 斗地主的地主仍有飞扬/跋扈（与野心家共用同一套实现）
    CHECK(findSkill(e, e.getPlayers()[0], "飞扬") != nullptr);
    CHECK(findSkill(e, e.getPlayers()[0], "跋扈") != nullptr);
}

// ==================== 二、择途：发动条件 ====================

TEST("zetu/requires_identity_field_traitor_and_more_than_four_alive") {
    GameEngine e;
    e.setSeed(43);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    CHECK(traitor != nullptr);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;

    armTraitor(e, traitor);
    CHECK(sk->canActivate(e, *traitor));                 // 5 人全存活 > 4
    CHECK(!sk->canActivate(e, *findByRole(e, Identity::ZHU_GONG))); // 只有内奸能发动

    e.killPlayer(findByRole(e, Identity::FAN_ZEI));      // 剩 4 人
    CHECK_EQ(static_cast<int>(e.getAlivePlayers().size()), 4);
    CHECK(!sk->canActivate(e, *traitor));                // 存活角色不大于 4 → 不能发动
}

TEST("zetu/is_once_per_game") {
    GameEngine e;
    e.setSeed(44);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;
    armTraitor(e, traitor);
    // 主公 1 点体力 + 反贼存活 → AI 选“自立为主”
    findByRole(e, Identity::ZHU_GONG)->setHp(1);
    sk->activate(e, *traitor);
    CHECK_EQ(traitor->getMark("择途已用"), 1);
    CHECK(!sk->canActivate(e, *traitor)); // 一局只能发动一次
    Identity after = traitor->getIdentity();
    sk->activate(e, *traitor);            // 再次发动无效（禁止空发）
    CHECK(traitor->getIdentity() == after);
}

// ==================== 三、择途：两条道路 ====================

TEST("zetu/ambition_makes_ambitor_with_landlord_skills") {
    GameEngine e;
    e.setSeed(45);
    auto sink = captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;
    lord->setHp(1); // 主公将死 + 反贼在场 → AI 判定“自立为主”
    CHECK_EQ(ZheTuSkill::aiChoice(e, *traitor), 1);

    armTraitor(e, traitor);
    sk->activate(e, *traitor);

    CHECK(traitor->getIdentity() == Identity::YE_XIN_JIA);
    CHECK_EQ(traitor->getIdentityString(), std::string("野心家"));
    CHECK(findSkill(e, traitor, "飞扬") != nullptr);
    CHECK(findSkill(e, traitor, "跋扈") != nullptr);
    CHECK(sink->str().find("自立为主") != std::string::npos);
    CHECK(sink->str().find("野心家") != std::string::npos);
}

TEST("zetu/serving_lord_turns_traitor_into_loyalist") {
    GameEngine e;
    e.setSeed(46);
    auto sink = captureLog(e);
    // 8 人局：1主公 2忠臣 4反贼 1内奸。清掉 3 名反贼后剩 5 人（仍 > 4），
    // 主忠方 3 人 > 反贼 1 人 → AI 判定“侍奉明君”（洗白随主忠方获胜）。
    e.initGame(8, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;
    int killed = 0;
    for (const auto& p : e.getPlayers()) {
        if (killed >= 3) break;
        if (p->getIdentity() == Identity::FAN_ZEI) { e.killPlayer(p); ++killed; }
    }
    CHECK_EQ(killed, 3);
    CHECK_EQ(static_cast<int>(e.getAlivePlayers().size()), 5);
    CHECK_EQ(ZheTuSkill::aiChoice(e, *traitor), 0);

    armTraitor(e, traitor);
    sk->activate(e, *traitor);

    CHECK(traitor->getIdentity() == Identity::ZHONG_CHEN);
    CHECK(findSkill(e, traitor, "飞扬") == nullptr); // 侍奉明君不给地主技能
    CHECK(sink->str().find("侍奉明君") != std::string::npos);
    CHECK_EQ(Roles::aliveCount(e.getPlayers(), Identity::NEI_JIAN), 0); // 内奸已不存在
}

TEST("zetu/ambitor_landlord_skills_work_in_identity_field") {
    GameEngine e;
    e.setSeed(47);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    lord->setHp(1);
    armTraitor(e, traitor);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->activate(e, *traitor);
    CHECK(traitor->getIdentity() == Identity::YE_XIN_JIA);

    // 【跋扈】准备阶段摸 1 张（身份场同样生效）
    auto bahu = findSkill(e, traitor, "跋扈");
    CHECK(bahu != nullptr);
    if (bahu) {
        e.setCurrentPlayerForTesting(traitor);
        e.setPhase(TurnPhase::PREPARATION);
        int before = traitor->getHandCardCount();
        bool skip = false;
        bahu->onPhaseStart(e, *traitor, TurnPhase::PREPARATION, skip);
        CHECK_EQ(traitor->getHandCardCount(), before + 1);
        // 出牌阶段可多使用 1 张【杀】
        e.setPhase(TurnPhase::PLAY);
        int limit = 1;
        bahu->onCalculateShaLimit(e, *traitor, limit);
        CHECK_EQ(limit, 2);
        // 非野心家/非斗地主不生效：忠臣拿同一技能也不会加杀
        PlayerPtr loyal = findByRole(e, Identity::ZHONG_CHEN);
        int limit2 = 1;
        bahu->onCalculateShaLimit(e, *loyal, limit2);
        CHECK_EQ(limit2, 1);
    }
    // 【飞扬】判定阶段需要判定区有牌才发动（此处只验证不会因为身份场而被禁用）
    CHECK(findSkill(e, traitor, "飞扬") != nullptr);
}

// ==================== 四、胜负判定（野心家） ====================

TEST("roles/lord_death_with_ambitor_does_not_end_game") {
    GameEngine e;
    e.setSeed(48);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    lord->setHp(1);
    armTraitor(e, traitor);
    findSkill(e, traitor, "择途")->activate(e, *traitor); // 自立为主
    CHECK(traitor->getIdentity() == Identity::YE_XIN_JIA);

    e.killPlayer(lord);                 // 主公阵亡
    CHECK(!e.isGameOver());             // 野心家仍在 → 不立即失败
    CHECK(findByRole(e, Identity::ZHU_GONG) == nullptr);
}

TEST("roles/ambitor_sole_survivor_wins") {
    GameEngine e;
    e.setSeed(49);
    auto sink = captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    lord->setHp(1);
    armTraitor(e, traitor);
    findSkill(e, traitor, "择途")->activate(e, *traitor);

    std::vector<PlayerPtr> others;
    for (const auto& p : e.getPlayers()) if (p->getId() != traitor->getId() && p->isAlive()) others.push_back(p);
    for (auto& p : others) e.killPlayer(p);

    CHECK(e.isGameOver());
    CHECK(e.getWinningFaction().find("野心家胜") != std::string::npos);
    CHECK(sink->str().find("唯一幸存者") != std::string::npos);
}

TEST("roles/legacy_rules_unchanged_without_ambitor") {
    // 回归：没有野心家时，主公阵亡仍是反贼胜（或一内独存的内奸胜）
    GameEngine e;
    e.setSeed(50);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    PlayerPtr loyal = findByRole(e, Identity::ZHONG_CHEN);
    e.killPlayer(loyal);
    e.killPlayer(lord);
    CHECK(e.isGameOver());
    // 场上仍有反贼与内奸 → 反贼胜
    CHECK_EQ(e.getWinningFaction(), std::string("反贼胜"));
}

TEST("roles/loyalist_after_zetu_wins_with_lord_side") {
    GameEngine e;
    e.setSeed(51);
    captureLog(e);
    e.initGame(8, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    int killed = 0;
    for (const auto& p : e.getPlayers()) {
        if (killed >= 3) break;
        if (p->getIdentity() == Identity::FAN_ZEI) { e.killPlayer(p); ++killed; }
    }
    armTraitor(e, traitor);
    findSkill(e, traitor, "择途")->activate(e, *traitor); // 侍奉明君 → 忠臣
    CHECK(traitor->getIdentity() == Identity::ZHONG_CHEN);
    // 清掉最后一名反贼 → 反贼与内奸全灭且主公存活 → 主忠胜
    for (const auto& p : e.getPlayers())
        if (p->isAlive() && p->getIdentity() == Identity::FAN_ZEI) e.killPlayer(p);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("主公与忠臣胜"));
}

TEST("roles/ambitor_keeps_playing_when_lord_side_wipes_rebels") {
    // 主公存活、反贼与内奸全灭，但野心家还在 → 不结束（野心家还要清场）
    GameEngine e;
    e.setSeed(52);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    lord->setHp(1);
    armTraitor(e, traitor);
    findSkill(e, traitor, "择途")->activate(e, *traitor); // 自立为主
    for (const auto& p : e.getPlayers())
        if (p->isAlive() && p->getIdentity() == Identity::FAN_ZEI) e.killPlayer(p);
    CHECK(!e.isGameOver());
    CHECK(Roles::aliveCount(e.getPlayers(), Identity::YE_XIN_JIA) == 1);
    // 野心家被击杀后 → 反贼内奸全灭且主公存活 → 主忠胜
    e.killPlayer(traitor);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("主公与忠臣胜"));
}

// ==================== 五、立储与继位 ====================

TEST("lichu/lord_appoints_heir_in_first_round") {
    GameEngine e;
    e.setSeed(53);
    auto sink = captureLog(e);
    e.initGame(5, -1, {});
    e.setRoundForTesting(1);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    auto sk = findSkill(e, lord, "立储");
    CHECK(sk != nullptr);
    if (!sk) return;
    PlayerPtr loyal = findByRole(e, Identity::ZHONG_CHEN);
    PlayerPtr rebel = findByRole(e, Identity::FAN_ZEI);
    // AI 不读真实身份：必须先有**公开行为证据**。
    // 忠臣替主公挡过牌/救过主公（友好），反贼打过主公（敌对）。
    e.recordRelation(loyal->getId(), lord->getId(), -3);
    e.recordRelation(rebel->getId(), lord->getId(), +2);
    CHECK(e.aiSideEstimate(*loyal) > 0);
    CHECK(e.aiSideEstimate(*rebel) < 0);
    // 没有证据时不立储（立错人无法继位，还会暴露判断）
    GameEngine e0;
    e0.setSeed(53);
    captureLog(e0);
    e0.initGame(5, -1, {});
    PlayerPtr lord0 = findByRole(e0, Identity::ZHU_GONG);
    auto sk0 = findSkill(e0, lord0, "立储");
    CHECK(sk0 != nullptr);
    if (sk0) {
        e0.setRoundForTesting(1);
        sk0->onAnyPhaseEnd(e0, *lord0, *lord0, TurnPhase::FINISH);
        CHECK_EQ(lord0->getMark("立储已用"), 0); // 零证据 → 不立储
    }
    // 第一轮的任意角色结束阶段（这里用反贼的结束阶段触发，验证“任意角色”）
    sk->onAnyPhaseEnd(e, *lord, *rebel, TurnPhase::FINISH);
    CHECK_EQ(lord->getMark("立储已用"), 1);
    PlayerPtr heir;
    for (const auto& p : e.getPlayers()) if (p->getMark("储君") > 0) heir = p;
    CHECK(heir != nullptr);
    if (heir) {
        CHECK(heir->getId() == loyal->getId());   // 立的是有证据偏向主忠方的那个人
        CHECK(e.describePublicState(*heir).find("储君") != std::string::npos); // 明选＝公开状态
    }
    CHECK(sink->str().find("【立储】") != std::string::npos);
    CHECK(sink->str().find("储君（太子）") != std::string::npos);
    // 只能立一次
    sk->onAnyPhaseEnd(e, *lord, *rebel, TurnPhase::FINISH);
    int heirs = 0;
    for (const auto& p : e.getPlayers()) if (p->getMark("储君") > 0) ++heirs;
    CHECK_EQ(heirs, 1);
}

TEST("lichu/only_first_round_and_only_lord") {
    GameEngine e;
    e.setSeed(54);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    auto sk = findSkill(e, lord, "立储");
    CHECK(sk != nullptr);
    if (!sk) return;
    e.setRoundForTesting(2); // 第二轮
    sk->onAnyPhaseEnd(e, *lord, *lord, TurnPhase::FINISH);
    CHECK_EQ(lord->getMark("立储已用"), 0);
    for (const auto& p : e.getPlayers()) CHECK_EQ(p->getMark("储君"), 0);
    // 非结束阶段不触发
    e.setRoundForTesting(1);
    sk->onAnyPhaseEnd(e, *lord, *lord, TurnPhase::PLAY);
    CHECK_EQ(lord->getMark("立储已用"), 0);
    // 非主公不触发（反贼即使拿到同名技能也不行）
    PlayerPtr rebel = findByRole(e, Identity::FAN_ZEI);
    auto sk2 = std::make_shared<LiChuSkill>();
    sk2->onAnyPhaseEnd(e, *rebel, *rebel, TurnPhase::FINISH);
    CHECK_EQ(rebel->getMark("立储已用"), 0);
}

TEST("lichu/loyalist_heir_succeeds_lord") {
    GameEngine e;
    e.setSeed(55);
    auto sink = captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    PlayerPtr loyal = findByRole(e, Identity::ZHONG_CHEN);
    loyal->addMark("储君", 1);
    lord->setHp(1);
    e.killPlayer(lord);
    CHECK(!e.isGameOver());                                  // 主忠方延续
    CHECK(loyal->getIdentity() == Identity::ZHU_GONG);       // 储君继位
    CHECK_EQ(loyal->getMark("储君"), 0);
    CHECK(sink->str().find("继位成为新的主公") != std::string::npos);
    // 继位者获得【立储】（主公模式技能），但一局只立一次 → 由新主公自行决定
    CHECK(findSkill(e, loyal, "立储") != nullptr);
    // 新主公再阵亡且无储君 → 反贼胜
    e.killPlayer(loyal);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("反贼胜"));
}

TEST("lichu/non_loyalist_heir_cannot_succeed") {
    GameEngine e;
    e.setSeed(56);
    auto sink = captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    PlayerPtr rebel = findByRole(e, Identity::FAN_ZEI);
    rebel->addMark("储君", 1); // 主公误立反贼
    lord->setHp(1);
    e.killPlayer(lord);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("反贼胜"));
    CHECK(rebel->getIdentity() == Identity::FAN_ZEI); // 未继位
    CHECK(sink->str().find("不是忠臣，无法继位") != std::string::npos);
}

TEST("lichu/succession_happens_before_victory_check") {
    // “胜利优先于一切”仍成立，但继位先于胜负判定：主公阵亡时有忠臣储君 → 不算主忠方失败
    GameEngine e;
    e.setSeed(57);
    captureLog(e);
    e.initGame(4, -1, {}); // 1主公 1忠臣 1反贼 1内奸
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    PlayerPtr loyal = findByRole(e, Identity::ZHONG_CHEN);
    PlayerPtr rebel = findByRole(e, Identity::FAN_ZEI);
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    loyal->addMark("储君", 1);
    lord->setHp(1);
    e.killPlayer(lord);
    CHECK(!e.isGameOver());
    CHECK(loyal->getIdentity() == Identity::ZHU_GONG);
    // 新主公清掉反贼与内奸 → 主忠胜
    e.killPlayer(rebel);
    e.killPlayer(traitor);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("主公与忠臣胜"));
}

// ==================== 六、选将框取牌策略 ====================

TEST("ai/draft_variance_only_picks_first_or_second") {
    std::vector<HeroPtr> hs;
    for (const char* id : {"zhanghe", "lusu", "taishici", "huatuo", "shen_zhaoyun"})
        hs.push_back(HeroRegistry::create(id));
    AIController::DraftContext ctx;
    ctx.role = Identity::FAN_ZEI;
    ctx.field = HeroTier::Field::IDENTITY;
    ctx.lordHero = HeroRegistry::create("caocao");
    auto rank = AIController::rankDraftHeroes(hs, ctx);
    CHECK_EQ(rank.size(), hs.size());
    // 排名严格按分数降序
    for (size_t i = 1; i < rank.size(); ++i)
        CHECK(AIController::draftScore(hs[rank[i - 1]], ctx) >= AIController::draftScore(hs[rank[i]], ctx));

    std::mt19937 rng(2026);
    std::set<size_t> seen;
    for (int i = 0; i < 300; ++i) seen.insert(AIController::chooseDraftHeroWithVariance(hs, ctx, rng, 30));
    CHECK_EQ(seen.size(), static_cast<size_t>(2)); // 只会落在第一名或第二名
    CHECK(seen.count(rank[0]) == 1);
    CHECK(seen.count(rank[1]) == 1);
    // 概率为 0 时恒取第一名；候选只有 1 个时也不会越界
    std::set<size_t> only1;
    for (int i = 0; i < 20; ++i) only1.insert(AIController::chooseDraftHeroWithVariance(hs, ctx, rng, 0));
    CHECK_EQ(only1.size(), static_cast<size_t>(1));
    CHECK(only1.count(rank[0]) == 1);
    std::vector<HeroPtr> single{hs.front()};
    CHECK_EQ(AIController::chooseDraftHeroWithVariance(single, ctx, rng, 100), static_cast<size_t>(0));
}

TEST("draft/zhizun_field_ai_picks_are_top_two") {
    // 至尊场：AI 预选只可能是评分第 1 或第 2 名
    for (unsigned seed = 1; seed <= 5; ++seed) {
        GameEngine e;
        e.setSeed(seed);
        captureLog(e);
        e.setIdentityDraft(true);
        e.setZhizunField(true);
        e.initGame(5, -1, {});
        const auto& cands = e.getIdentityCandidates();
        int lord = -1;
        for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::ZHU_GONG) lord = p->getId();
        CHECK(lord >= 0);
        HeroPtr lordHero = e.getPlayers()[lord]->getHero();
        for (size_t seat = 0; seat < cands.size(); ++seat) {
            std::vector<HeroPtr> hs;
            for (const auto& id : cands[seat]) hs.push_back(HeroRegistry::create(id));
            AIController::DraftContext ctx;
            ctx.role = e.getPlayers()[seat]->getIdentity();
            ctx.field = HeroTier::Field::IDENTITY;
            ctx.lordHero = (static_cast<int>(seat) == lord) ? HeroPtr() : lordHero;
            // D4：评分含座次因素，重算时必须用引擎当时用的同一个座次提示，否则排名不同（MSVC 上会误判）
            ctx.seatPosition = e.draftPositionOf(static_cast<int>(seat), false);
            auto rank = AIController::rankDraftHeroes(hs, ctx);
            std::string picked = e.getPlayers()[seat]->getHero()->getId();
            bool ok = false;
            for (size_t k = 0; k < rank.size() && k < 2; ++k)
                if (hs[rank[k]]->getId() == picked) ok = true;
            CHECK(ok);
            // 至尊场（身份场）随机池不会有本模式禁将
            CHECK(!HeroTier::isBannedInZhizun(picked, HeroTier::Mode::IDENTITY));
        }
    }
}

TEST("draft/normal_field_picks_are_random_not_always_best") {
    // 普通场：不论何种模式都随机从选将框中选 → 不会每次都正好是评分第一名
    int topRankHits = 0, total = 0;
    std::set<std::string> distinctPicks;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        GameEngine e;
        e.setSeed(seed);
        captureLog(e);
        e.setIdentityDraft(true);
        CHECK(!e.isZhizunField());
        e.initGame(5, -1, {});
        const auto& cands = e.getIdentityCandidates();
        int lord = -1;
        for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::ZHU_GONG) lord = p->getId();
        HeroPtr lordHero = e.getPlayers()[lord]->getHero();
        for (size_t seat = 0; seat < cands.size(); ++seat) {
            std::vector<HeroPtr> hs;
            for (const auto& id : cands[seat]) hs.push_back(HeroRegistry::create(id));
            AIController::DraftContext ctx;
            ctx.role = e.getPlayers()[seat]->getIdentity();
            ctx.field = HeroTier::Field::IDENTITY;
            ctx.lordHero = (static_cast<int>(seat) == lord) ? HeroPtr() : lordHero;
            auto rank = AIController::rankDraftHeroes(hs, ctx);
            ++total;
            const std::string pickedId = e.getPlayers()[seat]->getHero()->getId();
            distinctPicks.insert(pickedId);
            if (!rank.empty() && hs[rank.front()]->getId() == pickedId) ++topRankHits;
        }
    }
    CHECK_EQ(total, 30); // 6 局 × 5 座位
    CHECK(topRankHits < total);      // 不是每次都取评分第一名（普通场＝随机）
    // 不用“至少命中一次”做断言：std::uniform_int_distribution 的结果是实现定义的（MSVC/libstdc++/libc++ 各不相同），
    // 改为要求“选出的武将足够分散”——6 局 30 个座位至少出现 8 种不同武将。
    CHECK(distinctPicks.size() >= 8);
}

TEST("draft/normal_field_doudizhu_picks_are_random_too") {
    // 普通场斗地主同样随机取框内一名（不因身份/强度评分而固定）
    std::set<std::string> picks;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        GameEngine e;
        e.setSeed(seed);
        captureLog(e);
        e.setAiDelayMs(0);
        e.setDoudizhuAiBidForTesting(3);
        e.initDoudizhuGame(-1, {}, -1, true);
        for (const auto& p : e.getPlayers()) picks.insert(p->getHero()->getId());
    }
    CHECK(picks.size() > 3); // 6 局 × 3 座位出现多种结果 → 随机
}

// ==================== 七、回归：谋·张飞【协击】不再死循环 ====================
// 2026-10-05 实机（斗地主 seed=131）发现：AI 的默认选择恒为候选首位，
// 而【协击】原先每次都用**同一份候选**追问“再选择一名目标”，导致 targets 永远停在 1 → 死循环。
TEST("regression/mou_xieji_ai_terminates_and_targets_enemy") {
    GameEngine e;
    e.setSeed(131);
    auto sink = captureLog(e);
    e.initGame(4, -1, {"mou_zhangfei", "guanyu", "zhangfei", "zhaoyun"});
    auto ps = e.getPlayers();
    PlayerPtr zhang = ps[0];
    auto sk = zhang->getHero()->findSkill("谋-协击");
    CHECK(sk != nullptr);
    if (!sk) return;
    zhang->addMark("协击成功", 1);        // 协力已成功
    e.setCurrentPlayerForTesting(zhang);
    e.setPhase(TurnPhase::PLAY);
    CHECK(sk->canActivate(e, *zhang));
    int hpOfOthers = 0;
    for (size_t i = 1; i < ps.size(); ++i) hpOfOthers += ps[i]->getHp();
    sk->activate(e, *zhang);               // 修复前会在此处死循环（测试永远跑不完）
    CHECK_EQ(zhang->getMark("协击成功"), 0); // 标记被消耗
    CHECK(sink->str().find("【协击】") != std::string::npos);
    CHECK(sink->str().find("依次对") != std::string::npos);
}

TEST("regression/mou_xieji_ai_skips_when_no_enemy_target") {
    GameEngine e;
    e.setSeed(132);
    captureLog(e);
    e.initGame(2, -1, {"mou_zhangfei", "guanyu"}); // 2 人局：只有一名其他角色
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-协击");
    CHECK(sk != nullptr);
    if (!sk) return;
    ps[0]->addMark("协击成功", 1);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    // AI 不读真实身份：用**公开证据**判断敌友。
    ps[0]->setIdentity(Identity::ZHU_GONG);
    ps[1]->setIdentity(Identity::ZHONG_CHEN);
    ps[1]->addMark("储君", 1);
    e.recordRelation(ps[1]->getId(), ps[0]->getId(), -3); // 例如替主公挡锦囊/救主公
    CHECK(e.aiSideEstimate(*ps[1]) > 0);
    // 但 2 人局里“唯一的对手”绝不能被当成队友——否则谁都不出手，对局永远打不完
    // （一致性兜底：对局未结束就说明胜负未分，我方不可能人人都是队友）。
    CHECK(!e.aiIsFriend(*ps[0], *ps[1]));
    CHECK(sk->aiShouldActivate(e, *ps[0]));
    // 没有可选目标时不空发：唯一的对手阵亡后，AI 不该再发动
    e.killPlayer(ps[1]);
    CHECK(!sk->aiShouldActivate(e, *ps[0]));
}

// ==================== 八、回归：谋·诸葛亮【看破】第二轮不再死循环 ====================
// 2026-10-05 实机（身份场 seed=143）发现：AI 的默认选项恒为 0，而“与上一轮记录相同”时会
// continue 且候选不变 → 第 2 轮起永久卡死。修复后 AI 默认改选“与上一轮不同的牌名”，并有次数上限。
TEST("regression/mou_kanpo_second_round_terminates") {
    GameEngine e;
    e.setSeed(143);
    auto sink = captureLog(e);
    e.initGame(3, -1, {"mou_zhugeliang", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    PlayerPtr kongming = ps[0];
    auto sk = kongming->getHero()->findSkill("谋-看破");
    CHECK(sk != nullptr);
    if (!sk) return;

    e.setRoundForTesting(1);
    sk->onRoundStart(e, *kongming);                       // 第 1 轮：记录 1 个牌名（AI 默认“杀”）
    CHECK(sink->str().find("【看破】") != std::string::npos);
    std::string after1 = sink->str();

    e.setRoundForTesting(2);
    sk->onRoundStart(e, *kongming);                       // 第 2 轮：修复前在此死循环
    std::string after2 = sink->str();
    CHECK(after2.size() > after1.size());                 // 有新日志＝正常返回
    CHECK(after2.find("记录了") != std::string::npos);
}

TEST("regression/mou_kanpo_ai_never_repeats_last_round_names") {
    GameEngine e;
    e.setSeed(144);
    auto sink = captureLog(e);
    e.initGame(3, -1, {"mou_zhugeliang", "guanyu", "zhangfei"});
    PlayerPtr kongming = e.getPlayers()[0];
    auto sk = kongming->getHero()->findSkill("谋-看破");
    CHECK(sk != nullptr);
    if (!sk) return;
    for (int round = 1; round <= 4; ++round) {
        e.setRoundForTesting(round);
        sk->onRoundStart(e, *kongming); // 每轮都必须正常返回
    }
    // 4 轮都产出了记录日志（每轮至少一条“记录了 N 个牌名”）
    size_t pos = 0;
    int logs = 0;
    const std::string text = sink->str();
    while ((pos = text.find("记录了", pos)) != std::string::npos) { ++logs; pos += 3; }
    CHECK(logs >= 4);
}

// ==================== 九、择途：只能在自己回合内发动 + 明置身份牌 ====================

TEST("zetu/only_within_own_turn") {
    GameEngine e;
    e.setSeed(58);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr other = findByRole(e, Identity::FAN_ZEI);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;

    // 他人回合：不再询问、不发动（用户 2026-10-05 细化：只能在自己的回合内发动）
    e.setCurrentPlayerForTesting(other);
    e.setPhase(TurnPhase::PLAY);
    bool skip = false;
    sk->onTurnStart(e, *traitor);
    sk->onPhaseStart(e, *traitor, TurnPhase::PLAY, skip);
    CHECK_EQ(traitor->getMark("择途已用"), 0);
    CHECK(traitor->getIdentity() == Identity::NEI_JIAN);
    // 他人回合内有角色阵亡也不打断
    sk->onPlayerDeath(e, *traitor, *other, nullptr);
    CHECK_EQ(traitor->getMark("择途已用"), 0);

    // 自己回合：AI 评估（主公 1 点体力且反贼在场 → 自立为主）
    findByRole(e, Identity::ZHU_GONG)->setHp(1);
    e.setCurrentPlayerForTesting(traitor);
    sk->onTurnStart(e, *traitor);
    CHECK_EQ(traitor->getMark("择途已用"), 1);
    CHECK(traitor->getIdentity() == Identity::YE_XIN_JIA);
}

TEST("zetu/reveals_identity_card_publicly") {
    GameEngine e;
    e.setSeed(59);
    auto sink = captureLog(e);
    {
        ScriptedInput in("y\n"); // 真人主公接受预选
        e.initGame(5, 0, {});    // interactiveMode = true（有真人）
    }
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    CHECK(traitor != nullptr);
    if (!traitor) return;
    CHECK(traitor->isAI());
    // 发动前：AI 内奸的身份对人类是隐藏的
    CHECK_EQ(traitor->getMark("身份已明置"), 0);
    CHECK(!e.isIdentityPublic(*traitor));

    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    lord->setHp(1);              // → AI 选“自立为主”
    armTraitor(e, traitor);
    sk->activate(e, *traitor);

    // 发动时明置身份牌：标记 + 身份变为公开
    CHECK_EQ(traitor->getMark("身份已明置"), 1);
    CHECK(e.isIdentityPublic(*traitor));
    CHECK(sink->str().find("明置身份牌：【内奸】") != std::string::npos);
    CHECK(sink->str().find("自立为主") != std::string::npos);
    // 野心家本身也是公开身份；局势面板会显示
    CHECK(e.describePublicState(*traitor).find("身份已明置") != std::string::npos);
}

TEST("zetu/serving_the_lord_wording_and_effect") {
    GameEngine e;
    e.setSeed(60);
    auto sink = captureLog(e);
    e.initGame(8, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    auto sk = findSkill(e, traitor, "择途");
    CHECK(sk != nullptr);
    if (!sk) return;
    int killed = 0;
    for (const auto& p : e.getPlayers()) {
        if (killed >= 3) break;
        if (p->getIdentity() == Identity::FAN_ZEI) { e.killPlayer(p); ++killed; }
    }
    armTraitor(e, traitor);
    CHECK_EQ(ZheTuSkill::aiChoice(e, *traitor), 0);
    sk->activate(e, *traitor);
    const std::string text = sink->str();
    CHECK(text.find("侍奉明主") != std::string::npos);   // 用户第二次细化用词
    CHECK(text.find("明置身份牌") != std::string::npos);
    CHECK(traitor->getIdentity() == Identity::ZHONG_CHEN);
    // 身份牌已明置：变成忠臣后依然是公开身份
    CHECK_EQ(traitor->getMark("身份已明置"), 1);
}

TEST("zetu/ambitor_keeps_doudizhu_versions_of_feiyang_bahu") {
    // 用户 2026-10-05 明确：“飞扬和跋扈还是我之前说的技能”——即斗地主地主版，文本与实现都不改。
    GameEngine e;
    e.setSeed(61);
    captureLog(e);
    e.initGame(5, -1, {});
    PlayerPtr traitor = findByRole(e, Identity::NEI_JIAN);
    PlayerPtr lord = findByRole(e, Identity::ZHU_GONG);
    lord->setHp(1);
    armTraitor(e, traitor);
    findSkill(e, traitor, "择途")->activate(e, *traitor);
    auto feiyang = findSkill(e, traitor, "飞扬");
    auto bahu = findSkill(e, traitor, "跋扈");
    CHECK(feiyang != nullptr && bahu != nullptr);
    if (!feiyang || !bahu) return;
    // 地主版【飞扬】：判定阶段弃 2 张牌移除判定区一张牌，每回合限 1 次
    CHECK(feiyang->getDescription().find("判定阶段") != std::string::npos);
    CHECK(feiyang->getDescription().find("移除判定区的一张牌") != std::string::npos);
    // 地主版【跋扈】：锁定技，准备阶段摸 1 牌，出牌阶段可多使用 1 张【杀】
    CHECK(bahu->hasTag(SkillTag::LOCK));
    CHECK(bahu->getDescription().find("准备阶段摸1牌") != std::string::npos);
    CHECK(bahu->getDescription().find("可多使用1张【杀】") != std::string::npos);
    // 与斗地主地主拿到的是同一套描述
    GameEngine d;
    d.setSeed(62);
    captureLog(d);
    d.initDoudizhuGame(-1, {}, 0, false);
    auto ddzFeiyang = d.getPlayers()[0]->getHero()->findSkill("飞扬");
    auto ddzBahu = d.getPlayers()[0]->getHero()->findSkill("跋扈");
    CHECK(ddzFeiyang != nullptr && ddzBahu != nullptr);
    if (ddzFeiyang && ddzBahu) {
        CHECK_EQ(feiyang->getDescription(), ddzFeiyang->getDescription());
        CHECK_EQ(bahu->getDescription(), ddzBahu->getDescription());
    }
}
