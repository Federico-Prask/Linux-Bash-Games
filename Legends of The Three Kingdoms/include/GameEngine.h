#ifndef THKS_GAME_ENGINE_H
#define THKS_GAME_ENGINE_H

#include <vector>
#include <algorithm>
#include <deque>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <random>
#include <ostream>
#include "Card.h"
#include "Player.h"
#include "Hero.h"
#include "HeroTier.h"

namespace Thks {

class Logger;
class Interaction;

class GameEngine {
public:
    enum class GameMode { JUNZHENG, PAIWEI, DOUDIZHU };
    // 至尊场选将：AI 排名后“有概率选排第二的”的概率（%）。用户 2026-10-05 只说“有概率”，
    // 未给数值，本项目取 30%（可在 docs/identity_field_rules.md 查证并按需调整）。
    static constexpr int kSecondPickPercent = 30;
private:
private:
    Deck deck;
    std::vector<PlayerPtr> players;
    int currentTurnIndex;
    TurnPhase currentPhase;
    bool gameOver;
    PlayerPtr activeDying; // 濒死响应窗口；允许完杀等按实际救援对象检查用桃资格
    PlayerPtr activeDuelUser; // 正在结算的【决斗】的使用者（裸衣等“你使用【决斗】”判定）
    std::string winningFaction;
    bool interactiveMode;   // 是否有人类玩家
    int humanSeat = -1;     // 人类玩家座位（-1=全 AI）
    int aiDelayMs;          // AI 行动间隔（全 AI 演示时可设为 0）
    std::mt19937 rng;
    unsigned seed;
    int roundCount;
    int roundStartSeat = 0;   // 本轮的起点座位（开局时的当前回合角色；主公先动时可能不是 0）
    int turnsWithoutDamage = 0;   // F4 僵局检测：连续多少个回合没有任何伤害
    int turnsWithoutDeath = 0;    // F4 僵局检测：连续多少个回合没有任何死亡（更能识别“奶妈 vs 奸雄”型僵局）
    static constexpr int kStalemateTurnLimit = 80;    // 约 8~10 轮无人受伤即判僵局
    // 阈值取 120 而不是更小：实测 60 会误伤正常慢局（8 人局前 7 轮无人阵亡并不罕见），
// 120 回合（8 人局≈15 轮 / 3 人局≈40 轮）无阵亡只出现在“奶妈自守 vs 奸雄”这类真僵局。
static constexpr int kStalemateDeathLimit = 120;
    // AI 行为证据表：key = actorId*64 + targetId，value = 各次记录的（轮次, 分值）。
    // 保留轮次是为了做**时效衰减**（E2①）：越早的证据权重越低（每早一轮 ×0.8，最低 0.3）。
    std::map<long long, std::vector<std::pair<int, int>>> observedRelations;     // 当前轮数（从 1 开始）
    std::deque<PlayerPtr> scheduledExtraTurns;
    bool inChainTransmission = false;   // 铁索传导进行中标志（焚涛）
    bool inDamage = false;               // 伤害结算进行中（屯田“非伤害失去”剔除）
    bool roundHadDeath = false;          // 本轮已有角色死亡（迂难）
    std::vector<PlayerPtr> lastResolvedTargets; // 最近一次使用牌的目标
    GameMode gameMode = GameMode::JUNZHENG; // 默认军争（身份）
    // ---- 斗地主（叫分/选将/专属技能）----
    int ddzLandlordSeat = 0;      // 地主座位
    int ddzBaseScore = 1;         // 叫分（底分）
    bool quietSetup = false;      // 叫分预览用临时建局：抑制初始化日志
    // ---- 开局换牌（游戏开始前，任何技能都未发动）----
    bool mulliganEnabled = false; // 建局时按“是否有真人座位”设置
    bool mulliganExplicit = false; // 玩家/测试显式设置过开关（建局时不覆盖）
    int mulliganMaxSwaps = 7;     // 每人最多换牌次数
    std::vector<int> mulliganSwaps; // 各座位实际换牌次数
    void runStartMulligan();      // 发完初始手牌、技能发动之前执行
    // 死亡结算窗口：仅在此期间允许“死亡时”技能（断肠/行殇/武魂等）为死者本人触发；
    // 窗口关闭后，已阵亡角色的技能一律不再发动（规则：死亡后非特殊技能不发动）。
    int resolvingDeathPlayerId = -1;
    int ddzAiBidForTesting = -1;  // 测试辅助：AI 叫分覆盖
    // ---- 场次与身份场选将（用户 2026-10-05 裁定）----
    bool zhizunField = false;      // 至尊场：按当前模式使用对应禁止池（见 HeroTier / docs/zhizun_field_rules.md）
    bool identityDraft = false;    // 身份场选将：主公先选并亮出，其余按座次“针对或辅助”选将
    int preferredIdentity = -1;    // 玩家指定身份：-1 随机 / 0 主公 / 1 忠臣 / 2 反贼 / 3 内奸
    // 身份场各座位选将框（用户 2026-10-05）：主公 10（4 主公武将 + 6 常规）、忠臣/内奸 5、反贼 4
    std::vector<std::vector<std::string>> idCandidates;
    std::vector<std::string> idPreselect;  // 各座位的系统预选（普通场随机 / 至尊场选将 AI 排名）
    std::vector<std::string> idPool;                    // 身份场候选池（未亮出）
    std::vector<std::string> idUsedPersons;             // 已亮出候选的人物（personKey 去重）
    std::vector<std::string> availableHeroPool() const; // 全部武将 id（至尊场则按当前模式剔除禁将）
    // 当前模式（身份场/斗地主）的禁将判定：至尊场下查该模式的禁止池，普通场恒 false
    HeroTier::Mode banMode() const;                    // 当前模式对应的禁止池
    bool isHeroBannedInCurrentMode(const std::string& id) const;
    // 选将框取牌：普通场随机；至尊场由选将 AI 排名（有概率取第二名）
    int draftPickIndex(const std::vector<std::string>& cands, HeroTier::Field field,
                       Identity role, const HeroPtr& lordHero);
    void logZhizunBanner() const;                       // 开局打印至尊场移除说明
    void applyPreferredIdentity(std::vector<Identity>& identities, int humanIndex); // 指定身份入座
    int lordSeat() const;                                // 主公座位（身份场；找不到则 0）
    void initIdentityHeroPool(int humanIndex, const std::string& preferred);
    bool drawIdentityCandidate(int seat, bool lordOnly = false); // lordOnly：只抽拥有主公技的武将
    bool isLordHero(const std::string& id);                       // 该武将是否拥有主公技
    void logIdentityCandidates(const std::string& tag) const;
    int askIdentityDraftChoice(int seat, const HeroPtr& lordHero, int positionHint = -1);
    // D1：AI 也会用换将次数——整框候选都偏弱（最强者低于门槛）时，换掉最弱的一个再评一次
    void aiImproveDraftBox(int seat, HeroTier::Field field, Identity role, const HeroPtr& lordHero,
                           int swapsLeft, bool identityField);
    static constexpr int kAiSwapScoreThreshold = 250; // 低于此分（弱将档 100+体力）才值得换
    // 开局换牌：AI 只换掉“价值低于此值”的牌（AIController::cardValue 口径：闪电 2、酒 3、
    // 杀/兵粮 4，闪/乐/借刀/五谷/桃园 5……），保证不会把杀闪桃换掉。
    static constexpr int kMulliganAiKeepValue = 4;
    static constexpr int kAiSideConfidence = 2;       // E2④：阵营判定的置信阈值（一次【桃】−3 或一次伤害 +2 才算数）
    int idDraftPosition = -1;   // 当前选将者的座次提示（D4）：0 先手位 / 1 中间 / 2 末位
    std::vector<int> idDraftPositionUsed;   // 各座位实际使用的座次提示（供测试精确重算评分）
    std::vector<int> ddzDraftPositionUsed;
    std::vector<std::string> chooseIdentityHeroes();
    std::vector<std::vector<std::string>> ddzCandidates; // 各座位选将候选（地主 5、农民 3）
    std::vector<std::string> ddzPreselect;               // 各座位的系统预选
    std::vector<std::string> ddzPool;                     // 选将/换将的候选池（未亮出）
    std::vector<std::string> ddzUsedPersons;              // 已亮出候选的人物（personKey 去重）
    int runDoudizhuBidding(int humanIndex);                       // 返回地主座位
    void initDoudizhuHeroPool(int humanIndex, const std::string& preferred); // 候选池初始化
    bool drawDoudizhuCandidate(int seat);                         // 为座位抽 1 个候选
    void logDoudizhuCandidates(const std::string& tag) const;     // 亮将日志（各座位候选人）
    int askDoudizhuDraftChoice(int seat);                         // 人类选将（含换将 2 次/位置）；AI 随机
    std::vector<std::string> chooseDoudizhuHeroes(int humanIndex); // 返回 3 个座位的武将 id
    void logDoudizhuSettlement() const;                           // 结算日志
    std::vector<CardPtr> exilePile;       // 游戏外牌堆
    PlayerPtr checkingSource;            // canBeTargeted 检查中的来源角色
    // 延时锦囊在其他角色回合才结算；保留当时的来源，供无懈连锁判断阵营。
    std::unordered_map<const Card*, int> delayedTrickSources;
    int extraTurnDepth = 0;
    std::vector<CardPtr> discardedThisPhase;
    std::string judgeReason; // 正在进行的判定原因（如“八卦阵”），供改判技能识别判定时机
    // 协力任务（谋·张飞/谋·赵云等）：记录发起者与协力对象的四类计数
    struct XieLiRecord {
        int initiatorId = -1;
        int partnerId = -1;
        std::string skill; // 如“协击”/“积著”
        int damage = 0; // 同仇：双方造成伤害之和
        int draws = 0;  // 并进：双方摸牌之和
        std::set<Suit> discardSuits; // 疏财：弃置花色集合（双方）
        std::set<Suit> useSuits;     // 勠力：使用/打出花色集合（双方）
        bool resolved = false;
    };
    std::vector<XieLiRecord> xieLiRecords;

