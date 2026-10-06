#include "test_helpers.h"
#include "Interaction.h"
#include "Roles.h"
#include "HeroRegistry.h"
#include <numeric>
#include <sstream>
#include <algorithm>
#include <set>
#include <iostream>

// ---------------- 身份配置表 ----------------

TEST("roles/config_table_official") {
    struct Expect { int n, lord, loyal, rebel, traitor; };
    const Expect expects[] = {
        {2, 1, 0, 1, 0},
        {3, 1, 0, 1, 1},
        {4, 1, 1, 1, 1},
        {5, 1, 1, 2, 1},
        {6, 1, 1, 3, 1},
        {7, 1, 2, 3, 1},
        {8, 1, 2, 4, 1},
    };
    for (const auto& e : expects) {
        auto cfg = Roles::configFor(e.n);
        CHECK_EQ(cfg.lords, e.lord);
        CHECK_EQ(cfg.loyalists, e.loyal);
        CHECK_EQ(cfg.rebels, e.rebel);
        CHECK_EQ(cfg.traitors, e.traitor);
        CHECK_EQ(cfg.lords + cfg.loyalists + cfg.rebels + cfg.traitors, e.n);
    }
    // 越界夹取
    CHECK_EQ(Roles::configFor(1).lords, 1);
    CHECK_EQ(Roles::configFor(99).rebels, 4);
}

TEST("roles/build_identities_seat0_is_lord") {
    for (int n = Roles::MIN_PLAYERS; n <= Roles::MAX_PLAYERS; ++n) {
        auto ids = Roles::buildIdentities(n);
        CHECK_EQ(ids.size(), static_cast<size_t>(n));
        CHECK(ids[0] == Identity::ZHU_GONG);
        int lords = 0;
        for (auto id : ids) if (id == Identity::ZHU_GONG) ++lords;
        CHECK_EQ(lords, 1); // 恰好一名主公
    }
}

TEST("roles/shuffle_keeps_lord_and_is_permutation") {
    std::mt19937 rng(12345);
    auto ids = Roles::buildIdentities(8);
    Roles::shuffleIdentities(ids, rng);
    CHECK(ids[0] == Identity::ZHU_GONG);
    int lords = 0, rebels = 0, loyal = 0, traitors = 0;
    for (auto id : ids) {
        if (id == Identity::ZHU_GONG) ++lords;
        else if (id == Identity::ZHONG_CHEN) ++loyal;
        else if (id == Identity::FAN_ZEI) ++rebels;
        else ++traitors;
    }
    CHECK((lords == 1 && loyal == 2 && rebels == 4 && traitors == 1));
}

TEST("roles/describe_composition") {
    CHECK_EQ(Roles::describeComposition(4), std::string("1主公 1忠臣 1反贼 1内奸"));
    CHECK_EQ(Roles::describeComposition(5), std::string("1主公 1忠臣 2反贼 1内奸"));
    CHECK_EQ(Roles::describeComposition(8), std::string("1主公 2忠臣 4反贼 1内奸"));
    CHECK_EQ(Roles::describeComposition(2), std::string("1主公 1反贼"));
}

// ---------------- 胜负判定 ----------------

namespace {

// 斗地主人机流程的脚本输入（叫分/选将/换将）
struct DdzInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit DdzInput(const std::string& text) : script(text), saved(std::cin.rdbuf(script.rdbuf())) {
        Interaction::resetInputState();
    }
    ~DdzInput() { std::cin.rdbuf(saved); Interaction::resetInputState(); }
};
std::vector<PlayerPtr> makePlayers(const std::vector<Identity>& ids, const std::vector<bool>& alive) {
    std::vector<PlayerPtr> players;
    for (size_t i = 0; i < ids.size(); ++i) {
        auto p = std::make_shared<Player>(static_cast<int>(i), "p" + std::to_string(i), ids[i], true, blankHero());
        p->setAlive(alive[i]);
        players.push_back(p);
    }
    return players;
}
} // namespace

TEST("roles/gameover_lord_dead_rebels_win") {
    auto players = makePlayers({Identity::ZHU_GONG, Identity::ZHONG_CHEN, Identity::FAN_ZEI, Identity::NEI_JIAN},
                               {false, true, true, true});
    bool gameOver = false;
    std::string faction;
    Roles::checkGameOver(players, gameOver, faction);
    CHECK(gameOver);
    CHECK_EQ(faction, std::string("反贼胜"));
}

