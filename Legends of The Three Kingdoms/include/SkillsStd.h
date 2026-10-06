// SkillsStd.h — 三国杀标准包（标包）武将技能
// 覆盖：刘备、诸葛亮、曹操、司马懿、夏侯惇、张辽、许褚、郭嘉、甄姬、
//       孙权、甘宁、吕蒙、黄盖、周瑜、大乔、孙尚香、华佗、吕布、貂蝉
#ifndef THKS_SKILLS_STD_H
#define THKS_SKILLS_STD_H

#include "Hero.h"

namespace Thks {

// ============================== 蜀 ==============================

// 【仁德】出牌阶段可将任意张手牌交给其他角色；本阶段以此法给出第 2 张牌时回复 1 点体力
class RenDeSkill : public ActiveSkill {
    int givenThisPhase = 0;
    bool healedThisPhase = false;

public:
    RenDeSkill();
    void resetTurnState() override { givenThisPhase = 0; healedThisPhase = false; }
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// 【激将·主公技】需要使用或打出【杀】时，可令其他蜀势力角色选择是否打出【杀】（视为由你使用）
class JiJiangSkill : public ActiveSkill {
public:
    JiJiangSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
    bool onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) override;
};

// 【集智】当你使用一张普通锦囊牌时，可摸一张牌（含【无懈可击】）
class JiZhiSkill : public TriggerSkill {
public:
    JiZhiSkill();
    void onUseCard(GameEngine& engine, Player& self, CardPtr card) override;
};

// 【奇才·锁定技】你使用锦囊牌无距离限制（本引擎锦囊无距离规则，效果自然成立）
class QiCaiSkill : public StateSkill {
public:
    QiCaiSkill();
};

// 【观星】准备阶段，观看牌堆顶 X 张牌（X 为存活角色数且至多 5），以任意顺序置于牌堆顶，其余置于牌堆底
class GuanXingSkill : public TriggerSkill {
public:
    GuanXingSkill();
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) override;
};

// 【空城·锁定技】若你没有手牌，你不能成为【杀】或【决斗】的目标
class KongChengSkill : public StateSkill {
public:
    KongChengSkill();
    void onCheckCardTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr card, bool& canTarget) override;
};

// ============================== 魏 ==============================

// 【奸雄】当你受到伤害后，你可以获得对你造成伤害的牌
class JianXiongSkill : public TriggerSkill {
public:
    JianXiongSkill();
    bool onClaimDamageCard(GameEngine& engine, Player& self, Player* source, CardPtr cause) override;
};

// 【护驾·主公技】当你需要使用或打出【闪】时，你可以令其他魏势力角色选择是否打出【闪】
class HuJiaSkill : public TriggerSkill {
public:
    HuJiaSkill();
    bool onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) override;
};

// 【反馈】当你受到伤害后，你可以获得伤害来源的一张牌（按伤害事件计）
class FanKuiSkill : public TriggerSkill {
public:
    FanKuiSkill();
    void onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage, ShaElement element, CardPtr cause) override;
};

// 【鬼才】当一名角色的判定牌生效前，你可以打出一张手牌代替之
class GuiCaiSkill : public TriggerSkill {
public:
    GuiCaiSkill();
    void onBeforeJudge(GameEngine& engine, Player& self, Player& judgeTarget, CardPtr& judgeCard) override;
};

// 【刚烈】当你受到伤害后，你可以判定：若结果不为红桃，来源弃置两张手牌，否则受到你造成的 1 点伤害
class GangLieSkill : public TriggerSkill {
public:
    GangLieSkill();
    void onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage, ShaElement element, CardPtr cause) override;
};

// 【突袭】摸牌阶段，你可以放弃摸牌，改为获得至多两名其他角色的各一张手牌
class TuXiSkill : public TriggerSkill {
public:
    TuXiSkill();
    void onDrawCards(GameEngine& engine, Player& self, int& drawCount) override;
};

// 【裸衣】摸牌阶段，你可以少摸一张牌，然后本回合你使用【杀】或【决斗】造成伤害时，此伤害+1
class LuoYiSkill : public TriggerSkill {
    bool active = false;

public:
    LuoYiSkill();
    void resetTurnState() override { active = false; }
    void onDrawCards(GameEngine& engine, Player& self, int& drawCount) override;
    void onDealDamage(GameEngine& engine, Player& self, Player& target, int& damage, ShaElement element, CardPtr cause) override;
};

// 【天妒】当你的判定牌生效后，你可以获得之
class TianDuSkill : public TriggerSkill {
public:
    TianDuSkill();
    void onAfterJudge(GameEngine& engine, Player& self, Player& judgeTarget, CardPtr judgeCard, bool& claimed) override;
};

// 【遗计】当你受到1点伤害后，你可以观看牌堆顶的两张牌，然后将这些牌交给任意角色（每点伤害触发一次）
class YiJiSkill : public TriggerSkill {
public:
    YiJiSkill();
    void onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage, ShaElement element, CardPtr cause) override;
};

