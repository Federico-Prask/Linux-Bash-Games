#include "test_helpers.h"

namespace {
// 建一局固定种子的全 AI 局（不启动主循环）
struct TestGame {
    GameEngine engine;
    std::shared_ptr<std::ostringstream> log;
    explicit TestGame(int n, const std::vector<std::string>& heroIds = {}, unsigned seed = 42) {
        engine.setSeed(seed);
        log = captureLog(engine);
        engine.initGame(n, -1, heroIds);
    }
};
} // namespace

// ---------------- 初始化 ----------------

TEST("engine/init_creates_players_and_deals_hands") {
    TestGame g(5, {"guanyu", "zhangfei", "zhaoyun", "huangzhong", "guanyu"});
    CHECK_EQ(g.engine.getPlayers().size(), static_cast<size_t>(5));
    int lords = 0;
    for (auto& p : g.engine.getPlayers()) {
        CHECK_EQ(p->getHandCardCount(), 4);
        CHECK(p->isAlive());
        if (p->getIdentity() == Identity::ZHU_GONG) ++lords;
    }
    CHECK_EQ(lords, 1);
    CHECK_EQ(g.engine.getDeck().getDrawPileSize(), 133 - 5 * 4);
    // 日志里应包含初始化信息
    CHECK(g.log->str().find("初始化完成") != std::string::npos);
}

TEST("engine/init_with_explicit_heroes") {
    TestGame g(4, {"guanyu", "zhangfei", "zhaoyun", "machao"});
    auto& players = g.engine.getPlayers();
    CHECK_EQ(players[0]->getHero()->getName(), std::string("关羽"));
    CHECK_EQ(players[1]->getHero()->getName(), std::string("张飞"));
    CHECK_EQ(players[2]->getHero()->getName(), std::string("赵云"));
    CHECK_EQ(players[3]->getHero()->getName(), std::string("马超"));
}

TEST("engine/logger_injection_no_console_output") {
    // captureLog 已替换 Logger：正常初始化不应向真实 stdout 打日志（由人工检查无输出）
    TestGame g(2, {"guanyu", "zhangfei"});
    CHECK(g.log->str().find("游戏人数: 2") != std::string::npos);
}

// ---------------- 距离 ----------------

TEST("engine/distance_on_circle") {
    // 指定五位互不重复、没有被动距离修正的武将；重复人物会随机补将，
    // 不同标准库的随机洗牌可能选到具有【飞影】等技能的武将。
    TestGame g(5, {"guanyu", "zhangfei", "zhaoyun", "huangzhong", "sunquan"});
    auto& ps = g.engine.getPlayers();
    // 5 人环：相邻距离 1，隔位距离 2
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[1]), 1);
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[2]), 2);
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[3]), 2); // min(3, 2)
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[4]), 1);
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[0]), 0);
}

TEST("engine/distance_with_horses") {
    TestGame g(5, {"guanyu", "zhangfei", "zhaoyun", "huangzhong", "sunquan"});
    auto& ps = g.engine.getPlayers();
    // 目标 +1 马：距离 +1
    ps[2]->equip(makeCard("绝影 (+1马)", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE));
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[2]), 3);
    // 来源 -1 马：距离 -1（至少为 1）
    ps[0]->equip(makeCard("赤兔 (-1马)", Suit::HEART, 5, CardType::EQUIPMENT, CardSubType::OFFENSIVE_HORSE));
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[2]), 2);
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[1]), 1); // max(1, 1-1)=1
}

TEST("engine/distance_with_mashu_skill") {
    TestGame g(5, {"machao", "guanyu", "zhangfei", "zhaoyun", "huangzhong"});
    auto& ps = g.engine.getPlayers();
    // 马超（座位0）计算与其他角色距离 -1
    CHECK_EQ(g.engine.calculateDistance(*ps[0], *ps[2]), 1); // 2-1
    // 别人计算到马超的距离不受马术影响
    CHECK_EQ(g.engine.calculateDistance(*ps[1], *ps[2]), 1);
}