TEST("roles/gameover_lord_dead_lone_traitor_wins") {
    auto players = makePlayers({Identity::ZHU_GONG, Identity::ZHONG_CHEN, Identity::FAN_ZEI, Identity::NEI_JIAN},
                               {false, false, false, true});
    bool gameOver = false;
    std::string faction;
    Roles::checkGameOver(players, gameOver, faction);
    CHECK(gameOver);
    CHECK_EQ(faction, std::string("内奸胜 (一内独存)"));
}

TEST("roles/gameover_lord_side_wins") {
    auto players = makePlayers({Identity::ZHU_GONG, Identity::ZHONG_CHEN, Identity::FAN_ZEI, Identity::NEI_JIAN},
                               {true, true, false, false});
    bool gameOver = false;
    std::string faction;
    Roles::checkGameOver(players, gameOver, faction);
    CHECK(gameOver);
    CHECK_EQ(faction, std::string("主公与忠臣胜"));
}

TEST("roles/game_not_over_while_both_sides_alive") {
    auto players = makePlayers({Identity::ZHU_GONG, Identity::FAN_ZEI}, {true, true});
    bool gameOver = false;
    std::string faction;
    Roles::checkGameOver(players, gameOver, faction);
    CHECK(!gameOver);
}

TEST("roles/alive_count") {
    auto players = makePlayers({Identity::FAN_ZEI, Identity::FAN_ZEI, Identity::FAN_ZEI}, {true, false, true});
    CHECK_EQ(Roles::aliveCount(players, Identity::FAN_ZEI), 2);
    CHECK_EQ(Roles::aliveCount(players, Identity::ZHU_GONG), 0);
}

// ---------------- 斗地主（GameMode::DOUDIZHU）骨架 ----------------

TEST("roles/doudizhu_identities") {
    auto ids = Roles::doudizhuIdentities(0);
    CHECK_EQ(ids.size(), static_cast<size_t>(3));
    CHECK(ids[0] == Identity::DI_ZHU);
    CHECK(ids[1] == Identity::NONG_MIN);
    CHECK(ids[2] == Identity::NONG_MIN);
    auto ids2 = Roles::doudizhuIdentities(2);
    CHECK(ids2[2] == Identity::DI_ZHU);
    CHECK(ids2[0] == Identity::NONG_MIN && ids2[1] == Identity::NONG_MIN);
    auto ids3 = Roles::doudizhuIdentities(9); // 越界回退座位 0
    CHECK(ids3[0] == Identity::DI_ZHU);
}

TEST("roles/doudizhu_game_over") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"});
    CHECK(e.getGameMode() == GameEngine::GameMode::DOUDIZHU);
    auto ps = e.getPlayers();
    CHECK_EQ(ps.size(), static_cast<size_t>(3));
    CHECK(ps[0]->getIdentity() == Identity::DI_ZHU);
    CHECK(ps[1]->getIdentity() == Identity::NONG_MIN);
    CHECK(ps[0]->getMaxHp() == ps[0]->getHero()->getMaxHp() + 1); // 地主 +1 体力上限
    CHECK_EQ(e.getCurrentPlayer()->getId(), 0);                   // 地主先手
    // 一名农民阵亡：游戏继续
    e.killPlayer(ps[1], ps[0]);
    CHECK(!e.isGameOver());
    // 两名农民阵亡：地主胜
    e.killPlayer(ps[2], ps[0]);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("地主胜"));
}

TEST("roles/doudizhu_farmers_win_when_landlord_dies") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"});
    auto ps = e.getPlayers();
    e.killPlayer(ps[0], ps[1]);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("农民胜"));
}

// ---------------- 斗地主：叫分 / 选将 / 结算（2026-10-04；明牌/加倍已删去） ----------------

