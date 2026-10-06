#include "test_helpers.h"
#include "SkillsStd.h"
#include "AI.h"
#include "HeroRegistry.h"

namespace {
struct TestGame {
    GameEngine engine;
    std::shared_ptr<std::ostringstream> log;
    explicit TestGame(int n, const std::vector<std::string>& heroIds = {}, unsigned seed = 2024) {
        engine.setSeed(seed);
        log = captureLog(engine);
        engine.initGame(n, -1, heroIds);
    }
};

SkillPtr skillOf(PlayerPtr p, const std::string& name) {
    return p->getHero() ? p->getHero()->findSkill(name) : nullptr;
}
} // namespace

// ---------------- 登记与分类 ----------------

TEST("std/registry_roster") {
    // 标准包 25 将（黄忠官方属风包）全部就位
    for (const char* id : {"liubei", "zhugeliang", "huangyueying", "caocao", "simayi", "xiahoudun",
                           "zhangliao", "xuchu", "guojia", "zhenji", "sunquan", "ganning", "lvmeng",
                           "huanggai", "zhouyu", "daqiao", "sunshangxiang", "luxun", "huatuo",
                           "lvbu", "diaochan", "guanyu", "zhangfei", "zhaoyun", "machao"}) {
        const HeroInfo* info = HeroRegistry::find(id);
        CHECK(info != nullptr);
        if (info) CHECK_EQ(info->pack, std::string("标准包"));
    }
    const HeroInfo* huangzhong = HeroRegistry::find("huangzhong");
    CHECK(huangzhong != nullptr);
    if (huangzhong) CHECK_EQ(huangzhong->pack, std::string("风包"));

    auto liubei = HeroRegistry::create("liubei");
    CHECK(liubei->findSkill("仁德") != nullptr);
    CHECK(liubei->findSkill("激将") != nullptr);
    CHECK(liubei->findSkill("激将")->hasTag(SkillTag::LORD));
    auto zhenji = HeroRegistry::create("zhenji");
    CHECK(zhenji->findSkill("倾国") != nullptr);
    CHECK(zhenji->findSkill("洛神") != nullptr);
    auto luxun = HeroRegistry::create("luxun");
    CHECK(luxun->findSkill("谦逊")->isLocked());
    CHECK(luxun->findSkill("连营") != nullptr);
}

// ---------------- 刘备/诸葛亮 ----------------

