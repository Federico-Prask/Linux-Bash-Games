#ifndef THKS_HERO_H
#define THKS_HERO_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>
#include "Card.h"

namespace Thks {

class GameEngine;
class Player;
using PlayerPtr = std::shared_ptr<Player>;

enum class Country {
    WEI, // 魏
    SHU, // 蜀
    WU,  // 吴
    QUN, // 群
    GOD  // 神
};

enum class Gender {
    MALE,  // 男
    FEMALE // 女
};

enum class TurnPhase {
    NONE,        // 回合外
    PREPARATION, // 准备阶段
    JUDGEMENT,   // 判定阶段
    DRAW,        // 摸牌阶段
    PLAY,        // 出牌阶段
    DISCARD,     // 弃牌阶段
    FINISH       // 结束阶段
};

// =====================================================================
//  技能分类（依据游卡官方规则集）
//
//  一、按"发动方式"分为三大种类（互斥，对应第一层继承）：
//      主动技  ActiveSkill  —— 由玩家主动发动（含"将X当Y使用或打出"的视为转化）
//      触发技  TriggerSkill —— 具有发动时机，满足条件时发动
//      状态技  StateSkill   —— 无发动时机，持续生效（技能分类；与移动版“持恒技”标签分开建模）
//
//  二、标签（可叠加，位掩码）：锁定技 / 限定技 / 觉醒技 / 主公技 / 转换技 / 持恒技
//      持恒技是独立的官方标签，不等同于 SkillKind::STATE；本引擎依其特殊规则免于失效/失去。
//      例：马术 = 状态技 + 锁定技；自立 = 触发技 + 觉醒技（觉醒技视为附带锁定+限定）
// =====================================================================
enum class SkillKind {
    ACTIVE,  // 主动技
    TRIGGER, // 触发技
    STATE    // 状态技（持续生效，但不自动等同于“持恒技”标签）
};

namespace SkillTag {
    enum : unsigned {
        NONE    = 0,
        LOCK    = 1u << 0, // 锁定技
        LIMITED = 1u << 1, // 限定技
        AWAKEN  = 1u << 2, // 觉醒技
        LORD      = 1u << 3, // 主公技
        SWITCH    = 1u << 4, // 转换技
        SUSTAINED = 1u << 5  // 持恒技：不因技能失效效果失效；不可移除按本项目需求建模
    };
}

// 【杀】结算上下文：从"指定目标后"到"结算结束"期间在各技能钩子之间传递
struct ShaContext {
    PlayerPtr source;
    PlayerPtr target;
    CardPtr card;
    bool cannotDodge = false;
    int extraResponseCost = 0; // 响应此【杀】需额外弃置一张牌方可响应（势-战烈）
    bool invalidTarget = false; // 享乐等在指定目标后令此杀对目标无效   // 目标不能使用【闪】响应（铁骑/烈弓）
    int extraDamage = 0;        // 额外伤害（界烈弓/界义绝）
    bool dodgedByShan = false;  // 目标使用/视为使用【闪】抵消此【杀】
    bool hit = false;           // 是否造成了伤害
    int hpBeforeDamage = -1;    // 进入伤害结算前目标的体力（替身判断“未造成伤害”，-1 表示未进入伤害结算）
    bool cardClaimed = false;   // 【杀】的实体牌是否已被某技能获得（替身），为真则不进弃牌堆
    PlayerPtr redirect = nullptr;   // 重定向目标（如【流离】）
};

// ---------------------------------------------------------------------
//  技能抽象基类：所有事件钩子默认空实现，子类按需覆盖
// ---------------------------------------------------------------------
class Skill {
protected:
    std::string name;
    std::string description;
    SkillKind kind;
    unsigned tags;

    // 所有主动发动统一经过公开的 activate() 守卫；具体效果只能在该内部钩子中实现，
    // 不得让派生类重写公开入口绕过合法性检查。
    virtual void onActivate(GameEngine& /*engine*/, Player& /*self*/) {}
    // 少数事件结算型主动技（如势-炽沄）可在非出牌阶段由已验证的事件内部发动。
    virtual bool canActivateOutsidePlayPhase() const { return false; }

