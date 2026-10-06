// 转化类技能统一入口（2026-10-06 重写）：
//   ① 引擎按 CardSubType 全表枚举“一张牌经某个转化技能能变成什么”，不再依赖调用方写死的目标表；
//   ② canPlayCardNow 只保留“此刻适合使用”的转化（无合法目标/不能主动使用的牌被滤掉）；
//   ③ 出牌阶段直接发动转化技能（s+N）时，由技能筛出候选牌，玩家选定后才使用——不选技能就不会发生转化。
#include "test_helpers.h"
#include "HeroRegistry.h"
#include "Interaction.h"
#include <iostream>
#include <map>
#include <sstream>

using namespace Thks;

namespace {

// 一张可用的实体牌
CardPtr material(const std::string& name, Suit suit, int rank, CardType type, CardSubType sub) {
    return makeCard(name, suit, rank, type, sub);
}

// 把某个座位的回合设为进行中（转化技能里“回合内/回合外”的分支需要）
void makeTurn(GameEngine& engine, int seat) {
    engine.setCurrentPlayerForTesting(engine.getPlayers()[seat]);
    engine.setPhase(TurnPhase::PLAY);
}

} // namespace

// 1. 每个登记在册的转化类技能都必须至少能转化出一张牌（不存在“死技能”）。
TEST("conversion/every_registered_conversion_skill_yields_at_least_one_target") {
    std::vector<CardPtr> pool = {
        material("红桃K", Suit::HEART, 13, CardType::BASIC, CardSubType::SHA),
        material("方块K", Suit::DIAMOND, 13, CardType::BASIC, CardSubType::SHA),
        material("黑桃K", Suit::SPADE, 13, CardType::BASIC, CardSubType::SHA),
        material("梅花K", Suit::CLUB, 13, CardType::BASIC, CardSubType::SHA),
        material("红桃闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN),
        material("黑桃闪", Suit::SPADE, 2, CardType::BASIC, CardSubType::SHAN),
        material("红桃过拆", Suit::HEART, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO),
        material("黑桃决斗", Suit::SPADE, 1, CardType::TRICK, CardSubType::JUE_DOU),
    };
    // 少数转化依赖“发动后的状态/由主动技组织全部手牌”，无法用单张素材枚举，逐一列明原因。
    const std::map<std::string, std::string> stateDependent = {
        {"双雄", "需先由摸牌阶段判定获得本次可转化的花色状态（bonus）"},
        {"谋-激昂", "官网原文为“将所有手牌当【决斗】使用”，由主动技一次性组织，无单牌转化入口"},
        {"谋-观星", "“星”牌入口尚未实现（见 docs/implementation_audit_2026-10-06.md 第十三节）"},
    };
    int checked = 0;
    for (const auto& info : HeroRegistry::all()) {
        auto hero = info.create();
        if (!hero) continue;
        for (const auto& skill : hero->getSkills()) {
            if (!skill || !skill->isConversionSkill()) continue;
            GameEngine engine;
            captureLog(engine);
            engine.initGame(3, -1, {info.id, "zhangfei", "guanyu"});
            auto me = engine.getPlayers()[0];
            for (const auto& p : engine.getPlayers()) clearHand(*p);
            engine.setCurrentPlayerForTesting(engine.getPlayers()[1]); // 回合外，便于急救类
            bool any = false;
            // 转化技能多数要求素材在手牌区，逐张放进手牌再试；回合外/回合内两个时机都试。
            for (int turnSeat = 1; turnSeat >= 0 && !any; --turnSeat) {
                engine.setCurrentPlayerForTesting(engine.getPlayers()[turnSeat]);
                engine.setPhase(turnSeat == 0 ? TurnPhase::PLAY : TurnPhase::DRAW);
                for (const auto& card : pool) {
                    me->addHandCard(card);
                    if (!engine.getSkillConversionsFor(skill, me, card).empty()) any = true;
                    me->removeHandCard(card);
                    if (any) break;
                }
            }
            if (!any) {
                if (stateDependent.count(skill->getName()) == 0) {
                    std::cerr << " [转化技能无候选] " << info.name << "·" << skill->getName() << std::endl;
                    CHECK(any);
                }
                continue;
            }
            ++checked;
        }
    }
    CHECK(checked >= 10); // 124 将中共 10+ 个转化类技能
}

// 2. 旧出牌阶段写死的 16 种目标类型里没有【闪】/【无懈可击】，重写后必须能枚举出来。
TEST("conversion/enumerates_subtypes_outside_the_legacy_hardcoded_list") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 0, {"zhenji", "zhangfei", "guanyu"});
    auto ps = engine.getPlayers();
    for (const auto& p : ps) clearHand(*p);
    auto black = material("黑桃7", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(black);
    makeTurn(engine, 0);

    auto conv = engine.getAllConversionsFor(ps[0], black);
    bool hasShan = false;
    for (const auto& v : conv)
        if (v->getSubType() == CardSubType::SHAN && v->getSkillSource() == "倾国") hasShan = true;
    CHECK(hasShan);                                  // 倾国：黑色手牌→【闪】（旧表没有闪）
    for (const auto& v : conv) {
        if (v->getSubType() == CardSubType::SHAN)
            CHECK(!engine.canPlayCardNow(*ps[0], v)); // 闪不能主动使用 → 会被筛选掉
    }
    // 卧龙诸葛亮【看破】：黑色手牌→【无懈可击】（旧表同样没有）
    auto wolong = HeroRegistry::create("wolong");
    if (wolong) {
        GameEngine e2;
        captureLog(e2);
        e2.initGame(3, -1, {"wolong", "zhangfei", "guanyu"});
        auto p2 = e2.getPlayers();
        for (const auto& p : p2) clearHand(*p);
        auto black2 = material("梅花7", Suit::CLUB, 7, CardType::BASIC, CardSubType::SHA);
        p2[0]->addHandCard(black2);
        makeTurn(e2, 1);
        bool hasWuxie = false;
        for (const auto& v : e2.getAllConversionsFor(p2[0], black2))
            if (v->getSubType() == CardSubType::WU_XIE_KE_JI) hasWuxie = true;
        CHECK(hasWuxie);
    }
}

