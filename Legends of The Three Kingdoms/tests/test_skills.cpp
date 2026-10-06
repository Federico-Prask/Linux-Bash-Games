#include "test_helpers.h"
#include "Skills.h"

namespace {
struct TestGame {
    GameEngine engine;
    std::shared_ptr<std::ostringstream> log;
    explicit TestGame(int n, const std::vector<std::string>& heroIds = {}, unsigned seed = 7) {
        engine.setSeed(seed);
        log = captureLog(engine);
        engine.initGame(n, -1, heroIds);
    }
};
} // namespace

// ---------------- 武圣 ----------------

TEST("skill/wusheng_converts_only_red_non_sha") {
    WuShengSkill wusheng;
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, -1, {"guanyu", "zhangfei"});
    Player& self = *engine.getPlayers()[0];

    auto redTao = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);
    auto conv = wusheng.convertCard(engine, self, redTao, CardSubType::SHA);
    CHECK(conv != nullptr);
    CHECK_EQ(conv->getName(), std::string("杀"));
    CHECK(conv->getSuit() == Suit::HEART); // 继承花色

    CHECK(wusheng.convertCard(engine, self, makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA),
                             CardSubType::SHA) == nullptr); // 黑色不能转
    auto redSha = makeCard("杀", Suit::DIAMOND, 6, CardType::BASIC, CardSubType::SHA);
    CHECK(wusheng.convertCard(engine, self, redSha, CardSubType::SHA) == nullptr); // 本来就是杀，无需转化
    CHECK(wusheng.convertCard(engine, self, redTao, CardSubType::SHAN) == nullptr); // 只能当杀
}

TEST("skill/jie_wusheng_diamond_sha_ignores_distance") {
    TestGame g(5, {"jie_guanyu", "guanyu", "zhangfei", "zhaoyun", "huangzhong"});
    auto& ps = g.engine.getPlayers();
    // 界关羽用方块【杀】可指定距离外目标（方天画戟距离 4 之外，用无距离判定验证）
    CardPtr diamondSha = makeCard("杀", Suit::DIAMOND, 6, CardType::BASIC, CardSubType::SHA);
    CHECK(g.engine.canUseShaOn(*ps[0], *ps[2], diamondSha)); // 距离 2，无武器基础范围 1，靠界武圣可指定
    CardPtr heartSha = makeCard("杀", Suit::HEART, 10, CardType::BASIC, CardSubType::SHA);
    CHECK(!g.engine.canUseShaOn(*ps[0], *ps[2], heartSha)); // 红桃杀无此效果，距离 2 > 范围 1
}

// ---------------- 龙胆 ----------------

TEST("skill/longdan_converts_sha_shan_both_ways") {
    LongDanSkill longdan;
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2, -1, {"zhaoyun", "guanyu"});
    Player& self = *engine.getPlayers()[0];

    auto shan = makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto tao = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);

    auto asSha = longdan.convertCard(engine, self, shan, CardSubType::SHA);
    CHECK(asSha != nullptr);
    CHECK(asSha->getSubType() == CardSubType::SHA);

    auto asShan = longdan.convertCard(engine, self, sha, CardSubType::SHAN);
    CHECK(asShan != nullptr);
    CHECK(asShan->getSubType() == CardSubType::SHAN);

    CHECK(longdan.convertCard(engine, self, tao, CardSubType::SHA) == nullptr); // 桃不能转
}

// ---------------- 咆哮 / 马术 ----------------

TEST("skill/paoxiao_unlimited_sha_via_engine") {
    TestGame g(2, {"zhangfei", "guanyu"});
    CHECK_EQ(g.engine.getShaLimit(*g.engine.getPlayers()[0]), 999);
}

TEST("skill/mashu_distance_minus_one_via_engine") {
    TestGame g(5, {"machao", "guanyu", "zhangfei", "zhaoyun", "huangzhong"});
    CHECK_EQ(g.engine.calculateDistance(*g.engine.getPlayers()[0], *g.engine.getPlayers()[3]), 1); // 2-1
}

// ---------------- 权计 / 自立 ----------------

TEST("skill/quanji_hand_limit_plus_quan") {
    TestGame g(2, {"zhonghui", "guanyu"});
    auto lord = g.engine.getPlayers()[0];
    QuanJiSkill quanji; // 直接测钩子
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
    int limit = lord->getHandLimit();
    quanji.onCalculateHandLimit(g.engine, *lord, limit);
    CHECK_EQ(limit, lord->getHp() + 2); // 手牌上限 + 权数
}