    // 官网以“持恒技”标注该技能类型；标签可能与限定技/主公技等并列。
    // 只检查描述首个完整句号之前的技能标签区，避免把正文中提到的其他持恒技误判。
    static bool declaresSustainedTag(const std::string& text) {
        const auto marker = text.find("持恒技");
        if (marker == std::string::npos) return false;
        auto end = text.find("。");
        const auto semicolon = text.find("；");
        const auto newline = text.find('\n');
        if (semicolon != std::string::npos && (end == std::string::npos || semicolon < end)) end = semicolon;
        if (newline != std::string::npos && (end == std::string::npos || newline < end)) end = newline;
        return end == std::string::npos || marker < end;
    }

public:
    Skill(const std::string& name, const std::string& description, SkillKind kind, unsigned tags = SkillTag::NONE)
        : name(name), description(description), kind(kind), tags(tags) {
        if (declaresSustainedTag(description)) this->tags |= SkillTag::SUSTAINED;
    }

    virtual ~Skill() = default;

    std::string getName() const { return name; }
    std::string getDescription() const { return description; }
    SkillKind getKind() const { return kind; }
    unsigned getTags() const { return tags; }
    bool hasTag(unsigned tag) const { return (tags & tag) != 0; }
    bool declaresSustainedTagInDescription() const { return declaresSustainedTag(description); }
    bool isSustained() const { return hasTag(SkillTag::SUSTAINED); }
    // 锁定技、觉醒技和持恒技均不会因“非锁定技失效”而失效。
    bool isLocked() const { return hasTag(SkillTag::LOCK) || hasTag(SkillTag::AWAKEN); }
    // 混合技能在非锁定部分失效时仍须保留锁定部分或持恒部分。
    virtual bool hasEffectiveLockedComponent() const { return isLocked() || isSustained(); }

    std::string getKindString() const;   // "主动技" / "触发技" / "状态技"
    std::string getTagString() const;    // "锁定技" / "觉醒技" / "持恒技" ...（可能为空）
    std::string getFullDescription() const; // 【武圣】(主动技) 描述...

    // ---- 回合流程 ----
    virtual void onGameStart(GameEngine& /*engine*/, Player& /*self*/) {}
    // 一名角色的回合开始时（广播给所有存活角色）
    virtual void onTurnStart(GameEngine& /*engine*/, Player& /*self*/) {}
    virtual void onTurnEnd(GameEngine& /*engine*/, Player& /*self*/, Player& /*turnOwner*/) {}
    // 持续状态到期不受技能暂时失效影响；仅清理旧效果，不触发新的技能。
    virtual void onTurnBoundary(GameEngine& /*engine*/, Player& /*self*/, Player& /*turnOwner*/, bool /*starting*/) {}
    // 一轮开始时（广播给所有存活角色）
    virtual void onRoundStart(GameEngine& /*engine*/, Player& /*self*/) {}
    // 主公技等借由其他角色在其自己的出牌阶段发动；owner 与 actor 分离。
    virtual bool canDelegate(GameEngine& /*engine*/, Player& /*owner*/, Player& /*actor*/) { return false; }
    virtual void invokeDelegated(GameEngine& /*engine*/, Player& /*owner*/, Player& /*actor*/) {}
    virtual void onPhaseStart(GameEngine& /*engine*/, Player& /*self*/, TurnPhase /*phase*/, bool& /*skipPhase*/) {}
    virtual void onPhaseEnd(GameEngine& /*engine*/, Player& /*self*/, TurnPhase /*phase*/) {}
    // 任一角色跳过阶段后广播（区别于摸牌数改为零）。
    virtual void onPhaseSkipped(GameEngine& /*engine*/, Player& /*self*/, Player& /*owner*/, TurnPhase /*phase*/) {}