// 【倾国】你可以将一张黑色手牌当【闪】使用或打出
class QingGuoSkill : public ActiveSkill {
public:
    QingGuoSkill();
    bool isConversionSkill() const override;
    CardPtr convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) override;
};

// 【洛神】准备阶段，你可以判定：若结果为黑色，你获得判定牌且可以重复此流程
class LuoShenSkill : public TriggerSkill {
public:
    LuoShenSkill();
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) override;
};

// ============================== 吴 ==============================

// 【制衡】出牌阶段限一次，你可以弃置任意张牌，然后摸等量的牌
class ZhiHengSkill : public ActiveSkill {
public:
    ZhiHengSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// 【救援·主公技·锁定技】其他吴势力角色对你使用的【桃】回复体力值 +1
class JiuYuanSkill : public StateSkill {
public:
    JiuYuanSkill();
    void onCalculateRecover(GameEngine& engine, Player& self, Player* source, int& amount, const std::string& reason) override;
};

// 【奇袭】你可以将一张黑色牌当【过河拆桥】使用
class QiXiSkill : public ActiveSkill {
public:
    QiXiSkill();
    bool isConversionSkill() const override;
    CardPtr convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) override;
};

// 【克己】若你于出牌阶段内没有使用或打出过【杀】，你可以跳过弃牌阶段
class KeJiSkill : public TriggerSkill {
public:
    KeJiSkill();
    void onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) override;
};

// 【苦肉】出牌阶段，你可以失去 1 点体力，然后摸两张牌
class KuRouSkill : public ActiveSkill {
public:
    KuRouSkill();
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// 【英姿】摸牌阶段，你可以多摸一张牌
class YingZiSkill : public TriggerSkill {
public:
    YingZiSkill();
    void onDrawCards(GameEngine& engine, Player& self, int& drawCount) override;
};

// 【反间】出牌阶段限一次，令一名角色选择一种花色并获得你的一张手牌展示之，花色不符则你对其造成 1 点伤害
class FanJianSkill : public ActiveSkill {
public:
    FanJianSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// ============================== 吴（大乔/孙尚香） ==============================

// 【国色】你可以将一张方块牌当【乐不思蜀】使用
class GuoSeSkill : public ActiveSkill {
public:
    GuoSeSkill();
    bool isConversionSkill() const override;
    CardPtr convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) override;
};

// 【流离】当你成为【杀】的目标时，你可以弃置一张牌，将此【杀】转移给你攻击范围内的一名其他角色
class LiuLiSkill : public TriggerSkill {
public:
    LiuLiSkill();
    void onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) override;
};

// 【结姻】出牌阶段限一次，你可以弃置两张手牌并选择一名已受伤的男性角色，你与其各回复 1 点体力
class JieYinSkill : public ActiveSkill {
public:
    JieYinSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// 【枭姬】当你失去装备区的牌后，你可以摸两张牌
class XiaoJiSkill : public TriggerSkill {
public:
    XiaoJiSkill();
    void onEquipmentLost(GameEngine& engine, Player& self, CardPtr card) override;
};

// ============================== 群 ==============================

// 【急救】回合外，你可以将一张红色牌当【桃】使用（装备区的红色牌亦可）
class JiJiuSkill : public ActiveSkill {
public:
    JiJiuSkill();
    bool isConversionSkill() const override;
    CardPtr convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) override;
};

// 【青囊】出牌阶段限一次，你可以弃置一张手牌并令一名已受伤的角色回复 1 点体力
class QingNangSkill : public ActiveSkill {
public:
    QingNangSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// 【无双·锁定技】你使用【杀】需两张【闪】才能抵消；与你进行【决斗】的角色每次需打出两张【杀】
class WuShuangSkill : public StateSkill {
public:
    WuShuangSkill();
    void onCalculateResponseCount(GameEngine& engine, const Player& self, const Player& responder, CardSubType wanted, int& count) override;
};

// 【离间】出牌阶段限一次，你可以弃置一张牌并选择两名男性角色，令其中一名视为对另一名使用【决斗】
class LiJianSkill : public ActiveSkill {
public:
    LiJianSkill();
    bool canActivate(GameEngine& engine, Player& self) override;
    void onActivate(GameEngine& engine, Player& self) override;
    bool aiShouldActivate(GameEngine& engine, Player& self) override;
};

// 【闭月】结束阶段，你可以摸一张牌
class BiYueSkill : public TriggerSkill {
public:
    BiYueSkill();
    void onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) override;
};

// 【谦逊·锁定技】你不能成为【顺手牵羊】和【乐不思蜀】的目标
class QianXunSkill : public StateSkill {
public:
    QianXunSkill();
    void onCheckCardTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr card, bool& canTarget) override;
};

// 【连营】当你失去最后一张手牌后，你可以摸一张牌
class LianYingSkill : public TriggerSkill {
public:
    LianYingSkill();
    void onHandEmpty(GameEngine& engine, Player& self) override;
};

} // namespace Thks

#endif // THKS_SKILLS_STD_H