TEST("skill/quanji_std_ignores_quanmou_x") {
    TestGame g(2, {"zhonghui", "guanyu"});
    auto lord = g.engine.getPlayers()[0];
    CHECK(lord->getHero()->findSkill("权谋") == nullptr); // 标钟会没有权谋/X
    int handBefore = lord->getHandCardCount();
    int pileBefore = lord->getPileCount(PILE_QUAN);
    lord->getHero()->findSkill("一-权计")->onAfterDamage(g.engine, *lord, nullptr, 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(lord->getPileCount(PILE_QUAN), pileBefore + 1);
    CHECK_EQ(lord->getHandCardCount(), handBefore); // 只摸1置1，无 X 摸牌
    CHECK(g.log->str().find("权谋") == std::string::npos);
}

TEST("skill/zili_awakens_at_three_quan") {
    TestGame g(2, {"zhonghui", "guanyu"});
    auto lord = g.engine.getPlayers()[0];
    // 座位 0 是主公：体力上限 4+1=5
    CHECK_EQ(lord->getMaxHp(), 5);
    lord->changeHp(-1); // 受伤，觉醒时选择回血
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
    lord->addToPile(PILE_QUAN, makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO));

    auto hero = lord->getHero();
    auto zili = hero->findSkill("一-自立");
    CHECK(zili != nullptr);
    bool skip = false;
    zili->onPhaseStart(g.engine, *lord, TurnPhase::PREPARATION, skip);
    // 觉醒后：先回血 4→5（上限 5），再减 1 上限并夹取：5/4
    CHECK_EQ(lord->getMaxHp(), 4);
    CHECK_EQ(lord->getHp(), 4);
    CHECK(hero->findSkill("一-排异") != nullptr);
    // 再次触发不再觉醒（觉醒技一次性）
    zili->onPhaseStart(g.engine, *lord, TurnPhase::PREPARATION, skip);
    CHECK_EQ(lord->getMaxHp(), 4);
    CHECK(g.log->str().find("觉醒技【自立】发动") != std::string::npos);
    // 未满 3 权时不触发
    TestGame g2(2, {"zhonghui", "guanyu"});
    auto lord2 = g2.engine.getPlayers()[0];
    auto zili2 = lord2->getHero()->findSkill("一-自立");
    zili2->onPhaseStart(g2.engine, *lord2, TurnPhase::PREPARATION, skip);
    CHECK_EQ(lord2->getMaxHp(), 5); // 主公 4+1，未减
}

// ---------------- 界烈弓 ----------------

TEST("skill/jie_liegong_lock_dodge_and_extra_damage") {
    TestGame g(2, {"jie_huangzhong", "guanyu"});
    auto& ps = g.engine.getPlayers();
    auto self = ps[0];   // 界黄忠（主公，HP 5）
    auto target = ps[1]; // 关羽（HP 4）
    self->changeHp(-1);  // 自身 HP 4，使"目标体力值不小于你"成立

    clearHand(*target);
    target->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA)); // 1 张 < 自己手牌数

    ShaContext ctx;
    ctx.source = self;
    ctx.target = target;
    ctx.card = makeCard("杀", Suit::HEART, 10, CardType::BASIC, CardSubType::SHA);

    auto skill = self->getHero()->findSkill("界-烈弓");
    CHECK(skill != nullptr);
    skill->onShaTargeted(g.engine, *self, ctx);
    // 目标手牌数(1) <= 自己手牌数(4)：不能闪
    CHECK(ctx.cannotDodge);
    // 目标体力值(4) >= 自己体力值(4)：伤害 +1
    CHECK_EQ(ctx.extraDamage, 1);
}

TEST("skill/jie_liegong_no_bonus_when_conditions_fail") {
    TestGame g(2, {"jie_huangzhong", "guanyu"});
    auto& ps = g.engine.getPlayers();
    auto self = ps[0];   // 界黄忠（主公，HP 5）
    auto target = ps[1]; // 关羽（HP 4）
    self->changeHp(-4);  // 自己 HP 1

    clearHand(*self);
    for (int i = 0; i < 3; ++i) self->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    clearHand(*target);
    for (int i = 0; i < 5; ++i) target->addHandCard(makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN));

    ShaContext ctx;
    ctx.source = self;
    ctx.target = target;
    ctx.card = makeCard("杀", Suit::HEART, 10, CardType::BASIC, CardSubType::SHA);

    auto skill = self->getHero()->findSkill("界-烈弓");
    skill->onShaTargeted(g.engine, *self, ctx);
    // 目标手牌 5 > 自己 3：不能锁闪
    CHECK(!ctx.cannotDodge);
    // 目标体力 4 >= 自己 1：伤害仍 +1
    CHECK_EQ(ctx.extraDamage, 1);
}

// ---------------- 义绝（次数限制） ----------------

TEST("skill/yijue_once_per_turn") {
    YiJueSkill yijue;
    TestGame g(2, {"jie_guanyu", "zhangfei"});
    auto self = g.engine.getPlayers()[0];
    auto other = g.engine.getPlayers()[1];
    clearHand(*self);
    self->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    CHECK(yijue.canActivate(g.engine, *self));
    yijue.markUsed();
    CHECK(!yijue.canActivate(g.engine, *self)); // 出牌阶段限一次
    (void)other;
}

// ---------------- 排异 ----------------

TEST("skill/paiyi_is_active_skill") {
    PaiYiSkill paiyi;
    CHECK(paiyi.getKind() == SkillKind::ACTIVE);
    CHECK_EQ(paiyi.getName(), std::string("一-排异"));
}

// ---------------- 技能标签体系 ----------------

TEST("skill/tags_and_kinds") {
    auto paoxiao = std::make_shared<PaoXiaoSkill>(false);
    CHECK(paoxiao->isLocked());
    CHECK(paoxiao->getKind() == SkillKind::STATE);
    CHECK(paoxiao->getTagString().find("锁定技") != std::string::npos);

    auto zili = std::make_shared<ZiLiSkill>();
    CHECK(zili->getTagString().find("觉醒技") != std::string::npos);

    auto wusheng = std::make_shared<WuShengSkill>(false);
    CHECK(wusheng->getKind() == SkillKind::ACTIVE);
    CHECK(wusheng->isConversionSkill());

    auto tieqi = std::make_shared<TieQiSkill>();
    CHECK(tieqi->getKind() == SkillKind::TRIGGER);
}
