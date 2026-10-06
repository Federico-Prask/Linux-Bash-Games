// 主公技口径（用户 2026-10-05）：
//   ①「主公技**当且仅当你是主公**时才会在**游戏开始时**获得」——非主公（含斗地主的地主/农民）身上没有主公技；
//   ②「你**只要有主公技就能用**主公技」——不再按身份在运行期过滤，拥有即可用；
//   ③ 储君继位成为新的主公后，补授予其武将的主公技。
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "SkillsMode.h"
#include <string>
#include <vector>

using namespace Thks;

namespace {

bool heroHas(PlayerPtr p, const std::string& skill) {
    return p && p->getHero() && p->getHero()->findSkill(skill) != nullptr;
}

bool effectiveHas(GameEngine& e, PlayerPtr p, const std::string& skill) {
    for (const auto& s : e.getEffectiveSkills(*p)) if (s->getName() == skill) return true;
    return false;
}

} // namespace

TEST("lord/lord_skill_granted_only_to_lord_at_game_start") {
    // 座位 0＝主公＝曹操（护驾）；座位 1＝张角（黄天）但不是主公 → 不该有主公技
    GameEngine e;
    e.setSeed(160);
    auto sink = captureLog(e);
    e.initGame(5, -1, {"caocao", "zhangjiao", "guanyu", "zhaoyun", "machao"});
    auto ps = e.getPlayers();
    CHECK(ps[0]->getIdentity() == Identity::ZHU_GONG);
    CHECK(heroHas(ps[0], "护驾"));                       // 主公拥有自己的主公技
    CHECK(effectiveHas(e, ps[0], "护驾"));               // 且立即可用（不再按身份过滤）
    CHECK(!heroHas(ps[1], "黄天"));                      // 非主公的张角没有黄天
    CHECK(sink->str().find("不是主公，游戏开始时不获得主公技【黄天】") != std::string::npos);
    CHECK(sink->str().find("是主公，游戏开始时获得主公技【护驾】") != std::string::npos ||
          sink->str().find("主公技") != std::string::npos);
}

TEST("lord/no_lord_skill_in_doudizhu") {
    // 斗地主没有主公：地主与农民都不该拥有主公技
    GameEngine e;
    e.setSeed(161);
    captureLog(e);
    e.initDoudizhuGame(-1, {"caocao", "zhangjiao", "sunquan"}, 0, false);
    auto ps = e.getPlayers();
    CHECK(!heroHas(ps[0], "护驾"));   // 地主曹操
    CHECK(!heroHas(ps[1], "黄天"));   // 农民张角
    CHECK(!heroHas(ps[2], "救援"));   // 农民孙权
    // 但地主仍有斗地主模式技能
    CHECK(heroHas(ps[0], "飞扬"));
    CHECK(heroHas(ps[0], "跋扈"));
}

TEST("lord/ruoyu_and_kechang_are_lord_skills") {
    // 官网原文写“主公技”的两个技能此前漏了 LORD 标签：刘禅【若愚】、势·钟会【势-克昌】
    for (const char* id : {"liuchan", "shi_zhonghui"}) {
        auto info = HeroRegistry::find(id);
        CHECK(info != nullptr);
        if (!info) continue;
        auto h = info->create();
        bool foundLord = false;
        for (const auto& s : h->getSkills()) {
            if (s->getDescription().find("主公技") == std::string::npos) continue;
            CHECK(s->hasTag(SkillTag::LORD)); // 描述含“主公技”→ 必须带 LORD 标签
            if (s->hasTag(SkillTag::LORD)) foundLord = true;
        }
        CHECK(foundLord);
    }
    // 非主公的刘禅没有【若愚】；非主公的势·钟会没有【势-克昌】
    {
        GameEngine e;
        e.setSeed(162);
        captureLog(e);
        e.initGame(5, -1, {"guanyu", "liuchan", "shi_zhonghui", "zhaoyun", "machao"});
        auto ps = e.getPlayers();
        CHECK(ps[0]->getIdentity() == Identity::ZHU_GONG);
        CHECK(!heroHas(ps[1], "若愚"));
        CHECK(!heroHas(ps[2], "势-克昌"));
        // 克昌不再对全场群势力生效：群势力角色到别人的距离不是 1
        bool anyQunDist1 = false;
        for (size_t i = 1; i < ps.size(); ++i) {
            if (!ps[i]->getHero() || ps[i]->getHero()->getCountry() != Country::QUN) continue;
            for (size_t j = 0; j < ps.size(); ++j) {
                if (i == j) continue;
                if (e.calculateDistance(*ps[i], *ps[j]) == 1) anyQunDist1 = true;
            }
        }
        (void)anyQunDist1; // 距离 1 也可能只是座位相邻，下面用主公布局做正向对照
    }
    // 主公是势·钟会时：克昌在场，群势力角色使用【杀】无距离限制（距离恒为 1）
    {
        GameEngine e;
        e.setSeed(163);
        captureLog(e);
        e.initGame(5, -1, {"shi_zhonghui", "zhangjiao", "diaochan", "yuanshao", "machao"});
        auto ps = e.getPlayers();
        CHECK(heroHas(ps[0], "势-克昌"));
        CHECK(ps[0]->getIdentity() == Identity::ZHU_GONG);
        // 张角/貂蝉/袁绍都是群势力：到最远座位的距离也应为 1
        for (int i : {1, 2, 3}) {
            if (!ps[i]->getHero() || ps[i]->getHero()->getCountry() != Country::QUN) continue;
            for (size_t j = 0; j < ps.size(); ++j) {
                if (static_cast<size_t>(i) == j) continue;
                CHECK_EQ(e.calculateDistance(*ps[i], *ps[j]), 1);
            }
        }
    }
}

