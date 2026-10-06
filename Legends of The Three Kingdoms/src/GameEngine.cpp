#include "GameEngine.h"
#include "AI.h"
#include "Codex.h"
#include "Interaction.h"
#include "Logger.h"
#include "Platform.h"
#include "HeroRegistry.h"
#include "HeroTier.h"
#include "SkillsMyth.h"
#include "SkillsMode.h"
#include "Roles.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <set>

namespace Thks {

namespace {

CardPtr wumouMaterial(CardPtr sha) {
    if(!sha || sha->getSkillSource()!="无谋" || sha->getSubCards().size()!=1)return nullptr;
    auto material=sha->getSubCards().front();
    return material && material->getType()==CardType::TRICK ? material : nullptr;
}
bool wumouAllTargets(CardSubType st) {
    return st==CardSubType::NAN_MAN_RU_QIN || st==CardSubType::WAN_JIAN_QI_FA ||
           st==CardSubType::TAO_YUAN_JIE_YI || st==CardSubType::WU_GU_FENG_DENG;
}

bool isTrickStayingOnTable(const CardPtr& card) {
    auto st = card->getSubType();
    return st == CardSubType::LE_BU_SI_SHU || st == CardSubType::BING_LIANG_CUN_DUAN || st == CardSubType::SHAN_DIAN;
}

} // namespace

GameEngine::GameEngine()
    : currentTurnIndex(0), currentPhase(TurnPhase::NONE), gameOver(false), winningFaction(""),
      interactiveMode(true), aiDelayMs(300), seed(0), roundCount(0), logger(std::make_shared<Logger>()) {
    ui = std::make_unique<Interaction>(*this, *logger);
    unsigned s = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
    const char* env = std::getenv("THKS_SEED");
    if (env && *env) {
        s = static_cast<unsigned>(std::strtoul(env, nullptr, 10));
    }
    setSeed(s);
    // 可选：通过环境变量 THKS_AI_DELAY（毫秒）调整电脑行动间隔，0 表示不等待
    const char* delayEnv = std::getenv("THKS_AI_DELAY");
    if (delayEnv && *delayEnv) {
        aiDelayMs = static_cast<int>(std::strtol(delayEnv, nullptr, 10));
        if (aiDelayMs < 0) aiDelayMs = 0;
    }
}

GameEngine::~GameEngine() = default;

void GameEngine::setLogger(std::shared_ptr<Logger> newLogger) {
    if (!newLogger) return;
    logger = std::move(newLogger);
    ui = std::make_unique<Interaction>(*this, *logger);
}

std::ostream& GameEngine::log() {
    return logger->raw();
}

std::ostream& GameEngine::log() const {
    return logger->raw();
}

void GameEngine::logMessage(const std::string& msg) const {
    logger->log(msg);
}

void GameEngine::setSeed(unsigned newSeed) {
    seed = newSeed;
    rng.seed(seed);
    deck.setSeed(seed);
}

namespace {
// 选将/亮将日志里的武将标签：“张辽（山包）”
// 日志用短标签：名称（包名）
std::string heroDraftShort(const std::string& id) {
    const HeroInfo* info = HeroRegistry::find(id);
    return info ? info->name + "（" + info->pack + "）" : id;
}

// 选将框里一名候选的展示文本（用户 2026-10-05 指定字段）：
// 武将名（包名） 势力 体力x/y ｜技能名（主公技标注）
std::string heroDraftLabel(const std::string& id) {
    const HeroInfo* info = HeroRegistry::find(id);
    if (!info) return id;
    std::string label = info->name + "（" + info->pack + "）";
    HeroPtr h = HeroRegistry::create(id);
    if (!h) return label;
    label += " " + h->getCountryString() + " " + std::to_string(h->getMaxHp()) + "/" + std::to_string(h->getMaxHp());
    std::string skills;
    for (const auto& sk : h->getSkills()) {
        if (!sk) continue;
        if (!skills.empty()) skills += "、";
        skills += sk->getName();
        if (sk->hasTag(SkillTag::LORD)) skills += "(主公技)";
    }
    if (!skills.empty()) label += " ｜" + skills;
    return label;
}
} // namespace

int GameEngine::draftPositionOf(int seat, bool doudizhu) const {
    const auto& v = doudizhu ? ddzDraftPositionUsed : idDraftPositionUsed;
    if (seat < 0 || seat >= static_cast<int>(v.size())) return -1;
    return v[seat];
}

std::string GameEngine::preselectOf(int seat, bool doudizhu) const {
    const auto& v = doudizhu ? ddzPreselect : idPreselect;
    if (seat < 0 || seat >= static_cast<int>(v.size()) || v[seat].empty()) return "（未记录）";
    return v[seat];
}

// 对比面板：把选中的候选并排列出（势力/体力/技能全文/三档强度/本身份评分）
std::string GameEngine::draftCompareText(const std::vector<std::string>& cands, const std::vector<int>& idxs,
                                        HeroTier::Field field, Identity role, const HeroPtr& lordHero) const {
    std::ostringstream ss;
    ss << "\n---------- 候选对比（输入 0 返回选将） ----------\n";
    for (int idx : idxs) {
        if (idx < 1 || idx > static_cast<int>(cands.size())) continue;
        const std::string id = cands[idx - 1];
        HeroPtr h = HeroRegistry::create(id);
        const HeroInfo* info = HeroRegistry::find(id);
        ss << "【" << (idx) << "】" << (info ? info->name : id) << "（" << (info ? info->pack : "?") << "）\n";
        if (!h) continue;
        ss << "    势力: " << h->getCountryString() << " | 体力: " << h->getMaxHp() << "/" << h->getMaxHp()
           << " | 称号: " << h->getTitle() << "\n";
        ss << "    强度档: 身份场 " << HeroTier::tierName(HeroTier::tierOf(id, HeroTier::Field::IDENTITY))
           << " / 地主 " << HeroTier::tierName(HeroTier::tierOf(id, HeroTier::Field::DDZ_LANDLORD))
           << " / 农民 " << HeroTier::tierName(HeroTier::tierOf(id, HeroTier::Field::DDZ_FARMER))
           << (HeroTier::isBannedInZhizun(
                   id, gameMode == GameMode::DOUDIZHU ? HeroTier::Mode::DOUDIZHU : HeroTier::Mode::IDENTITY)
                   ? "（本模式禁止池）" : "") << "\n";
        AIController::DraftContext ctx;
        ctx.field = field;
        ctx.role = role;
        ctx.lordHero = lordHero;
        ss << "    本身份评分: " << AIController::draftScore(h, ctx) << "\n";
        for (const auto& sk : h->getSkills()) {
            if (!sk) continue;
            ss << "    【" << sk->getName() << "】" << sk->getKindString();
            if (!sk->getTagString().empty()) ss << "·" << sk->getTagString();
            ss << "：" << sk->getDescription() << "\n";
        }
        ss << "    依据: " << HeroTier::rationale(id) << "\n";
    }
    ss << "-----------------------------------------------\n";
    return ss.str();
}

// 解析对比指令 d1,3 / d1 3 / d1，3 → 候选编号列表（最多并排 3 个）
std::vector<int> parseCompareIndices(const std::string& body) {
    std::vector<int> out;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty()) return;
        int v = Interaction::parseInt(cur);
        if (v > 0) out.push_back(v);
        cur.clear();
    };
    for (char c : body) {
        if (c >= '0' && c <= '9') { cur.push_back(c); continue; }
        flush();
    }
    flush();
    if (out.size() > 3) out.resize(3);
    return out;
}

// ==================== AI 队友分辨（用户 2026-10-05：“AI 需分辨队友等”） ====================
//
// 原则：AI 不再直接读真实身份，而是用**公开可见的信息 + 行为证据**推断阵营。
//   公开信息：主公（选将后亮出/储君继位）、斗地主的明牌身份、【择途】明置的身份牌、
//             野心家、阵亡者（死亡时揭示身份）、储君标记。
//   行为证据：谁对谁使用了【杀】/锦囊、造成了伤害（敌对），谁用【桃】救谁、
//             谁用【无懈可击】替谁挡锦囊（友好）。
// 推断结果只用于 AI 决策，不改变任何规则结算。

void GameEngine::recordRelation(int actorId, int targetId, int delta) {
    if (actorId < 0 || targetId < 0 || actorId == targetId || delta == 0) return;
    observedRelations[static_cast<long long>(actorId) * 64 + targetId].push_back({roundCount, delta});
}

// E2①：证据按轮次衰减——当轮 100%，每早一轮 ×0.8，最低保留 30%（第一轮的旧账不该和第五轮等权）。
int GameEngine::relationOf(int actorId, int targetId) const {
    if (actorId < 0 || targetId < 0 || actorId == targetId) return 0;
    auto it = observedRelations.find(static_cast<long long>(actorId) * 64 + targetId);
    if (it == observedRelations.end()) return 0;
    double sum = 0.0;
    for (const auto& rec : it->second) {
        int age = roundCount - rec.first;
        if (age < 0) age = 0;
        double w = 1.0;
        for (int i = 0; i < age; ++i) w *= 0.8;
        if (w < 0.3) w = 0.3;
        sum += rec.second * w;
    }
    return static_cast<int>(std::lround(sum));
}

void GameEngine::recordCardRelation(PlayerPtr source, CardPtr card, const std::vector<PlayerPtr>& targets) {
    if (!source || !card) return;
    int delta = 0;
    switch (card->getSubType()) {
        case CardSubType::SHA:
        case CardSubType::JUE_DOU:
        case CardSubType::NAN_MAN_RU_QIN:
        case CardSubType::WAN_JIAN_QI_FA:
        case CardSubType::JIE_DAO_SHA_REN:
        case CardSubType::GUO_HE_CHAI_QIAO:
        case CardSubType::SHUN_SHOU_QIAN_YANG:
        case CardSubType::LE_BU_SI_SHU:
        case CardSubType::BING_LIANG_CUN_DUAN:
        case CardSubType::SHAN_DIAN:
            delta = 1;   // 攻击/控制/夺牌＝敌对
            break;
        case CardSubType::TAO:
            delta = -3;  // 救人＝强友好
            break;
        case CardSubType::TAO_YUAN_JIE_YI:
            delta = -1;  // 群体回复＝友好
            break;
        default:
            delta = 0;   // 无中生有/五谷/装备等中性
            break;
    }
    if (delta == 0) return;
    // E2②：群体牌（南蛮/万箭/桃园）会打到所有人（含使用者队友），**不构成阵营证据**；单体牌才是可靠信号。
    const bool isAoe = (card->getSubType() == CardSubType::NAN_MAN_RU_QIN ||
                        card->getSubType() == CardSubType::WAN_JIAN_QI_FA ||
                        card->getSubType() == CardSubType::TAO_YUAN_JIE_YI);
    if (isAoe && targets.size() > 1) return;
    for (const auto& t : targets) {
        if (!t || t->getId() == source->getId()) continue;
        recordRelation(source->getId(), t->getId(), delta);
    }
}

int GameEngine::livingLordId() const {
    for (const auto& p : players)
        if (p && p->isAlive() && p->getIdentity() == Identity::ZHU_GONG) return p->getId();
    return -1;
}

// 身份是否为“全场公开可知”（与显示用的 isIdentityPublic 不同：不含“全 AI 观战”这种上帝视角）
static bool identityKnownToAll(const GameEngine& e, const Player& p) {
    if (p.getIdentity() == Identity::ZHU_GONG) return true;          // 主公选将后亮出（含继位者）
    if (p.getIdentity() == Identity::DI_ZHU || p.getIdentity() == Identity::NONG_MIN) return true; // 斗地主明牌
    if (p.getIdentity() == Identity::YE_XIN_JIA) return true;        // 野心家本身即公开
    if (p.getMark("身份已明置") > 0) return true;                    // 【择途】明置过身份牌
    if (!p.isAlive()) return true;                                   // 阵亡时揭示身份
    (void)e;
    return false;
}

int GameEngine::aiSideEstimate(const Player& other) const {
    int side = 0;
    // ① 公开身份
    if (identityKnownToAll(*this, other)) {
        switch (other.getIdentity()) {
            case Identity::ZHU_GONG:
            case Identity::ZHONG_CHEN: side += 4; break;
            case Identity::FAN_ZEI:    side -= 4; break;
            default: break;   // 内奸/野心家不属于任何一方
        }
    }
    if (other.getMark("储君") > 0) side += 2; // 主公明选的太子（多半是忠臣，也可能误立）
    // ② 行为证据：与“已知阵营锚点”的互动
    std::vector<int> lordSide, rebelSide;
    for (const auto& p : players) {
        if (!p || p->getId() == other.getId()) continue;
        if (!identityKnownToAll(*this, *p)) continue;
        if (p->getIdentity() == Identity::ZHU_GONG || p->getIdentity() == Identity::ZHONG_CHEN)
            lordSide.push_back(p->getId());
        else if (p->getIdentity() == Identity::FAN_ZEI)
            rebelSide.push_back(p->getId());
    }
    for (int id : lordSide) {
        side -= relationOf(other.getId(), id); // other 攻击主忠方 → 偏反贼
        side -= relationOf(id, other.getId()); // 主忠方攻击 other → other 偏反贼
    }
    for (int id : rebelSide) {
        side += relationOf(other.getId(), id); // other 攻击已揭示的反贼 → 偏主忠
        side += relationOf(id, other.getId());
    }
    return side;
}

void GameEngine::aiEstimateSides(const Player& self, int& loyal, int& rebels) const {
    loyal = rebels = 0;
    if (gameMode != GameMode::JUNZHENG) return; // 斗地主身份全是明牌，无需推断
    const Roles::RoleConfig cfg = Roles::configFor(static_cast<int>(players.size()));
    int knownLoyal = 0, knownRebel = 0, deadLoyal = 0, deadRebel = 0, deadTraitor = 0, unknownAlive = 0;
    for (const auto& p : players) {
        if (!p || p->getId() == self.getId()) continue;
        if (!p->isAlive()) {
            // 阵亡者的身份在死亡时揭示 → 公开信息
            if (p->getIdentity() == Identity::ZHU_GONG || p->getIdentity() == Identity::ZHONG_CHEN) ++deadLoyal;
            else if (p->getIdentity() == Identity::FAN_ZEI) ++deadRebel;
            else if (p->getIdentity() == Identity::NEI_JIAN) ++deadTraitor;
            continue;
        }
        if (identityKnownToAll(*this, *p)) {
            if (p->getIdentity() == Identity::ZHU_GONG || p->getIdentity() == Identity::ZHONG_CHEN) ++knownLoyal;
            else if (p->getIdentity() == Identity::FAN_ZEI) ++knownRebel;
            continue;
        }
        int side = aiSideEstimate(*p);
        if (side > 0) ++knownLoyal;        // 有证据偏向主忠方（替主公挡过牌、打过反贼…）
        else if (side < 0) ++knownRebel;   // 有证据偏向反贼方（打过主公、被主忠方集火…）
        else ++unknownAlive;               // 尚无证据
    }
    loyal = knownLoyal;
    rebels = knownRebel;
    // 未识别者按“公开构成的剩余名额”比例分摊（身份构成在开局就公示，属公开信息）
    // 观察者自己占掉一个名额：主忠方观察者扣主忠名额，反贼观察者扣反贼名额，内奸/野心家不占这两类。
    const Identity mine = self.getIdentity();
    const int selfLoyal = (mine == Identity::ZHU_GONG || mine == Identity::ZHONG_CHEN) ? 1 : 0;
    const int selfRebel = (mine == Identity::FAN_ZEI) ? 1 : 0;
    const int selfTraitor = (mine == Identity::NEI_JIAN) ? 1 : 0;
    const int remainLoyal = std::max(0, cfg.lords + cfg.loyalists - deadLoyal - knownLoyal - selfLoyal);
    const int remainRebel = std::max(0, cfg.rebels - deadRebel - knownRebel - selfRebel);
    const int remainTraitor = std::max(0, cfg.traitors - deadTraitor - selfTraitor);
    // 池子里也包含内奸名额：未识别者按三类剩余名额比例分摊，落在内奸份额上的不计入任何一方
    // （内奸既不是主忠方也不是反贼方，硬算进一边会让估计系统性偏大）。
    const int pool = remainLoyal + remainRebel + remainTraitor;
    if (unknownAlive > 0 && pool > 0) {
        loyal += (unknownAlive * remainLoyal) / pool;
        rebels += (unknownAlive * remainRebel) / pool;
    }
}

bool GameEngine::aiIsFriend(const Player& self, const Player& other) const {
    if (self.getId() == other.getId()) return true;
    if (!other.isAlive()) return false;
    if (gameMode == GameMode::DOUDIZHU) { // 斗地主身份是明牌
        if (self.getIdentity() == Identity::DI_ZHU) return other.getIdentity() == Identity::DI_ZHU;
        return other.getIdentity() == Identity::NONG_MIN;
    }
    const Identity mine = self.getIdentity();
    if (mine == Identity::NEI_JIAN || mine == Identity::YE_XIN_JIA) return false; // 没有队友
    const bool lordSide = (mine == Identity::ZHU_GONG || mine == Identity::ZHONG_CHEN);
    const int lordId = livingLordId();

    // 硬证据否决：曾经攻击过主公的人，不可能是主忠方的队友（AOE 误伤也会留下痕迹，
    // 但“打过主公”这一条足以否定队友关系，避免把反贼误认成忠臣而形成谁也打不了的僵局）。
    if (lordSide && lordId >= 0 && other.getId() != lordId) {
        if (relationOf(other.getId(), lordId) > 0 || relationOf(lordId, other.getId()) > 0) return false;
    }

    bool friendByEvidence = lordSide ? (aiSideEstimate(other) >= kAiSideConfidence || other.getId() == lordId)
                                     : (aiSideEstimate(other) <= -kAiSideConfidence);
    if (!friendByEvidence) return false;

    // 一致性兜底：对局仍在继续就说明胜负未分，我方不可能“人人都是队友”。
    // 若按推断所有存活者都是队友，则把证据最弱的一名视为非队友——否则谁都不出手，
    // 对局会一直打到回合上限（实测 seed=934 的 7 人至尊场曾因此僵死）。
    if (gameOver) return friendByEvidence;
    int weakestId = -1, weakestScore = 0;
    bool allFriends = true;
    for (const auto& p : players) {
        if (!p || !p->isAlive() || p->getId() == self.getId()) continue;
        bool f = lordSide ? (aiSideEstimate(*p) >= kAiSideConfidence || p->getId() == lordId)
                          : (aiSideEstimate(*p) <= -kAiSideConfidence);
        if (lordSide && lordId >= 0 && p->getId() != lordId &&
            (relationOf(p->getId(), lordId) > 0 || relationOf(lordId, p->getId()) > 0)) f = false;
        if (!f) { allFriends = false; break; }
        int score = std::abs(aiSideEstimate(*p));
        if (weakestId < 0 || score < weakestScore) { weakestId = p->getId(); weakestScore = score; }
    }
    if (allFriends && weakestId >= 0 && weakestId == other.getId()) return false;
    return friendByEvidence;
}

// 身份是否公开（用户 2026-10-05：【择途】发动时要“明置身份牌”；野心家本身即公开身份）
bool GameEngine::isIdentityPublic(const Player& p) const {
    if (!p.isAI()) return true;              // 真人玩家自己
    if (identityKnownToAll(*this, p)) return true;
    return !interactiveMode;                 // 全 AI 观战时全部公开（仅显示用，AI 推断不使用此上帝视角）
}

// 当前模式的禁止池（用户 2026-10-06：身份场与斗地主禁止池不同；普通场为空）
HeroTier::Mode GameEngine::banMode() const {
    return gameMode == GameMode::DOUDIZHU ? HeroTier::Mode::DOUDIZHU : HeroTier::Mode::IDENTITY;
}

bool GameEngine::isHeroBannedInCurrentMode(const std::string& id) const {
    return zhizunField && HeroTier::isBannedInZhizun(id, banMode());
}

std::vector<std::string> GameEngine::currentBanList() const {
    if (!zhizunField) return {};
    return HeroTier::bannedInZhizun(banMode());
}

// 至尊场说明（开局即打印，便于核对被移除的禁将；两个模式的池不同）
void GameEngine::logZhizunBanner() const {
    if (!zhizunField) return;
    const auto& pool = HeroTier::bannedInZhizun(HeroTier::Mode::IDENTITY);
    const auto& ddz = HeroTier::bannedInZhizun(HeroTier::Mode::DOUDIZHU);
    logMessage("场次: 至尊场（身份场禁止池 " + std::to_string(pool.size()) +
               " 名 / 斗地主禁止池 " + std::to_string(ddz.size()) +
               " 名；本局为" + HeroTier::modeName(banMode()) +
               "，随机池已剔除该池；点将可用全将。名单与依据见 docs/zhizun_field_rules.md）");
}

// 可用武将池：全部登记武将；至尊场则按**当前模式**剔除该模式禁止池里的武将。
std::vector<std::string> GameEngine::availableHeroPool() const {
    std::vector<std::string> ids = HeroRegistry::allIds();
    if (!zhizunField) return ids;
    ids.erase(std::remove_if(ids.begin(), ids.end(),
                             [this](const std::string& id) { return isHeroBannedInCurrentMode(id); }),
              ids.end());
    return ids;
}

// 主公座位（身份场）：identities/players 中唯一的 ZHU_GONG；找不到则 0。
int GameEngine::lordSeat() const {
    for (size_t i = 0; i < players.size(); ++i)
        if (players[i] && players[i]->getIdentity() == Identity::ZHU_GONG) return static_cast<int>(i);
    return 0;
}

// 玩家指定身份（用户 2026-10-05 裁定：玩家可以指定或随机身份）。
// 做法：在已洗牌的身份序列里找到目标身份的座位，与人类座位交换；主公不必固定在座位 0，
// 但“主公先动/先选将”由 lordSeat() 保证。该人数下没有目标身份时保留随机并说明。
void GameEngine::applyPreferredIdentity(std::vector<Identity>& identities, int humanIndex) {
    if (preferredIdentity < 0 || humanIndex < 0 || humanIndex >= static_cast<int>(identities.size())) return;
    static const Identity kMap[4] = {Identity::ZHU_GONG, Identity::ZHONG_CHEN, Identity::FAN_ZEI, Identity::NEI_JIAN};
    static const char* kName[4] = {"主公", "忠臣", "反贼", "内奸"};
    Identity want = kMap[preferredIdentity % 4];
    int target = -1;
    for (size_t i = 0; i < identities.size(); ++i) {
        if (identities[i] != want) continue;
        target = static_cast<int>(i);
        break;
    }
    if (target < 0) {
        logMessage("【身份】" + std::to_string(identities.size()) + " 人局的身份构成里没有“" +
                   kName[preferredIdentity % 4] + "”，改为随机分配（构成：" +
                   Roles::describeComposition(static_cast<int>(identities.size())) + "）。");
        return;
    }
    if (target == humanIndex) return;
    std::swap(identities[static_cast<size_t>(target)], identities[static_cast<size_t>(humanIndex)]);
    logMessage("【身份】" + (humanIndex >= 0 ? "玩家(你)" : "座位 " + std::to_string(humanIndex)) +
               " 指定身份：" + kName[preferredIdentity % 4] + "（主公在座位 " +
               std::to_string(target == humanIndex ? humanIndex : target) + "，主公先动）。");
}

// 选将框里最终取哪一个（用户 2026-10-05 裁定）：
//   普通场——不论身份场还是斗地主，都是**随机**从选将框中选；
//   至尊场——排除弱将后由**选将 AI** 排名确定预选，并有概率（默认 30%）选排第二的。
int GameEngine::draftPickIndex(const std::vector<std::string>& cands, HeroTier::Field field,
                              Identity role, const HeroPtr& lordHero) {
    if (cands.empty()) return 0;
    if (cands.size() == 1) return 0;
    if (!zhizunField) {
        return std::uniform_int_distribution<int>(0, static_cast<int>(cands.size()) - 1)(rng);
    }
    std::vector<HeroPtr> heroes;
    heroes.reserve(cands.size());
    for (const auto& id : cands) heroes.push_back(HeroRegistry::create(id));
    AIController::DraftContext ctx;
    ctx.field = field;
    ctx.role = role;
    ctx.lordHero = lordHero;
    ctx.seatPosition = idDraftPosition;   // D4：座次因素（先手位/末位）
    return static_cast<int>(AIController::chooseDraftHeroWithVariance(heroes, ctx, rng, kSecondPickPercent));
}

void GameEngine::initGame(int totalPlayers, int humanIndex, const std::vector<std::string>& heroIds) {
    gameMode = GameMode::JUNZHENG; // 身份模式：默认军争
    idCandidates.clear();
    logZhizunBanner();
    std::vector<Identity> identities = Roles::buildIdentities(totalPlayers);
    Roles::shuffleIdentities(identities, rng);
    applyPreferredIdentity(identities, humanIndex);

    std::vector<std::string> chosen = heroIds;
    if (identityDraft) {
        // 阶段 1：临时建局（随机武将 + 4 张手牌），只为拿到座位/身份用于选将；日志抑制，随后整局重建。
        quietSetup = true;
        setupGame(totalPlayers, humanIndex, {}, identities);
        quietSetup = false;
        // 阶段 2：发候选（主公 5 选 1，其余 3 选 1；同一人物不重复）
        initIdentityHeroPool(humanIndex, heroIds.empty() ? std::string() : heroIds[0]);
        int lord = lordSeat();
        // 选将框数量（用户 2026-10-05）：主公＝4 个主公武将 + 6 个常规武将；
        // 忠臣与内奸各比反贼多 1 个（4+1＝5）；反贼 4 个。
        for (int seat = 0; seat < totalPlayers; ++seat) {
            if (seat == lord) {
                int lordSlots = 0;
                while (lordSlots < 4 && drawIdentityCandidate(seat, true)) ++lordSlots;
                int want = 6 + lordSlots; // 主公武将不足 4 名时用常规武将补齐到 10 个
                while (static_cast<int>(idCandidates[seat].size()) < want)
                    if (!drawIdentityCandidate(seat, false)) break;
                continue;
            }
            Identity role = identities[seat];
            int want = (role == Identity::ZHONG_CHEN || role == Identity::NEI_JIAN) ? 5 : 4;
            while (static_cast<int>(idCandidates[seat].size()) < want)
                if (!drawIdentityCandidate(seat, false)) break;
        }
        logIdentityCandidates("【亮将】");
        // 阶段 3：主公先选并亮出，其余按座次选将（AI 依据亮出的主公武将“针对或辅助”）
        chosen = chooseIdentityHeroes();
    }

    // 用户 2026-10-06：“每个人可以换七次牌……所有座位都能换（AI 也换）”
    // → 默认对所有对局开启（含全 AI 观战；AI 的换牌策略是确定性的）。
    // 只有显式 setMulliganEnabled(false) 才关闭。
    if (!mulliganExplicit) mulliganEnabled = true;
    (void)totalPlayers;
    (void)humanIndex;
    setupGame(totalPlayers, humanIndex, chosen, identities, true);
    // 身份场模式技能（用户 2026-10-05）：内奸【择途】、主公【立储】
    for (auto& p : players) applyIdentityModeSkills(*this, *p);
    currentTurnIndex = lordSeat(); // 主公先动（默认主公在座位 0；指定身份后可能换位）
    if (identityDraft) logMessage("模式: 身份场·选将（主公先选完并亮出，别人再选针对或辅助）");
    {
        bool hasTraitor = Roles::aliveCount(players, Identity::NEI_JIAN) > 0;
        std::string rules = "规则: ";
        if (hasTraitor)
            rules += "内奸【择途】（存活>4 人时可发动一次：侍奉明君→忠臣 / 自立为主→野心家并获得【飞扬】【跋扈】）；";
        rules += "主公【立储】（第一轮任意角色结束阶段明选储君，主公阵亡时忠臣储君继位）";
        logMessage(rules);
    }
}

// ============ 身份场选将（主公先选并亮出；其余针对/辅助） ============

// 初始化候选池：人类预选武将优先进入其候选（保留“自选武将”入口）；至尊场下弱将不入池。
void GameEngine::initIdentityHeroPool(int humanIndex, const std::string& preferred) {
    idPool = availableHeroPool();
    std::shuffle(idPool.begin(), idPool.end(), rng);
    idUsedPersons.clear();
    idCandidates.assign(players.size(), {});
    idPreselect.assign(players.size(), "");
    idDraftPositionUsed.assign(players.size(), -1);
    if (humanIndex < 0 || humanIndex >= static_cast<int>(idCandidates.size())) return;
    if (preferred.empty() || !HeroRegistry::find(preferred)) return;
    if (zhizunField && isHeroBannedInCurrentMode(preferred)) {
        // 用户 2026-10-06：“随机池剔除禁将，但**点将可以用全将**”→ 显式指定照常入框并披露。
        logMessage("  【至尊场·点将】" + heroDraftShort(preferred) +
                   " 在本模式禁止池内，因是点将（显式指定）仍可用；随机候选不含禁止池武将。");
    }
    idCandidates[humanIndex].push_back(preferred);
    idUsedPersons.push_back(HeroRegistry::personKey(preferred));
    idPool.erase(std::remove(idPool.begin(), idPool.end(), preferred), idPool.end());
}

// 武将是否拥有主公技（选将框里主公的 4 个“主公武将”槽位用）
bool GameEngine::isLordHero(const std::string& id) {
    HeroPtr h = HeroRegistry::create(id);
    if (!h) return false;
    for (const auto& sk : h->getSkills()) if (sk && sk->hasTag(SkillTag::LORD)) return true;
    return false;
}

// 为某个座位抽 1 个候选（同一人物在全部候选间不重复）；lordOnly=true 时只抽拥有主公技的武将
bool GameEngine::drawIdentityCandidate(int seat, bool lordOnly) {
    if (seat < 0 || seat >= static_cast<int>(idCandidates.size())) return false;
    for (size_t i = 0; i < idPool.size(); ++i) {
        const std::string cand = idPool[i];
        std::string person = HeroRegistry::personKey(cand);
        if (std::find(idUsedPersons.begin(), idUsedPersons.end(), person) != idUsedPersons.end()) continue;
        if (std::find(idCandidates[seat].begin(), idCandidates[seat].end(), cand) != idCandidates[seat].end()) continue;
        if (lordOnly && !isLordHero(cand)) continue;
        idCandidates[seat].push_back(cand);
        idUsedPersons.push_back(person);
        idPool.erase(idPool.begin() + static_cast<long>(i));
        return true;
    }
    return false;
}