// 完整流程：人类（座位 0）叫 3 分直接成为地主；底分=3。
TEST("roles/doudizhu_bidding_three_points_makes_landlord") {
    GameEngine e;
    auto sink = captureLog(e);
    e.setDoudizhuAiBidForTesting(0); // 固定 AI 不叫，保证人类叫分结果可预期
    DdzInput in("4\n1\n"); // 第 4 项＝叫 3 分；选将第 1 项
    e.initDoudizhuGame(0, {}, -1, true);
    CHECK_EQ(e.getDoudizhuLandlordSeat(), 0);
    CHECK_EQ(e.getDoudizhuBaseScore(), 3);
    auto ps = e.getPlayers();
    CHECK(ps[0]->getIdentity() == Identity::DI_ZHU);
    CHECK_EQ(e.getCurrentPlayer()->getId(), 0);
    CHECK(sink->str().find("叫 3 分") != std::string::npos);
    // 所有人初始 4 张手牌；地主 +1 体力上限
    for (auto& p : ps) CHECK_EQ(p->getHandCardCount(), 4);
    CHECK_EQ(ps[0]->getMaxHp(), ps[0]->getHero()->getMaxHp() + 1);
}

// 叫分须高于上家：座位 0 叫 1 分，人类（座位 1）只能叫 2/3 分或不叫。
TEST("roles/doudizhu_bid_must_exceed_previous") {
    GameEngine e; captureLog(e);
    e.setDoudizhuAiBidForTesting(1); // 座位 0 的 AI 固定叫 1 分
    DdzInput in("2\n1\n"); // 人类（座位 1）在“不叫/叫2分/叫3分”中选择第 2 项＝叫 2 分；再选将
    e.initDoudizhuGame(1, {}, -1, true);
    CHECK_EQ(e.getDoudizhuLandlordSeat(), 1);
    CHECK_EQ(e.getDoudizhuBaseScore(), 2);
    CHECK(e.getPlayers()[1]->getIdentity() == Identity::DI_ZHU);
}

// 全不叫：首位（座位 0）默认地主，底分按 1。
TEST("roles/doudizhu_all_pass_defaults_to_first_seat") {
    GameEngine e; captureLog(e);
    e.setDoudizhuAiBidForTesting(0); // AI 全不叫
    DdzInput in("1\n1\n"); // 人类（座位0）不叫；选将第 1 项
    e.initDoudizhuGame(0, {}, -1, true);
    CHECK_EQ(e.getDoudizhuLandlordSeat(), 0);
    CHECK_EQ(e.getDoudizhuBaseScore(), 1);
}

// 选将：地主 5 个候选、农民各 3 个；候选间同一人物不重复。
TEST("roles/doudizhu_hero_draft_candidate_counts") {
    GameEngine e; captureLog(e);
    e.setDoudizhuAiBidForTesting(0);
    DdzInput in("4\n2\n"); // 第 4 项＝叫 3 分 → 座位 0 地主；选第 2 个候选
    e.initDoudizhuGame(0, {}, -1, true);
    const auto& cands = e.getDoudizhuCandidates();
    CHECK_EQ(cands.size(), static_cast<size_t>(3));
    CHECK_EQ(cands[0].size(), static_cast<size_t>(5)); // 地主 5 选 1
    CHECK_EQ(cands[1].size(), static_cast<size_t>(3)); // 农民 3 选 1
    CHECK_EQ(cands[2].size(), static_cast<size_t>(3));
    // 同一人物不重复（跨座位）
    std::set<std::string> persons;
    for (const auto& seatCands : cands)
        for (const auto& id : seatCands) persons.insert(HeroRegistry::personKey(id));
    size_t total = cands[0].size() + cands[1].size() + cands[2].size();
    CHECK_EQ(persons.size(), total);
}