    // 输出与交互均通过可替换组件注入（解耦控制台 / GUI / 测试）
    std::shared_ptr<Logger> logger;
    std::unique_ptr<Interaction> ui;

    void runTurnPhases(PlayerPtr player);
    void notifyPhaseSkipped(PlayerPtr player, TurnPhase phase);
    void humanPlayLoop(PlayerPtr player);
    // 已确定使用方式（可能是转化牌）后：选择目标并使用
    bool humanUseChosenCard(PlayerPtr player, CardPtr chosen);
    void aiPlayLoop(PlayerPtr player);
    // 记录并广播一次真正的“使用牌”事件（不含打出响应）。
    void notifyCardUsed(PlayerPtr source, CardPtr card);
    void notifyAnyCardPlayed(PlayerPtr user, CardPtr card);

public:
    GameEngine();
    // unique_ptr<Interaction> 需要完整类型，析构在 .cpp 中定义
    ~GameEngine();

    // 日志注入：默认输出到标准输出；测试可注入字符串流捕获日志
    void setLogger(std::shared_ptr<Logger> newLogger);
    // 底层输出流（log() << "..." << std::endl 风格）
    std::ostream& log();
    std::ostream& log() const;

    // heroIds[i] 为第 i 名玩家的武将 id；为空或数量不足时从武将池随机补齐。humanIndex < 0 表示全 AI。
    void initGame(int totalPlayers = 4, int humanIndex = 0, const std::vector<std::string>& heroIds = {});
    // 斗地主：3 人（1 地主 + 2 农民），所有人初始 4 张手牌，地主 +1 体力上限并先手。
    // landlordIndex >= 0：直接指定地主（测试/兼容，不做叫分/选将）。
    // landlordIndex < 0 且 fullFlow=true：完整流程——亮 3 将 → 抢地主（叫分）→ 地主追加 2 将 →
    // 选将（地主 5 选 1、农民 3 选 1；位置可换将 2 次）→ 分发专属技能。
    void initDoudizhuGame(int humanIndex = 0, const std::vector<std::string>& heroIds = {},
                          int landlordIndex = 0, bool fullFlow = false);
    // 场次：至尊场＝按当前模式移除弱将（身份池 / 斗地主池各自独立，用户 2026-10-06）
    void setZhizunField(bool on) { zhizunField = on; }
    bool isZhizunField() const { return zhizunField; }
    // 当前模式禁止池名单（普通场返回空）
    std::vector<std::string> currentBanList() const;
    // ---- 开局换牌（用户 2026-10-06）----
    // “每个人可以换七次牌（初始手牌，属于游戏开始前，此时任何技能都未发动）”。
    // 默认：有真人座位时开启（AI 也参与换牌），全 AI 对局默认关闭以保持既有测试/随机分布稳定；
    // 需要时可显式 setMulliganEnabled(true/false)，并用 setMulliganMaxSwaps 调整次数（默认 7）。
    void setMulliganEnabled(bool on) { mulliganEnabled = on; mulliganExplicit = true; }
    bool isMulliganEnabled() const { return mulliganEnabled; }
    void setMulliganMaxSwaps(int n) { mulliganMaxSwaps = std::max(0, n); }
    int getMulliganMaxSwaps() const { return mulliganMaxSwaps; }
    // 某座位实际用掉的换牌次数（测试/展示用）
    int mulliganSwapsOf(int seat) const {
        return (seat >= 0 && seat < static_cast<int>(mulliganSwaps.size())) ? mulliganSwaps[seat] : 0;
    }
    // 身份场选将（主公先选完并亮出，别人再选针对或辅助）
    void setIdentityDraft(bool on) { identityDraft = on; }
    bool isIdentityDraft() const { return identityDraft; }
    const std::vector<std::vector<std::string>>& getIdentityCandidates() const { return idCandidates; }
    // 身份是否公开：真人自己 / 主公 / 斗地主地主 / 野心家 / 【择途】已明置身份牌 / 全 AI 观战
    bool isIdentityPublic(const Player& p) const;