void GameEngine::logIdentityCandidates(const std::string& tag) const {
    // 与斗地主一致：有真人时只公开真人自己的候选；全 AI 观战则全部展示。
    for (size_t seat = 0; seat < idCandidates.size(); ++seat) {
        std::string line = tag + players[seat]->getName() + " 的候选武将: ";
        if (humanSeat >= 0 && static_cast<int>(seat) != humanSeat) {
            logMessage(line + "（对方候选不公开）");
            continue;
        }
        for (const auto& id : idCandidates[seat]) line += heroDraftLabel(id) + " | ";
        logMessage(line);
    }
}

// D1：AI 也会用换将次数——整框候选都偏弱时（最强者的评分低于门槛），换掉**最弱**的一个再评一次。
// 门槛 250 的依据：弱将档＝100 + 体力×5（约 115~125），中档＝200 + 体力×5（约 215~235），
// 强档＝300 + 体力×5；所以“最强者 < 250”等价于“整框没有中档以上的将”。
void GameEngine::aiImproveDraftBox(int seat, HeroTier::Field field, Identity role, const HeroPtr& lordHero,
                                  int swapsLeft, bool identityField) {
    auto& cands = identityField ? idCandidates[seat] : ddzCandidates[seat];
    if (cands.empty()) return;
    AIController::DraftContext ctx;
    ctx.field = field;
    ctx.role = role;
    ctx.lordHero = lordHero;
    ctx.seatPosition = -1;
    auto evaluate = [&]() {
        int best = -(1 << 30), worst = (1 << 30);
        size_t worstIdx = 0;
        for (size_t i = 0; i < cands.size(); ++i) {
            int sc = AIController::draftScore(HeroRegistry::create(cands[i]), ctx);
            if (sc > best) best = sc;
            if (sc < worst) { worst = sc; worstIdx = i; }
        }
        return std::make_pair(best, worstIdx);
    };
    for (int used = 0; used < swapsLeft; ++used) {
        auto [best, worstIdx] = evaluate();
        if (best >= kAiSwapScoreThreshold) break;                 // 框里已有能用的将，不浪费换将
        std::string before = heroDraftShort(cands[worstIdx]);
        bool ok = identityField ? drawIdentityCandidate(seat, false) : drawDoudizhuCandidate(seat);
        if (!ok) break;                                            // 池子空了
        std::string fresh = cands.back();
        cands.pop_back();
        cands[worstIdx] = fresh;
        logMessage("  【换将·AI】" + players[seat]->getName() + " 嫌 " + before + " 太弱，换成了 " +
                   heroDraftShort(fresh) + "（评分 " + std::to_string(best) + " < " +
                   std::to_string(kAiSwapScoreThreshold) + "）");
    }
}

// 选将：普通场随机取框内一名；至尊场由选将 AI 排名预选（有机会取第二名）。
// 人类可自由改选，并可用 r编号 换将（身份场每个框只允许换 1 次）。
int GameEngine::askIdentityDraftChoice(int seat, const HeroPtr& lordHero, int positionHint) {
    PlayerPtr p = players[seat];
    if (idCandidates[seat].empty()) return 0;
    if (p->isAI()) {
        // D1：身份场每框只有 1 次换将，AI 在整框偏弱时会用掉它
        aiImproveDraftBox(seat, HeroTier::Field::IDENTITY, p->getIdentity(), lordHero, 1, true);
    }
    const auto& cands = idCandidates[seat];
    idDraftPosition = positionHint;
    if (seat >= 0 && seat < static_cast<int>(idDraftPositionUsed.size())) idDraftPositionUsed[seat] = positionHint;
    int preselect = draftPickIndex(cands, HeroTier::Field::IDENTITY, p->getIdentity(), lordHero);
    if (seat >= 0 && seat < static_cast<int>(idPreselect.size())) idPreselect[seat] = cands[preselect];
    if (p->isAI()) return preselect;
    std::vector<int> swaps(cands.size(), 1); // 身份场：每个框只能换将 1 次（用户 2026-10-05）
    std::string heroId = p->getHero() ? p->getHero()->getId() : "";
    // 主公框分页（用户 2026-10-05 选择“按主公武将/常规武将分两页”）：
    // 第 1 页只看拥有主公技的“主公武将”，第 2 页看常规武将；n/p 翻页，编号仍是全局编号。
    const bool paged = (seat == lordSeat()) && cands.size() > 6;
    int page = 0;
    auto pageIndices = [&](int which) {
        std::vector<int> idx;
        for (int i = 0; i < static_cast<int>(cands.size()); ++i)
            if ((isLordHero(cands[i]) ? 0 : 1) == which) idx.push_back(i);
        return idx;
    };
    auto reprompt = [&]() {
        log() << "\n>>> 【选将】" << (seat == lordSeat() ? "你是主公，请先选将（选完即亮出，其他人再针对/辅助）"
                                                        : "请选择你的武将")
              << "（输入编号；y=接受预选；r编号=换将该位置，身份场每个框限 1 次；d1,3=对比候选"
              << (paged ? "；n/p 翻页" : "") << "）\n";
        if (lordHero) log() << "    已亮出的主公武将: " << lordHero->getName() << "（" << lordHero->getCountryString() << "）\n";
        log() << "    场次: " << (zhizunField ? "至尊场（已排除弱将，预选由选将 AI 排名）" : "普通场（预选为随机）") << "\n";
        std::vector<int> show;
        if (paged) {
            show = pageIndices(page);
            log() << "    第 " << (page + 1) << "/2 页【" << (page == 0 ? "主公武将（拥有主公技）" : "常规武将")
                  << "】共 " << show.size() << " 名（n 下一页 / p 上一页）\n";
        } else {
            for (int i = 0; i < static_cast<int>(cands.size()); ++i) show.push_back(i);
        }
        for (int i : show) {
            log() << "  (" << (i + 1) << ") " << heroDraftLabel(cands[i])
                  << (i == preselect ? " ★预选" : "")
                  << "（还可换将 " << swaps[i] << " 次）\n";
        }
        if (paged) {
            int prePage = isLordHero(cands[preselect]) ? 0 : 1;
            if (prePage != page)
                log() << "    （★预选在第 " << (prePage + 1) << " 页：编号 " << (preselect + 1) << " "
                      << heroDraftShort(cands[preselect]) << "，输入 " << (prePage == 0 ? "p" : "n") << " 翻页）\n";
        }
        log() << "输入编号（t 图鉴, m 记牌器, c 局势, d1,3 对比" << (paged ? ", n/p 翻页" : "") << "）: " << std::flush;
    };
    while (true) {
        reprompt();
        std::string token = ui->readTokenWithCodex(heroId, reprompt);
        if (Interaction::inputClosed()) break;
        // 真人选将**不设超时、不倒计时**（用户 2026-10-05 选择“一直等”）：只有输入源结束（EOF，
        // 例如管道演示/脚本对局）才兜底采用 ★预选，并明确记录，绝不静默。
        if (paged && (token == "n" || token == "N")) { page = 1; continue; }
        if (paged && (token == "p" || token == "P")) { page = 0; continue; }
        if (!token.empty() && (token[0] == 'd' || token[0] == 'D')) {
            std::vector<int> idxs = parseCompareIndices(token.substr(1));
            if (idxs.empty()) { logMessage("对比指令格式：d编号[,编号]，例如 d1,3"); continue; }
            log() << draftCompareText(cands, idxs, HeroTier::Field::IDENTITY, p->getIdentity(), lordHero);
            continue; // 插入式面板：返回后重印选将问题，不改游戏状态
        }
        if (!token.empty() && (token[0] == 'r' || token[0] == 'R')) {
            int idx = Interaction::parseInt(token.substr(1));
            if (idx < 1 || idx > static_cast<int>(cands.size())) { logMessage("无效的换将指令！"); continue; }
            if (swaps[idx - 1] <= 0) { logMessage("该位置换将次数已用完（身份场每个框限 1 次）。"); continue; }
            std::string before = heroDraftShort(idCandidates[seat][idx - 1]);
            if (!drawIdentityCandidate(seat, false)) { logMessage("没有更多可换的武将了。"); continue; }
            std::string next = idCandidates[seat].back();
            idCandidates[seat].pop_back();
            idCandidates[seat][idx - 1] = next;
            swaps[idx - 1]--;
            logMessage("  【换将】第 " + std::to_string(idx) + " 个位置：" + before + " → " +
                       heroDraftShort(next) + "（剩余 " + std::to_string(swaps[idx - 1]) + " 次）");
            continue;
        }
        if (token == "y" || token == "Y") return preselect;
        int idx = Interaction::parseInt(token);
        if (idx >= 1 && idx <= static_cast<int>(cands.size())) return idx - 1;
        logMessage("无效的选项，请重新输入！");
    }
    logMessage("  【选将】输入源已结束（未选择）：按 ★预选 采用 " + heroDraftShort(cands[preselect]) + "。");
    return preselect;
}

// 逐个座位确定武将：主公先选（选完亮出），其余按座次选（AI 据此针对或辅助）。
std::vector<std::string> GameEngine::chooseIdentityHeroes() {
    std::vector<std::string> chosen(players.size());
    int lord = lordSeat();
    std::vector<int> order;
    order.push_back(lord);
    for (int i = 0; i < static_cast<int>(players.size()); ++i) if (i != lord) order.push_back(i);
    HeroPtr lordHero;
    int pickerIndex = 0;                       // 主公之后的第几个选将者（D4 座次因素）
    const int nonLordCount = std::max(0, static_cast<int>(order.size()) - 1);
    for (int seat : order) {
        if (seat < 0 || seat >= static_cast<int>(idCandidates.size()) || idCandidates[seat].empty()) continue;
        int positionHint = -1;
        if (seat != lord && nonLordCount > 0) {
            int k = pickerIndex - 1;           // 0＝紧跟主公的先手位
            positionHint = (k == 0) ? 0 : (k == nonLordCount - 1 ? 2 : 1);
        }
        int pick = askIdentityDraftChoice(seat, lordHero, positionHint);
        ++pickerIndex;
        pick = std::max(0, std::min(pick, static_cast<int>(idCandidates[seat].size()) - 1));
        chosen[seat] = idCandidates[seat][pick];
        if (seat == lord) {
            lordHero = HeroRegistry::create(chosen[seat]);
            logMessage("  【选将·主公亮出】" + players[seat]->getName() + " 选择了 " + heroDraftShort(chosen[seat]) +
                       (lordHero ? "，势力 " + lordHero->getCountryString() : "") +
                       "（预选: " + heroDraftShort(preselectOf(seat, false)) + "）");
        } else {
            logMessage("  【选将】" + players[seat]->getName() + " 选择了 " + heroDraftShort(chosen[seat]) +
                       "（预选: " + heroDraftShort(preselectOf(seat, false)) + "）");
        }
    }
    return chosen;
}

// 斗地主：3 人，1 地主 + 2 农民；所有人初始 4 张手牌；地主 +1 体力上限并先手。
// landlordIndex >= 0：直接指定地主（测试/兼容路径，不做叫分/选将）。
// landlordIndex < 0 且 fullFlow：完整流程——亮 3 将 → 抢地主（叫分）→ 地主追加 2 将 → 选将。
// 地主/农民的专属技能（飞扬/跋扈/共苦）暂未实现：等移动版官网原文（用户 2026-10-04 裁定）。
void GameEngine::initDoudizhuGame(int humanIndex, const std::vector<std::string>& heroIds,
                                  int landlordIndex, bool fullFlow) {
    gameMode = GameMode::DOUDIZHU;
    logZhizunBanner();
    ddzBaseScore = 1;
    ddzCandidates.clear();
    ddzLandlordSeat = (landlordIndex >= 0 && landlordIndex < 3) ? landlordIndex : 0;

    std::vector<std::string> chosen = heroIds;
    if (fullFlow) {
        // 阶段 1：临时建局（随机武将 + 4 张手牌）用于叫分预览；日志抑制，随后整局重建。
        quietSetup = true;
        setupGame(3, humanIndex, {}, Roles::doudizhuIdentities(0));
        quietSetup = false;
        // 阶段 2：所有人先亮 3 个候选武将（各自互相看不到，只有本人能看到自己的）
        initDoudizhuHeroPool(humanIndex, heroIds.empty() ? "" : heroIds[0]);
        for (int seat = 0; seat < 3; ++seat) {
            while ((int)ddzCandidates[seat].size() < 3) if (!drawDoudizhuCandidate(seat)) break;
        }
        logDoudizhuCandidates("【亮将】");
        // 阶段 3：抢地主（叫分）
        ddzLandlordSeat = runDoudizhuBidding(humanIndex);
        // 阶段 4：地主再亮 2 个候选（共 5 选 1）
        while ((int)ddzCandidates[ddzLandlordSeat].size() < 5) {
            if (!drawDoudizhuCandidate(ddzLandlordSeat)) break;
        }
        logDoudizhuCandidates("【亮将·地主追加】");
        // 阶段 5：各自选将（人类可在选择前换将，每个位置最多 2 次）
        chosen = chooseDoudizhuHeroes(humanIndex);
    }

    // 阶段 6：按确定结果正式建局（武将/身份/地主先手）
    quietSetup = true;
    if (!mulliganExplicit) mulliganEnabled = true; // 同上：斗地主同样默认开启（全 AI 观战也换）
    (void)humanIndex;
    setupGame(3, humanIndex, chosen, Roles::doudizhuIdentities(ddzLandlordSeat), true);
    quietSetup = false;
    // 阶段 7：分发斗地主专属技能（地主：飞扬/跋扈；农民：共苦）
    for (auto& p : players) applyDoudizhuModeSkills(*this, *p);
    // 阶段 7b：游戏开始时点的技能必须发动（用户 2026-10-05）。此前正式建局也走 quietSetup，
    // 导致斗地主下 onGameStart 全部漏发：七星/狂暴/化身/明任/妙略/结姻不生效、蓄力技初始全为 0。
    runGameStartHooks();
    currentTurnIndex = ddzLandlordSeat;
    logMessage("模式: 斗地主（1 地主 vs 2 农民，所有人初始 4 张手牌，地主先手；叫分底分 " +
               std::to_string(ddzBaseScore) +
               (fullFlow ? "）" : "；直接指定地主（未走叫分流程））"));
    if (fullFlow) {
        for (auto& p : players) {
            std::string names;
            for (const auto& sk : p->getHero()->getSkills()) {
                if (sk->getName() == "飞扬" || sk->getName() == "跋扈" || sk->getName() == "共苦")
                    names += (names.empty() ? "" : "、") + sk->getName();
            }
            if (!names.empty()) logMessage("  【模式技能】" + p->getName() + " 获得 " + names + "。");
        }
    }
}

// 斗地主选将：初始化候选池（人类预选武将优先进入其候选，保留“自选武将”入口）。
void GameEngine::initDoudizhuHeroPool(int humanIndex, const std::string& preferred) {
    ddzPool = availableHeroPool(); // 至尊场：弱将不入斗地主候选池
    std::shuffle(ddzPool.begin(), ddzPool.end(), rng);
    ddzUsedPersons.clear();
    ddzCandidates.assign(3, {});
    ddzPreselect.assign(3, "");
    ddzDraftPositionUsed.assign(3, -1);
    if (zhizunField && !preferred.empty() && isHeroBannedInCurrentMode(preferred) &&
        humanIndex >= 0 && humanIndex < 3 && HeroRegistry::find(preferred)) {
        logMessage("  【至尊场·点将】" + heroDraftLabel(preferred) +
                   " 在本模式禁止池内，因是点将（显式指定）仍可用；随机候选不含禁止池武将。");
        ddzCandidates[humanIndex].push_back(preferred);
        ddzUsedPersons.push_back(HeroRegistry::personKey(preferred));
        ddzPool.erase(std::remove(ddzPool.begin(), ddzPool.end(), preferred), ddzPool.end());
    } else if (humanIndex >= 0 && humanIndex < 3 && !preferred.empty() && HeroRegistry::find(preferred)) {
        ddzCandidates[humanIndex].push_back(preferred);
        ddzUsedPersons.push_back(HeroRegistry::personKey(preferred));
        ddzPool.erase(std::remove(ddzPool.begin(), ddzPool.end(), preferred), ddzPool.end());
    }
}

// 斗地主选将：为某个座位抽 1 个候选（同一人物在全部候选间不重复）。
bool GameEngine::drawDoudizhuCandidate(int seat) {
    if (seat < 0 || seat >= 3) return false;
    for (size_t i = 0; i < ddzPool.size(); ++i) {
        const std::string& cand = ddzPool[i];
        std::string person = HeroRegistry::personKey(cand);
        if (std::find(ddzUsedPersons.begin(), ddzUsedPersons.end(), person) != ddzUsedPersons.end()) continue;
        if (std::find(ddzCandidates[seat].begin(), ddzCandidates[seat].end(), cand) != ddzCandidates[seat].end()) continue;
        ddzCandidates[seat].push_back(cand);
        ddzUsedPersons.push_back(person);
        ddzPool.erase(ddzPool.begin() + static_cast<long>(i));
        return true;
    }
    return false;
}

void GameEngine::logDoudizhuCandidates(const std::string& tag) const {
    // “各自互相看不到”：有真人时只公开真人自己的候选，其余座位不展示（全 AI 观战则全部展示）。
    for (int seat = 0; seat < 3; ++seat) {
        std::string line = tag + players[seat]->getName() + " 的候选武将: ";
        if (humanSeat >= 0 && seat != humanSeat) {
            logMessage(line + "（对方候选不公开）");
            continue;
        }
        for (const auto& id : ddzCandidates[seat]) line += heroDraftLabel(id) + " | ";
        logMessage(line);
    }
}

// 斗地主选将：人类交互选择（支持换将：r编号，每个位置最多 2 次）；AI 从 3/5 候选中随机选一。
int GameEngine::askDoudizhuDraftChoice(int seat) {
    PlayerPtr p = players[seat];
    const auto& cands = ddzCandidates[seat];
    if (cands.empty()) return 0;
    Identity role = (seat == ddzLandlordSeat) ? Identity::DI_ZHU : Identity::NONG_MIN;
    HeroTier::Field field = (seat == ddzLandlordSeat) ? HeroTier::Field::DDZ_LANDLORD
                                                      : HeroTier::Field::DDZ_FARMER;
    if (p->isAI()) aiImproveDraftBox(seat, field, role, HeroPtr(), 2, false); // D1：斗地主每位置可换 2 次
    if (seat >= 0 && seat < static_cast<int>(ddzDraftPositionUsed.size())) ddzDraftPositionUsed[seat] = idDraftPosition;
    int preselect = draftPickIndex(cands, field, role, HeroPtr());
    if (seat >= 0 && seat < static_cast<int>(ddzPreselect.size())) ddzPreselect[seat] = cands[preselect];
    if (p->isAI()) return preselect; // 普通场随机；至尊场由选将 AI 排名（有机会取第二名）
    std::vector<int> swaps(cands.size(), 2); // 斗地主：每个亮出的位置都可换将 2 次
    std::string heroId = p->getHero() ? p->getHero()->getId() : "";
    auto reprompt = [&]() {
        log() << "\n>>> 【选将】请选择你的武将（输入编号；y=接受预选；r编号=换将该位置，每个位置最多 2 次；d1,3=对比候选）\n";
        log() << "    场次: " << (zhizunField ? "至尊场（已排除双状态弱将，预选由选将 AI 排名）" : "普通场（预选为随机）") << "\n";
        for (size_t i = 0; i < ddzCandidates[seat].size(); ++i) {
            log() << "  (" << (i + 1) << ") " << heroDraftLabel(ddzCandidates[seat][i])
                  << (static_cast<int>(i) == preselect ? " ★预选" : "")
                  << "（还可换将 " << swaps[i] << " 次）\n";
        }
        log() << "输入编号（t 图鉴, m 记牌器, c 局势, d1,3 对比）: " << std::flush;
    };
    while (true) {
        reprompt();
        std::string token = ui->readTokenWithCodex(heroId, reprompt);
        if (Interaction::inputClosed()) break;
        if (!token.empty() && (token[0] == 'd' || token[0] == 'D')) {
            std::vector<int> idxs = parseCompareIndices(token.substr(1));
            if (idxs.empty()) {
                logMessage("对比指令格式：d编号[,编号]，例如 d1,3");
                continue;
            }
            log() << draftCompareText(cands, idxs, field, role, HeroPtr());
            continue; // 插入式面板：返回后重印选将问题，不改游戏状态
        }
        if (!token.empty() && (token[0] == 'r' || token[0] == 'R')) {
            int idx = Interaction::parseInt(token.substr(1));
            if (idx < 1 || idx > static_cast<int>(cands.size())) {
                logMessage("无效的换将指令！");
                continue;
            }
            if (swaps[idx - 1] <= 0) {
                logMessage("该位置换将次数已用完（每个位置最多 2 次）。");
                continue;
            }
            std::string before = heroDraftShort(ddzCandidates[seat][idx - 1]);
            if (!drawDoudizhuCandidate(seat)) {
                logMessage("没有更多可换的武将了。");
                continue;
            }
            std::string next = ddzCandidates[seat].back();
            ddzCandidates[seat].pop_back();
            ddzCandidates[seat][idx - 1] = next;
            swaps[idx - 1]--;
            logMessage("  【换将】第 " + std::to_string(idx) + " 个位置：" + before + " → " +
                       heroDraftShort(next) + "（剩余 " + std::to_string(swaps[idx - 1]) + " 次）");
            continue;
        }
        if (token == "y" || token == "Y") return preselect;
        int idx = Interaction::parseInt(token);
        if (idx >= 1 && idx <= static_cast<int>(cands.size())) return idx - 1;
        logMessage("无效的选项，请重新输入！");
    }
    return preselect;
}

// 斗地主选将：逐个座位确定武将（普通场随机；至尊场由选将 AI 排名预选）。
std::vector<std::string> GameEngine::chooseDoudizhuHeroes(int humanIndex) {
    std::vector<std::string> chosen(3);
    // 行动顺序：地主先动，其余按座次 → 座次提示（D4）：0＝紧跟地主的先手农民，1＝末位农民
    std::vector<int> order;
    order.push_back(ddzLandlordSeat);
    for (int i = 0; i < 3; ++i) if (i != ddzLandlordSeat) order.push_back(i);
    int farmerIdx = 0;
    for (int seat : order) {
        if (ddzCandidates[seat].empty()) continue;
        idDraftPosition = (seat == ddzLandlordSeat) ? -1 : (farmerIdx == 0 ? 0 : 2);
        if (seat != ddzLandlordSeat) ++farmerIdx;
        int pick = askDoudizhuDraftChoice(seat);
        pick = std::max(0, std::min(pick, static_cast<int>(ddzCandidates[seat].size()) - 1));
        chosen[seat] = ddzCandidates[seat][pick];
        logMessage("  【选将】" + players[seat]->getName() + " 选择了 " + heroDraftShort(chosen[seat]) +
                   "（预选: " + heroDraftShort(preselectOf(seat, true)) + "）");
        (void)humanIndex;
    }
    idDraftPosition = -1;
    return chosen;
}

// 叫分：按座位顺序（0→1→2）依次选择，叫分须高于当前最高分；有人叫 3 分立即成为地主；
// 全部不叫则由首位（座位 0）默认成为地主。返回地主座位。
int GameEngine::runDoudizhuBidding(int /*humanIndex*/) {
    int bestSeat = -1;
    int bestBid = 0;
    for (int seat = 0; seat < 3; ++seat) {
        PlayerPtr p = players[seat];
        std::vector<std::string> opts;
        opts.push_back("不叫");
        for (int bid = bestBid + 1; bid <= 3; ++bid) opts.push_back("叫" + std::to_string(bid) + "分");
        // D2：叫分与**候选武将的斗地主强度档（地主档）**联动，再叠加手牌质量——强将才叫 3 分。
        int aiChoice = 0;
        int bestTier = 1;
        for (const auto& id : ddzCandidates[seat])
            bestTier = std::max(bestTier, HeroTier::powerScore(id, HeroTier::Field::DDZ_LANDLORD));
        int shaCount = 0, shanCount = 0, taoCount = 0;
        for (const auto& c : p->getHandCards()) {
            if (!c) continue;
            if (c->getSubType() == CardSubType::SHA) ++shaCount;
            else if (c->getSubType() == CardSubType::SHAN) ++shanCount;
            else if (c->getSubType() == CardSubType::TAO) ++taoCount;
        }
        const int handScore = shaCount + shanCount + taoCount * 2;   // 桃最值钱（1 打 2 要靠它续命）
        int aiBid = 0;
        if (bestTier >= 3 && handScore >= 3) aiBid = 3;              // 强将 + 好牌 → 抢地主
        else if (bestTier >= 3 || (bestTier == 2 && handScore >= 4)) aiBid = 2;
        else if (bestTier >= 2 || handScore >= 3) aiBid = 1;
        if (p->getHero() && p->getHero()->getMaxHp() >= 5 && aiBid > 0 && aiBid < 3) aiBid += 1; // 体力上限高可加一档
        if (ddzAiBidForTesting >= 0) aiBid = ddzAiBidForTesting; // 测试覆盖
        if (aiBid <= bestBid) aiChoice = 0;                 // 不叫
        else aiChoice = std::min(3, aiBid) - bestBid;       // 选项索引（1..）
        int choiceIdx = askChooseOption(p, opts, "【叫地主】请叫分（须高于当前最高分）", aiChoice);
        if (choiceIdx <= 0) continue;
        int bid = bestBid + choiceIdx;
        if (bid > bestBid) { bestBid = bid; bestSeat = seat; }
        logMessage("  【叫地主】" + p->getName() + " 叫 " + std::to_string(bestBid) + " 分。");
        if (bestBid == 3) break; // 叫 3 分：立即成为地主
    }
    if (bestSeat < 0) {
        bestSeat = 0; // 全不叫：首位默认地主
        bestBid = 1;  // 底分按最低
        logMessage("  【叫地主】无人叫分，座位 0 默认成为地主（底分 1）。");
    }
    ddzBaseScore = bestBid;
    logMessage("  【叫地主】" + players[bestSeat]->getName() + " 成为地主，底分 " + std::to_string(ddzBaseScore) + "。");
    return bestSeat;
}

std::vector<int> GameEngine::getDoudizhuScores() const {
    std::vector<int> scores(players.size(), 0);
    if (gameMode != GameMode::DOUDIZHU || winningFaction.empty()) return scores;
    int unit = ddzBaseScore; // 明牌/加倍已按用户要求删去（2026-10-04）
    bool landlordWon = (winningFaction == "地主胜");
    for (size_t i = 0; i < players.size(); ++i) {
        bool isLandlord = players[i]->getIdentity() == Identity::DI_ZHU;
        if (isLandlord) scores[i] = landlordWon ? 2 * unit : -2 * unit;
        else scores[i] = landlordWon ? -unit : unit;
    }
    return scores;
}

void GameEngine::logDoudizhuSettlement() const {
    auto scores = getDoudizhuScores();
    if (scores.empty()) return;
    std::string line = "【斗地主结算】" + winningFaction + " | 底分 " + std::to_string(ddzBaseScore) +
                       "（本地计分，不涉及任何资源）";
    logMessage(line);
    for (size_t i = 0; i < players.size(); ++i) {
        logMessage("  " + players[i]->getName() + "（" + players[i]->getIdentityString() + "）: " +
                   (scores[i] >= 0 ? "+" : "") + std::to_string(scores[i]) + " 分");
    }
}

// ==================== 开局换牌（用户 2026-10-06） ====================
// “每个人可以换七次牌（初始手牌，属于游戏开始前，此时任何技能都未发动）。”
// 规则落地：
//   · 每人至多 mulliganMaxSwaps（默认 7）次；每次换掉**一张**手牌并立即从牌堆补一张；
//   · 换掉的牌置于牌堆底（不洗回牌堆顶，避免同一张牌被当场换回）；
//   · 全程直接操作牌堆与手牌，不经过任何引擎通知 → 任何技能都不会因此发动；
//   · 顺序：从当前回合角色（主公/地主）开始，按座位依次；AI 有简单的换牌策略（见下），
//     真人逐个询问（输入编号换牌，0 结束）。
void GameEngine::runStartMulligan() {
    if (players.empty()) return;
    logMessage("开局换牌：每名角色可用至多 " + std::to_string(mulliganMaxSwaps) +
               " 次更换初始手牌（游戏开始前，此时任何技能都未发动）。");
    // 从当前回合角色开始（身份场＝主公、斗地主＝地主），按座位依次换牌。
    int start = (gameMode == GameMode::DOUDIZHU) ? ddzLandlordSeat : lordSeat();
    std::vector<int> order;
    for (size_t i = 0; i < players.size(); ++i)
        order.push_back((start + static_cast<int>(i)) % static_cast<int>(players.size()));
    for (int seat : order) {
        PlayerPtr p = players[seat];
        if (!p || !p->isAlive() || !p->getHero()) continue;
        int used = 0;
        while (used < mulliganMaxSwaps) {
            auto hand = p->getHandCards();
            if (hand.empty()) break;
            CardPtr pick = nullptr;
            if (p->isAI()) {
                // AI 简单策略：换掉价值最低的一张，只要它低于门槛（杀/闪/桃等基本牌不换）。
                CardPtr worst = AIController::chooseLeastValuableCard(hand);
                if (!worst || AIController::cardValue(worst) >= kMulliganAiKeepValue) break;
                pick = worst;
            } else {
                pick = ui->askChooseCard(p, hand,
                    "【换牌】更换初始手牌（剩余 " + std::to_string(mulliganMaxSwaps - used) +
                    " 次；输入编号换掉该牌，0 结束换牌）", true, nullptr);
                if (!pick) break;
            }
            p->removeHandCard(pick);
            deck.putOnBottom({pick});
            auto fresh = deck.drawCards(1);
            if (fresh.empty()) { deck.putOnTop({pick}); break; } // 牌堆空：回滚
            p->addHandCards(fresh);
            ++used;
            ++mulliganSwaps[seat];
            // 换牌属于游戏开始前的手牌交换，对其他玩家不公开抽到了什么（只报次数）。
            if (seat == humanSeat)
                logMessage("  【换牌】你换掉 " + pick->getFormattedName() + "，摸到 " +
                           fresh.front()->getFormattedName() + "（第 " + std::to_string(used) + "/" +
                           std::to_string(mulliganMaxSwaps) + " 次）。");
            else
                logMessage("  【换牌】" + p->getName() + " 换掉 1 张手牌（第 " + std::to_string(used) + "/" +
                           std::to_string(mulliganMaxSwaps) + " 次）。");
        }
        if (mulliganSwaps[seat] > 0)
            logMessage("  【换牌】" + p->getName() + " 共换牌 " + std::to_string(mulliganSwaps[seat]) + " 次。");
    }
}

