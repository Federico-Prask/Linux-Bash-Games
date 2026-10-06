// 「游戏开始」时点与轮次起点（用户 2026-10-05 裁定）：
//   ① “所有游戏开始时点的技能都没有发动”是**错的**——游戏开始时点的技能必须发动（此前斗地主路径漏发）；
//   ② **游戏开始时也等同于首轮开始时**（要求，必须满足）；
//   ③ 蓄力技初始点数按各自声明（1/5、4/4、0/7、2/4、1/3、0/0），**不是全为 0**。
#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "HeroRegistry.h"
#include "SkillsMyth.h"
#include <memory>
#include <utility>
#include <vector>

using namespace Thks;

namespace {

// 记录轮次广播与回合开始的探针技能（测试专用）
class RoundProbeSkill : public StateSkill {
public:
    std::vector<std::pair<int, int>> rounds; // (广播时的轮次, 当时的当前回合座位)
    int turnStarts = 0;
    int turnsBeforeFirstRound = -1;

    RoundProbeSkill() : StateSkill("轮次探针", "测试用：记录轮次广播与回合开始。") {}

    void onRoundStart(GameEngine& engine, Player&) override {
        int seat = engine.getCurrentPlayer() ? engine.getCurrentPlayer()->getId() : -1;
        if (rounds.empty()) turnsBeforeFirstRound = turnStarts;
        rounds.push_back({engine.getCurrentRound(), seat});
    }
    void onTurnStart(GameEngine& engine, Player& self) override {
        if (engine.isPlayerTurn(self)) ++turnStarts;
    }
};

int pileOf(GameEngine& e, int seat, const std::string& pile) {
    return e.getPlayers()[seat]->getPileCount(pile);
}
int markOf(GameEngine& e, int seat, const std::string& key) {
    return e.getPlayers()[seat]->getMark(key);
}

} // namespace

// ==================== 一、游戏开始时点的技能必须发动（两个模式都要） ====================

TEST("game_start/qixing_fires_in_identity_and_doudizhu") {
    // 神诸葛亮【七星】：游戏开始时，你将牌堆顶的七张牌扣置于你的武将牌上（称为“星”）
    {
        GameEngine e;
        e.setSeed(91);
        captureLog(e);
        e.initGame(3, -1, {"shen_zhugeliang", "guanyu", "zhangfei"});
        CHECK_EQ(pileOf(e, 0, "星"), 7);
    }
    {
        GameEngine e;
        e.setSeed(92);
        auto sink = captureLog(e);
        e.initDoudizhuGame(-1, {"shen_zhugeliang", "guanyu", "zhangfei"}, 0, false);
        CHECK_EQ(pileOf(e, 0, "星"), 7); // 修复前斗地主下为 0（onGameStart 被 quietSetup 跳过）
        CHECK(sink->str().find("七星") != std::string::npos || pileOf(e, 0, "星") == 7);
    }
}

TEST("game_start/kuangbao_and_huashen_fire_in_doudizhu") {
    // 神吕布【狂暴】锁定技：游戏开始时，你获得 2 个“暴怒”标记
    {
        GameEngine e;
        e.setSeed(93);
        captureLog(e);
        e.initDoudizhuGame(-1, {"shen_lvbu", "guanyu", "zhangfei"}, 0, false);
        CHECK_EQ(markOf(e, 0, "暴怒"), 2);
    }
    // 左慈【化身】：游戏开始时，你随机获得两张武将牌作为化身
    {
        GameEngine e;
        e.setSeed(94);
        captureLog(e);
        e.initDoudizhuGame(-1, {"zuoci", "guanyu", "zhangfei"}, 0, false);
        auto huashen = e.getPlayers()[0]->getHero()->findSkill("化身");
        CHECK(huashen != nullptr);
        // 化身牌（武将牌）保存在技能自身的 avatars 列表里：游戏开始时应随机获得两张
        auto myth = std::dynamic_pointer_cast<MythSkill>(huashen);
        CHECK(myth != nullptr);
        if (myth) CHECK_EQ(myth->getAvatarIds().size(), static_cast<size_t>(2));
    }
}