// 3. canPlayCardNow：不能主动使用的牌 / 无合法目标的牌会被滤掉，有目标的牌保留。
TEST("conversion/canPlayCardNow_filters_by_real_usability") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 0, {"guanyu", "zhangfei", "machao"});
    auto ps = engine.getPlayers();
    for (const auto& p : ps) clearHand(*p);
    ps[0]->setHp(ps[0]->getMaxHp());
    makeTurn(engine, 0);

    auto shan = material("红桃2", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    auto wuxie = material("黑桃无懈", Suit::SPADE, 4, CardType::TRICK, CardSubType::WU_XIE_KE_JI);
    auto tao = material("红桃桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO);
    auto sha = material("黑桃杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto wuzhong = material("红桃无中", Suit::HEART, 8, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);

    CHECK(!engine.canPlayCardNow(*ps[0], shan));    // 闪：不能主动使用
    CHECK(!engine.canPlayCardNow(*ps[0], wuxie));   // 无懈：不能主动使用
    CHECK(!engine.canPlayCardNow(*ps[0], tao));     // 满血不能用桃
    ps[0]->changeHp(-1);
    CHECK(engine.canPlayCardNow(*ps[0], tao));      // 受伤后可以
    CHECK(engine.canPlayCardNow(*ps[0], sha));      // 有合法目标
    CHECK(engine.canPlayCardNow(*ps[0], wuzhong));  // 无目标锦囊照常可用
}

// 4. 出牌阶段 s+N 直接发动转化技能：技能筛出候选，选定后以转化牌结算。
TEST("conversion/skill_first_entry_uses_skill_filtered_candidate") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 0, {"guanyu", "zhangfei", "machao"});
    auto ps = engine.getPlayers();
    for (const auto& p : ps) clearHand(*p);
    makeTurn(engine, 0);
    auto shan = material("红桃2", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan); // 【闪】本身不能在出牌阶段使用，只能靠【武圣】转化为【杀】
    int hp = ps[1]->getHp();

    auto skill = ps[0]->getHero()->findSkill("武圣");
    CHECK(skill != nullptr);
    if (!skill) return;
    {
        ScriptedInput input("1 1"); // 选择第一张候选牌；再选择第一个【杀】目标（张飞）
        CHECK(engine.humanUseConversionSkill(ps[0], skill));
    }
    CHECK_EQ(ps[0]->getHandCardCount(), 0); // 素材（闪）已作为【杀】被使用
    CHECK_EQ(ps[1]->getHp(), hp - 1);
    CHECK_EQ(ps[0]->getShaCountThisTurn(), 1);
}

// 5. 技能筛不出“适合的牌”时不发动，也不消耗任何牌（不选技能就不转化）。
TEST("conversion/skill_first_entry_refuses_when_no_suitable_card") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 0, {"zhenji", "zhangfei", "guanyu"});
    auto ps = engine.getPlayers();
    for (const auto& p : ps) clearHand(*p);
    makeTurn(engine, 0);
    auto black = material("黑桃7", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(black);
    auto skill = ps[0]->getHero()->findSkill("倾国");
    CHECK(skill != nullptr);
    if (!skill) return;
    CHECK(!engine.humanUseConversionSkill(ps[0], skill)); // 倾国只能出【闪】，出牌阶段无适合时机
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    CHECK_EQ(ps[0]->getShaCountThisTurn(), 0);
}

// 6. 人类玩家使用“原牌不可用”的牌时必须经技能询问：给出输入才会转化。
TEST("conversion/human_use_requires_explicit_skill_choice") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 0, {"guanyu", "zhangfei", "machao"});
    auto ps = engine.getPlayers();
    for (const auto& p : ps) clearHand(*p);
    makeTurn(engine, 0);
    auto shan = material("红桃2", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan);
    int hp = ps[1]->getHp();
    auto before = captureLog(engine);
    {
        ScriptedInput input("1 1"); // 询问“请选择使用方式” → 选择【武圣】；再选目标
        CHECK(engine.humanUseCard(ps[0], shan));
    }
    CHECK_EQ(ps[1]->getHp(), hp - 1);
    CHECK(before->str().find("请选择使用方式") != std::string::npos);
}