void GameEngine::setupGame(int totalPlayers, int humanIndex, const std::vector<std::string>& heroIds,
                           const std::vector<Identity>& identitiesIn, bool mulliganNow) {
    deck.initStandardDeck();
    deck.shuffleDrawPile();
    // 除外区储备（原“游戏外”，巨象「随机从游戏外」）：不计入牌堆/弃牌堆的实体南蛮
    exilePile.clear();
    exilePile.push_back(std::make_shared<Card>(-9101, "南蛮入侵", Suit::SPADE, 14, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN));
    exilePile.push_back(std::make_shared<Card>(-9102, "南蛮入侵", Suit::HEART, 14, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN));

    players.clear();
    extraTurnDepth = 0;
    currentTurnIndex = 0;
    currentPhase = TurnPhase::NONE;
    gameOver = false;
    scheduledExtraTurns.clear();
    delayedTrickSources.clear();
    activeDying.reset();
    winningFaction = "";
    interactiveMode = (humanIndex >= 0 && humanIndex < totalPlayers);
    humanSeat = interactiveMode ? humanIndex : -1;

    // 身份由调用方给出：身份模式按官方配置表（座位 0 主公 + 随机入座）；
    // 斗地主为 1 地主 + 2 农民（地主先手）。
    std::vector<Identity> identities = identitiesIn;
    if ((int)identities.size() < totalPlayers) identities.resize(totalPlayers, Identity::NONG_MIN);

    // 明确指定的先占位；非法或撞人物的后续指定改走剩余池。
    // 随机分配每次从未占用的人物中抽取，避免黄忠/界黄忠等同场。
    std::vector<std::string> ids(totalPlayers);
    std::vector<std::string> usedPersons;
    for (int i = 0; i < totalPlayers && i < static_cast<int>(heroIds.size()); ++i) {
        const auto& requested = heroIds[i];
        if (!HeroRegistry::find(requested)) continue;
        auto person = HeroRegistry::personKey(requested);
        if (std::find(usedPersons.begin(), usedPersons.end(), person) != usedPersons.end()) continue;
        if (zhizunField && isHeroBannedInCurrentMode(requested)) {
            // 点将（显式指定武将）可以用全将；只有随机补位才走去禁将的池。
            if (!quietSetup)
                logMessage("  【至尊场·点将】" + heroDraftLabel(requested) +
                           " 在本模式禁止池内，因是显式指定仍可用。");
        }
        ids[i] = requested;
        usedPersons.push_back(person);
    }
    std::vector<std::string> pool = availableHeroPool();
    std::shuffle(pool.begin(), pool.end(), rng);
    for (auto& id : ids) {
        if (!id.empty()) continue;
        auto available = std::find_if(pool.begin(), pool.end(), [&](const std::string& candidate) {
            return std::find(usedPersons.begin(), usedPersons.end(), HeroRegistry::personKey(candidate)) == usedPersons.end();
        });
        if (available == pool.end()) break;
        id = *available;
        usedPersons.push_back(HeroRegistry::personKey(id));
        pool.erase(available);
    }

    for (int i = 0; i < totalPlayers; ++i) {
        std::string pName = (i == humanIndex) ? "玩家(你)" : ("电脑" + std::to_string(i));
        bool isAi = (i != humanIndex);

        HeroPtr hero = ids[i].empty() ? nullptr : HeroRegistry::create(ids[i]);
        if (!hero) {
            hero = std::make_shared<BlankHero>("通用武将" + std::to_string(i + 1), Country::QUN, Gender::MALE, 4);
        }

        auto p = std::make_shared<Player>(i, pName, identities[i], isAi, hero);
        players.push_back(p);
    }

    for (auto& p : players) {
        p->addHandCards(deck.drawCards(4));
    }

    // 开局换牌（用户 2026-10-06）：“每个人可以换七次牌（初始手牌，属于游戏开始前，
    // 此时任何技能都未发动）”。必须在任何技能钩子（onGameStart / 主公技授予日志）之前执行。
    mulliganSwaps.assign(players.size(), 0);
    if (mulliganNow && mulliganEnabled) runStartMulligan(); // 开关关闭时不换牌（全 AI 对局默认关闭）

    // 主公技当且仅当你是主公时才会在游戏开始时获得（用户 2026-10-05）。
    // 必须放在 quietSetup 提前返回之前：斗地主的正式建局同样走 quietSetup，
    // 否则地主/农民会带着主公技入场（斗地主没有主公，任何人都不能有主公技）。
    applyLordSkillsByIdentity();

    if (quietSetup) return; // 斗地主叫分预览的临时建局：不输出初始化信息（随后整局重建）
    logMessage("=================== 三国杀(thks)核心引擎初始化完成 ===================");
    logMessage("游戏人数: " + std::to_string(totalPlayers) + " 人 | 随机种子: " + std::to_string(seed));
    for (const auto& p : players) {
        bool showIdentity = isIdentityPublic(*p);
        logMessage(p->getFormattedStatus(showIdentity));
        if (p->getHero()) {
            logMessage("    武将: " + p->getHero()->getName() + "·" + p->getHero()->getTitle() +
                       " [" + p->getHero()->getPack() + "]");
            std::istringstream lines(p->getHero()->getSkillSummary());
            std::string line;
            while (std::getline(lines, line)) {
                if (!line.empty()) logMessage(line);
            }
        }
    }
    logMessage("======================================================================\n");

    applyLordSkillsByIdentity();
    runGameStartHooks();
}

// 主公技当且仅当你是主公时才会在游戏开始时获得（用户 2026-10-05）：
// 建局时按身份授予/剥夺——非主公（含斗地主的地主/农民）身上的主公技被移除，
// 主公保留自己武将的主公技；储君继位后再由 grantLordSkills() 补授予。
void GameEngine::applyLordSkillsByIdentity() {
    for (auto& p : players) {
        if (!p->getHero()) continue;
        if (p->getIdentity() == Identity::ZHU_GONG) grantLordSkills(p);
        else stripLordSkills(p);
    }
}

void GameEngine::stripLordSkills(PlayerPtr player) {
    if (!player || !player->getHero()) return;
    std::vector<std::string> lords;
    for (const auto& s : player->getHero()->getSkills())
        if (s && s->hasTag(SkillTag::LORD)) lords.push_back(s->getName());
    for (const auto& name : lords) {
        player->getHero()->removeSkill(name);
        if (!quietSetup)
            logMessage("  【主公技】" + player->getName() + " 不是主公，游戏开始时不获得主公技【" + name + "】。");
    }
}

void GameEngine::grantLordSkills(PlayerPtr player) {
    if (!player || !player->getHero()) return;
    // 以登记表的原始武将为基准补齐（继位者的主公技在建局时已被剥夺）
    HeroPtr fresh = HeroRegistry::create(player->getHero()->getId());
    if (!fresh) return;
    for (const auto& s : fresh->getSkills()) {
        if (!s || !s->hasTag(SkillTag::LORD)) continue;
        if (player->getHero()->findSkill(s->getName())) continue;
        player->getHero()->addSkill(s);
        if (!quietSetup)
            logMessage("  【主公技】" + player->getName() + " 是主公，游戏开始时获得主公技【" + s->getName() + "】。");
    }
}

// “游戏开始时”时点的技能统一入口（含蓄力技的初始点数、七星/化身/狂暴/明任/妙略/结姻等）。
void GameEngine::runGameStartHooks() {
    for (auto& p : players) {
        if (!p->isAlive()) continue;
        for (auto& s : getEffectiveSkills(*p)) {
            s->onGameStart(*this, *p);
        }
    }
}

// ==================== 基础查询 ====================

PlayerPtr GameEngine::getCurrentPlayer() const {
    if (players.empty()) return nullptr;
    return players[currentTurnIndex];
}

PlayerPtr GameEngine::getPlayerById(int id) const {
    for (const auto& p : players) {
        if (p->getId() == id) return p;
    }
    return nullptr;
}

bool GameEngine::isPlayerTurn(const Player& p) const {
    auto cur = getCurrentPlayer();
    return cur && cur->getId() == p.getId() && currentPhase != TurnPhase::NONE;
}

std::vector<PlayerPtr> GameEngine::getAlivePlayers() const {
    std::vector<PlayerPtr> result;
    for (const auto& p : players) {
        if (p->isAlive()) result.push_back(p);
    }
    return result;
}

std::vector<PlayerPtr> GameEngine::getOtherAlivePlayers(const Player& self) const {
    std::vector<PlayerPtr> result;
    for (const auto& p : players) {
        if (p->isAlive() && p->getId() != self.getId()) {
            result.push_back(p);
        }
    }
    return result;
}

int GameEngine::calculateDistance(const Player& from, const Player& to) const {
    if (from.getId() == to.getId()) return 0;

    auto alive = getAlivePlayers();
    int totalAlive = alive.size();
    if (totalAlive <= 1) return 1;

    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < totalAlive; ++i) {
        if (alive[i]->getId() == from.getId()) idx1 = i;
        if (alive[i]->getId() == to.getId()) idx2 = i;
    }

    if (idx1 == -1 || idx2 == -1) return 999;

    int rawDist = std::min(std::abs(idx1 - idx2), totalAlive - std::abs(idx1 - idx2));

    if (from.getOffensiveHorse()) {
        rawDist = std::max(1, rawDist - 1);
    }
    if (to.getDefensiveHorse()) {
        rawDist += 1;
    }

    for (const auto& skill : getEffectiveSkills(from)) {
        skill->onCalculateDistance(*const_cast<GameEngine*>(this), from, to, rawDist);
    }
    for (const auto& skill : getEffectiveSkills(to)) {
        skill->onCalculateDistanceToYou(*const_cast<GameEngine*>(this), to, from, rawDist);
    }
    // 势-克昌（主公技）：群势力角色使用【杀】无距离限制（全局主公技，需遍历场上所有持有者）
    // 官网：「群势力角色使用【杀】无距离限制」为全场增益，不限于持有者本人
    if (from.getHero() && from.getHero()->getCountry() == Country::QUN) {
        for (const auto& pp : getAlivePlayers()) {
            if (!pp->getHero()) continue;
            auto sk = pp->getHero()->findSkill("势-克昌");
            if (!sk) continue;
            bool effective = false;
            for (const auto& es : getEffectiveSkills(*pp)) if (es->getName() == "势-克昌") { effective = true; break; }
            if (effective) { rawDist = 1; break; }
        }
    }
    // 防御性距离（神曹操【飞影】）属于目标侧，不与来源侧的马术混淆。
    if (to.getHero() && to.getHero()->findSkill("飞影")) rawDist++;
    return std::max(1, rawDist);
}

std::vector<PlayerPtr> GameEngine::getPlayersInRange(const Player& from, int range) const {
    std::vector<PlayerPtr> result;
    for (const auto& target : getOtherAlivePlayers(from)) {
        if (calculateDistance(from, *target) <= range) {
            result.push_back(target);
        }
    }
    return result;
}

std::vector<PlayerPtr> GameEngine::getPlayersAtDistance(const Player& from, int distance) const {
    std::vector<PlayerPtr> result;
    for (const auto& target : getOtherAlivePlayers(from)) {
        if (calculateDistance(from, *target) <= distance) {
            result.push_back(target);
        }
    }
    return result;
}

// ==================== 技能相关 ====================

std::vector<SkillPtr> GameEngine::getEffectiveSkills(const Player& p) const {
    std::vector<SkillPtr> result;
    // 规则（用户 2026-10-04）：死亡后非特殊技能不发动——已阵亡角色的技能不再参与任何广播/结算。
    // 例外：“死亡时”类技能在死亡结算窗口内为死者本人触发（断肠/行殇/武魂等）。
    if (!p.isAlive() && p.getId() != resolvingDeathPlayerId) return result;
    if (!p.getHero()) return result;
    for (const auto& s : p.getHero()->getSkills()) {
        if (p.isNonLockSkillsDisabled() && !s->hasEffectiveLockedComponent()) continue;
        // 主公技口径（用户 2026-10-05）：**当且仅当你是主公时才会在游戏开始时获得**主公技，
        // 而**只要你拥有主公技就能使用**。因此不在运行期按身份过滤，而是在建局时
        // 由 applyLordSkillsByIdentity() 授予/剥夺（储君继位后再补授予）。
        result.push_back(s);
    }
    return result;
}

bool GameEngine::removeHeroSkill(PlayerPtr player,const std::string& name) {
    if(!player || !player->getHero())return false;
    auto skill=player->getHero()->findSkill(name);
    if(!skill || skill->isSustained())return false;
    skill->onRemoved(*this,*player);
    return player->getHero()->removeSkill(name);
}

void GameEngine::removeHeroSkills(PlayerPtr player) {
    if(!player || !player->getHero())return;
    // “失去所有技能”仍不能剥离持恒技；逐项走统一移除入口以保留其持续效果。
    auto skills=player->getHero()->getSkills();
    for(auto skill:skills)if(skill)removeHeroSkill(player,skill->getName());
}

int GameEngine::calculateHandLimit(PlayerPtr player) {
    if(!player)return 0;
    int limit=player->getHandLimit();
    // 【瞒天过海】在任何角色手牌中均不计入手牌上限，
    // 包括实体牌和【势-妙略】生成的手牌；交换/转移后也不改变此属性。
    for(const auto& card:player->getHandCards())
        if(card && card->getSubType()==CardSubType::MANTIAN_GUOHAI)++limit;
    for(const auto& skill:getEffectiveSkills(*player))
        skill->onCalculateHandLimit(*this,*player,limit);
    return limit;
}

int GameEngine::getShaLimit(const Player& p) const {
    int limit = 1;
    if (p.getWeapon() && p.getWeapon()->getName() == "诸葛连弩") limit = 999;
    for (const auto& s : getEffectiveSkills(p)) {
        s->onCalculateShaLimit(*const_cast<GameEngine*>(this), p, limit);
    }
    return limit;
}

int GameEngine::getShaLimitForCard(const Player& p,CardPtr sha) const {
    int limit=getShaLimit(p);
    if (sha && p.hasChenglueUnlimitedSuit(effectiveSuit(p,sha))) limit=999;
    for(auto skill:getEffectiveSkills(p))skill->onCalculateShaCardLimit(*const_cast<GameEngine*>(this),p,sha,limit);
    return limit;
}

bool GameEngine::canUseOriginalCard(const Player& p,CardPtr card) const {
    if(!card || card->isVirtual() || !p.hasHandCard(card))return true;
    for(auto skill:getEffectiveSkills(p)) {
        if(skill->getName()=="武神" && effectiveSuit(p,card)==Suit::HEART)return false;
        // 锁定技【无谋】禁止把普通锦囊当原牌使用/打出，延时锦囊不受影响。
        if(skill->getName()=="无谋" && card->getType()==CardType::TRICK &&
           card->getSubType()!=CardSubType::LE_BU_SI_SHU &&
           card->getSubType()!=CardSubType::SHAN_DIAN &&
           card->getSubType()!=CardSubType::BING_LIANG_CUN_DUAN)return false;
    }
    return true;
}

int GameEngine::getShaTargetLimit(const Player& p, CardPtr sha) const {
    int count=1;
    if(auto original=wumouMaterial(sha)) {
        auto st=original->getSubType();
        if(wumouAllTargets(st))return static_cast<int>(getAlivePlayers().size());
        if(st==CardSubType::TIE_SUO_LIAN_HUAN) {
            int limit=2;
            for(const auto& skill:getEffectiveSkills(p))
                skill->onCalculateTieSuoTargetLimit(*const_cast<GameEngine*>(this),p,limit);
            return limit;
        }
        if(st==CardSubType::JIE_DAO_SHA_REN)return 2;
    }
    if(canUseFangTianExtraTargets(p,sha))count+=2;
    for(const auto& skill:getEffectiveSkills(p))
        skill->onCalculateShaTargets(*const_cast<GameEngine*>(this),p,sha,count);
    return count;
}

CardPtr GameEngine::materializeConversion(PlayerPtr player, CardPtr card) {
    if(!player || !card || card->getSkillSource()!="龙魂" || card->getSubCards().size()!=1)return card;
    auto first=card->getSubCards().front();
    std::vector<CardPtr> alternatives;
    for(auto c:player->getAllCards())if(c!=first && effectiveSuit(*player,c)==effectiveSuit(*player,first) &&
                         (player->hasHandCard(c) || player->hasEquipment(c)))alternatives.push_back(c);
    if(alternatives.empty())return card;
    bool worthExtra=card->getShaElement()==ShaElement::FIRE ||
        (card->getSubType()==CardSubType::TAO &&
         (player->getMaxHp()-player->getHp()>=2 || (activeDying && activeDying->getHp()<0))) ||
        (getCurrentPlayer() && isPlayerTurn(*getCurrentPlayer()) &&
         !AIController::isFriend(*this, *player,*getCurrentPlayer()) &&
         !getCurrentPlayer()->getAllCards().empty());
    if(!askConfirm(player,"【龙魂】是否再选择一张同花色牌强化效果？",worthExtra))return card;
    auto second=askChooseCard(player,alternatives,"【龙魂】选择第二张同花色牌",false,
                              AIController::chooseLeastValuableCard(alternatives));
    if(!second)return card;
    return Card::makeVirtual(card->getName(),card->getType(),card->getSubType(),
                             {first,second},"龙魂",card->getShaElement());
}

bool GameEngine::canUseShaOn(const Player& source, const Player& target, CardPtr sha,
                             bool ignoreDistance) const {
    if (!target.isAlive()) return false;
    auto original=wumouMaterial(sha);
    if(source.getId()==target.getId() && !original)return false;
    if(original) {
        // 【无谋】按原锦囊确定可指定的角色，不能绕过原牌的目标禁令（帷幕等）。
        // “对目标无效”（祸首/巨象）不是“不能成为目标”，不在此排除。
        if(!const_cast<GameEngine*>(this)->canBeTargeted(getPlayerById(target.getId()),original,
                                                         getPlayerById(source.getId())))return false;
        auto st=original->getSubType();
        // 无懈可击原本指定的是锦囊牌而非角色，不能凭空指定一名角色作【杀】的目标；
        // 作为需要打出【杀】的响应材料时仍可通过无谋转化。
        if(st==CardSubType::WU_XIE_KE_JI)return false;
        if(st==CardSubType::WU_ZHONG_SHENG_YOU)return source.getId()==target.getId();
        if(st==CardSubType::NAN_MAN_RU_QIN || st==CardSubType::WAN_JIAN_QI_FA)
            return source.getId()!=target.getId();
        if(st==CardSubType::TAO_YUAN_JIE_YI || st==CardSubType::WU_GU_FENG_DENG ||
           st==CardSubType::TIE_SUO_LIAN_HUAN)return true;
        if(st==CardSubType::JIE_DAO_SHA_REN) {
            for(auto holder:getJieDaoWeaponHolders(source)) {
                if(holder.get()==&target)return true;
                for(auto victim:getJieDaoVictims(*holder))if(victim.get()==&target)return true;
            }
            return false;
        }
        if(source.getId()==target.getId())return false;
        if(st==CardSubType::GUO_HE_CHAI_QIAO || st==CardSubType::SHUN_SHOU_QIAN_YANG)
            return !target.getAllCards().empty() &&
                   (st!=CardSubType::SHUN_SHOU_QIAN_YANG || calculateDistance(source,target)<=1);
        if(st==CardSubType::HUO_GONG)return target.getHandCardCount()>0;
        return true; // 决斗等原锦囊没有杀的攻击距离限制
    }
    bool can = ignoreDistance || (sha && sha->getSkillSource() == "挑衅") ||
               calculateDistance(source, target) <= source.getAttackRange() ||
               (sha && source.hasChenglueUnlimitedSuit(effectiveSuit(source, sha)));
    for (const auto& s : getEffectiveSkills(source)) {
        s->onCheckShaTarget(*const_cast<GameEngine*>(this), source, target, sha, can);
    }
    return can;
}

bool GameEngine::canUseShaBeyondLimitOn(const Player& source, const Player& target, CardPtr sha) const {
    for (const auto& skill : getEffectiveSkills(source))
        if (skill->canUseShaBeyondLimitOn(*const_cast<GameEngine*>(this), source, target, sha)) return true;
    return false;
}

bool GameEngine::canUseBingLiangOn(const Player& source,const Player& target,CardPtr card) const {
    if(source.getId()==target.getId() || !target.isAlive() ||
       hasJudgeCardOf(target,CardSubType::BING_LIANG_CUN_DUAN))return false;
    if(card && source.hasChenglueUnlimitedSuit(effectiveSuit(source,card)))return true;
    if(calculateDistance(source,target)<=1)return true;
    for(auto skill:getEffectiveSkills(source)) {
        if(skill->getName()=="奇才" ||
           (skill->getName()=="断粮" && calculateDistance(source,target)<=2))return true;
    }
    return false;
}

bool GameEngine::canUseShunShouOn(const Player& source,const Player& target,CardPtr card) const {
    if(source.getId()==target.getId() || !target.isAlive() || target.getAllCards().empty())return false;
    return (card && card->getSkillSource()=="势-急袭") || calculateDistance(source,target)<=1 ||
           (card && source.hasChenglueUnlimitedSuit(effectiveSuit(source,card)));
}

bool GameEngine::hasJudgeCardOf(const Player& p, CardSubType type) {
    for (const auto& c : p.getJudgeZone()) {
        if (c->getSubType() == type) return true;
    }
    return false;
}

std::vector<PlayerPtr> GameEngine::getShaTargets(const Player& source, CardPtr sha) const {
    std::vector<PlayerPtr> result;
    auto pool=wumouMaterial(sha) ? getAlivePlayers() : getOtherAlivePlayers(source);
    for (const auto& t : pool) {
        if (canUseShaOn(source, *t, sha) &&
            const_cast<GameEngine*>(this)->canBeTargeted(t,sha,getPlayerById(source.getId())))result.push_back(t);
    }
    return result;
}

std::vector<CardPtr> GameEngine::getConversionsFor(PlayerPtr player, CardPtr card, CardSubType wanted) const {
    std::vector<CardPtr> result;
    if (!player || !card) return result;
    for (const auto& s : getEffectiveSkills(*player)) {
        auto variants=s->convertCards(*const_cast<GameEngine*>(this),*player,card,wanted);
        result.insert(result.end(),variants.begin(),variants.end());
    }
    return result;
}

std::vector<CardPtr> GameEngine::getSkillConversionsFor(const SkillPtr& skill, PlayerPtr player, CardPtr card) const {
    std::vector<CardPtr> result;
    if (!skill || !player || !card || card->isVirtual()) return result;
    // 遍历 CardSubType 全表：技能自己决定某张牌能否变成某种牌（convertCards 返回空即“不适合”）。
    for (int i = 0; i <= static_cast<int>(CardSubType::DEFENSIVE_HORSE); ++i) {
        auto variants = skill->convertCards(*const_cast<GameEngine*>(this), *player, card,
                                           static_cast<CardSubType>(i));
        for (const auto& v : variants) {
            if (!v) continue;
            bool duplicate = std::any_of(result.begin(), result.end(), [&](const CardPtr& e) {
                return e->getSubType() == v->getSubType() && e->getSkillSource() == v->getSkillSource() &&
                       e->getShaElement() == v->getShaElement();
            });
            if (!duplicate) result.push_back(v);
        }
    }
    return result;
}

std::vector<CardPtr> GameEngine::getAllConversionsFor(PlayerPtr player, CardPtr card) const {
    std::vector<CardPtr> result;
    if (!player || !card || card->isVirtual()) return result;
    for (const auto& skill : getEffectiveSkills(*player)) {
        auto variants = getSkillConversionsFor(skill, player, card);
        for (const auto& v : variants) {
            bool duplicate = std::any_of(result.begin(), result.end(), [&](const CardPtr& e) {
                return e->getSubType() == v->getSubType() && e->getSkillSource() == v->getSkillSource() &&
                       e->getShaElement() == v->getShaElement();
            });
            if (!duplicate) result.push_back(v);
        }
    }
    return result;
}

bool GameEngine::canPlayCardNow(const Player& player, CardPtr card) const {
    if (!card || !player.isAlive()) return false;
    switch (card->getSubType()) {
        case CardSubType::SHAN:
        case CardSubType::WU_XIE_KE_JI:
        case CardSubType::SHAN_DIAN:
            return false; // 这些牌没有“出牌阶段主动使用”的时机
        case CardSubType::TAO:
            return player.isWounded();
        case CardSubType::SHA:
            return !getShaTargets(player, card).empty();
        case CardSubType::GUO_HE_CHAI_QIAO:
            for (const auto& t : getOtherAlivePlayers(player))
                if (!t->getAllCards().empty() &&
                    const_cast<GameEngine*>(this)->canBeTargeted(t, card, getPlayerById(player.getId())))
                    return true;
            return false;
        case CardSubType::SHUN_SHOU_QIAN_YANG:
            for (const auto& t : getOtherAlivePlayers(player))
                if (!t->getAllCards().empty() && canUseShunShouOn(player, *t, card) &&
                    const_cast<GameEngine*>(this)->canBeTargeted(t, card, getPlayerById(player.getId())))
                    return true;
            return false;
        case CardSubType::LE_BU_SI_SHU:
            for (const auto& t : getOtherAlivePlayers(player))
                if (!hasJudgeCardOf(*t, CardSubType::LE_BU_SI_SHU) &&
                    const_cast<GameEngine*>(this)->canBeTargeted(t, card, getPlayerById(player.getId())))
                    return true;
            return false;
        case CardSubType::HUO_GONG:
            for (const auto& t : getOtherAlivePlayers(player))
                if (t->getHandCardCount() > 0 &&
                    const_cast<GameEngine*>(this)->canBeTargeted(t, card, getPlayerById(player.getId())))
                    return true;
            return false;
        case CardSubType::JUE_DOU:
            for (const auto& t : getOtherAlivePlayers(player))
                if (const_cast<GameEngine*>(this)->canBeTargeted(t, card, getPlayerById(player.getId())))
                    return true;
            return false;
        case CardSubType::JIE_DAO_SHA_REN:
            for (const auto& holder : getJieDaoWeaponHolders(player))
                if (!getJieDaoVictims(*holder).empty()) return true;
            return false;
        case CardSubType::TIE_SUO_LIAN_HUAN:
            return true; // 无目标时可直接重铸
        default:
            return true;
    }
}

std::vector<CardPtr> GameEngine::getResponseCandidates(PlayerPtr player, CardSubType wanted) const {
    std::vector<CardPtr> result;
    if (!player || !player->isAlive()) return result;
    if (!player->isHandCardsBanned()) {
        for (const auto& c : player->getHandCards()) {
            if (c->getSubType() == wanted && canUseOriginalCard(*player,c)) result.push_back(c);
        }
        for (const auto& c : player->getHandCards()) {
            auto conv = getConversionsFor(player, c, wanted);
            result.insert(result.end(), conv.begin(), conv.end());
        }
    }
    // 技能对具体响应牌进行最终合法性检查（如谋·韩当【弓骑】）。
    result.erase(std::remove_if(result.begin(), result.end(), [&](const CardPtr& candidate) {
        bool allowed = true;
        for (const auto& holder : getAlivePlayers())
            for (const auto& skill : getEffectiveSkills(*holder))
                skill->onCheckResponseCard(*this, *holder, *player, candidate, wanted, allowed);
        return !allowed;
    }), result.end());
    // 好施：可如手牌般使用或打出借用手牌的来源角色的手牌
    auto borrowedFrom = getBorrowedHandFrom(player);
    if (borrowedFrom && !player->isHandCardsBanned()) {
        for (const auto& c : borrowedFrom->getHandCards()) {
            if (c->getSubType() == wanted && canUseOriginalCard(*borrowedFrom, c)) result.push_back(c);
        }
    }
    // 装备区的牌也可能被转化（如武圣的红色装备）
    for (const auto& c : player->getAllEquipment()) {
        auto conv = getConversionsFor(player, c, wanted);
        result.insert(result.end(), conv.begin(), conv.end());
    }
    // 丈八蛇矛：两张手牌当【杀】（先给出占位牌，选中后再指定两张手牌）
    if (wanted == CardSubType::SHA && !player->isHandCardsBanned() && hasWeapon(*player, "丈八蛇矛") &&
        player->getHandCardCount() >= 2) {
        result.push_back(Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "丈八蛇矛"));
    }
    result.erase(std::remove_if(result.begin(), result.end(), [&](const CardPtr& candidate) {
        bool allowed = true;
        for (const auto& holder : getAlivePlayers())
            for (const auto& skill : getEffectiveSkills(*holder))
                skill->onCheckResponseCard(*this, *holder, *player, candidate, wanted, allowed);
        return !allowed;
    }), result.end());
    return result;
}

// ==================== 交互辅助 ====================

// ==================== 交互辅助（委托给 Interaction） ====================

void GameEngine::viewCards(PlayerPtr viewer, const std::vector<CardPtr>& cards, const std::string& prompt) {
    ui->viewCards(std::move(viewer), cards, prompt);
}

bool GameEngine::askConfirm(PlayerPtr player, const std::string& prompt, bool aiDecision) {
    return ui->askConfirm(std::move(player), prompt, aiDecision);
}

CardPtr GameEngine::askChooseCard(PlayerPtr player, const std::vector<CardPtr>& candidates, const std::string& prompt,
                                  bool optional, CardPtr aiChoice) {
    return ui->askChooseCard(std::move(player), candidates, prompt, optional, std::move(aiChoice));
}

PlayerPtr GameEngine::askChoosePlayer(PlayerPtr player, const std::vector<PlayerPtr>& candidates, const std::string& prompt,
                                      bool optional, PlayerPtr aiChoice) {
    return ui->askChoosePlayer(std::move(player), candidates, prompt, optional, std::move(aiChoice));
}

int GameEngine::askChooseOption(PlayerPtr player, const std::vector<std::string>& options, const std::string& prompt, int aiChoice) {
    return ui->askChooseOption(std::move(player), options, prompt, aiChoice);
}

CardPtr GameEngine::chooseHiddenHandCard(PlayerPtr chooser, PlayerPtr owner, const std::string& prompt) {
    if (!chooser || !owner || owner->getHandCards().empty()) return nullptr;
    std::vector<std::string> backs;
    for(size_t i=0;i<owner->getHandCards().size();++i)
        backs.push_back("隐藏手牌第"+std::to_string(i+1)+"张");
    int random=std::uniform_int_distribution<int>(0,static_cast<int>(backs.size())-1)(rng);
    int selected=askChooseOption(chooser,backs,prompt,random);
    if(selected<0 || selected>=static_cast<int>(backs.size()))return nullptr;
    return owner->getHandCards()[static_cast<size_t>(selected)];
}

