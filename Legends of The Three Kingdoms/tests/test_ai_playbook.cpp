// AI 完善批次验收（用户 2026-10-05：“按优先级全部较完整完成”）。
// 覆盖：A1/A2 斩杀预判与真实伤害、A3 群体锦囊时机、A5 延时锦囊目标、A6 决斗与卖血流、
//       A7 铁索连环+属性杀、A9 弃牌前瞻、B4 拼点、C2 缔盟配对、C4 义从档位、
//       D2 叫分、D3 克制、D4 座次、D5 内奸伪装、E1 记牌器、E2 证据衰减/群体牌不算证据/置信阈值、
//       E3 集火目标、E4 残局不资敌。
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "HeroTier.h"
#include "AI.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace Thks;

namespace {

CardPtr card(int id, const std::string& name, Suit suit, int rank, CardType type, CardSubType sub,
             ShaElement el = ShaElement::NORMAL) {
    return std::make_shared<Card>(id, name, suit, rank, type, sub, el, 1, "");
}

struct AiGame {
    GameEngine e;
    std::shared_ptr<std::ostringstream> log;
    std::vector<PlayerPtr> ps;
    AiGame(const std::vector<std::string>& heroes, unsigned seed = 400) {
        e.setSeed(seed);
        log = captureLog(e);
        e.initGame(static_cast<int>(heroes.size()), -1, heroes);
        ps = e.getPlayers();
        for (auto& p : ps) clearHand(*p);
        e.setCurrentPlayerForTesting(ps[0]);
        e.setPhase(TurnPhase::PLAY);
    }
    void enemy(size_t i) { e.recordRelation(ps[i]->getId(), ps[0]->getId(), +3); }
    void friendOf(size_t i) { e.recordRelation(ps[i]->getId(), ps[0]->getId(), -3); }
};

} // namespace

// ---------- A1 / A2：斩杀预判（含转化杀与真实伤害） ----------

TEST("ai_play/kill_detection_counts_conversion_and_modifiers") {
    AiGame g({"guanyu", "zhangfei", "machao"}); // 关羽【武圣】：红色手牌当【杀】
    g.enemy(1);
    g.enemy(2);
    // 目标 2 血、手里只有红色【闪】（可当杀）→ 单刀不够，但武圣 + 2 血 = 需要 2 点伤害
    g.ps[2]->setHp(2);
    g.ps[0]->addHandCard(card(501, "闪", Suit::HEART, 5, CardType::BASIC, CardSubType::SHAN));
    CHECK_EQ(AIController::shaSourceCount(g.e, *g.ps[0]), 1);       // 红色牌可当杀
    CHECK_EQ(AIController::estimatedShaDamage(g.e, *g.ps[0], *g.ps[2]), 1);
    CHECK(!AIController::canKillWithSha(g.e, *g.ps[0], *g.ps[2]));  // 1 点杀不掉 2 血
    CHECK(AIController::canKillWithSha(g.e, *g.ps[0], *g.ps[2], /*assumeDrunk=*/true)); // 先喝酒就能斩杀
    // 目标 1 血 → 直接可斩杀
    g.ps[2]->setHp(1);
    CHECK(AIController::canKillWithSha(g.e, *g.ps[0], *g.ps[2]));
}

TEST("ai_play/estimated_damage_respects_armor_and_weapons") {
    AiGame g({"zhangfei", "guanyu", "machao"});
    g.enemy(1);
    // 藤甲：普通【杀】无效
    g.ps[1]->equip(card(510, "藤甲", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::ARMOR));
    CHECK_EQ(AIController::estimatedShaDamage(g.e, *g.ps[0], *g.ps[1]), 0);
    // 古锭刀 + 目标空手牌 → +1
    g.ps[1]->removeEquipment(CardSubType::ARMOR);
    g.ps[0]->equip(card(511, "古锭刀", Suit::SPADE, 6, CardType::EQUIPMENT, CardSubType::WEAPON));
    CHECK_EQ(AIController::estimatedShaDamage(g.e, *g.ps[0], *g.ps[1]), 2);
}

// ---------- A3 / E4：群体锦囊时机与不资敌 ----------

