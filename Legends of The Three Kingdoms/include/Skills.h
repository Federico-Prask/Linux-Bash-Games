#ifndef THKS_SKILLS_H
#define THKS_SKILLS_H

// =====================================================================
//  具体技能实现（第二层继承）—— 技能文本以《三国杀移动版》(手杀) 官网为准
//
//  Skill ─┬─ ActiveSkill  ─┬─ WuShengSkill   武圣（关羽 / 界关羽）
//         │                ├─ LongDanSkill   龙胆（赵云 / 界赵云）
//         │                ├─ YiJueSkill     义绝（界关羽）
//         │                └─ PaiYiSkill     排异（钟会·觉醒后获得）
//         ├─ TriggerSkill ─┬─ TieQiSkill     铁骑（马超）
//         │                ├─ JieTieQiSkill  铁骑（界马超）
//         │                ├─ LieGongSkill   烈弓（黄忠）
//         │                ├─ JieLieGongSkill 烈弓（界黄忠）
//         │                ├─ TiShenSkill    替身（界张飞）
//         │                ├─ YaJiaoSkill    涯角（界赵云）
//         │                ├─ QuanJiSkill    权计（钟会）
//         │                └─ ZiLiSkill      自立（钟会，觉醒技）
//         └─ StateSkill   ─┬─ PaoXiaoSkill   咆哮（张飞 / 界张飞，锁定技）
//                          └─ MaShuSkill     马术（马超 / 界马超，锁定技）
// =====================================================================

#include "Hero.h"

namespace Thks {

// ------------------------- 主动技 -------------------------

// 武圣：你可以将一张红色牌当【杀】使用或打出。
// 界限突破追加：你使用方块【杀】无距离限制。
class WuShengSkill : public ActiveSkill {
    bool jie;
public:
    explicit WuShengSkill(bool jieVersion = false);
    bool isConversionSkill() const override { return true; }
    CardPtr convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) override;
    void onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr sha, bool& canTarget) override;
};

// 龙胆：你可以将一张【杀】当【闪】、【闪】当【杀】使用或打出。
class LongDanSkill : public ActiveSkill {
public:
    LongDanSkill();
    bool isConversionSkill() const override { return true; }
    CardPtr convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) override;
};

// 义绝（界关羽）：出牌阶段限一次，你可以弃置一张牌，然后令一名其他角色展示一张手牌。
// 若此牌为黑色，则其本回合非锁定技失效且不能使用或打出手牌，你对其使用的红桃【杀】伤害+1；
// 若此牌为红色，则你获得之，然后你可令该角色回复1点体力。
class YiJueSkill : public ActiveSkill {
public:
    YiJueSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
    void onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) override; // 红桃【杀】伤害+1
};

// 排异（钟会觉醒后获得）：出牌阶段限一次，你可以将一张"权"置入弃牌堆并选择一名角色，
// 然后其摸两张牌。若该角色的手牌多于你，则你对其造成1点伤害。
class PaiYiSkill : public ActiveSkill {
public:
    explicit PaiYiSkill(bool jieVersion = false);
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// DIY 钟会觉醒后独立的【排异】，不复用普通/界钟会的技能对象。
class DiyPaiYiSkill : public ActiveSkill {
public:
    DiyPaiYiSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// ------------------------- 触发技 -------------------------

// 铁骑（马超）：当你使用【杀】指定一个目标后，你可以进行判定，若结果为红色，该角色不能使用【闪】响应此【杀】。
class TieQiSkill : public TriggerSkill {
public:
    TieQiSkill();
    void onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) override;
};

// 铁骑（界马超）：当你使用【杀】指定一个目标后，你可令其本回合内非锁定技失效，然后你进行判定，
// 除非该角色弃置与结果花色相同的一张牌，否则不能使用【闪】响应此【杀】。
class JieTieQiSkill : public TriggerSkill {
public:
    JieTieQiSkill();
    void onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) override;
};

// 烈弓（黄忠）：当你于出牌阶段内使用【杀】指定一个目标后，若该角色的手牌数不小于你的体力值
// 或不大于你的攻击范围，则你可以令其不能使用【闪】响应此【杀】。
class LieGongSkill : public TriggerSkill {
public:
    LieGongSkill();
    void onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) override;
};

// 烈弓（界黄忠）：你使用【杀】可以选择与你距离不大于此【杀】点数的角色为目标。
// 当你使用【杀】指定目标后：若其手牌数不大于你，你可以令其不能使用【闪】响应此【杀】；
// 若其体力值不小于你，你可以令此【杀】伤害+1。
class JieLieGongSkill : public TriggerSkill {
public:
    JieLieGongSkill();
    void onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr sha, bool& canTarget) override;
    void onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) override;
};

// 替身（界张飞）：出牌阶段结束时，你可发动此技能。你弃置所有锦囊牌和坐骑牌。
// 然后直到你的下回合开始，获得所有以你为目标且未对你造成伤害的【杀】。
class TiShenSkill : public TriggerSkill {
public:
    TiShenSkill();
    void onTurnStart(GameEngine& engine, Player& self) override;
    void onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) override;
    void onShaFinished(GameEngine& engine, Player& self, ShaContext& ctx) override;
};

