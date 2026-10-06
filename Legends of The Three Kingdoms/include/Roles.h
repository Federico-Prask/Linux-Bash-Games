#ifndef THKS_ROLES_H
#define THKS_ROLES_H

#include <vector>
#include <string>
#include <random>
#include "Player.h"

namespace Thks {

// =====================================================================
//  身份模式配置表（对齐游卡官方规则）与胜负判定
//
//  人数:  2   3   4   5   6   7   8
//  主公:  1   1   1   1   1   1   1
//  忠臣:  0   0   1   1   1   2   2
//  反贼:  1   1   1   2   3   3   4
//  内奸:  0   1   1   1   1   1   1
//
//  从 GameEngine 拆出：开局的硬编码身份分配（仅支持 4/5 人）替换为
//  通用配置表，胜负判定同样集中于此，便于扩展新的人数/身份玩法。
// =====================================================================
namespace Roles {

// 支持的玩家人数范围
constexpr int MIN_PLAYERS = 2;
constexpr int MAX_PLAYERS = 8;

// 某人数下的各类身份数量（越界人数夹取到 [MIN_PLAYERS, MAX_PLAYERS]）
struct RoleConfig {
    int lords;
    int loyalists;
    int rebels;
    int traitors;
};
RoleConfig configFor(int totalPlayers);

// 生成身份序列：座位 0 为主公，其余按 忠臣→反贼→内奸 顺序排列（尚未洗牌）
std::vector<Identity> buildIdentities(int totalPlayers);

// 除主公（座位 0）外随机洗牌入座
void shuffleIdentities(std::vector<Identity>& identities, std::mt19937& rng);

// 统计存活角色中某身份的数量
int aliveCount(const std::vector<PlayerPtr>& players, Identity role);

// 胜负判定（与原 GameEngine::checkGameOver 语义一致）：
//   主公阵亡   -> 若场上仅剩一名内奸则"内奸胜 (一内独存)"，否则"反贼胜"
//   反贼内奸全灭且主公存活 -> "主公与忠臣胜"
void checkGameOver(const std::vector<PlayerPtr>& players, bool& gameOver, std::string& winningFaction);

// ---- 斗地主（GameMode::DOUDIZHU）骨架 ----
// 3 人：1 地主 + 2 农民。座位 landlordIndex 为地主（默认 0，骨架阶段不做竞标）。
std::vector<Identity> doudizhuIdentities(int landlordIndex = 0);
// 胜负判定：地主阵亡 -> 农民胜；两名农民阵亡 -> 地主胜。
void checkDoudizhuGameOver(const std::vector<PlayerPtr>& players, bool& gameOver, std::string& winningFaction);

// "1主公 1忠臣 2反贼 1内奸" 形式的身份构成描述（菜单展示用）
std::string describeComposition(int totalPlayers);

} // namespace Roles

} // namespace Thks

#endif // THKS_ROLES_H