TEST("game_start/mode_specific_game_start_skills_fire_in_doudizhu") {
    // 势·董昭【势-妙略】：游戏开始时，你获得两张【瞒天过海】
    {
        GameEngine e;
        e.setSeed(95);
        captureLog(e);
        e.initDoudizhuGame(-1, {"shi_dongzhao", "guanyu", "zhangfei"}, 0, false);
        int cnt = 0;
        for (const auto& c : e.getPlayers()[0]->getHandCards())
            if (c && c->getName() == "瞒天过海") ++cnt;
        for (const auto& name : e.getPlayers()[0]->getPileNames())
            for (const auto& c : e.getPlayers()[0]->getPile(name))
                if (c && c->getName() == "瞒天过海") ++cnt;
        CHECK(cnt >= 2);
    }
    // 谋·卢植【谋-明任】：游戏开始时摸两张牌，然后将一张手牌扣置为“任”
    {
        GameEngine e;
        e.setSeed(96);
        captureLog(e);
        e.initDoudizhuGame(-1, {"mou_luzhi", "guanyu", "zhangfei"}, 0, false);
        CHECK_EQ(pileOf(e, 0, "任"), 1);
    }
}

// ==================== 二、蓄力技初始点数（不是全 0） ====================

TEST("charge/initial_points_match_declared_in_both_modes") {
    struct Case { const char* id; int charge; int cap; };
    const std::vector<Case> cases = {
        {"diy_zhonghui", 1, 5},      // 持恒技，蓄力技（1/5）
        {"shi_zhonghui", 4, 4},      // 蓄力技（4/4）
        {"mou_menghuo", 0, 7},       // 蓄力技（0/7）
        {"mou_gongsunzan", 2, 4},    // 蓄力技（2/4）
        {"mou_zhugejin", 1, 3},      // 蓄力技（1/3）
        {"shi_dengai", 0, 0},        // 蓄力技（0/0）
    };
    for (const auto& c : cases) {
        // 身份场
        {
            GameEngine e;
            e.setSeed(97);
            captureLog(e);
            e.initGame(3, -1, {c.id, "guanyu", "zhangfei"});
            CHECK_EQ(markOf(e, 0, "蓄力"), c.charge);
            CHECK_EQ(markOf(e, 0, "蓄力上限"), c.cap);
        }
        // 斗地主（修复前一律 0/0）
        {
            GameEngine e;
            e.setSeed(98);
            captureLog(e);
            e.initDoudizhuGame(-1, {c.id, "guanyu", "zhangfei"}, 0, false);
            CHECK_EQ(markOf(e, 0, "蓄力"), c.charge);
            CHECK_EQ(markOf(e, 0, "蓄力上限"), c.cap);
        }
    }
}

// ==================== 三、游戏开始＝首轮开始（轮次起点＝开局当前回合角色） ====================

TEST("round/game_start_is_first_round_start") {
    GameEngine e;
    e.setSeed(99);
    captureLog(e);
    e.initGame(3, -1, {"caocao", "guanyu", "zhangfei"});
    auto probe = std::make_shared<RoundProbeSkill>();
    e.getPlayers()[0]->getHero()->addSkill(probe);
    // 模拟“主公先动而主公不在座位 0”：开局当前回合角色为座位 2
    e.setCurrentPlayerForTesting(e.getPlayers()[2]);
    e.startGame();

    CHECK(probe->rounds.size() >= 2);
    if (probe->rounds.size() < 2) return;
    CHECK_EQ(probe->rounds[0].first, 1);        // 首次 onRoundStart 就是第 1 轮
    CHECK_EQ(probe->turnsBeforeFirstRound, 0);  // 且在任何回合开始之前（游戏开始＝首轮开始）
    CHECK_EQ(probe->rounds[0].second, 2);       // 轮次起点＝开局当前回合角色（座位 2，不是 0）
    for (const auto& r : probe->rounds) CHECK_EQ(r.second, 2); // 每轮都从同一起点开始
    for (size_t i = 0; i < probe->rounds.size(); ++i)
        CHECK_EQ(probe->rounds[i].first, static_cast<int>(i) + 1); // 轮次连续递增
}

