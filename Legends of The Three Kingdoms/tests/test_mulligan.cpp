#include "test_framework.h"
#include "test_helpers.h"
#include "GameEngine.h"
#include "Player.h"
#include "HeroRegistry.h"

// 开局换牌（用户 2026-10-06）：“每个人可以换七次牌（初始手牌，属于游戏开始前，
// 此时任何技能都未发动）”；经追问确认“所有座位都能换（AI 也换）”。

namespace {

// 日志中某个子串第一次出现的位置（不存在时返回 npos）。
size_t firstOf(const std::string& text, const std::string& needle) {
    return text.find(needle);
}

} // namespace

// 逐个座位（含 AI）都会换牌；真人按输入换牌；换完仍恰好 4 张手牌。
TEST("mulligan/every_seat_can_swap_and_hand_stays_four") {
    GameEngine e;
    e.setSeed(4242);
    e.setAiDelayMs(0);
    auto sink = captureLog(e);
    CHECK_EQ(e.getMulliganMaxSwaps(), 7); // 默认七次
    ScriptedInput in("1\n1\n0");         // 真人（座位 0）换两次后输入 0 结束
    e.initGame(3, 0, {"mou_jiangwei", "guanyu", "zhangfei"});

    CHECK(e.isMulliganEnabled()); // 建局默认开启（所有座位都能换）
    CHECK_EQ(e.mulliganSwapsOf(0), 2);
    CHECK(e.mulliganSwapsOf(0) <= e.getMulliganMaxSwaps());
    const auto& seats = e.getPlayers();
    for (size_t i = 0; i < seats.size(); ++i) {
        CHECK_EQ((int)seats[i]->getHandCards().size(), 4); // 换牌不改变手牌数
        CHECK(e.mulliganSwapsOf((int)i) <= e.getMulliganMaxSwaps());
    }
    const std::string text = sink->str();
    CHECK(text.find("开局换牌") != std::string::npos);
    CHECK(text.find("【换牌】") != std::string::npos);
}

// 全 AI 对局同样换牌（用户裁定“所有座位都能换（AI 也换）”）；显式关闭后一个座位都不换。
TEST("mulligan/ai_seats_also_swap_and_switch_can_disable_it") {
    {
        GameEngine e;
        e.setSeed(11);
        auto sink = captureLog(e);
        e.initGame(3, -1, {"mou_jiangwei", "guanyu", "zhangfei"}); // 全 AI
        CHECK(e.isMulliganEnabled()); // 默认开启
        int total = 0;
        for (int i = 0; i < 3; ++i) {
            CHECK(e.mulliganSwapsOf(i) <= e.getMulliganMaxSwaps());
            CHECK(e.getPlayers()[i]->getHandCards().size() == 4);
            total += e.mulliganSwapsOf(i);
        }
        CHECK(total >= 1); // AI 会换掉低价值牌（本种子至少有 1 次）
        CHECK(sink->str().find("开局换牌") != std::string::npos);
    }
    {
        GameEngine e;
        e.setSeed(11);
        auto sink = captureLog(e);
        e.setMulliganEnabled(false); // 显式关闭：即使有真人也不换
        ScriptedInput in("1\n1\n1\n");
        e.initGame(3, 0, {"mou_jiangwei", "guanyu", "zhangfei"});
        CHECK(!e.isMulliganEnabled());
        for (int i = 0; i < 3; ++i) CHECK_EQ(e.mulliganSwapsOf(i), 0);
        CHECK(sink->str().find("开局换牌") == std::string::npos);
    }
}

// 每人最多 7 次：真人想换更多也只会换到上限为止（setMulliganMaxSwaps 生效）。
TEST("mulligan/swap_count_is_capped_by_max_swaps") {
    GameEngine e;
    e.setSeed(77);
    e.setMulliganMaxSwaps(2);
    captureLog(e);
    ScriptedInput in("1\n1\n1\n1\n1\n1\n1\n1\n"); // 输入 8 次换牌请求
    e.initGame(3, 0, {"guanyu", "zhangfei", "zhangfei"});
    CHECK_EQ(e.getMulliganMaxSwaps(), 2);
    CHECK_EQ(e.mulliganSwapsOf(0), 2); // 上限 2：多出的请求不再换
    CHECK_EQ((int)e.getPlayers()[0]->getHandCards().size(), 4);
}

// 换牌发生在“游戏开始前、任何技能都未发动”之时：日志顺序上先换牌、后技能（onGameStart）。
TEST("mulligan/happens_before_any_skill_hook") {
    GameEngine e;
    e.setSeed(5);
    e.setAiDelayMs(0);
    auto sink = captureLog(e);
    ScriptedInput in("1\n0");
    e.initGame(3, 0, {"mou_jiangwei", "guanyu", "zhangfei"}); // 谋·姜维【挑衅】在 onGameStart 记录 4/4
    const std::string text = sink->str();
    const size_t swap = firstOf(text, "开局换牌");
    const size_t skill = firstOf(text, "的蓄力点 4/4");
    CHECK(swap != std::string::npos);
    CHECK(skill != std::string::npos);
    if (swap != std::string::npos && skill != std::string::npos)
        CHECK(swap < skill); // 换牌先于任何技能发动
    CHECK(firstOf(text, "【换牌】") < skill);
    CHECK(firstOf(text, "【换牌】") < firstOf(text, "核心引擎初始化完成"));
}

// 换牌不触发任何技能钩子：弃牌相关钩子（如弃牌阶段计数）不应因换牌而累积。
TEST("mulligan/does_not_fire_discard_or_start_hooks") {
    GameEngine e;
    e.setSeed(9);
    e.setAiDelayMs(0);
    captureLog(e);
    ScriptedInput in("1\n1\n1\n0");
    e.initGame(3, 0, {"mou_jiangwei", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);       // 只有 onGameStart 的 4/4，没有被换牌“弃置”抬高
    CHECK_EQ(ps[0]->getMark("挑衅累计"), 0);   // 换牌不发动挑衅
    CHECK_EQ(ps[0]->getMark("挑衅已用"), 0);
}
