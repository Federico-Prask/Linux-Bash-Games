#include "test_helpers.h"
#include "AI.h"

TEST("ai/friend_relations") {
    Player lord(0, "主", Identity::ZHU_GONG, true, blankHero());
    Player loyal(1, "忠", Identity::ZHONG_CHEN, true, blankHero());
    Player rebel(2, "反", Identity::FAN_ZEI, true, blankHero());
    Player traitor(3, "内", Identity::NEI_JIAN, true, blankHero());

    CHECK(AIController::isFriend(lord, loyal));
    CHECK(AIController::isFriend(loyal, lord));
    CHECK(AIController::isFriend(lord, lord));
    CHECK(AIController::isFriend(rebel, rebel));
    CHECK(!AIController::isFriend(lord, rebel));
    CHECK(!AIController::isFriend(rebel, lord));
    // 内奸与任何人（除自己）都不是队友
    CHECK(!AIController::isFriend(traitor, lord));
    CHECK(!AIController::isFriend(traitor, rebel));
    CHECK(!AIController::isFriend(lord, traitor));
}

TEST("ai/card_value_ordering") {
    auto tao = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto shan = makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN);
    CHECK(AIController::cardValue(tao) > AIController::cardValue(shan));
    CHECK(AIController::cardValue(shan) > AIController::cardValue(sha));
    CHECK(AIController::cardValue(nullptr) == 0);
}

TEST("ai/discard_least_valuable_first") {
    GameEngine engine; // 空引擎：无局面信息时退化为基础价值
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero());
    auto tao = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto jiu = makeCard("酒", Suit::CLUB, 3, CardType::BASIC, CardSubType::JIU);
    p.addHandCards({tao, sha, jiu});
    auto toDiscard = AIController::chooseCardsToDiscard(engine, p, 2);
    CHECK_EQ(toDiscard.size(), static_cast<size_t>(2));
    // 酒(3) 与 杀(4) 价值低于桃(10)，先弃
    CHECK(toDiscard[0] == jiu);
    CHECK(toDiscard[1] == sha);
}

TEST("ai/sha_target_prefers_lowest_hp_enemy") {
    Player self(0, "self", Identity::ZHU_GONG, true, blankHero());
    auto e1 = std::make_shared<Player>(1, "e1", Identity::FAN_ZEI, true, blankHero());
    auto e2 = std::make_shared<Player>(2, "e2", Identity::FAN_ZEI, true, blankHero());
    auto friendP = std::make_shared<Player>(3, "f", Identity::ZHONG_CHEN, true, blankHero());
    e1->changeHp(-2); // e1 HP 2
    e2->changeHp(-1); // e2 HP 3
    std::vector<std::shared_ptr<Player>> candidates = {friendP, e2, e1};
    GameEngine engine; // AI 目标选择不依赖引擎状态
    auto target = AIController::chooseShaTarget(engine, self, candidates);
    CHECK(target == e1); // 优先体力最少的敌人，且跳过队友
}

TEST("ai/pick_zhang_ba_keeps_defense_cards") {
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero());
    auto tao = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);
    auto shan = makeCard("闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN);
    auto jiu = makeCard("酒", Suit::CLUB, 3, CardType::BASIC, CardSubType::JIU);
    auto liang = makeCard("兵粮寸断", Suit::CLUB, 4, CardType::TRICK, CardSubType::BING_LIANG_CUN_DUAN);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    p.addHandCards({tao, shan, jiu, liang, sha});
    auto picks = AIController::pickZhangBaCards(p);
    // 桃(10)/闪(5) 价值高于阈值不参与；【杀】本身不用转化；只挑出酒(3)与兵粮寸断(4)
    CHECK_EQ(picks.size(), static_cast<size_t>(2));
    for (auto& c : picks) {
        CHECK(c == jiu || c == liang);
    }
}