TEST("round/first_round_banner_counts_every_seat_once") {
    // 5 人局、主公在座位 0：第 2 轮横幅之前，每个座位都恰好行动过一次
    GameEngine e;
    e.setSeed(100);
    auto sink = captureLog(e);
    e.initGame(5, -1, {"caocao", "guanyu", "zhangfei", "zhaoyun", "machao"});
    e.startGame();
    const std::string text = sink->str();
    size_t banner = text.find("第 2 轮");
    CHECK(banner != std::string::npos);
    if (banner == std::string::npos) return;
    int turns = 0;
    size_t pos = 0;
    while ((pos = text.find("的回合开始", pos)) != std::string::npos && pos < banner) { ++turns; pos += 6; }
    // 5 名角色各行动一次；若第一轮内有人阵亡或跳过回合则会少于 5（不同平台随机源不同，故给区间）
    CHECK(turns >= 3);
    CHECK(turns <= 5);
}

TEST("round/first_round_limited_skills_available_at_once") {
    // 【立储】限定“第一轮”：开局即第 1 轮，主公（哪怕不在座位 0）在第一轮内就能立储
    GameEngine e;
    e.setSeed(101);
    captureLog(e);
    e.initGame(5, -1, {"caocao", "guanyu", "zhangfei", "zhaoyun", "machao"});
    CHECK_EQ(e.getCurrentRound(), 0); // 尚未开始
    e.setRoundForTesting(1);
    PlayerPtr lord;
    for (const auto& p : e.getPlayers()) if (p->getIdentity() == Identity::ZHU_GONG) lord = p;
    CHECK(lord != nullptr);
    if (!lord) return;
    auto lichu = lord->getHero()->findSkill("立储");
    CHECK(lichu != nullptr);
    if (!lichu) return;
    // AI 立储只看公开证据（不读真实身份）：先让一名角色留下“偏向主忠方”的行为证据
    PlayerPtr ally;
    for (const auto& p : e.getPlayers())
        if (p->getId() != lord->getId() && p->getIdentity() == Identity::ZHONG_CHEN) ally = p;
    CHECK(ally != nullptr);
    if (!ally) return;
    e.recordRelation(ally->getId(), lord->getId(), -3); // 例如替主公挡了锦囊/救了主公
    lichu->onAnyPhaseEnd(e, *lord, *lord, TurnPhase::FINISH);
    CHECK_EQ(lord->getMark("立储已用"), 1);
    CHECK_EQ(ally->getMark("储君"), 1);
}

TEST("round/game_start_and_first_round_do_not_double_grant") {
    // 用户既有裁定：onGameStart 与首次 onRoundStart 统一为“第 1 轮开始”，不重复发放。
    // DIY 钟会【权谋】蓄力技（1/5）：游戏开始给 1 点，首轮开始不再重复给，第 2 轮起 +2。
    GameEngine e;
    e.setSeed(140);
    auto sink = captureLog(e);
    e.initGame(2, -1, {"diy_zhonghui", "guanyu"});
    CHECK_EQ(markOf(e, 0, "蓄力"), 1);
    CHECK_EQ(markOf(e, 0, "蓄力上限"), 5);

    e.broadcastRoundStart(); // 第 1 轮开始（＝游戏开始的同一时点）
    CHECK_EQ(markOf(e, 0, "蓄力"), 1);      // 不重复发放
    CHECK_EQ(markOf(e, 0, "蓄力上限"), 5);

    e.broadcastRoundStart(); // 第 2 轮开始
    CHECK_EQ(markOf(e, 0, "蓄力"), 3);      // +2
    CHECK(sink->str().find("权谋") != std::string::npos);
}
