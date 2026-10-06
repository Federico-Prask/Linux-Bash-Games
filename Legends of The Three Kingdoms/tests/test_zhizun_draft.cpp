// 至尊场（移除弱将）+ 身份场选将（主公先选并亮出，别人针对/辅助）+ 玩家指定身份
// 依据：用户 2026-10-05 裁定；分档数据与来源见 docs/zhizun_field_rules.md
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "HeroTier.h"
#include "AI.h"
#include "Roles.h"
#include "Interaction.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>

using namespace Thks;

namespace {


HeroPtr hero(const std::string& id) { return HeroRegistry::create(id); }

AIController::DraftContext ctx(Identity role, HeroTier::Field field = HeroTier::Field::IDENTITY,
                              const std::string& lordId = "") {
    AIController::DraftContext c;
    c.role = role;
    c.field = field;
    if (!lordId.empty()) c.lordHero = hero(lordId);
    return c;
}

std::vector<HeroPtr> heroes(const std::vector<std::string>& ids) {
    std::vector<HeroPtr> out;
    for (const auto& id : ids) out.push_back(hero(id));
    return out;
}

// 该武将是否拥有主公技（与 GameEngine::isLordHero 同口径）
bool isLordHeroId(const std::string& id) {
    HeroPtr h = HeroRegistry::create(id);
    if (!h) return false;
    for (const auto& sk : h->getSkills()) if (sk && sk->hasTag(SkillTag::LORD)) return true;
    return false;
}

int lordSeatOf(GameEngine& e) {
    const auto& ps = e.getPlayers();
    for (size_t i = 0; i < ps.size(); ++i)
        if (ps[i]->getIdentity() == Identity::ZHU_GONG) return static_cast<int>(i);
    return -1;
}

} // namespace

// ==================== 一、分档表与移除名单 ====================

TEST("zhizun/tier_table_covers_every_registered_hero") {
    CHECK_EQ(HeroTier::tableSize(), HeroRegistry::all().size());
    for (const auto& info : HeroRegistry::all()) {
        // 每名武将都有身份场 + 斗地主双状态（地主/农民）三档记录，且都写了依据（可追溯到 docs）
        CHECK(HeroTier::rationale(info.id).size() > 0);
        CHECK(HeroTier::powerScore(info.id, HeroTier::Field::IDENTITY) >= 1);
        CHECK(HeroTier::powerScore(info.id, HeroTier::Field::DDZ_LANDLORD) >= 1);
        CHECK(HeroTier::powerScore(info.id, HeroTier::Field::DDZ_FARMER) >= 1);
    }
}

// 用户 2026-10-06：「弱将门槛可以高一点，估计每个模式都各禁 30 个以上，
// 将（种）分武将两个模式禁止池可以不一样」→ 两个模式各有独立禁止池，且都 ≥30 名。
TEST("zhizun/ban_pools_are_mode_specific_and_big_enough") {
    using HeroTier::Mode;
    auto identity = HeroTier::bannedInZhizun(Mode::IDENTITY);
    auto doudizhu = HeroTier::bannedInZhizun(Mode::DOUDIZHU);
    CHECK(identity.size() >= 30);
    CHECK(doudizhu.size() >= 30);
    CHECK(identity.size() < HeroRegistry::all().size());
    CHECK(doudizhu.size() < HeroRegistry::all().size());
    // isBannedInZhizun(id, mode) 与名单一致
    for (const auto& info : HeroRegistry::all()) {
        CHECK_EQ(HeroTier::isBannedInZhizun(info.id, Mode::IDENTITY),
                 std::find(identity.begin(), identity.end(), info.id) != identity.end());
        CHECK_EQ(HeroTier::isBannedInZhizun(info.id, Mode::DOUDIZHU),
                 std::find(doudizhu.begin(), doudizhu.end(), info.id) != doudizhu.end());
    }
    // 两个池**不一样**：身份弱但斗地主可用（标甘宁：军争 3 星、斗地主农民一级）
    CHECK(HeroTier::isBannedInZhizun("ganning", Mode::IDENTITY));
    CHECK(!HeroTier::isBannedInZhizun("ganning", Mode::DOUDIZHU));
    // 斗地主弱但身份可用（骆统：地主胜率榜倒数第四、农民无数据）→ 只在斗地主池
    CHECK(!HeroTier::isBannedInZhizun("luotong", Mode::IDENTITY));
    CHECK(HeroTier::isBannedInZhizun("luotong", Mode::DOUDIZHU));
    // 并集大于任一单池
    std::set<std::string> uni(identity.begin(), identity.end());
    uni.insert(doudizhu.begin(), doudizhu.end());
    CHECK(uni.size() > identity.size());
}