    // ==================== AI 队友分辨（用户 2026-10-05：“AI 需分辨队友等”） ====================
    // 引擎记录**公开可见的行为证据**（谁对谁用了什么牌、造成了伤害、用【无懈可击】帮了谁），
    // AI 据此推断阵营，而不是直接读真实身份。
    void recordRelation(int actorId, int targetId, int delta); // delta>0 敌对，<0 友好
    int relationOf(int actorId, int targetId) const;           // 累计证据分（正＝敌对，负＝友好）
    void recordCardRelation(PlayerPtr source, CardPtr card, const std::vector<PlayerPtr>& targets);
    // 阵营估计：>0 偏主忠方，<0 偏反贼方，0 未知（按公开身份 + 行为证据）
    int aiSideEstimate(const Player& other) const;
    // AI 视角的“是否队友”（推断版；内奸/野心家没有队友，斗地主身份是明牌）
    bool aiIsFriend(const Player& self, const Player& other) const;
    // AI 用：按**公开的身份构成**（人数→1主1忠2反1内 等，开局即公示）+ 已公开身份 + 行为证据，
    // 估计主忠方/反贼方的存活人数；未被识别的人按剩余构成比例分摊（不读真实身份）。
    void aiEstimateSides(const Player& self, int& loyal, int& rebels) const;
    // B5 濒死救助优先级：队友必救；内奸/野心家按“维持平衡”决定（主公将死且反贼仍在才救主公）
    bool aiWantsToSave(const Player& saver, const Player& dying) const;
    int livingLordId() const;   // 当前存活主公的座位 id（含储君继位后的新主公），无则 -1
    // 某座位的系统预选武将 id（未记录则返回空串）
    std::string preselectOf(int seat, bool doudizhu) const;
    // 某座位选将时使用的座次提示（D4）；未记录返回 -1。测试用它精确重算 AI 评分排名。
    int draftPositionOf(int seat, bool doudizhu) const;
    // 选将框对比面板（用户 2026-10-05 选择“加对比面板”）：d1,3 并排比较两个候选
    std::string draftCompareText(const std::vector<std::string>& cands, const std::vector<int>& idxs,
                                 HeroTier::Field field, Identity role, const HeroPtr& lordHero) const;
    // 玩家指定身份：-1 随机（默认）/ 0 主公 / 1 忠臣 / 2 反贼 / 3 内奸
    void setPreferredIdentity(int role) { preferredIdentity = role; }
    int getPreferredIdentity() const { return preferredIdentity; }
    // 斗地主结果查询
    int getDoudizhuLandlordSeat() const { return ddzLandlordSeat; }
    int getDoudizhuBaseScore() const { return ddzBaseScore; }     // 叫分（全不叫按 1 分）
    const std::vector<std::vector<std::string>>& getDoudizhuCandidates() const { return ddzCandidates; }
    // 结算分数（座位→分数）：地主胜 地主 +2×底分、农民 −底分；农民胜反之
    std::vector<int> getDoudizhuScores() const;
    // 初始化内核：身份由调用方给出（身份模式=官方配置表；斗地主=1 地主 + 2 农民）。
    void setupGame(int totalPlayers, int humanIndex, const std::vector<std::string>& heroIds,
                   const std::vector<Identity>& identities, bool mulliganNow = false);
    void setSeed(unsigned newSeed);
    unsigned getSeed() const { return seed; }
    std::mt19937& getRng() { return rng; }
    // 当前判定的原因（doJudgement 传入的 reason）；判定结束后为空。
    const std::string& getJudgeReason() const { return judgeReason; }
    void setJudgeReasonForTesting(const std::string& reason) { judgeReason = reason; } // 测试：直接设置判定来源
    void setAiDelayMs(int ms) { aiDelayMs = ms; }

