#ifndef THKS_TEST_HELPERS_H
#define THKS_TEST_HELPERS_H

// 测试辅助：构造引擎 / 捕获日志 / 造牌

#include "test_framework.h"
#include "Card.h"
#include "Player.h"
#include "GameEngine.h"
#include "Logger.h"
#include "Hero.h"
#include "Interaction.h"
#include <iostream>
#include <sstream>
#include <memory>

using namespace Thks;

// 给引擎挂一个字符串流 Logger，返回该 sink 以便断言日志内容
inline std::shared_ptr<std::ostringstream> captureLog(GameEngine& engine) {
    auto sink = std::make_shared<std::ostringstream>();
    engine.setLogger(std::make_shared<Logger>(sink));
    return sink;
}

// 快速造一张实体牌
inline CardPtr makeCard(const std::string& name, Suit suit, int rank, CardType type, CardSubType sub,
                        ShaElement element = ShaElement::NORMAL, int range = 1) {
    return std::make_shared<Card>(-1, name, suit, rank, type, sub, element, range, "");
}

// 造一个空白武将（无技能）
inline HeroPtr blankHero(int maxHp = 4) {
    return std::make_shared<BlankHero>("测试武将", Country::QUN, Gender::MALE, maxHp);
}

// 清空一名玩家的手牌（保证 AI 无牌可响应，便于确定性断言）
inline void clearHand(Player& p) {
    auto hand = p.getHandCards();
    for (auto& c : hand) p.removeHandCard(c);
}

// 脚本化 stdin（RAII）：构造时把 std::cin 换成字符串流，析构时恢复并重置输入状态。
// 用于测试人类交互路径（选将、换将、对比面板等）。
class ScriptedInput {
public:
    explicit ScriptedInput(const std::string& text)
        : script(text), saved(std::cin.rdbuf(script.rdbuf())) {
        std::cin.clear();                 // 清掉上一次测试可能留下的 eofbit/failbit
        Interaction::resetInputState();
    }
    ~ScriptedInput() {
        std::cin.rdbuf(saved);
        std::cin.clear();
        Interaction::resetInputState();
    }
    ScriptedInput(const ScriptedInput&) = delete;
    ScriptedInput& operator=(const ScriptedInput&) = delete;

private:
    std::istringstream script;
    std::streambuf* saved;
};

#endif // THKS_TEST_HELPERS_H