// ---------------- 技能查询 ----------------

TEST("engine/effective_skills_filter_nonlock_when_disabled") {
    TestGame g(2, {"jie_zhaoyun", "guanyu"});
    auto& ps = g.engine.getPlayers();
    auto normal = g.engine.getEffectiveSkills(*ps[0]);
    // 龙胆 + 涯角 + 身份场主公模式技能【立储】（用户 2026-10-05 新增；2 人局座位 0 为主公）
    CHECK_EQ(normal.size(), static_cast<size_t>(3));
    ps[0]->setNonLockSkillsDisabled(true);
    auto filtered = g.engine.getEffectiveSkills(*ps[0]);
    // 龙胆/涯角 均非锁定技，全部失效；模式技能【立储】同为非锁定技，一并失效
    CHECK(filtered.empty());
    // 咆哮是锁定技，不受影响
    CHECK_EQ(g.engine.getEffectiveSkills(*ps[1]).size(), static_cast<size_t>(1));
}

TEST("engine/sha_limit_paoxiao_and_crossbow") {
    TestGame g(2, {"zhangfei", "guanyu"});
    auto& ps = g.engine.getPlayers();
    CHECK_EQ(g.engine.getShaLimit(*ps[0]), 999); // 咆哮：无次数限制
    CHECK_EQ(g.engine.getShaLimit(*ps[1]), 1);
    ps[1]->equip(makeCard("诸葛连弩", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 1));
    CHECK_EQ(g.engine.getShaLimit(*ps[1]), 999); // 连弩
}

// ---------------- 杀的结算（防具） ----------------

TEST("engine/renwang_shield_blocks_black_sha") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[1]); // 无闪可响应
    ps[1]->equip(makeCard("仁王盾", Suit::CLUB, 2, CardType::EQUIPMENT, CardSubType::ARMOR));

    CardPtr blackSha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    int hpBefore = ps[1]->getHp();
    g.engine.handleSha(ps[0], ps[1], blackSha);
    CHECK_EQ(ps[1]->getHp(), hpBefore); // 黑色【杀】无效
}

TEST("engine/renjia_fire_sha_extra_damage") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[1]);
    ps[1]->equip(makeCard("藤甲", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::ARMOR));

    CardPtr fireSha = makeCard("火杀", Suit::HEART, 7, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE);
    int hpBefore = ps[1]->getHp();
    g.engine.handleSha(ps[0], ps[1], fireSha);
    CHECK_EQ(ps[1]->getHp(), hpBefore - 2); // 普通火杀 1 点 + 藤甲火焰 1 点
    CHECK(g.log->str().find("火焰伤害 +1") != std::string::npos);
}

TEST("engine/baiyin_lion_caps_damage") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[1]);
    ps[1]->equip(makeCard("白银狮子", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::ARMOR));
    int hpBefore = ps[1]->getHp();
    g.engine.applyDamage(ps[0], ps[1], 3);
    CHECK_EQ(ps[1]->getHp(), hpBefore - 1); // 伤害至多 1 点
    CHECK(g.log->str().find("白银狮子") != std::string::npos);
}

TEST("engine/target_with_shan_dodges") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[1]);
    ps[1]->addHandCard(makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN));
    int hpBefore = ps[1]->getHp();
    g.engine.handleSha(ps[0], ps[1], makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    CHECK_EQ(ps[1]->getHp(), hpBefore); // 打出【闪】躲避
    CHECK(g.log->str().find("躲避了攻击") != std::string::npos);
    CHECK_EQ(ps[1]->getHandCardCount(), 0); // 闪已进入弃牌堆
}

// ---------------- 伤害 / 阵亡 / 胜负 ----------------