    const std::vector<PlayerPtr>& getPlayers() const { return players; }
    PlayerPtr getCurrentPlayer() const;
    PlayerPtr getPlayerById(int id) const;
    TurnPhase getCurrentPhase() const { return currentPhase; }
    int getCurrentRound() const { return roundCount; }
    bool isPlayerTurn(const Player& p) const;

    std::vector<PlayerPtr> getAlivePlayers() const;
    std::vector<PlayerPtr> getOtherAlivePlayers(const Player& self) const;
    int calculateDistance(const Player& from, const Player& to) const;
    std::vector<PlayerPtr> getPlayersInRange(const Player& from, int range) const;
    std::vector<PlayerPtr> getPlayersAtDistance(const Player& from, int distance) const;

    // ---- 技能相关 ----
    // 当前生效的技能（"非锁定技失效"状态下只返回锁定技/觉醒技）
    std::vector<SkillPtr> getEffectiveSkills(const Player& p) const;
    void removeHeroSkills(PlayerPtr player); // 断肠等移除全部可失去技能，并清理其持续效果（持恒技保留）
    bool removeHeroSkill(PlayerPtr player, const std::string& name); // 单独移除借得/临时技能也需清理持续状态；持恒技不可移除
    int calculateHandLimit(PlayerPtr player);
    int getShaLimit(const Player& p) const;
    int getShaTargetLimit(const Player& p, CardPtr sha) const;
    int getShaLimitForCard(const Player& p, CardPtr sha) const;
    // 人类玩家出牌阶段入口（CLI/测试用）：选牌使用或直接发动转化技能。
    bool humanUseCard(PlayerPtr player, CardPtr card);
    // 出牌阶段直接发动某个转化类技能：由技能筛选候选牌后使用（2026-10-06）
    bool humanUseConversionSkill(PlayerPtr player, SkillPtr skill);
    bool canUseOriginalCard(const Player& p, CardPtr card) const;
    bool canUseShaOn(const Player& source, const Player& target, CardPtr sha,
                     bool ignoreDistance = false) const;
    bool canUseShaBeyondLimitOn(const Player& source, const Player& target, CardPtr sha) const;
    bool canUseBingLiangOn(const Player& source, const Player& target, CardPtr card = nullptr) const;
    bool canUseShunShouOn(const Player& source, const Player& target, CardPtr card) const;
    std::vector<PlayerPtr> getShaTargets(const Player& source, CardPtr sha) const;
    // 判定区内是否已有某种延时锦囊（同名延时锦囊不能重复放置）
    static bool hasJudgeCardOf(const Player& p, CardSubType type);
    // 可以当作 wanted 类型使用/打出的所有候选牌（实体牌 + 技能转化牌 + 丈八蛇矛占位牌）
    std::vector<CardPtr> getResponseCandidates(PlayerPtr player, CardSubType wanted) const;
    std::vector<CardPtr> getConversionsFor(PlayerPtr player, CardPtr card, CardSubType wanted) const;
    // —— 转化类技能的统一入口（2026-10-06 重写）——
    // 任意“可以用被转化出来的牌”的时机：枚举某张实体牌经一个技能 / 全部技能可变成的所有牌。
    // 与 getConversionsFor 的区别是不再依赖调用方写死的目标类型表（旧实现对出牌阶段写死了 16 种）。
    std::vector<CardPtr> getSkillConversionsFor(const SkillPtr& skill, PlayerPtr player, CardPtr card) const;
    std::vector<CardPtr> getAllConversionsFor(PlayerPtr player, CardPtr card) const;
    // 出牌阶段粗筛：该牌（含转化牌）此刻是否“适合”主动使用（无合法目标/不能主动使用的牌会被滤掉）。
    bool canPlayCardNow(const Player& player, CardPtr card) const;