// 身份池门槛：军争 ≤4 星，或 5 星但资料有明确负面评述。
TEST("zhizun/identity_pool_threshold_is_four_stars_or_bad_five_star") {
    using HeroTier::Mode;
    // 3 星及以下（含官方强力指数一星半、资料点名白板）
    CHECK(HeroTier::isBannedInZhizun("xiahoudun", Mode::IDENTITY));  // 1 星“完全白板”
    CHECK(HeroTier::isBannedInZhizun("lvmeng", Mode::IDENTITY));     // 1 星“克己是负收益”
    CHECK(HeroTier::isBannedInZhizun("ganning", Mode::IDENTITY));    // 3 星
    // 4 星（旧门槛记“中”，新门槛进池）
    CHECK(HeroTier::isBannedInZhizun("daqiao", Mode::IDENTITY));
    CHECK(HeroTier::isBannedInZhizun("xiaoqiao", Mode::IDENTITY));
    CHECK(HeroTier::isBannedInZhizun("huatuo", Mode::IDENTITY));
    // 5 星且资料有明确负面评述（郭嘉“不入流”、于吉“一星半”、邓艾“节奏慢”、神周瑜“自爆卡车”）
    CHECK(HeroTier::isBannedInZhizun("guojia", Mode::IDENTITY));
    CHECK(HeroTier::isBannedInZhizun("yuji", Mode::IDENTITY));
    CHECK(HeroTier::isBannedInZhizun("dengai", Mode::IDENTITY));
    CHECK(HeroTier::isBannedInZhizun("shen_zhouyu", Mode::IDENTITY));
    // 6 星及以上不受身份池影响
    CHECK(!HeroTier::isBannedInZhizun("lusu", Mode::IDENTITY));
    CHECK(!HeroTier::isBannedInZhizun("caopi", Mode::IDENTITY));
    // 无星级数据的老将/新包不因身份池被移除
    CHECK(!HeroTier::isBannedInZhizun("jie_zhonghui", Mode::IDENTITY));
    CHECK(!HeroTier::isBannedInZhizun("diy_zhonghui", Mode::IDENTITY));
    CHECK(!HeroTier::isBannedInZhizun("mou_zhugeliang", Mode::IDENTITY));
}

// 斗地主池门槛：地主档弱（未入榜）**且**农民档无“明确可用”评级（含无数据）。
TEST("zhizun/doudizhu_pool_threshold_landlord_weak_and_farmer_not_clearly_usable") {
    using HeroTier::Mode;
    // 地主弱 + 农民二级/三级（明确可用）→ 保留
    CHECK(!HeroTier::isBannedInZhizun("zhugeliang", Mode::DOUDIZHU)); // 农民三级“勉强能用”
    CHECK(!HeroTier::isBannedInZhizun("zhangfei", Mode::DOUDIZHU));   // 农民二级
    CHECK(!HeroTier::isBannedInZhizun("simayi", Mode::DOUDIZHU));     // 农民二级（防兵乐）
    // 地主弱 + 农民无数据 → 进池（门槛抬高后新增）
    CHECK(HeroTier::isBannedInZhizun("luotong", Mode::DOUDIZHU));     // 地主胜率倒数第四、农民无数据
    CHECK(HeroTier::isBannedInZhizun("caocao", Mode::DOUDIZHU));      // 地主未入榜、农民无数据
    CHECK(HeroTier::isBannedInZhizun("zhoutai", Mode::DOUDIZHU));     // 军争强，但斗地主两榜无数据
    // 农民一级（明确强）→ 即使地主弱也保留
    CHECK(!HeroTier::isBannedInZhizun("ganning", Mode::DOUDIZHU));
    CHECK(!HeroTier::isBannedInZhizun("huanggai", Mode::DOUDIZHU));
    CHECK(!HeroTier::isBannedInZhizun("jie_guanyu", Mode::DOUDIZHU)); // 农民及格线
    // 无公开榜单的新包（地主档记中）不参与斗地主池
    CHECK(!HeroTier::isBannedInZhizun("mou_zhugeliang", Mode::DOUDIZHU));
    CHECK(!HeroTier::isBannedInZhizun("shi_zhonghui", Mode::DOUDIZHU));
    CHECK(!HeroTier::isBannedInZhizun("diy_zhonghui", Mode::DOUDIZHU));
    // 分档口径仍保留（双状态弱将），且是斗地主池的子集
    auto ddzWeak = HeroTier::weakDoudizhuHeroes();
    CHECK(ddzWeak.size() > 0);
    for (const auto& id : ddzWeak) {
        CHECK(HeroTier::tierOf(id, HeroTier::Field::DDZ_LANDLORD) == HeroTier::Tier::WEAK);
        CHECK(HeroTier::tierOf(id, HeroTier::Field::DDZ_FARMER) == HeroTier::Tier::WEAK);
        CHECK(HeroTier::isDoudizhuWeak(id));
        CHECK(HeroTier::isBannedInZhizun(id, Mode::DOUDIZHU));
    }
}

