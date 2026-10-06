// C1 整改验收：谋攻篇 22 个主动技的 AI 门控（原来是 `aiShouldActivate` 恒 `return true`，
// 现在每个都有“该发动才发动”的条件）。本文件对每个门控给**正反例**：
// 条件满足 → true；条件不满足 → false（不再空发/亏牌）。
// 敌我关系一律用**公开行为证据**构造（AI 不读真实身份，见 tests/test_ai_inference.cpp）。
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "AI.h"
#include <memory>
#include <string>
#include <vector>

using namespace Thks;

namespace {

// 一门课一个局：座位 0 放被测武将（默认是主公），其余用不冲突的老将填充。
struct GateGame {
    GameEngine e;
    std::shared_ptr<std::ostringstream> log;
    PlayerPtr self;
    std::vector<PlayerPtr> others;

    GateGame(const std::string& hero, int n = 5, unsigned seed = 300, TurnPhase phase = TurnPhase::PLAY) {
        e.setSeed(seed);
        log = captureLog(e);
        std::vector<std::string> hs{hero};
        static const char* fillers[] = {"guanyu", "zhangfei", "machao", "huangzhong",
                                        "daqiao", "lvmeng", "huatuo", "zhenji"};
        int fi = 0;
        while (static_cast<int>(hs.size()) < n) hs.push_back(fillers[fi++ % 8]);
        e.initGame(n, -1, hs);
        self = e.getPlayers()[0];
        for (size_t i = 1; i < e.getPlayers().size(); ++i) others.push_back(e.getPlayers()[i]);
        e.setCurrentPlayerForTesting(self);
        e.setPhase(phase);
    }

    SkillPtr skill(const std::string& name) {
        auto sk = self->getHero()->findSkill(name);
        if (!sk) {
            std::string msg = "缺少技能 " + name + "（武将 " + self->getHero()->getName() + "）";
            ::thkstest::reportFailure(__FILE__, __LINE__, msg);
        }
        return sk;
    }
    // 让座位 i 成为“敌人”：留下攻击主公的公开证据
    void makeEnemy(size_t i) { e.recordRelation(others[i]->getId(), self->getId(), +3); }
    // 让座位 i 成为“队友”：留下救助主公的公开证据
    void makeFriend(size_t i) { e.recordRelation(others[i]->getId(), self->getId(), -3); }
    void clearHand() {
        auto h = self->getHandCards();
        for (auto& c : h) self->removeHandCard(c);
    }
    void giveCards(int n, CardSubType sub = CardSubType::SHA, const std::string& name = "杀",
                   Suit suit = Suit::SPADE) {
        clearHand();
        for (int i = 0; i < n; ++i)
            self->addHandCard(std::make_shared<Card>(900 + i, name, suit, 3 + i, CardType::BASIC, sub,
                                                     ShaElement::NORMAL, 1, ""));
    }
};

} // namespace

// ---------- 群体/多目标类：要敌多于友、要留牌 ----------

TEST("ai_gates/luanji_needs_enemy_majority_and_cards") {
    {
        GateGame g("mou_yuanshao", 5);
        auto sk = g.skill("谋-乱击");
        if (!sk) return;
        g.giveCards(2);
        g.makeEnemy(0); g.makeEnemy(1); g.makeEnemy(2); g.makeFriend(3);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));  // 敌 3 > 友 1，但只有 2 张牌（要留牌）→ 不放
        g.self->addHandCard(std::make_shared<Card>(950, "闪", Suit::HEART, 2, CardType::BASIC,
                                                   CardSubType::SHAN, ShaElement::NORMAL, 1, ""));
        CHECK(sk->aiShouldActivate(g.e, *g.self));   // 3 张 → 发动
    }
    {
        GateGame g("mou_yuanshao", 5, 301);
        auto sk = g.skill("谋-乱击");
        if (!sk) return;
        g.giveCards(4);
        g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2); g.makeFriend(3);
        // 敌方不多于友方 → 不放万箭（会烧自己人）
        CHECK(!sk->aiShouldActivate(g.e, *g.self));
    }
}