    // ---- 装备效果（实现见 src/Equipment.cpp）----
    static bool hasWeapon(const Player& p, const std::string& name);
    static bool hasArmor(const Player& p, const std::string& name);
    // 角色失去装备区的牌后（白银狮子：回复 1 点体力）
    void afterEquipmentLost(PlayerPtr owner, CardPtr card);
    // 丈八蛇矛：占位牌（尚未选定两张手牌）→ 选定后生成真正的转化【杀】；返回 nullptr 表示放弃
    static bool isZhangBaPlaceholder(const CardPtr& card);
    CardPtr materializeZhangBaSha(PlayerPtr player, CardPtr placeholder);
    CardPtr materializeConversion(PlayerPtr player, CardPtr card);
    // 方天画戟：此【杀】是否为你最后的手牌（可额外指定至多两个目标）
    bool canUseFangTianExtraTargets(const Player& source, CardPtr sha) const;
    // 借刀杀人：可选的持武器角色 / 某角色攻击范围内可被指定的角色
    std::vector<PlayerPtr> getJieDaoVictims(const Player& holder) const;
    std::vector<PlayerPtr> getJieDaoWeaponHolders(const Player& user) const;
    // 【杀】被【闪】抵消后的武器效果：贯石斧（返回 true 表示此【杀】依然造成伤害）
    bool weaponAfterShaDodged(ShaContext& ctx);
    // 【杀】即将造成伤害时的武器效果：麒麟弓 / 寒冰剑（返回 true 表示伤害被防止）
    bool weaponBeforeShaDamage(ShaContext& ctx);
    // 【杀】被【闪】抵消后：青龙偃月刀追杀
    void weaponQingLongChase(ShaContext& ctx);
    // 让角色选择并"使用"一张【杀】（含转化），已从区域移除但尚未结算；返回 nullptr 表示放弃
    CardPtr askUseSha(PlayerPtr player, const std::string& prompt, bool aiWants,
                      PlayerPtr requiredTarget = nullptr, bool ignoreDistance = false,
                      bool optional = true);