TEST("ai/strong_tricks_keep_high_value") {
    // 强力锦囊不应被当成废牌（丈八不拆、制衡不弃、弃牌阶段后弃）
    auto chai = makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO);
    auto shun = makeCard("顺手牵羊", Suit::DIAMOND, 3, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG);
    auto wuzhong = makeCard("无中生有", Suit::HEART, 7, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);
    auto duel = makeCard("决斗", Suit::CLUB, 1, CardType::TRICK, CardSubType::JUE_DOU);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    CHECK(AIController::cardValue(chai) > AIController::cardValue(sha));
    CHECK(AIController::cardValue(shun) > AIController::cardValue(sha));
    CHECK(AIController::cardValue(wuzhong) > AIController::cardValue(sha));
    CHECK(AIController::cardValue(duel) > AIController::cardValue(sha));
    // 丈八蛇矛凑不出两张低价值牌时宁可不用，也不拆强力锦囊
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero());
    auto jiu = makeCard("酒", Suit::CLUB, 3, CardType::BASIC, CardSubType::JIU);
    p.addHandCards({jiu, chai});
    CHECK(AIController::pickZhangBaCards(p).empty());
}

namespace {
struct ValueGame {
    GameEngine engine;
    explicit ValueGame(int n, const std::vector<std::string>& heroIds, unsigned seed = 11) {
        engine.setSeed(seed);
        engine.setLogger(std::make_shared<Logger>(std::make_shared<std::ostringstream>()));
        engine.initGame(n, -1, heroIds);
    }
};
} // namespace

TEST("ai/equipment_base_values_differ") {
    auto weapon = [](const std::string& n) {
        return makeCard(n, Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::WEAPON);
    };
    auto armor = [](const std::string& n) {
        return makeCard(n, Suit::CLUB, 2, CardType::EQUIPMENT, CardSubType::ARMOR);
    };
    // 武器：连弩6 > 青龙/贯石/麒麟5 > 其余4
    CHECK_EQ(AIController::equipmentBaseValue(weapon("诸葛连弩")), 6);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("青龙偃月刀")), 5);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("贯石斧")), 5);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("麒麟弓")), 5);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("丈八蛇矛")), 4);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("方天画戟")), 4);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("青釭剑")), 4);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("寒冰剑")), 4);
    CHECK_EQ(AIController::equipmentBaseValue(weapon("未知武器")), 4);
    // 防具：八卦/仁王5 > 白银/藤甲4
    CHECK_EQ(AIController::equipmentBaseValue(armor("八卦阵")), 5);
    CHECK_EQ(AIController::equipmentBaseValue(armor("仁王盾")), 5);
    CHECK_EQ(AIController::equipmentBaseValue(armor("白银狮子")), 4);
    CHECK_EQ(AIController::equipmentBaseValue(armor("藤甲")), 4);
    // 坐骑：同功能等价，一律3
    auto horsePlus = makeCard("绝影 (+1马)", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE);
    auto horseMinus = makeCard("赤兔 (-1马)", Suit::HEART, 5, CardType::EQUIPMENT, CardSubType::OFFENSIVE_HORSE);
    CHECK_EQ(AIController::equipmentBaseValue(horsePlus), 3);
    CHECK_EQ(AIController::equipmentBaseValue(horseMinus), 3);
    // 基础 cardValue 走同一套定价
    CHECK_EQ(AIController::cardValue(weapon("诸葛连弩")), 6);
    CHECK_EQ(AIController::cardValue(armor("八卦阵")), 5);
}