CardPtr GameEngine::chooseCardFromPlayer(PlayerPtr chooser, PlayerPtr owner,const std::string& prompt,
                                               bool includeJudgeZone) {
    if(!chooser || !owner)return nullptr;
    auto hand=owner->getHandCards(),equipment=owner->getAllEquipment();
    auto judgement=includeJudgeZone ? owner->getJudgeZone() : std::vector<CardPtr>{};
    std::vector<std::string> zones;
    if(!hand.empty())zones.push_back("手牌区（"+std::to_string(hand.size())+"张，随机）");
    if(!equipment.empty())zones.push_back("装备区");
    if(includeJudgeZone && !judgement.empty())zones.push_back("判定区");
    if(zones.empty())return nullptr;
    int ai=0;
    if(chooser->isAI()) {
        // 只能凭公开信息评估装备/判定区；不能偷看目标手牌以作弊。
        int best=3;
        if(!equipment.empty()) {
            ai=hand.empty()?0:1;
            best=4;
        }
        if(!judgement.empty() && best<4)ai=static_cast<int>(zones.size())-1;
    }
    int selected=askChooseOption(chooser,zones,prompt,ai);
    if(selected<0 || selected>=static_cast<int>(zones.size()))return nullptr;
    const auto& zone=zones[selected];
    if(zone.find("手牌区")==0)return hand[std::uniform_int_distribution<size_t>(0,hand.size()-1)(rng)];
    if(zone=="装备区")return askChooseCard(chooser,equipment,prompt+"：选择装备",false,equipment.front());
    return askChooseCard(chooser,judgement,prompt+"：选择判定牌",false,judgement.front());
}

// ==================== 牌的移动 ====================

void GameEngine::drawCards(PlayerPtr player, int count, const std::string& reason) {
    if (gameOver) return; // 游戏结束后不再有牌结算
    if (!player || count <= 0) return;
    int _handBefore = player->getHandCardCount();
    bool fromBottom = false;
    for (const auto& skill : getEffectiveSkills(*player))
        if (skill->drawsFromBottom()) { fromBottom = true; break; }
    auto drawn = fromBottom ? deck.drawCardsFromBottom(count) : deck.drawCards(count);
    player->addHandCards(drawn);
    std::string why = reason.empty() ? "" : "（" + reason + "）";
    logMessage("  [" + player->getName() + "] 摸了 " + std::to_string(drawn.size()) + " 张牌" + why + "。");
    {
        int gained = player->getHandCardCount() - _handBefore;
        if (gained > 0) {
            notifyCardsObtained(player, gained);
            for (auto& holder : getAlivePlayers())
                for (auto& skill : getEffectiveSkills(*holder))
                    skill->onAnyCardsObtained(*this, *holder, *player, gained);
        }
    }
    updateXieLiOnDraw(player, (int)drawn.size());
}

bool GameEngine::canPutUsedCardOnTop(CardPtr usedCard) const {
    if (!usedCard) return false;
    auto realCards = usedCard->getRealCards(usedCard);
    if (realCards.size() != 1 || !realCards.front() || realCards.front()->isVirtual()) return false;
    CardPtr physical = realCards.front();
    if (std::find(deck.getDrawPile().begin(), deck.getDrawPile().end(), physical) != deck.getDrawPile().end())
        return false;
    if (std::find(deck.getDiscardPile().begin(), deck.getDiscardPile().end(), physical) != deck.getDiscardPile().end())
        return true;
    for (const auto& owner : players) {
        if (!owner) continue;
        if (owner->hasHandCard(physical) || owner->hasEquipment(physical) || owner->hasCardInAnyPile(physical) ||
            std::find(owner->getJudgeZone().begin(), owner->getJudgeZone().end(), physical) != owner->getJudgeZone().end())
            return true;
    }
    const auto& outside = exilePile;
    return std::find(outside.begin(), outside.end(), physical) != outside.end();
}

bool GameEngine::putUsedCardOnTop(PlayerPtr source, CardPtr usedCard) {
    if (!canPutUsedCardOnTop(usedCard)) return false;
    auto realCards = usedCard->getRealCards(usedCard);
    CardPtr physical = realCards.front();
    bool detached = deck.removeDiscardCard(physical);
    if (!detached) {
        for (const auto& owner : players) {
            if (!owner) continue;
            if (owner->hasEquipment(physical)) {
                CardPtr removed = owner->removeEquipment(physical->getSubType());
                if (removed == physical) {
                    afterEquipmentLost(owner, physical);
                    notifyCardLostOutsideTurn(owner, physical);
                    detached = true;
                    break;
                }
            }
            if (owner->removeJudgeCard(physical)) {
                delayedTrickSources.erase(physical.get());
                detached = true;
                break;
            }
            if (owner->hasHandCard(physical) && loseHandCard(owner, physical)) {
                detached = true;
                break;
            }
            if (owner->removeCardFromAnyPile(physical)) {
                detached = true;
                break;
            }
        }
    }
    if (!detached) {
        auto it = std::find(exilePile.begin(), exilePile.end(), physical);
        if (it != exilePile.end()) {
            exilePile.erase(it);
            detached = true;
        }
    }
    if (!detached) return false;
    deck.putOnTop({physical});
    logMessage("  【恃才】将 " + physical->getFormattedName() + " 置于牌堆顶。");
    (void)source;
    return true;
}

bool GameEngine::loseHandCard(PlayerPtr owner,CardPtr card) {
    if(!owner || !card || !owner->hasHandCard(card))return false;
    int before=owner->getHandCardCount();
    owner->removeHandCard(card);
    notifyHandEmpty(owner,before);
    notifyCardLostOutsideTurn(owner,card);
    notifyAnyCardLost(owner,card);
    return true;
}

bool GameEngine::equipHandCard(PlayerPtr from,PlayerPtr to,CardPtr card) {
    if(!from || !to || !card || card->getType()!=CardType::EQUIPMENT ||
       !loseHandCard(from,card))return false;
    auto replaced=to->equip(card);
    if(replaced) {
        deck.discardCard(replaced);
        afterEquipmentLost(to,replaced);
        notifyCardLostOutsideTurn(to,replaced);
        notifyAnyCardLost(to,replaced);
    }
    return true;
}

void GameEngine::notifyHandEmpty(PlayerPtr player, int beforeCount) {
    if (!player || !player->isAlive() || beforeCount <= 0 || player->getHandCardCount() > 0) return;
    for (const auto& s : getEffectiveSkills(*player)) s->onHandEmpty(*this, *player);
    for (auto& p : getOtherAlivePlayers(*player))
        for (auto& s : getEffectiveSkills(*p)) s->onOtherHandEmpty(*this, *p, *player);
}

void GameEngine::notifyCardLostOutsideTurn(PlayerPtr owner,CardPtr card) {
    if(owner && card && owner->isAlive() && !isPlayerTurn(*owner))
        for(auto skill:getEffectiveSkills(*owner))skill->onCardLostOutsideTurn(*this,*owner,card);
}

void GameEngine::notifyAnyCardLost(PlayerPtr owner, CardPtr card) {
    if (!owner || !card || !owner->isAlive()) return;
    for (auto skill : getEffectiveSkills(*owner)) skill->onAnyCardLost(*this, *owner, card);
}

void GameEngine::notifyBorrowedHandCardUsed(PlayerPtr lender, PlayerPtr borrower, CardPtr card) {
    if (!lender || !borrower || !card || !lender->isAlive()) return;
    for (auto skill : getEffectiveSkills(*lender))
        skill->onBorrowedHandCardUsed(*this, *lender, *borrower, card);
}

void GameEngine::notifyHandCardLostToOther(PlayerPtr victim, PlayerPtr instigator) {
    if(!victim || !instigator || victim==instigator)return;
    for(auto& p:getAlivePlayers())for(auto skill:getEffectiveSkills(*p))
        skill->onHandCardLostToOther(*this,*p,*victim,*instigator);
}

void GameEngine::notifyCardsObtained(PlayerPtr to, int count) {
    if (!to || count <= 0) return;
    for (auto skill : getEffectiveSkills(*to)) skill->onCardsObtained(*this, *to, count);
}

void GameEngine::notifyCardUsed(PlayerPtr source, CardPtr card) {
    if (!source || !card) return;
    const bool firstOfType = !source->hasUsedCardTypeThisTurn(card->getType());
    source->recordCardTypeUsedThisTurn(card->getType());
    for (auto skill : getEffectiveSkills(*source))
        skill->onCardUsed(*this, *source, card, firstOfType);
    updateXieLiOnUse(source, card->getSuit());
}

void GameEngine::notifyAnyCardPlayed(PlayerPtr user, CardPtr card) {
    if (!user || !card) return;
    for (auto& pp : getAlivePlayers())
        for (auto sk : getEffectiveSkills(*pp)) sk->onAnyCardPlayed(*this, *pp, *user, card);
}

void GameEngine::notifyCardsLostBatch(PlayerPtr victim, int count, PlayerPtr instigator) {
    if (!victim || count < 2) return;
    for (auto& p : getAlivePlayers())
        for (auto skill : getEffectiveSkills(*p))
            skill->onCardsLostBatch(*this, *p, *victim, count);
    (void)instigator;
}

CardPtr GameEngine::takeFromExile(const std::function<bool(const CardPtr&)>& pred, std::mt19937& rng) {
    std::vector<CardPtr> cands;
    for (auto& c : exilePile) if (c && pred(c)) cands.push_back(c);
    if (cands.empty()) return nullptr;
    std::uniform_int_distribution<size_t> d(0, cands.size() - 1);
    CardPtr c = cands[d(rng)];
    exilePile.erase(std::find(exilePile.begin(), exilePile.end(), c));
    return c;
}

bool GameEngine::moveToExile(CardPtr card) {
    if (!card) return false;
    CardPtr physical = card;
    if (std::find(exilePile.begin(), exilePile.end(), physical) != exilePile.end()) return true; // 幂等
    bool detached = deck.removeDiscardCard(physical);
    if (!detached) {
        for (const auto& owner : players) {
            if (!owner) continue;
            if (owner->hasEquipment(physical)) {
                CardPtr removed = owner->removeEquipment(physical->getSubType());
                if (removed == physical) {
                    afterEquipmentLost(owner, physical);
                    notifyCardLostOutsideTurn(owner, physical);
                    detached = true;
                    break;
                }
            }
            if (owner->removeJudgeCard(physical)) {
                delayedTrickSources.erase(physical.get());
                detached = true;
                break;
            }
            if (owner->hasHandCard(physical) && loseHandCard(owner, physical)) {
                detached = true;
                break;
            }
            if (owner->removeCardFromAnyPile(physical)) {
                detached = true;
                break;
            }
        }
    }
    exilePile.push_back(physical);
    return true;
}

bool GameEngine::isInExile(CardPtr card) const {
    return card && std::find(exilePile.begin(), exilePile.end(), card) != exilePile.end();
}

void GameEngine::resetPlayPhaseUses(PlayerPtr player) {
    if (!player || !player->getHero()) return;
    for (auto& sk : player->getHero()->getSkills()) {
        if (auto* act = dynamic_cast<ActiveSkill*>(sk.get())) act->resetPhaseUses();
    }
}

void GameEngine::consumeCharge(PlayerPtr player, int count) {
    if (!player || count <= 0) return;
    player->addMark("蓄力", -std::min(count, player->getMark("蓄力")));
    for (auto skill : getEffectiveSkills(*player)) skill->onChargeConsumed(*this, *player, count);
}

PlayerPtr GameEngine::getBorrowedHandFrom(PlayerPtr player) const {
    if (!player) return nullptr;
    // 标记存的是“来源角色 id + 1”（0 保留为“无标记”，玩家 id 从 0 起）——
    // 2026-10-06 实现级复核：原先直接与 id 比较，导致查到的总是标记持有者自己，
    // 【势-好施】“其可以如手牌般使用或打出你的手牌”从未生效。
    int src = player->getMark("好施借牌") - 1;
    if (src < 0) return nullptr;
    for (auto& p : players) if (p && p->getId() == src && p->getId() != player->getId() && p->isAlive()) return p;
    return nullptr;
}

void GameEngine::consumeCard(PlayerPtr player, CardPtr card, bool toDiscardPile) {
    if (!player || !card) return;
    bool fromHand = false;
    int handBefore = player->getHandCardCount();
    auto reals = card->getRealCards(card);
    // 【势-妙略】将无素材的虚拟【瞒天过海】置入手牌；这种牌不进入弃牌堆，
    // 但使用/弃置时必须从手牌区移除，否则 AI 会反复使用同一张“幽灵牌”。
    if (reals.empty() && player->hasHandCard(card)) {
        player->removeHandCard(card);
        fromHand = true;
        notifyCardLostOutsideTurn(player,card);
    }
    std::vector<CardPtr> lostEquipment;
    PlayerPtr lostOwner = player; // 实际失去这张牌的角色（借出的手牌由来源失去）
    for (auto& r : reals) {
        bool owned=false;
        if (player->hasHandCard(r)) {
            player->removeHandCard(r);
            fromHand = owned = true;
        } else if (auto borrowSrc = getBorrowedHandFrom(player); borrowSrc && borrowSrc->hasHandCard(r)) {
            const int lenderHandBefore = borrowSrc->getHandCardCount();
            borrowSrc->removeHandCard(r);
            fromHand = owned = true;
            logMessage("  [" + player->getName() + "] 使用了 [" + borrowSrc->getName() + "] 借与的手牌。");
            // 2026-10-06 实现级复核：失去这张牌的**是借出来源**（原先按使用者上报，
            // 使来源的【势-好施】“因此失去最后的手牌”永远不触发，并让使用者误收“失去牌”通知）。
            lostOwner = borrowSrc;
            notifyCardLostOutsideTurn(borrowSrc, r);
            notifyAnyCardLost(borrowSrc, r);
            notifyBorrowedHandCardUsed(borrowSrc, player, r);
            notifyHandEmpty(borrowSrc, lenderHandBefore); // 来源的“失去最后的手牌”类技能同样要通知
        } else if (player->hasEquipment(r)) {
            player->removeEquipment(r->getSubType());
            lostEquipment.push_back(r);
            owned=true;
        }
        if (toDiscardPile) deck.discardCard(r);
        if(owned && lostOwner == player) notifyCardLostOutsideTurn(player,r);
    }
    for (auto& e : lostEquipment) afterEquipmentLost(player, e);
    if (fromHand) notifyHandEmpty(player, handBefore);
    if (fromHand && !isPlayerTurn(*player)) {
        for (auto& s : getEffectiveSkills(*player)) {
            s->onCardUsedOutsideTurn(*this, *player, card);
        }
    }
}

// 蓄力点的统一入口。官方描述只写“获得1点蓄力点”，点数上限来自武将牌上的
// “蓄力技（x/y）”标注（y 为上限）。没有上限时曾出现谋·姜维【挑衅】无限累积蓄力点，
// 进而在 1v1 对局里反复控制对手、无法收尾（判为僵局）——故这里统一封顶。
int GameEngine::gainCharge(Player& target, int amount) {
    if (amount <= 0) return 0;
    const int cap = target.getMark("蓄力上限");
    const int cur = target.getMark("蓄力");
    const int next = (cap > 0) ? std::min(cap, cur + amount) : cur + amount;
    if (next <= cur) return 0;
    target.addMark("蓄力", next - cur);
    return next - cur;
}

void GameEngine::discardCardOf(PlayerPtr owner, CardPtr card, const std::string& reason,PlayerPtr instigator) {
    if (!owner || !card) return;
    int handBefore = owner->getHandCardCount();
    bool fromHand = owner->hasHandCard(card);
    bool lostEquipment = false;
    bool fromJudge=std::find(owner->getJudgeZone().begin(),owner->getJudgeZone().end(),card)!=owner->getJudgeZone().end();
    bool owned=fromHand || owner->hasEquipment(card) || fromJudge;
    if (!owner->removeHandCard(card)) {
        if (owner->hasEquipment(card)) {
            owner->removeEquipment(card->getSubType());
            lostEquipment = true;
        } else {
            owner->removeJudgeCard(card);
        }
    }
    deck.discardCard(card);
    for (auto& pp : getAlivePlayers())
        for (auto sk : getEffectiveSkills(*pp)) sk->onAnyCardDiscarded(*this, *pp, *owner, card);
    if (fromHand && currentPhase == TurnPhase::DISCARD && getCurrentPlayer() == owner) {
        discardedThisPhase.push_back(card);
        for (auto& s : getEffectiveSkills(*owner)) s->onDiscardedInDiscardPhase(*this,*owner,card);
    }
    if (fromHand) {
        notifyHandEmpty(owner, handBefore);
        notifyHandCardLostToOther(owner,instigator);
    }
    std::string why = reason.empty() ? "" : ("（" + reason + "）");
    logMessage("  [" + owner->getName() + "] 弃置了 " + card->getFormattedName() + why);
    if (lostEquipment) afterEquipmentLost(owner, card);
    if(fromJudge)delayedTrickSources.erase(card.get());
    if(owned){notifyCardLostOutsideTurn(owner,card);notifyAnyCardLost(owner,card);updateXieLiOnDiscard(owner, card->getSuit());}
}

bool GameEngine::canMoveFieldCard(PlayerPtr from, PlayerPtr to, CardPtr card) const {
    if(!from || !to || !card || from==to || !from->isAlive() || !to->isAlive())return false;
    if(from->hasEquipment(card)) {
        for(auto equipped:to->getAllEquipment())if(equipped->getSubType()==card->getSubType())return false;
        return true;
    }
    if(std::find(from->getJudgeZone().begin(),from->getJudgeZone().end(),card)==from->getJudgeZone().end())return false;
    return !hasJudgeCardOf(*to,card->getSubType());
}

bool GameEngine::moveFieldCard(PlayerPtr from, PlayerPtr to, CardPtr card) {
    if(!canMoveFieldCard(from,to,card))return false;
    if(from->hasEquipment(card)) {
        from->removeEquipment(card->getSubType());
        to->equip(card);
        afterEquipmentLost(from,card);
    }else {from->removeJudgeCard(card);to->addJudgeCard(card);}
    notifyCardLostOutsideTurn(from,card);
    notifyAnyCardLost(from,card);
    logMessage("  ["+from->getName()+"] 的 "+card->getFormattedName()+" 移至 ["+to->getName()+"] 的场上");
    return true;
}

std::vector<CardPtr> GameEngine::chooseCards(PlayerPtr chooser, std::vector<CardPtr> candidates,
                                             int count, const std::string& prompt) {
    std::vector<CardPtr> picked;
    if (!chooser || count < 0 || static_cast<int>(candidates.size()) < count) return picked;
    for (int i=0;i<count;i++) {
        auto choice=askChooseCard(chooser,candidates,prompt+"（"+std::to_string(i+1)+"/"+
                std::to_string(count)+"）",false,AIController::chooseLeastValuableCard(candidates));
        if (!choice) return {};
        picked.push_back(choice);
        candidates.erase(std::remove(candidates.begin(),candidates.end(),choice),candidates.end());
    }
    return picked;
}

void GameEngine::swapHands(PlayerPtr a, PlayerPtr b) {
    if (!a || !b || a==b) return;
    auto ah=a->getHandCards(),bh=b->getHandCards();
    for (auto c:ah)a->removeHandCard(c);
    for (auto c:bh)b->removeHandCard(c);
    a->addHandCards(bh);b->addHandCards(ah);
    notifyHandEmpty(a,static_cast<int>(ah.size()));
    notifyHandEmpty(b,static_cast<int>(bh.size()));
    for(auto card:ah){notifyHandCardLostToOther(a,b);notifyCardLostOutsideTurn(a,card);notifyAnyCardLost(a,card);}
    for(auto card:bh){notifyHandCardLostToOther(b,a);notifyCardLostOutsideTurn(b,card);notifyAnyCardLost(b,card);}
    if(!ah.empty())notifyCardsLostBatch(a,static_cast<int>(ah.size()),b);
    if(!bh.empty())notifyCardsLostBatch(b,static_cast<int>(bh.size()),a);
}

void GameEngine::obtainCard(PlayerPtr to, CardPtr card, PlayerPtr from) {
    if (!to || !card) return;
    bool lostEquipment = false;
    int fromHandBefore = from ? from->getHandCardCount() : 0;
    bool fromLostHand = false;
    if (from) {
        fromLostHand = from->hasHandCard(card);
        if(std::find(from->getJudgeZone().begin(),from->getJudgeZone().end(),card)!=from->getJudgeZone().end())
            delayedTrickSources.erase(card.get());
        if (!from->removeHandCard(card)) {
            if (from->hasEquipment(card)) {
                from->removeEquipment(card->getSubType());
                lostEquipment = true;
            } else {
                from->removeJudgeCard(card);
            }
        }
        if (fromLostHand) {
            logMessage("  [" + to->getName() + "] 获得了 [" + from->getName() + "] 的一张手牌。");
            viewCards(to,{card},"你获得的手牌（仅你可见）");
        } else {
            logMessage("  [" + to->getName() + "] 获得了 [" + from->getName() + "] 的 " + card->getFormattedName());
        }
    } else {
        logMessage("  [" + to->getName() + "] 获得了 " + card->getFormattedName());
    }
    to->addHandCard(card);
    for (auto& holder : getAlivePlayers())
        for (auto& skill : getEffectiveSkills(*holder)) {
            skill->onAnyCardsObtained(*this, *holder, *to, 1);
            skill->onAnyCardObtained(*this, *holder, *to, card);
        }
    if (lostEquipment) afterEquipmentLost(from, card);
    if (from && fromLostHand) {
        notifyHandEmpty(from, fromHandBefore);
        notifyHandCardLostToOther(from,to);
    }
    if(from){notifyCardLostOutsideTurn(from,card);notifyAnyCardLost(from,card);}
}

void GameEngine::recoverHp(PlayerPtr player, int amount, const std::string& reason, PlayerPtr source) {
    if (!player || amount <= 0 || !player->isWounded()) return;
    for (const auto& s : getEffectiveSkills(*player)) {
        s->onCalculateRecover(*this, *player, source.get(), amount, reason);
    }
    if (amount <= 0) return;
    int oldHp=player->getHp();
    player->changeHp(amount);
    for (auto sk : getEffectiveSkills(*player)) sk->onHpChanged(*this, *player, amount);
    for(auto skill:getEffectiveSkills(*player))
        skill->onAfterRecover(*this,*player,player->getHp()-oldHp);
    std::string why = reason.empty() ? "" : ("（" + reason + "）");
    logMessage("  [" + player->getName() + "] 回复了 " + std::to_string(amount) + " 点体力" + why + "，当前体力: " +
               std::to_string(player->getHp()) + "/" + std::to_string(player->getMaxHp()));
}

void GameEngine::loseHp(PlayerPtr player, int amount, const std::string& reason, PlayerPtr source) {
    if (!player || amount <= 0 || !player->isAlive()) return;
    std::string why = reason.empty() ? "" : ("（" + reason + "）");
    logMessage("  [" + player->getName() + "] 失去了 " + std::to_string(amount) + " 点体力" + why + "，体力: " +
               std::to_string(player->getHp() - amount) + "/" + std::to_string(player->getMaxHp()));
    player->changeHp(-amount);
    for (auto sk : getEffectiveSkills(*player)) sk->onHpChanged(*this, *player, -amount);
    for (auto skill : getEffectiveSkills(*player))
        skill->onLoseHp(*this, *player, amount);
    if (player->getHp() <= 0) {
        processDying(player, source);
    }
}

Suit GameEngine::effectiveSuit(const Player& owner, CardPtr card) const {
    if (!card) return Suit::NONE;
    // 红颜只改变黑桃手牌的使用/打出/弃置/交付以及判定牌；装备区与判定区
    // 放置的实体牌并不自动变成红桃。判定生效牌尚未置入持有者区域。
    if (card->getSuit() == Suit::SPADE && owner.getHero() &&
        (owner.getHero()->findSkill("红颜") || owner.getHero()->findSkill("谋-红颜")) && !owner.hasEquipment(card) &&
        std::find(owner.getJudgeZone().begin(),owner.getJudgeZone().end(),card)==owner.getJudgeZone().end())
        return Suit::HEART;
    return card->getSuit();
}

CardPtr GameEngine::doJudgement(PlayerPtr target, const std::string& reason, bool toDiscard,
                                bool* claimedOut) {
    CardPtr judgeCard = deck.drawCard();
    if (!judgeCard) return nullptr;
    std::string previousJudgeReason = judgeReason;
    judgeReason = reason; // 改判技能（鬼道等）可据此识别当前判定的来源

    logMessage("  [" + target->getName() + "] 为【" + reason + "】进行判定，判定牌为: " + judgeCard->getFormattedName());

    // 判定牌生效前（如【鬼才】可替换判定牌）
    for (const auto& p : players) {
        if (!p->isAlive()) continue;
        for (const auto& s : getEffectiveSkills(*p)) {
            CardPtr before = judgeCard;
            s->onBeforeJudge(*this, *p, *target, judgeCard);
            if (judgeCard != before) {
                if (before) {
                    if (s->getName()=="鬼道") p->addHandCard(before);
                    else deck.discardCard(before);
                }
                logMessage("  [" + p->getName() + "] 发动【" + s->getName() + "】将判定牌替换为: " +
                           (judgeCard ? judgeCard->getFormattedName() : "（空）"));
            }
        }
    }

    // 判定牌生效后（如【天妒】可获得判定牌）
    bool claimed = false;
    for (const auto& s : getEffectiveSkills(*target)) {
        s->onAfterJudge(*this, *target, *target, judgeCard, claimed);
    }
    for (auto& p : getOtherAlivePlayers(*target))
        for (auto& s : getEffectiveSkills(*p)) s->onAfterJudge(*this,*p,*target,judgeCard,claimed);
    if(claimedOut)*claimedOut=claimed;
    if (toDiscard && !claimed && judgeCard) deck.discardCard(judgeCard);
    judgeReason = previousJudgeReason; // 恢复外层判定原因（判定不嵌套时为空）
    return judgeCard;
}

namespace {

// 观察局势：公开标记中属于“技能状态/资源”的部分（显式列举，避免把内部计数器当公开信息）。
// 蓄力技相关（蓄力/上限）单独成行展示。
bool isPublicMarkName(const std::string& name) {
    static const std::set<std::string> kPublic = {
        "护甲", "忍", "暴怒", "椎", "烈", "业", "奇", "策", "眩", "妆", "威",
        "仁望", "助", "任清", "天香", "荆", "挟志上限超", "椎数", "储君", "身份已明置",
    };
    if (kPublic.count(name)) return true;
    // 技能状态：限定技/觉醒技等“已用/已发动/已觉醒”类标记（发动本身是公开信息）。
    static const std::vector<std::string> kSuffixes = {"已用", "已发动", "已觉醒", "已献", "已挑战"};
    for (const auto& suf : kSuffixes) {
        if (name.size() > suf.size() && name.compare(name.size() - suf.size(), suf.size(), suf) == 0)
            return true;
    }
    return false;
}

} // namespace

// 该武将是否拥有蓄力技（用于展示蓄力点；官网文本以“蓄力技（x/y）”标注）。
static bool hasChargeSkill(const Hero& hero) {
    for (const auto& s : hero.getSkills()) {
        if (s->getDescription().find("蓄力技") != std::string::npos) return true;
    }
    return false;
}

std::string GameEngine::describePublicState(const Player& p) const {
    std::ostringstream ss;
    // 场上的牌：判定区（延时锦囊）
    ss << "     判定区: ";
    if (p.getJudgeZone().empty()) ss << "（无）";
    for (const auto& c : p.getJudgeZone()) ss << "[" << c->getFormattedName() << "] ";
    ss << "\n";
    // 场上的牌：武将牌上的牌（技能移出的牌，公开）
    if (!p.getPileNames().empty()) {
        ss << "     武将牌上: ";
        for (const auto& name : p.getPileNames()) {
            ss << name << ":";
            const auto& cards = p.getPile(name);
            for (const auto& c : cards) ss << c->getFormattedName() << " ";
            ss << " ";
        }
        ss << "\n";
    }
    // 公开状态：蓄力点/技能状态/公开标记
    std::ostringstream st;
    bool any = false;
    bool charge = false;
    if (p.getHero() && hasChargeSkill(*p.getHero())) {
        int cap = p.getMark("蓄力上限");
        int cur = p.getMark("蓄力");
        st << "蓄力 " << cur << "/" << cap;
        any = true; charge = true;
    }
    for (const auto& kv : p.getMarks()) {
        if (kv.second == 0 || kv.first == "蓄力" || kv.first == "蓄力上限") continue;
        if (!isPublicMarkName(kv.first)) continue;
        if (any) st << " | ";
        if (kv.first == "护甲") st << "护甲 " << kv.second;
        else st << kv.first << " " << kv.second;
        any = true;
    }
    (void)charge;
    if (any) ss << "     公开状态: " << st.str() << "\n";
    return ss.str();
}

std::string GameEngine::describePrivateState(const Player& p) const {
    std::ostringstream ss;
    ss << "     手牌: " << p.getFormattedHandCards() << "\n";
    return ss.str();
}

void GameEngine::printGameState() const {
    log() << "\n--------------------- 场上当前局势 ---------------------" << std::endl;
    for (const auto& p : players) {
        if (!p->isAlive()) {
            log() << " [阵亡] " + p->getName() + " (" + p->getIdentityString() + ")" << std::endl;
            continue;
        }
        bool isCurrent = (p->getId() == getCurrentPlayer()->getId());
        std::string prefix = isCurrent ? "▶ " : "  ";
        std::string statusStr = p->getFormattedStatus(isIdentityPublic(*p));
        std::string equipStr = p->getFormattedEquipment();

        log() << prefix << statusStr << std::endl;
        log() << "     装备: " << equipStr << std::endl;
        log() << describePublicState(*p);
        // 私有状态（手牌等）：仅玩家本人可见（明牌/加倍已删去）
        if (!p->isAI()) log() << describePrivateState(*p);
    }
    log() << "牌堆剩余: " << deck.getDrawPileSize() << " 张 | 弃牌堆: " << deck.getDiscardPileSize() << " 张" << std::endl;
    log() << "--------------------------------------------------------\n" << std::endl;
}

// ==================== 回合流程 ====================