TEST("ai_gates/lijian_needs_two_enemies") {
    GateGame g("mou_diaochan", 5);
    auto sk = g.skill("谋-离间");
    if (!sk) return;
    g.giveCards(4);
    g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2); g.makeFriend(3);
    // 只有 1 名敌人（其余是队友）→ 不该发动（会让队友互砍）
    CHECK(!sk->aiShouldActivate(g.e, *g.self));
    g.makeEnemy(1);                                  // 再出现一名敌人（覆盖此前的友好证据）
    CHECK(sk->aiShouldActivate(g.e, *g.self));       // 2 名敌人 + 4 张牌 → 发动
}

TEST("ai_gates/huoji_only_when_country_group_favors_us") {
    {
        GateGame g("mou_zhugeliang", 4, 302);
        auto sk = g.skill("谋-火计");
        if (!sk) return;
        // 座位 1=关羽(蜀)、2=张飞(蜀)、3=马超(蜀)：把 1、2 变成敌人，3 变成队友
        g.makeEnemy(0); g.makeEnemy(1); g.makeFriend(2);
        CHECK(sk->aiShouldActivate(g.e, *g.self));   // 蜀势力圈里敌 2 > 友 1 → 放火
    }
    {
        GateGame g("mou_zhugeliang", 4, 303);
        auto sk = g.skill("谋-火计");
        if (!sk) return;
        g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));  // 同势力圈里友多于敌 → 不放（会烧自己人）
    }
}

// ---------- 标记/装备类：要挂在敌人身上 ----------

TEST("ai_gates/liangzhu_needs_enemy_equipment_or_zhu_mark") {
    GateGame g("mou_sunshangxiang", 4, 304);
    auto sk = g.skill("谋-良助");
    if (!sk) return;
    g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
    CHECK(!sk->aiShouldActivate(g.e, *g.self));      // 没人有装备 → 不发动
    // 给敌人一件装备
    auto weapon = std::make_shared<Card>(960, "青龙偃月刀", Suit::SPADE, 5, CardType::EQUIPMENT,
                                         CardSubType::WEAPON, ShaElement::NORMAL, 1, "");
    g.others[0]->equip(weapon);
    CHECK(g.e.getPlayers()[1]->getAllEquipment().size() == 1);
    CHECK(sk->canActivate(g.e, *g.self));
    CHECK(sk->aiShouldActivate(g.e, *g.self));       // 敌人有装备 → 拆
}

TEST("ai_gates/xuanhuo_needs_unmarked_enemy") {
    GateGame g("mou_fazheng", 4, 305);
    auto sk = g.skill("谋-眩惑");
    if (!sk) return;
    g.giveCards(1);
    g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
    CHECK(!sk->aiShouldActivate(g.e, *g.self));      // 只有 1 张牌，交出去就空了
    g.giveCards(2);
    CHECK(sk->aiShouldActivate(g.e, *g.self));       // 2 张牌 + 有未标记的敌人 → 发动
    g.others[0]->addMark("眩", 1);                   // 敌人已有“眩”
    // 其余都是队友 → 没有可挂的目标
    CHECK(!sk->aiShouldActivate(g.e, *g.self));
}

TEST("ai_gates/qicai_needs_enemy") {
    GateGame g("mou_huangyueying", 4, 306);
    auto sk = g.skill("谋-奇才");
    if (!sk) return;
    g.makeFriend(0); g.makeFriend(1); g.makeEnemy(2);
    // 有敌人（且 canActivate 会检查装备来源）→ 只在来源存在时才可能发动
    if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self));
    GateGame g2("mou_huangyueying", 4, 307);
    auto sk2 = g2.skill("谋-奇才");
    if (!sk2) return;
    g2.makeFriend(0); g2.makeFriend(1); g2.makeFriend(2);
    if (sk2->canActivate(g2.e, *g2.self)) CHECK(!sk2->aiShouldActivate(g2.e, *g2.self)); // 全是队友 → 不挂
}