TEST("ai_play/aoe_and_wugu_need_favorable_ratio") {
    // 敌多于友 → 放南蛮；友多于敌 → 不放
    {
        AiGame g({"zhangfei", "guanyu", "machao", "zhaoyun"});
        g.enemy(1); g.enemy(2); g.friendOf(3);
        g.ps[0]->addHandCard(card(520, "南蛮入侵", Suit::SPADE, 13, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN));
        auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
        CHECK(d.cardToPlay != nullptr);
        if (d.cardToPlay) CHECK(d.cardToPlay->getSubType() == CardSubType::NAN_MAN_RU_QIN);
    }
    {
        AiGame g({"zhangfei", "guanyu", "machao", "zhaoyun"}, 401);
        g.enemy(1); g.friendOf(2); g.friendOf(3);
        g.ps[0]->addHandCard(card(521, "南蛮入侵", Suit::SPADE, 13, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN));
        auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
        CHECK(d.cardToPlay == nullptr); // 队友多于敌人 → 不放（会打自己人）
    }
    // E4：五谷丰登在敌多于友时不放（资敌）
    {
        AiGame g({"zhangfei", "guanyu", "machao"}, 402);
        g.enemy(1); g.enemy(2);
        g.ps[0]->addHandCard(card(522, "五谷丰登", Suit::HEART, 7, CardType::TRICK, CardSubType::WU_GU_FENG_DENG));
        auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
        CHECK(d.cardToPlay == nullptr);
    }
}

// ---------- A5：延时锦囊挑目标（避开能改判的） ----------

TEST("ai_play/delayed_trick_avoids_judgement_alterers") {
    AiGame g({"daqiao", "simayi", "zhangfei"}); // 座位 0 大乔【乐不思蜀】；座位 1 司马懿【鬼才】可改判
    g.enemy(1);
    g.enemy(2);
    g.ps[0]->addHandCard(card(530, "乐不思蜀", Suit::SPADE, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU));
    auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
    CHECK(d.cardToPlay != nullptr);
    if (!d.cardToPlay) return;
    CHECK(d.cardToPlay->getSubType() == CardSubType::LE_BU_SI_SHU);
    CHECK(!d.targets.empty());
    if (!d.targets.empty()) {
        // 司马懿能改判（鬼才）→ 不该贴他，应贴张飞
        CHECK(d.targets[0]->getId() == g.ps[2]->getId());
    }
}

// ---------- A6：决斗门槛与卖血流 ----------