bool GameEngine::pindian(PlayerPtr a, PlayerPtr b, const std::string& reason,
                         std::vector<CardPtr>* revealed) {
    if (!a || !b || a == b || !a->isAlive() || !b->isAlive() ||
        a->getHandCards().empty() || b->getHandCards().empty()) return false;
    auto choose = [&](PlayerPtr p) {
        const auto& hand = p->getHandCards();
        CardPtr high = *std::max_element(hand.begin(), hand.end(),
            [](CardPtr x, CardPtr y) { return x->getRank() < y->getRank(); });
        // B4 拼点策略：双方都想赢，所以默认出最大的牌；但**别把救命牌（桃级）拿去拼**——
        // 若存在点数只差 2 以内、价值明显更低的牌，就用那张（几乎不影响胜率，省下关键牌）。
        CardPtr pick = high;
        if (p->isAI() && AIController::cardValue(*this, *p, high) >= 9) {
            for (const auto& c : hand) {
                if (!c || c->getRank() < high->getRank() - 2) continue;
                if (AIController::cardValue(*this, *p, c) < AIController::cardValue(*this, *p, pick)) pick = c;
            }
        }
        return askChooseCard(p, hand, "【" + reason + "】选择拼点手牌", false, pick);
    };
    CardPtr ca = choose(a), cb = choose(b);
    if (!ca || !cb) return false;
    a->removeHandCard(ca); b->removeHandCard(cb);
    if (revealed) {revealed->push_back(ca);revealed->push_back(cb);}
    deck.discardCard(ca); deck.discardCard(cb);
    notifyHandEmpty(a, 1); notifyHandEmpty(b, 1);
    notifyCardLostOutsideTurn(a,ca);notifyCardLostOutsideTurn(b,cb);
    logMessage("  【" + reason + "】拼点：" + a->getName() + " " + std::to_string(ca->getRank()) +
               " / " + b->getName() + " " + std::to_string(cb->getRank()));
    return ca->getRank() > cb->getRank();
}

void GameEngine::broadcastRoundStart() {
    static const bool roundTrace = std::getenv("THKS_AI_TRACE") != nullptr;
    for (auto& p : players) {
        if (!p->isAlive()) continue;
        for (const auto& s : getEffectiveSkills(*p)) {
            if (roundTrace)
                std::fprintf(stderr, "[轮次开始] 第%d轮 %s 的【%s】onRoundStart\n", roundCount,
                             p->getName().c_str(), s->getName().c_str());
            s->onRoundStart(*this, *p);
        }
    }
}

void GameEngine::startGame() {
    logMessage("游戏正式开始！");
    // 用户 2026-10-05 要求：**游戏开始时也等同于首轮开始时**。
    // 轮次从开局这一刻起算（roundCount=1 并广播 onRoundStart），轮次起点是**开局的当前回合角色**
    // （主公先动时可能不是座位 0——旧实现等回合轮到座位 0 才算第 1 轮，会让首轮技能与
    // “第一轮”限定（如【立储】、友-演策）延后甚至错过）。
    if (roundCount < 1) {
        roundStartSeat = currentTurnIndex;
        roundCount = 1;
        roundHadDeath = false;
        broadcastRoundStart();
    }
    int safety = 0;
    bool firstTurn = true;
    while (!gameOver && safety++ < 2000) {
        // 新一轮开始：回合重新回到本轮起点座位
        if (!firstTurn && currentTurnIndex == roundStartSeat) {
            roundCount++;
            roundHadDeath = false;
            logMessage("\n>>> ===== 第 " + std::to_string(roundCount) + " 轮 ===== <<<");
            broadcastRoundStart();
        }
        firstTurn = false;
        // F4 僵局防线：连续 kStalemateTurnLimit 个回合没有任何伤害/死亡（例如华佗青囊自守 vs 曹操奸雄，
        // 双方都打不死对方）→ 判平局并输出诊断，避免打到 2000 回合上限（实测会产生 5 万行日志）。
        ++turnsWithoutDamage;
        ++turnsWithoutDeath;
        // 两条判据任一成立即判僵局：①长时间无人受伤；②长时间无人阵亡（奶妈自守 vs 奸雄型对局，
        // 双方都在回血/摸牌，伤害一直有但局面不推进）。
        if (turnsWithoutDamage > kStalemateTurnLimit || turnsWithoutDeath > kStalemateDeathLimit) {
            gameOver = true;
            winningFaction = "平局（连续 " + std::to_string(turnsWithoutDeath) + " 个回合无阵亡、" +
                             std::to_string(turnsWithoutDamage) + " 个回合无伤害，判定为僵局）";
            logMessage("  [诊断] 僵局：存活 " + std::to_string(getAlivePlayers().size()) +
                       " 人，连续 " + std::to_string(turnsWithoutDeath) + " 回合无阵亡 / " +
                       std::to_string(turnsWithoutDamage) + " 回合无伤害");
            for (const auto& p : getAlivePlayers())
                logMessage("    " + p->getName() + "（" + p->getIdentityString() + " HP " + std::to_string(p->getHp()) +
                           "/" + std::to_string(p->getMaxHp()) + " 手牌 " + std::to_string(p->getHandCardCount()) +
                           " 阵营估计 " + std::to_string(aiSideEstimate(*p)) + "）");
            break;
        }
        PlayerPtr cur = getCurrentPlayer();
        static const bool loopTrace = std::getenv("THKS_AI_TRACE") != nullptr;
        if (loopTrace)
            std::fprintf(stderr, "[主循环] round=%d turn=%s alive=%zu safety=%d\n", roundCount,
                         cur ? cur->getName().c_str() : "(null)", getAlivePlayers().size(), safety);
        if (cur->isAlive()) {
            runTurn(cur);
        }
        if (gameOver) break;

        currentTurnIndex = (currentTurnIndex + 1) % players.size();
    }
    if (!gameOver) {
        winningFaction = "平局（回合数达到上限）";
        // F4 挂起防线：把当时的局面打出来，便于定位“谁也不出手”的僵局
        logMessage("  [诊断] 达到回合上限：存活 " + std::to_string(getAlivePlayers().size()) + " 人，各人阵营估计如下");
        for (const auto& p : getAlivePlayers())
            logMessage("    " + p->getName() + "（" + p->getIdentityString() + " HP " + std::to_string(p->getHp()) +
                       "/" + std::to_string(p->getMaxHp()) + " 手牌 " + std::to_string(p->getHandCardCount()) +
                       " 阵营估计 " + std::to_string(aiSideEstimate(*p)) + "）");
    }

    logMessage("\n========================================================");
    logMessage("【游戏结束】胜利阵营: " + winningFaction);
    for (const auto& p : players) {
        logMessage("  " + p->getFormattedStatus(true));
    }
    logMessage("========================================================\n");
}

void GameEngine::runTurn(PlayerPtr player) {
    printGameState();
    logMessage("\n>>> 【" + player->getName() + "】的回合开始 <<<");

    player->resetShaCount();
    // “本回合已使用的牌类型”按当前全局回合清零，供拥有恃才等技能的角色判定。
    for (auto& p : players) p->clearCardTypesUsedThisTurn();
    // 次数标记属于回合/行动者，而不属于技能对象；委托技能的赠与次数于其回合开始重置。
    player->addMark("黄天已献",-player->getMark("黄天已献"));
    player->addMark("制霸已挑战",-player->getMark("制霸已挑战"));
    if (player->getHero()) {
        for (const auto& s : player->getHero()->getSkills()) s->resetTurnState();
    }
    currentPhase = TurnPhase::PREPARATION;
    for (auto& p : players)
        if (p->getMark("本回合用过杀") > 0)
            p->addMark("本回合用过杀", -p->getMark("本回合用过杀"));
    // 所有仍在场上的持续效果先到期，包括此前暂时失效的技能留下的状态。
    for(auto& p:players)if(p->isAlive() && p->getHero()) {
        auto skills=p->getHero()->getSkills();
        for(auto& s:skills)s->onTurnBoundary(*this,*p,*player,true);
    }
    // 回合开始：广播给所有存活角色（"每回合一次"类计数在此重置）
    for (auto& p : players) {
        if (gameOver) break;
        if (!p->isAlive()) continue;
        for (const auto& s : getEffectiveSkills(*p)) {
            if (gameOver) break;
            s->onTurnStart(*this, *p);
        }
    }

    if (player->isTurnedOver()) {
        logMessage("[" + player->getName() + "] 处于翻面状态，本回合跳过，翻回正面。");
        player->setTurnedOver(false);
    } else {
        runTurnPhases(player);
    }

    // 回合结束时的费用仍是本回合内的失牌（例如放权），不能提前置为 NONE
    // 而误触发【屯田】等“回合外失牌”技能。
    for (auto& p : players) {
        if (gameOver) break;
        if (!p->isAlive()) continue;
        for (auto& s : getEffectiveSkills(*p)) {
            if (gameOver) break;
            s->onTurnEnd(*this, *p, *player);
        }
    }
    resolveXieLiAtTurnEnd(player);
    for(auto& p:players)if(p->isAlive() && p->getHero()) {
        auto skills=p->getHero()->getSkills();
        for(auto& s:skills)s->onTurnBoundary(*this,*p,*player,false);
    }
    currentPhase = TurnPhase::NONE;
    for (auto& p : players) p->clearTurnEffects();
    while (!scheduledExtraTurns.empty() && !gameOver && extraTurnDepth < 8) {
        auto extra=scheduledExtraTurns.front();scheduledExtraTurns.pop_front();
        if (!extra->isAlive())continue;
        ++extraTurnDepth;
        int oldIndex=currentTurnIndex;
        currentTurnIndex=extra->getId();
        logMessage("  [" + extra->getName() + "] 获得一个额外回合！");
        runTurn(extra);
        currentTurnIndex=oldIndex;
        --extraTurnDepth;
    }
}

void GameEngine::notifyPhaseSkipped(PlayerPtr player, TurnPhase phase) {
    // 跳过阶段是全场事件；摸零张牌不等于跳过摸牌阶段。
    for (auto p : getAlivePlayers()) {
        for (auto skill : getEffectiveSkills(*p))
            skill->onPhaseSkipped(*this, *p, *player, phase);
    }
}

void GameEngine::runTurnPhases(PlayerPtr player) {
    bool skipPreparation = false;
    bool skipPlay = false;
    bool skipDraw = false;

    currentPhase = TurnPhase::PREPARATION;
    phasePreparation(player, skipPreparation);
    if (gameOver || !player->isAlive()) return;
    if (skipPreparation) {
        logMessage("["+player->getName()+"] 跳过了准备阶段！");
        notifyPhaseSkipped(player,TurnPhase::PREPARATION);
    }

    currentPhase = TurnPhase::JUDGEMENT;
    bool skipJudgement = false;
    for (auto& s : getEffectiveSkills(*player)) s->onPhaseStart(*this,*player,TurnPhase::JUDGEMENT,skipJudgement);
    bool speedy=false;
    for(auto s:getEffectiveSkills(*player))if(s->getName()=="神速")speedy=true;
    bool speedOne=speedy && askConfirm(player,"【神速】跳过判定和摸牌阶段，视为使用无距离限制的杀？",false);
    if(speedOne){skipDraw=true;playSkillSha(player,"神速");}
    if (speedOne || skipJudgement) notifyPhaseSkipped(player,TurnPhase::JUDGEMENT);
    else phaseJudgement(player,skipPlay,skipDraw);
    if (gameOver || !player->isAlive()) return;

    currentPhase = TurnPhase::DRAW;
    for (auto& s : getEffectiveSkills(*player)) s->onPhaseStart(*this,*player,TurnPhase::DRAW,skipDraw);
    if (!skipDraw) {
        phaseDraw(player);
    } else {
        logMessage("[" + player->getName() + "] 跳过了摸牌阶段！");
        notifyPhaseSkipped(player,TurnPhase::DRAW);
    }
    if (gameOver || !player->isAlive()) return;

    currentPhase = TurnPhase::PLAY;
    // 进入出牌阶段：向全体广播（势·诫节等“该角色出牌阶段”类技能）
    for (auto& pp : getAlivePlayers())
        for (auto sk : getEffectiveSkills(*pp))
            sk->onAnyPhaseStart(*this, *pp, *player, TurnPhase::PLAY);
    if (!skipPlay && speedy) {
        std::vector<CardPtr> equipment;
        for(auto c:player->getAllCards())if(c->getType()==CardType::EQUIPMENT)equipment.push_back(c);
        if(!equipment.empty() && askConfirm(player,"【神速】弃置一张装备牌并跳过出牌阶段，视为使用无距离限制的杀？",false)) {
            auto c=askChooseCard(player,equipment,"【神速】弃置一张装备牌",false,
                                 AIController::chooseLeastValuableCard(equipment));
            if(c){discardCardOf(player,c,"神速");skipPlay=true;playSkillSha(player,"神速");}
        }
    }
    bool _extraPlay = false;
    do {
        _extraPlay = false;
        if (!skipPlay) {
            // 出牌阶段开始：广播 onPhaseStart(PLAY)（"出牌阶段开始时"类效果）
            for (const auto& s : getEffectiveSkills(*player)) s->onPhaseStart(*this, *player, TurnPhase::PLAY, skipPlay);
        }
        if (!skipPlay) {
            phasePlay(player);
            if (gameOver || !player->isAlive()) return;
            // 出牌阶段结束：广播给所有存活角色（"一名玩家的出牌阶段结束时"类触发）
            for (auto& p : players) {
                if (!p->isAlive()) continue;
                for (const auto& s : getEffectiveSkills(*p)) s->onPhaseEnd(*this, *p, TurnPhase::PLAY);
            }
            // 额外出牌阶段（如谋·大乔【流离】标记角色回合开始执行）
            if (!gameOver && player->isAlive() && player->getMark("流离额外出牌阶段") > 0) {
                player->addMark("流离额外出牌阶段", -player->getMark("流离额外出牌阶段"));
                logMessage("[" + player->getName() + "] 执行一个额外的出牌阶段！");
                _extraPlay = true;
                // 额外出牌阶段 = 新的出牌阶段：阶段级限次技能可再发动（回合级杀次数等不重置）
                resetPlayPhaseUses(player);
            }
        } else {
            logMessage("[" + player->getName() + "] 跳过了出牌阶段！");
            notifyPhaseSkipped(player,TurnPhase::PLAY);
        }
    } while (_extraPlay);
    if (gameOver || !player->isAlive()) return;

    currentPhase = TurnPhase::DISCARD;
    phaseDiscard(player);
    if (gameOver || !player->isAlive()) return;

    currentPhase = TurnPhase::FINISH;
    phaseFinish(player);
}

void GameEngine::phasePreparation(PlayerPtr player, bool& skipTurn) {
    // 准备阶段开始时统一确定可触发技能；阶段中才获得的技能（魂姿→英魂、
    // 志继→观星）不能回溯本次阶段开始的时机。
    for (const auto& skill : getEffectiveSkills(*player)) {
        if(skipTurn)break;
        skill->onPhaseStart(*this, *player, TurnPhase::PREPARATION, skipTurn);
    }
}

void GameEngine::phaseJudgement(PlayerPtr player, bool& skipPlay, bool& skipDraw) {
    const auto& judgeCards = player->getJudgeZone();
    if (judgeCards.empty()) return;

    logMessage("[" + player->getName() + "] 进入判定阶段，有 " + std::to_string(judgeCards.size()) + " 张延时锦囊。");

    std::vector<CardPtr> copyZone = judgeCards;
    for (int i = static_cast<int>(copyZone.size()) - 1; i >= 0; --i) {
        CardPtr card = copyZone[i];
        player->removeJudgeCard(card);
        auto it=delayedTrickSources.find(card.get());
        PlayerPtr origin=it!=delayedTrickSources.end()?getPlayerById(it->second):nullptr;
        if(!origin || !origin->isAlive())origin=player;
        if(askNullification(origin,player,card)) {
            logMessage("  【"+card->getName()+"】在判定阶段被【无懈可击】抵消。");
            deck.discardCards(card->getRealCards(card));
            delayedTrickSources.erase(card.get());
            continue;
        }
        if (card->getSubType()==CardSubType::LE_BU_SI_SHU ||
            card->getSubType()==CardSubType::BING_LIANG_CUN_DUAN)
            delayedTrickSources.erase(card.get());

        if (card->getSubType() == CardSubType::LE_BU_SI_SHU) {
            logMessage("[" + player->getName() + "] 进行【乐不思蜀】判定...");
            CardPtr res = doJudgement(player, "乐不思蜀");
            if (res && effectiveSuit(*player, res) != Suit::HEART) {
                logMessage("判定结果非红桃！【" + player->getName() + "】本回合跳过出牌阶段！");
                skipPlay = true;
            } else {
                logMessage("判定成功为红桃！【乐不思蜀】失效。");
            }
            deck.discardCards(card->getRealCards(card));
        } else if (card->getSubType() == CardSubType::BING_LIANG_CUN_DUAN) {
            logMessage("[" + player->getName() + "] 进行【兵粮寸断】判定...");
            CardPtr res = doJudgement(player, "兵粮寸断");
            if (res && effectiveSuit(*player, res) != Suit::CLUB) {
                logMessage("判定结果非草花！【" + player->getName() + "】本回合跳过摸牌阶段！");
                skipDraw = true;
            } else {
                logMessage("判定成功为草花！【兵粮寸断】失效。");
            }
            deck.discardCards(card->getRealCards(card));
        } else if (card->getSubType() == CardSubType::SHAN_DIAN) {
            logMessage("[" + player->getName() + "] 进行【闪电】判定...");
            CardPtr res = doJudgement(player, "闪电");
            if (res && effectiveSuit(*player, res) == Suit::SPADE && res->getRank() >= 2 && res->getRank() <= 9) {
                logMessage("判定结果为黑桃 2-9！【闪电】命中！受 3 点雷电伤害！");
                deck.discardCards(card->getRealCards(card));
                delayedTrickSources.erase(card.get());
                applyDamage(nullptr, player, 3, ShaElement::THUNDER);
            } else {
                logMessage("【闪电】未命中，移至下一名角色的判定区。");
                auto alive = getAlivePlayers();
                if(alive.size()<=1) {
                    deck.discardCards(card->getRealCards(card));
                    delayedTrickSources.erase(card.get());
                } else for (size_t k = 0; k < alive.size(); ++k) {
                    if (alive[k]->getId() == player->getId()) {
                        auto nextPlayer = alive[(k + 1) % alive.size()];
                        nextPlayer->addJudgeCard(card);
                        break;
                    }
                }
            }
        }
    }
}

void GameEngine::phaseDraw(PlayerPtr player) {
    int drawCount = 2;

    for (const auto& skill : getEffectiveSkills(*player)) {
        skill->onDrawCards(*this, *player, drawCount);
    }

    drawCards(player, drawCount);
    for (auto& s : getEffectiveSkills(*player)) s->onPhaseEnd(*this,*player,TurnPhase::DRAW);
}

void GameEngine::phasePlay(PlayerPtr player) {
    if (player->isAI()) {
        aiPlayLoop(player);
    } else {
        humanPlayLoop(player);
    }
}

void GameEngine::aiPlayLoop(PlayerPtr player) {
    int safeLoop = 0;
    // 可选决策跟踪（docs/ai_todo.md F1）：THKS_AI_TRACE=1 时把每次 AI 出牌迭代打到 stderr。
    static const bool aiTrace = std::getenv("THKS_AI_TRACE") != nullptr;
    while (safeLoop++ < 40 && player->isAlive() && !gameOver) {
        if (aiTrace)
            std::fprintf(stderr, "[AI出牌] %s iter=%d hand=%d phase=%d\n", player->getName().c_str(),
                         safeLoop, player->getHandCardCount(), static_cast<int>(currentPhase));
        // 1. 主动技
        bool acted = false;
        for (const auto& s : getEffectiveSkills(*player)) {
            if (!s->isUsableActively()) continue;
            // 中央兜底：限次主动技用完次数后直接跳过（即使某技能的 canActivate 忘记检查次数）
            if (auto active = std::dynamic_pointer_cast<ActiveSkill>(s)) {
                if (!active->hasUsesLeft()) continue;
            }
            if (s->canActivate(*this, *player) && s->aiShouldActivate(*this, *player)) {
                if (aiTrace) std::fprintf(stderr, "[AI出牌]   发动主动技【%s】\n", s->getName().c_str());
                int before = player->getHandCardCount();
                int quanBefore = player->getPileCount("权");
                s->activate(*this, *player);
                // 若技能未产生任何变化（例如 AI 放弃），避免死循环
                auto active = std::dynamic_pointer_cast<ActiveSkill>(s);
                bool unchanged = before == player->getHandCardCount() && quanBefore == player->getPileCount("权");
                if (unchanged && (!active || active->getUsesThisTurn() == 0)) {
                    continue;
                }
                acted = true;
                break;
            }
        }
        if (acted) {
            if (aiDelayMs > 0) Platform::sleepMs(aiDelayMs);
            continue;
        }

        if (aiTrace) std::fprintf(stderr, "[AI出牌]   主动技扫描结束 acted=%d\n", (int)acted);
        // 主公技按行动者的出牌阶段计次，而不是在主公的回合开始时偷跑。
        bool given=false;
        for(auto owner:getOtherAlivePlayers(*player)) {
            if(!AIController::isFriend(*this, *player,*owner))continue;
            for(auto skill:getEffectiveSkills(*owner))if(skill->canDelegate(*this,*owner,*player)) {
                int before=player->getHandCardCount();
                skill->invokeDelegated(*this,*owner,*player);
                if(player->getHandCardCount()!=before){given=true;break;}
            }
            if(given)break;
        }
        if(given)continue;
        if (aiTrace) std::fprintf(stderr, "[AI出牌]   委托结束 given=%d，开始决策\n", (int)given);
        // 2. 出牌
        auto decision = AIController::makePlayDecision(*this, *player);
        if (aiTrace)
            std::fprintf(stderr, "[AI出牌]   决策=%s 目标=%zu，开始结算\n",
                         decision.cardToPlay ? decision.cardToPlay->getName().c_str() : "(无)",
                         decision.targets.size());
        if (!decision.cardToPlay) {
            break;
        }
        // 无效目标等拒绝结算时结束本次出牌决策，避免 AI 反复尝试同一张牌。
        if(!useCard(player, decision.cardToPlay, decision.targets))break;
        if (aiTrace) std::fprintf(stderr, "[AI出牌]   结算完成\n");
        if (aiDelayMs > 0) Platform::sleepMs(aiDelayMs);
    }
}

// 出牌阶段提示（含当前手牌/装备/可用技能）；插入式面板（图鉴/记牌器/局势）返回后
// 由 readTokenWithCodex 回调重印本提示（TODO 第 3 项：恢复状态并重新输出问题）。
void GameEngine::printPlayPhasePrompt(PlayerPtr player) {
    log() << "\n========================================================\n";
    log() << "【你的武将】: " << (player->getHero() ? player->getHero()->getName() : "无") << "  HP "
              << player->getHp() << "/" << player->getMaxHp()
              << "  本回合已用【杀】" << player->getShaCountThisTurn() << "/" << (getShaLimit(*player) >= 999 ? std::string("∞") : std::to_string(getShaLimit(*player))) << "\n";
    log() << "【你的手牌】: " << player->getFormattedHandCards() << "\n";
    log() << "【你的装备】: " << player->getFormattedEquipment() << "\n";

    std::vector<SkillPtr> actives;
    for (const auto& s : getEffectiveSkills(*player)) {
        if (s->isUsableActively()) actives.push_back(s);
    }
    std::vector<std::pair<SkillPtr,PlayerPtr>> delegated;
    for(auto owner:getOtherAlivePlayers(*player))
        for(auto skill:getEffectiveSkills(*owner))
            if(skill->canDelegate(*this,*owner,*player))delegated.emplace_back(skill,owner);
    if (!actives.empty() || !delegated.empty()) {
        log() << "【可用技能】: ";
        for (size_t i = 0; i < actives.size(); ++i) {
            log() << "[s" << (i + 1) << "]【" << actives[i]->getName() << "】";
            if (actives[i]->isConversionSkill()) log() << "(技能会筛出此刻可用的转化)";
            else if (!actives[i]->canActivate(*this, *player)) log() << "(暂不可发动)";
            log() << " ";
        }
        for(size_t j=0;j<delegated.size();j++)
            log() << "[s" << (actives.size()+j+1) << "]【" << delegated[j].first->getName()
                  << "】（" << delegated[j].second->getName() << "的主公技） ";
        log() << "\n";
    }
    if (hasWeapon(*player, "丈八蛇矛") && player->getHandCardCount() >= 2 && !player->isHandCardsBanned()) {
        log() << "【武器】: [z]【丈八蛇矛】将两张手牌当【杀】使用\n";
    }
    if (player->isHandCardsBanned()) {
        log() << "（你本回合不能使用或打出手牌！）\n";
    }
    log() << "输入手牌编号使用牌 | e+编号 转化装备区的牌 | s+编号 发动技能"
          << (hasWeapon(*player, "丈八蛇矛") && player->getHandCardCount() >= 2 && !player->isHandCardsBanned() ? " | z 丈八蛇矛" : "")
          << " | v 图鉴 | m 记牌器 | c 局势 | 0 结束出牌阶段: ";
}

void GameEngine::humanPlayLoop(PlayerPtr player) {
    while (player->isAlive() && !gameOver) {
        printPlayPhasePrompt(player);

        bool zhangBaReady = hasWeapon(*player, "丈八蛇矛") && player->getHandCardCount() >= 2 && !player->isHandCardsBanned();
        std::vector<SkillPtr> actives;
        for (const auto& s : getEffectiveSkills(*player)) {
            if (s->isUsableActively()) actives.push_back(s);
        }
        std::vector<std::pair<SkillPtr,PlayerPtr>> delegated;
        for(auto owner:getOtherAlivePlayers(*player))
            for(auto skill:getEffectiveSkills(*owner))
                if(skill->canDelegate(*this,*owner,*player))delegated.emplace_back(skill,owner);
        std::string heroId = player->getHero() ? player->getHero()->getId() : "";
        std::string token = ui->readTokenWithCodex(heroId, [this, player]() { printPlayPhasePrompt(player); });
        if (token == "0") {
            logMessage("你选择了结束出牌阶段。");
            break;
        }
        if (token == "z" || token == "Z") {
            if (!zhangBaReady) {
                log() << "当前不能发动【丈八蛇矛】！" << std::endl;
                continue;
            }
            if (player->getShaCountThisTurn() >= getShaLimit(*player)) {
                log() << "本回合使用【杀】的次数已达上限！" << std::endl;
                continue;
            }
            CardPtr sha = materializeZhangBaSha(player, Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "丈八蛇矛"));
            if (sha) humanUseChosenCard(player, sha);
            continue;
        }
        if (token == "v" || token == "V" || token == "?" || token == "t" || token == "T") {
            Codex::open(heroId); // 图鉴代替旧"查看技能说明"：先看自己，可再浏览全部
            continue;
        }
        if (!token.empty() && (token[0] == 's' || token[0] == 'S')) {
            int idx = Interaction::parseInt(token.substr(1));
            if (idx < 1 || idx > static_cast<int>(actives.size()+delegated.size())) {
                log() << "无效的技能编号！" << std::endl;
                continue;
            }
            if(idx>static_cast<int>(actives.size())) {
                auto [skill,owner]=delegated[idx-actives.size()-1];
                if(skill->canDelegate(*this,*owner,*player))skill->invokeDelegated(*this,*owner,*player);
                continue;
            }
            auto skill = actives[idx - 1];
            if (skill->isConversionSkill()) {
                // 转化类技能：技能自己筛出此刻适合使用的候选牌，玩家选定后使用；
                // 不选技能就不会发生任何转化（“你不用技能就不生效”）。
                humanUseConversionSkill(player, skill);
                continue;
            }
            if (!skill->canActivate(*this, *player)) {
                log() << "【" << skill->getName() << "】当前不能发动！" << std::endl;
                continue;
            }
            skill->activate(*this, *player);
            continue;
        }

        if(token.size()>1 && (token[0]=='e' || token[0]=='E')) {
            auto equips=player->getAllEquipment();
            int n=Interaction::parseInt(token.substr(1));
            if(n<1 || n>static_cast<int>(equips.size())) {
                log()<<"无效的装备编号（按武器、防具、进攻坐骑、防御坐骑顺序）！"<<std::endl;
            }else humanUseCard(player,equips[n-1]);
            continue;
        }
        int choice = Interaction::parseInt(token);
        if (choice < 1 || choice > player->getHandCardCount()) {
            log() << "无效的选项，请重新输入！" << std::endl;
            continue;
        }
        CardPtr card = player->getHandCards()[choice - 1];
        humanUseCard(player, card);
    }
}

bool GameEngine::humanUseCard(PlayerPtr player, CardPtr card) {
    if (player->isHandCardsBanned() && player->hasHandCard(card)) {
        log() << "你本回合不能使用或打出手牌！" << std::endl;
        return false;
    }

    // 1. 技能转化选项（如武圣：红色牌当【杀】；奇袭：黑色牌当过拆；国色/急救等）
    // 2026-10-06 重写：不再写死目标类型表，而是枚举该牌经所有转化类技能的全部结果，
    // 并用 canPlayCardNow 过滤出“此刻适合使用”的牌（技能只提供它筛选出的候选）。
    // 原牌“适合使用”也要过滤：canUseOriginalCard 只判禁用类限制（武神/无谋），
    // 具体时机（闪/无懈不能主动使用、满血不能用桃、杀要有目标）由 canPlayCardNow 判定。
    const bool originalUsable = player->hasHandCard(card) && canUseOriginalCard(*player, card) &&
                                canPlayCardNow(*player, card);
    std::vector<CardPtr> conv = getAllConversionsFor(player, card);
    conv.erase(std::remove_if(conv.begin(), conv.end(),
                              [&](const CardPtr& v) { return !canPlayCardNow(*player, v); }),
               conv.end());
    if (originalUsable && conv.empty()) return humanUseChosenCard(player, card);

    std::vector<CardPtr> options;
    std::vector<std::string> labels;
    if (originalUsable) {
        options.push_back(card);
        labels.push_back("按原牌使用 " + card->getFormattedName());
    }
    for (const auto& v : conv) {
        options.push_back(v);
        labels.push_back("发动【" + v->getSkillSource() + "】当作【" + v->getName() + "】使用");
    }
    if (options.empty()) {
        log() << "这张牌现在不能使用！" << std::endl;
        return false;
    }
    // 锁定类转化（武神/无谋：原牌被技能禁止直接使用）没有“不用技能”的选项，
    // 玩家选出这张牌即视为发动技能，不再重复询问；其余情况必须由玩家显式选择。
    const bool originalForbidden = player->hasHandCard(card) && !canUseOriginalCard(*player, card);
    if (options.size() == 1 && (originalUsable || originalForbidden))
        return humanUseChosenCard(player, options.front());
    int opt = askChooseOption(player, labels, "请选择使用方式");
    if (opt < 0 || opt >= static_cast<int>(options.size())) return false;
    return humanUseChosenCard(player, options[opt]);
}

