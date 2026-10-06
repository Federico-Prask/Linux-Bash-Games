#include "Roles.h"
#include <algorithm>

namespace Thks {

namespace Roles {

RoleConfig configFor(int totalPlayers) {
    if (totalPlayers <= 2) return {1, 0, 1, 0}; // 2人：主公 vs 反贼（单挑）
    switch (totalPlayers) {
        case 3: return {1, 0, 1, 1};
        case 4: return {1, 1, 1, 1};
        case 5: return {1, 1, 2, 1};
        case 6: return {1, 1, 3, 1};
        case 7: return {1, 2, 3, 1};
        case 8: return {1, 2, 4, 1};
        default: return {1, 2, 4, 1}; // >8 人夹取到 8 人配置
    }
}

std::vector<Identity> buildIdentities(int totalPlayers) {
    RoleConfig cfg = configFor(totalPlayers);
    std::vector<Identity> ids;
    ids.reserve(static_cast<size_t>(cfg.lords + cfg.loyalists + cfg.rebels + cfg.traitors));
    for (int i = 0; i < cfg.lords; ++i)     ids.push_back(Identity::ZHU_GONG);
    for (int i = 0; i < cfg.loyalists; ++i) ids.push_back(Identity::ZHONG_CHEN);
    for (int i = 0; i < cfg.rebels; ++i)    ids.push_back(Identity::FAN_ZEI);
    for (int i = 0; i < cfg.traitors; ++i)  ids.push_back(Identity::NEI_JIAN);
    return ids;
}

void shuffleIdentities(std::vector<Identity>& identities, std::mt19937& rng) {
    if (identities.size() <= 1) return;
    std::shuffle(identities.begin() + 1, identities.end(), rng);
}

std::vector<Identity> doudizhuIdentities(int landlordIndex) {
    std::vector<Identity> ids(3, Identity::NONG_MIN);
    if (landlordIndex < 0 || landlordIndex >= 3) landlordIndex = 0; // 越界回退：座位 0 为地主
    ids[landlordIndex] = Identity::DI_ZHU;
    return ids;
}

void checkDoudizhuGameOver(const std::vector<PlayerPtr>& players, bool& gameOver, std::string& winningFaction) {
    bool landlordAlive = false;
    for (const auto& p : players)
        if (p->getIdentity() == Identity::DI_ZHU && p->isAlive()) { landlordAlive = true; break; }
    if (!landlordAlive) {
        gameOver = true;
        winningFaction = "农民胜";
        return;
    }
    if (aliveCount(players, Identity::NONG_MIN) == 0) {
        gameOver = true;
        winningFaction = "地主胜";
    }
}

int aliveCount(const std::vector<PlayerPtr>& players, Identity role) {
    int count = 0;
    for (const auto& p : players) {
        if (p->isAlive() && p->getIdentity() == role) ++count;
    }
    return count;
}

// 身份场胜负判定（含内奸【择途】后的野心家）：
//   ① 野心家是唯一存活角色 -> "野心家胜（唯一幸存者）"（最高优先，先于其他判定）；
//   ② 主公阵亡：若仍有野心家存活 -> **不结束**（用户裁定：即使主公死亡野心家也不会立即失败）；
//      否则按原规则——仅剩一名内奸则内奸胜，其余反贼胜；
//   ③ 主公存活且反贼与内奸全灭：若仍有野心家存活 -> 不结束（野心家还要清场）；
//      否则主公与忠臣胜。
// 项目裁定（已在 docs/identity_field_rules.md 记录）：主公阵亡且野心家随后也阵亡、
// 场上再无反贼/内奸时，回落到“反贼胜（主公阵亡）”。
void checkGameOver(const std::vector<PlayerPtr>& players, bool& gameOver, std::string& winningFaction) {
    bool lordAlive = false;
    int alive = 0;
    for (const auto& p : players) {
        if (!p->isAlive()) continue;
        ++alive;
        if (p->getIdentity() == Identity::ZHU_GONG) lordAlive = true;
    }
    int ambitors = aliveCount(players, Identity::YE_XIN_JIA);

    // ① 野心家清场：唯一幸存者
    if (ambitors > 0 && alive == ambitors) {
        gameOver = true;
        winningFaction = "野心家胜（唯一幸存者）";
        return;
    }

    if (!lordAlive) {
        if (ambitors > 0) return; // 主公阵亡但野心家仍在 -> 游戏继续
        int rebels = aliveCount(players, Identity::FAN_ZEI);
        int loyalists = aliveCount(players, Identity::ZHONG_CHEN);
        int traitors = aliveCount(players, Identity::NEI_JIAN);
        if (rebels == 0 && loyalists == 0 && traitors == 1) {
            gameOver = true;
            winningFaction = "内奸胜 (一内独存)";
        } else {
            gameOver = true;
            winningFaction = "反贼胜";
        }
        return;
    }

    if (aliveCount(players, Identity::FAN_ZEI) == 0 && aliveCount(players, Identity::NEI_JIAN) == 0 &&
        ambitors == 0) {
        gameOver = true;
        winningFaction = "主公与忠臣胜";
    }
}

std::string describeComposition(int totalPlayers) {
    RoleConfig cfg = configFor(totalPlayers);
    std::string text;
    auto append = [&text](int count, const std::string& name) {
        if (count <= 0) return;
        if (!text.empty()) text += " ";
        text += std::to_string(count) + name;
    };
    append(cfg.lords, "主公");
    append(cfg.loyalists, "忠臣");
    append(cfg.rebels, "反贼");
    append(cfg.traitors, "内奸");
    return text;
}

} // namespace Roles

} // namespace Thks