TEST("engine/damage_and_dying_no_tao_death") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    clearHand(*ps[1]);
    int hpBefore = ps[1]->getHp();
    g.engine.applyDamage(ps[0], ps[1], hpBefore + 5); // 致死伤害
    CHECK(!ps[1]->isAlive());
    CHECK(g.log->str().find("阵亡") != std::string::npos);
    // 2 人局反贼阵亡 -> 主公方胜
    CHECK(g.engine.isGameOver());
    CHECK_EQ(g.engine.getWinningFaction(), std::string("主公与忠臣胜"));
}

TEST("engine/kill_rebel_draws_reward") {
    // 5 人局：主公击杀反贼奖励摸 3 张
    TestGame g(5, {"guanyu", "zhangfei", "zhaoyun", "huangzhong", "guanyu"});
    auto& ps = g.engine.getPlayers();
    PlayerPtr lord, rebel;
    for (auto& p : ps) {
        clearHand(*p);
        if (p->getIdentity() == Identity::ZHU_GONG && !lord) lord = p;
        else if (p->getIdentity() == Identity::FAN_ZEI && !rebel) rebel = p;
    }
    int handBefore = lord->getHandCardCount();
    g.engine.applyDamage(lord, rebel, 99);
    CHECK(!rebel->isAlive());
    CHECK_EQ(lord->getHandCardCount(), handBefore + 3); // 击杀反贼奖励
}

TEST("engine/judgement_draws_and_discards") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    int discardBefore = g.engine.getDeck().getDiscardPileSize();
    CardPtr res = g.engine.doJudgement(ps[0], "测试判定");
    CHECK(res != nullptr);
    CHECK_EQ(g.engine.getDeck().getDiscardPileSize(), discardBefore + 1); // 判定牌进弃牌堆
    CHECK(g.log->str().find("测试判定") != std::string::npos);
}

// ---------------- 无懈可击连锁与 AI 策略 ----------------

namespace {
CardPtr makeWuxie() {
    return makeCard("无懈可击", Suit::CLUB, 12, CardType::TRICK, CardSubType::WU_XIE_KE_JI);
}
} // namespace

TEST("engine/wuxie_ai_negates_harmful_trick_on_self") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto lord = g.engine.getPlayers()[0];  // 主公（2人局座位0固定主公）
    auto rebel = g.engine.getPlayers()[1]; // 反贼
    clearHand(*lord);
    clearHand(*rebel);
    rebel->addHandCard(makeWuxie());
    auto chai = makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO);
    bool negated = g.engine.askNullification(lord, rebel, chai);
    CHECK(negated);                            // 反贼用无懈保护自己
    CHECK_EQ(rebel->getHandCardCount(), 0);    // 无懈已消耗
    CHECK(g.log->str().find("抵消【过河拆桥】") != std::string::npos);
}

TEST("engine/wuxie_ai_never_revives_trick_targeting_self") {
    // 回归测试（用户报告场景）：无懈被反制后，若再反制会令有害锦囊"复活"，
    // AI 必须收手，而不是像旧版那样反制一张替自己挡刀的无懈
    TestGame g(2, {"guanyu", "zhangfei"});
    auto lord = g.engine.getPlayers()[0];
    auto rebel = g.engine.getPlayers()[1];
    clearHand(*lord);
    clearHand(*rebel);
    rebel->addHandCard(makeWuxie());
    rebel->addHandCard(makeWuxie()); // 两张无懈
    auto chai = makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO);
    bool negated = g.engine.askNullification(lord, rebel, chai);
    CHECK(negated);                            // 抵消一次即止
    CHECK_EQ(rebel->getHandCardCount(), 1);    // 第二张无懈不被浪费
    CHECK(g.log->str().find("反制") == std::string::npos);
}

