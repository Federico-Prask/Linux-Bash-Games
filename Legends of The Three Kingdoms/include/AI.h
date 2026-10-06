#ifndef THKS_AI_H
#define THKS_AI_H

#include <vector>
#include <memory>
#include <random>
#include "Card.h"
#include "Hero.h"
#include "Player.h"
#include "HeroTier.h"

namespace Thks {

class GameEngine;
class Player;

class AIController {
public:
    // 真实身份版（仅供测试与内部对照）：直接读双方身份牌。
    static bool isFriend(const Player& self, const Player& other);
    // **推断版**（用户 2026-10-05：“AI 需分辨队友等”）：用公开身份 + 行为证据判断队友，
    // 不读未公开的真实身份。所有 AI 出牌/响应/技能选择一律走这一版。
    static bool isFriend(const GameEngine& engine, const Player& self, const Player& other);
    // 阵营估计（>0 偏主忠方，<0 偏反贼方，0 未知）；供 AI 决策与调试
    static int sideEstimate(const GameEngine& engine, const Player& other);

    // 牌的粗略价值（越大越重要，弃牌/转化时优先消耗低价值牌）
    // 基础价值（无局面信息）：桃10 / 无中8 / 顺手·无懈7 / 拆·决斗·南蛮·万箭6 /
    // 闪·乐·借刀·五谷·桃园5 / 杀·兵粮4 / 酒·马3 / 闪电2；
    // 装备按名称单独定价（见 equipmentBaseValue），杀/闪只是普通牌，锦囊普遍是好牌。
    static int cardValue(const CardPtr& card);
    // 装备基础价值：武器 4~6（连弩6/青龙·贯石·麒麟5/其余4），防具 4~5
    // （八卦·仁王5/白银·藤甲4），坐骑 3（同功能完全等价）
    static int equipmentBaseValue(const CardPtr& card);
    // 结合局面的估值（在基础价值上按可行动性修正，如借刀杀人需敌方有武器可借）
    static int cardValue(GameEngine& engine, Player& self, const CardPtr& card);
    static CardPtr chooseLeastValuableCard(const std::vector<CardPtr>& cards);

    // 从候选响应牌（含技能转化牌）中选择一张打出，可能返回 nullptr 表示放弃响应
    static CardPtr chooseResponseCard(GameEngine& engine, Player& self, const std::vector<CardPtr>& candidates, CardSubType requestedType);
    // 选择【杀】目标：优先能击杀（体力≤本次伤害）者，其次优先体力低者，再按手牌数（威胁）加权。
    static std::shared_ptr<Player> chooseShaTarget(GameEngine& engine, Player& self,
                                                  const std::vector<std::shared_ptr<Player>>& candidates,
                                                  int damage = 1);
    static std::shared_ptr<Player> chooseTrickTarget(GameEngine& engine, Player& self, CardSubType trickType, const std::vector<std::shared_ptr<Player>>& candidates);
    // 选择延时/拆顺类锦囊的目标：按“可见牌量”（手牌数+装备+判定区）从多到少选敌方
    static std::shared_ptr<Player> chooseRichEnemy(GameEngine& engine, Player& self,
                                                 const std::vector<std::shared_ptr<Player>>& candidates);
    // 从一组牌中挑价值最高的一张（五谷丰登选牌等）
    static CardPtr chooseMostValuableCard(const std::vector<CardPtr>& cards);
    // 结合局面的版本（同上，按持牌者视角估值）
    static CardPtr chooseMostValuableCard(GameEngine& engine, Player& self, const std::vector<CardPtr>& cards);
    // 丈八蛇矛：挑两张愿意当【杀】用掉的低价值手牌；凑不齐返回空
    static std::vector<CardPtr> pickZhangBaCards(const Player& self);

    // ---------------- 选将 AI（身份场 / 斗地主，非随机） ----------------
    // 选将语境：场次（用哪张强度表）、自己的身份、以及已经亮出的主公/地主武将。
    struct DraftContext {
        HeroTier::Field field = HeroTier::Field::IDENTITY;
        Identity role = Identity::ZHU_GONG;
        HeroPtr lordHero;   // 为空表示尚未有人亮将
        // D4 座次因素：0＝紧跟主公/地主行动的先手位，1＝中间，2＝末位（收割位）；-1＝未知
        int seatPosition = -1;
    };
    // 技能文本风格统计（输出/控制/辅助/防御），用于“针对或辅助”取舍
    struct StyleProfile { int output = 0, control = 0, support = 0, defense = 0; };
    static StyleProfile profileHero(const Hero& hero);
    // 选将评分：强度档 ×100 + 体力上限 ×5 + 身份倾向加权（同/异势力、攻守兼备等）
    static int draftScore(const HeroPtr& hero, const DraftContext& ctx);
    // 从候选武将里挑一个（同分取靠前者，保证确定性、可测试）
    static size_t chooseDraftHero(const std::vector<HeroPtr>& heroes, const DraftContext& ctx);
    // 按评分从高到低排序的下标（稳定排序：同分保持原顺序）
    static std::vector<size_t> rankDraftHeroes(const std::vector<HeroPtr>& heroes, const DraftContext& ctx);
    // 至尊场选将（用户 2026-10-05）：由选将 AI 先确定预选，**有概率选排第二的**。
    // secondPickPercent＝选第二名的概率（默认 30%）；候选不足 2 个时恒取第一名。
    static size_t chooseDraftHeroWithVariance(const std::vector<HeroPtr>& heroes, const DraftContext& ctx,
                                              std::mt19937& rng, int secondPickPercent = 30);