TEST("zhizun/pool_still_large_enough_for_8_players") {
    for (HeroTier::Mode mode : {HeroTier::Mode::IDENTITY, HeroTier::Mode::DOUDIZHU}) {
        std::set<std::string> persons;
        for (const auto& info : HeroRegistry::all()) {
            if (HeroTier::isBannedInZhizun(info.id, mode)) continue;
            persons.insert(HeroRegistry::personKey(info.id));
        }
        CHECK(persons.size() >= 30); // 8 人局需要 8 个互不相同的人物，留足余量
    }
}

// ==================== 二、至尊场在两个模式里生效 ====================

TEST("zhizun/identity_game_has_no_weak_heroes") {
    GameEngine e;
    e.setSeed(5);
    captureLog(e);
    e.setZhizunField(true);
    e.initGame(8, -1, {});
    const auto& ps = e.getPlayers();
    CHECK_EQ(ps.size(), static_cast<size_t>(8));
    for (const auto& p : ps) {
        CHECK(p->getHero() != nullptr);
        CHECK(!HeroTier::isBannedInZhizun(p->getHero()->getId(), HeroTier::Mode::IDENTITY));
    }
}

// 用户 2026-10-06：「玩家……点将可以用全将」→ 显式指定（点将/测试指定）即使在本模式禁止池内也照常建局。
TEST("zhizun/point_pick_allows_banned_heroes") {
    GameEngine e;
    e.setSeed(9);
    auto sink = captureLog(e);
    e.setZhizunField(true);
    e.initGame(4, -1, {"caocao", "ganning", "lvmeng", "luxun"}); // 四名都在身份禁止池
    const auto& ps = e.getPlayers();
    CHECK_EQ(ps[0]->getHero()->getId(), std::string("caocao"));
    CHECK_EQ(ps[1]->getHero()->getId(), std::string("ganning"));
    CHECK_EQ(ps[2]->getHero()->getId(), std::string("lvmeng"));
    CHECK_EQ(ps[3]->getHero()->getId(), std::string("luxun"));
    CHECK(sink->str().find("点将") != std::string::npos);
    CHECK(sink->str().find("禁止池") != std::string::npos);
}

TEST("zhizun/doudizhu_pool_has_no_weak_heroes") {
    GameEngine e;
    e.setSeed(3);
    captureLog(e);
    e.setZhizunField(true);
    e.initDoudizhuGame(-1, {}, 0, false); // 直接指定地主（不走叫分），武将走随机池
    for (const auto& p : e.getPlayers())
        CHECK(!HeroTier::isBannedInZhizun(p->getHero()->getId(), HeroTier::Mode::DOUDIZHU));
}

TEST("zhizun/doudizhu_full_flow_candidates_exclude_weak_heroes") {
    GameEngine e;
    e.setSeed(17);
    captureLog(e);
    e.setAiDelayMs(0);
    e.setDoudizhuAiBidForTesting(3); // 座位 0 叫 3 分成为地主
    e.setZhizunField(true);
    e.initDoudizhuGame(-1, {}, -1, true);
    const auto& cands = e.getDoudizhuCandidates();
    CHECK_EQ(cands.size(), static_cast<size_t>(3));
    for (const auto& list : cands)
        for (const auto& id : list) CHECK(!HeroTier::isBannedInZhizun(id, HeroTier::Mode::DOUDIZHU));
    for (const auto& p : e.getPlayers())
        CHECK(!HeroTier::isBannedInZhizun(p->getHero()->getId(), HeroTier::Mode::DOUDIZHU));
}

TEST("zhizun/normal_field_keeps_weak_heroes_available") {
    GameEngine e;
    e.setSeed(5);
    captureLog(e);
    CHECK(!e.isZhizunField()); // 默认普通场
    e.initGame(4, -1, {"caocao", "ganning", "lvmeng", "luxun"});
    const auto& ps = e.getPlayers();
    CHECK_EQ(ps[0]->getHero()->getId(), std::string("caocao"));
    CHECK_EQ(ps[1]->getHero()->getId(), std::string("ganning"));
    CHECK_EQ(ps[2]->getHero()->getId(), std::string("lvmeng"));
    CHECK_EQ(ps[3]->getHero()->getId(), std::string("luxun"));
}

// ==================== 三、身份场选将：主公先选完并亮出 ====================