// 出牌阶段主动发动转化类技能：由技能筛选出此刻适合使用的转化候选，玩家选定后使用。
bool GameEngine::humanUseConversionSkill(PlayerPtr player, SkillPtr skill) {
    if (!player || !skill) return false;
    std::vector<CardPtr> material;
    for (const auto& c : player->getHandCards()) material.push_back(c);
    if (!player->isHandCardsBanned())
        for (const auto& c : player->getAllEquipment()) material.push_back(c);
    std::vector<CardPtr> candidates;
    for (const auto& c : material)
        for (const auto& v : getSkillConversionsFor(skill, player, c))
            if (canPlayCardNow(*player, v)) candidates.push_back(v);
    if (candidates.empty()) {
        log() << "【" << skill->getName() << "】现在没有适合的牌。" << std::endl;
        return false;
    }
    CardPtr chosen = askChooseCard(player, candidates,
                                   "【" + skill->getName() + "】选择一张牌当作对应牌使用", true);
    if (!chosen) return false;
    return humanUseChosenCard(player, chosen);
}

bool GameEngine::humanUseChosenCard(PlayerPtr player, CardPtr chosen) {
    if (!player || !chosen) return false;
    chosen=materializeConversion(player,chosen);
    CardSubType st = chosen->getSubType();
    if (st == CardSubType::SHAN || st == CardSubType::WU_XIE_KE_JI) {
        log() << "这张牌不能在出牌阶段主动使用！" << std::endl;
        return false;
    }
    if (st == CardSubType::TAO && !player->isWounded()) {
        log() << "你的体力已满，不能使用【桃】！" << std::endl;
        return false;
    }

    std::vector<PlayerPtr> targets;
    if (st == CardSubType::SHA) {
        auto original=wumouMaterial(chosen);
        auto candidates = getShaTargets(*player, chosen);
        const bool skillIgnoresShaLimit = chosen->getSkillSource() == "莺舞" ||
                                           chosen->getSkillSource() == "势-战烈";
        if (isPlayerTurn(*player) && currentPhase == TurnPhase::PLAY &&
            player->getShaCountThisTurn() >= getShaLimitForCard(*player,chosen) &&
            !skillIgnoresShaLimit) {
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const PlayerPtr& target) {
                return !target || !canUseShaBeyondLimitOn(*player, *target, chosen);
            }), candidates.end());
        }
        if (candidates.empty()) {
            log() << (player->getShaCountThisTurn() >= getShaLimitForCard(*player,chosen) && !skillIgnoresShaLimit
                          ? "本回合使用【杀】的次数已达上限！"
                          : "攻击范围内没有可选的目标！") << std::endl;
            return false;
        }
        if(original && original->getSubType()==CardSubType::JIE_DAO_SHA_REN) {
            auto holders=getJieDaoWeaponHolders(*player);
            holders.erase(std::remove_if(holders.begin(), holders.end(), [&](const PlayerPtr& holder) {
                return !canBeTargeted(holder, original, player);
            }), holders.end());
            auto holder=askChoosePlayer(player,holders,"【无谋】选择借刀杀人的持武器目标",true);
            if(!holder)return false;
            auto victim=askChoosePlayer(player,getJieDaoVictims(*holder),
                                        "【无谋】选择借刀杀人的第二目标",true);
            return victim && useCard(player,chosen,{holder,victim});
        }
        if(original && wumouAllTargets(original->getSubType())) {
            targets=candidates; // 原群体锦囊的目标集，不是任选一名。
            return useCard(player,chosen,targets);
        }
        if(original && original->getSubType()==CardSubType::WU_ZHONG_SHENG_YOU) {
            targets={player};
            return useCard(player,chosen,targets);
        }
        PlayerPtr t = askChoosePlayer(player, candidates, "选择【杀】的目标", true);
        if (!t) return false;
        targets.push_back(t);
        // 技能与装备统一叠加目标数修正。
        int extra=original ? (original->getSubType()==CardSubType::TIE_SUO_LIAN_HUAN ? 1 : 0)
                           : getShaTargetLimit(*player,chosen)-1;
        std::vector<PlayerPtr> remaining;
        for(auto p:candidates)if(p!=t)remaining.push_back(p);
        for(int i=0;i<extra && !remaining.empty();i++) {
            auto next=askChoosePlayer(player,remaining,"可额外指定【杀】的目标",true);
            if(!next)break;
            targets.push_back(next);
            remaining.erase(std::remove(remaining.begin(),remaining.end(),next),remaining.end());
        }
    } else if (st == CardSubType::JIE_DAO_SHA_REN) {
        auto holders = getJieDaoWeaponHolders(*player);
        holders.erase(std::remove_if(holders.begin(), holders.end(), [&](const PlayerPtr& holder) {
            return !canBeTargeted(holder, chosen, player);
        }), holders.end());
        if (holders.empty()) {
            log() << "没有合法的目标（需要有武器、有合法杀目标且能成为【借刀杀人】目标）！" << std::endl;
            return false;
        }
        PlayerPtr holder = askChoosePlayer(player, holders, "选择【借刀杀人】的目标（装备区有武器的角色）", true);
        if (!holder) return false;
        PlayerPtr victim = askChoosePlayer(player, getJieDaoVictims(*holder),
                                           "选择要让 [" + holder->getName() + "] 使用【杀】攻击的角色", true);
        if (!victim) return false;
        targets.push_back(holder);
        targets.push_back(victim);
    } else if (st == CardSubType::TIE_SUO_LIAN_HUAN) {
        // 不选择目标即重铸；普通版本最多横置或重置两人，候选仅列可成为目标者。
        auto pool = getAlivePlayers();
        pool.erase(std::remove_if(pool.begin(), pool.end(), [&](const PlayerPtr& target) {
            return !canBeTargeted(target, chosen, player);
        }), pool.end());
        int maxTargets = 2;
        for (const auto& skill : getEffectiveSkills(*player))
            skill->onCalculateTieSuoTargetLimit(*this, *player, maxTargets);
        maxTargets = std::min(maxTargets, static_cast<int>(pool.size()));
        for (int i = 0; i < maxTargets && !pool.empty(); ++i) {
            auto target = askChoosePlayer(player, pool, "【铁索连环】选择角色（取消为结束选择/重铸）", true);
            if (!target) break;
            targets.push_back(target);
            pool.erase(std::remove(pool.begin(), pool.end(), target), pool.end());
        }
    } else if (st == CardSubType::GUO_HE_CHAI_QIAO || st == CardSubType::JUE_DOU || st == CardSubType::LE_BU_SI_SHU || st == CardSubType::HUO_GONG) {
        std::vector<PlayerPtr> candidates;
        for (auto& p : getOtherAlivePlayers(*player)) {
            if (!canBeTargeted(p, chosen, player)) continue;
            if (st == CardSubType::GUO_HE_CHAI_QIAO && p->getAllCards().empty()) continue;
            if (st == CardSubType::HUO_GONG && p->getHandCardCount()==0) continue;
            if (st == CardSubType::LE_BU_SI_SHU && hasJudgeCardOf(*p, CardSubType::LE_BU_SI_SHU)) continue;
            candidates.push_back(p);
        }
        if (candidates.empty()) {
            log() << "没有合法的目标！" << std::endl;
            return false;
        }
        PlayerPtr t = askChoosePlayer(player, candidates, "选择【" + chosen->getName() + "】的目标", true);
        if (!t) return false;
        targets.push_back(t);
        // 谋·孙策【激昂】：任何【决斗】均可额外指定一名目标，代价为流失1点体力。
        if (st == CardSubType::JUE_DOU && player->getHero() &&
            player->getHero()->findSkill("谋-激昂")) {
            std::vector<PlayerPtr> extraCandidates;
            for (auto& p : candidates)
                if (p->getId() != t->getId()) extraCandidates.push_back(p);
            PlayerPtr extra = askChoosePlayer(player, extraCandidates,
                "【激昂】是否流失1点体力额外指定一名目标？", true);
            if (extra) {
                player->addMark("谋激昂追加目标", 1 - player->getMark("谋激昂追加目标"));
                loseHp(player, 1, "激昂");
                if (player->isAlive()) targets.push_back(extra);
            }
        }
    } else if (st == CardSubType::SHUN_SHOU_QIAN_YANG || st == CardSubType::BING_LIANG_CUN_DUAN) {
        std::vector<PlayerPtr> candidates;
        for (auto& p : getOtherAlivePlayers(*player)) {
            if (!canBeTargeted(p, chosen, player)) continue;
            if(st==CardSubType::BING_LIANG_CUN_DUAN ?
                !canUseBingLiangOn(*player,*p,chosen) : !canUseShunShouOn(*player,*p,chosen))continue;
            if (st == CardSubType::SHUN_SHOU_QIAN_YANG && p->getAllCards().empty()) continue;
            if (st == CardSubType::BING_LIANG_CUN_DUAN && hasJudgeCardOf(*p, CardSubType::BING_LIANG_CUN_DUAN)) continue;
            candidates.push_back(p);
        }
        if (candidates.empty()) {
            log() << "距离 1 以内没有合法的目标！" << std::endl;
            return false;
        }
        PlayerPtr t = askChoosePlayer(player, candidates, "选择【" + chosen->getName() + "】的目标", true);
        if (!t) return false;
        targets.push_back(t);
    } else if (st == CardSubType::MANTIAN_GUOHAI) {
        std::vector<PlayerPtr> candidates;
        for (const auto& target : getOtherAlivePlayers(*player))
            if (!target->getAllCards().empty() && canBeTargeted(target,chosen,player))
                candidates.push_back(target);
        if (candidates.empty()) {
            log() << "没有区域内有牌且能成为【瞒天过海】目标的其他角色！" << std::endl;
            return false;
        }
        for (int i=0; i<2 && !candidates.empty(); ++i) {
            PlayerPtr target=askChoosePlayer(player,candidates,
                i==0 ? "【瞒天过海】选择至多两名目标（选择第一名，0结束）"
                     : "【瞒天过海】可再选择一名目标（0结束）",
                true,candidates.front());
            if(!target)break;
            targets.push_back(target);
            candidates.erase(std::remove(candidates.begin(),candidates.end(),target),candidates.end());
        }
        if(targets.empty())return false;
    }

    return useCard(player, chosen, targets);
}

void GameEngine::phaseDiscard(PlayerPtr player) {
    discardedThisPhase.clear();
    {
        bool skipDiscard = false;
        for (const auto& s : getEffectiveSkills(*player)) {
            s->onPhaseStart(*this, *player, TurnPhase::DISCARD, skipDiscard);
        }
        if (skipDiscard) {
            logMessage("  [" + player->getName() + "] 跳过了弃牌阶段");
            notifyPhaseSkipped(player,TurnPhase::DISCARD);
            return;
        }
    }
    int handLimit = calculateHandLimit(player);
    int excess = player->getHandCardCount() - handLimit;
    if (excess <= 0) {
        for (auto& s : getEffectiveSkills(*player)) s->onPhaseEnd(*this,*player,TurnPhase::DISCARD);
        return;
    }

    logMessage("[" + player->getName() + "] 当前手牌 " + std::to_string(player->getHandCardCount()) +
               " 张，上限 " + std::to_string(handLimit) + " 张，需弃置 " + std::to_string(excess) + " 张手牌。");

    if (player->isAI()) {
        auto toDiscard = AIController::chooseCardsToDiscard(*this, *player, excess);
        for (auto card : toDiscard) {
            discardCardOf(player, card);
        }
    } else {
        std::string heroId = player->getHero() ? player->getHero()->getId() : "";
        for (int i = 0; i < excess; ++i) {
            auto repromptDiscard = [this, player, i, excess]() {
                log() << "【你的手牌】: " << player->getFormattedHandCards() << "\n";
                log() << "选择第 " << (i + 1) << "/" << excess << " 张要弃置的手牌编号 (1~" << player->getHandCardCount()
                      << ", t=图鉴, m=记牌器, c=局势): ";
            };
            repromptDiscard();
            std::string token = ui->readTokenWithCodex(heroId, repromptDiscard);
            int idx = Interaction::parseInt(token);
            if (idx >= 1 && idx <= player->getHandCardCount()) {
                CardPtr card = player->getHandCards()[idx - 1];
                discardCardOf(player, card);
            } else if (Interaction::inputClosed()) {
                discardCardOf(player, player->getHandCards().front());
            } else {
                --i;
            }
        }
    }
    for (auto& p : getOtherAlivePlayers(*player))
        for (auto& s : getEffectiveSkills(*p)) s->onOtherDiscardPhaseEnd(*this,*p,*player,discardedThisPhase);
    for (auto& s : getEffectiveSkills(*player)) s->onPhaseEnd(*this,*player,TurnPhase::DISCARD);
    discardedThisPhase.clear();
}

void GameEngine::phaseFinish(PlayerPtr player) {
    bool skipped=false;
    // “结束阶段开始时”（据守）与“结束阶段”（崩坏等）分开结算，
    // 不能因为缺少阶段开始事件把先后时机合并成阶段结束。
    for(const auto& skill:getEffectiveSkills(*player)) {
        if(skipped)break;
        skill->onPhaseStart(*this,*player,TurnPhase::FINISH,skipped);
    }
    if(!player->isAlive() || gameOver)return;
    if(skipped){
        logMessage("["+player->getName()+"] 跳过了结束阶段！");
        notifyPhaseSkipped(player,TurnPhase::FINISH);
        return;
    }
    for (const auto& skill : getEffectiveSkills(*player)) {
        skill->onPhaseEnd(*this, *player, TurnPhase::FINISH);
    }
    // 结束阶段是全场事件：广播给所有存活角色（主公【立储】“第一轮的任意角色结束阶段”）。
    if (!gameOver) {
        for (auto& p : getAlivePlayers())
            for (const auto& skill : getEffectiveSkills(*p))
                skill->onAnyPhaseEnd(*this, *p, *player, TurnPhase::FINISH);
    }
}

// ==================== 用牌 ====================

bool GameEngine::resolveGuHuo(PlayerPtr source, CardPtr card) {
    if (!card || card->getSkillSource()!="蛊惑" || !source) return true;
    logMessage("  【蛊惑】["+source->getName()+"] 扣置一张手牌，声明【"+card->getName()+"】");
    std::vector<PlayerPtr> challengers;
    // 先收集所有质疑者，再统一验明底牌；不能在第一个质疑者处提前结束。
    for (auto p : getOtherAlivePlayers(*source)) {
        bool wants = getRng()()%5==0; // AI 不读取扣置牌真实内容
        if(askConfirm(p,"【蛊惑】是否质疑？",wants)) challengers.push_back(p);
    }
    if(challengers.empty())return true;
    auto reals=card->getRealCards(card);
    bool truthful=reals.size()==1 && reals.front()->getSubType()==card->getSubType() &&
                  reals.front()->getName()==card->getName() &&
                  (card->getSubType()!=CardSubType::SHA ||
                   reals.front()->getShaElement()==card->getShaElement());
    logMessage("  【蛊惑】揭示底牌：" + (reals.empty()?std::string("无") : reals.front()->getFormattedName()));
    for(auto challenger:challengers) {
        if(!challenger->isAlive())continue;
        if(truthful)loseHp(challenger,1,"蛊惑质疑属实");
        else drawCards(challenger,1,"蛊惑质疑为假");
    }
    // 被质疑的真牌也仅在原牌为红桃时继续按声明结算；其余情况作废。
    bool valid=truthful && effectiveSuit(*source,reals.front())==Suit::HEART;
    if(!valid)logMessage("  【蛊惑】被质疑的牌作废并弃置。");
    return valid;
}

bool GameEngine::useCard(PlayerPtr source, CardPtr card, std::vector<PlayerPtr> targets) {
    // 用户 2026-10-05：游戏结束后不存在技能发动或牌结算。
    if (gameOver) return false;
    if (!source || !card || !canUseOriginalCard(*source,card)) return false;
    if (card->getSubType() == CardSubType::NAN_MAN_RU_QIN &&
        currentPhase == TurnPhase::PLAY && source->getMark("谋祸首本阶段已用") > 0) {
        logMessage("  【祸首】本出牌阶段已使用过【南蛮入侵】，不能再次使用。");
        return false;
    }
    lastResolvedTargets = targets; // 供结算后的目标查询（戮连等）
    card=materializeConversion(source,card);
    if(auto original=wumouMaterial(card)) {
        auto st=original->getSubType();
        if(wumouAllTargets(st))targets=getShaTargets(*source,card);
        else if(st==CardSubType::WU_ZHONG_SHENG_YOU)targets={source};
        else if(st==CardSubType::JIE_DAO_SHA_REN) {
            if(targets.size()!=2 || !targets[0] || !targets[1])return false;
            auto holders=getJieDaoWeaponHolders(*source);
            if(std::find(holders.begin(),holders.end(),targets[0])==holders.end())return false;
            auto victims=getJieDaoVictims(*targets[0]);
            if(std::find(victims.begin(),victims.end(),targets[1])==victims.end())return false;
        }
    }
    if(card->getSubType()==CardSubType::SHA && isPlayerTurn(*source) &&
       currentPhase==TurnPhase::PLAY &&
       source->getShaCountThisTurn()>=getShaLimitForCard(*source,card)) {
        bool limitBypassed = card->getSkillSource()=="莺舞" || card->getSkillSource()=="势-战烈";
        if(!limitBypassed)
            for(const auto& target:targets)
                if(target && canUseShaBeyondLimitOn(*source,*target,card)) { limitBypassed=true; break; }
        if(!limitBypassed)return false;
    }
    if(card->getSubType()==CardSubType::TAO && !canUsePeach(source,activeDying ? activeDying : source))return false;

    bool recast=card->getSubType()==CardSubType::TIE_SUO_LIAN_HUAN && targets.empty();
    logMessage("[" + source->getName() + "] " + (recast ? "重铸了 " : "使用了 ") + card->getPublicName());

    if (card->getType() == CardType::EQUIPMENT && !card->isVirtual()) {
        consumeCard(source, card, false);
        for (auto& skill : getEffectiveSkills(*source)) skill->onCardPlayed(*this, *source, card);
        notifyCardUsed(source, card);
        CardPtr oldEquip = source->equip(card);
        if (oldEquip) {
            deck.discardCard(oldEquip);
            logMessage("  替换并弃置旧装备 " + oldEquip->getFormattedName());
            afterEquipmentLost(source, oldEquip);
            notifyCardLostOutsideTurn(source,oldEquip);
        }
        for (auto& skill : getEffectiveSkills(*source)) skill->onCardResolved(*this, *source, card);
        return true;
    }

    auto sub=card->getSubType();
    // 目标合法性检查（如【空城】不能成为【杀】/【决斗】的目标）。
    // 【借刀杀人】的第二名角色不是借刀的直接目标，而是持武器者要杀的目标；
    // 因此对其应用【杀】的合法性，而不是借刀锦囊本身的目标技能。
    targets.erase(std::remove_if(targets.begin(), targets.end(),
                                 [&](const PlayerPtr& t) {
                                     if (!t) return false;
                                     if (sub == CardSubType::JIE_DAO_SHA_REN && targets.size() >= 2 &&
                                         t == targets[1]) return false;
                                     if (!canBeTargeted(t, card, source)) {
                                         logMessage("  [" + t->getName() + "] 不能成为 " + card->getName() + " 的目标！");
                                         return true;
                                     }
                                     return false;
                                 }),
                  targets.end());

    bool needsTarget=sub==CardSubType::JUE_DOU || sub==CardSubType::HUO_GONG ||
        sub==CardSubType::GUO_HE_CHAI_QIAO || sub==CardSubType::SHUN_SHOU_QIAN_YANG ||
        sub==CardSubType::MANTIAN_GUOHAI || sub==CardSubType::LE_BU_SI_SHU ||
        sub==CardSubType::BING_LIANG_CUN_DUAN || sub==CardSubType::JIE_DAO_SHA_REN;
    // 帷幕/空城等排除目标后不能把一张无目标的指定锦囊当作成功使用，
    // 也不能因此支付无谋、蛊惑等使用费用。群体锦囊与铁索重铸不受影响。
    if((needsTarget && targets.empty()) ||
       (sub==CardSubType::JIE_DAO_SHA_REN && targets.size()!=2))return false;
    if(sub==CardSubType::JIE_DAO_SHA_REN) {
        PlayerPtr holder=targets[0], victim=targets[1];
        if(!holder || !victim || !holder->isAlive() || !victim->isAlive() ||
           !canBeTargeted(holder,card,source))return false;
        auto holders=getJieDaoWeaponHolders(*source);
        if(std::find(holders.begin(),holders.end(),holder)==holders.end())return false;
        auto victims=getJieDaoVictims(*holder);
        if(std::find(victims.begin(),victims.end(),victim)==victims.end())return false;
    }
    if(sub==CardSubType::JUE_DOU) {
        const size_t maxTargets=source->getMark("谋激昂追加目标")>0 ? 2 : 1;
        if(targets.empty() || targets.size()>maxTargets)return false;
        std::set<int> seen;
        for(const auto& target:targets)
            if(!target || !seen.insert(target->getId()).second)return false;
    }
    if(sub==CardSubType::TIE_SUO_LIAN_HUAN) {
        int maxTargets=2;
        for(const auto& skill:getEffectiveSkills(*source))
            skill->onCalculateTieSuoTargetLimit(*this,*source,maxTargets);
        if(static_cast<int>(targets.size())>maxTargets)return false;
        std::set<int> seen;
        for(const auto& target:targets)
            if(!target || !seen.insert(target->getId()).second)return false;
    }
    if((sub==CardSubType::HUO_GONG || sub==CardSubType::GUO_HE_CHAI_QIAO ||
        sub==CardSubType::LE_BU_SI_SHU || sub==CardSubType::BING_LIANG_CUN_DUAN ||
        (sub==CardSubType::SHUN_SHOU_QIAN_YANG && card->getSkillSource()!="势-急袭")) &&
       targets.size()!=1)return false;
    if((sub==CardSubType::JUE_DOU || sub==CardSubType::HUO_GONG ||
        sub==CardSubType::GUO_HE_CHAI_QIAO || sub==CardSubType::LE_BU_SI_SHU ||
        sub==CardSubType::SHUN_SHOU_QIAN_YANG || sub==CardSubType::BING_LIANG_CUN_DUAN) &&
       (!targets.front() || targets.front()->getId()==source->getId()))return false;
    if(sub==CardSubType::GUO_HE_CHAI_QIAO && targets.front()->getAllCards().empty())return false;
    if(sub==CardSubType::HUO_GONG && targets.front()->getHandCardCount()==0)return false;
    if(sub==CardSubType::MANTIAN_GUOHAI) {
        // 瞒天过海：必须指定1至2名互不重复、区域内有牌的其他角色。
        // 直接调用 useCard 的路径也执行同一合法性检查，不能靠 UI 兜底。
        if(targets.size()>2)return false;
        std::set<int> seen;
        for(const auto& target:targets) {
            if(!target || !target->isAlive() || target->getId()==source->getId() ||
               target->getAllCards().empty() || !seen.insert(target->getId()).second)
                return false;
        }
    }
    if((sub==CardSubType::LE_BU_SI_SHU || sub==CardSubType::BING_LIANG_CUN_DUAN) &&
       (!targets.front() || hasJudgeCardOf(*targets.front(),sub)))return false;
    if(sub==CardSubType::SHUN_SHOU_QIAN_YANG) {
        std::set<int> seen;
        for(const auto& target:targets) {
            if(!target || !canUseShunShouOn(*source,*target,card) ||
               !seen.insert(target->getId()).second)return false;
        }
    }
    if(sub==CardSubType::BING_LIANG_CUN_DUAN &&
       !canUseBingLiangOn(*source,*targets.front(),card))return false;
    if(sub==CardSubType::SHA) {
        if(targets.empty())return false;
        for(const auto& target:targets)
            if(!target || !target->isAlive() || !canUseShaOn(*source,*target,card))return false;
    }
    if(sub==CardSubType::JUE_DOU && source->getMark("谋激昂追加目标")>0)
        source->addMark("谋激昂追加目标",-source->getMark("谋激昂追加目标"));
    // 仅在所有候选都通过最终合法性检查后才记录“用过杀”。
    if(sub==CardSubType::SHA)source->addMark("本回合用过杀",1); // 肆恣等
    // 任何角色使用一张牌（演策预测验证、符济“使用符济牌时”等）
    for (auto& pp : getAlivePlayers())
        for (auto sk : getEffectiveSkills(*pp)) {
            sk->onAnyCardUsed(*this, *pp, *source, card);
        }
    // 目标确定：向全体广播（帷幕二级「上一轮成为目标的次数」等计数）。
    // 【借刀杀人】只以持武器角色为直接目标；其被杀目标不是借刀锦囊的目标。
    {
        std::vector<PlayerPtr> confirmedTargets = targets;
        if (confirmedTargets.empty()) {
            std::vector<PlayerPtr> areaTargets;
            if (sub == CardSubType::NAN_MAN_RU_QIN || sub == CardSubType::WAN_JIAN_QI_FA)
                areaTargets = getOtherAlivePlayers(*source);
            else if (sub == CardSubType::TAO_YUAN_JIE_YI || sub == CardSubType::WU_GU_FENG_DENG)
                areaTargets = getAlivePlayers();
            for (const auto& target : areaTargets)
                if (canBeTargeted(target, card, source)) confirmedTargets.push_back(target);
        }
        // 【借刀杀人】只以持武器角色为直接目标；其被杀目标不是借刀锦囊的目标。
        if (sub == CardSubType::JIE_DAO_SHA_REN && confirmedTargets.size() >= 2)
            confirmedTargets.resize(1);
        if (!confirmedTargets.empty()) {
            for (auto& pp : getAlivePlayers())
                for (auto sk : getEffectiveSkills(*pp))
                    sk->onCardTargetConfirmed(*this, *pp, source.get(), card, confirmedTargets);
        }
        // AI 队友分辨：记录“谁对谁用了什么牌”（公开可见的行为证据）
        recordCardRelation(source, card, confirmedTargets);
    }
    // 其他角色使用与记录牌名相同的牌时可令其无效（如谋·诸葛亮【看破】）：消耗此牌但不结算效果。
    {
        bool cancelled=false;
        for (auto& pp : getAlivePlayers()) {
            if (pp->getId() == source->getId()) continue;
            for (auto sk : getEffectiveSkills(*pp)) {
                sk->onOtherCardUsedBefore(*this, *pp, *source, card, cancelled);
                if (cancelled) break;
            }
            if (cancelled) break;
        }
        if (cancelled) {
            consumeCard(source, card, false);
            deck.discardCards(card->getRealCards(card));
            return true;
        }
    }
    // 重铸是移动牌并摸牌，不是使用锦囊；不触发集智、无谋等使用时事件。
    if(recast) {
        if(card->getSkillSource()=="蛊惑")return false; // 蛊惑只能使用/打出，不能重铸
        consumeCard(source,card,false);
        deck.discardCards(card->getRealCards(card));
        drawCards(source,1,"重铸铁索连环");
        return true;
    }
    if(card->getSubType()==CardSubType::SHA) {
        std::vector<PlayerPtr> legal;
        for(auto t:targets)if(t && canUseShaOn(*source,*t,card) &&
                              std::find(legal.begin(),legal.end(),t)==legal.end() &&
                              static_cast<int>(legal.size())<getShaTargetLimit(*source,card))legal.push_back(t);
        targets=std::move(legal);
        if(targets.empty())return false;
    }
    if (!resolveGuHuo(source,card)) {
        consumeCard(source,card,true);
        return false;
    }
    // 决斗在指定目标时触发技能；之后即使被无懈可击抵消也已成为目标。
    if(card->getSubType()==CardSubType::JUE_DOU && !targets.empty() && targets.front()) {
        for(auto skill:getEffectiveSkills(*source))
            skill->onDuelTargeted(*this,*source,*source,*targets.front());
        for(auto skill:getEffectiveSkills(*targets.front()))
            skill->onDuelTargeted(*this,*targets.front(),*source,*targets.front());
    }
    // 先从区域移除（实体牌暂不进弃牌堆，等结算完成后处理，以便替身等技能获得之）
    consumeCard(source, card, false);
    for (auto& s : getEffectiveSkills(*source)) s->onCardPlayed(*this, *source, card);
    notifyCardUsed(source, card);

    if (card->getSubType() == CardSubType::SHA) {
        source->incrementShaCount();
        resolveSha(source, card, targets);
        return true;
    }

    bool stays = (card->getType() == CardType::TRICK && isTrickStayingOnTable(card));
    bool claimed = false;
    if (card->getType() == CardType::TRICK && !isTrickStayingOnTable(card)) {
        for (const auto& s : getEffectiveSkills(*source)) {
            s->onUseCard(*this, *source, card);
        }
    }
    if (card->getSubType() == CardSubType::TAO) {
        handleTao(source, card);
    } else if (card->getSubType() == CardSubType::JIU) {
        handleJiu(source, card);
    } else if (card->getType() == CardType::TRICK) {
        claimed = handleTrick(source, card, targets);
    }

    if (!stays && !claimed) {
        deck.discardCards(card->getRealCards(card));
    }
    for(auto skill:getEffectiveSkills(*source))skill->onCardResolved(*this,*source,card);
    return true;
}

// 目标合法性（如【空城】）：target 侧技能可否决
bool GameEngine::canTakeCardEffect(PlayerPtr target, CardPtr card, PlayerPtr source) {
    if(!target)return false;
    CardPtr viewed=source && card && effectiveSuit(*source,card)!=card->getSuit()
        ? card->copyWithSuit(effectiveSuit(*source,card)) : card;
    bool effective=true;
    for(const auto& s:getEffectiveSkills(*target))
        s->onCheckCardEffect(*this,*target,viewed,effective);
    return effective;
}