TEST("ai/equipment_duplicate_slot_reduced") {
    ValueGame g(3, {"guanyu", "zhangfei", "machao"});
    auto& ps = g.engine.getPlayers();
    auto weapon = [](const std::string& n) {
        return makeCard(n, Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::WEAPON);
    };
    clearHand(*ps[0]);
    // 装备区无牌、手牌两把武器：只有优先级高的保留原始价值
    auto hanbing = weapon("寒冰剑");
    auto lianu = weapon("诸葛连弩");
    ps[0]->addHandCard(hanbing);
    ps[0]->addHandCard(lianu);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], lianu), 6);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], hanbing), 2);
    // 同值：先占据的手牌保留，后者判2
    clearHand(*ps[0]);
    auto jueying = makeCard("绝影 (+1马)", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE);
    auto zhuahuang = makeCard("爪黄飞电 (+1马)", Suit::HEART, 13, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE);
    ps[0]->addHandCard(jueying);
    ps[0]->addHandCard(zhuahuang);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], jueying), 3);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], zhuahuang), 2);
    // 不同槽位互不影响：+1/-1 各保留
    auto chitu = makeCard("赤兔 (-1马)", Suit::HEART, 5, CardType::EQUIPMENT, CardSubType::OFFENSIVE_HORSE);
    ps[0]->addHandCard(chitu);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], chitu), 3);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], jueying), 3);
    // 已装备：同槽手牌同值或更低 → 2；严格更优 → 保留（可升级替换）
    clearHand(*ps[0]);
    ps[0]->equip(weapon("丈八蛇矛"));
    auto hanbing2 = weapon("寒冰剑");
    auto lianu2 = weapon("诸葛连弩");
    auto zhangba2 = weapon("丈八蛇矛");
    ps[0]->addHandCard(hanbing2);
    ps[0]->addHandCard(lianu2);
    ps[0]->addHandCard(zhangba2);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], hanbing2), 2); // 4，不如已装备的丈八+手牌连弩
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], lianu2), 6);   // 6，全场最高，可替换升级
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], zhangba2), 2); // 同值，已装备者先占据
    // 坐骑已装备：同功能手牌直接判2
    ps[0]->equip(jueying);
    ps[0]->addHandCard(zhuahuang);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], zhuahuang), 2);
    // 未持有的亮出牌（如五谷）：按"后占据"判定——手牌已有连弩，重复判2
    auto lianu3 = weapon("诸葛连弩");
    auto hanbing3 = weapon("寒冰剑");
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], lianu3), 2);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], hanbing3), 2);
    // 只剩已装备的丈八(4)时，亮出的连弩可升级替换 → 保留6
    clearHand(*ps[0]);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], lianu3), 6);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], hanbing3), 2);
}

TEST("ai/contextual_jiedao_needs_enemy_weapon") {
    // 3人局：0号位主公，1/2号位都是敌人
    ValueGame g(3, {"guanyu", "zhangfei", "machao"});
    auto& ps = g.engine.getPlayers();
    auto jiedao = makeCard("借刀杀人", Suit::CLUB, 13, CardType::TRICK, CardSubType::JIE_DAO_SHA_REN);
    ps[0]->addHandCard(jiedao);
    // 场上无人有武器 → 无处可借，只值 2
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], jiedao), 2);
    // 敌方装上武器（攻击范围覆盖另一名敌人）→ 值 5
    ps[1]->equip(makeCard("青龙偃月刀", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::WEAPON,
                           ShaElement::NORMAL, 3));
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], jiedao), 5);
    // 武器被卸掉 → 跌回 2
    ps[1]->removeEquipment(CardSubType::WEAPON);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], jiedao), 2);
}

TEST("ai/contextual_tricks_need_valid_targets") {
    ValueGame g(3, {"guanyu", "zhangfei", "machao"});
    auto& ps = g.engine.getPlayers();
    // 顺手牵羊：距离1内有带牌敌人 → 7；无人可顺 → 3
    auto shun = makeCard("顺手牵羊", Suit::DIAMOND, 3, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG);
    ps[0]->addHandCard(shun);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], shun), 7);
    clearHand(*ps[1]);
    clearHand(*ps[2]);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], shun), 3);
    // 乐不思蜀：有敌人可贴 → 5；所有敌人已有乐 → 2
    auto le = makeCard("乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], le), 5);
    ps[1]->addJudgeCard(makeCard("乐", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU));
    ps[2]->addJudgeCard(makeCard("乐", Suit::CLUB, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU));
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], le), 2);
    // 决斗：手握2张杀 → 6；杀不够 → 4
    clearHand(*ps[0]);
    auto duel = makeCard("决斗", Suit::CLUB, 1, CardType::TRICK, CardSubType::JUE_DOU);
    ps[0]->addHandCard(duel);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], duel), 4);
    ps[0]->addHandCard(makeCard("杀1", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("杀2", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA));
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], duel), 6);
    // 南蛮入侵：敌人多于队友 → 6
    auto nanman = makeCard("南蛮入侵", Suit::SPADE, 7, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], nanman), 6);
    // 闪电：敌人占优且自己判定区无闪电 → 4；已有闪电 → 1
    auto lightning = makeCard("闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN);
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], lightning), 4);
    ps[0]->addJudgeCard(makeCard("闪电", Suit::HEART, 9, CardType::TRICK, CardSubType::SHAN_DIAN));
    CHECK_EQ(AIController::cardValue(g.engine, *ps[0], lightning), 1);
}