// 结算：仅按叫分底分（明牌/加倍已按用户要求删去）——地主胜 +2×底分、农民 −底分；农民胜反之。
TEST("roles/doudizhu_settlement_by_base_score") {
    GameEngine e;
    auto sink = captureLog(e);
    e.setDoudizhuAiBidForTesting(0);
    DdzInput in("4\n1\n"); // 叫3分；选将1
    e.initDoudizhuGame(0, {}, -1, true);
    CHECK_EQ(e.getDoudizhuBaseScore(), 3);
    auto ps = e.getPlayers();
    // 日志中不应再出现明牌/加倍相关内容
    CHECK(sink->str().find("明牌") == std::string::npos);
    CHECK(sink->str().find("加倍") == std::string::npos);
    // 地主胜：+2×3=6，农民各 −3
    e.killPlayer(ps[1], ps[0]);
    e.killPlayer(ps[2], ps[0]);
    CHECK(e.isGameOver());
    auto scores = e.getDoudizhuScores();
    CHECK_EQ(scores[0], 6);
    CHECK_EQ(scores[1], -3);
    CHECK_EQ(scores[2], -3);
    CHECK(sink->str().find("斗地主结算") != std::string::npos);
    CHECK(sink->str().find("倍率") == std::string::npos);
    // 农民胜：地主 −2×底分、农民各 +底分
    GameEngine e2; captureLog(e2);
    e2.setDoudizhuAiBidForTesting(0);
    DdzInput in2("4\n1\n");
    e2.initDoudizhuGame(0, {}, -1, true);
    auto ps2 = e2.getPlayers();
    e2.killPlayer(ps2[0], ps2[1]);
    auto s2 = e2.getDoudizhuScores();
    CHECK_EQ(s2[0], -6);
    CHECK_EQ(s2[1], 3);
    CHECK_EQ(s2[2], 3);
}

// ---------------- 斗地主专属技能：飞扬 / 跋扈 / 共苦（移动版原文，2026-10-04） ----------------

// 地主【飞扬】：判定阶段弃 2 张牌并移除判定区的一张牌；每回合限 1 次。
TEST("roles/doudizhu_feiyang_removes_judge_card_once_per_turn") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"}); // 座位 0 地主（测试路径）
    auto ps = e.getPlayers();
    CHECK(ps[0]->getIdentity() == Identity::DI_ZHU);
    auto fei = ps[0]->getHero()->findSkill("飞扬");
    CHECK(fei != nullptr);
    if (!fei) return;
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::JUDGEMENT);
    auto le = makeCard("乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    ps[0]->addJudgeCard(le);
    int hand0 = ps[0]->getHandCardCount();
    bool skip = false;
    fei->onPhaseStart(e, *ps[0], TurnPhase::JUDGEMENT, skip);
    CHECK_EQ(ps[0]->getJudgeZone().size(), static_cast<size_t>(0)); // 移除判定区的一张牌
    CHECK_EQ(ps[0]->getHandCardCount(), hand0 - 2);                 // 弃 2 张牌
    // 每回合限 1 次：再次放入判定牌也不会发动
    auto le2 = makeCard("乐不思蜀", Suit::DIAMOND, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    ps[0]->addJudgeCard(le2);
    fei->onPhaseStart(e, *ps[0], TurnPhase::JUDGEMENT, skip);
    CHECK_EQ(ps[0]->getJudgeZone().size(), static_cast<size_t>(1));
    // 新回合重置后可再次发动
    fei->onTurnStart(e, *ps[0]);
    fei->onPhaseStart(e, *ps[0], TurnPhase::JUDGEMENT, skip);
    CHECK_EQ(ps[0]->getJudgeZone().size(), static_cast<size_t>(0));
    e.setPhase(TurnPhase::NONE);
}

// 地主【飞扬】：判定区没有牌 / 不足 2 张可弃牌时不发动。
TEST("roles/doudizhu_feiyang_requires_judge_card_and_two_cards") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"});
    auto ps = e.getPlayers();
    auto fei = ps[0]->getHero()->findSkill("飞扬");
    CHECK(fei != nullptr);
    if (!fei) return;
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::JUDGEMENT);
    bool skip = false;
    fei->onPhaseStart(e, *ps[0], TurnPhase::JUDGEMENT, skip); // 判定区为空：不发动
    CHECK_EQ(ps[0]->getHandCardCount(), 4);
    // 只留 1 张牌（手牌+装备共 1）
    while (ps[0]->getHandCardCount() > 1) ps[0]->removeHandCard(ps[0]->getHandCards().back());
    auto le = makeCard("乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    ps[0]->addJudgeCard(le);
    fei->onPhaseStart(e, *ps[0], TurnPhase::JUDGEMENT, skip); // 不足 2 张：不发动
    CHECK_EQ(ps[0]->getJudgeZone().size(), static_cast<size_t>(1));
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    e.setPhase(TurnPhase::NONE);
}