    // ---- 交互辅助（自动区分 AI / 人类，实现委托给 Interaction）----
    void viewCards(PlayerPtr viewer, const std::vector<CardPtr>& cards, const std::string& prompt);
    bool askConfirm(PlayerPtr player, const std::string& prompt, bool aiDecision = true);
    CardPtr askChooseCard(PlayerPtr player, const std::vector<CardPtr>& candidates, const std::string& prompt,
                          bool optional, CardPtr aiChoice = nullptr);
    PlayerPtr askChoosePlayer(PlayerPtr player, const std::vector<PlayerPtr>& candidates, const std::string& prompt,
                              bool optional, PlayerPtr aiChoice = nullptr);
    int askChooseOption(PlayerPtr player, const std::vector<std::string>& options, const std::string& prompt, int aiChoice = 0);
    // 选择其他角色的隐藏手牌：仅展示牌背序号，不泄露牌面。
    CardPtr chooseHiddenHandCard(PlayerPtr chooser, PlayerPtr owner, const std::string& prompt);
    // 默认选择手牌/装备区的一张牌，手牌不可见；原文明确指向区域时可包含判定区。
    CardPtr chooseCardFromPlayer(PlayerPtr chooser, PlayerPtr owner, const std::string& prompt,
                                 bool includeJudgeZone = false);

    // ---- 牌的移动 ----
    void drawCards(PlayerPtr player, int count, const std::string& reason = "");
    // 【恃才】等效果：将刚使用的一张实体牌从弃牌堆/场上/获得区移动到牌堆顶。
    bool canPutUsedCardOnTop(CardPtr usedCard) const;
    bool putUsedCardOnTop(PlayerPtr source, CardPtr usedCard);
    // 通用失去手牌入口：通知连营、屯田等效果，不把牌自动放入弃牌堆。
    bool loseHandCard(PlayerPtr owner, CardPtr card);
    // 将一张手牌置入装备区，旧装备按失去/弃置规则结算。
    bool equipHandCard(PlayerPtr from, PlayerPtr to, CardPtr card);
    // 使用/打出一张牌（含转化牌）：从手牌/装备区移除实体牌并置入弃牌堆
    void consumeCard(PlayerPtr player, CardPtr card, bool isUse);
    // 弃置某角色区域里的一张牌（手牌/装备/判定区）
    void discardCardOf(PlayerPtr owner, CardPtr card, const std::string& reason = "",
                           PlayerPtr instigator = nullptr);
    // 令角色获得一张牌（从原持有者处移除）
    void obtainCard(PlayerPtr to, CardPtr card, PlayerPtr from = nullptr);
    // 批量获得/失去通知（一次获得≥2张、一次失去≥2张的技能触发点）
    void notifyCardsObtained(PlayerPtr to, int count);
    void notifyCardsLostBatch(PlayerPtr victim, int count, PlayerPtr instigator);
    // 蓄力点统一消耗入口（消耗后广播 onChargeConsumed，供凿险等“一次性消耗”类技能）
    void consumeCharge(PlayerPtr player, int count);
    // 伤害结算进行中（屯田“非伤害失去”精确判定）
    bool isInDamage() const { return inDamage; }
    // 铁索传导进行中（焚涛等“传导中的伤害”类效果）
    bool isInTransmission() const { return inChainTransmission; }
    // 当前【成为目标检查】中的使用者（canBeTargeted 期间有效；芳踪等目标侧技能用）
    PlayerPtr getCheckingSource() const { return checkingSource; }
    // 本轮是否已有角色死亡（势·迂难觉醒条件）
    bool isAnyPlayerDeadThisRound() const { return roundHadDeath; }
    // 协力（谋·张飞/谋·赵云等）：发起、计数与结算
    void startXieLi(PlayerPtr initiator, PlayerPtr partner, const std::string& skill);
    bool isXieLiSuccess(const XieLiRecord& r) const; // 四类任一达标即成功
    void updateXieLiOnDamage(PlayerPtr source, int dmg);
    void updateXieLiOnDraw(PlayerPtr who, int n);
    void updateXieLiOnDiscard(PlayerPtr who, Suit suit);
    void updateXieLiOnUse(PlayerPtr who, Suit suit);
    void resolveXieLiAtTurnEnd(PlayerPtr turnOwner);
    GameMode getGameMode() const { return gameMode; }
    void setGameMode(GameMode m) { gameMode = m; }
    // 除外区（2026-10-04 裁定 B：由原“游戏外”升级而来；巨象「随机从游戏外」等效果并入本区）。
    // 除外区中的牌不计入牌堆/弃牌堆，只能被「从除外区获得/使用」类效果取回。
    std::vector<CardPtr>& exileZone() { return exilePile; }
    CardPtr takeFromExile(const std::function<bool(const CardPtr&)>& pred, std::mt19937& rng);
    // 将一张实体牌移入除外区（自动从手牌/装备区/判定区/弃牌堆中移出；已在除外区的幂等忽略）。
    bool moveToExile(CardPtr card);
    // 一张牌当前是否位于除外区。
    bool isInExile(CardPtr card) const;