bool GameEngine::canBeTargeted(PlayerPtr target, CardPtr card, PlayerPtr source) {
    // 目标技能看到的是使用者眼中的花色，不可直接更改实体牌（离手后会恢复原花色）。
    CardPtr viewed=source && card && effectiveSuit(*source,card)!=card->getSuit()
        ? card->copyWithSuit(effectiveSuit(*source,card)) : card;
    if (!target) return true;
    PlayerPtr prevSource = checkingSource;
    checkingSource = source;
    bool canTarget = true;
    for (const auto& s : getEffectiveSkills(*target)) {
        s->onCheckCardTarget(*this, *target, *target, viewed, canTarget);
    }
    // 使用者侧限制（如【北伐】：标记角色用牌只能选择姜维或其自己为目标）。
    if (source && canTarget) {
        for (const auto& s : getEffectiveSkills(*source)) {
            s->onCheckCardTargetAsSource(*this, *source, *target, viewed, canTarget);
        }
        // 谋·姜维【志继】的“北伐”是授予给其他角色的标记；被标记者使用牌时，
        // 目标仅能是授予标记的姜维或其自身。来源 ID 存在标记上，以支持该角色的全牌类型。
        const int grantorId = source->getMark("北伐来源") - 1;
        if(source->getMark("北伐")>0 && grantorId>=0 &&
           target->getId()!=source->getId() && target->getId()!=grantorId)
            canTarget=false;
    }
    checkingSource = prevSource;
    return canTarget;
}

void GameEngine::playSkillSha(PlayerPtr source, const std::string& skill) {
    if (!source || !source->isAlive())return;
    // “视为使用【杀】”同样计入本回合使用【杀】的次数（如夏侯渊【神速】）。
    source->incrementShaCount();
    auto card=Card::makeVirtual("杀",CardType::BASIC,CardSubType::SHA,{},skill);
    std::vector<PlayerPtr> candidates;
    for(auto t:getOtherAlivePlayers(*source))if(canBeTargeted(t,card,source))candidates.push_back(t);
    PlayerPtr ai;
    for(auto t:candidates)if(!AIController::isFriend(*this, *source,*t)){ai=t;break;}
    auto target=askChoosePlayer(source,candidates,"【"+skill+"】选择无距离限制的杀目标",true,ai);
    if(target) {
        for(const auto& s:getEffectiveSkills(*source))s->onCardPlayed(*this,*source,card);
        notifyCardUsed(source,card);
        resolveSha(source,card,{target});
    }
}

void GameEngine::resolveSha(PlayerPtr source, CardPtr card, const std::vector<PlayerPtr>& targets) {
    if (gameOver) return; // 游戏结束后不再有牌结算
    if (!source || !card) return;
    // 【酒】：对使用的下一张【杀】生效（无论是否命中都消耗）
    bool drunk = source->isDrunk();
    if (drunk) source->setDrunk(false);

    bool claimed = false;
    for (const auto& t : targets) {
        if (!t || !t->isAlive()) continue;
        if (!source->isAlive() || gameOver) break;
        if (!canBeTargeted(t,card,source))continue;
        if (handleSha(source, t, card, drunk, claimed)) claimed = true;
    }
    if (!claimed) {
        deck.discardCards(card->getRealCards(card));
    }
    if(source->isAlive())for(auto skill:getEffectiveSkills(*source))
        skill->onCardResolved(*this,*source,card);
}

bool GameEngine::handleSha(PlayerPtr source, PlayerPtr target, CardPtr card, bool drunk, bool alreadyClaimed) {
    if (!source || !target || !card || !target->isAlive()) return false;

    ShaContext ctx;
    ctx.source = source;
    ctx.target = target;
    ctx.card = card;
    ctx.cardClaimed = alreadyClaimed;

    logMessage("  目标为 [" + target->getName() + "]");

    // 指定目标后：来源的技能（铁骑 / 烈弓 / 义绝加伤）
    for (const auto& s : getEffectiveSkills(*source)) {
        s->onShaTargeted(*this, *source, ctx);
    }

    // 指定目标后：目标的技能（如【流离】可转移目标）
    for (const auto& s : getEffectiveSkills(*target)) {
        s->onShaTargeted(*this, *target, ctx);
    }
    if (ctx.redirect && ctx.redirect->isAlive()) {
        logMessage("  此【杀】的目标转移至 [" + ctx.redirect->getName() + "]！");
        return handleSha(source, ctx.redirect, card, drunk, ctx.cardClaimed);
    }

    // 青釭剑：无视目标防具（仁王盾 / 八卦阵 / 藤甲 / 白银狮子 对此【杀】均无效）
    bool ignoreArmor = hasWeapon(*source, "青釭剑") || target->getMark("无前防具失效")>0 ||
                       source->getMark("无前目标:"+std::to_string(target->getId())) > 0;
    if (ignoreArmor && target->getArmor()) {
        logMessage("  【青釭剑】：无视 [" + target->getName() + "] 的防具 " + target->getArmor()->getFormattedName());
    }

    // 1. 使【杀】无效的防具（不属于"抵消"，不触发青龙偃月刀 / 贯石斧）
    bool invalid = ctx.invalidTarget;
    if (!ignoreArmor && target->getArmor()) {
        const std::string armorName = target->getArmor()->getName();
        if (armorName == "仁王盾" && (effectiveSuit(*source,card)==Suit::SPADE || effectiveSuit(*source,card)==Suit::CLUB)) {
            logMessage("  [" + target->getName() + "] 装备了【仁王盾】，黑色【杀】对其无效！");
            invalid = true;
        } else if (armorName == "藤甲" && card->getShaElement() == ShaElement::NORMAL) {
            logMessage("  [" + target->getName() + "] 装备了【藤甲】，普通【杀】对其无效！");
            invalid = true;
        }
    }

    // 2. 抵消：八卦阵 / 打出【闪】
    bool dodged = false;
    if (!invalid) {
        if (ctx.cannotDodge) {
            logMessage("  [" + target->getName() + "] 不能使用【闪】响应此【杀】！");
        } else {
            int providedShan = 0;
            if (!ignoreArmor && (hasArmor(*target, "八卦阵") ||
                (!target->getArmor() && target->getHero() && target->getHero()->findSkill("八阵")))) {
                logMessage("  [" + target->getName() + "] 发动【八卦阵】判定...");
                CardPtr judgeRes = doJudgement(target, "八卦阵");
                if (judgeRes && (effectiveSuit(*target, judgeRes) == Suit::HEART || effectiveSuit(*target, judgeRes) == Suit::DIAMOND)) {
                    logMessage("  【八卦阵】判定为红色！视为打出【闪】！");
                    auto virtualShan=Card::makeVirtual("闪",CardType::BASIC,CardSubType::SHAN,{},"八卦阵");
                    for(auto& skill:getEffectiveSkills(*target))
                        skill->onCardResponded(*this,*target,virtualShan);
                    providedShan = 1;
                }
            }
            // 八卦阵仅视为打出一张【闪】，无双/肉林仍须补足其余的【闪】。
            int need = 1;
            for (const auto& s : getEffectiveSkills(*source)) {
                s->onCalculateResponseCount(*this, *source, *target, CardSubType::SHAN, need);
            }
            // 肉林双向生效：女性攻击董卓时也需要连续两张闪。
            if (target->getHero() && target->getHero()->findSkill("肉林") && source->getHero() &&
                source->getHero()->getGender() == Gender::FEMALE) need = std::max(need,2);
            bool allDodged = true;
            for (int i = providedShan; i < need; ++i) {
                std::string prompt = "[" + source->getName() + "] 对你使用了【杀】，请打出一张【闪】";
                if (need > 1) prompt += "（" + std::to_string(i + 1) + "/" + std::to_string(need) + "）";
                CardPtr responseShan = askResponseCard(target, CardSubType::SHAN, prompt);
                if (responseShan && ctx.extraResponseCost > 0) {
                    // 额外弃置一张牌方可响应（势-战烈选项3）
                    auto discardable = target->getHandAndEquipmentCards();
                    if (!discardable.empty()) {
                        auto fee = askChooseCard(target, discardable,
                            "【额外弃置】此【杀】需额外弃置一张牌方可响应", false,
                            AIController::chooseLeastValuableCard(discardable));
                        if (fee) {
                            discardCardOf(target, fee, "额外弃置");
                        } else {
                            responseShan = nullptr;
                            logMessage("  [" + target->getName() + "] 无牌可弃，无法响应。");
                        }
                    } else {
                        responseShan = nullptr;
                        logMessage("  [" + target->getName() + "] 无牌可弃，无法响应。");
                    }
                }
                if (responseShan) target->addMark("杀响应已打出", 1); // 无双：是否打出过【闪】
                if (!responseShan) {
                    allDodged = false;
                    break;
                }
            }
            if (allDodged) {
                logMessage("  [" + target->getName() + "] 躲避了攻击！");
                dodged = true;
            }
        }
    }

    // 3. 被【闪】抵消后的武器效果：贯石斧可令此【杀】依然造成伤害
    bool dodgedByShan = dodged;
    if (dodged && weaponAfterShaDodged(ctx)) {
        dodged = false;
    }
    ctx.dodgedByShan = dodgedByShan && dodged;

    // 4. 造成伤害（麒麟弓 / 寒冰剑 在此时机结算）
    if (!invalid && !dodged) {
        ctx.hpBeforeDamage = target->getHp(); // 供替身等判断本【杀】是否实际造成了伤害
        int damage = 1 + ctx.extraDamage;
        if(card->getSkillSource()=="龙魂" && card->getSubCards().size()==2 &&
           card->getSubType()==CardSubType::SHA && card->getShaElement()==ShaElement::FIRE)
            damage++;
        if (drunk) {
            damage += 1;
            logMessage("  （加上【酒】的加成，伤害 +1！）");
        }
        for (auto& pp : getAlivePlayers())
            for (auto sk : getEffectiveSkills(*pp))
                sk->onCalculateShaDamage(*this, *pp, *source, *target, card, damage);
        if (!weaponBeforeShaDamage(ctx)) {
            ctx.hit = true;
            applyDamage(source, target, damage, card->getShaElement(), ignoreArmor, card, &ctx.cardClaimed);
        }
    }

    // 5. 结算结束：目标的技能（替身）
    if (target->isAlive()) {
        for (const auto& s : getEffectiveSkills(*target)) {
            s->onShaFinished(*this, *target, ctx);
        }
    }

    // 使用者侧的结算后技能。
    if (source->isAlive())
        for (auto& s : getEffectiveSkills(*source)) s->onShaFinished(*this,*source,ctx);
    target->addMark("杀响应已打出", -target->getMark("杀响应已打出")); // 响应窗口结束

    // 6. 青龙偃月刀：被【闪】抵消（且未被贯石斧强制命中）时可再使用一张【杀】
    if (dodgedByShan && dodged) {
        weaponQingLongChase(ctx);
    }
    return ctx.cardClaimed;
}

void GameEngine::handleTao(PlayerPtr source, CardPtr card) {
    (void)card;
    if (source->getHp() < source->getMaxHp()) {
        recoverHp(source, (card->getSkillSource()=="龙魂" && card->getSubCards().size()==2)?2:1,"桃",source);
        for (auto& pp : getAlivePlayers())
            for (auto sk : getEffectiveSkills(*pp)) sk->onAnyPeachUsed(*this, *pp, *source, *source);
    } else {
        logMessage("  [" + source->getName() + "] 体力已满，【桃】无效果。");
    }
}

void GameEngine::handleJiu(PlayerPtr source, CardPtr card) {
    (void)card;
    source->setDrunk(true);
    logMessage("  [" + source->getName() + "] 使用了【酒】，本回合下一张【杀】造成的伤害 +1！");
}

bool GameEngine::handleTrick(PlayerPtr source, CardPtr card, std::vector<PlayerPtr> targets) {
    CardSubType subType = card->getSubType();
    bool trickClaimed = false;

    bool aoe = (subType == CardSubType::NAN_MAN_RU_QIN || subType == CardSubType::WAN_JIAN_QI_FA || subType == CardSubType::TAO_YUAN_JIE_YI ||
                subType == CardSubType::WU_GU_FENG_DENG);
    if (!aoe && subType!=CardSubType::TIE_SUO_LIAN_HUAN &&
        subType!=CardSubType::WU_ZHONG_SHENG_YOU && subType!=CardSubType::MANTIAN_GUOHAI &&
        !(subType==CardSubType::JUE_DOU && targets.size()>1) &&
        !(subType==CardSubType::SHUN_SHOU_QIAN_YANG && targets.size()>1) &&
        !isTrickStayingOnTable(card) &&
        !targets.empty() && targets[0]) {
        if (askNullification(source, targets[0], card)) {
            logMessage("  【" + card->getName() + "】的效果被【无懈可击】抵消！");
            return false;
        }
    }

    if (subType == CardSubType::TIE_SUO_LIAN_HUAN) {
        if (targets.empty()) drawCards(source, 1, "重铸铁索连环");
        else for (auto& t : targets) if (t && t->isAlive() &&
            canBeTargeted(t,card,source) && !askNullification(source,t,card)) {
            t->setChained(!t->isChained());
            logMessage("  [" + t->getName() + "] " + (t->isChained() ? "横置" : "重置"));
        }
    } else if (subType == CardSubType::HUO_GONG) {
        if (!targets.empty() && targets[0] && !targets[0]->getHandCards().empty()) {
            auto t = targets[0];
            auto shown=askChooseCard(t,t->getHandCards(),"【火攻】选择一张手牌展示",false,
                                      AIController::chooseLeastValuableCard(t->getHandCards()));
            if(!shown)return trickClaimed;
            logMessage("  【火攻】[" + t->getName() + "] 展示 " + shown->getFormattedName());
            std::vector<CardPtr> same;
            for (auto c : source->getHandCards()) if (effectiveSuit(*source,c) == effectiveSuit(*t,shown)) same.push_back(c);
            auto c = askChooseCard(source,same,"【火攻】弃置同花色手牌造成1点火焰伤害",true,same.empty()?nullptr:same.front());
            if (c) {discardCardOf(source,c,"火攻");applyDamage(source,t,1,ShaElement::FIRE,false,card,&trickClaimed);}
        }
    } else if (subType == CardSubType::WU_ZHONG_SHENG_YOU) {
        if (askNullification(source, source, card)) {
            logMessage("  【无中生有】被【无懈可击】抵消！");
            return false;
        }
        drawCards(source, 2, "无中生有");
    } else if (subType == CardSubType::GUO_HE_CHAI_QIAO) {
        if (!targets.empty() && targets[0]) {
            PlayerPtr t = targets[0];
            CardPtr chosen = chooseCardFromPlayer(source,t,"选择要弃置的 [" + t->getName() + "] 区域内的一张牌",true);
            if (chosen) {
                discardCardOf(t, chosen, "过河拆桥",source);
            }
        }
    } else if (subType == CardSubType::SHUN_SHOU_QIAN_YANG) {
        for (const auto& target : targets) {
            if (!source->isAlive() || gameOver) break;
            if (!target || !target->isAlive() || !canBeTargeted(target,card,source) ||
                !canTakeCardEffect(target,card,source)) continue;
            if (targets.size() > 1 && askNullification(source,target,card)) continue;
            CardPtr chosen = chooseCardFromPlayer(source,target,
                "选择要获得的 [" + target->getName() + "] 区域内的一张牌",true);
            if (chosen) obtainCard(source, chosen, target);
        }
    } else if (subType == CardSubType::MANTIAN_GUOHAI) {
        // 先逐一获得每个未被无懈的目标角色区域内一张牌，再逐一交给这些目标一张手牌。
        // 目标被无懈后，其获得与交还两段效果都不结算。
        std::vector<PlayerPtr> affected;
        for (const auto& target : targets) {
            if (!source->isAlive() || gameOver) break;
            if (!target || !target->isAlive()) continue;
            if (!canBeTargeted(target,card,source)) continue;
            if (askNullification(source, target, card)) continue;
            if (!canTakeCardEffect(target,card,source)) continue;
            affected.push_back(target);
            CardPtr chosen = chooseCardFromPlayer(
                source,target,"【瞒天过海】选择获得 [" + target->getName() + "] 区域内的一张牌",true);
            if (chosen) {
                logMessage("  【瞒天过海】获得 [" + target->getName() + "] 的 " + chosen->getFormattedName());
                obtainCard(source, chosen, target);
            }
        }
        for (const auto& target : affected) {
            if (!source->isAlive() || gameOver) break;
            if (!target || !target->isAlive() || source->getHandCardCount() == 0) continue;
            auto hand = source->getHandCards();
            CardPtr give = askChooseCard(
                source, hand,
                "【瞒天过海】选择交给 [" + target->getName() + "] 的一张手牌", false,
                AIController::chooseLeastValuableCard(hand));
            if (!give || !source->hasHandCard(give)) continue;
            obtainCard(target, give, source);
            logMessage("  【瞒天过海】交给 [" + target->getName() + "] 一张 " + give->getFormattedName());
        }
    } else if (subType == CardSubType::JUE_DOU) {
        for (const auto& target : targets) {
            if (!source->isAlive() || gameOver) break;
            if (!target || !target->isAlive()) continue;
            if (targets.size() > 1 && askNullification(source, target, card)) continue;
            trickClaimed = resolveJueDou(source, target, card, true) || trickClaimed;
        }
    } else if (subType == CardSubType::NAN_MAN_RU_QIN) {
        for (auto t : getOtherAlivePlayers(*source)) {
            if (!source->isAlive() || gameOver) break;
            if (!canBeTargeted(t,card,source)) continue;
            if (!canTakeCardEffect(t,card,source)) continue;
            if (hasArmor(*t, "藤甲")) {
                logMessage("  [" + t->getName() + "] 装备了【藤甲】，【南蛮入侵】对其无效！");
                continue;
            }
            if (askNullification(source, t, card)) continue;
            CardPtr sha = askResponseCard(t, CardSubType::SHA, "【南蛮入侵】！请打出一张【杀】响应");
            if (!sha) {
                PlayerPtr owner = source;
                for (auto& p : getAlivePlayers()) if (p->getHero() && (p->getHero()->findSkill("祸首") || p->getHero()->findSkill("谋-祸首"))) { owner=p; break; }
                applyDamage(owner, t, 1, ShaElement::NORMAL, false, card, &trickClaimed);
            }
        }
    } else if (subType == CardSubType::WAN_JIAN_QI_FA) {
        for (auto t : getOtherAlivePlayers(*source)) {
            if (!source->isAlive() || gameOver) break;
            if (!canBeTargeted(t,card,source)) continue;
            if (!canTakeCardEffect(t,card,source)) continue;
            if (hasArmor(*t, "藤甲")) {
                logMessage("  [" + t->getName() + "] 装备了【藤甲】，【万箭齐发】对其无效！");
                continue;
            }
            if (askNullification(source, t, card)) continue;
            CardPtr shan = askResponseCard(t, CardSubType::SHAN, "【万箭齐发】！请打出一张【闪】响应");
            if (!shan) {
                applyDamage(source, t, 1, ShaElement::NORMAL, false, card, &trickClaimed);
            }
        }
    } else if (subType == CardSubType::WU_GU_FENG_DENG) {
        // 从使用者开始按行动顺序，每名角色依次选择并获得一张亮出的牌
        auto alive = getAlivePlayers();
        size_t start = 0;
        for (size_t i = 0; i < alive.size(); ++i) {
            if (alive[i]->getId() == source->getId()) { start = i; break; }
        }
        std::vector<CardPtr> revealed = deck.drawCards(static_cast<int>(alive.size()));
        std::string shown;
        for (const auto& c : revealed) shown += c->getFormattedName() + " ";
        logMessage("  【五谷丰登】亮出了 " + std::to_string(revealed.size()) + " 张牌: " + shown);
        for (size_t k = 0; k < alive.size() && !revealed.empty(); ++k) {
            PlayerPtr p = alive[(start + k) % alive.size()];
            if (!p->isAlive()) continue;
            if (askNullification(source, p, card)) {
                logMessage("  【五谷丰登】对 [" + p->getName() + "] 无效。");
                continue;
            }
            CardPtr pick = askChooseCard(p, revealed, "【五谷丰登】请选择一张牌获得", false,
                                         AIController::chooseMostValuableCard(*this, *p, revealed));
            if (!pick) pick = revealed.front();
            revealed.erase(std::remove(revealed.begin(), revealed.end(), pick), revealed.end());
            obtainCard(p, pick, nullptr);
        }
        if (!revealed.empty()) {
            deck.discardCards(revealed);
            logMessage("  【五谷丰登】剩余的 " + std::to_string(revealed.size()) + " 张牌置入弃牌堆。");
        }
    } else if (subType == CardSubType::JIE_DAO_SHA_REN) {
        if (targets.size() < 2 || !targets[0] || !targets[1]) return false;
        PlayerPtr holder = targets[0];
        PlayerPtr victim = targets[1];
        if (!holder->isAlive() || !holder->getWeapon()) {
            logMessage("  [" + holder->getName() + "] 没有武器，【借刀杀人】无效。");
            return false;
        }
        if (!victim->isAlive()) return false;
        logMessage("  [" + holder->getName() + "] 需对 [" + victim->getName() + "] 使用一张【杀】，否则将武器交给 [" + source->getName() + "]。");
        bool aiWants = !AIController::isFriend(*this, *holder, *victim);
        CardPtr sha = askUseSha(holder, "【借刀杀人】是否对 [" + victim->getName() + "] 使用一张【杀】？（否则交出武器）", aiWants,victim);
        if (sha) {
            logMessage("  [" + holder->getName() + "] 使用了 " + sha->getPublicName());
            resolveSha(holder, sha, {victim});
        } else {
            CardPtr weapon = holder->getWeapon();
            logMessage("  [" + holder->getName() + "] 没有使用【杀】，将武器交给 [" + source->getName() + "]。");
            if (weapon) obtainCard(source, weapon, holder);
        }
    } else if (subType == CardSubType::TAO_YUAN_JIE_YI) {
        for (auto p : getAlivePlayers()) {
            if (p->getHp() < p->getMaxHp()) {
                if (askNullification(source, p, card)) continue;
                recoverHp(p, 1, "桃园结义");
            }
        }
    } else if (subType == CardSubType::LE_BU_SI_SHU) {
        if (!targets.empty() && targets[0]) {
            delayedTrickSources[card.get()]=source->getId();
            targets[0]->addJudgeCard(card);
            logMessage("  将【乐不思蜀】放入了 [" + targets[0]->getName() + "] 的判定区。");
        }
    } else if (subType == CardSubType::BING_LIANG_CUN_DUAN) {
        if (!targets.empty() && targets[0]) {
            delayedTrickSources[card.get()]=source->getId();
            targets[0]->addJudgeCard(card);
            logMessage("  将【兵粮寸断】放入了 [" + targets[0]->getName() + "] 的判定区。");
        }
    } else if (subType == CardSubType::SHAN_DIAN) {
        delayedTrickSources[card.get()]=source->getId();
        source->addJudgeCard(card);
        logMessage("  将【闪电】放入了自己的判定区。");
    }
    // 一张牌结算结束后向全体广播（谋·谦逊、花鬘【象阵】等）。
    for (auto& pp : getAlivePlayers())
        for (auto sk : getEffectiveSkills(*pp)) sk->onCardResolvedByAny(*this, *pp, *source, card);
    // 巨象：只有真正结算后本应进入弃牌堆的【南蛮入侵】才能被获得。
    if (subType == CardSubType::NAN_MAN_RU_QIN && !trickClaimed &&
        !card->getRealCards(card).empty()) {
        for (auto& p : getAlivePlayers()) {
            if (p != source && p->getHero() && (p->getHero()->findSkill("巨象") || p->getHero()->findSkill("谋-巨象"))) {
                for (auto real : card->getRealCards(card)) obtainCard(p,real);
                logMessage("  [" + p->getName() + "] 发动【巨象】获得【南蛮入侵】的实体牌");
                trickClaimed = true;
                break;
            }
        }
    }
    return trickClaimed;
}

// 决斗结算（供【决斗】与【离间】复用）；返回伤害牌是否被认领
bool GameEngine::resolveJueDou(PlayerPtr attacker, PlayerPtr target, CardPtr card,
                                bool targetsNotified) {
    if (gameOver) return false; // 游戏结束后不再有牌结算
    if (!attacker || !target || !target->isAlive()) return false;
    if(!targetsNotified) {
        for(auto skill:getEffectiveSkills(*attacker))skill->onDuelTargeted(*this,*attacker,*attacker,*target);
        for(auto skill:getEffectiveSkills(*target))skill->onDuelTargeted(*this,*target,*attacker,*target);
    }
    logMessage("  [" + attacker->getName() + "] 与 [" + target->getName() + "] 发起决斗！");
    attacker->addMark("决斗已打杀", -attacker->getMark("决斗已打杀"));
    target->addMark("决斗已打杀", -target->getMark("决斗已打杀"));
    // 记录决斗使用者（嵌套结算时恢复外层），供“你使用【决斗】造成伤害”类技能判定。
    struct DuelUserGuard {
        PlayerPtr& slot; PlayerPtr saved;
        DuelUserGuard(PlayerPtr& s, PlayerPtr user) : slot(s), saved(s) { slot = std::move(user); }
        ~DuelUserGuard() { slot = saved; }
    } duelUserGuard(activeDuelUser, attacker);
    PlayerPtr currentTarget = target;
    PlayerPtr currentAttacker = attacker;
    bool claimed = false;

    int duelRounds = 0;
    while (duelRounds++ < 200) {   // F4 挂起防线：决斗轮数兜底（正常远达不到）
        // 无双：每次需打出的【杀】数量
        int need = 1;
        PlayerPtr opponent = (currentTarget == target) ? attacker : target;
        if (opponent) {
            for (const auto& s : getEffectiveSkills(*opponent)) {
                s->onCalculateResponseCount(*this, *opponent, *currentTarget, CardSubType::SHA, need);
            }
        }
        bool answered = true;
        for (int i = 0; i < need; ++i) {
            std::string prompt = "[" + currentAttacker->getName() + "] 与你决斗，请打出一张【杀】";
            if (need > 1) prompt += "（" + std::to_string(i + 1) + "/" + std::to_string(need) + "）";
            if (!askResponseCard(currentTarget, CardSubType::SHA, prompt)) {
                answered = false;
                break;
            }
            currentTarget->addMark("决斗已打杀", 1); // 无双：是否打出过【杀】
        }
        if (answered) {
            std::swap(currentTarget, currentAttacker);
        } else {
            logMessage("  [" + currentTarget->getName() + "] 无法打出【杀】！");
            applyDamage(currentAttacker, currentTarget, 1, ShaElement::NORMAL, false, card, &claimed);
            break;
        }
    }
    return claimed;
}

// AI 是否希望该锦囊"最终被抵消"（按阵营关系判断价值）
namespace {
// B1【无懈可击】时机：只为“关键锦囊”出手，且要算收益（用户 2026-10-05：按优先级完善 AI）。
//   - 敌方受益的锦囊（无中生有/桃园结义）→ 抵消；
//   - 敌方的有害锦囊 → 不掺和（省下【无懈】）；
//   - 自己/队友被控制（乐不思蜀/兵粮寸断）→ 自己被控必挡；队友被控只在其手牌≥2 或是主公时挡；
//   - 自己/队友被拆顺 → 只有确有值得保的牌（装备或手牌≥3）才挡；
//   - 伤害类（决斗/南蛮/万箭/借刀）→ 自己必挡，队友只在体力吃紧（≤2）时挡，血量足就硬吃省牌；
//   - 闪电 → 只在会落到自己或低血队友头上时挡。
bool aiWantsTrickNegated(const GameEngine& engine, const Player& p, const Player& source, const Player& target,
                         const Card& trick) {
    const bool friendOfTarget = AIController::isFriend(engine, p, target);
    const bool friendOfSource = AIController::isFriend(engine, p, source);
    const bool isSelf = (target.getId() == p.getId());
    const bool mine = isSelf || friendOfTarget;
    switch (trick.getSubType()) {
        case CardSubType::WU_GU_FENG_DENG:
            return false; // 五谷人人有份，不值得消耗无懈
        case CardSubType::WU_ZHONG_SHENG_YOU:
            return !friendOfSource; // 不让敌方凭空摸两张
        case CardSubType::TAO_YUAN_JIE_YI:
            return !friendOfTarget; // 不让敌方回血
        case CardSubType::LE_BU_SI_SHU:
        case CardSubType::BING_LIANG_CUN_DUAN:
            if (!mine) return false;                       // 敌方被控正好，不掺和
            if (isSelf) return true;                       // 自己被控一定挡
            return target.getHandCardCount() >= 2 || target.getIdentity() == Identity::ZHU_GONG;
        case CardSubType::GUO_HE_CHAI_QIAO:
        case CardSubType::SHUN_SHOU_QIAN_YANG:
            if (!mine) return false;
            if (isSelf) return true;   // 自己被拆/顺一定挡（哪怕只有一张牌，那也是全部家当）
            // 队友：只有确有值得保的牌（装备，或手牌≥3 张）才花【无懈】
            return !target.getAllEquipment().empty() || target.getHandCardCount() >= 3;
        case CardSubType::JUE_DOU:
        case CardSubType::JIE_DAO_SHA_REN:
        case CardSubType::NAN_MAN_RU_QIN:
        case CardSubType::WAN_JIAN_QI_FA:
            if (!mine) return false;
            return isSelf || target.getHp() <= 2;          // 队友血量足就硬吃，省【无懈】
        case CardSubType::SHAN_DIAN:
            return isSelf || (friendOfTarget && target.getHp() <= 2);
        default:
            if (friendOfSource && friendOfTarget) return false; // 队友之间不掺和
            return friendOfTarget;
    }
}
} // namespace