// 地主【跋扈】：锁定技，准备阶段摸 1 张牌；出牌阶段使用【杀】次数上限 +1。
TEST("roles/doudizhu_bahu_draws_and_raises_sha_limit") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"guanyu", "zhangfei", "zhaoyun"}); // 座位0用关羽（无“杀无次数限制”类技能）
    auto ps = e.getPlayers();
    auto bahu = ps[0]->getHero()->findSkill("跋扈");
    CHECK(bahu != nullptr);
    if (!bahu) return;
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PREPARATION);
    int hand0 = ps[0]->getHandCardCount();
    bool skip = false;
    bahu->onPhaseStart(e, *ps[0], TurnPhase::PREPARATION, skip);
    CHECK_EQ(ps[0]->getHandCardCount(), hand0 + 1); // 准备阶段摸 1 张
    e.setPhase(TurnPhase::PLAY);
    CHECK_EQ(e.getShaLimit(*ps[0]), 2);             // 出牌阶段多使用 1 张【杀】
    e.setPhase(TurnPhase::DRAW);
    CHECK_EQ(e.getShaLimit(*ps[0]), 1);             // 非出牌阶段不生效
    e.setPhase(TurnPhase::NONE);
    // 农民没有该技能
    CHECK(ps[1]->getHero()->findSkill("跋扈") == nullptr);
}

// 农民【共苦】：队友阵亡时，存活的农民可选择回复 1 点体力或摸 2 张牌。
TEST("roles/doudizhu_gongku_choice_draw_or_recover") {
    // AI 农民：默认摸 2 张
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"});
    auto ps = e.getPlayers();
    auto gk = ps[1]->getHero()->findSkill("共苦");
    CHECK(gk != nullptr);
    CHECK(ps[0]->getHero()->findSkill("共苦") == nullptr); // 地主没有共苦
    int hand1 = ps[1]->getHandCardCount();
    e.killPlayer(ps[2], ps[0]);
    CHECK_EQ(ps[1]->getHandCardCount(), hand1 + 2);
    CHECK_EQ(ps[1]->getMark("共苦已用"), 1);

    // 人类农民：选择回复 1 点体力
    GameEngine e2; captureLog(e2);
    e2.initDoudizhuGame(1, {"zhangfei", "guanyu", "zhaoyun"}); // 人类在座位 1（农民）
    auto ps2 = e2.getPlayers();
    ps2[1]->changeHp(-2);
    int hpBefore = ps2[1]->getHp();
    {
        DdzInput in("y\n1\n"); // 确认发动；选择回复 1 点体力
        e2.killPlayer(ps2[2], ps2[0]);
    }
    CHECK_EQ(ps2[1]->getHp(), hpBefore + 1);
}

// 斗地主专属技能分发：仅斗地主模式，且按身份区分。
TEST("roles/doudizhu_mode_skills_assignment") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"});
    auto ps = e.getPlayers();
    CHECK(ps[0]->getHero()->findSkill("飞扬") != nullptr);
    CHECK(ps[0]->getHero()->findSkill("跋扈") != nullptr);
    CHECK(ps[1]->getHero()->findSkill("共苦") != nullptr);
    CHECK(ps[2]->getHero()->findSkill("共苦") != nullptr);
    // 身份模式：不带斗地主模式技能
    GameEngine e2; captureLog(e2);
    e2.initGame(3, -1, {"zhangfei", "guanyu", "zhaoyun"});
    for (auto& p : e2.getPlayers()) {
        CHECK(p->getHero()->findSkill("飞扬") == nullptr);
        CHECK(p->getHero()->findSkill("共苦") == nullptr);
    }
}

// ---------------- 斗地主选将流程：先亮 3 将 → 叫分 → 地主再亮 2 将 → 选将（每位置可换将 2 次） ----------------

TEST("roles/doudizhu_draft_order_three_then_landlord_two") {
    GameEngine e; auto sink = captureLog(e);
    e.setDoudizhuAiBidForTesting(0);
    DdzInput in("1\n1\n"); // 不叫 → 座位 0 默认地主；选将第 1 项
    e.initDoudizhuGame(0, {}, -1, true);
    const std::string& out = sink->str();
    size_t p1 = out.find("【亮将】");
    size_t p2 = out.find("【叫地主】");
    size_t p3 = out.find("【亮将·地主追加】");
    size_t p4 = out.find("【选将】");
    CHECK(p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos && p4 != std::string::npos);
    CHECK(p1 < p2); // 先亮 3 将，再抢地主
    CHECK(p2 < p3); // 地主确定后再亮 2 将
    CHECK(p3 < p4);
    const auto& cands = e.getDoudizhuCandidates();
    CHECK_EQ(cands[0].size(), static_cast<size_t>(5)); // 地主 3+2
    CHECK_EQ(cands[1].size(), static_cast<size_t>(3));
    CHECK_EQ(cands[2].size(), static_cast<size_t>(3));
}