    // ---- 数值/规则修正（多为状态技） ----
    virtual void onDrawCards(GameEngine& /*engine*/, Player& /*self*/, int& /*drawCount*/) {}
    virtual void onCalculateHandLimit(GameEngine& /*engine*/, const Player& /*self*/, int& /*handLimit*/) {}
    virtual void onCalculateDistance(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*target*/, int& /*distance*/) {}
    virtual void onCalculateShaLimit(GameEngine& /*engine*/, const Player& /*self*/, int& /*shaLimit*/) {}
    // 杀指定目标的总数上限（默认 1）；如天义/方天画戟可叠加。
    virtual void onCalculateShaTargets(GameEngine& /*engine*/, const Player& /*self*/, CardPtr /*sha*/, int& /*limit*/) {}
    virtual void onCalculateTieSuoTargetLimit(GameEngine& /*engine*/, const Player& /*self*/, int& /*limit*/) {}
    virtual void onCalculateShaCardLimit(GameEngine& /*engine*/, const Player& /*self*/,
                                         CardPtr /*sha*/, int& /*limit*/) {}
    // 按目标例外免除【杀】次数限制（例如谋·武圣只对被指定角色无次数限制）。
    virtual bool canUseShaBeyondLimitOn(GameEngine& /*engine*/, const Player& /*self*/,
                                        const Player& /*target*/, CardPtr /*sha*/) { return false; }
    // 使用【杀】时是否可以指定该目标（无视距离类效果），canTarget 默认由距离/攻击范围计算得出
    virtual void onCheckShaTarget(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*target*/, CardPtr /*sha*/, bool& /*canTarget*/) {}

    // ---- 视为转化：若本技能能把 card 当作 wanted 类型使用/打出，则返回转化牌，否则返回 nullptr ----
    virtual CardPtr convertCard(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/, CardSubType /*wanted*/) { return nullptr; }
    // 一个实体素材可以声明多种同类型牌（如蛊惑的普通/火/雷【杀】）。
    virtual std::vector<CardPtr> convertCards(GameEngine& engine, Player& self,
                                                CardPtr card, CardSubType wanted) {
        auto result=convertCard(engine,self,card,wanted);
        return result ? std::vector<CardPtr>{result} : std::vector<CardPtr>{};
    }

    // ---- 【杀】结算 ----
    // 你使用【杀】指定目标后
    virtual void onShaTargeted(GameEngine& /*engine*/, Player& /*self*/, ShaContext& /*ctx*/) {}
    virtual void onDuelTargeted(GameEngine& /*engine*/, Player& /*self*/, Player& /*source*/, Player& /*target*/) {}
    // 以你为目标的【杀】结算结束后
    virtual void onShaFinished(GameEngine& /*engine*/, Player& /*self*/, ShaContext& /*ctx*/) {}

