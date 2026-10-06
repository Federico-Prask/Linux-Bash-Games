// SkillsExtra.h —— 花鬘 / 友·诸葛亮 / 势系列武将（官网原文逐字见 docs/shi_you_appendix.md）
#ifndef THKS_SKILLSEXTRA_H
#define THKS_SKILLSEXTRA_H

#include "Hero.h"
#include <string>
#include <vector>
#include <set>
#include <map>

namespace Thks {

// =====================================================================
//  花鬘（蜀，hero-detail-424）：象阵 / 芳踪 / 嬉战
//  技能名按官网原样不加前缀（官网未标注包归属，且与标包无同名技能）。
// =====================================================================

// 象阵：锁定技，【南蛮入侵】对你无效；【南蛮入侵】结算结束后，若此牌造成过伤害，你与伤害来源各摸一张牌。
class XiangZhenSkill : public StateSkill {
    CardPtr damagedNanman;      // 造成过伤害的南蛮牌
    Player* damageSourceRaw = nullptr; // 伤害来源（引擎内有效）
public:
    XiangZhenSkill();
    void onCheckCardEffect(GameEngine&, const Player&, CardPtr, bool&) override;
    void onGlobalDamage(GameEngine&, Player&, Player*, Player&, int, CardPtr) override;
    void onCardResolvedByAny(GameEngine&, Player&, Player&, CardPtr) override;
};

// 芳踪：锁定技，出牌阶段，你使用伤害类的牌不能指定你攻击范围内的角色为目标。
// 攻击范围内含有你的其他角色使用伤害类卡牌时，不能指定你为目标。
// 结束阶段，你将手牌摸至X张（X为场上存活人数）。
class FangZongSkill : public StateSkill {
public:
    FangZongSkill();
    void onCheckCardTargetAsSource(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onCheckCardTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 嬉战：锁定技，其他角色回合开始时，你须弃一张牌并令你本回合“芳踪”失效，或流失1点体力。
// 若你以此法弃置了牌，根据弃置牌的花色执行效果。
class XiZhanSkill : public StateSkill {
public:
    XiZhanSkill(); // 锁定技
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    // 方块效果：“视为对其使用一张无距离限制的火【杀】”——仅该【杀】无视距离。
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
};

// =====================================================================
//  友·诸葛亮（群，hero-detail-604）：友-演策 / 友-方遒 / 友-共砺
// =====================================================================

// 演策：每轮限一次（首轮开始时或准备阶段），二选一；卧龙演策预测系统。
class YouYanCeSkill : public ActiveSkill {
    // —— 一次“卧龙演策”的执行状态 ——
    struct Pred { bool categoryMode; int value; }; // categoryMode: true=类别(BASIC/TRICK/EQUIPMENT)，false=颜色(红/黑)
    std::vector<Pred> preds;
    size_t verified = 0;          // 已验证（被使用的牌依次填充）
    int correct = 0;
    int drawnFromMatch = 0;       // 每次执行至多因此摸五张
    bool active = false;          // 是否有进行中的预测
    int predictCount = 3;         // 可预测牌数（官网未载初始值，按3实现并披露），1..7
    bool fangqiuRevealed = false; // 方遒已展示
    // 每轮限一次
    int lastRoundActed = -1;
    void settle(GameEngine& engine, Player& self); // 预测全部验证后的结算
    void startPrediction(GameEngine& engine, Player& self); // 声明预测并执行摸取/匹配
    CardPtr drawRandomTrick(GameEngine& engine);
    CardPtr drawMatchingDeclared(GameEngine& engine);
public:
    YouYanCeSkill();
    void onRoundStart(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onAnyCardUsed(GameEngine&, Player&, Player&, CardPtr) override;
    void activate(GameEngine&, Player&); // 首轮开始/准备阶段的共用入口（每轮限一次）
    bool isPredictionActive() const { return active; }
    int currentPredictCount() const { return predictCount; }
    // 方遒再发动判定用
    bool lastSettleAllCorrect = false;
    int lastSettleTotal = 0;
};

// 方遒：限定技，执行卧龙演策后可展示预测，全部验证后效果值+1；牌数>3且全对可再次发动。
class YouFangQiuSkill : public StateSkill {
    bool spent = false;
public:
    YouFangQiuSkill();
    void onPredictionStarted(GameEngine&, Player&) override;
    bool isSpent() const { return spent; }
};

// 共砺：身份：此模式无效（原文照录）；排位、斗地主：友庞统/友徐庶 同阵营时强化“卧龙演策”。
// 本引擎支持斗地主模式，故斗地主分支按原文实现（本名册暂无友庞统、友徐庶，加入后即生效）。
class YouGongLiSkill : public StateSkill {
public:
    YouGongLiSkill();
    int gongLiPredictionCountBonus(GameEngine&, Player&) const override;       // 友庞统：可预测牌数 +1
    bool gongLiFirstPredictionAutoCorrect(GameEngine&, Player&) const override; // 友徐庶：首项预测视为正确
};

// =====================================================================
//  势·太史慈（吴，hero-detail-600）：势-酣战 / 势-战烈 / 势-振锋
// =====================================================================

class ShiHanZhanSkill : public ActiveSkill {
    int lastRoundUsed = -1;
public:
    ShiHanZhanSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    // X 修改（振锋）：0=各自体力上限
    int xMode = 0; // 0=体力上限 1=当前体力值 2=已损失体力 3=存活角色数
};

class ShiZhanLieSkill : public TriggerSkill {
    int recordX = 0;            // 本回合开始时记录的X
    int discardedShaThisTurn = 0; // 本回合已进入弃牌堆的【杀】计数（前X张）
    int currentX(GameEngine& engine, const Player& self) const;
public:
    ShiZhanLieSkill();
    int xMode = 0;              // 振锋修改
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    void onAnyCardDiscarded(GameEngine&, Player&, Player&, CardPtr) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
};

class ShiZhenFengSkill : public ActiveSkill {
    bool spent = false;
public:
    ShiZhenFengSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// =====================================================================
//  势·董昭（魏，hero-detail-602）：势-妙略 / 势-迎驾
// =====================================================================

class ShiMiaoLveSkill : public StateSkill {
    int lastRoundTriggered = -1;
public:
    ShiMiaoLveSkill();
    void onGameStart(GameEngine&, Player&) override;
    void onAfterDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

class ShiYingJiaSkill : public StateSkill {
    bool spent = false;          // 限定技：每局一次
    // 本回合锦囊名称计数（至多跟踪两个名称）
    std::string nameA, nameB;
    int usedTrickCountA = 0, usedTrickCountB = 0;
public:
    ShiYingJiaSkill();
    void onTurnEnd(GameEngine&, Player&, Player&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    void onAnyCardUsed(GameEngine&, Player&, Player&, CardPtr) override;
};

// =====================================================================
//  势·于吉（群，hero-detail-610）：势-符济 / 势-道转
// =====================================================================

class ShiFuJiSkill : public ActiveSkill {
    int lastRoundUsed = -1;
    bool selfBuff = false;      // 发动后手牌全场最少：直至你的下回合开始，第一张杀/闪带符济效果
    std::vector<int> fujiCardIds;     // 交给其他角色的“符济”牌卡牌 id
    bool selfBuffShaUsed = false;     // 自强化【杀】已用（窗口内仅一次）
    bool selfBuffShanUsed = false;    // 自强化【闪】已用（窗口内仅一次）
    CardPtr selfBuffSha;              // 带符济效果的第一张【杀】（基础伤害+1）
    CardPtr selfBuffShanCard;         // 带符济效果的第一张【闪】（结算后摸一张）
public:
    ShiFuJiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onAnyCardUsed(GameEngine&, Player&, Player&, CardPtr) override;
    // 【闪】通常作为响应“打出”而非“使用”，走 onAnyCardPlayed（引擎在 askResponseCard 中广播）。
    void onAnyCardPlayed(GameEngine&, Player&, Player&, CardPtr) override;
    void onCardResolvedByAny(GameEngine&, Player&, Player&, CardPtr) override; // 【闪】结算后使用者摸一张
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    void onCalculateShaDamage(GameEngine&, const Player&, const Player&, const Player&, CardPtr, int&) override;
    // 测试用：登记一张“符济”牌（等价于技能发动时交给其他角色的牌）。
    void markFujiCardForTesting(const CardPtr& c) { if (c) fujiCardIds.push_back(c->getId()); }
};

class ShiDaoZhuanSkill : public TriggerSkill {
    bool usedThisTurnFlag = false;          // 每回合限一次
    int roundDisabled = -1;                 // 若当前回合角色本次失去了牌：本轮次本技能失效
    std::map<std::string, int> nameRoundUsed; // 每轮每牌名限一次（牌名 -> 轮次）
    static std::string cardNameFor(CardSubType st);
public:
    ShiDaoZhuanSkill();
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override; // 每回合限一次的重置
    bool onNeedResponseCard(GameEngine&, Player&, CardSubType, CardPtr&) override;
};

// =====================================================================
//  势·辛宪英（魏，hero-detail-633）：势-诫节 / 势-清识
// =====================================================================

class ShiJieJieSkill : public TriggerSkill {
    int qingShiUsed = 0;                 // 每轮限两次：视为发动“清识”的次数
    std::set<int> watchedSeats;          // 本轮已观看过的角色（每名角色的出牌阶段限一次）
    // 本轮各角色“观看手牌”的花色数（用于“唯一最多→视为清识”）
    std::vector<std::pair<int,int>> watchedThisRound; // {playerId, suitCount}
public:
    ShiJieJieSkill();
    void onAnyPhaseStart(GameEngine&, Player&, Player&, TurnPhase) override;
    void onRoundStart(GameEngine&, Player&) override;
};

class ShiQingShiSkill : public TriggerSkill {
public:
    ShiQingShiSkill();
    void onAfterDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

// =====================================================================
//  势·鲁肃（吴，hero-detail-638）：势-好施 / 势-缔盟
// =====================================================================

class ShiHaoShiSkill : public StateSkill {
    int lostLastTurnCount = 0;   // “前两次因此失去最后的手牌”已触发次数（本局前两次）
public:
    ShiHaoShiSkill();
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    // “因此失去最后的手牌”＝借出的手牌被对方使用或打出（不是任何成批失去牌）
    void onBorrowedHandCardUsed(GameEngine&, Player&, Player&, CardPtr) override;
};

class ShiDiMengSkill : public ActiveSkill {
    int lastRoundUsed = -1;
public:
    ShiDiMengSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// =====================================================================
//  势·钟会（魏，hero-detail-640）：势-肆恣 / 势-挟志 / 势-迂难 / 势-克昌
// =====================================================================

class ShiSiZhiSkill : public ActiveSkill {
    int lastRoundUsed = -1;
    bool siZhiActive = false;
    int siZhiTurns = 0;
    bool siZhiEffect1 = false, siZhiEffect2 = false, siZhiEffect3 = false;
    void endSiZhi(GameEngine& engine, Player& self, const std::string& why);
public:
    ShiSiZhiSkill();
    void onGameStart(GameEngine&, Player&) override;
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onTurnEnd(GameEngine&, Player&, Player&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    void onCalculateShaDamage(GameEngine&, const Player&, const Player&, const Player&, CardPtr, int&) override;
};

class ShiXieZhiSkill : public TriggerSkill {
public:
    ShiXieZhiSkill();
    void onHpChanged(GameEngine&, Player&, int) override;
    void onCalculateHandLimit(GameEngine&, const Player&, int&) override;
    void onCalculateShaLimit(GameEngine&, const Player&, int&) override;
};

class ShiYuNanSkill : public StateSkill {
    bool awakened = false;
    void tryAwaken(GameEngine& engine, Player& self, Player& dyingPlayer);
public:
    ShiYuNanSkill();
    void onOtherDying(GameEngine&, Player&, Player&) override;
    void onDying(GameEngine&, Player&, Player&) override;
};

class ShiKeChangSkill : public StateSkill {
    int level = 1;
public:
    ShiKeChangSkill();
    void upgrade() { level = 2; }
    int getLevel() const { return level; }
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
};

// =====================================================================
//  势·邓艾（魏，hero-detail-643）：势-屯田 / 势-凿险 / 势-急袭
// =====================================================================

class ShiTunTianSkill : public ActiveSkill {
    int lastRoundUsed = -1;
    void gainCharge(GameEngine& engine, Player& self, CardPtr card);
public:
    ShiTunTianSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onCardLostOutsideTurn(GameEngine&, Player&, CardPtr) override;
    void onAnyCardLost(GameEngine&, Player&, CardPtr) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

class ShiZaoXianSkill : public TriggerSkill {
public:
    ShiZaoXianSkill();
    void onChargeConsumed(GameEngine&, Player&, int) override;
};

class ShiJiXiSkill : public TriggerSkill {
    std::vector<int> targetsThisTurn; // 本回合成为过你牌目标的角色 id
public:
    ShiJiXiSkill();
    void onTurnEnd(GameEngine&, Player&, Player&) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr,
                               const std::vector<PlayerPtr>&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// =====================================================================
//  势·孙綝（吴，hero-detail-660）：势-逆固 / 势-戮连
// =====================================================================

class ShiNiGuSkill : public ActiveSkill {
    int lastRoundUsed = -1;
    int niGuCharges = 0;
public:
    ShiNiGuSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onDealDamage(GameEngine&, Player&, Player&, int&, ShaElement, CardPtr) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

class ShiLuLianSkill : public StateSkill {
public:
    ShiLuLianSkill();
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
};

// =====================================================================
//  势·周瑜（吴，hero-detail-662）：势-炽沄 / 势-焰洄 / 势-焚涛 / 势-雄姿
// =====================================================================

class ShiChiYunSkill : public ActiveSkill {
    int lastStageGained = -1;   // 每阶段首次获得牌
    int stageSerial = 0;
    bool gotThisStage = false;  // 本阶段已触发过（每阶段首次）
    bool pending = false;       // 等待主动发动
public:
    ShiChiYunSkill();
    bool canActivate(GameEngine&, Player&) override;
    bool canActivateOutsidePlayPhase() const override { return true; }
    void onActivate(GameEngine&, Player&) override;
    void onCardsObtained(GameEngine&, Player&, int) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    bool aiShouldActivate(GameEngine&, Player&) override; // 阶段首次获得牌后的“是否交给他人”
    int pendingStage = -1;      // 已提示“可发动”的阶段序号（-1=无）
};

class ShiYanHuiSkill : public TriggerSkill {
    std::set<int> shownPlayers;       // 本回合展示过牌的角色
    std::set<int> lostPlayers;        // 本阶段因此因弃置而失去过牌的角色
    std::set<int> shownCardThisTurn;  // 本回合已被展示过的牌 id
public:
    ShiYanHuiSkill();
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

class ShiFenTaoSkill : public StateSkill {
    int fenTaoBoostTarget = -1;    // 选择传导+1的目标
    int fenTaoRelinkTarget = -1;   // 选择弃牌后重连的目标
public:
    ShiFenTaoSkill();
    void onDamageTakingByAny(GameEngine&, Player&, Player*, Player&, int&, ShaElement, CardPtr) override;
    void onTakeDamage(GameEngine&, Player&, Player*, int&, ShaElement) override;
};

class ShiXiongZiSkill : public StateSkill {
    bool spent = false;
public:
    ShiXiongZiSkill();
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// =====================================================================
//  势·田丰（群，hero-detail-623）：势-刚鲠 / 势-死谏
// =====================================================================

// 刚鲠：出牌阶段限一次，将至少两张手牌交给一名其他角色；回合结束时按对手牌数摸一张或弃其一张。
class ShiGangGengSkill : public ActiveSkill {
    int targetId = -1;
public:
    ShiGangGengSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 死谏：失去最后手牌/进入濒死时二选一，可背水。
class ShiSiJianSkill : public TriggerSkill {
    int usedThisTurn = 0;
    int beishuiCount = 0;   // 本局发动过背水的次数（X 取发动前的次数）
public:
    ShiSiJianSkill();
    void onHandEmpty(GameEngine&, Player&) override;
    void onDying(GameEngine&, Player&, Player&) override;
    void onAnyCardUsed(GameEngine&, Player&, Player&, CardPtr) override;
    void resetTurnState() override { usedThisTurn = 0; }
private:
    void tryActivate(GameEngine& engine, Player& self, bool allowBeishui);
};

// =====================================================================
//  势·黄祖（群，hero-detail-620）：势-鸱张 / 势-断鞅
// =====================================================================

// 鸱张：伤害牌无距离限制；指定目标后可弃牌封锁同色响应。
class ShiChiZhangSkill : public TriggerSkill {
    bool blocking = false;
    bool blockRed = false;
    bool blockBlack = false;
public:
    ShiChiZhangSkill();
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr, const std::vector<PlayerPtr>&) override;
    void onCheckResponseCard(const GameEngine&, const Player&, const Player&, CardPtr, CardSubType, bool&) const override;
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
private:
    void askBlock(GameEngine& engine, Player& self, CardPtr card);
};

// 断鞅：手牌不因使用进入弃牌堆时收【杀】；阶段结束时无次数限制使用；造成伤害后重铸并摸四张。
class ShiDuanYangSkill : public TriggerSkill {
    int usedThisTurn = 0;
    int pilePhase = -1;     // 收取时所处的阶段
public:
    ShiDuanYangSkill();
    void onAnyCardDiscarded(GameEngine&, Player&, Player&, CardPtr) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    bool canUseShaBeyondLimitOn(GameEngine&, const Player&, const Player&, CardPtr) override;
    void onAfterDealDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
    void resetTurnState() override { usedThisTurn = 0; }
};

} // namespace Thks

#endif // THKS_SKILLSEXTRA_H
