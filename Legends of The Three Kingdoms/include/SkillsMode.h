// SkillsMode.h —— 玩法专属技能（斗地主 + 身份场新机制）
// 文本来源：
//   ① 三国杀移动版官网口径（用户提供原文，2026-10-04）：
//      地主【飞扬】判定阶段，可弃2张牌，移除判定区的一张牌（如【乐不思蜀】），每回合限1次。
//      地主【跋扈】锁定技，准备阶段摸1牌，出牌阶段可多使用1张【杀】。
//      农民【共苦】当队友阵亡时，存活的农民可选择回复1点体力或摸2张牌。
//   ② 身份场新机制（用户提供原文，2026-10-05，含当日第二次细化）：内奸【择途】、主公【立储】。
//      择途·自立为主后获得的〖飞扬〗〖跋扈〗就是上面的地主版技能（用户明确不改文本与实现）。
// 详见 docs/doudizhu_rules.md 与 docs/identity_field_rules.md。
#pragma once
#include "Hero.h"

namespace Thks {

// 地主专属：飞扬——判定阶段弃 2 张牌，移除自己判定区的一张牌；每回合限 1 次。
class DouFeiYangSkill : public TriggerSkill {
public:
    DouFeiYangSkill();
    void onTurnStart(GameEngine& engine, Player& self) override;                        // 每回合重置
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skip) override;
};

// 地主专属：跋扈——锁定技，准备阶段摸 1 张牌，出牌阶段可多使用 1 张【杀】。
class DouBaHuSkill : public TriggerSkill {
public:
    DouBaHuSkill();
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skip) override;
    void onCalculateShaLimit(GameEngine& engine, const Player& self, int& shaLimit) override;
};

// 农民专属：共苦——当队友阵亡时，存活的农民可选择回复 1 点体力或摸 2 张牌。
class DouGongKuSkill : public TriggerSkill {
public:
    DouGongKuSkill();
    void onPlayerDeath(GameEngine& engine, Player& self, Player& dead, Player* killer) override;
};

// 按斗地主身份分发模式技能（地主：飞扬 + 跋扈；农民：共苦）。
void applyDoudizhuModeSkills(GameEngine& engine, Player& player);

// ==================== 身份场新机制（用户 2026-10-05 提供规则） ====================

// 内奸【择途】（用户 2026-10-05 第二次细化的原文）：
//   人数要求：场上存活的角色数量大于 4 人时才能发动。
//   发动时机：每局游戏限一次。你可以在**自己的回合内任意时刻**主动发动。
//   操作方式：发动时，你需要**明置自己的身份牌**，并立即选择“侍奉明主”或“自立为主”：
//     1. 侍奉明主——你的身份牌会直接变为忠臣（胜利条件同步变更为主忠方）；
//     2. 自立为主——你成为独立的“野心家”，获得〖飞扬〗〖跋扈〗（**即斗地主地主的那两个技能**，
//        用户 2026-10-05 明确：“飞扬和跋扈还是我之前说的技能”——判定阶段弃 2 张牌移除判定区一张牌 /
//        锁定技准备阶段摸 1 牌且出牌阶段可多用 1 张【杀】，文本与实现均不改）；
//        即使主公死亡，你也不会立即失败，可以继续游戏，目标是击败场上所有其他角色、成为唯一的幸存者。
// 工程落实（近似口径见 docs/identity_field_rules.md）：“自己回合内任意时刻”＝回合开始 +
// 自己每个阶段开始（AI 静默评估，人类在准备阶段与弃牌阶段被询问）+ 出牌阶段主动技菜单 +
// 自己回合内有角色阵亡后；一局一次（标记“择途已用”，属公开状态）。
class ZheTuSkill : public ActiveSkill {
public:
    ZheTuSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    bool canActivateOutsidePlayPhase() const override { return true; } // 不限出牌阶段
    void onTurnStart(GameEngine& engine, Player& self) override;       // 自己的回合开始
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skip) override; // 自己回合内各阶段
    void onPlayerDeath(GameEngine& engine, Player& self, Player& dead, Player* killer) override; // 自己回合内有角色阵亡
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
    // AI 的道路选择：-1 不发动 / 0 侍奉明主 / 1 自立为主
    static int aiChoice(GameEngine& engine, Player& self);
    // 统一询问入口（人类）：确认后立即发动
    void offer(GameEngine& engine, Player& self, const std::string& when);

protected:
    void onActivate(GameEngine& engine, Player& self) override;
};

// 主公【立储】：第一轮的任意角色结束阶段，明选一名角色为“储君”（太子）。
// 主公阵亡时，储君若为忠臣则继位成为新的主公，延续主忠方的游戏。
class LiChuSkill : public TriggerSkill {
public:
    LiChuSkill();
    void onAnyPhaseEnd(GameEngine& engine, Player& self, Player& turnOwner, TurnPhase phase) override;
    // AI 选择储君：优先忠臣（体力高、手牌多者）；没有忠臣则不立储
    static PlayerPtr aiChooseHeir(GameEngine& engine, Player& self);
};

// 玩法赋予的模式技能名（飞扬/跋扈/共苦/择途/立储）：不属于武将自身技能，
// 因此不能被“失去一个技能”“与你有相同技能”这类按武将技能判定的效果选中。
bool isModeSkillName(const std::string& name);

// 身份场模式技能分发：内奸→择途；主公→立储（其余身份无模式技能）
void applyIdentityModeSkills(GameEngine& engine, Player& player);
// 野心家（择途·自立为主）获得【飞扬】【跋扈】＝斗地主地主的同名同效技能（用户确认不改）
void applyAmbitionSkills(GameEngine& engine, Player& player);

} // namespace Thks