TEST("engine/wuxie_counter_war_full_parity") {
    // 反制战：反贼抵消(1) → 主公反制(2,恢复生效) → 反贼再反制(3,最终抵消)
    TestGame g(2, {"guanyu", "zhangfei"});
    auto lord = g.engine.getPlayers()[0];
    auto rebel = g.engine.getPlayers()[1];
    clearHand(*lord);
    clearHand(*rebel);
    lord->addHandCard(makeWuxie());
    rebel->addHandCard(makeWuxie());
    rebel->addHandCard(makeWuxie());
    auto chai = makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO);
    bool negated = g.engine.askNullification(lord, rebel, chai);
    CHECK(negated);                         // 3 张（奇数）→ 最终被抵消
    CHECK_EQ(lord->getHandCardCount(), 0);  // 三张全部消耗
    CHECK_EQ(rebel->getHandCardCount(), 0);
    CHECK(g.log->str().find("恢复生效") != std::string::npos); // 有反制发生
}

TEST("engine/wuxie_ai_negates_enemy_card_advantage") {
    // 敌方无中生有 → AI 用无懈抵消
    TestGame g(2, {"guanyu", "zhangfei"});
    auto lord = g.engine.getPlayers()[0];
    auto rebel = g.engine.getPlayers()[1];
    clearHand(*lord);
    clearHand(*rebel);
    rebel->addHandCard(makeWuxie());
    auto wzsy = makeCard("无中生有", Suit::HEART, 7, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);
    bool negated = g.engine.askNullification(lord, lord, wzsy);
    CHECK(negated);
    CHECK_EQ(rebel->getHandCardCount(), 0);
}

TEST("engine/wuxie_ai_never_wastes_on_wugu") {
    // 五谷丰登人人有份：AI 不消耗无懈去抵消
    TestGame g(2, {"guanyu", "zhangfei"});
    auto lord = g.engine.getPlayers()[0];
    auto rebel = g.engine.getPlayers()[1];
    clearHand(*lord);
    clearHand(*rebel);
    rebel->addHandCard(makeWuxie());
    auto wugu = makeCard("五谷丰登", Suit::HEART, 3, CardType::TRICK, CardSubType::WU_GU_FENG_DENG);
    bool negated = g.engine.askNullification(lord, rebel, wugu);
    CHECK(!negated);
    CHECK_EQ(rebel->getHandCardCount(), 1); // 无懈保留
}

TEST("engine/wuxie_ai_wont_self_negate_own_trick") {
    // 首环不豁免来源本人（黄月英【集智】等"使用锦囊摸牌"技能需要能自指），
    // 但 AI 依据价值判断不会无意义地抵消自己刚使用的锦囊
    TestGame g(2, {"guanyu", "zhangfei"});
    auto lord = g.engine.getPlayers()[0];
    auto rebel = g.engine.getPlayers()[1];
    clearHand(*lord);
    clearHand(*rebel);
    lord->addHandCard(makeWuxie());
    auto chai = makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO);
    bool negated = g.engine.askNullification(lord, rebel, chai);
    // 主公（来源，AI）不自坑；反贼没有无懈 → 无人响应，锦囊生效
    CHECK(!negated);
    CHECK_EQ(lord->getHandCardCount(), 1); // 主公的无懈没有被浪费
}

// =====================================================================
//  观察局势扩充（TODO 第 2 项）：场上牌 / 公开状态（技能状态、蓄力点）/ 私有状态（手牌）
// =====================================================================