// 斗地主阵营：地主单方，两名农民互为同伙
TEST("ai/friend_relations_doudizhu") {
    GameEngine e; captureLog(e);
    e.initDoudizhuGame(-1, {"zhangfei", "guanyu", "zhaoyun"});
    auto ps = e.getPlayers();
    CHECK(!AIController::isFriend(*ps[0], *ps[1])); // 地主 vs 农民
    CHECK(!AIController::isFriend(*ps[1], *ps[0]));
    CHECK(AIController::isFriend(*ps[1], *ps[2]));  // 农民互为队友
    CHECK(AIController::isFriend(*ps[2], *ps[1]));
    CHECK(AIController::isFriend(*ps[0], *ps[0]));
}

TEST("ai/pack_active_skills_gate_sensibly") {
    // 势包主动技补 AI 门控后：不该空发（牌不够/资源不足时返回 false），该发时返回 true。
    GameEngine e; captureLog(e);
    e.initGame(6, -1, {"shi_taishici", "shi_suncun", "shi_yuji", "shi_lusu", "shi_zhonghui", "shi_dengai"});
    auto ps = e.getPlayers();
    e.setPhase(TurnPhase::PLAY);

    // 势-振锋（限定技）：有酣战/战烈可改造，或受伤时发动
    auto zhenfeng = ps[0]->getHero()->findSkill("势-振锋");
    CHECK(zhenfeng != nullptr);
    CHECK(zhenfeng->aiShouldActivate(e, *ps[0]));            // 满血但拥有酣战+战烈（X 可改强）
    ps[0]->changeHp(-2);
    CHECK(zhenfeng->aiShouldActivate(e, *ps[0]));            // 受伤 → 优先回血

    // 势-逆固：单张牌不发动（弃不同花色换伤害+1 需要至少两张不同花色）
    e.setCurrentPlayerForTesting(ps[1]);
    auto nigu = ps[1]->getHero()->findSkill("势-逆固");
    clearHand(*ps[1]);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 90, CardType::BASIC, CardSubType::SHA));
    CHECK(!nigu->aiShouldActivate(e, *ps[1]));
    ps[1]->addHandCard(makeCard("闪", Suit::HEART, 91, CardType::BASIC, CardSubType::SHAN));
    CHECK(nigu->aiShouldActivate(e, *ps[1]));

    // 势-符济：没有手牌可交时不发动
    e.setCurrentPlayerForTesting(ps[2]);
    auto fuji = ps[2]->getHero()->findSkill("势-符济");
    clearHand(*ps[2]);
    CHECK(!fuji->aiShouldActivate(e, *ps[2]));
    ps[2]->addHandCard(makeCard("杀", Suit::SPADE, 92, CardType::BASIC, CardSubType::SHA));
    CHECK(fuji->aiShouldActivate(e, *ps[2]));

    // 势-肆恣：没有蓄力点时不发动（X=0 只有空结算窗口）
    e.setCurrentPlayerForTesting(ps[4]);
    auto sizi = ps[4]->getHero()->findSkill("势-肆恣");
    int siziCharge = ps[4]->getMark("蓄力");
    CHECK(siziCharge > 0); // 势·钟会开局即有蓄力点
    ps[4]->addMark("蓄力", -siziCharge);
    CHECK(!sizi->aiShouldActivate(e, *ps[4]));
    ps[4]->addMark("蓄力", 1);
    CHECK(sizi->aiShouldActivate(e, *ps[4]));

    // 势-屯田：蓄力点>0 且牌堆/弃牌堆有红桃牌才发动
    e.setCurrentPlayerForTesting(ps[5]);
    auto tuntian = ps[5]->getHero()->findSkill("势-屯田");
    CHECK(!tuntian->aiShouldActivate(e, *ps[5]));
    ps[5]->addMark("蓄力", 1);
    CHECK(tuntian->aiShouldActivate(e, *ps[5]));
}