TEST("draft/lord_selects_first_and_is_revealed") {
    GameEngine e;
    e.setSeed(21);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    e.initGame(5, -1, {});
    const std::string text = sink->str();
    size_t reveal = text.find("【选将·主公亮出】");
    size_t firstPick = text.find("【选将】");
    CHECK(reveal != std::string::npos);
    CHECK(firstPick != std::string::npos);
    CHECK(reveal < firstPick); // 主公的亮出记录在所有其他选将之前
    int lord = lordSeatOf(e);
    CHECK(lord >= 0);
    // 亮出的正是主公最终使用的武将
    CHECK(text.find("【选将·主公亮出】" + e.getPlayers()[lord]->getName() + " 选择了 " +
                    e.getPlayers()[lord]->getHero()->getName()) != std::string::npos);
}

TEST("draft/box_sizes_lord_ten_others_four_or_five") {
    GameEngine e;
    e.setSeed(22);
    captureLog(e);
    e.setIdentityDraft(true);
    e.initGame(5, -1, {});
    const auto& cands = e.getIdentityCandidates();
    CHECK_EQ(cands.size(), static_cast<size_t>(5));
    int lord = lordSeatOf(e);
    std::set<std::string> persons;
    int lordHeroes = 0;
    for (const auto& id : cands[lord]) if (isLordHeroId(id)) ++lordHeroes;
    // 主公：4 个主公武将 + 6 个常规武将＝10 个（主公武将不足时以常规补齐，总数仍为 10）
    CHECK_EQ(cands[lord].size(), static_cast<size_t>(10));
    CHECK(lordHeroes >= 1);
    CHECK(lordHeroes <= 4);
    for (size_t seat = 0; seat < cands.size(); ++seat) {
        Identity role = e.getPlayers()[seat]->getIdentity();
        size_t want = (seat == static_cast<size_t>(lord)) ? 10
                    : ((role == Identity::ZHONG_CHEN || role == Identity::NEI_JIAN) ? 5 : 4);
        CHECK_EQ(cands[seat].size(), want);
        for (const auto& id : cands[seat]) {
            CHECK(HeroRegistry::find(id) != nullptr);
            // 同一人物在全部候选间不重复（含标/界/神/谋/势 各版本）
            CHECK(persons.insert(HeroRegistry::personKey(id)).second);
        }
    }
    // 每名玩家的最终武将都来自自己的候选
    for (size_t seat = 0; seat < cands.size(); ++seat) {
        const std::string picked = e.getPlayers()[seat]->getHero()->getId();
        CHECK(std::find(cands[seat].begin(), cands[seat].end(), picked) != cands[seat].end());
    }
}

TEST("draft/human_preferred_hero_is_first_candidate_and_pickable") {
    GameEngine e;
    e.setSeed(23);
    captureLog(e);
    e.setIdentityDraft(true);
    {
        ScriptedInput in("1\n"); // 选第 1 个候选
        e.initGame(4, 0, {"jie_zhonghui"});
    }
    CHECK_EQ(e.getIdentityCandidates()[0][0], std::string("jie_zhonghui"));
    CHECK_EQ(e.getPlayers()[0]->getHero()->getId(), std::string("jie_zhonghui"));
}

TEST("draft/identity_swap_is_once_per_slot") {
    GameEngine e;
    e.setSeed(24);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    {
        // 用户 2026-10-05：“身份的换将每个框只有一次” → 第 2 次 r1 应被拒绝
        ScriptedInput in("r1\nr1\n1\n");
        e.initGame(4, 0, {});
    }
    const std::string text = sink->str();
    CHECK(text.find("【换将】第 1 个位置") != std::string::npos);
    CHECK(text.find("身份场每个框限 1 次") != std::string::npos);
    // 只发生了一次换将
    size_t first = text.find("【换将】第 1 个位置");
    CHECK(text.find("【换将】第 1 个位置", first + 1) == std::string::npos);
}

TEST("draft/zhizun_also_filters_draft_candidates") {
    GameEngine e;
    e.setSeed(25);
    captureLog(e);
    e.setIdentityDraft(true);
    e.setZhizunField(true);
    e.initGame(6, -1, {});
    for (const auto& list : e.getIdentityCandidates())
        for (const auto& id : list) CHECK(!HeroTier::isBannedInZhizun(id, HeroTier::Mode::IDENTITY));
    for (const auto& p : e.getPlayers())
        CHECK(!HeroTier::isBannedInZhizun(p->getHero()->getId(), HeroTier::Mode::IDENTITY));
}