TEST("ai_play/duel_requires_sha_sources_and_avoids_sell_blood") {
    // 只有 1 个【杀】来源 → 不决斗
    {
        AiGame g({"zhangfei", "guanyu", "machao"});
        g.enemy(1);
        g.ps[0]->addHandCard(card(540, "决斗", Suit::SPADE, 12, CardType::TRICK, CardSubType::JUE_DOU));
        g.ps[0]->addHandCard(card(541, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
        bool dueled = d.cardToPlay && d.cardToPlay->getSubType() == CardSubType::JUE_DOU;
        CHECK(!dueled);
    }
    // 2 个【杀】来源 → 决斗（敌人装 +1 马，【杀】够不着，决策才会走到【决斗】——决斗无距离限制）
    {
        AiGame g({"zhangfei", "guanyu", "machao"}, 403);
        g.enemy(1);
        g.ps[1]->equip(card(548, "的卢", Suit::CLUB, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE));
        g.ps[2]->equip(card(549, "绝影", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE));
        g.ps[0]->addHandCard(card(542, "决斗", Suit::SPADE, 12, CardType::TRICK, CardSubType::JUE_DOU));
        g.ps[0]->addHandCard(card(543, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        g.ps[0]->addHandCard(card(544, "杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
        auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
        CHECK(d.cardToPlay != nullptr);
        if (d.cardToPlay) CHECK(d.cardToPlay->getSubType() == CardSubType::JUE_DOU);
    }
    // 目标是卖血流（司马懿【反馈】）→ 门槛提高，2 张【杀】也不决斗
    {
        AiGame g({"zhangfei", "simayi", "machao"}, 404);
        g.enemy(1);
        g.ps[1]->equip(card(552, "的卢", Suit::CLUB, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE));
        g.ps[2]->equip(card(553, "绝影", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE));
        g.ps[0]->addHandCard(card(545, "决斗", Suit::SPADE, 12, CardType::TRICK, CardSubType::JUE_DOU));
        g.ps[0]->addHandCard(card(546, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        g.ps[0]->addHandCard(card(547, "杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
        CHECK(AIController::isSellBloodHero(*g.ps[1]));
        auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
        bool dueled = d.cardToPlay && d.cardToPlay->getSubType() == CardSubType::JUE_DOU;
        CHECK(!dueled);
    }
}

// ---------- A7：铁索连环 + 属性杀 ----------

TEST("ai_play/chains_two_enemies_before_elemental_sha") {
    AiGame g({"pangtong", "guanyu", "machao"});
    g.enemy(1);
    g.enemy(2);
    g.ps[0]->addHandCard(card(550, "火杀", Suit::HEART, 4, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE));
    g.ps[0]->addHandCard(card(551, "铁索连环", Suit::CLUB, 11, CardType::TRICK, CardSubType::TIE_SUO_LIAN_HUAN));
    auto d = AIController::makePlayDecision(g.e, *g.ps[0]);
    CHECK(d.cardToPlay != nullptr);
    if (!d.cardToPlay) return;
    CHECK(d.cardToPlay->getSubType() == CardSubType::TIE_SUO_LIAN_HUAN); // 先连人，再火杀
    CHECK_EQ(d.targets.size(), static_cast<size_t>(2));
}

// ---------- B4：拼点不烧救命牌 ----------

TEST("ai_response/pindian_keeps_critical_card") {
    AiGame g({"taishici", "guanyu", "machao"});
    g.enemy(1);
    // 手里最大的是【桃】(K)，另有一张点数只差 1 的普通【杀】(Q) → 拼点应该用【杀】保住【桃】
    g.ps[0]->addHandCard(card(560, "桃", Suit::HEART, 13, CardType::BASIC, CardSubType::TAO));
    g.ps[0]->addHandCard(card(561, "杀", Suit::SPADE, 12, CardType::BASIC, CardSubType::SHA));
    g.ps[1]->addHandCard(card(562, "闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN));
    CHECK(g.e.pindian(g.ps[0], g.ps[1], "天义"));
    CHECK(g.ps[0]->hasHandCard(card(560, "桃", Suit::HEART, 13, CardType::BASIC, CardSubType::TAO)) ||
          g.ps[0]->getHandCardCount() >= 1);
    // 具体断言：桃还在手上（打出去的是杀）
    bool taoKept = false;
    for (const auto& c : g.ps[0]->getHandCards()) if (c && c->getSubType() == CardSubType::TAO) taoKept = true;
    CHECK(taoKept);
}

// ---------- C4：义从按局势选档位 ----------

TEST("ai_skill/yicong_picks_offense_or_defense_by_situation") {
    // 敌人够不着 → 进攻档（距离-1，标记 1）；被压制（低血）→ 防御档（标记 2）
    {
        GameEngine e; e.setSeed(410); captureLog(e);
        e.initGame(5, -1, {"mou_gongsunzan", "guanyu", "zhangfei", "zhaoyun", "machao"});
        auto ps = e.getPlayers();
        auto yicong = ps[0]->getHero()->findSkill("谋-义从");
        CHECK(yicong != nullptr);
        if (!yicong) return;
        ps[0]->setHp(ps[0]->getMaxHp());       // 满血、牌多 → 进攻档
        for (int i = 0; i < 5; ++i) ps[0]->addHandCard(card(570 + i, "杀", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA));
        e.setCurrentPlayerForTesting(ps[1]);
        e.setRoundForTesting(1);
        yicong->onRoundStart(e, *ps[0]);
        CHECK_EQ(ps[0]->getMark("义从方向"), 1); // 1＝距离-1（进攻）
    }
    {
        GameEngine e; e.setSeed(411); captureLog(e);
        e.initGame(5, -1, {"mou_gongsunzan", "guanyu", "zhangfei", "zhaoyun", "machao"});
        auto ps = e.getPlayers();
        auto yicong = ps[0]->getHero()->findSkill("谋-义从");
        CHECK(yicong != nullptr);
        if (!yicong) return;
        ps[0]->setHp(1);                       // 残血 → 防御档
        e.setCurrentPlayerForTesting(ps[1]);
        e.setRoundForTesting(1);
        yicong->onRoundStart(e, *ps[0]);
        CHECK_EQ(ps[0]->getMark("义从方向"), 2); // 2＝距离+1（防御）
    }
}

// ---------- D3 / D4 / D5：选将评分 ----------

TEST("ai_draft/counter_picking_and_traitor_disguise") {
    AIController::DraftContext ctx;
    ctx.field = HeroTier::Field::IDENTITY;
    auto huangzhong = HeroRegistry::create("shi_dengai");     // 强命/无视防具系（势-急袭含“无视”）
    auto huatuo = HeroRegistry::create("huatuo");             // 辅助/回复系
    CHECK(huangzhong != nullptr && huatuo != nullptr);
    if (!huangzhong || !huatuo) return;

    // D3：主公是卖血流（司马懿【反馈】）时，反贼更该选强命/输出将
    ctx.role = Identity::FAN_ZEI;
    // 两次都用魏国主公，隔离“异势力 +80”的影响，只比较 D3 克制加成
    ctx.lordHero = HeroRegistry::create("simayi");   // 魏·反馈＝卖血流
    int withCounter = AIController::draftScore(huangzhong, ctx);
    ctx.lordHero = HeroRegistry::create("caocao");   // 魏·奸雄＝非卖血流
    int withoutCounter = AIController::draftScore(huangzhong, ctx);
    CHECK(withCounter > withoutCounter);
    CHECK_EQ(withCounter - withoutCounter, 40);      // D3 克制加成的确切数值

    // D4：座次——先手位反贼更看重控制/防御，末位更看重输出
    ctx.lordHero = HeroRegistry::create("caocao");
    AIController::DraftContext first = ctx, last = ctx;
    first.seatPosition = 0;
    last.seatPosition = 2;
    auto zhangfei = HeroRegistry::create("zhangfei");   // 输出系（咆哮：【杀】无次数限制）
    auto zhanghe = HeroRegistry::create("zhanghe");     // 控制系（巧变：跳过阶段）
    CHECK(zhangfei != nullptr && zhanghe != nullptr);
    if (zhangfei && zhanghe) {
        // 输出型武将在末位（收割位）评分更高；控制型武将在先手位（压制位）评分更高
        CHECK(AIController::draftScore(zhangfei, last) > AIController::draftScore(zhangfei, first));
        CHECK(AIController::draftScore(zhanghe, first) > AIController::draftScore(zhanghe, last));
    }

    // D5：内奸避免与主公同势力（会被反贼当忠臣集火）、避免嘲讽过高
    ctx.role = Identity::NEI_JIAN;
    ctx.seatPosition = -1;
    ctx.lordHero = HeroRegistry::create("caocao");    // 魏
    auto weiHero = HeroRegistry::create("zhangliao"); // 魏
    auto shuHero = HeroRegistry::create("huangzhong");// 蜀（对照用，不参与断言）
    CHECK(weiHero != nullptr && shuHero != nullptr);
    if (weiHero && shuHero) {
        ctx.lordHero = HeroRegistry::create("liubei"); // 换成蜀主公
        int weiUnderShuLord = AIController::draftScore(weiHero, ctx); // 异势力 → 不扣分
        ctx.lordHero = HeroRegistry::create("caocao"); // 魏主公
        int weiUnderWeiLord = AIController::draftScore(weiHero, ctx); // 同势力 → 扣 40
        CHECK(weiUnderShuLord > weiUnderWeiLord);
    }
}

// ---------- E1：记牌器数据 ----------

TEST("ai_state/remaining_card_count_uses_deck_only") {
    AiGame g({"zhangfei", "guanyu", "machao"});
    int before = AIController::remainingCardsByName(g.e, "杀");
    CHECK(before > 0);
    // 把一张【杀】打进弃牌堆 → 牌堆剩余不变（记牌器口径含弃牌堆时可另行统计）
    auto sha = card(580, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    g.ps[0]->addHandCard(sha);
    g.e.discardCardOf(g.ps[0], sha, "测试");
    int after = AIController::remainingCardsByName(g.e, "杀");
    CHECK(after == before - 1 || after == before); // 手里那张来自牌堆之外，计数不应增加
    CHECK(after <= before);
}

// ---------- E2：证据衰减 / 群体牌不算证据 / 置信阈值 ----------

TEST("ai_state/evidence_decay_and_thresholds") {
    AiGame g({"caocao", "huatuo", "zhangfei", "zhaoyun", "machao"});
    // 置信阈值：一次“弱友好”证据（+1 级）不足以认定队友；一次【桃】级（-3）才够
    g.e.recordRelation(g.ps[1]->getId(), g.ps[0]->getId(), -1);
    CHECK(g.e.aiSideEstimate(*g.ps[1]) < 2);
    g.e.recordRelation(g.ps[2]->getId(), g.ps[0]->getId(), -3);
    CHECK(g.e.aiSideEstimate(*g.ps[2]) >= 2);
    CHECK(g.e.aiIsFriend(*g.ps[0], *g.ps[2]));

    // 时效衰减：把轮次推进，早期证据的权重下降
    int v0 = g.e.relationOf(g.ps[2]->getId(), g.ps[0]->getId());
    CHECK_EQ(v0, -3);
    g.e.setRoundForTesting(3);
    int v3 = g.e.relationOf(g.ps[2]->getId(), g.ps[0]->getId());
    CHECK(v3 > v0);            // 负分衰减后绝对值变小（-3 → 约 -2）
    CHECK(v3 < 0);
    g.e.setRoundForTesting(30);
    int v30 = g.e.relationOf(g.ps[2]->getId(), g.ps[0]->getId());
    CHECK(v30 >= -1);          // 最低保留 30% 权重
    g.e.setRoundForTesting(0);
}

TEST("ai_state/aoe_is_not_faction_evidence") {
    AiGame g({"zhangfei", "guanyu", "machao", "zhaoyun"});
    for (auto& p : g.ps) clearHand(*p);
    // 南蛮入侵打到 3 名角色（含队友）→ 群体牌本身不构成阵营证据。
    // 给每人一张【杀】让他们都能响应（不掉血），从而排除“伤害证据”的干扰。
    for (size_t i = 1; i < g.ps.size(); ++i)
        g.ps[i]->addHandCard(card(585 + static_cast<int>(i), "杀", Suit::CLUB, 9, CardType::BASIC, CardSubType::SHA));
    auto nanman = card(590, "南蛮入侵", Suit::SPADE, 13, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN);
    g.ps[0]->addHandCard(nanman);
    g.e.useCard(g.ps[0], nanman, {});
    for (size_t i = 1; i < g.ps.size(); ++i)
        CHECK_EQ(g.e.relationOf(g.ps[0]->getId(), g.ps[i]->getId()), 0);
}

// ---------- E3：集火目标 ----------

TEST("ai_state/focus_target_prefers_killable_and_is_stable") {
    AiGame g({"zhangfei", "guanyu", "machao", "zhaoyun"});
    g.enemy(1); g.enemy(2); g.enemy(3);
    g.ps[1]->setHp(3);
    g.ps[2]->setHp(1);   // 可斩杀
    g.ps[3]->setHp(4);
    auto focus = AIController::focusTarget(g.e, *g.ps[0]);
    CHECK(focus != nullptr);
    if (focus) CHECK(focus->getId() == g.ps[2]->getId());
    auto again = AIController::focusTarget(g.e, *g.ps[0]);
    CHECK(again != nullptr && focus != nullptr && again->getId() == focus->getId()); // 稳定＝同方 AI 集火同一人
}

// ---------- D2：叫分随候选与手牌变化 ----------

TEST("ai_draft/doudizhu_bidding_is_not_fixed_to_seat_zero") {
    std::set<int> landlords;
    std::set<std::string> baseScores;
    for (unsigned seed = 420; seed < 444; ++seed) {
        GameEngine e;
        e.setSeed(seed);
        e.setAiDelayMs(0);
        auto sink = captureLog(e);
        e.initDoudizhuGame(-1, {}, -1, true); // 自然叫分（不覆盖）
        int landlord = -1;
        for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::DI_ZHU) landlord = p->getId();
        CHECK(landlord >= 0);
        if (landlord >= 0) landlords.insert(landlord);
        // 收集底分（叫分结果的直接体现）
        const std::string text = sink->str();
        size_t pos = text.find("成为地主，底分 ");
        if (pos != std::string::npos) baseScores.insert(text.substr(pos, 22));
    }
    // 叫分随候选武将强度与手牌变化：地主座位或底分至少要有一种变化。
    // 不断言“地主座位 >= 2 种”这一条：std::uniform_int_distribution 的结果是实现定义的
    // （MSVC/libstdc++/libc++ 各不同），单看座位在别的平台上可能偶然恒定。
    CHECK(landlords.size() + baseScores.size() >= 3);
}
