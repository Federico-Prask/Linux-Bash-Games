#ifndef THKS_SKILLS_MOU_H
#define THKS_SKILLS_MOU_H

// =====================================================================
//  谋攻篇（谋）武将技能实现 —— 技能文本以《三国杀移动版》官网
//  hero-detail 详情页“技能介绍”原文为准（逐字，见 docs/mou_appendix.md 附录C）。
//  官网详情页未载的从属技能（夺荆、英姿※、英魂※）来源为百度百科词条，已披露。
// =====================================================================

#include "Hero.h"
#include <set>

namespace Thks {

// 附录C官网原文表（键 = 武将名 + "/" + 技能名，保证同名技能跨包不冲突）
const std::string& mouText(const std::string& hero, const std::string& skill);

// ---------------- 蜀 ----------------

// 掠影（谋·刘赪）：使用杀/非伤害普通锦囊积累“椎”标记，两张时摸牌并拆/杀。
class MouLueYingSkill : public TriggerSkill {
public:
    MouLueYingSkill();
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 莺舞（谋·刘赪）：配合掠影的锦囊侧标记与杀结算。
class MouYingWuSkill : public TriggerSkill {
public:
    MouYingWuSkill();
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 克己（谋·吕蒙）：弃牌或流失体力换护甲；手牌上限+护甲；非濒死不能用桃。
class MouKeJiSkill : public ActiveSkill {
    bool usedDiscard = false;
    bool usedLose = false;
public:
    MouKeJiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onCalculateHandLimit(GameEngine&, const Player&, int&) override;
    bool onNeedResponseCard(GameEngine&, Player&, CardSubType, CardPtr&) override;
    void resetTurnState() override { usedDiscard = false; usedLose = false; }
};

// 渡江（谋·吕蒙）：觉醒技，护甲≥3 获得“夺荆”。
class MouDuJiangSkill : public TriggerSkill {
public:
    MouDuJiangSkill();
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 夺荆（谋·吕蒙，官网详情页未载，来源见附录C）：杀指定目标后失护甲→无视防具+获得其一张牌+杀次数+1。
class MouDuoJingSkill : public TriggerSkill {
public:
    MouDuoJingSkill();
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onCalculateShaLimit(GameEngine&, const Player&, int&) override;
};

// 烈弓（谋·黄忠）：记录花色，杀唯一目标后展示牌堆顶X张增伤并封响应。
class MouLieGongSkill : public TriggerSkill {
public:
    MouLieGongSkill();
    void onUseCard(GameEngine&, Player&, CardPtr) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr, const std::vector<PlayerPtr>&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
};

// 耀武（谋·华雄）：受到杀伤害时红杀来源回复或摸牌，非红自己摸牌。
class MouYaoWuSkill : public TriggerSkill {
public:
    MouYaoWuSkill();
    void onAfterDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

// 扬威（谋·华雄）：摸2获得“威”标记，威惠己方杀增益。
class MouYangWeiSkill : public ActiveSkill {
    bool usedThisTurn = false;
    bool disablePending = false;
public:
    MouYangWeiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onCalculateShaLimit(GameEngine&, const Player&, int&) override;
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 暝眩（谋·杨婉）：出牌阶段开始随机交牌，对方选杀你或交牌摸牌。
class MouMingXuanSkill : public TriggerSkill {
public:
    MouMingXuanSkill();
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 陷仇（谋·杨婉）：受伤害后令选中角色对伤害来源用杀，造成伤害则回复。
class MouXianChouSkill : public TriggerSkill {
public:
    MouXianChouSkill();
    void onAfterDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

// 谋弈（谋·马超【铁骑】）：发动者与目标各从两项中选择一项（互不可见、按克制判定）。
// 两人选择不同则发动者成功，执行发动者所选项的效果；选择相同则谋弈失败，无事发生。
// 选项：0=直取敌营（你获得其一张牌）；1=扰阵疲敌（你摸两张牌）。
// mineChoice/theirsChoice < 0 时走交互询问（人类输入 / AI 随机）；返回发动者是否成功。
bool mouYiDuel(GameEngine& engine, Player& self, Player& target, int mineChoice = -1, int theirsChoice = -1);

// 铁骑（谋·马超）：非锁定技；杀指定目标后可令其本回合非锁定技失效、不能使用【闪】，并进行谋弈。
class MouTieQiSkill : public TriggerSkill {
public:
    MouTieQiSkill();
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
};

class MouMaShuSkill : public StateSkill {
public:
    MouMaShuSkill();
    void onCalculateDistance(GameEngine&, const Player& self, const Player& target, int& distance) override;
};

// 谋·张飞【咆哮】：锁定技；无限【杀】、武器无距离；本阶段第二张起的【杀】封非锁定技、不可响应且伤害+1，未击杀目标时失去体力并随机弃牌。
class MouPaoXiaoSkill : public StateSkill {
public:
    MouPaoXiaoSkill();
    void onCalculateShaLimit(GameEngine&, const Player&, int&) override;
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onCalculateShaDamage(GameEngine&, const Player&, const Player&, const Player&, CardPtr, int&) override;
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
};

// 协击：准备阶段选择其他角色协力；其回合结束协力成功后依次对至多三名角色使用普通杀，每次造成伤害后分别摸等量牌。
class MouXieJiSkill : public ActiveSkill {
public:
    MouXieJiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onTurnEnd(GameEngine&, Player&, Player&) override;
};

// 龙胆（谋·赵云）：限次杀↔闪转化，每名角色回合结束+1次上限。
class MouLongDanSkill : public ActiveSkill {
public:
    MouLongDanSkill();
    bool isConversionSkill() const override { return true; }
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void onCardResponded(GameEngine&, Player&, CardPtr) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 积著（谋·赵云）：准备阶段发起协力，成功后强化龙胆。
class MouJiZhuSkill : public ActiveSkill {
public:
    MouJiZhuSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onTurnEnd(GameEngine&, Player&, Player&) override;
};

// 结姻（谋·孙尚香）：使命技，助标记与势力变更。
class MouJieYinSkill : public TriggerSkill {
public:
    MouJieYinSkill();
    void onGameStart(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 良助（谋·孙尚香·蜀势力技）：移装备为妆，助标记角色回血或摸牌。
class MouLiangZhuSkill : public ActiveSkill {
public:
    MouLiangZhuSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 枭姬（谋·孙尚香·吴势力技）：失去装备摸2并可弃场上一张牌。
class MouXiaoJiSkill : public TriggerSkill {
public:
    MouXiaoJiSkill();
    void onEquipmentLost(GameEngine&, Player&, CardPtr) override;
};

// 燕语（谋·夏侯氏）：限两次弃杀摸1；出牌阶段结束令人摸X（弃杀数×3）。
class MouYanYuSkill : public ActiveSkill {
    int killedThisPhase = 0;
    int usesThisPhase = 0;
public:
    MouYanYuSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 樵拾（谋·夏侯氏）：受伤害后伤害来源可令你回血并摸2。
class MouQiaoShiSkill : public TriggerSkill {
public:
    MouQiaoShiSkill();
    void onTurnStart(GameEngine&, Player&) override;
    void onAfterDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

// ---------------- 吴 ----------------

// 英姿（谋·周瑜）：三项数值每满足一项多摸一张且本回合手牌上限+1。
class MouYingZiSkill : public StateSkill {
public:
    MouYingZiSkill();
    void onDrawCards(GameEngine&, Player&, int&) override;
    void onCalculateHandLimit(GameEngine&, const Player&, int&) override;
};

// 反间（谋·周瑜）：扣置花色手牌令对方猜或翻面。
class MouFanJianSkill : public ActiveSkill {
public:
    MouFanJianSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onTurnStart(GameEngine&, Player&) override;
};

// 离间（谋·貂蝉）：选≥2名弃X牌，依次视为对另一名用决斗。
class MouLiJianSkill : public ActiveSkill {
public:
    MouLiJianSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onTurnStart(GameEngine&, Player&) override;
};

// 闭月（谋·貂蝉）：结束阶段摸X（受伤角色数+1，至多4）。
class MouBiYueSkill : public TriggerSkill {
    std::vector<int> damagedThisRound;
public:
    MouBiYueSkill();
    void onGlobalDamage(GameEngine&, Player&, Player*, Player&, int, CardPtr) override;
    void onTurnStart(GameEngine&, Player&) override;
    void onRoundStart(GameEngine&, Player&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 乱击（谋·袁绍）：两张手牌当万箭齐发；闪响应摸牌（每回合至多3）。
class MouLuanJiSkill : public ActiveSkill {
public:
    MouLuanJiSkill();
    bool isConversionSkill() const override { return false; }
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onTurnStart(GameEngine&, Player&) override;
    void onCardResponded(GameEngine&, Player&, CardPtr) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr, const std::vector<PlayerPtr>&) override;
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
};

// 血裔（谋·袁绍·主公技）：手牌上限+群势力数×2；指定群势力目标摸牌（每回合至多2）。
class MouXueYiSkill : public StateSkill {
public:
    MouXueYiSkill();
    void onCalculateHandLimit(GameEngine&, const Player&, int&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr, const std::vector<PlayerPtr>&) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void onTurnStart(GameEngine&, Player&) override;
};

// 制衡（谋·孙权）：弃任意张摸等量，弃光额外摸X+1并移除一“业”。
class MouZhiHengSkill : public ActiveSkill {
public:
    MouZhiHengSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 统业（谋·孙权）：结束阶段按装备数是否变化获得/失去“业”。
class MouTongYeSkill : public TriggerSkill {
    int equipCountAtEnd = -1;
public:
    MouTongYeSkill();
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 救援（谋·孙权·主公技）：吴势力用桃你摸1；对你用桃回复+1。
class MouJiuYuanSkill : public StateSkill {
public:
    MouJiuYuanSkill();
    void onAnyPeachUsed(GameEngine&, Player&, Player&, Player&) override;
    void onCalculateRecover(GameEngine&, Player&, Player*, int&, const std::string&) override;
};

// 国色（谋·大乔）：限四次方块牌当乐不思蜀或弃场上乐，然后摸1。
class MouGuoSeSkill : public ActiveSkill {
    int uses = 0;
public:
    MouGuoSeSkill();
    bool isConversionSkill() const override { return true; }
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void resetTurnState() override { uses = 0; }
};

// 流离（谋·大乔）：杀转移+红桃牌给“流离”标记（额外出牌阶段）。
class MouLiuLiSkill : public TriggerSkill {
public:
    MouLiuLiSkill();
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onTurnStart(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 祸首（谋·孟获）：南蛮无效、代来源、出牌阶段开始获得弃牌堆南蛮；阶段内限一次。
class MouHuoShouSkill : public StateSkill {
    bool usedThisPhase = false;
public:
    MouHuoShouSkill();
    void onCheckCardEffect(GameEngine&, const Player&, CardPtr, bool&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 再起（谋·孟获）：蓄力技，弃牌阶段结束消耗蓄力点；造成伤害+1蓄力（每回合1）。
class MouZaiQiSkill : public ActiveSkill {
    bool gainedThisTurn = false;
public:
    MouZaiQiSkill();
    void onGameStart(GameEngine&, Player&) override; // 蓄力技（0/7）：登记上限 7
    bool canActivate(GameEngine&, Player&) override;
    bool canActivateOutsidePlayPhase() const override { return true; }
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onAfterDealDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
    void resetTurnState() override { gainedThisTurn = false; }
};

// ---------------- 蜀（续） ----------------

// 连环（谋·庞统）：梅花手牌当铁索连环（阶段限1次转化）或重铸；两级；使用铁索可失血令不连环者弃牌。
class MouLianHuanSkill : public ActiveSkill {
    int level = 1;
    int convertsThisPhase = 0;
public:
    MouLianHuanSkill();
    bool isConversionSkill() const override { return true; }
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr,
                               const std::vector<PlayerPtr>&) override;
    void onCalculateTieSuoTargetLimit(GameEngine&, const Player&, int&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void resetTurnState() override;
    int getLevel() const { return level; }
    void upgrade() { level = 2; }
};

// 涅槃（谋·庞统）：限定技濒死弃区摸2回复至2并升级连环。
class MouNiePanSkill : public TriggerSkill {
    bool spent = false;
public:
    MouNiePanSkill();
    void onDying(GameEngine&, Player&, Player&) override;
};

// 仁德（谋·刘备）：阶段开始得2“仁望”；交牌得“仁望”；弃2“仁望”视为用/打基本牌。
class MouRenDeSkill : public ActiveSkill {
    int givenThisPhase = 0;
    std::set<int> receivedThisPhase;
public:
    MouRenDeSkill();
    bool isConversionSkill() const override { return false; }
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    bool onNeedResponseCard(GameEngine&, Player&, CardSubType, CardPtr&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 章武（谋·刘备）：限定技，收“仁德”牌角色交Y张，回3血并失去仁德。
// “获得过仁德牌”以目标身上的 mark「仁德获得:<刘备id>」记录（由仁德技能添加）。
class MouZhangWuSkill : public ActiveSkill {
    bool spent = false;
public:
    MouZhangWuSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 激将（谋·刘备·主公技）：出牌阶段结束时指定目标，符合条件的蜀势力角色选杀或跳过下阶段。
class MouJiJiangSkill : public TriggerSkill {
public:
    MouJiJiangSkill();
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 挑衅（谋·姜维）：蓄力技，限一次选至多X名其二择；弃牌阶段每弃一张+1蓄力。
class MouTiaoXinSkill : public ActiveSkill {
    int chosenTotal = 0;
public:
    MouTiaoXinSkill();
    // 蓄力技（4/4）：初始 4 点蓄力点、上限 4（供局势显示与跨技能引用）。
    void onGameStart(GameEngine&, Player&) override;
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    // “弃牌阶段，你每弃置一张牌，获得1点蓄力点”：仅本人在弃牌阶段弃置手牌时计数。
    void onDiscardedInDiscardPhase(GameEngine&, Player&, CardPtr) override;
};

// 志继（谋·姜维）：觉醒，减1上限给“北伐”标记（其用牌只能选你或其）。
class MouZhiJiSkill : public TriggerSkill {
    bool awakened = false;
public:
    MouZhiJiSkill();
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 眩惑（谋·法正）：交牌给无“眩”者；其摸牌阶段外得牌时你随机得其一张手牌（每标记限5）。
class MouXuanHuoSkill : public ActiveSkill {
public:
    MouXuanHuoSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onAnyCardsObtained(GameEngine&, Player&, Player&, int) override;
};

// 恩怨（谋·法正）：准备阶段结算“眩”标记角色的拿牌/失血。
class MouEnYuanSkill : public TriggerSkill {
public:
    MouEnYuanSkill();
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// ---------------- 群 ----------------

// 耀武/扬威（华雄）、暝眩/陷仇（杨婉）、乱击/血裔（袁绍）见上。

// 明策（谋·陈宫）：交牌其选流失体力你摸2获“策”，或其摸1；阶段开始耗“策”造成伤害。
class MouMingCeSkill : public ActiveSkill {
public:
    MouMingCeSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 智迟（谋·陈宫）：同回合第二次及以后伤害防止。
class MouZhiChiSkill : public StateSkill {
    bool damagedThisTurn = false;
public:
    MouZhiChiSkill();
    void onTakeDamage(GameEngine&, Player&, Player*, int&, ShaElement) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 奇袭（谋·甘宁）：令其猜手牌花色，猜错弃其区域牌。
class MouQiXiSkill : public ActiveSkill {
public:
    MouQiXiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 奋威（谋·甘宁）：限定技，至多三张牌置为“威”牌；其成锦囊目标时保护或获牌。
class MouFenWeiSkill : public ActiveSkill {
    bool spent = false;
public:
    MouFenWeiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    void onCheckCardTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
};

// 苦肉（谋·黄盖）：阶段开始交牌失体力（桃酒-2）；失1体力后获得2护甲（经 onLoseHp 全局生效）。
class MouKuRouSkill : public TriggerSkill {
public:
    MouKuRouSkill();
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onLoseHp(GameEngine&, Player&, int) override;
};

// 诈降（谋·黄盖）：每回合前X张牌无距离次数限制不可响应，多摸X（X=已损失体力）。
class MouZhaXiangSkill : public StateSkill {
    int cardsUsedThisTurn = 0;
public:
    MouZhaXiangSkill();
    void onDrawCards(GameEngine&, Player&, int&) override;
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onCheckCardTargetAsSource(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onCalculateShaLimit(GameEngine&, const Player&, int&) override;
    void onCardUsed(GameEngine&, Player&, CardPtr, bool) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// ---------------- 吴（续） ----------------

// 貂蝉/袁绍等见上；以下为 吴 其余：

// （黄盖吴、甘宁吴、大乔吴、孙权吴、周瑜吴、陆逊吴、韩当吴、诸葛瑾吴、小乔吴、朱然吴、公孙瓒群、诸葛亮蜀等）

// 祸首/再起 见上（孟获蜀）。

// 激昂（谋·孙策）：决斗可流失体力额外指定一目标；决斗/红杀关联摸牌；限次全手牌当决斗。
class MouJiAngSkill : public ActiveSkill {
    int level = 1;
    int usedDuels = 0;
public:
    MouJiAngSkill();
    bool isConversionSkill() const override { return true; }
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onDuelTargeted(GameEngine&, Player&, Player&, Player&) override;
    void resetTurnState() override { usedDuels = 0; }
    void setLevel(int l) { level = l; }
};

// 魂姿（谋·孙策）：脱离濒死时减1上限、得1护甲、摸3，获英姿※英魂※。
class MouHunZiSkill : public TriggerSkill {
public:
    MouHunZiSkill();
    void onLeaveDying(GameEngine&, Player&) override;
};

// 制霸（谋·孙策·主公技限定）：进入濒死回复X并升级激昂，吴势力依次受1点无来源伤害。
class MouZhiBaSkill : public TriggerSkill {
public:
    MouZhiBaSkill();
    void onDying(GameEngine&, Player&, Player&) override;
};

// 英姿※（谋·孙策觉醒获得）：三条件多摸+上限（与谋周瑜英姿同构）。
class MouXingYingZiSkill : public StateSkill {
public:
    MouXingYingZiSkill();
    void onDrawCards(GameEngine&, Player&, int&) override;
    void onCalculateHandLimit(GameEngine&, const Player&, int&) override;
};

// 英魂※（谋·孙策觉醒获得）：准备阶段令一名角色摸X弃1或摸1弃X。
class MouXingYingHunSkill : public ActiveSkill {
public:
    MouXingYingHunSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 烈刃（谋·祝融）：杀唯一目标后可拼点，赢则结算后对另一名角色1伤害。
class MouLieRenSkill : public TriggerSkill {
public:
    MouLieRenSkill();
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
};

// 巨象（谋·祝融）：南蛮无效、结算后获得；结束阶段未用过则游戏外交给一名角色。
class MouJuXiangSkill : public StateSkill {
    bool usedNanmanThisTurn = false;
public:
    MouJuXiangSkill();
    void onCheckCardEffect(GameEngine&, const Player&, CardPtr, bool&) override;
    void onCardResolvedByAny(GameEngine&, Player&, Player&, CardPtr) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 明任（谋·卢植）：开局摸2扣置“任”；结束阶段可换。
class MouMingRenSkill : public ActiveSkill {
public:
    MouMingRenSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onGameStart(GameEngine&, Player&) override;
};

// 贞良（谋·卢植）：转换技阳/阴。
class MouZhenLiangSkill : public ActiveSkill {
    bool yang = true;
public:
    MouZhenLiangSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
};

// 火计（谋·诸葛亮）：使命技，限一次对目标及同势力各1火焰伤害；成功换观星空城。
class MouHuoJiSkill : public ActiveSkill {
    bool completed = false;
    bool failed = false;
public:
    MouHuoJiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onAfterDealDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
    void onDying(GameEngine&, Player&, Player&) override;
};

// 看破（谋·诸葛亮）：每轮清记录并可记牌名；他方用同名牌移除记录→无效且摸1。
class MouKanPoSkill : public StateSkill {
    std::vector<std::string> recorded;
    std::vector<std::string> lastRoundRecorded;
    int recordedThisGame = 0;   // “每局游戏最多记录4个牌名（斗地主/排位为2）”的累计消耗（每记录一个牌名 +1）
public:
    int recordedBudgetUsedForTesting() const { return recordedThisGame; }
public:
    MouKanPoSkill();
    void onRoundStart(GameEngine&, Player&) override;
    void onOtherCardUsedBefore(GameEngine&, Player&, Player&, CardPtr, bool&) override;
};

// 观星※（谋·诸葛亮使命成功获得）：准备阶段置星、结束阶段可摆星，需牌时星视为手牌。
class MouGuanXingXinSkill : public ActiveSkill {
    int preparedCount = 0;
public:
    MouGuanXingXinSkill();
    bool isConversionSkill() const override { return true; }
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
};

// 空城※（谋·诸葛亮）：受伤害时有点数判定减伤/增伤。
class MouKongChengXinSkill : public StateSkill {
public:
    MouKongChengXinSkill();
    void onTakeDamage(GameEngine&, Player&, Player*, int&, ShaElement) override;
};

// 武圣（谋·关羽）：转化+阶段指定主公外角色的多重效果。
class MouWuShengSkill : public TriggerSkill {
public:
    MouWuShengSkill();
    bool isConversionSkill() const override { return true; }
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onCalculateDistance(GameEngine&, const Player&, const Player&, int&) override;
    bool canUseShaBeyondLimitOn(GameEngine&, const Player&, const Player&, CardPtr) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void resetTurnState() override;
};

// 义绝（谋·关羽）：锁定，回合内对其他造成将死伤害时防死（每名角色每局一次），后本回合对其用牌取消。
class MouYiJueSkill : public StateSkill {
public:
    MouYiJueSkill();
    void onDealDamage(GameEngine&, Player&, Player&, int&, ShaElement, CardPtr) override;
    void onCheckCardTargetAsSource(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 集智（谋·黄月英）：普通锦囊摸1（本回合不计上限）。
class MouJiZhiSkill : public TriggerSkill {
public:
    MouJiZhiSkill();
    void onUseCard(GameEngine&, Player&, CardPtr) override;
};

// 奇才（谋·黄月英）：锦囊无距离；限一次置装备给“奇”，其后续3张普通锦囊须交给你。
class MouQiCaiSkill : public ActiveSkill {
public:
    MouQiCaiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onCalculateDistance(GameEngine&, const Player&, const Player&, int&) override;
    void onCheckCardTargetAsSource(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onAnyCardObtained(GameEngine&, Player&, Player&, CardPtr) override;
};

// 天香（谋·小乔）：红手牌给“天香”花色标记；受伤害时按花色转移或收牌。
class MouTianXiangSkill : public ActiveSkill {
public:
    MouTianXiangSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onTakeDamage(GameEngine&, Player&, Player*, int&, ShaElement) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
};

// 红颜（谋·小乔）：黑桃手牌视红桃；判定牌红桃时可改花色。
class MouHongYanSkill : public StateSkill {
public:
    MouHongYanSkill();
    void onBeforeJudge(GameEngine&, Player&, Player&, CardPtr&) override;
};

// 义从（谋·公孙瓒）：蓄力技，轮始消耗蓄力点得距离效果与“扈”。
class MouYiCongSkill : public ActiveSkill {
public:
    MouYiCongSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onRoundStart(GameEngine&, Player&) override;
    void onGameStart(GameEngine&, Player&) override;
    void onCalculateDistance(GameEngine&, const Player&, const Player&, int&) override;
    void onCalculateDistanceToYou(GameEngine&, const Player&, const Player&, int&) override;
    bool onNeedResponseCard(GameEngine&, Player&, CardSubType, CardPtr&) override;
};

// 趫猛（谋·公孙瓒）：杀造成伤害后弃其一张摸1或+3蓄力。
class MouQiaoMengSkill : public TriggerSkill {
public:
    MouQiaoMengSkill();
    void onAfterDealDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

// 弓骑（谋·韩当）：出牌阶段限一次，弃牌使本回合攻击范围无限；若弃置装备牌，可弃置其他角色一张牌。
class MouGongQiSkill : public ActiveSkill {
    bool phaseStartAvailable = false;
public:
    MouGongQiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// 解烦（谋·韩当）：指定角色选弃牌或摸牌；背水失效至杀角色。
class MouJieFanSkill : public ActiveSkill {
    bool disabled = false;
public:
    MouJieFanSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void onPlayerDeath(GameEngine&, Player&, Player&, Player*) override;
};

// 谦逊（谋·陆逊）：锦囊生效记录牌名并屯牌；阶段开始可移记录视为使用。
class MouQianXunSkill : public TriggerSkill {
    int recordedCount = 0;
    std::vector<std::string> recordedNames; // 已记录的牌名
    int pendingReturnOwner = -1;            // 放牌所在回合的所有者（“此回合结束时获得”）
    void returnPile(GameEngine& engine, Player& self);
    void recordResolved(GameEngine& engine, Player& self, CardPtr card);
public:
    MouQianXunSkill();
    void onCheckCardEffect(GameEngine&, const Player&, CardPtr, bool&) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr, const std::vector<PlayerPtr>&) override;
    void onAnyCardUsed(GameEngine&, Player&, Player&, CardPtr) override;
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
    void onCardResolvedByAny(GameEngine&, Player&, Player&, CardPtr) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
};

// 连营（谋·陆逊）：其他角色回合结束看堆顶X张分配。
class MouLianYingSkill : public TriggerSkill {
    int lostThisTurn = 0;
public:
    MouLianYingSkill();
    void onCardLostOutsideTurn(GameEngine&, Player&, CardPtr) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    void onTurnEnd(GameEngine&, Player&, Player&) override;
};

// 完杀（谋·贾诩）：回合内他方非濒死不能用桃；濒死时观看分配牌（两级）。
class MouWanShaSkill : public StateSkill {
    int level = 1;
public:
    MouWanShaSkill();
    bool onNeedResponseCard(GameEngine&, Player&, CardSubType, CardPtr&) override;
    void onDying(GameEngine&, Player&, Player&) override;
    void onOtherDying(GameEngine&, Player&, Player&) override;
    void onRoundStart(GameEngine&, Player&) override;
    void process(GameEngine&, Player&, Player&);
    void upgrade() { level = 2; }
};

// 乱武（谋·贾诩）：限定技，全场除非对最近角色用杀否则失体力；失体力数升级技能。
class MouLuanWuSkill : public ActiveSkill {
    bool spent = false;
public:
    MouLuanWuSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 帷幕（谋·贾诩）：黑锦囊取消（两级，二级每轮回收）。
class MouWeiMuSkill : public StateSkill {
    int level = 1;
public:
    MouWeiMuSkill();
    void onCheckCardTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onCardTargetConfirmed(GameEngine&, Player&, Player*, CardPtr, const std::vector<PlayerPtr>&) override;
    void onRoundStart(GameEngine&, Player&) override;
    void upgrade() { level = 2; }
};

// 缓释（谋·诸葛瑾）：判定生效前观看堆顶并替换。
class MouHuanShiSkill : public TriggerSkill {
public:
    MouHuanShiSkill();
    void onBeforeJudge(GameEngine&, Player&, Player&, CardPtr&) override;
};

// 弘援（谋·诸葛瑾）：蓄力技，一次获得≥2牌/他方一次失去≥2牌时消耗蓄力点补牌。
class MouHongYuanSkill : public TriggerSkill {
public:
    MouHongYuanSkill();
    void onCardsObtained(GameEngine&, Player&, int) override;
    void onCardsLostBatch(GameEngine&, Player&, Player&, int) override;
    void onGameStart(GameEngine&, Player&) override;
};

// 明哲（谋·诸葛瑾）：回合外失牌（每轮限2）选角色，有蓄力技+1蓄力；含非基本牌其摸1。
class MouMingZheSkill : public TriggerSkill {
    int usedThisRound = 0;
public:
    MouMingZheSkill();
    void onCardLostOutsideTurn(GameEngine&, Player&, CardPtr) override;
    void onRoundStart(GameEngine&, Player&) override;
};

// 无双（谋·吕布）：双闪双杀响应+未响应加伤（每回合限1次）。
class MouWuShuangSkill : public StateSkill {
    bool bonusUsedThisTurn = false;
public:
    MouWuShuangSkill();
    void onCalculateResponseCount(GameEngine&, const Player&, const Player&, CardSubType, int&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onDealDamage(GameEngine&, Player&, Player&, int&, ShaElement, CardPtr) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool) override;
    void onDuelTargeted(GameEngine&, Player&, Player&, Player&) override;
    void resetTurnState() override { bonusUsedThisTurn = false; }
};

// 利驭（谋·吕布）：杀造成伤害后获得其区域牌等量、其摸等量；集齐三类触发二择。
class MouLiYuSkill : public TriggerSkill {
public:
    MouLiYuSkill();
    void onAfterDealDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
};

// 镇围（谋·朱然）：与一名角色比弃牌（牌数/花色数），执行伤害或摸3。
class MouZhenWeiSkill : public ActiveSkill {
public:
    MouZhenWeiSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 合援（谋·朱然）：结束阶段限一次，弃X令已受伤角色重复上次镇围最后一项。
class MouHeYuanSkill : public TriggerSkill {
public:
    MouHeYuanSkill();
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
};

// ---------------- 势·小乔（势包，hero-detail-666） ----------------

// 合韵：出牌阶段限两次，你可选择一名与你有相同技能的角色，然后你失去一个技能并令其摸两张牌。
class ShiHeYunSkill : public ActiveSkill {
public:
    ShiHeYunSkill();
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
};

// 音洄：每轮开始时，你可清除因此获得的技能，然后你选择一名其他角色当前拥有的一个技能获得之。
class ShiYinHuiSkill : public TriggerSkill {
    std::vector<std::string> granted; // 通过音洄获得的技能名（下次发动时清除）
public:
    ShiYinHuiSkill();
    void onRoundStart(GameEngine&, Player&) override;
    const std::vector<std::string>& grantedSkills() const { return granted; }
};

} // namespace Thks

#endif