TEST("draft/off_by_default_keeps_legacy_behaviour") {
    GameEngine e;
    e.setSeed(26);
    captureLog(e);
    CHECK(!e.isIdentityDraft());
    e.initGame(4, -1, {"jie_zhonghui", "zhanghe", "lusu", "caopi"});
    const auto& ps = e.getPlayers();
    CHECK_EQ(ps[0]->getHero()->getId(), std::string("jie_zhonghui"));
    CHECK(e.getIdentityCandidates().empty());
}

// ==================== 四、玩家可以指定或随机身份 ====================

TEST("identity/random_by_default_lord_at_seat_zero") {
    GameEngine e;
    e.setSeed(31);
    captureLog(e);
    e.initGame(5, 0, {});
    CHECK(e.getPlayers()[0]->getIdentity() == Identity::ZHU_GONG);
    CHECK_EQ(e.getPreferredIdentity(), -1);
    CHECK(e.getCurrentPlayer()->getIdentity() == Identity::ZHU_GONG); // 主公先动
}

TEST("identity/player_can_specify_rebel") {
    GameEngine e;
    e.setSeed(32);
    auto sink = captureLog(e);
    e.setPreferredIdentity(2); // 反贼
    e.initGame(5, 0, {});
    CHECK(e.getPlayers()[0]->getIdentity() == Identity::FAN_ZEI);
    int lords = 0, lord = -1;
    const auto& ps = e.getPlayers();
    for (size_t i = 0; i < ps.size(); ++i)
        if (ps[i]->getIdentity() == Identity::ZHU_GONG) { ++lords; lord = static_cast<int>(i); }
    CHECK_EQ(lords, 1);
    CHECK(lord != 0);                                  // 主公不在人类座位
    CHECK(e.getCurrentPlayer()->getId() == lord);      // 仍然主公先动
    CHECK(sink->str().find("指定身份：反贼") != std::string::npos);
}

TEST("identity/player_can_specify_lord") {
    GameEngine e;
    e.setSeed(33);
    captureLog(e);
    e.setPreferredIdentity(0); // 主公
    e.initGame(5, 0, {});
    CHECK(e.getPlayers()[0]->getIdentity() == Identity::ZHU_GONG);
    CHECK(e.getCurrentPlayer()->getId() == 0);
}

TEST("identity/player_can_specify_traitor_and_loyalist") {
    GameEngine e1;
    e1.setSeed(34);
    captureLog(e1);
    e1.setPreferredIdentity(3); // 内奸
    e1.initGame(5, 0, {});
    CHECK(e1.getPlayers()[0]->getIdentity() == Identity::NEI_JIAN);

    GameEngine e2;
    e2.setSeed(35);
    captureLog(e2);
    e2.setPreferredIdentity(1); // 忠臣
    e2.initGame(5, 0, {});
    CHECK(e2.getPlayers()[0]->getIdentity() == Identity::ZHONG_CHEN);
}

TEST("identity/unavailable_role_falls_back_to_random") {
    // 3 人局构成＝1主公 1反贼 1内奸，没有忠臣
    GameEngine e;
    e.setSeed(36);
    auto sink = captureLog(e);
    e.setPreferredIdentity(1); // 忠臣
    e.initGame(3, 0, {});
    CHECK(!(e.getPlayers()[0]->getIdentity() == Identity::ZHONG_CHEN));
    CHECK(sink->str().find("没有“忠臣”") != std::string::npos);
    int lords = 0;
    for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::ZHU_GONG) ++lords;
    CHECK_EQ(lords, 1);
}

TEST("identity/specified_role_still_uses_full_composition") {
    GameEngine e;
    e.setSeed(37);
    captureLog(e);
    e.setPreferredIdentity(2); // 反贼
    e.initGame(8, 0, {});
    Roles::RoleConfig cfg = Roles::configFor(8);
    int lords = 0, loyal = 0, rebel = 0, traitor = 0;
    for (const auto& p : e.getPlayers()) {
        switch (p->getIdentity()) {
            case Identity::ZHU_GONG: ++lords; break;
            case Identity::ZHONG_CHEN: ++loyal; break;
            case Identity::FAN_ZEI: ++rebel; break;
            default: ++traitor; break;
        }
    }
    CHECK_EQ(lords, cfg.lords);
    CHECK_EQ(loyal, cfg.loyalists);
    CHECK_EQ(rebel, cfg.rebels);
    CHECK_EQ(traitor, cfg.traitors);
}

// ==================== 五、选将 AI 非随机：针对或辅助 ====================