    // ---- 伤害 ----
    virtual void onTakeDamage(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/, int& /*damage*/, ShaElement /*element*/) {}
    // 伤害牌和认领状态随伤害转移；旧钩子由缺省实现继续调用。
    virtual void onTakeDamageFromCard(GameEngine& engine, Player& self, Player* source,
                                      int& damage, ShaElement element, CardPtr /*cause*/, bool* /*claimed*/) {
        onTakeDamage(engine,self,source,damage,element);
    }
    // 伤害结算（cause 为造成此伤害的牌，可能为空）
    virtual void onDealDamage(GameEngine& /*engine*/, Player& /*self*/, Player& /*target*/, int& /*damage*/, ShaElement /*element*/, CardPtr /*cause*/) {}
    // 伤害结算前可认领造成伤害的牌（如【奸雄】）；返回 true 表示获得之，该牌不再进入弃牌堆
    virtual bool onClaimDamageCard(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/, CardPtr /*cause*/) { return false; }
    // 受到伤害后（已完成扣血与濒死结算，且自己仍存活）
    // 伤害后（self 为受伤者）
    virtual void onAfterDamage(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/, int /*damage*/, ShaElement /*element*/, CardPtr /*cause*/) {}
    // 扣血后、求桃前结算的伤害标记；死亡伤害亦可触发。
    virtual void onDamageApplied(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/,
                                 Player& /*target*/, int /*damage*/, CardPtr /*cause*/) {}
    // 伤害后（self 为伤害来源，与 onAfterDamage 对称）
    virtual void onAfterDealDamage(GameEngine& /*engine*/, Player& /*self*/, Player* /*target*/, int /*damage*/, ShaElement /*element*/, CardPtr /*cause*/) {}
    virtual void onGlobalDamage(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/, Player& /*target*/, int /*damage*/, CardPtr /*cause*/) {}
    virtual void onDying(GameEngine& /*engine*/, Player& /*self*/, Player& /*dyingPlayer*/) {}
    // 其他角色进入濒死状态时广播（self 为技能持有者，dying 为濒死者；如谋·贾诩【完杀】）
    virtual void onOtherDying(GameEngine& /*engine*/, Player& /*self*/, Player& /*dyingPlayer*/) {}
    // 任何角色使用【桃】结算后广播（self 为技能持有者，user 为使用者，target 为回复对象；如谋·孙权【救援】）
    virtual void onAnyPeachUsed(GameEngine& /*engine*/, Player& /*self*/, Player& /*user*/, Player& /*target*/) {}
    // 其他角色使用一张牌进入结算前广播；cancelled=true 时该牌被令无效（如谋·诸葛亮【看破】）
    virtual void onOtherCardUsedBefore(GameEngine& /*engine*/, Player& /*self*/, Player& /*user*/, CardPtr /*card*/, bool& /*cancelled*/) {}
    // 计算“其他角色到你”的距离时广播（self 为目标侧技能持有者，otherFrom 为来源；如谋·义从方向二）
    virtual void onCalculateDistanceToYou(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*otherFrom*/, int& /*distance*/) {}
    // 一次获得 n 张牌后广播（含摸牌/交换；self 为获得者；如谋·诸葛瑾【弘援】）
    virtual void onCardsObtained(GameEngine& /*engine*/, Player& /*self*/, int /*count*/) {}
    // 任意角色一次获得牌后广播（self 为技能持有者；recipient 为获得者；如谋·法正【眩惑】）
    virtual void onAnyCardsObtained(GameEngine& /*engine*/, Player& /*self*/, Player& /*recipient*/, int /*count*/) {}
    // 任意角色获得一张具体牌后广播（self 为技能持有者；recipient 为获得者）
    virtual void onAnyCardObtained(GameEngine& /*engine*/, Player& /*self*/, Player& /*recipient*/, CardPtr /*card*/) {}
    // 一名角色一次性失去 n 张牌后广播（n≥2 才有意义；self 为技能持有者，victim 为失去者；如【弘援】）
    virtual void onCardsLostBatch(GameEngine& /*engine*/, Player& /*self*/, Player& /*victim*/, int /*count*/) {}
    // 任何角色使用一张牌时广播（self 为技能持有者，user 为使用者；如友·诸葛亮【演策】验证、势·于吉【符济】）
    virtual void onAnyCardUsed(GameEngine& /*engine*/, Player& /*self*/, Player& /*user*/, CardPtr /*card*/) {}
    // 一张牌进入弃牌堆时广播（self 为技能持有者，owner 为原持有者；如势·太史慈【战烈】）
    virtual void onAnyCardDiscarded(GameEngine& /*engine*/, Player& /*self*/, Player& /*owner*/, CardPtr /*card*/) {}
    // 一次性消耗 n 点蓄力点后广播（self 为消耗者；如势·邓艾【凿险】）
    virtual void onChargeConsumed(GameEngine& /*engine*/, Player& /*self*/, int /*count*/) {}
    // 一张牌（AOE等）结算结束后向全体广播（self 为技能持有者，user 为使用者；如花鬘【象阵】）
    virtual void onCardResolvedByAny(GameEngine& /*engine*/, Player& /*self*/, Player& /*user*/, CardPtr /*card*/) {}
    // 自己的体力值变化后广播（delta>0 回复，<0 失去/受伤；如势·钟会【挟志】）
    virtual void onHpChanged(GameEngine& /*engine*/, Player& /*self*/, int /*delta*/) {}
    // 一次「卧龙演策」预测开始后向该角色技能广播（友·诸葛亮【方遒】展示预测）
    virtual void onPredictionStarted(GameEngine& /*engine*/, Player& /*self*/) {}
    // 任意角色进入出牌阶段时向全体广播（self 为技能持有者，turnOwner 为回合角色；如势·诫节）
    virtual void onAnyPhaseStart(GameEngine& /*engine*/, Player& /*self*/, Player& /*turnOwner*/, TurnPhase /*phase*/) {}
    // 任意角色的某阶段结束时向全体广播（self 为技能持有者，turnOwner 为回合角色；
    // 如主公【立储】“第一轮的任意角色结束阶段”）。
    virtual void onAnyPhaseEnd(GameEngine& /*engine*/, Player& /*self*/, Player& /*turnOwner*/, TurnPhase /*phase*/) {}
    // 一张牌确定了最终目标后向全体广播（帷幕二级「成为目标的次数」计数）
    virtual void onCardTargetConfirmed(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/,
                                       CardPtr /*card*/, const std::vector<PlayerPtr>& /*targets*/) {}