TEST("ai/dimeng_gate_requires_profitable_pair") {
    // C2 之后：缔盟要“敌方手牌多、己方手牌少且差 ≤3”才发动（原来只要差 ≤3 就发动，
    // 结果全员空手牌时也会空发一次“交换 0 张牌”）。
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"shi_lusu", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    auto dimeng = ps[0]->getHero()->findSkill("势-缔盟");
    CHECK(dimeng != nullptr);
    if (!dimeng) return;
    for (auto& p : ps) clearHand(*p);
    CHECK(!dimeng->aiShouldActivate(e, *ps[0]));  // 全员空手牌：交换等于空发 → 不发动
    // 敌人手牌多、自己空手 → 划算（把敌人的牌换给自己）
    for (int i = 0; i < 3; i++)
        ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 100 + i, CardType::BASIC, CardSubType::SHA));
    CHECK(dimeng->aiShouldActivate(e, *ps[0]));
    // 手牌差 > 3 → 发动会失败，不该发动
    for (auto& p : ps) clearHand(*p);
    for (int i = 0; i < 10; i++)
        ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 100 + i, CardType::BASIC, CardSubType::SHA));
    for (int i = 0; i < 20; i++)
        ps[2]->addHandCard(makeCard("闪", Suit::DIAMOND, 120 + i, CardType::BASIC, CardSubType::SHAN));
    CHECK(!dimeng->aiShouldActivate(e, *ps[0])); // 0/10/20：任意两人手牌差 > 3
}

TEST("ai/chiyun_gate_needs_pending_cards") {
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"shi_zhouyu", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    auto chiyun = ps[0]->getHero()->findSkill("势-炽沄");
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 140, CardType::BASIC, CardSubType::SHA));
    CHECK(!chiyun->aiShouldActivate(e, *ps[0])); // 阶段内没有“首次获得牌”的待发动 → 不发动
}

TEST("ai/rich_enemy_prefers_more_visible_cards") {
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"guanyu", "zhangfei", "machao"});
    auto ps = e.getPlayers();
    for (auto& p : ps) clearHand(*p);
    for (int i = 0; i < 4; i++)
        ps[1]->addHandCard(makeCard("闪", Suit::DIAMOND, 150 + i, CardType::BASIC, CardSubType::SHAN));
    ps[2]->addHandCard(makeCard("闪", Suit::DIAMOND, 160, CardType::BASIC, CardSubType::SHAN));
    CHECK(AIController::chooseRichEnemy(e, *ps[0], {ps[1], ps[2]}) == ps[1]); // 4 手牌 > 1 手牌
    ps[2]->addJudgeCard(makeCard("乐", Suit::HEART, 161, CardType::TRICK, CardSubType::LE_BU_SI_SHU));
    ps[2]->addJudgeCard(makeCard("兵", Suit::CLUB, 162, CardType::TRICK, CardSubType::BING_LIANG_CUN_DUAN));
    CHECK(AIController::chooseRichEnemy(e, *ps[0], {ps[1], ps[2]}) == ps[2]); // 判定区 2 张（×2）反超
}

TEST("ai/jiu_then_sha_secures_kill") {
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"guanyu", "zhangfei", "machao"});
    auto ps = e.getPlayers();
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    for (auto& p : ps) clearHand(*p);
    auto jiu = makeCard("酒", Suit::CLUB, 31, CardType::BASIC, CardSubType::JIU);
    auto sha = makeCard("杀", Suit::SPADE, 32, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCards({jiu, sha});
    ps[1]->setHp(2); // 喝酒后一刀正好斩杀
    ps[2]->setHp(3);

    auto d1 = AIController::makePlayDecision(e, *ps[0]);
    CHECK(d1.cardToPlay == jiu); // 先喝酒
    CHECK(e.useCard(ps[0], d1.cardToPlay, std::vector<std::shared_ptr<Player>>{}));
    CHECK(ps[0]->isDrunk());

    auto d2 = AIController::makePlayDecision(e, *ps[0]);
    CHECK(d2.cardToPlay == sha);
    CHECK(!d2.targets.empty());
    CHECK(d2.targets[0] == ps[1]); // 优先可击杀目标
    CHECK(e.useCard(ps[0], d2.cardToPlay, d2.targets));
    CHECK(ps[1]->getHp() <= 0); // 【酒】+1 → 2 点伤害
}