    // 技能状态按“出牌阶段”重置（流离额外出牌阶段：出牌阶段限一次类可再发动）
    void resetPlayPhaseUses(PlayerPtr player);
    // 最近一次 useCard 的目标（戮连等“结算后查看目标”类技能）
    std::vector<PlayerPtr> getCardTargets(CardPtr) const { return lastResolvedTargets; }
    // 本轮是否已有角色死亡（势·迂难觉醒条件）
    // 好施等：查询可借用手牌的来源角色（无则返回空）
    PlayerPtr getBorrowedHandFrom(PlayerPtr player) const;
    // 原子交换两人的整副手牌；失去最后手牌事件只在交换完成后按最终手牌数判定。
    void swapHands(PlayerPtr a, PlayerPtr b);
    // 合法地迁移场上装备/延时锦囊，不隐式顶替装备或叠加同名判定牌。
    bool canMoveFieldCard(PlayerPtr from, PlayerPtr to, CardPtr card) const;
    bool moveFieldCard(PlayerPtr from, PlayerPtr to, CardPtr card);
    bool canUsePeach(PlayerPtr actor, PlayerPtr recipient) const;
    // 向选择者从候选实体牌中选择恰好 count 张（先全部选好再结算费用）。
    std::vector<CardPtr> chooseCards(PlayerPtr chooser, std::vector<CardPtr> candidates,
                                    int count, const std::string& prompt);

    void recoverHp(PlayerPtr player, int amount, const std::string& reason = "", PlayerPtr source = nullptr);
    void loseHp(PlayerPtr player, int amount, const std::string& reason = "", PlayerPtr source = nullptr);   // 失去体力（非伤害，如苦肉）

    // 花色修正统一入口：小乔【红颜】将自己的黑桃牌视为红桃牌。
    Suit effectiveSuit(const Player& owner, CardPtr card) const;
    CardPtr doJudgement(PlayerPtr target, const std::string& reason, bool toDiscard = true,
                        bool* claimedOut = nullptr);
    // 双方各亮出一张手牌，点数高者赢；双方牌均进入弃牌堆（平局为未赢）。
    bool pindian(PlayerPtr initiator, PlayerPtr opponent, const std::string& reason,
                 std::vector<CardPtr>* revealed = nullptr);

    void startGame();
    // 游戏开始时点的技能（七星/狂暴/化身/明任/妙略/结姻/权谋等）与蓄力技初始点数：
    // 用户 2026-10-05 明确“所有游戏开始时点的技能都没有发动”是错的——必须发动，
    // 且斗地主路径此前因 quietSetup 提前返回而漏发，现由本方法统一触发。
    void runGameStartHooks();
    // 主公技（用户 2026-10-05）：当且仅当你是主公时才会在游戏开始时获得；只要拥有就能使用。
    void applyLordSkillsByIdentity();     // 建局时按身份授予/剥夺
    void stripLordSkills(PlayerPtr player);
    void grantLordSkills(PlayerPtr player);
    // 一轮开始：广播 onRoundStart（游戏开始即第 1 轮开始）
    void broadcastRoundStart();
    // 多个额外回合按获得顺序逐个执行，不会被后来的技能覆盖。
    void scheduleExtraTurn(PlayerPtr target) { if (target && target->isAlive()) scheduledExtraTurns.push_back(target); }
    void runTurn(PlayerPtr player);
    // 测试/回放辅助：直接设置当前玩家与阶段（运行中由 startGame 维护）。
    void setCurrentPlayerForTesting(PlayerPtr player) {
        if (!player) return;
        for (size_t i = 0; i < players.size(); ++i)
            if (players[i] && players[i]->getId() == player->getId()) {
                currentTurnIndex = static_cast<int>(i);
                return;
            }
    }
    void setPhase(TurnPhase phase) { currentPhase = phase; }
    void setRoundForTesting(int round) { roundCount = round; } // 测试/回放辅助：直接设置轮次
    // 测试辅助：固定 AI 的叫分行为（-1=用内置启发式；0=总是不叫；1/2/3=总是叫该分值或更高时取该值）
    void setDoudizhuAiBidForTesting(int bid) { ddzAiBidForTesting = bid; }

    void phasePreparation(PlayerPtr player, bool& skipTurn);
    void phaseJudgement(PlayerPtr player, bool& skipPlay, bool& skipDraw);
    void phaseDraw(PlayerPtr player);
    void phasePlay(PlayerPtr player);
    void phaseDiscard(PlayerPtr player);
    void phaseFinish(PlayerPtr player);