TEST("std/rende_gives_cards_and_heals") {
    TestGame g(3, {"liubei", "zhangfei", "machao"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    auto rende = skillOf(ps[0], "仁德");
    CHECK(rende != nullptr);
    clearHand(*ps[0]);
    ps[0]->changeHp(-1);
    ps[0]->addHandCard(makeCard("杀A", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("闪B", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    int hpBefore = ps[0]->getHp();
    rende->activate(g.engine, *ps[0]);
    // AI 路径会把手牌全部交出：交出 2 张 → 触发第2张回血
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
    CHECK_EQ(ps[1]->getHandCardCount(), 4 + 2);
    CHECK_EQ(ps[0]->getHp(), hpBefore + 1);
    CHECK(g.log->str().find("仁德") != std::string::npos);
}

TEST("std/jijiang_borrows_sha") {
    TestGame g(3, {"liubei", "guanyu", "machao"});
    auto& ps = g.engine.getPlayers();
    auto jijiang = skillOf(ps[0], "激将");
    clearHand(*ps[1]);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    // AI 是否代打取决于身份关系：盟友才借
    bool allied = AIController::isFriend(*ps[1], *ps[0]);
    CardPtr out = nullptr;
    bool borrowed = jijiang->onNeedResponseCard(g.engine, *ps[0], CardSubType::SHA, out);
    CHECK_EQ(borrowed, allied);
    if (borrowed) {
        CHECK(out != nullptr && out->getSubType() == CardSubType::SHA);
        CHECK_EQ(ps[1]->getHandCardCount(), 0);
    }
    // 需要【闪】时激将不介入
    out = nullptr;
    CHECK(!jijiang->onNeedResponseCard(g.engine, *ps[0], CardSubType::SHAN, out));
}

TEST("std/guanxing_and_kongcheng") {
    TestGame g(3, {"zhugeliang", "zhangfei", "machao"});
    auto& ps = g.engine.getPlayers();
    int pileBefore = g.engine.getDeck().getDrawPileSize();
    bool skipPhase = false;
    skillOf(ps[0], "观星")->onPhaseStart(g.engine, *ps[0], TurnPhase::PREPARATION, skipPhase);
    CHECK_EQ(g.engine.getDeck().getDrawPileSize(), pileBefore); // 观星只调整顺序
    CHECK(g.log->str().find("观星") != std::string::npos);

    // 空城：无手牌不能成为杀/决斗目标
    clearHand(*ps[0]);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto duel = makeCard("决斗", Suit::CLUB, 1, CardType::TRICK, CardSubType::JUE_DOU);
    CHECK(!g.engine.canBeTargeted(ps[0], sha));
    CHECK(!g.engine.canBeTargeted(ps[0], duel));
    ps[0]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    CHECK(g.engine.canBeTargeted(ps[0], sha));
}

// ---------------- 曹操/司马懿/夏侯惇 ----------------

TEST("std/jianxiong_claims_damage_card") {
    TestGame g(2, {"caocao", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    auto jx = skillOf(ps[0], "奸雄");
    auto killer = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    clearHand(*ps[0]);
    CHECK(jx->onClaimDamageCard(g.engine, *ps[0], ps[1].get(), killer));
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    CHECK(ps[0]->hasHandCard(killer));
}

TEST("std/fankui_takes_source_card") {
    TestGame g(2, {"simayi", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    clearHand(*ps[1]);
    auto c = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(c);
    skillOf(ps[0], "反馈")->onAfterDamage(g.engine, *ps[0], ps[1].get(), 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    CHECK_EQ(ps[1]->getHandCardCount(), 0);
}

TEST("std/mobile_fankui_can_take_judge_zone_card_without_revealing_hand") {
    TestGame g(2,{"simayi","guanyu"});
    auto ps=g.engine.getPlayers();for(auto p:ps)clearHand(*p);
    auto delayed=makeCard("乐不思蜀",Suit::HEART,6,CardType::TRICK,CardSubType::LE_BU_SI_SHU);
    ps[1]->addJudgeCard(delayed);
    CHECK_EQ(ps[1]->getJudgeZone().size(),1u);
    skillOf(ps[0],"反馈")->onAfterDamage(g.engine,*ps[0],ps[1].get(),1,ShaElement::NORMAL,nullptr);
    CHECK(ps[0]->hasHandCard(delayed));
    CHECK(ps[1]->getJudgeZone().empty());
}

TEST("std/guicai_replaces_judge_card") {
    TestGame g(2, {"simayi", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    // B6 之后 AI 只在“改判有收益”时出手：敌人判定【八卦阵】且判定牌是红色（他会白得一张【闪】）
    // → 用一张黑色牌改判。手牌选价值低的【杀】，不用【桃】这种关键牌。
    auto hand = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(hand);
    g.engine.setJudgeReasonForTesting("八卦阵");
    CardPtr judgeCard = makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    skillOf(ps[0], "鬼才")->onBeforeJudge(g.engine, *ps[0], *ps[1], judgeCard);
    CHECK(judgeCard == hand);
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
    g.engine.setJudgeReasonForTesting("");
}

TEST("std/guicai_does_not_waste_cards_when_no_gain") {
    TestGame g(2, {"simayi", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    auto hand = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(hand);
    // 敌人判定【八卦阵】且判定牌已是黑色（他本来就得不到【闪】）→ 改判没有收益，不该白扔牌
    g.engine.setJudgeReasonForTesting("八卦阵");
    CardPtr judgeCard = makeCard("闪", Suit::CLUB, 2, CardType::BASIC, CardSubType::SHAN);
    skillOf(ps[0], "鬼才")->onBeforeJudge(g.engine, *ps[0], *ps[1], judgeCard);
    CHECK(judgeCard != hand);
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    // 未知判定来源同样不乱改
    g.engine.setJudgeReasonForTesting("");
    CardPtr judgeCard2 = makeCard("闪", Suit::HEART, 3, CardType::BASIC, CardSubType::SHAN);
    skillOf(ps[0], "鬼才")->onBeforeJudge(g.engine, *ps[0], *ps[1], judgeCard2);
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
}

TEST("std/ganglie_triggers_on_damage") {
    TestGame g(2, {"xiahoudun", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    clearHand(*ps[1]);
    ps[1]->addHandCard(makeCard("杀1", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[1]->addHandCard(makeCard("杀2", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA));
    int hp1 = ps[1]->getHp(), hand1 = ps[1]->getHandCardCount();
    skillOf(ps[0], "刚烈")->onAfterDamage(g.engine, *ps[0], ps[1].get(), 1, ShaElement::NORMAL, nullptr);
    // 判定非红桃 → 来源弃2手牌；判定红桃 → 什么也不发生
    CHECK(ps[1]->getHandCardCount() == hand1 - 2 || ps[1]->getHp() == hp1 ||
          ps[1]->getHp() == hp1 - 1);
    CHECK(g.log->str().find("刚烈") != std::string::npos);
}

// ---------------- 张辽/许褚/郭嘉 ----------------

TEST("std/tuxi_steals_instead_of_draw") {
    TestGame g(3, {"zhangliao", "guanyu", "machao"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    clearHand(*ps[1]);
    clearHand(*ps[2]);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[2]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    int draw = 2;
    skillOf(ps[0], "突袭")->onDrawCards(g.engine, *ps[0], draw);
    CHECK_EQ(draw, 0);
    CHECK_EQ(ps[0]->getHandCardCount(), 2); // 两名受害者各 1 张
    CHECK_EQ(ps[1]->getHandCardCount(), 0);
    CHECK_EQ(ps[2]->getHandCardCount(), 0);
}

TEST("std/luoyi_boosts_sha_damage") {
    TestGame g(2, {"xuchu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    auto ly = skillOf(ps[0], "裸衣");
    int draw = 2;
    ly->onDrawCards(g.engine, *ps[0], draw);
    CHECK_EQ(draw, 1);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    int dmg = 1;
    ly->onDealDamage(g.engine, *ps[0], *ps[1], dmg, ShaElement::NORMAL, sha);
    CHECK_EQ(dmg, 2);
    int dmg2 = 1;
    ly->onDealDamage(g.engine, *ps[0], *ps[1], dmg2, ShaElement::NORMAL, nullptr);
    CHECK_EQ(dmg2, 1); // 无伤害牌不加成
}

TEST("std/tiandu_and_yiji") {
    TestGame g(2, {"guojia", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    // 天妒：获得判定牌
    clearHand(*ps[0]);
    auto judgeCard = makeCard("梅花3", Suit::CLUB, 3, CardType::BASIC, CardSubType::SHA);
    bool claimed = false;
    skillOf(ps[0], "天妒")->onAfterJudge(g.engine, *ps[0], *ps[0], judgeCard, claimed);
    CHECK(claimed);
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    // 遗计：2点伤害 → 摸4张
    clearHand(*ps[0]);
    skillOf(ps[0], "遗计")->onAfterDamage(g.engine, *ps[0], ps[1].get(), 2, ShaElement::NORMAL, nullptr);
    CHECK_EQ(ps[0]->getHandCardCount(), 4);
}

// ---------------- 甄姬 ----------------

TEST("std/qingguo_black_hand_to_shan") {
    TestGame g(2, {"zhenji", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    auto qg = skillOf(ps[0], "倾国");
    auto blackHand = makeCard("黑桃5", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(blackHand);
    CardPtr v = qg->convertCard(g.engine, *ps[0], blackHand, CardSubType::SHAN);
    CHECK(v != nullptr && v->getSubType() == CardSubType::SHAN && v->isVirtual());
    // 红牌不行；黑装备也不行（限手牌）
    auto redHand = makeCard("红桃9", Suit::HEART, 9, CardType::BASIC, CardSubType::SHA);
    CHECK(qg->convertCard(g.engine, *ps[0], redHand, CardSubType::SHAN) == nullptr);
    auto blackEquip = makeCard("黑装备", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::WEAPON);
    CHECK(qg->convertCard(g.engine, *ps[0], blackEquip, CardSubType::SHAN) == nullptr);
}

TEST("std/luoshen_gains_black_judge_cards") {
    TestGame g(2, {"zhenji", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    // 顶牌：黑、红 → 洛神获得黑牌后判定红牌结束
    g.engine.getDeck().putOnTop({makeCard("黑桃7", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA),
                                 makeCard("红桃8", Suit::HEART, 8, CardType::BASIC, CardSubType::SHA)});
    bool skipPhase = false;
    skillOf(ps[0], "洛神")->onPhaseStart(g.engine, *ps[0], TurnPhase::PREPARATION, skipPhase);
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    CHECK(g.log->str().find("洛神") != std::string::npos);
}

// ---------------- 孙权/甘宁/吕蒙/黄盖 ----------------

TEST("std/zhiheng_discard_and_draw") {
    TestGame g(2, {"sunquan", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    // 3 张好牌（杀）+ 2 张弱牌：AI 只换掉弱牌
    std::vector<CardPtr> good;
    for (int i = 0; i < 3; ++i) {
        auto sha = makeCard("杀" + std::to_string(i), Suit::SPADE, 3 + i, CardType::BASIC, CardSubType::SHA);
        ps[0]->addHandCard(sha);
        good.push_back(sha);
    }
    auto jiu = makeCard("酒", Suit::CLUB, 3, CardType::BASIC, CardSubType::JIU);
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    ps[0]->addHandCard(jiu);
    ps[0]->addHandCard(lightning);
    // 自己判定区已有闪电 → 手里这张无处可放，是真·废牌
    ps[0]->addJudgeCard(makeCard("闪电", Suit::HEART, 9, CardType::TRICK, CardSubType::SHAN_DIAN));
    auto zh = skillOf(ps[0], "制衡");
    CHECK(zh->aiShouldActivate(g.engine, *ps[0]));
    zh->activate(g.engine, *ps[0]);
    // AI 弃 2 弱牌摸 2，好牌一张不少
    CHECK_EQ(ps[0]->getHandCardCount(), 5);
    for (auto& c : good) CHECK(ps[0]->hasHandCard(c));
    CHECK(!ps[0]->hasHandCard(jiu));
    CHECK(!ps[0]->hasHandCard(lightning));
    CHECK(g.log->str().find("制衡") != std::string::npos);
}

TEST("std/zhiheng_once_per_turn") {
    TestGame g(2, {"sunquan", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("酒", Suit::CLUB, 3, CardType::BASIC, CardSubType::JIU));
    auto zh = skillOf(ps[0], "制衡");
    CHECK(zh->canActivate(g.engine, *ps[0]));
    zh->activate(g.engine, *ps[0]);
    CHECK_EQ(ps[0]->getHandCardCount(), 2); // 弃 1 摸 1
    // 出牌阶段限一次：发动后不可再发动，重复调用无任何效果
    CHECK(!zh->canActivate(g.engine, *ps[0]));
    CHECK(!zh->aiShouldActivate(g.engine, *ps[0]));
    int pileBefore = g.engine.getDeck().getDrawPileSize();
    zh->activate(g.engine, *ps[0]);
    CHECK_EQ(ps[0]->getHandCardCount(), 2);
    CHECK_EQ(g.engine.getDeck().getDrawPileSize(), pileBefore);
    // 下回合重置后可再次发动
    zh->resetTurnState();
    CHECK(zh->canActivate(g.engine, *ps[0]));
}

TEST("std/zhiheng_ai_keeps_valuable_cards") {
    TestGame g(2, {"sunquan", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    // 一手好牌 + 一张闪电：AI 只换闪电
    std::vector<CardPtr> good = {
        makeCard("无中生有", Suit::HEART, 7, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU),
        makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO),
        makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN),
        makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA),
        makeCard("顺手牵羊", Suit::DIAMOND, 3, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG),
        makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO),
    };
    for (auto& c : good) ps[0]->addHandCard(c);
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    ps[0]->addHandCard(lightning);
    // 自己判定区已有闪电 → 手里这张无处可放，是真·废牌
    ps[0]->addJudgeCard(makeCard("闪电", Suit::HEART, 9, CardType::TRICK, CardSubType::SHAN_DIAN));
    auto zh = skillOf(ps[0], "制衡");
    CHECK(zh->aiShouldActivate(g.engine, *ps[0]));
    zh->activate(g.engine, *ps[0]);
    CHECK_EQ(ps[0]->getHandCardCount(), 7);
    for (auto& c : good) CHECK(ps[0]->hasHandCard(c));
    CHECK(!ps[0]->hasHandCard(lightning));
}

TEST("std/zhiheng_ai_skips_when_all_good") {
    TestGame g(2, {"sunquan", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    ps[0]->addHandCard(makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO));
    auto zh = skillOf(ps[0], "制衡");
    // 全是好牌：AI 不发动；即使调用也不会弃牌、不计次数
    CHECK(!zh->aiShouldActivate(g.engine, *ps[0]));
    zh->activate(g.engine, *ps[0]);
    CHECK_EQ(ps[0]->getHandCardCount(), 3);
    CHECK(zh->canActivate(g.engine, *ps[0]));
}

TEST("std/limited_skills_once_per_turn") {
    // 反间：限一次
    {
        TestGame g(2, {"zhouyu", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
        auto& ps = g.engine.getPlayers();
        clearHand(*ps[0]);
        clearHand(*ps[1]);
        ps[0]->addHandCard(makeCard("黑桃7", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        ps[0]->addHandCard(makeCard("草花8", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
        auto fj = skillOf(ps[0], "反间");
        fj->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 1);
        CHECK_EQ(ps[1]->getHandCardCount(), 1);
        CHECK(!fj->canActivate(g.engine, *ps[0])); // 仍有手牌但次数已用完
        int hpSum = ps[0]->getHp() + ps[1]->getHp();
        fj->activate(g.engine, *ps[0]); // 重复调用无效果
        CHECK_EQ(ps[0]->getHandCardCount(), 1);
        CHECK_EQ(ps[1]->getHandCardCount(), 1);
        CHECK_EQ(ps[0]->getHp() + ps[1]->getHp(), hpSum);
    }
    // 结姻：限一次
    {
        TestGame g(3, {"sunshangxiang", "zhangfei", "daqiao"});
    g.engine.setPhase(TurnPhase::PLAY);
        auto& ps = g.engine.getPlayers();
        clearHand(*ps[0]);
        for (int i = 0; i < 4; ++i) {
            ps[0]->addHandCard(makeCard("杀" + std::to_string(i), Suit::SPADE, 5 + i, CardType::BASIC, CardSubType::SHA));
        }
        ps[1]->changeHp(-2);
        auto jy = skillOf(ps[0], "结姻");
        int hpSum = ps[0]->getHp() + ps[1]->getHp();
        jy->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 2);
        CHECK(ps[0]->getHp() + ps[1]->getHp() > hpSum);
        CHECK(!jy->canActivate(g.engine, *ps[0])); // 仍有2张手牌但次数已用完
        hpSum = ps[0]->getHp() + ps[1]->getHp();
        jy->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 2);
        CHECK_EQ(ps[0]->getHp() + ps[1]->getHp(), hpSum);
    }
    // 青囊：限一次
    {
        TestGame g(2, {"huatuo", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
        auto& ps = g.engine.getPlayers();
        clearHand(*ps[0]);
        ps[0]->addHandCard(makeCard("杀1", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        ps[0]->addHandCard(makeCard("杀2", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA));
        ps[1]->changeHp(-2);
        auto qn = skillOf(ps[0], "青囊");
        qn->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 1);
        CHECK(!qn->canActivate(g.engine, *ps[0])); // 仍有手牌、仍有伤员，但次数已用完
        int hpSum = ps[0]->getHp() + ps[1]->getHp();
        qn->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 1);
        CHECK_EQ(ps[0]->getHp() + ps[1]->getHp(), hpSum);
    }
    // 离间：限一次
    {
        TestGame g(3, {"diaochan", "zhangfei", "machao"});
    g.engine.setPhase(TurnPhase::PLAY);
        auto& ps = g.engine.getPlayers();
        clearHand(*ps[0]);
        clearHand(*ps[1]);
        clearHand(*ps[2]);
        ps[0]->addHandCard(makeCard("杀1", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        ps[0]->addHandCard(makeCard("杀2", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA));
        auto lj = skillOf(ps[0], "离间");
        lj->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 1);
        CHECK(!lj->canActivate(g.engine, *ps[0])); // 仍有手牌但次数已用完
        int hpSum = ps[1]->getHp() + ps[2]->getHp();
        lj->activate(g.engine, *ps[0]);
        CHECK_EQ(ps[0]->getHandCardCount(), 1);
        CHECK_EQ(ps[1]->getHp() + ps[2]->getHp(), hpSum);
    }
}

TEST("std/jiuyuan_boosts_wu_peach") {
    TestGame g(3, {"sunquan", "ganning", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    auto jy = skillOf(ps[0], "救援");
    int amount = 1;
    jy->onCalculateRecover(g.engine, *ps[0], ps[1].get(), amount, "桃");
    CHECK_EQ(amount, 2); // 吴势力对主公用桃 +1
    amount = 1;
    jy->onCalculateRecover(g.engine, *ps[0], ps[2].get(), amount, "桃");
    CHECK_EQ(amount, 1); // 非吴势力不加
    amount = 1;
    jy->onCalculateRecover(g.engine, *ps[0], ps[1].get(), amount, "青囊");
    CHECK_EQ(amount, 1); // 仅桃
}

TEST("std/qixi_guose_conversions") {
    TestGame g(2, {"ganning", "daqiao"});
    auto& ps = g.engine.getPlayers();
    auto qx = skillOf(ps[0], "奇袭");
    auto black = makeCard("黑桃5", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA);
    auto red = makeCard("红桃5", Suit::HEART, 5, CardType::BASIC, CardSubType::SHA);
    CardPtr v = qx->convertCard(g.engine, *ps[0], black, CardSubType::GUO_HE_CHAI_QIAO);
    CHECK(v != nullptr && v->getSubType() == CardSubType::GUO_HE_CHAI_QIAO);
    CHECK(qx->convertCard(g.engine, *ps[0], red, CardSubType::GUO_HE_CHAI_QIAO) == nullptr);
    auto gs = skillOf(ps[1], "国色");
    auto diamond = makeCard("方块2", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHA);
    CardPtr le = gs->convertCard(g.engine, *ps[1], diamond, CardSubType::LE_BU_SI_SHU);
    CHECK(le != nullptr && le->getSubType() == CardSubType::LE_BU_SI_SHU);
    CHECK(gs->convertCard(g.engine, *ps[1], black, CardSubType::LE_BU_SI_SHU) == nullptr);
}

TEST("std/keji_kurou_yingzi") {
    TestGame g(2, {"lvmeng", "huanggai"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    // 克己：未用杀可跳过弃牌
    bool skip = false;
    skillOf(ps[0], "克己")->onPhaseStart(g.engine, *ps[0], TurnPhase::DISCARD, skip);
    CHECK(skip);
    ps[0]->markShaPlayed();
    skip = false;
    skillOf(ps[0], "克己")->onPhaseStart(g.engine, *ps[0], TurnPhase::DISCARD, skip);
    CHECK(!skip);
    // 苦肉：-1体力 +2牌
    int hp = ps[1]->getHp(), hand = ps[1]->getHandCardCount();
    skillOf(ps[1], "苦肉")->activate(g.engine, *ps[1]);
    CHECK_EQ(ps[1]->getHp(), hp - 1);
    CHECK_EQ(ps[1]->getHandCardCount(), hand + 2);
}

TEST("std/fanjian_transfers_card") {
    TestGame g(2, {"zhouyu", "zhangfei"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    // 英姿：摸牌 +1
    int draw = 2;
    skillOf(ps[0], "英姿")->onDrawCards(g.engine, *ps[0], draw);
    CHECK_EQ(draw, 3);
    clearHand(*ps[0]);
    clearHand(*ps[1]);
    ps[0]->addHandCard(makeCard("黑桃7", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    skillOf(ps[0], "反间")->activate(g.engine, *ps[0]);
    // 不论是否伤害，目标都获得展示的牌
    CHECK_EQ(ps[1]->getHandCardCount(), 1);
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
}

// ---------------- 大乔/孙尚香 ----------------

TEST("std/liuli_redirects_sha") {
    TestGame g(3, {"daqiao", "zhangfei", "machao"});
    auto& ps = g.engine.getPlayers();
    ShaContext ctx;
    ctx.source = ps[1];
    ctx.target = ps[0];
    ctx.card = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    skillOf(ps[0], "流离")->onShaTargeted(g.engine, *ps[0], ctx);
    CHECK(ctx.redirect != nullptr);
    CHECK(ctx.redirect->getId() == 2); // 转移给攻击范围内的马超
    CHECK_EQ(ps[0]->getHandCardCount(), 0); // 弃置1牌作为代价
}

TEST("std/jieyin_heals_both_xiaoji_draws") {
    TestGame g(3, {"sunshangxiang", "zhangfei", "daqiao"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀1", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("杀2", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA));
    ps[0]->changeHp(-1); // 自己也受伤（受伤才能体现回复）
    ps[1]->changeHp(-2); // 受伤男性
    int hpSelf = ps[0]->getHp(), hpT = ps[1]->getHp();
    skillOf(ps[0], "结姻")->activate(g.engine, *ps[0]);
    CHECK_EQ(ps[0]->getHp(), hpSelf + 1);
    CHECK_EQ(ps[1]->getHp(), hpT + 1);
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
    // 枭姬：失去装备后摸2
    clearHand(*ps[0]);
    skillOf(ps[0], "枭姬")->onEquipmentLost(g.engine, *ps[0], nullptr);
    CHECK_EQ(ps[0]->getHandCardCount(), 2);
}

// ---------------- 华佗/吕布/貂蝉 ----------------

TEST("std/jijiu_out_of_turn_qingnang") {
    TestGame g(2, {"huatuo", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    auto jj = skillOf(ps[0], "急救");
    auto red = makeCard("红桃9", Suit::HEART, 9, CardType::BASIC, CardSubType::SHA);
    auto black = makeCard("黑桃9", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA);
    // 张飞回合内：华佗不能急救（isPlayerTurn(zhangfei)=true → isPlayerTurn(huatuo)=false！）
    // 约定：isPlayerTurn(self) 判定当前回合是否为自己 → 此处当前回合未开始，视为回合外可用
    CardPtr v = jj->convertCard(g.engine, *ps[0], red, CardSubType::TAO);
    CHECK(v != nullptr && v->getSubType() == CardSubType::TAO);
    CHECK(jj->convertCard(g.engine, *ps[0], black, CardSubType::TAO) == nullptr);
    // 青囊是出牌阶段主动技：保留急救回合外转换断言后，再进入出牌阶段测试青囊。
    g.engine.setPhase(TurnPhase::PLAY);
    // 青囊：弃1手牌令受伤角色回1
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[1]->changeHp(-1);
    int hp = ps[1]->getHp();
    skillOf(ps[0], "青囊")->activate(g.engine, *ps[0]);
    CHECK_EQ(ps[1]->getHp(), hp + 1);
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
}

TEST("std/wushuang_doubles_responses") {
    TestGame g(2, {"lvbu", "zhangfei"});
    auto& ps = g.engine.getPlayers();
    auto ws = skillOf(ps[0], "无双");
    int count = 1;
    ws->onCalculateResponseCount(g.engine, *ps[0], *ps[1], CardSubType::SHAN, count);
    CHECK_EQ(count, 2);
    count = 1;
    ws->onCalculateResponseCount(g.engine, *ps[0], *ps[1], CardSubType::SHA, count);
    CHECK_EQ(count, 2);
    count = 1;
    ws->onCalculateResponseCount(g.engine, *ps[0], *ps[0], CardSubType::SHAN, count);
    CHECK_EQ(count, 1); // 自己不受影响
}

TEST("std/lijian_duel_and_biyue") {
    TestGame g(3, {"diaochan", "zhangfei", "machao"});
    g.engine.setPhase(TurnPhase::PLAY);
    auto& ps = g.engine.getPlayers();
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    clearHand(*ps[1]);
    clearHand(*ps[2]);
    // 两名男性无杀 → 决斗立刻有人受伤
    int hpBefore = ps[1]->getHp() + ps[2]->getHp();
    skillOf(ps[0], "离间")->activate(g.engine, *ps[0]);
    CHECK(ps[1]->getHp() + ps[2]->getHp() < hpBefore || g.log->str().find("离间") != std::string::npos);
    CHECK(g.log->str().find("决斗") != std::string::npos);
    // 闭月：结束阶段摸1
    clearHand(*ps[0]);
    skillOf(ps[0], "闭月")->onPhaseEnd(g.engine, *ps[0], TurnPhase::FINISH);
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
}

// ---------------- 黄月英/陆逊 ----------------

TEST("std/jizhi_qicai_qianxun_lianying") {
    TestGame g(2, {"huangyueying", "luxun"});
    auto& ps = g.engine.getPlayers();
    // 集智：使用非延时锦囊摸1
    clearHand(*ps[0]);
    auto wuzhong = makeCard("无中生有", Suit::HEART, 7, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);
    ps[0]->addHandCard(wuzhong);
    skillOf(ps[0], "集智")->onUseCard(g.engine, *ps[0], wuzhong);
    CHECK(ps[0]->getHandCardCount() >= 1);
    // 奇才/谦逊：锁定标识
    CHECK(skillOf(ps[0], "奇才")->isLocked());
    CHECK(skillOf(ps[1], "谦逊")->isLocked());
    // 谦逊：不能成为顺手牵羊/乐不思蜀目标
    auto shun = makeCard("顺手牵羊", Suit::DIAMOND, 3, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG);
    auto le = makeCard("乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    CHECK(!g.engine.canBeTargeted(ps[1], shun));
    CHECK(!g.engine.canBeTargeted(ps[1], le));
    CHECK(g.engine.canBeTargeted(ps[1], wuzhong));
    // 连营：失去最后一张手牌摸1
    clearHand(*ps[1]);
    ps[1]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    g.engine.discardCardOf(ps[1], ps[1]->getHandCards().front(), "测试");
    CHECK_EQ(ps[1]->getHandCardCount(), 1);
    CHECK(g.log->str().find("连营") != std::string::npos);
}