    // 任意角色即将受到伤害时向全体广播（damage 可改；在传导记录之前；如势·焚涛）
    virtual void onDamageTakingByAny(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/,
                                     Player& /*target*/, int& /*damage*/, ShaElement /*element*/, CardPtr /*cause*/) {}
    // 结算【杀】伤害前计算修正（全体广播；self=技能持有者，source=使用者；如势·钟会【肆恣】）
    virtual void onCalculateShaDamage(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*source*/,
                                      const Player& /*target*/, CardPtr /*card*/, int& /*damage*/) {}
    // 濒死救援完成且存活；仅对该次濒死事件调用一次。
    virtual void onLeaveDying(GameEngine& /*engine*/, Player& /*self*/) {}
    // 死亡发生时广播（结算奖励、清空区域前），self 可为死者本人。
    virtual void onPlayerDeath(GameEngine& /*engine*/, Player& /*self*/, Player& /*dead*/, Player* /*killer*/) {}
    // 失去技能前释放它施加的持续状态；例如风/雾、临时无视防具。
    virtual void onRemoved(GameEngine& /*engine*/, Player& /*self*/) {}
    // 回复体力前可修改回复量（self 为回复者，reason 如"桃"）
    virtual void onCalculateRecover(GameEngine& /*engine*/, Player& /*self*/, Player* /*source*/, int& /*amount*/, const std::string& /*reason*/) {}
    virtual void onAfterRecover(GameEngine& /*engine*/, Player& /*self*/, int /*actualAmount*/) {}
    // 流失体力（不经过伤害结算）后广播；self 为流失者（如【苦肉】获得护甲）
    virtual void onLoseHp(GameEngine& /*engine*/, Player& /*self*/, int /*amount*/) {}
    // 失去装备区的牌后（如【枭姬】）
    virtual void onEquipmentLost(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 使用一张非延时类锦囊牌时（如【集智】，结算前触发）
    virtual void onUseCard(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 失去最后一张手牌后（如【连营】）
    virtual void onHandEmpty(GameEngine& /*engine*/, Player& /*self*/) {}
    virtual void onOtherHandEmpty(GameEngine& /*engine*/, Player& /*self*/, Player& /*owner*/) {}
    // 其他角色获得/弃置某人的手牌时逐张广播；自身弃牌与普通打出牌不触发。
    virtual void onHandCardLostToOther(GameEngine& /*engine*/, Player& /*self*/,
                                       Player& /*victim*/, Player& /*instigator*/) {}
    virtual void onDiscardedInDiscardPhase(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    virtual void onOtherDiscardPhaseEnd(GameEngine& /*engine*/, Player& /*self*/, Player& /*owner*/,
                                        const std::vector<CardPtr>& /*discarded*/) {}
    // 判定牌生效前，可用手牌替换判定牌（如【鬼才】）
    virtual void onBeforeJudge(GameEngine& /*engine*/, Player& /*self*/, Player& /*judgeTarget*/, CardPtr& /*judgeCard*/) {}
    // 判定牌生效后，可获得判定牌（如【天妒】）；claimed 为 true 时不再进入弃牌堆
    virtual void onAfterJudge(GameEngine& /*engine*/, Player& /*self*/, Player& /*judgeTarget*/, CardPtr /*judgeCard*/, bool& /*claimed*/) {}
    // 目标合法性检查（self 为目标，如【空城】）
    virtual void onCheckCardTarget(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*target*/, CardPtr /*card*/, bool& /*canTarget*/) {}
    // 使用者侧的目标合法性检查（self 为卡牌使用者，target 为候选目标，如【北伐】标记）
    virtual void onCheckCardTargetAsSource(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*target*/, CardPtr /*card*/, bool& /*canTarget*/) {}
    // 指定目标与卡牌对目标生效是两个独立步骤：如【南蛮入侵】仍指定孟获，
    // 但因【祸首】对其无效；原目标转为其他牌时仍保留原目标的合法性。
    virtual void onCheckCardEffect(GameEngine& /*engine*/, const Player& /*self*/, CardPtr /*card*/, bool& /*effective*/) {}
    // 响应需求量修正（responder 需打出 count 张 wanted，如【无双】）；self 为对立方
    virtual void onCalculateResponseCount(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*responder*/, CardSubType /*wanted*/, int& /*count*/) {}
    // 检查具体响应牌是否合法（self 为技能持有者，responder 为实际响应者）
    virtual void onCheckResponseCard(const GameEngine& /*engine*/, const Player& /*self*/, const Player& /*responder*/, CardPtr /*card*/, CardSubType /*wanted*/, bool& /*allowed*/) const {}
    // 需要使用或打出某牌时，可令同势力角色代为打出（如【激将】/【护驾】）；返回 true 表示已由 out 提供
    virtual bool onNeedResponseCard(GameEngine& /*engine*/, Player& /*self*/, CardSubType /*wanted*/, CardPtr& /*out*/) { return false; }

    // ---- 用牌事件 ----
    // 你于回合外使用或打出手牌后
    virtual void onCardUsedOutsideTurn(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 不限使用/打出/弃置/转移，区域里的牌在回合外离开时逐张触发。
    virtual void onCardLostOutsideTurn(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 任意失去牌时触发（含回合内外），用于“失去非伤害牌”等精确判定（屯田）
    virtual void onAnyCardLost(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 别人“如手牌般使用或打出”了你借出的手牌（势-好施）：来源因“因此失去最后的手牌”而收到通知。
    virtual void onBorrowedHandCardUsed(GameEngine& /*engine*/, Player& /*self*/, Player& /*borrower*/,
                                        CardPtr /*card*/) {}
    // 使用牌（含真正“使用【杀】”的要求）；响应时纯打出牌走独立钩子。
    virtual void onCardPlayed(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 任意角色打出/使用一张牌时广播（含打出，用于演策“使用或打出”）
    virtual void onAnyCardPlayed(GameEngine& /*engine*/, Player& /*self*/, Player& /*user*/, CardPtr /*card*/) {}
    // 仅“使用”牌（不含响应时打出），供恃才等按使用记录牌型的技能使用。
    virtual void onCardUsed(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/,
                            bool /*firstUseOfTypeThisTurn*/) {}
    virtual void onCardResponded(GameEngine& engine, Player& self, CardPtr card) {
        onCardPlayed(engine,self,card); // 既适用使用又适用打出（如勤政）的技能保持行为
    }
    // 整张牌（包括多目标杀）的所有目标结算完毕后；可用于依伤害结果清理持续状态。
    virtual void onCardResolved(GameEngine& /*engine*/, Player& /*self*/, CardPtr /*card*/) {}
    // 【共砺】（友·诸葛亮）斗地主分支：友庞统在场且阵营一致 → “卧龙演策”可预测的牌数 +1；
    // 友徐庶在场且阵营一致 → 首项预测的结果视为正确。（身份模式该技能整体无效。）
    virtual int gongLiPredictionCountBonus(GameEngine& /*engine*/, Player& /*self*/) const { return 0; }
    virtual bool gongLiFirstPredictionAutoCorrect(GameEngine& /*engine*/, Player& /*self*/) const { return false; }
    // 摸牌修正接口（如【寸目】：强制从牌堆底摸牌）。
    virtual bool drawsFromBottom() const { return false; }

    // ---- 主动技接口（仅主动技覆盖） ----
    // 纯转化类主动技（如武圣/龙胆）：不在出牌阶段菜单中单独发动，而是在使用/打出牌时选择转化
    virtual bool isConversionSkill() const { return false; }
    // 是否作为主动技进入出牌阶段技能菜单（主动技默认是；持恒技可覆盖为 true）
    virtual bool isUsableActively() const { return getKind() == SkillKind::ACTIVE; }
    virtual bool canActivate(GameEngine& /*engine*/, Player& /*self*/) { return false; }
    // 非虚公开入口：每次调用都会重新校验存活状态、次数、发动时机与 canActivate，
    // 合法后才进入受保护的 onActivate() 效果实现。
    void activate(GameEngine& engine, Player& self);
    virtual bool aiShouldActivate(GameEngine& /*engine*/, Player& /*self*/) { return false; }

    // 每回合开始时重置回合内状态（次数等）
    virtual void resetTurnState() {}
};

using SkillPtr = std::shared_ptr<Skill>;

// ---------------------------------------------------------------------
//  第一层继承：三大种类
// ---------------------------------------------------------------------

// 主动技：出牌阶段主动发动，或在需要使用/打出某牌时主动选择转化
class ActiveSkill : public Skill {
protected:
    int maxUsesPerTurn;   // 0 表示不限次数（如武圣/龙胆）
    int usesThisTurn;

public:
    ActiveSkill(const std::string& name, const std::string& description, int maxUsesPerTurn = 0, unsigned tags = SkillTag::NONE)
        : Skill(name, description, SkillKind::ACTIVE, tags), maxUsesPerTurn(maxUsesPerTurn), usesThisTurn(0) {}

    bool hasUsesLeft() const { return maxUsesPerTurn == 0 || usesThisTurn < maxUsesPerTurn; }
    bool canActivate(GameEngine& engine, Player& self) override;
    void markUsed() { usesThisTurn++; }
    int getUsesThisTurn() const { return usesThisTurn; }
    void resetTurnState() override { usesThisTurn = 0; }
    // 出牌阶段级重置（额外出牌阶段：「出牌阶段限一次」类技能在额外阶段可再发动）
    void resetPhaseUses() { usesThisTurn = 0; }
};

// 触发技：拥有明确的发动时机
class TriggerSkill : public Skill {
public:
    TriggerSkill(const std::string& name, const std::string& description, unsigned tags = SkillTag::NONE)
        : Skill(name, description, SkillKind::TRIGGER, tags) {}
};

// 状态技：无发动时机、持续生效；是否为移动版“持恒技”由独立的 SUSTAINED 标签判断
class StateSkill : public Skill {
public:
    StateSkill(const std::string& name, const std::string& description, unsigned tags = SkillTag::LOCK)
        : Skill(name, description, SkillKind::STATE, tags) {}
};

// ---------------------------------------------------------------------
//  武将
// ---------------------------------------------------------------------
class Hero {
protected:
    std::string id;      // 唯一标识，如 "guanyu" / "jie_guanyu"
    std::string name;    // 关羽
    std::string title;   // 美髯公
    std::string pack;    // 所属扩展包：标准包 / 界限突破 / 一将成名 ...
    Country country;
    Gender gender;
    std::optional<Country> avatarCountry;
    std::optional<Gender> avatarGender;
    int maxHp;
    std::vector<SkillPtr> skills;

public:
    Hero(const std::string& id, const std::string& name, const std::string& title, const std::string& pack,
         Country country, Gender gender, int maxHp);
    virtual ~Hero() = default;

    std::string getId() const { return id; }
    std::string getName() const { return name; }
    std::string getTitle() const { return title; }
    std::string getPack() const { return pack; }
    Country getCountry() const { return avatarCountry.value_or(country); }
    Gender getGender() const { return avatarGender.value_or(gender); }
    void setAvatarIdentity(Country c, Gender g) { avatarCountry = c; avatarGender = g; }
    void clearAvatarIdentity() { avatarCountry.reset(); avatarGender.reset(); }
    int getMaxHp() const { return maxHp; }
    const std::vector<SkillPtr>& getSkills() const { return skills; }

    void addSkill(SkillPtr skill) { skills.push_back(skill); }
    // 按游戏规则清除所有可失去技能；持恒技保留。
    void clearSkills() {
        skills.erase(std::remove_if(skills.begin(), skills.end(),
                                    [](const SkillPtr& s) { return !s || !s->isSustained(); }),
                     skills.end());
    }
    bool removeSkill(const std::string& skillName);
    SkillPtr findSkill(const std::string& skillName) const;

    std::string getCountryString() const;
    std::string getGenderString() const;
    // 多行技能说明（用于选将预览）
    std::string getSkillSummary() const;
};

using HeroPtr = std::shared_ptr<Hero>;

class BlankHero : public Hero {
public:
    BlankHero(const std::string& name = "通用武将", Country country = Country::QUN, Gender gender = Gender::MALE, int hp = 4)
        : Hero("blank", name, "无名之辈", "通用", country, gender, hp) {}
};

} // namespace Thks

#endif // THKS_HERO_H
