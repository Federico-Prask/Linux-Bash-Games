#include "Interaction.h"
#include "Codex.h"
#include "CardTracker.h"
#include "GameEngine.h"
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <algorithm>

namespace Thks {

namespace {

// 标准输入是否已关闭（EOF）。关闭后所有必选提示都自动选择默认项，避免死循环。
// 全局唯一：无论从引擎还是交互层调用，EOF 状态保持一致。
bool g_inputClosed = false;

std::string heroIdOf(PlayerPtr player) {
    if (!player || !player->getHero()) return "";
    return player->getHero()->getId();
}

} // namespace

Interaction::Interaction(GameEngine& engineRef, Logger& loggerRef)
    : engine(engineRef), logger(loggerRef) {}

bool Interaction::isGameStateHotkey(const std::string& token) {
    return token == "c" || token == "C" || token == "q" || token == "Q" ||
           token == "g" || token == "G" || token == "l" || token == "L";
}

std::string Interaction::readTokenWithCodex(const std::string& heroId, const std::function<void()>& reprompt) {
    auto reprintPrompt = [&reprompt]() { if (reprompt) reprompt(); };
    while (true) {
        std::string token = readToken();
        if (inputClosed()) return token;
        if (Codex::isCodexHotkey(token)) {
            Codex::open(heroId); // 模态：退出后恢复状态并重印原问题
            reprintPrompt();
            continue;
        }
        if (CardTracker::isHotkey(token)) {
            CardTracker::open(engine, heroId); // 与图鉴相同：退出后恢复状态并重印原问题
            reprintPrompt();
            continue;
        }
        if (isGameStateHotkey(token)) {
            // 局势查看：同图鉴/记牌器的插入式模态，退出后恢复原询问
            engine.printGameState();
            std::cout << "\n[局势查看] 输入 0 返回";
            if (!inputClosed()) std::cout << " | t/v 图鉴 | m 记牌器 | c 刷新";
            std::cout << "\n> " << std::flush;
            while (!inputClosed()) {
                std::string inner = readToken();
                if (inner == "0" || inputClosed()) break;
                if (Codex::isCodexHotkey(inner)) {
                    Codex::open(heroId);
                    std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器 | c 刷新\n> " << std::flush;
                    continue;
                }
                if (CardTracker::isHotkey(inner)) {
                    CardTracker::open(engine, heroId);
                    std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器 | c 刷新\n> " << std::flush;
                    continue;
                }
                if (isGameStateHotkey(inner)) {
                    engine.printGameState();
                    std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器 | c 刷新\n> " << std::flush;
                    continue;
                }
                std::cout << "输入 0 返回局势查看。\n> " << std::flush;
            }
            if (!inputClosed()) std::cout << "已退出局势查看，回到对局。\n";
            reprintPrompt();
            continue;
        }
        return token;
    }
}

std::string Interaction::readToken() {
    std::string token;
    if (g_inputClosed) return "0";
    if (!(std::cin >> token)) {
        g_inputClosed = true;
        std::cin.clear();
        return "0";
    }
    return token;
}

int Interaction::parseInt(const std::string& s, int fallback) {
    if (s.empty()) return fallback;
    size_t i = (s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return fallback;
    for (; i < s.size(); ++i) {
        if (!isdigit(static_cast<unsigned char>(s[i]))) return fallback;
    }
    return std::atoi(s.c_str());
}

// 重置输入状态：既清“输入已关闭”标志，也清 std::cin 的错误位。
// 后者很关键——测试里常用字符串流替换 std::cin 的 rdbuf，若上一次测试把 cin 读到 EOF，
// eofbit/failbit 会残留到新测试，导致新的脚本流一开始就读不到内容（曾造成
// mou/liucheng_lueying_* 偶发失败：AI 默认项没被消费、椎标记数不符）。
void Interaction::resetInputState() {
    g_inputClosed = false;
    std::cin.clear();
}

bool Interaction::inputClosed() {
    return g_inputClosed;
}

void Interaction::viewCards(PlayerPtr viewer, const std::vector<CardPtr>& cards, const std::string& prompt) {
    if (!viewer || viewer->isAI()) return;
    logger.raw() << "\n>>> " << prompt << "\n";
    for (const auto& card : cards) logger.raw() << "  " << card->getFormattedName() << "\n";
}

bool Interaction::askConfirm(PlayerPtr player, const std::string& prompt, bool aiDecision) {
    if (!player) return false;
    if (player->isAI()) return aiDecision;
    auto reprompt = [this, &prompt]() {
        logger.raw() << "\n>>> " << prompt << " (y/n, t=图鉴, m=记牌器, c=局势): " << std::flush;
    };
    reprompt();
    std::string token = readTokenWithCodex(heroIdOf(player), reprompt);
    return !token.empty() && (token[0] == 'y' || token[0] == 'Y' || token == "1" || token == "是");
}

CardPtr Interaction::askChooseCard(PlayerPtr player, const std::vector<CardPtr>& candidates, const std::string& prompt,
                                   bool optional, CardPtr aiChoice) {
    if (!player || candidates.empty()) return nullptr;
    if (player->isAI()) {
        return aiChoice ? aiChoice : candidates.front();
    }
    auto reprompt = [this, &prompt, &candidates, optional]() {
        logger.raw() << "\n>>> " << prompt << "\n";
        for (size_t i = 0; i < candidates.size(); ++i) {
            logger.raw() << "  (" << (i + 1) << ") " << candidates[i]->getFormattedName() << "\n";
        }
        logger.raw() << "输入编号" << (optional ? "（0 放弃，t 图鉴，m 记牌器，c 局势）" : "（t 图鉴，m 记牌器，c 局势）") << ": " << std::flush;
    };
    while (true) {
        reprompt();
        std::string token = readTokenWithCodex(heroIdOf(player), reprompt);
        int idx = parseInt(token);
        if (idx >= 1 && idx <= static_cast<int>(candidates.size())) return candidates[idx - 1];
        if (optional && idx == 0) return nullptr;
        if (!optional && idx == 0 && inputClosed()) return candidates.front();
        logger.log("无效的选项，请重新输入！");
    }
}

PlayerPtr Interaction::askChoosePlayer(PlayerPtr player, const std::vector<PlayerPtr>& candidates, const std::string& prompt,
                                       bool optional, PlayerPtr aiChoice) {
    if (!player || candidates.empty()) return nullptr;
    if (player->isAI()) {
        return aiChoice ? aiChoice : candidates.front();
    }
    auto reprompt = [this, &prompt, player, &candidates, optional]() {
        logger.raw() << "\n>>> " << prompt << "\n";
        for (size_t i = 0; i < candidates.size(); ++i) {
            bool showIdentity = !candidates[i]->isAI() || candidates[i]->getIdentity() == Identity::ZHU_GONG;
            logger.raw() << "  (" << (i + 1) << ") " << candidates[i]->getFormattedStatus(showIdentity)
                         << " | 距离 " << engine.calculateDistance(*player, *candidates[i]) << "\n";
        }
        logger.raw() << "输入编号" << (optional ? "（0 取消，t 图鉴，m 记牌器，c 局势）" : "（t 图鉴，m 记牌器，c 局势）") << ": " << std::flush;
    };
    while (true) {
        reprompt();
        std::string token = readTokenWithCodex(heroIdOf(player), reprompt);
        int idx = parseInt(token);
        if (idx >= 1 && idx <= static_cast<int>(candidates.size())) return candidates[idx - 1];
        if (optional && idx == 0) return nullptr;
        if (!optional && idx == 0 && inputClosed()) return candidates.front();
        logger.log("无效的选项，请重新输入！");
    }
}

int Interaction::askChooseOption(PlayerPtr player, const std::vector<std::string>& options, const std::string& prompt,
                                 int aiChoice) {
    if (!player || options.empty()) return 0;
    if (player->isAI()) {
        return std::max(0, std::min(aiChoice, static_cast<int>(options.size()) - 1));
    }
    auto reprompt = [this, &prompt, &options]() {
        logger.raw() << "\n>>> " << prompt << "\n";
        for (size_t i = 0; i < options.size(); ++i) {
            logger.raw() << "  (" << (i + 1) << ") " << options[i] << "\n";
        }
        logger.raw() << "输入编号（t 图鉴, m 记牌器, c 局势）: " << std::flush;
    };
    while (true) {
        reprompt();
        std::string token = readTokenWithCodex(heroIdOf(player), reprompt);
        int idx = parseInt(token);
        if (idx >= 1 && idx <= static_cast<int>(options.size())) return idx - 1;
        if (inputClosed()) return 0;
        logger.log("无效的选项，请重新输入！");
    }
}

} // namespace Thks