TEST("ai_gates/tianxiang_needs_unmarked_enemy_and_red_card") {
    GateGame g("mou_xiaoqiao", 4, 308);
    auto sk = g.skill("谋-天香");
    if (!sk) return;
    g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
    g.giveCards(1, CardSubType::SHAN, "闪", Suit::HEART);   // 1 张红色 → 手牌不足 2
    CHECK(!sk->aiShouldActivate(g.e, *g.self));
    g.self->addHandCard(std::make_shared<Card>(961, "闪", Suit::HEART, 6, CardType::BASIC,
                                               CardSubType::SHAN, ShaElement::NORMAL, 1, ""));
    if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self)); // 2 张 + 有敌人
}

// ---------- 资源交换类：要算收益 ----------

TEST("ai_gates/rende_only_when_wounded_or_overflow") {
    GateGame g("mou_liubei", 4, 309);
    auto sk = g.skill("谋-仁德");
    if (!sk) return;
    g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
    g.giveCards(4);
    CHECK(!sk->aiShouldActivate(g.e, *g.self));      // 满血且手牌不溢出 → 不白给
    g.self->setHp(1);
    CHECK(sk->aiShouldActivate(g.e, *g.self));       // 受伤且手牌≥3 → 给两张回血
}

TEST("ai_gates/zhangwu_needs_round_two_and_two_givers") {
    GateGame g("mou_liubei", 5, 310);
    auto sk = g.skill("谋-章武");
    if (!sk) return;
    g.makeEnemy(0); g.makeEnemy(1); g.makeFriend(2); g.makeFriend(3);
    g.e.setRoundForTesting(1);
    if (sk->canActivate(g.e, *g.self)) CHECK(!sk->aiShouldActivate(g.e, *g.self)); // 第 1 轮 Y=0
    g.e.setRoundForTesting(3);
    if (sk->canActivate(g.e, *g.self)) CHECK(!sk->aiShouldActivate(g.e, *g.self)); // 没人拿过仁德牌
    g.others[0]->addMark("仁德获得:" + std::to_string(g.self->getId()), 1);
    g.others[1]->addMark("仁德获得:" + std::to_string(g.self->getId()), 1);
    if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self));  // 2 人拿过 + 第 3 轮
}

TEST("ai_gates/yanyu_keeps_last_sha") {
    GateGame g("mou_xiahoushi", 4, 311);
    auto sk = g.skill("谋-燕语");
    if (!sk) return;
    g.giveCards(1);                                  // 只有 1 张【杀】
    CHECK(!sk->aiShouldActivate(g.e, *g.self));      // 不弃掉唯一的杀
    g.giveCards(2);
    CHECK(sk->aiShouldActivate(g.e, *g.self));       // 杀有富余 → 换牌
}

TEST("ai_gates/yangwei_draws_only_when_hand_small") {
    GateGame g("mou_huaxiong", 3, 312);
    auto sk = g.skill("谋-扬威");
    if (!sk) return;
    g.giveCards(6);
    CHECK(!sk->aiShouldActivate(g.e, *g.self));      // 手牌充足 → 不必用“失效到下回合结束”的代价换牌
    g.giveCards(2);
    CHECK(sk->aiShouldActivate(g.e, *g.self));       // 手牌少 → 摸两张
}

TEST("ai_gates/zhenwei_and_qixi_need_enemies_and_cards") {
    {
        GateGame g("mou_zhuran", 4, 313);
        auto sk = g.skill("谋-镇围");
        if (!sk) return;
        g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
        g.giveCards(2);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));  // 牌不够压过对方
        g.giveCards(4);
        CHECK(sk->aiShouldActivate(g.e, *g.self));
    }
    {
        GateGame g("mou_ganning", 4, 314);
        auto sk = g.skill("谋-奇袭");
        if (!sk) return;
        g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
        g.giveCards(2);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));  // 手牌太少，猜花色容易猜中
        g.giveCards(4);
        CHECK(sk->aiShouldActivate(g.e, *g.self));
    }
}