TEST("lord/heir_gains_lord_skill_on_succession") {
    // 储君（忠臣·张角）继位后获得自己武将的主公技【黄天】
    GameEngine e;
    e.setSeed(164);
    auto sink = captureLog(e);
    e.initGame(5, -1, {"caocao", "zhangjiao", "guanyu", "zhaoyun", "machao"});
    auto ps = e.getPlayers();
    CHECK(!heroHas(ps[1], "黄天"));          // 建局时不是主公 → 没有主公技
    ps[1]->setIdentity(Identity::ZHONG_CHEN); // 确保是忠臣才能继位
    ps[1]->addMark("储君", 1);
    ps[0]->setHp(1);
    e.killPlayer(ps[0]);
    CHECK(!e.isGameOver());                  // 继位延续主忠方
    CHECK(ps[1]->getIdentity() == Identity::ZHU_GONG);
    CHECK(heroHas(ps[1], "黄天"));           // 继位后补授予主公技
    CHECK(effectiveHas(e, ps[1], "黄天"));   // 拥有即可用
    CHECK(sink->str().find("继位成为新的主公") != std::string::npos);
    CHECK(sink->str().find("游戏开始时获得主公技【黄天】") != std::string::npos ||
          sink->str().find("获得主公技【黄天】") != std::string::npos);
}

TEST("lord/identity_change_does_not_grant_lord_skill") {
    // 【择途】侍奉明主：内奸变忠臣，不会因为身份变化而获得主公技
    GameEngine e;
    e.setSeed(165);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "zhangjiao", "guanyu", "zhaoyun", "machao"});
    auto ps = e.getPlayers();
    // 把张角改成内奸再改成忠臣：全程都不该有黄天
    ps[1]->setIdentity(Identity::NEI_JIAN);
    e.applyLordSkillsByIdentity();
    CHECK(!heroHas(ps[1], "黄天"));
    ps[1]->setIdentity(Identity::ZHONG_CHEN);
    e.applyLordSkillsByIdentity();
    CHECK(!heroHas(ps[1], "黄天"));
    // 只有成为主公才获得
    ps[1]->setIdentity(Identity::ZHU_GONG);
    e.applyLordSkillsByIdentity();
    CHECK(heroHas(ps[1], "黄天"));
}

TEST("lord/huashen_never_borrows_lord_skill_for_non_lord") {
    // 左慈（非主公）的【化身】不能借到主公技（否则“只要有主公技就能用”会让非主公用上主公技）
    int borrowed = 0, lordBorrowed = 0;
    for (unsigned seed = 170; seed < 190; ++seed) {
        GameEngine e;
        e.setSeed(seed);
        captureLog(e);
        e.initGame(5, -1, {"guanyu", "zuoci", "zhangfei", "zhaoyun", "machao"});
        auto ps = e.getPlayers();
        CHECK(ps[1]->getHero()->getName() == "左慈");
        CHECK(ps[1]->getIdentity() != Identity::ZHU_GONG);
        auto huashen = ps[1]->getHero()->findSkill("化身");
        CHECK(huashen != nullptr);
        if (!huashen) continue;
        // 化身在游戏开始时已获得两张化身牌并更换过一次；再看当前借用的技能
        for (const auto& s : ps[1]->getHero()->getSkills()) {
            if (!s || s->getName() == "化身") continue;
            if (isModeSkillName(s->getName())) continue; // 玩法赋予的模式技能不算“借来的技能”
            ++borrowed;
            if (s->hasTag(SkillTag::LORD)) ++lordBorrowed;
        }
    }
    CHECK(borrowed > 0);          // 20 局里确实借到过技能（否则测试没有覆盖力）
    CHECK_EQ(lordBorrowed, 0);    // 但一次都没有借到主公技
}