TEST("ai/draft_loyalist_prefers_lord_country_for_lord_skill_synergy") {
    // 主公曹操（魏，【护驾】要魏势力打闪）：忠臣应优先同势力的张郃（魏），
    // 而不是强度更高但异势力的鲁肃（吴）。
    auto cands = heroes({"zhanghe", "zhangzhao_zhanghong", "jiaxu", "lusu"});
    size_t pick = AIController::chooseDraftHero(cands, ctx(Identity::ZHONG_CHEN, HeroTier::Field::IDENTITY, "caocao"));
    CHECK_EQ(cands[pick]->getId(), std::string("zhanghe"));
    // 同势力加成确实存在：换主公为孙权（吴）后，忠臣改选吴势力
    auto pick2 = AIController::chooseDraftHero(cands, ctx(Identity::ZHONG_CHEN, HeroTier::Field::IDENTITY, "sunquan"));
    CHECK_EQ(cands[pick2]->getId(), std::string("lusu"));
}

TEST("ai/draft_rebel_targets_lord_with_output_not_support") {
    // 主公刘备（蜀）：反贼在“辅助型华佗（群）”和“输出型太史慈（吴）”之间选输出，且避开蜀势力
    auto cands = heroes({"huatuo", "taishici", "daqiao"});
    size_t pick = AIController::chooseDraftHero(cands, ctx(Identity::FAN_ZEI, HeroTier::Field::IDENTITY, "liubei"));
    CHECK_EQ(cands[pick]->getId(), std::string("taishici"));
    // 同一批候选下“针对”与“辅助”确实分流：主公曹操（魏）时，
    // 忠臣选同势力的张郃（吃【护驾】），反贼选输出最高的太史慈。
    auto mixed = heroes({"zhanghe", "zhangzhao_zhanghong", "taishici"});
    size_t loyal = AIController::chooseDraftHero(mixed, ctx(Identity::ZHONG_CHEN, HeroTier::Field::IDENTITY, "caocao"));
    size_t rebel = AIController::chooseDraftHero(mixed, ctx(Identity::FAN_ZEI, HeroTier::Field::IDENTITY, "caocao"));
    CHECK_EQ(mixed[loyal]->getId(), std::string("zhanghe"));
    CHECK_EQ(mixed[rebel]->getId(), std::string("taishici"));
    CHECK(loyal != rebel);
}

TEST("ai/draft_lord_prefers_hero_with_lord_skill") {
    auto cands = heroes({"mou_liubei", "shen_zhugeliang", "caopi", "sunce"});
    size_t pick = AIController::chooseDraftHero(cands, ctx(Identity::ZHU_GONG));
    CHECK_EQ(cands[pick]->getId(), std::string("mou_liubei")); // 主公技【激将】+ 强度档
    // 主公技加分真实存在：去掉主公技语境后，同为忠臣时谋·刘备不再必然第一
    int withLord = AIController::draftScore(hero("mou_liubei"), ctx(Identity::ZHU_GONG));
    int asLoyal = AIController::draftScore(hero("mou_liubei"), ctx(Identity::ZHONG_CHEN));
    CHECK(withLord > asLoyal);
}

TEST("ai/draft_traitor_prefers_balanced_duelist") {
    // 内奸要能单挑：攻守兼备的神赵云优先于纯辅助/纯盾
    auto cands = heroes({"jie_zhonghui", "huatuo", "shen_zhaoyun", "zhangzhao_zhanghong"});
    size_t pick = AIController::chooseDraftHero(cands, ctx(Identity::NEI_JIAN));
    CHECK_EQ(cands[pick]->getId(), std::string("shen_zhaoyun"));
}

TEST("ai/draft_landlord_prefers_self_sufficient_over_healer") {
    auto cands = heroes({"huatuo", "jie_zhonghui", "shen_zhugeliang", "xuyou"});
    size_t pick = AIController::chooseDraftHero(cands, ctx(Identity::DI_ZHU, HeroTier::Field::DDZ_LANDLORD));
    CHECK_EQ(cands[pick]->getId(), std::string("shen_zhugeliang"));
    CHECK(AIController::draftScore(hero("huatuo"), ctx(Identity::DI_ZHU, HeroTier::Field::DDZ_LANDLORD)) <
          AIController::draftScore(hero("shen_zhugeliang"), ctx(Identity::DI_ZHU, HeroTier::Field::DDZ_LANDLORD)));
}

TEST("ai/draft_farmer_prefers_team_player_over_weak_hero") {
    // 界关羽＝公认的农民及格线；标诸葛亮在斗地主是弱将（强度档 1）
    auto cands = heroes({"jie_guanyu", "zhugeliang", "huatuo", "diaochan"});
    size_t pick = AIController::chooseDraftHero(cands, ctx(Identity::NONG_MIN, HeroTier::Field::DDZ_FARMER));
    CHECK_EQ(cands[pick]->getId(), std::string("jie_guanyu"));
    CHECK(AIController::draftScore(hero("zhugeliang"), ctx(Identity::NONG_MIN, HeroTier::Field::DDZ_FARMER)) <
          AIController::draftScore(hero("jie_guanyu"), ctx(Identity::NONG_MIN, HeroTier::Field::DDZ_FARMER)));
}