// 【无懈可击】连锁（官方轮询规则）：
// 每打出一张无懈，锦囊的生效状态翻转一次（偶数张=生效，奇数张=被抵消），
// 随后重新询问所有角色，直到一整轮无人响应。
// AI 按阵营价值 + 当前连锁奇偶决策：只有"打出后令局面更优"才出手，
// 绝不会反制一张恰好替自己挡刀的无懈（旧版会自相矛盾地"救活"针对自己的锦囊）。
bool GameEngine::askNullification(PlayerPtr source, PlayerPtr target, CardPtr trickCard) {
    if (!source || !target || !trickCard) return false;
    int depth = 0;           // 链上已打出的无懈张数
    const int maxDepth = 64; // 安全上限（每张无懈都消耗实体牌，实际远达不到）
    bool playedThisRound = true;
    while (playedThisRound && depth < maxDepth) {
        playedThisRound = false;
        for (auto p : getAlivePlayers()) {
            bool pendingActive = (depth % 2 == 0);
            // 注：不豁免锦囊来源本人——"无懈可击自己使用的锦囊"是合法操作
            //（如黄月英【集智】可借使用锦囊摸牌），由玩家自主选择；
            // AI 端则由 aiWantsTrickNegated 的价值判断自然避免自坑。

            if (p->isAI()) {
                bool wantsNegated = aiWantsTrickNegated(*this, *p, *source, *target, *trickCard);
                // 打出无懈会翻转状态：仅当翻转后的结果对自己阵营更有利时才出手
                if (pendingActive != wantsNegated) continue;
            }

            std::string prompt;
            if (pendingActive) {
                prompt = "[" + source->getName() + "] 对 [" + target->getName() + "] 使用【" + trickCard->getName() +
                         "】，是否打出【无懈可击】抵消？";
            } else {
                prompt = "【" + trickCard->getName() + "】当前已被【无懈可击】抵消，是否打出【无懈可击】反制，令其恢复生效？（已打出 " +
                         std::to_string(depth) + " 张）";
            }
            CardPtr wuxie = askResponseCard(p, CardSubType::WU_XIE_KE_JI, prompt);
            if (wuxie) {
                // AI 队友分辨：打出【无懈可击】抵消锦囊＝帮目标、与来源为敌；反制则相反。
                if (pendingActive) {
                    recordRelation(p->getId(), target->getId(), -2);
                    recordRelation(p->getId(), source->getId(), +1);
                } else {
                    recordRelation(p->getId(), source->getId(), -2);
                    recordRelation(p->getId(), target->getId(), +1);
                }
                depth++;
                // 【无懈可击】是普通锦囊牌，使用时同样触发“使用普通锦囊牌时”类技能（集智等）。
                for (const auto& s : getEffectiveSkills(*p)) s->onUseCard(*this, *p, wuxie);
                logMessage(pendingActive
                    ? "  [" + p->getName() + "] 使用【无懈可击】抵消【" + trickCard->getName() + "】的效果！"
                    : "  [" + p->getName() + "] 使用【无懈可击】反制！【" + trickCard->getName() + "】恢复生效！");
                playedThisRound = true; // 有无懈打出：重新开始一轮询问
                break;
            }
        }
    }
    return depth % 2 == 1; // 奇数张 → 锦囊最终被抵消
}

CardPtr GameEngine::askResponseCard(PlayerPtr player, CardSubType requestedType, const std::string& prompt) {
    if (requestedType==CardSubType::TAO && !canUsePeach(player,activeDying ? activeDying : player))return nullptr;
    if (!player || !player->isAlive()) return nullptr;
    auto candidates = getResponseCandidates(player, requestedType);

    if (candidates.empty()) {
        // 无候选时：同势力借牌响应（如【激将】/【护驾】）
        if (requestedType == CardSubType::SHA || requestedType == CardSubType::SHAN) {
            CardPtr borrowed = nullptr;
            for (const auto& s : getEffectiveSkills(*player)) {
                if (s->onNeedResponseCard(*this, *player, requestedType, borrowed) && borrowed) {
                    if (requestedType == CardSubType::SHA) player->markShaPlayed();
                    return borrowed;
                }
            }
        }
        return nullptr;
    }

    CardPtr chosen = nullptr;
    if (player->isAI()) {
        chosen = AIController::chooseResponseCard(*this, *player, candidates, requestedType);
    } else {
        chosen = askChooseCard(player, candidates, prompt + "（选择要打出的牌）", true);
    }
    if (!chosen) return nullptr;
    chosen = materializeZhangBaSha(player, chosen);
    chosen = materializeConversion(player,chosen);
    if (!chosen) return nullptr;

    logMessage("  [" + player->getName() + "] 打出了 " + chosen->getPublicName());
    if (!resolveGuHuo(player,chosen)) {
        consumeCard(player,chosen,true);
        return nullptr;
    }
    consumeCard(player, chosen, true);
    for (auto& s : getEffectiveSkills(*player)) s->onCardResponded(*this, *player, chosen);
    notifyAnyCardPlayed(player, chosen);
    // 【闪】打出后视为“使用结算结束”，向全场广播（势-符济“使用结算后使用者摸一张牌”等）。
    if (requestedType == CardSubType::SHAN && chosen->getSubType() == CardSubType::SHAN) {
        for (auto& pp : getAlivePlayers())
            for (auto& sk : getEffectiveSkills(*pp))
                sk->onCardResolvedByAny(*this, *pp, *player, chosen);
    }
    if (requestedType == CardSubType::SHA) player->markShaPlayed();
    return chosen;
}

CardPtr GameEngine::askUseSha(PlayerPtr player, const std::string& prompt, bool aiWants,
                                PlayerPtr requiredTarget, bool ignoreDistance, bool optional) {
    if (!player || !player->isAlive()) return nullptr;
    auto candidates = getResponseCandidates(player, CardSubType::SHA);
    if(requiredTarget)
        candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](CardPtr c){
            return !canUseShaOn(*player,*requiredTarget,c,ignoreDistance) ||
                   !canBeTargeted(requiredTarget,c,player);
        }),candidates.end());
    if (candidates.empty()) return nullptr;

    CardPtr chosen = nullptr;
    if (player->isAI()) {
        if (!aiWants) return nullptr;
        // AI 主动使用【杀】时不拆自己的装备去转化（武圣的红色防具/坐骑等）
        std::vector<CardPtr> pool;
        for (const auto& c : candidates) {
            bool usesEquipment = false;
            for (const auto& sub : c->getSubCards()) {
                if (player->hasEquipment(sub)) usesEquipment = true;
            }
            if (!usesEquipment) pool.push_back(c);
        }
        if (pool.empty() && !optional) pool = candidates;
        chosen = AIController::chooseResponseCard(*this, *player, pool, CardSubType::SHA);
    } else {
        chosen = askChooseCard(player, candidates, prompt + "（选择要使用的【杀】）", optional);
    }
    if (!chosen) return nullptr;
    chosen = materializeZhangBaSha(player, chosen);
    chosen = materializeConversion(player,chosen);
    if (!chosen) return nullptr;
    // 使用（非打出）：蛊惑仍须经过质疑，不能绕过普通使用/响应的验真窗口。
    if(!resolveGuHuo(player,chosen)) {
        consumeCard(player,chosen,true);
        return nullptr;
    }
    consumeCard(player, chosen, false);
    for(auto skill:getEffectiveSkills(*player))skill->onCardPlayed(*this,*player,chosen);
    notifyCardUsed(player,chosen);
    player->markShaPlayed();
    return chosen;
}

// ==================== 伤害 / 濒死 / 死亡 ====================

void GameEngine::applyDamage(PlayerPtr source, PlayerPtr target, int damage, ShaElement element, bool ignoreArmor,
                             CardPtr cause, bool* causeClaimed, bool sourceModifiersApplied) {
    if (gameOver) return; // 游戏结束后不再有任何伤害结算
    if (!target || damage <= 0 || !target->isAlive()) return;
    // AI 队友分辨：造成伤害是最强的敌对证据（公开可见）
    if (source && source->getId() != target->getId()) recordRelation(source->getId(), target->getId(), 2);
    turnsWithoutDamage = 0;   // F4：有伤害＝局面在推进
    bool prevInDamage = inDamage;
    inDamage = true;
    struct DamageGuard { GameEngine& e; bool prev; ~DamageGuard(){ e.inDamage = prev; } } damageGuard{*this, prevInDamage};
    if(target->getMark("无前防具失效")>0 ||
       (source && source->getMark("无前目标:"+std::to_string(target->getId()))>0))
        ignoreArmor=true;

    // 转移的是已经计算好来源侧增伤的同一次伤害，不能再次发动裸衣等来源修正。
    if (source && !sourceModifiersApplied) {
        for (const auto& s : getEffectiveSkills(*source)) {
            s->onDealDamage(*this, *source, *target, damage, element, cause);
        }
    }
    // 大雾先防止伤害；不触发「受到伤害时」的其他技能。
    if (target->getMark("雾") > 0 && element != ShaElement::THUNDER) {
        logMessage("  [" + target->getName() + "] 的非雷电伤害被【大雾】防止了。");
        return;
    }
    for (const auto& s : getEffectiveSkills(*target)) {
        s->onTakeDamageFromCard(*this, *target, source.get(), damage, element, cause, causeClaimed);
    }
    if (target->getMark("风") > 0 && element == ShaElement::FIRE) damage += 1;
    // 任意角色即将受伤：全体广播（势·焚涛“传导中的伤害+1”等），在传导记录之前
    for (auto& pp : getAlivePlayers())
        for (auto sk : getEffectiveSkills(*pp))
            sk->onDamageTakingByAny(*this, *pp, source.get(), *target, damage, element, cause);
    const int transmittedDamage = damage;
    // 防具：藤甲（火焰伤害 +1）、白银狮子（伤害至多 1 点）
    if (!ignoreArmor && target->getArmor() && damage > 0) {
        const std::string armorName = target->getArmor()->getName();
        if (armorName == "藤甲" && element == ShaElement::FIRE) {
            damage += 1;
            logMessage("  [" + target->getName() + "] 装备了【藤甲】，受到的火焰伤害 +1！");
        } else if (armorName == "白银狮子" && damage > 1) {
            logMessage("  [" + target->getName() + "] 装备了【白银狮子】，防止了多余的 " + std::to_string(damage - 1) + " 点伤害！");
            damage = 1;
        }
    }
    if (damage <= 0) {
        logMessage("  [" + target->getName() + "] 的伤害被防止了。");
        return;
    }

    // 护甲（谋攻篇机制）：每点护甲吸收1点伤害，独立于防具结算。
    int guard = target->getMark("护甲");
    if (guard > 0 && damage > 0) {
        int absorbed = std::min(guard, damage);
        target->addMark("护甲", -absorbed);
        damage -= absorbed;
        logMessage("  [" + target->getName() + "] 的护甲吸收了 " + std::to_string(absorbed) +
                   " 点伤害（护甲剩余 " + std::to_string(guard - absorbed) + "）。");
        if (damage <= 0) {
            return;
        }
    }

    // 协力“同仇”统计真正穿透防具/护甲的伤害。
    updateXieLiOnDamage(source, damage);

    std::string srcName = source ? source->getName() : "环境/判定";
    logMessage("  💥 [" + target->getName() + "] 受到了来自 [" + srcName + "] 的 " + std::to_string(damage) + " 点伤害！（体力 " +
               std::to_string(target->getHp()) + " → " + std::to_string(target->getHp() - damage) + "）");

    // 属性伤害由横置角色传导一次；先解除本轮所有横置，避免递归传导循环。
    std::vector<PlayerPtr> chainedTargets;
    if (element != ShaElement::NORMAL && target->isChained()) {
        for (auto& p : players) {
            if (p->isAlive() && p->isChained()) {
                p->setChained(false);
                if (p != target) chainedTargets.push_back(p);
            }
        }
    }
    target->changeHp(-damage);
    for(auto skill:getEffectiveSkills(*target)) {
        skill->onHpChanged(*this, *target, -damage);
        skill->onDamageApplied(*this,*target,source.get(),*target,damage,cause);
    }
    if(source && source!=target)
        for(auto skill:getEffectiveSkills(*source))
            skill->onDamageApplied(*this,*source,source.get(),*target,damage,cause);

    if (target->getHp() <= 0) {
        processDying(target, source);
    }

    // 受伤后可认领造成伤害的牌（如【奸雄】）
    if (cause && causeClaimed && !*causeClaimed) {
        for (const auto& s : getEffectiveSkills(*target)) {
            if (s->onClaimDamageCard(*this, *target, source.get(), cause)) {
                *causeClaimed = true;
                logMessage("  [" + target->getName() + "] 发动【" + s->getName() + "】获得了造成伤害的牌");
                break;
            }
        }
    }

    if (target->isAlive()) {
        for (const auto& s : getEffectiveSkills(*target)) {
            s->onAfterDamage(*this, *target, source.get(), damage, element, cause);
        }
    }
    // 造成伤害后：来源侧的对称钩子（如【权谋】/【裸衣】）
    if (source && source->isAlive()) {
        for (const auto& s : getEffectiveSkills(*source)) {
            s->onAfterDealDamage(*this, *source, target.get(), damage, element, cause);
        }
    }
    for (auto& p : players) if (p->isAlive())
        for (auto& s : getEffectiveSkills(*p)) s->onGlobalDamage(*this,*p,source.get(),*target,damage,cause);
    inChainTransmission = true;
    int chainDmg = transmittedDamage + (target->getMark("焚涛传导+1") > 0 ? 1 : 0);
    if (target->getMark("焚涛传导+1") > 0) {
        logMessage("  [势-焚涛] 此次传导中的伤害 +1！");
        target->addMark("焚涛传导+1", -target->getMark("焚涛传导+1"));
    }
    for (auto& p : chainedTargets) {
        if (p->isAlive()) {
            logMessage("  [铁索连环] 属性伤害传导至 [" + p->getName() + "]！");
            // 传导不重复传导；每位被传导者自身的防具/触发技独立结算。
            applyDamage(source, p, chainDmg, element, false, cause);
        }
    }
    if (target->getMark("焚涛重连") > 0) {
        target->addMark("焚涛重连", -target->getMark("焚涛重连"));
        if (target->isAlive()) target->setChained(true);
    }
    inChainTransmission = false;
}

// B5 濒死救助优先级（用户 2026-10-05：按优先级完善 AI）：
//   队友一定救；内奸/野心家没有队友，但要**维持平衡**——主公濒死且反贼仍在（主忠方将崩）时救主公，
//   其余情况不救（让双方互耗）；【完杀】等“合法救助者”限制由 canUsePeach 处理。
bool GameEngine::aiWantsToSave(const Player& saver, const Player& dying) const {
    if (AIController::isFriend(*this, saver, dying)) return true;
    const Identity mine = saver.getIdentity();
    if (mine != Identity::NEI_JIAN && mine != Identity::YE_XIN_JIA) return false;
    if (dying.getId() == saver.getId()) return true;      // 自救
    if (dying.getIdentity() != Identity::ZHU_GONG) return false; // 反贼/忠臣濒死：内奸不救
    int loyal = 0, rebels = 0;
    aiEstimateSides(saver, loyal, rebels);
    return rebels > 0 && loyal <= rebels;                 // 主忠方将崩 → 救主公维持平衡
}

void GameEngine::processDying(PlayerPtr dyingPlayer, PlayerPtr source) {
    if (gameOver) return; // 游戏结束后不再进入濒死结算
    auto previousDying=activeDying;
    activeDying=dyingPlayer;
    // 记录濒死来源供“你令其濒死”等技能精确判定（原文：当你令一名角色进入濒死状态时）
    // 以 mark 形式暴露给 onOtherDying，避免修改 Skill 接口破坏既有约定
    int srcMark = source ? source->getId() + 1 : 0;
    int prevSrcMark = dyingPlayer->getMark("濒死来源");
    // 以差值方式设置当前濒死来源
    dyingPlayer->addMark("濒死来源", srcMark - prevSrcMark);
    auto leave=[&]() {
        if(dyingPlayer->isAlive() && dyingPlayer->getHp()>0)
            for(auto skill:getEffectiveSkills(*dyingPlayer))skill->onLeaveDying(*this,*dyingPlayer);
        // 恢复之前的濒死来源标记
        dyingPlayer->addMark("濒死来源", prevSrcMark - dyingPlayer->getMark("濒死来源"));
        activeDying=previousDying;
    };
    logMessage("  🆘 [" + dyingPlayer->getName() + "] 进入濒死状态 (当前 HP: " + std::to_string(dyingPlayer->getHp()) + ")！");

    // 庞统涅槃等濒死技必须早于求桃结算。
    for (auto& s : getEffectiveSkills(*dyingPlayer)) {
        if (dyingPlayer->getHp() > 0) break;
        s->onDying(*this, *dyingPlayer, *dyingPlayer);
    }
    if (dyingPlayer->getHp() > 0) {leave();return;}
    // 其他角色的濒死触发（如谋·贾诩【完杀】观看手牌与二择）
    for (auto& pp : getAlivePlayers()) {
        if (pp->getId() == dyingPlayer->getId()) continue;
        for (auto sk : getEffectiveSkills(*pp)) sk->onOtherDying(*this, *pp, *dyingPlayer);
        if (dyingPlayer->getHp() > 0) {leave();return;}
    }
    // 从濒死角色自己开始，按座次依次询问
    auto alive = getAlivePlayers();
    size_t start = 0;
    for (size_t i = 0; i < alive.size(); ++i) {
        if (alive[i]->getId() == dyingPlayer->getId()) { start = i; break; }
    }
    for (size_t k = 0; k < alive.size() && dyingPlayer->getHp() <= 0; ++k) {
        auto p = alive[(start + k) % alive.size()];
        if (!p->isAlive()) continue;
        // 【酒】自救（官方文本第二句：当你处于濒死状态时，对自己使用，回复1点体力）。
        // 仅濒死者本人可用，且不受【完杀】/【克己】对【桃】的限制影响。
        if (p->getId() == dyingPlayer->getId() && dyingPlayer->getHp() <= 0) {
            int jiuUses = 0; // 安全上限：避免异常转化牌导致死循环
            while (dyingPlayer->getHp() <= 0 && p->isAlive() && jiuUses++ < 20) {
                CardPtr jiu = askResponseCard(p, CardSubType::JIU,
                    "[" + dyingPlayer->getName() + "] 处于濒死状态，是否使用【酒】回复1点体力？");
                if (!jiu) break;
                logMessage("  [" + p->getName() + "] 使用【酒】回复了 1 点体力（濒死自救）。");
                recoverHp(dyingPlayer, 1, "酒", p);
            }
        }
        if (!canUsePeach(p, dyingPlayer)) continue;
        if (p->isAI() && !aiWantsToSave(*p, *dyingPlayer)) continue;
        while (dyingPlayer->getHp() <= 0 && p->isAlive()) {
            std::string prompt = "[" + dyingPlayer->getName() + "] 处于濒死状态，请问是否使用【桃】救治？";
            CardPtr tao = askResponseCard(p, CardSubType::TAO, prompt);
            if (tao) {
                logMessage("  [" + p->getName() + "] 对 [" + dyingPlayer->getName() + "] 使用了【桃】！");
                recoverHp(dyingPlayer,(tao->getSkillSource()=="龙魂" && tao->getSubCards().size()==2)?2:1,"桃",p);
                for (auto& pp : getAlivePlayers())
                    for (auto sk : getEffectiveSkills(*pp)) sk->onAnyPeachUsed(*this, *pp, *p, *dyingPlayer);
            } else {
                break;
            }
        }
    }

    // 若被桃救起部分体力，移去相应多出的创；创的数量必须对应剩余的非正体力值。
    if(dyingPlayer->getHero() && dyingPlayer->getHero()->findSkill("不屈")) {
        int needed=std::max(0,1-dyingPlayer->getHp());
        auto wounds=dyingPlayer->getPile("创");
        while(static_cast<int>(wounds.size())>needed) {
            auto c=wounds.back();wounds.pop_back();
            dyingPlayer->removeFromPile("创",c);
            deck.discardCard(c);
        }
    }
    // 不屈不会回复体力：救援后仍处于 0 以下，但足量且点数不同的创使其存活。
    bool buquSurvives=false;
    if(dyingPlayer->getHp()<=0 && dyingPlayer->getHero()) {
        for(auto skill:getEffectiveSkills(*dyingPlayer))if(skill->getName()=="不屈") {
            auto wounds=dyingPlayer->getPile("创");
            std::set<int> ranks;
            for(auto card:wounds)ranks.insert(card->getRank());
            buquSurvives=static_cast<int>(wounds.size())>=1-dyingPlayer->getHp() &&
                          ranks.size()==wounds.size();
        }
    }
    if (dyingPlayer->getHp() <= 0 && !buquSurvives) {
        handlePlayerDeath(dyingPlayer, source);
    } else if(dyingPlayer->getHp()>0) {
        logMessage("  [" + dyingPlayer->getName() + "] 脱离了濒死状态，当前体力: " + std::to_string(dyingPlayer->getHp()));
    }
    leave();
}

bool GameEngine::canUsePeach(PlayerPtr actor, PlayerPtr recipient) const {
    if (!actor) return false;
    // 谋·吕蒙【克己】：若你不处于濒死状态，你无法使用【桃】
    for (const auto& sk : getEffectiveSkills(*actor)) if (sk->getName()=="谋-克己" && actor->getHp()>0) return false;
    auto current=getCurrentPlayer();
    if (!current || !isPlayerTurn(*current) || !current->isAlive() || !current->getHero() ||
        !(current->getHero()->findSkill("完杀") || current->getHero()->findSkill("谋-完杀"))) return true;
    bool enabled=false;
    for(const auto& skill:getEffectiveSkills(*current))if(skill->getName()=="完杀"||skill->getName()=="谋-完杀")enabled=true;
    if(!enabled)return true;
    // 只有当前回合的完杀持有者与正在濒死的本人可使用桃；
    // “给自己用桃”本身不足以绕开“处于濒死状态”的限制。
    return actor==current || (actor==recipient && activeDying==recipient && recipient && recipient->getHp()<=0);
}

void GameEngine::killPlayer(PlayerPtr victim, PlayerPtr source) {
    if(!victim || !victim->isAlive())return;
    victim->setHp(0);
    handlePlayerDeath(victim,source);
}

void GameEngine::handlePlayerDeath(PlayerPtr deadPlayer, PlayerPtr killer) {
    deadPlayer->setAlive(false);
    turnsWithoutDamage = 0;   // F4：有死亡＝局面在推进
    turnsWithoutDeath = 0;
    roundHadDeath = true;
    logMessage("\n💀 [" + deadPlayer->getName() + "] 阵亡！身份是: [" + deadPlayer->getIdentityString() + "]");

    // 【立储】储君继位（用户 2026-10-05）：主公阵亡时，若储君存活且为忠臣，则继位成为新的主公，
    // 延续主忠方的游戏——必须在胜负判定之前处理，否则“主公阵亡”会立即结束对局。
    if (deadPlayer->getIdentity() == Identity::ZHU_GONG) {
        for (auto& p : players) {
            if (!p->isAlive() || p->getMark("储君") <= 0) continue;
            if (p->getIdentity() != Identity::ZHONG_CHEN) {
                logMessage("  【立储】储君 " + p->getName() + "（" + p->getIdentityString() +
                           "）不是忠臣，无法继位。");
                break;
            }
            p->setIdentity(Identity::ZHU_GONG);
            p->addMark("储君", -p->getMark("储君"));
            logMessage("  【立储】主公阵亡，储君 " + p->getName() + "（忠臣）继位成为新的主公，主忠方延续！");
            grantLordSkills(p); // 继位者获得自己武将的主公技（若有）——“只要有主公技就能用”
            if (!p->getHero()->findSkill("立储")) applyIdentityModeSkills(*this, *p);
            break;
        }
    }

    // 规则（用户 2026-10-04）：胜利优先级大于一切——死亡一旦满足胜负条件，立即结束，
    // 不再触发“死亡时”技能、击杀奖励与其他任何结算。
    checkGameOver();
    if (gameOver) {
        cleanupDeadPlayerZones(deadPlayer); // 仅清理区域（不触发技能），保证状态一致
        logMessage("  游戏结束（" + winningFaction + "）：胜利优先，死亡时技能与奖励不再结算。");
        return;
    }

    // 死亡结算窗口：仅在此期间允许死者本人的“死亡时”技能（断肠/行殇/武魂等）触发。
    // 保存/恢复以支持重入（如【武魂】在死亡结算中再造成死亡）。
    const int prevResolvingDeath = resolvingDeathPlayerId;
    resolvingDeathPlayerId = deadPlayer->getId();
    // 拍在弃牌前：行殇获取死亡者区域牌；断肠清空来源技能。
    for (auto& p : players) {
        if (gameOver) break; // 胜利优先：一旦分出胜负，死亡结算窗口立即关闭
        if (!p->isAlive() && p != deadPlayer) continue;
        for (auto& s : getEffectiveSkills(*p)) {
            if (gameOver) break;
            s->onPlayerDeath(*this, *p, *deadPlayer, killer.get());
        }
    }
    resolvingDeathPlayerId = prevResolvingDeath;

    cleanupDeadPlayerZones(deadPlayer);

    if (killer && killer->isAlive()) {
        if (deadPlayer->getIdentity() == Identity::FAN_ZEI) {
            logMessage("  [" + killer->getName() + "] 击杀了反贼，获得奖励摸 3 张牌！");
            drawCards(killer, 3, "击杀反贼");
        } else if (deadPlayer->getIdentity() == Identity::ZHONG_CHEN && killer->getIdentity() == Identity::ZHU_GONG) {
            logMessage("  主公 [" + killer->getName() + "] 亲手误杀了忠臣，惩罚弃置所有手牌和装备！");
            std::vector<CardPtr> killerHand = killer->getHandCards();
            for (auto& c : killerHand) discardCardOf(killer, c, "误杀忠臣");
            auto killerEquip = killer->getAllEquipment();
            for (auto& c : killerEquip) discardCardOf(killer, c, "误杀忠臣");
        }
    }

    checkGameOver(); // 兜底：死亡奖励/技能可能引发新的死亡（如行殇后的连锁）
}

// 阵亡者区域清理：离场牌进弃牌堆、移除装备/判定区牌；不触发任何技能（死者技能不再发动）。
void GameEngine::cleanupDeadPlayerZones(PlayerPtr deadPlayer) {
    if (!deadPlayer) return;
    // 死者即便此前非锁定技失效，持续标记也不能留在场上。
    if (deadPlayer->getHero()) {
        auto skills = deadPlayer->getHero()->getSkills();
        for (auto skill : skills) skill->onRemoved(*this, *deadPlayer);
    }
    auto cards = deadPlayer->getAllCards();
    deck.discardCards(cards);
    deck.discardCards(deadPlayer->takeAllPiles());
    std::vector<CardPtr> hand = deadPlayer->getHandCards();
    for (auto& c : hand) deadPlayer->removeHandCard(c);
    deadPlayer->removeEquipment(CardSubType::WEAPON);
    deadPlayer->removeEquipment(CardSubType::ARMOR);
    deadPlayer->removeEquipment(CardSubType::OFFENSIVE_HORSE);
    deadPlayer->removeEquipment(CardSubType::DEFENSIVE_HORSE);
    std::vector<CardPtr> judge = deadPlayer->getJudgeZone();
    for (auto& c : judge) { deadPlayer->removeJudgeCard(c); delayedTrickSources.erase(c.get()); }
}

void GameEngine::checkGameOver() {
    bool wasOver = gameOver;
    if (gameMode == GameMode::DOUDIZHU) Roles::checkDoudizhuGameOver(players, gameOver, winningFaction);
    else Roles::checkGameOver(players, gameOver, winningFaction);
    if (gameMode == GameMode::DOUDIZHU && !wasOver && gameOver) logDoudizhuSettlement();
}

void GameEngine::startXieLi(PlayerPtr initiator, PlayerPtr partner, const std::string& skill) {
    if (!initiator || !partner) return;
    // 同一对已存在未结算的同一技能则不重复
    for (auto& r : xieLiRecords) if (!r.resolved && r.initiatorId==initiator->getId() && r.partnerId==partner->getId() && r.skill==skill) return;
    XieLiRecord rec;
    rec.initiatorId = initiator->getId();
    rec.partnerId = partner->getId();
    rec.skill = skill;
    xieLiRecords.push_back(rec);
    logMessage("  [协力] " + initiator->getName() + " 与 " + partner->getName() + " 以“" + skill + "”进行协力。");
}
bool GameEngine::isXieLiSuccess(const XieLiRecord& r) const {
    if (r.damage >= 4) return true;
    if (r.draws >= 8) return true;
    if ((int)r.discardSuits.size() >= 4) return true;
    if ((int)r.useSuits.size() >= 4) return true;
    return false;
}
void GameEngine::updateXieLiOnDamage(PlayerPtr source, int dmg) {
    if (!source || dmg<=0) return;
    auto cur = getCurrentPlayer();
    if (!cur) return;
    for (auto& r : xieLiRecords) if (!r.resolved && r.partnerId==cur->getId()) {
        if (source->getId()==r.initiatorId || source->getId()==r.partnerId) r.damage += dmg;
    }
}
void GameEngine::updateXieLiOnDraw(PlayerPtr who, int n) {
    if (!who || n<=0) return;
    auto cur = getCurrentPlayer();
    if (!cur) return;
    for (auto& r : xieLiRecords) if (!r.resolved && r.partnerId==cur->getId()) {
        if (who->getId()==r.initiatorId || who->getId()==r.partnerId) r.draws += n;
    }
}
void GameEngine::updateXieLiOnDiscard(PlayerPtr who, Suit suit) {
    if (!who || suit==Suit::NONE) return;
    auto cur = getCurrentPlayer();
    if (!cur) return;
    for (auto& r : xieLiRecords) if (!r.resolved && r.partnerId==cur->getId()) {
        if (who->getId()==r.initiatorId || who->getId()==r.partnerId) r.discardSuits.insert(suit);
    }
}
void GameEngine::updateXieLiOnUse(PlayerPtr who, Suit suit) {
    if (!who || suit==Suit::NONE) return;
    auto cur = getCurrentPlayer();
    if (!cur) return;
    for (auto& r : xieLiRecords) if (!r.resolved && r.partnerId==cur->getId()) {
        if (who->getId()==r.initiatorId || who->getId()==r.partnerId) r.useSuits.insert(suit);
    }
}
void GameEngine::resolveXieLiAtTurnEnd(PlayerPtr turnOwner) {
    if (!turnOwner) return;
    for (auto& r : xieLiRecords) if (!r.resolved && r.partnerId==turnOwner->getId()) {
        bool ok = isXieLiSuccess(r);
        PlayerPtr init = getPlayerById(r.initiatorId);
        if (!init || !init->isAlive()) { r.resolved=true; continue; }
        if (ok) {
            // 按技能分发成功标记
            if (r.skill=="协击") init->addMark("协击成功",1);
            else if (r.skill=="积著") {
                // 强化龙胆：由 MouJiZhu 的 onTurnEnd 原逻辑改为在此标记
                // 此处仅标记，后续 MouJiZhu 在 turnEnd 中再消费
                init->addMark("积著协力成功",1);
            }
            logMessage("  [协力] " + init->getName() + " 与 " + turnOwner->getName() + " 的“" + r.skill + "”协力成功！（伤害"+std::to_string(r.damage)+" 摸牌"+std::to_string(r.draws)+" 弃花色"+std::to_string(r.discardSuits.size())+" 用花色"+std::to_string(r.useSuits.size())+"）");
        } else {
            logMessage("  [协力] " + init->getName() + " 与 " + turnOwner->getName() + " 的“" + r.skill + "”协力失败。");
        }
        r.resolved=true;
    }
    // 清理已结算
    xieLiRecords.erase(std::remove_if(xieLiRecords.begin(), xieLiRecords.end(), [](const XieLiRecord& r){return r.resolved;}), xieLiRecords.end());
}

} // namespace Thks