TEST("roles/doudizhu_hero_swap_limited_to_two_per_slot") {
    GameEngine e; auto sink = captureLog(e);
    e.setDoudizhuAiBidForTesting(0);
    // 不叫 → 选将：对第 1 个位置连续换将 3 次（第 3 次应被拒绝），再选第 1 项
    DdzInput in("1\nr1\nr1\nr1\n1\n");
    e.initDoudizhuGame(0, {}, -1, true);
    const std::string& out = sink->str();
    int swaps = 0;
    for (size_t pos = out.find("【换将】"); pos != std::string::npos; pos = out.find("【换将】", pos + 1)) ++swaps;
    CHECK_EQ(swaps, 2); // 每个位置最多 2 次
    CHECK(out.find("该位置换将次数已用完") != std::string::npos);
    // 选中第 1 个位置（换将后的武将）
    const auto& cands = e.getDoudizhuCandidates();
    CHECK(e.getPlayers()[0]->getHero()->getId() == cands[0][0]);
}

TEST("roles/doudizhu_ai_picks_from_its_candidates") {
    GameEngine e; captureLog(e);
    e.setDoudizhuAiBidForTesting(0);
    DdzInput in("1\n1\n");
    e.initDoudizhuGame(0, {}, -1, true);
    const auto& cands = e.getDoudizhuCandidates();
    for (int seat = 1; seat < 3; ++seat) { // AI 座位：所选武将在其候选内
        const std::string& picked = e.getPlayers()[seat]->getHero()->getId();
        CHECK(std::find(cands[seat].begin(), cands[seat].end(), picked) != cands[seat].end());
    }
}

// 各自互相看不到：有真人时只公布真人自己的候选，其余座位不展示；全 AI 观战则全部展示。
TEST("roles/doudizhu_candidates_hidden_from_others") {
    GameEngine e; auto sink = captureLog(e);
    e.setDoudizhuAiBidForTesting(0);
    DdzInput in("1\n1\n");
    e.initDoudizhuGame(0, {}, -1, true);
    const std::string& out = sink->str();
    CHECK(out.find("玩家(你) 的候选武将:") != std::string::npos);
    CHECK(out.find("（对方候选不公开）") != std::string::npos);
    // 真人的候选名单按名字输出；AI 的候选不出现具体武将标签
    CHECK(out.find("电脑1 的候选武将: （对方候选不公开）") != std::string::npos);

    GameEngine e2; auto sink2 = captureLog(e2);
    e2.setDoudizhuAiBidForTesting(0);
    e2.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"}, -1, false); // 全 AI 观战：直接流程
    CHECK(sink2->str().find("（对方候选不公开）") == std::string::npos);
}

// 斗地主：胜利优先级大于一切——地主被击杀立即判定农民胜并结算；
// 死者（地主）的非特殊技能不再发动，也不再有任何后续结算。
TEST("rules/doudizhu_victory_priority_and_settlement") {
    GameEngine e; auto sink = captureLog(e);
    e.initDoudizhuGame(-1, {"guanyu", "zhangfei", "zhaoyun"}); // 座位0 地主
    auto ps = e.getPlayers();
    CHECK(ps[0]->getIdentity() == Identity::DI_ZHU);
    e.killPlayer(ps[0], ps[1]);
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("农民胜"));
    const std::string& out = sink->str();
    CHECK(out.find("斗地主结算") != std::string::npos);     // 立即结算
    CHECK(out.find("胜利优先") != std::string::npos);
    // 死者技能不再被引擎采用
    CHECK(e.getEffectiveSkills(*ps[0]).empty());
    auto scores = e.getDoudizhuScores();
    CHECK_EQ(scores[0], -2);                               // 底分 1：地主 −2
    CHECK_EQ(scores[1], 1);
    CHECK_EQ(scores[2], 1);
}