TEST("ai/draft_choice_is_deterministic_not_random") {
    auto cands = heroes({"zhanghe", "lusu", "taishici", "huatuo", "shen_zhaoyun"});
    auto c = ctx(Identity::FAN_ZEI, HeroTier::Field::IDENTITY, "caocao");
    size_t first = AIController::chooseDraftHero(cands, c);
    for (int i = 0; i < 50; ++i) CHECK_EQ(AIController::chooseDraftHero(cands, c), first);
    // 空候选/空指针不崩
    CHECK_EQ(AIController::chooseDraftHero({}, c), static_cast<size_t>(0));
}

TEST("ai/draft_style_profile_counts_are_capped") {
    // 关键词计数封顶，避免“小作文”武将仅凭文本长度碾压
    auto p = AIController::profileHero(*hero("mou_xiaoqiao"));
    CHECK(p.output <= 4);
    CHECK(p.control <= 4);
    CHECK(p.support <= 4);
    CHECK(p.defense <= 4);
}

// ==================== 选将框展示与对比面板（用户 2026-10-05 规格） ====================

TEST("draft/box_shows_country_hp_and_skill_names") {
    GameEngine e;
    e.setSeed(70);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    e.initGame(5, -1, {});
    const std::string text = sink->str();
    size_t box = text.find("【亮将】");
    CHECK(box != std::string::npos);
    if (box == std::string::npos) return;
    std::string line = text.substr(box, text.find('\n', box) - box);
    // 每个候选都要有：势力、体力 x/y、技能名（用 ｜ 分隔）
    CHECK(line.find("｜") != std::string::npos);
    bool hasCountry = false;
    // 多字节字符必须用字符串字面量：char 字面量在 clang 下报 “character too large for enclosing character literal type”
    for (const char* c : {"魏", "蜀", "吴", "群"})
        if (line.find(c) != std::string::npos) hasCountry = true;
    CHECK(hasCountry);
    CHECK(line.find("/3") != std::string::npos || line.find("/4") != std::string::npos ||
          line.find("/5") != std::string::npos); // 体力 x/y
}

TEST("draft/lord_box_first_four_are_lord_heroes") {
    // 主公框：4 个主公武将（拥有主公技）+ 6 个常规武将
    for (unsigned seed = 71; seed <= 74; ++seed) {
        GameEngine e;
        e.setSeed(seed);
        captureLog(e);
        e.setIdentityDraft(true);
        e.initGame(8, -1, {});
        const auto& cands = e.getIdentityCandidates();
        int lord = lordSeatOf(e);
        CHECK_EQ(cands[lord].size(), static_cast<size_t>(10));
        int lordHeroesInFirst4 = 0;
        for (size_t i = 0; i < 4 && i < cands[lord].size(); ++i)
            if (isLordHeroId(cands[lord][i])) ++lordHeroesInFirst4;
        CHECK_EQ(lordHeroesInFirst4, 4); // 登记中有 12 名主公武将，至尊/普通场都够 4 名
    }
}

TEST("draft/compare_panel_shows_details_without_changing_state") {
    GameEngine e;
    e.setSeed(72);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    {
        ScriptedInput in("d1,2\n1\n"); // 先对比第 1、2 个候选，再选第 1 个
        e.initGame(4, 0, {});
    }
    const std::string text = sink->str();
    CHECK(text.find("候选对比") != std::string::npos);
    CHECK(text.find("强度档: 身份场") != std::string::npos);
    CHECK(text.find("本身份评分") != std::string::npos);
    CHECK(text.find("依据:") != std::string::npos);
    // 对比面板是插入式模态：返回后重印选将问题，且候选没有被改动
    CHECK(text.find(">>> 【选将】") != std::string::npos);
    size_t first = text.find(">>> 【选将】");
    CHECK(text.find(">>> 【选将】", first + 1) != std::string::npos);
    CHECK_EQ(e.getIdentityCandidates()[0].size(), static_cast<size_t>(10)); // 主公框仍是 10 个
    CHECK_EQ(e.getPlayers()[0]->getHero()->getId(), e.getIdentityCandidates()[0][0]); // 最终选了第 1 个
}

TEST("draft/preselect_is_logged_for_every_seat") {
    // “预选展示全部”：每个座位的选将记录都带上系统预选
    GameEngine e;
    e.setSeed(73);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    e.initGame(5, -1, {});
    const std::string text = sink->str();
    int picks = 0, withPre = 0;
    size_t pos = 0;
    while ((pos = text.find("【选将", pos)) != std::string::npos) {
        size_t eol = text.find('\n', pos);
        std::string line = text.substr(pos, eol - pos);
        if (line.find("选择了") != std::string::npos) {
            ++picks;
            if (line.find("（预选: ") != std::string::npos) ++withPre;
        }
        pos = eol;
    }
    CHECK_EQ(picks, 5);
    CHECK_EQ(withPre, 5);
}