// 涯角（界赵云）：当你于回合外使用或打出手牌时，你可以展示牌堆顶一张牌并将其交给任意一名角色。
// 若这两张牌类别不同，你弃置一张牌。
class YaJiaoSkill : public TriggerSkill {
public:
    YaJiaoSkill();
    void onCardUsedOutsideTurn(GameEngine& engine, Player& self, CardPtr card) override;
};

// 权计（钟会）：当你受到1点伤害后，你可以摸一张牌。若如此做，你将一张手牌置于武将牌上，称为"权"；
// 你的手牌上限+X（X为"权"的数量）。
class QuanJiSkill : public TriggerSkill {
    bool jie;
public:
    explicit QuanJiSkill(bool jieVersion = false);
    void onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) override;
    void onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage, ShaElement element, CardPtr cause) override;
    void onCalculateHandLimit(GameEngine& engine, const Player& self, int& handLimit) override;
};

// 自立（钟会）：觉醒技，准备阶段，若"权"的数量不小于3，你选择一项：1.回复1点体力；2.摸两张牌。
// 然后你减1点体力上限，获得"排异"（界版 368 原文为“获得技能‘排异※’”，分别照录）。
class ZiLiSkill : public TriggerSkill {
    bool awakened;
public:
    explicit ZiLiSkill(bool jieVersion = false);
    bool isAwakened() const { return awakened; }
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) override;
};

// ------------------------- 状态技 -------------------------

// 咆哮：锁定技，你使用【杀】无次数限制。
// 界限突破追加：你的出牌阶段，若你于当前阶段内使用过【杀】，你于此阶段使用【杀】无距离限制。
class PaoXiaoSkill : public StateSkill {
    bool jie;
public:
    explicit PaoXiaoSkill(bool jieVersion = false);
    void onCalculateShaLimit(GameEngine& engine, const Player& self, int& shaLimit) override;
    void onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr sha, bool& canTarget) override;
};

// 马术：锁定技，你计算与其他角色的距离-1。
class MaShuSkill : public StateSkill {
public:
    MaShuSkill();
    void onCalculateDistance(GameEngine& engine, const Player& self, const Player& target, int& distance) override;
};

// ------------------------- DIY 钟会专属（持恒技组） -------------------------
//
//  权谋（持恒技）：蓄力技（1/5）。游戏开始时视为首轮开始，获得1点蓄力；此后每轮开始时获得2点蓄力，
//    你的出牌阶段开始时获得1点蓄力，至多5点。当满足以下条件时，你可以发动一次【权计】：
//    1、当你造成伤害后（一回合一次），或每受到1点伤害后；
//    2、一名玩家的出牌阶段结束时，弃置两张牌，以此法发动。
//  权计（持恒技）：出牌阶段限一次，摸一张牌并将一张手牌置于武将牌上，称为“权”；
//    每次发动【权计】后，若你有蓄力点，你消耗1点蓄力并摸一张牌；你的手牌上限+“权”的数量。
//  自立（持恒技·觉醒技）：效果不变，觉醒后获得【排异】。
class QuanMouSkill : public StateSkill {
    int roundsSeen;     // 游戏开始的首轮开始事件计为第1轮
    bool dealTriggered; // 本回合是否已因“造成伤害”发动过
public:
    static constexpr int MAX_CHARGE = 5;
    QuanMouSkill();
    int getCharge(const Player& self) const;
    bool isDealTriggered() const { return dealTriggered; }
    void onGameStart(GameEngine& engine, Player& self) override;
    void onRoundStart(GameEngine& engine, Player& self) override;
    void onTurnStart(GameEngine& engine, Player& self) override;
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) override;
    void onAfterDealDamage(GameEngine& engine, Player& self, Player* target, int damage, ShaElement element, CardPtr cause) override;
    void onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage, ShaElement element, CardPtr cause) override;
    void onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) override;
    void resetTurnState() override { dealTriggered = false; }
};

// 权计（持恒版）：出牌阶段限一次的主动发动 + 手牌上限修正
class QuanJiConstSkill : public StateSkill {
    int usesThisTurn;
public:
    QuanJiConstSkill();
    bool isUsableActively() const override { return true; }
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
    void onCalculateHandLimit(GameEngine& engine, const Player& self, int& handLimit) override;
    void resetTurnState() override { usesThisTurn = 0; }
};

// 自立（持恒版·觉醒技）：效果与 ZiLiSkill 完全一致，仅技能种类为状态技（持恒技）
class ZiLiConstSkill : public StateSkill {
    bool awakened;
public:
    ZiLiConstSkill();
    bool isAwakened() const { return awakened; }
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) override;
};

// 常量：钟会"权"的牌堆名
extern const char* const PILE_QUAN;

} // namespace Thks

#endif // THKS_SKILLS_H