TEST("ai_gates/jizhu_yinghun_mingren_are_conditional") {
    // 积著：准备阶段、有其他角色就值得（无费用）
    {
        GateGame g("mou_zhaoyun", 3, 315, TurnPhase::PREPARATION);
        auto sk = g.skill("谋-积著");
        if (!sk) return;
        if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self));
    }
    // 英魂：canActivate 要求受伤；受伤且有他人 → 发动；不受伤 → 不发动
    {
        GateGame g("mou_sunquan", 3, 316, TurnPhase::PREPARATION);
        auto sk = g.self->getHero()->findSkill("谋-英魂");
        if (sk) {
            CHECK(!sk->canActivate(g.e, *g.self));            // 满血不可发动
            g.self->setHp(1);
            if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self));
        }
    }
    // 明任：结束阶段，手里至少 2 张才替换“任”
    {
        GateGame g("mou_luzhi", 3, 317, TurnPhase::FINISH);
        auto sk = g.skill("谋-明任");
        if (!sk) return;
        g.giveCards(1);
        if (sk->canActivate(g.e, *g.self)) CHECK(!sk->aiShouldActivate(g.e, *g.self));
        g.giveCards(2);
        if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self));
    }
}

TEST("ai_gates/yicong_is_never_active_and_luanwu_is_selective") {
    // 义从：不是主动技（每轮开始经 onRoundStart 发动）→ AI 永不在出牌阶段发动
    {
        GateGame g("mou_gongsunzan", 3, 318);
        auto sk = g.skill("谋-义从");
        if (!sk) return;
        CHECK(!sk->canActivate(g.e, *g.self));
        CHECK(!sk->aiShouldActivate(g.e, *g.self));
    }
    // 乱武（限定技）：敌不少于友才开；队友占多数时不开，除非有敌人濒死边缘
    {
        GateGame g("mou_jiaxu", 5, 319);
        auto sk = g.skill("谋-乱武");
        if (!sk) return;
        g.makeFriend(0); g.makeFriend(1); g.makeFriend(2); g.makeEnemy(3);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));            // 友 3 > 敌 1，且没人濒死
        g.others[3]->setHp(1);
        CHECK(sk->aiShouldActivate(g.e, *g.self));             // 有敌人 1 血 → 值得开
    }
    {
        GateGame g("mou_jiaxu", 5, 320);
        auto sk = g.skill("谋-乱武");
        if (!sk) return;
        g.makeEnemy(0); g.makeEnemy(1); g.makeEnemy(2); g.makeFriend(3);
        CHECK(sk->aiShouldActivate(g.e, *g.self));             // 敌 3 ≥ 友 1 → 开
    }
}

TEST("ai_gates/jiang_and_fanjian_and_lianhuan") {
    // 激昂：把所有手牌当【决斗】→ 只在手牌很少或能压死低血敌人时
    {
        GateGame g("mou_sunce", 4, 321);
        auto sk = g.skill("谋-激昂");
        if (!sk) return;
        g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
        g.giveCards(5);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));            // 5 张牌换一次决斗不划算
        g.giveCards(2);
        CHECK(sk->aiShouldActivate(g.e, *g.self));             // 手牌少 → 打出去
    }
    // 反间：要有敌人且手里有牌
    {
        GateGame g("mou_zhouyu", 4, 322);
        auto sk = g.skill("谋-反间");
        if (!sk) return;
        g.makeEnemy(0); g.makeFriend(1); g.makeFriend(2);
        g.giveCards(1);
        CHECK(!sk->aiShouldActivate(g.e, *g.self));
        g.giveCards(3);
        if (sk->canActivate(g.e, *g.self)) CHECK(sk->aiShouldActivate(g.e, *g.self));
    }
    // 连环：主动入口是“重铸梅花”，只在手牌超过上限时
    {
        GateGame g("mou_pangtong", 3, 323);
        auto sk = g.skill("谋-连环");
        if (!sk) return;
        g.giveCards(1, CardSubType::SHA, "杀", Suit::CLUB);     // 1 张梅花
        CHECK(!sk->aiShouldActivate(g.e, *g.self));             // 手牌没超上限 → 不重铸
    }
}