    // ==================== 局面评估共用底座（2026-10-05：“按优先级全部较完整完成”） ====================

    // F3：评分参数集中——所有魔数都在这里，便于统一调参与单测锁定
    struct Weights {
        int killBonus = 1000;        // 可一击致死的目标加成
        int hpBase = 20;             // 目标体力评分基准：hpBase - hp*hpFactor
        int hpFactor = 2;
        int handThreatCap = 8;       // 目标手牌威胁（min(手牌数, cap)）
        int lordFocusBonus = 60;     // 反贼/内奸集火主公的额外加成（E3）
        int focusTargetBonus = 40;   // 与本方集火目标一致的加成（E3）
        int aoeMargin = 0;           // 群体锦囊：敌方人数须 > 己方人数 + margin 才放（A3）
        int duelShaNeed = 2;         // 【决斗】要求手里的【杀】数（A6）
        int duelAvoidSellBlood = 1;  // 目标带卖血流技能时，决斗所需【杀】数再 +N（A6）
        int delayedTrickJudgeRisk = 3; // 目标能改判时，延时锦囊的评分扣减（A5/B6）
        int wuxieKeyOnly = 1;        // 【无懈可击】只为“关键锦囊”而出（B1）
        int peachReserveHp = 1;      // 手上【桃】保留阈值：自己体力 <= 此值时不当【闪】用（B2）
        int endgameAlone = 2;        // 残局单挑：存活 <= 此值时切换单挑策略（E4）
    };
    static const Weights& weights();

    // 阵营与人数（全部基于推断版 isFriend，不读未公开身份）
    static std::vector<std::shared_ptr<Player>> enemiesOf(GameEngine& engine, const Player& self);
    static std::vector<std::shared_ptr<Player>> friendsOf(GameEngine& engine, const Player& self);
    static int enemyCount(GameEngine& engine, const Player& self);
    static int friendCount(GameEngine& engine, const Player& self);
    static bool hasEnemy(GameEngine& engine, const Player& self);

    // A1：手里的“【杀】来源”张数＝实体【杀】+ 可转化牌（武圣红牌/龙胆闪/丈八两张/酒池等）
    static int shaSourceCount(GameEngine& engine, const Player& self);
    // A2：这一次【杀】预计造成的伤害（含【酒】醉酒、裸衣、烈弓、势-战烈等加伤与目标防具减免）
    static int estimatedShaDamage(GameEngine& engine, const Player& self, const Player& target);
    // A1+A2：用现有手牌能否斩杀目标（含“多刀来源”：咆哮/丈八/转化）
    // assumeDrunk＝假设已经喝下【酒】（A1：判断“先酒后杀”能否斩杀）
    static bool canKillWithSha(GameEngine& engine, const Player& self, const Player& target,
                               bool assumeDrunk = false);
    // A6：目标是否是卖血流（反馈/遗计/节命/狂骨…），决斗/多刀要谨慎
    static bool isSellBloodHero(const Player& target);
    // B6/A5：目标是否有改判能力（鬼才/鬼道/谋-红颜 等）
    static bool canAlterJudgement(GameEngine& engine, const Player& target);

    // E1：牌堆里还剩多少张某名牌（用记牌器口径，不偷看别人手牌）
    static int remainingCardsByName(GameEngine& engine, const std::string& cardName);

    // E3：本方集火目标（同一方 AI 尽量打同一个）——按“可击杀 > 体力低 > 手牌少 > 座位”统一评分
    static std::shared_ptr<Player> focusTarget(GameEngine& engine, const Player& self);

    // E4：是否进入残局单挑（存活人数 <= 2 且没有第三方）
    static bool isDuelEndgame(GameEngine& engine);

    // B6：判定改判决策——先看这次判定对谁有利（乐不思蜀/兵粮寸断/闪电/八卦阵/洛神/刚烈/雷击/铁骑），
    // 再挑一张“能达到期望结果且价值最低”的牌；无收益或代价过高时返回 nullptr（不改判）。
    // blackOnly＝只能打黑色牌（张角【鬼道】）。
    // pool 非空时从 pool 里挑（【鬼道】可用装备区的黑色牌），否则用手牌；花色一律按 effectiveSuit 计。
    static CardPtr chooseJudgementReplacement(GameEngine& engine, Player& self, const Player& judgeTarget,
                                              const CardPtr& judgeCard, bool blackOnly = false,
                                              const std::vector<CardPtr>& pool = {});

    struct PlayDecision {
        CardPtr cardToPlay;
        std::vector<std::shared_ptr<Player>> targets;
    };
    static PlayDecision makePlayDecision(GameEngine& engine, Player& self);
    // 弃牌阶段选牌（结合局面：当前用不出去的牌优先弃）
    static std::vector<CardPtr> chooseCardsToDiscard(GameEngine& engine, Player& self, int discardCount);
};

} // namespace Thks

#endif // THKS_AI_H