    // 蛊惑：所有质疑者一并验真；真则各失去1点体力，假则各摸一张牌；被质疑后仅红桃真牌生效。
    bool resolveGuHuo(PlayerPtr source, CardPtr card);
    bool useCard(PlayerPtr source, CardPtr card, std::vector<PlayerPtr> targets);
    // 结算一张已从区域移除的【杀】：依次对所有目标结算，最后处理实体牌去向（弃牌堆 / 被替身获得）
    void resolveSha(PlayerPtr source, CardPtr card, const std::vector<PlayerPtr>& targets);
    // 技能视为使用无距离限制的杀；不占用出牌阶段的常规杀次数。
    void playSkillSha(PlayerPtr source, const std::string& skill);
    // 对单个目标结算【杀】。返回值：【杀】的实体牌是否已被技能获得（为真则不进弃牌堆）
    bool handleSha(PlayerPtr source, PlayerPtr target, CardPtr card, bool drunk = false, bool alreadyClaimed = false);
    void handleTao(PlayerPtr source, CardPtr card);
    void handleJiu(PlayerPtr source, CardPtr card);
    bool handleTrick(PlayerPtr source, CardPtr card, std::vector<PlayerPtr> targets);   // 返回伤害牌是否被认领（奸雄）
    // 当前正在结算的【决斗】的使用者；不在决斗结算中时为空。
    PlayerPtr getActiveDuelUser() const { return activeDuelUser; }
    bool resolveJueDou(PlayerPtr attacker, PlayerPtr target, CardPtr card,
                        bool targetsNotified = false);                  // 决斗结算（供【离间】复用）

    bool askNullification(PlayerPtr source, PlayerPtr target, CardPtr trickCard);
    // 请求响应牌：返回被打出的牌（可能为转化牌），并已完成移除与弃置
    CardPtr askResponseCard(PlayerPtr player, CardSubType requestedType, const std::string& prompt);

    // ignoreArmor：此伤害无视目标防具（青釭剑的【杀】：藤甲不加伤、白银狮子不减伤）
    void applyDamage(PlayerPtr source, PlayerPtr target, int damage, ShaElement element = ShaElement::NORMAL, bool ignoreArmor = false, CardPtr cause = nullptr, bool* causeClaimed = nullptr,
                     bool sourceModifiersApplied = false);
    void processDying(PlayerPtr dyingPlayer, PlayerPtr source);
    void handlePlayerDeath(PlayerPtr deadPlayer, PlayerPtr killer);
    void cleanupDeadPlayerZones(PlayerPtr deadPlayer); // 阵亡区域清理（不触发技能）
    // 「直接死亡」效果不进入濒死/桃的救援窗口。
    void killPlayer(PlayerPtr victim, PlayerPtr source = nullptr);

    bool isGameOver() const { return gameOver; }
    const std::string& getWinningFaction() const { return winningFaction; }
    void checkGameOver();
    void printGameState() const;
    // 出牌阶段提示（插入式面板返回后重印原问题用）
    void printPlayPhasePrompt(PlayerPtr player);
    // 观察局势：公开状态/场上的牌（技能状态、蓄力点、判定区、武将牌上的牌；所有玩家可见）
    std::string describePublicState(const Player& player) const;

    // 蓄力点的统一入口：获得的蓄力点不超过武将牌上标注的“蓄力技（x/y）”上限 y
    // （上限为 0 的技能自行管理，例如势·邓艾【屯田】0/0）。返回实际获得的点数。
    int gainCharge(Player& target, int amount);
    // 观察局势：私有状态（仅玩家本人可见，如手牌；供己方视角使用）
    std::string describePrivateState(const Player& player) const;
    void logMessage(const std::string& msg) const;

    bool canBeTargeted(PlayerPtr target, CardPtr card, PlayerPtr source = nullptr);   // 目标合法性（如【空城】）
    bool canTakeCardEffect(PlayerPtr target, CardPtr card, PlayerPtr source = nullptr); // 已成为目标后是否生效（如【祸首】）
    void notifyCardLostOutsideTurn(PlayerPtr owner, CardPtr card);
    void notifyAnyCardLost(PlayerPtr owner, CardPtr card);
    // 借出的手牌被他人使用/打出（势-好施）：通知牌的实际来源
    void notifyBorrowedHandCardUsed(PlayerPtr lender, PlayerPtr borrower, CardPtr card);
    void notifyHandCardLostToOther(PlayerPtr victim, PlayerPtr instigator);
    void notifyHandEmpty(PlayerPtr player, int beforeCount); // 失去最后一张手牌后广播（如【连营】）
    Deck& getDeck() { return deck; }
    const Deck& getDeck() const { return deck; }
};

} // namespace Thks

#endif // THKS_GAME_ENGINE_H
