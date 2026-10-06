#ifndef THKS_INTERACTION_H
#define THKS_INTERACTION_H

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "Card.h"
#include "Player.h"
#include "Logger.h"

namespace Thks {

class GameEngine;

// =====================================================================
//  统一的人机交互层
//
//  - AI 角色：直接采用调用方传入的预选答案（aiDecision / aiChoice），
//    无任何 I/O；
//  - 人类玩家：通过 Logger 输出提示，从标准输入读取选项。
//
//  从 GameEngine 中拆出，使"流程与结算"和"玩家输入"彻底解耦：
//  引擎只调用 ask* 接口，便于将来接入 GUI / 联机（替换本类即可）。
// =====================================================================
class Interaction {
public:
    explicit Interaction(GameEngine& engine, Logger& logger);

    // ---- 输入原语（EOF 安全：标准输入关闭后必选项自动取默认值，不死循环） ----
    // 读取一个空白分隔的 token；EOF/错误时返回 "0"（视为放弃/结束）
    static std::string readToken();
    // 读 token；热键 t/v 打开图鉴、m 打开记牌器、c/q/g/l 观察局势。
    // 插入式面板退出后调用 reprompt 重印原问题/当前状态（TODO 第 3 项）。
    std::string readTokenWithCodex(const std::string& heroId = "", const std::function<void()>& reprompt = {});
    static bool isGameStateHotkey(const std::string& token);
    // 解析整数；含非数字字符或为空时返回 fallback
    static int parseInt(const std::string& s, int fallback = -1);
    // 标准输入是否已关闭（EOF）
    static bool inputClosed();
    // 新会话或可替换输入流重新连接后，允许恢复读取。
    static void resetInputState();

    // 仅向查看者展示牌面；AI 不向人类观战者输出隐藏信息。
    void viewCards(PlayerPtr viewer, const std::vector<CardPtr>& cards, const std::string& prompt);
    // y/n 确认
    bool askConfirm(PlayerPtr player, const std::string& prompt, bool aiDecision = true);
    // 从候选牌中选择一张（返回 nullptr 表示放弃）
    CardPtr askChooseCard(PlayerPtr player, const std::vector<CardPtr>& candidates, const std::string& prompt,
                          bool optional, CardPtr aiChoice = nullptr);
    // 从候选角色中选择一个（返回 nullptr 表示放弃）
    PlayerPtr askChoosePlayer(PlayerPtr player, const std::vector<PlayerPtr>& candidates, const std::string& prompt,
                              bool optional, PlayerPtr aiChoice = nullptr);
    // 从若干文字选项中选择一个（返回下标）
    int askChooseOption(PlayerPtr player, const std::vector<std::string>& options, const std::string& prompt,
                        int aiChoice = 0);

private:
    GameEngine& engine;
    Logger& logger;
};

} // namespace Thks

#endif // THKS_INTERACTION_H