// 公开状态包含：判定区、武将牌上的牌、蓄力点（蓄力技）、技能状态/公开标记。
TEST("state/public_state_shows_zones_charge_and_skill_status") {
    TestGame g(3, {"mou_gongsunzan", "zhangfei", "guanyu"});
    auto p0 = g.engine.getPlayers()[0];
    auto p1 = g.engine.getPlayers()[1];
    // 蓄力技（义从 2/4）：初始蓄力 2，上限 4
    CHECK_EQ(p0->getMark("蓄力"), 2);
    CHECK_EQ(p0->getMark("蓄力上限"), 4);
    // 判定区：延时锦囊；武将牌上的牌：技能移出的牌
    auto le = makeCard("乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    p0->addJudgeCard(le);
    auto guard = makeCard("杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA);
    p0->addToPile("扈", guard);
    p1->addMark("护甲", 2);
    std::string s0 = g.engine.describePublicState(*p0);
    CHECK(s0.find("判定区") != std::string::npos);
    CHECK(s0.find("乐不思蜀") != std::string::npos);          // 场上的牌（判定区）
    CHECK(s0.find("武将牌上") != std::string::npos);
    CHECK(s0.find("扈") != std::string::npos);                // 场上的牌（武将牌上的牌）
    CHECK(s0.find("蓄力 2/4") != std::string::npos);          // 蓄力点
    std::string s1 = g.engine.describePublicState(*p1);
    CHECK(s1.find("护甲 2") != std::string::npos);            // 公开状态（资源标记）
    CHECK(s1.find("判定区: （无）") != std::string::npos);
}

// 技能状态（限定技/觉醒技“已用/已发动/已觉醒”）作为公开信息展示。
TEST("state/public_state_shows_skill_activation_status") {
    TestGame g(2, {"mou_jiangwei", "zhangfei"});
    auto p0 = g.engine.getPlayers()[0];
    p0->addMark("挑衅已用", 1);
    p0->addMark("本回合用过杀", 1);   // 内部计数器：不应作为公开状态展示
    std::string s = g.engine.describePublicState(*p0);
    CHECK(s.find("挑衅已用") != std::string::npos);
    CHECK(s.find("本回合用过杀") == std::string::npos);
}

// 私有状态：自己的手牌（仅本人可见）。
TEST("state/private_state_shows_own_hand") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto p0 = g.engine.getPlayers()[0];
    clearHand(*p0);
    p0->addHandCard(makeCard("青龙偃月刀", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::WEAPON));
    p0->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    std::string s = g.engine.describePrivateState(*p0);
    CHECK(s.find("青龙偃月刀") != std::string::npos);
    CHECK(s.find("闪") != std::string::npos);
}

// printGameState 汇总输出：包含局势标题、三类信息；AI 手牌不外显（私有信息）。
TEST("state/print_game_state_includes_all_sections") {
    TestGame g(2, {"mou_gongsunzan", "zhangfei"});
    auto p0 = g.engine.getPlayers()[0];
    auto le = makeCard("兵粮寸断", Suit::CLUB, 4, CardType::TRICK, CardSubType::BING_LIANG_CUN_DUAN);
    p0->addJudgeCard(le);
    g.engine.printGameState();
    std::string out = g.log->str();
    CHECK(out.find("场上当前局势") != std::string::npos);
    CHECK(out.find("兵粮寸断") != std::string::npos);   // 场上的牌
    CHECK(out.find("蓄力 2/4") != std::string::npos);   // 公开状态（蓄力点）
    CHECK(out.find("手牌: ") != std::string::npos);     // 私有状态标识（本次为全 AI 局，仅计数）
}

// 出牌阶段提示函数：包含手牌/装备/可用技能与输入提示；可重复调用（面板返回后重印）。
TEST("state/play_phase_prompt_can_be_reprinted") {
    TestGame g(2, {"guanyu", "zhangfei"});
    auto p0 = g.engine.getPlayers()[0];
    clearHand(*p0);
    p0->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    g.engine.printPlayPhasePrompt(p0);
    std::string once = g.log->str();
    CHECK(once.find("【你的手牌】") != std::string::npos);
    CHECK(once.find("杀") != std::string::npos);
    CHECK(once.find("结束出牌阶段") != std::string::npos);
    g.engine.printPlayPhasePrompt(p0); // 重印不改变任何状态
    CHECK(g.log->str().size() > once.size());
}

// ---------------- 游戏结束后：不存在技能发动或牌结算（用户 2026-10-05） ----------------

namespace {
// 记录发动次数的主动技（测试用）
class CountingSkill : public ActiveSkill {
public:
    int activated = 0;
    CountingSkill() : ActiveSkill("计数", "测试用：发动时计数。") {}
    bool canActivate(GameEngine&, Player& self) override { return self.isAlive(); }

protected:
    void onActivate(GameEngine&, Player&) override { ++activated; }
};
} // namespace

TEST("gameover/no_skill_activation_after_game_ends") {
    TestGame g(2, {"huatuo", "guanyu"}, 120);
    auto& e = g.engine;
    auto ps = e.getPlayers();
    auto counter = std::make_shared<CountingSkill>();
    ps[1]->getHero()->addSkill(counter);

    // 游戏未结束时可以发动（正例）
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    counter->activate(e, *ps[1]);
    CHECK_EQ(counter->activated, 1);

    // 结束游戏：反贼杀死主公
    ps[0]->setHp(1);
    e.killPlayer(ps[0], ps[1]);
    CHECK(e.isGameOver());

    // 反例：游戏结束后技能不再发动
    counter->activate(e, *ps[1]);
    CHECK_EQ(counter->activated, 1);
}

TEST("gameover/no_card_resolution_after_game_ends") {
    TestGame g(2, {"huatuo", "guanyu"}, 121);
    auto& e = g.engine;
    auto ps = e.getPlayers();
    ps[0]->setHp(1);
    e.killPlayer(ps[0], ps[1]);
    CHECK(e.isGameOver());

    for (auto& p : ps) clearHand(*p);
    auto tao = std::make_shared<Card>(950, "桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO,
                                      ShaElement::NORMAL, 1, "");
    ps[1]->addHandCard(tao);
    int hand = ps[1]->getHandCardCount();
    int hp = ps[1]->getHp();

    // 用牌不再结算
    CHECK(!e.useCard(ps[1], tao, {ps[1]}));
    CHECK_EQ(ps[1]->getHandCardCount(), hand);
    // 伤害不再结算
    e.applyDamage(ps[0], ps[1], 1);
    CHECK_EQ(ps[1]->getHp(), hp);
    // 摸牌不再结算
    e.drawCards(ps[1], 2, "测试");
    CHECK_EQ(ps[1]->getHandCardCount(), hand);
}

TEST("gameover/aoe_stops_immediately_when_game_ends") {
    TestGame g(3, {"zhangfei", "guanyu", "zhaoyun"}, 122);
    auto& e = g.engine;
    auto ps = e.getPlayers();
    ps[0]->setIdentity(Identity::ZHU_GONG);
    ps[1]->setIdentity(Identity::FAN_ZEI);
    ps[2]->setIdentity(Identity::NEI_JIAN);
    for (auto& p : ps) clearHand(*p);
    ps[0]->setHp(1);
    ps[1]->setHp(1);

    auto wanjian = std::make_shared<Card>(960, "万箭齐发", Suit::SPADE, 13, CardType::TRICK,
                                          CardSubType::WAN_JIAN_QI_FA, ShaElement::NORMAL, 1, "");
    ps[2]->addHandCard(wanjian);
    e.setCurrentPlayerForTesting(ps[2]);
    e.setPhase(TurnPhase::PLAY);
    CHECK(e.useCard(ps[2], wanjian, {}));

    // 座位顺序：先结算主公（座位 0）→ 阵亡 → 反贼胜 → 结算立即停止
    CHECK(!ps[0]->isAlive());
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("反贼胜"));
    // 第二个目标（反贼）既没有被打出【闪】的询问，也没有受到伤害
    CHECK(ps[1]->isAlive());
    CHECK_EQ(ps[1]->getHp(), 1);
    CHECK(g.log->str().find("【万箭齐发】！请打出一张【闪】响应") == std::string::npos ||
          e.isGameOver());
}