TEST("draft/doudizhu_preselect_and_compare_prompt") {
    GameEngine e;
    e.setSeed(74);
    auto sink = captureLog(e);
    e.setAiDelayMs(0);
    e.setDoudizhuAiBidForTesting(3);
    e.initDoudizhuGame(-1, {}, -1, true);
    const std::string text = sink->str();
    int picks = 0, withPre = 0;
    size_t pos = 0;
    while ((pos = text.find("【选将】", pos)) != std::string::npos) {
        size_t eol = text.find('\n', pos);
        std::string line = text.substr(pos, eol - pos);
        if (line.find("选择了") != std::string::npos) {
            ++picks;
            if (line.find("（预选: ") != std::string::npos) ++withPre;
        }
        pos = eol;
    }
    CHECK_EQ(picks, 3);
    CHECK_EQ(withPre, 3);
}

// ==================== 主公框分页与“不设超时”（用户 2026-10-05 第二次问答） ====================

TEST("draft/lord_box_is_paged_lord_heroes_then_normal") {
    GameEngine e;
    e.setSeed(150);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    {
        ScriptedInput in("n\n5\n"); // 翻到第 2 页，选全局编号 5
        e.initGame(5, 0, {});       // 真人是主公（座位 0）
    }
    const std::string text = sink->str();
    CHECK(text.find("第 1/2 页【主公武将（拥有主公技）】共 4 名") != std::string::npos);
    CHECK(text.find("第 2/2 页【常规武将】共 6 名") != std::string::npos);
    CHECK(text.find("n 下一页 / p 上一页") != std::string::npos);
    // 第 1 页列出的 4 名都必须拥有主公技
    const auto& cands = e.getIdentityCandidates()[0];
    CHECK_EQ(cands.size(), static_cast<size_t>(10));
    int lordHeroes = 0;
    for (size_t i = 0; i < 4; ++i) {
        CHECK(isLordHeroId(cands[i]));
        if (isLordHeroId(cands[i])) ++lordHeroes;
    }
    CHECK_EQ(lordHeroes, 4);
    for (size_t i = 4; i < cands.size(); ++i) CHECK(!isLordHeroId(cands[i]));
    // 翻页后仍用全局编号：选了编号 5（第 2 页第一名＝常规武将）
    CHECK_EQ(e.getPlayers()[0]->getHero()->getId(), cands[4]);
    CHECK(!isLordHeroId(e.getPlayers()[0]->getHero()->getId()));
}

TEST("draft/non_lord_box_is_not_paged") {
    GameEngine e;
    e.setSeed(151);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    e.setPreferredIdentity(2); // 真人指定反贼（0 主公 / 1 忠臣 / 2 反贼 / 3 内奸）→ 4 个候选，一页展示
    {
        ScriptedInput in("2\n");
        e.initGame(5, 0, {});
    }
    const std::string text = sink->str();
    CHECK(text.find("第 1/2 页") == std::string::npos);
    CHECK(text.find("n/p 翻页") == std::string::npos);
    int rebelSeat = 0;
    for (int i = 0; i < 5; ++i)
        if (e.getPlayers()[i]->getIdentity() == Identity::FAN_ZEI && !e.getPlayers()[i]->isAI()) rebelSeat = i;
    CHECK_EQ(e.getIdentityCandidates()[rebelSeat].size(), static_cast<size_t>(4));
}

TEST("draft/no_timeout_but_eof_falls_back_to_preselect_explicitly") {
    // 用户选择“不设超时，一直等”：真人在终端会一直阻塞等待输入（引擎没有任何计时器）；
    // 唯一兜底是输入源结束（EOF，例如管道演示），且必须明确记录、绝不静默。
    GameEngine e;
    e.setSeed(152);
    auto sink = captureLog(e);
    e.setIdentityDraft(true);
    {
        ScriptedInput in(""); // 空脚本 → 立即 EOF
        e.initGame(5, 0, {});
    }
    const std::string text = sink->str();
    CHECK(text.find("输入源已结束（未选择）：按 ★预选 采用") != std::string::npos);
    const std::string picked = e.getPlayers()[0]->getHero()->getName();
    CHECK(text.find("按 ★预选 采用 " + picked) != std::string::npos);
    // 采用的正是系统预选那一名
    CHECK_EQ(e.getPlayers()[0]->getHero()->getId(), e.preselectOf(0, false));
}