TEST("ai/jiu_not_wasted_without_lethal") {
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"guanyu", "zhangfei", "machao"});
    auto ps = e.getPlayers();
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    for (auto& p : ps) clearHand(*p);
    ps[0]->addHandCard(makeCard("酒", Suit::CLUB, 33, CardType::BASIC, CardSubType::JIU));
    auto sha = makeCard("杀", Suit::SPADE, 34, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(sha);
    ps[1]->setHp(3);
    ps[2]->setHp(3);
    auto d = AIController::makePlayDecision(e, *ps[0]);
    CHECK(d.cardToPlay == sha); // 无法斩杀 → 不浪费【酒】，直接出【杀】
}

TEST("ai/sha_target_prefers_killable_over_card_rich") {
    Player self(0, "self", Identity::ZHU_GONG, true, blankHero());
    auto tanky = std::make_shared<Player>(1, "t", Identity::FAN_ZEI, true, blankHero());
    auto killable = std::make_shared<Player>(2, "k", Identity::FAN_ZEI, true, blankHero());
    tanky->changeHp(-2);    // HP 2，手牌多（威胁大）
    killable->changeHp(-3); // HP 1，手牌少
    for (int i = 0; i < 8; i++)
        tanky->addHandCard(makeCard("杀", Suit::SPADE, 40 + i, CardType::BASIC, CardSubType::SHA));
    killable->addHandCard(makeCard("杀", Suit::SPADE, 60, CardType::BASIC, CardSubType::SHA));
    GameEngine engine; // 目标选择不依赖引擎状态
    CHECK(AIController::chooseShaTarget(engine, self, {tanky, killable}, 1) == killable); // 斩杀优先
    CHECK(AIController::chooseShaTarget(engine, self, {tanky, killable}, 2) == tanky);    // 都可斩杀时先打威胁大的
}

TEST("ai/dying_ai_uses_jiu_to_self_save") {
    // 官方【酒】第二句：处于濒死状态时，对自己使用，回复1点体力。AI 会用【酒】自救。
    GameEngine e; captureLog(e);
    e.initGame(4, -1, {"guanyu", "zhangfei", "machao", "zhaoyun"});
    auto ps = e.getPlayers();
    for (auto& p : ps) clearHand(*p);
    ps[1]->addHandCard(makeCard("酒", Suit::CLUB, 31, CardType::BASIC, CardSubType::JIU));
    const int hpBefore = ps[1]->getHp();
    e.applyDamage(ps[0], ps[1], hpBefore); // 打到 0 点 → 濒死
    CHECK(ps[1]->isAlive());
    CHECK_EQ(ps[1]->getHp(), 1);           // 酒自救成功
    CHECK_EQ(ps[1]->getHandCardCount(), 0); // 酒已消耗
}

TEST("rules/jiu_cannot_save_other_dying_player") {
    // 只有濒死者本人可以用【酒】自救；他人的【酒】不能救他。
    GameEngine e; captureLog(e);
    e.initGame(4, -1, {"guanyu", "zhangfei", "machao", "zhaoyun"});
    auto ps = e.getPlayers();
    for (auto& p : ps) clearHand(*p);
    ps[2]->addHandCard(makeCard("酒", Suit::CLUB, 32, CardType::BASIC, CardSubType::JIU));
    const int hpBefore = ps[1]->getHp();
    e.applyDamage(ps[0], ps[1], hpBefore); // ps[1] 濒死且自己无【酒】
    CHECK(!ps[1]->isAlive());
    CHECK_EQ(ps[2]->getHandCardCount(), 1); // 他人的酒未被消耗
}
