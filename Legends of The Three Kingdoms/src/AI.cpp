#include "AI.h"
#include "Player.h"
#include "GameEngine.h"
#include "CardTracker.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace Thks {

bool AIController::isFriend(const Player& self, const Player& other) {
    if (self.getId() == other.getId()) return true;

    Identity s = self.getIdentity();
    Identity o = other.getIdentity();

    // 斗地主：地主单方，两名农民互为同伙。
    if (s == Identity::DI_ZHU) return (o == Identity::DI_ZHU);
    if (s == Identity::NONG_MIN) return (o == Identity::NONG_MIN);
    if (s == Identity::ZHU_GONG || s == Identity::ZHONG_CHEN) {
        return (o == Identity::ZHU_GONG || o == Identity::ZHONG_CHEN);
    }
    if (s == Identity::FAN_ZEI) {
        return (o == Identity::FAN_ZEI);
    }
    return false;
}

namespace {

// B6：判定结果的“期望类型”
enum class JudgeWant {
    DontCare,     // 不改
    Red, Black,   // 颜色
    Heart, Club,  // 具体花色
    NotHeart, NotClub, NotRed,
    Spade29,      // 黑桃 2~9（闪电命中）
    NotSpade29,   // 避开闪电
    Spade,        // 黑桃（雷击）
    NotSpade,
};

bool judgeCardSatisfies(GameEngine& engine, const Player& owner, const CardPtr& c, JudgeWant w) {
    if (!c) return false;
    const Suit s = engine.effectiveSuit(owner, c);   // 按“生效花色”算（红颜/谋-红颜等会改花色）
    const int r = c->getRank();
    const bool red = (s == Suit::HEART || s == Suit::DIAMOND);
    const bool black = (s == Suit::SPADE || s == Suit::CLUB);
    switch (w) {
        case JudgeWant::Red: return red;
        case JudgeWant::Black: return black;
        case JudgeWant::Heart: return s == Suit::HEART;
        case JudgeWant::Club: return s == Suit::CLUB;
        case JudgeWant::NotHeart: return s != Suit::HEART;
        case JudgeWant::NotClub: return s != Suit::CLUB;
        case JudgeWant::NotRed: return !red;
        case JudgeWant::Spade: return s == Suit::SPADE;
        case JudgeWant::NotSpade: return s != Suit::SPADE;
        case JudgeWant::Spade29: return s == Suit::SPADE && r >= 2 && r <= 9;
        case JudgeWant::NotSpade29: return !(s == Suit::SPADE && r >= 2 && r <= 9);
        default: return true;
    }
}

} // namespace

CardPtr AIController::chooseJudgementReplacement(GameEngine& engine, Player& self, const Player& judgeTarget,
                                                const CardPtr& judgeCard, bool blackOnly,
                                                const std::vector<CardPtr>& pool) {
    if (!judgeCard) return nullptr;
    const bool mine = (judgeTarget.getId() == self.getId()) || isFriend(engine, self, judgeTarget);
    const std::string reason = engine.getJudgeReason();
    // 该判定的“成功”对 judgeTarget 是好是坏 → 决定我方希望看到什么牌
    JudgeWant want = JudgeWant::DontCare;
    if (reason == "乐不思蜀")            want = mine ? JudgeWant::Heart : JudgeWant::NotHeart;      // 红桃＝判定失效
    else if (reason == "兵粮寸断")       want = mine ? JudgeWant::Club : JudgeWant::NotClub;        // 草花＝判定失效
    else if (reason == "闪电")           want = mine ? JudgeWant::NotSpade29 : JudgeWant::Spade29;  // 黑桃2~9＝中闪电
    else if (reason == "八卦阵")         want = mine ? JudgeWant::Red : JudgeWant::Black;           // 红＝视为打出【闪】
    else if (reason == "洛神")           want = mine ? JudgeWant::Black : JudgeWant::Red;           // 黑＝获得判定牌
    else if (reason == "刚烈")           want = mine ? JudgeWant::NotHeart : JudgeWant::Heart;      // 非红桃＝伤害来源付代价
    else if (reason == "雷击")           want = mine ? JudgeWant::NotSpade : JudgeWant::Spade;      // 黑桃＝受 2 点雷伤
    else if (reason == "铁骑")           want = mine ? JudgeWant::Black : JudgeWant::Red;           // 红＝【杀】不可被闪避
    else return nullptr;                                                                             // 未知判定：不乱改
    if (judgeCardSatisfies(engine, judgeTarget, judgeCard, want)) return nullptr; // 现状已是我们要的 → 不浪费牌

    CardPtr best;
    int bestVal = 1 << 30;
    const std::vector<CardPtr> hand = self.getHandCards();
    const auto& src = pool.empty() ? hand : pool;
    for (const auto& c : src) {
        if (!c) continue;
        const Suit cs = engine.effectiveSuit(self, c);
        if (blackOnly && !(cs == Suit::SPADE || cs == Suit::CLUB)) continue;
        if (!judgeCardSatisfies(engine, self, c, want)) continue;
        int v = cardValue(engine, self, c);
        if (v < bestVal) { bestVal = v; best = c; }
    }
    if (!best) return nullptr;
    // 代价控制：不为“别人”的判定搭上一张关键牌（【桃】级），除非是自己或主公的生死判定
    const bool critical = (judgeTarget.getId() == self.getId()) ||
                          (mine && judgeTarget.getIdentity() == Identity::ZHU_GONG) ||
                          reason == "闪电";
    if (bestVal >= 9 && !critical) return nullptr;   // 9＝【桃】级关键牌
    return best;
}

// 推断版：全部委托给引擎的公开信息 + 行为证据（见 GameEngine::aiIsFriend）
bool AIController::isFriend(const GameEngine& engine, const Player& self, const Player& other) {
    return engine.aiIsFriend(self, other);
}

int AIController::sideEstimate(const GameEngine& engine, const Player& other) {
    return engine.aiSideEstimate(other);
}

// ==================== 局面评估共用底座（A/B/C/D/E/F 各项共用） ====================

const AIController::Weights& AIController::weights() {
    static const Weights w;
    return w;
}

std::vector<PlayerPtr> AIController::enemiesOf(GameEngine& engine, const Player& self) {
    std::vector<PlayerPtr> out;
    for (const auto& p : engine.getOtherAlivePlayers(self))
        if (p && !isFriend(engine, self, *p)) out.push_back(p);
    return out;
}

std::vector<PlayerPtr> AIController::friendsOf(GameEngine& engine, const Player& self) {
    std::vector<PlayerPtr> out;
    for (const auto& p : engine.getOtherAlivePlayers(self))
        if (p && isFriend(engine, self, *p)) out.push_back(p);
    return out;
}

int AIController::enemyCount(GameEngine& engine, const Player& self) {
    return static_cast<int>(enemiesOf(engine, self).size());
}

int AIController::friendCount(GameEngine& engine, const Player& self) {
    return static_cast<int>(friendsOf(engine, self).size());
}

bool AIController::hasEnemy(GameEngine& engine, const Player& self) {
    return !enemiesOf(engine, self).empty();
}

// A1：手里的“【杀】来源”张数＝实体【杀】+ 各技能可转化的牌（武圣/龙胆/酒池…）+ 丈八蛇矛
int AIController::shaSourceCount(GameEngine& engine, const Player& self) {
    if (self.isHandCardsBanned()) return 0;
    Player& mutableSelf = const_cast<Player&>(self);
    int n = 0;
    for (const auto& c : self.getHandCards()) {
        if (!c) continue;
        if (c->getSubType() == CardSubType::SHA) { ++n; continue; }
        for (const auto& sk : engine.getEffectiveSkills(mutableSelf)) {
            if (sk && sk->convertCard(engine, mutableSelf, c, CardSubType::SHA)) { ++n; break; }
        }
    }
    if (GameEngine::hasWeapon(self, "丈八蛇矛")) n += self.getHandCardCount() / 2; // 两张当一张（近似）
    return n;
}

// A2：这一次【杀】预计造成的伤害（含【酒】/裸衣/烈弓/战烈/古锭刀，以及藤甲的免疫与加伤）
int AIController::estimatedShaDamage(GameEngine& engine, const Player& self, const Player& target) {
    int dmg = 1;
    const bool hasFireSha = [&] {
        for (const auto& c : self.getHandCards())
            if (c && c->getSubType() == CardSubType::SHA && c->getShaElement() == ShaElement::FIRE) return true;
        return false;
    }();
    if (GameEngine::hasArmor(target, "藤甲")) {
        if (!hasFireSha) return 0; // 普通【杀】对藤甲无效
        dmg += 1;                  // 火【杀】对藤甲 +1
    }
    if (self.isDrunk()) dmg += 1;                       // 【酒】：下一张【杀】伤害 +1
    if (self.getMark("裸衣") > 0) dmg += 1;             // 许褚【裸衣】
    if (self.getMark("战烈") > 0) dmg += 1;             // 势·太史慈【战烈】
    if (self.getHero() && self.getHero()->findSkill("烈弓") && target.getHp() <= self.getHp()) dmg += 1; // 近似
    if (GameEngine::hasWeapon(self, "古锭刀") && target.getHandCardCount() == 0) dmg += 1;
    (void)engine;
    return dmg;
}

// A1+A2：现有手牌能否斩杀目标（含咆哮/丈八/转化带来的多刀）
bool AIController::canKillWithSha(GameEngine& engine, const Player& self, const Player& target, bool assumeDrunk) {
    int first = estimatedShaDamage(engine, self, target);
    if (first <= 0) return false;
    if (assumeDrunk && !self.isDrunk()) first += 1;   // 先喝酒再出杀
    if (first >= target.getHp()) return true;
    const int per = std::max(1, first - ((self.isDrunk() || assumeDrunk) ? 1 : 0)); // 【酒】只加第一刀
    int shots = shaSourceCount(engine, self);
    const int limit = engine.getShaLimit(self) - self.getShaCountThisTurn();
    if (limit >= 0 && shots > limit) shots = limit;   // 咆哮等会把上限放到极大
    if (shots <= 1) return false;
    return first + (shots - 1) * per >= target.getHp();
}

// A6：卖血流（打死/打伤他反而亏牌）
bool AIController::isSellBloodHero(const Player& target) {
    if (!target.getHero()) return false;
    static const char* kNames[] = {"反馈", "遗计", "节命", "狂骨", "恩怨", "悲歌", "断肠", "行殇",
                                   "谋-恩怨", "谋-反馈", "谋-遗计", "势-悲歌"};
    for (const char* n : kNames) if (target.getHero()->findSkill(n)) return true;
    return false;
}

// A5/B6：能改判定的角色（延时锦囊对其收益低）
bool AIController::canAlterJudgement(GameEngine& engine, const Player& target) {
    for (const auto& sk : engine.getEffectiveSkills(target)) {
        if (!sk) continue;
        const std::string n = sk->getName();
        if (n.find("鬼才") != std::string::npos || n.find("鬼道") != std::string::npos) return true;
    }
    return false;
}

// E1：牌堆里还剩多少张某名牌（记牌器口径：只看牌堆，不偷看别人手牌）
int AIController::remainingCardsByName(GameEngine& engine, const std::string& cardName) {
    int total = 0;
    for (const auto& e : CardTracker::buildEntries(engine.getDeck()))
        if (e.name == cardName) total += e.count;
    return total;
}

// E3：本方集火目标（同一方 AI 尽量打同一个）
std::shared_ptr<Player> AIController::focusTarget(GameEngine& engine, const Player& self) {
    auto foes = enemiesOf(engine, self);
    if (foes.empty()) return nullptr;
    PlayerPtr best;
    int bestScore = -(1 << 30);
    for (const auto& t : foes) {
        if (!t || !t->isAlive()) continue;
        int score = 0;
        if (canKillWithSha(engine, self, *t)) score += weights().killBonus;
        score += weights().hpBase - t->getHp() * weights().hpFactor;
        if (t->getIdentity() == Identity::ZHU_GONG &&
            (self.getIdentity() == Identity::FAN_ZEI || self.getIdentity() == Identity::NEI_JIAN))
            score += weights().lordFocusBonus; // 反贼/内奸优先压主公
        if (isSellBloodHero(*t)) score -= 30;  // 卖血流不值得集火
        // 同分取座位小者，保证决策确定性（可复现、可测试）
        if (score > bestScore || (score == bestScore && best && t->getId() < best->getId())) {
            bestScore = score;
            best = t;
        }
    }
    return best;
}

bool AIController::isDuelEndgame(GameEngine& engine) {
    return static_cast<int>(engine.getAlivePlayers().size()) <= weights().endgameAlone;
}

int AIController::cardValue(const CardPtr& card) {
    if (!card) return 0;
    if (card->isVirtual()) {
        int v = 0;
        for (const auto& c : card->getSubCards()) v = std::max(v, cardValue(c));
        return v;
    }
    switch (card->getSubType()) {
        case CardSubType::TAO:                return 10; // 保命，永远最优先
        case CardSubType::WU_ZHONG_SHENG_YOU: return 8;  // 净+1牌差的顶级过牌
        case CardSubType::WU_XIE_KE_JI:       return 7;
        case CardSubType::SHUN_SHOU_QIAN_YANG:return 7;  // 稳赚一张牌的强力锦囊
        case CardSubType::GUO_HE_CHAI_QIAO:   return 6;
        case CardSubType::MANTIAN_GUOHAI:     return 5;  // 有选择地换走目标区域的一张牌
        case CardSubType::JUE_DOU:            return 6;
        case CardSubType::NAN_MAN_RU_QIN:     return 6;
        case CardSubType::WAN_JIAN_QI_FA:     return 6;
        case CardSubType::SHAN:               return 5;  // 普通牌：纯防御
        case CardSubType::LE_BU_SI_SHU:       return 5;
        case CardSubType::JIE_DAO_SHA_REN:    return 5;
        case CardSubType::WU_GU_FENG_DENG:    return 5;
        case CardSubType::TAO_YUAN_JIE_YI:    return 5;
        case CardSubType::SHA:                return 4;  // 普通牌：要距离/目标/次数
        case CardSubType::WEAPON:
        case CardSubType::ARMOR:
        case CardSubType::OFFENSIVE_HORSE:
        case CardSubType::DEFENSIVE_HORSE:    return equipmentBaseValue(card);
        case CardSubType::BING_LIANG_CUN_DUAN:return 4;
        case CardSubType::JIU:                return 3;  // AI 不会喝酒，基本用不出去
        case CardSubType::SHAN_DIAN:          return 2;
        default:                              return 3;
    }
}

int AIController::equipmentBaseValue(const CardPtr& card) {
    if (!card) return 0;
    switch (card->getSubType()) {
        case CardSubType::OFFENSIVE_HORSE:
        case CardSubType::DEFENSIVE_HORSE:
            return 3; // 坐骑：同功能完全等价，统一定价
        case CardSubType::WEAPON: {
            const std::string& n = card->getName();
            if (n == "诸葛连弩") return 6;                       // 无限出杀，质变
            if (n == "青龙偃月刀" || n == "贯石斧" || n == "麒麟弓") return 5;
            return 4; // 丈八蛇矛/方天画戟/青釭剑/寒冰剑及未知武器
        }
        case CardSubType::ARMOR: {
            const std::string& n = card->getName();
            if (n == "八卦阵" || n == "仁王盾") return 5;
            return 4; // 白银狮子/藤甲及未知防具
        }
        default:
            return 3;
    }
}

namespace {
// 同槽位去重：只有优先级最高的那张保留原始价值，其余一律判 2（多余）。
// 优先级即基础价值；已装备的牌先于手牌，同价值时先占据者保留。
int equipmentContextValue(const Player& self, const CardPtr& card) {
    int base = AIController::equipmentBaseValue(card);
    auto sameSlot = [&](const CardPtr& c) {
        return c && c->getSubType() == card->getSubType();
    };
    CardPtr equipped;
    switch (card->getSubType()) {
        case CardSubType::WEAPON:          equipped = self.getWeapon(); break;
        case CardSubType::ARMOR:           equipped = self.getArmor(); break;
        case CardSubType::OFFENSIVE_HORSE: equipped = self.getOffensiveHorse(); break;
        case CardSubType::DEFENSIVE_HORSE: equipped = self.getDefensiveHorse(); break;
        default: break;
    }
    const auto& hand = self.getHandCards();
    int selfIdx = -1;
    for (int i = 0; i < static_cast<int>(hand.size()); ++i) {
        if (hand[i] == card) { selfIdx = i; break; }
    }
    bool selfIsEquipped = (equipped == card);
    for (int i = 0; i < static_cast<int>(hand.size()); ++i) {
        const CardPtr& h = hand[i];
        if (h == card || !sameSlot(h)) continue;
        int ov = AIController::equipmentBaseValue(h);
        if (ov > base) return 2; // 有严格更优的同槽牌
        bool precedes = (selfIdx >= 0) ? (i < selfIdx) : !selfIsEquipped;
        if (ov == base && precedes) return 2; // 同值先占据者保留
    }
    if (equipped && equipped != card) {
        int ov = AIController::equipmentBaseValue(equipped);
        if (ov >= base) return 2; // 已装备者恒先于手牌/亮出牌
    }
    return base;
}
} // namespace

CardPtr AIController::chooseLeastValuableCard(const std::vector<CardPtr>& cards) {
    CardPtr best = nullptr;
    for (const auto& c : cards) {
        if (!best || cardValue(c) < cardValue(best)) best = c;
    }
    return best;
}

// 结合局面的估值：当前用不出去的牌大幅降值（与 makePlayDecision 的出牌条件镜像）
int AIController::cardValue(GameEngine& engine, Player& self, const CardPtr& card) {
    if (!card) return 0;
    if (card->isVirtual()) {
        int v = 0;
        for (const auto& c : card->getSubCards()) v = std::max(v, cardValue(engine, self, c));
        return v;
    }
    switch (card->getSubType()) {
        case CardSubType::JIE_DAO_SHA_REN: {
            // 有敌方武器可借、且能让其去杀另一名敌人，才是好牌
            for (auto holder : engine.getJieDaoWeaponHolders(self)) {
                if (isFriend(engine, self, *holder) ||
                    !engine.canBeTargeted(holder, card, engine.getPlayerById(self.getId()))) continue;
                for (auto victim : engine.getJieDaoVictims(*holder)) {
                    if (victim->getId() == self.getId() || isFriend(engine, self, *victim)) continue;
                    return 5;
                }
            }
            return 2;
        }
        case CardSubType::LE_BU_SI_SHU: {
            for (auto t : engine.getOtherAlivePlayers(self)) {
                if (isFriend(engine, self, *t) ||
                    GameEngine::hasJudgeCardOf(*t, CardSubType::LE_BU_SI_SHU) ||
                    !engine.canBeTargeted(t, card, engine.getPlayerById(self.getId()))) continue;
                return 5;
            }
            return 2;
        }
        case CardSubType::BING_LIANG_CUN_DUAN: {
            for (auto t : engine.getOtherAlivePlayers(self)) {
                if (isFriend(engine, self, *t) || !engine.canUseBingLiangOn(self,*t,card) ||
                    !engine.canBeTargeted(t, card, engine.getPlayerById(self.getId()))) continue;
                return 4;
            }
            return 2;
        }
        case CardSubType::SHUN_SHOU_QIAN_YANG: {
            for (auto t : engine.getOtherAlivePlayers(self)) {
                if (!isFriend(engine, self, *t) && engine.canUseShunShouOn(self, *t, card) &&
                    engine.canBeTargeted(t, card, engine.getPlayerById(self.getId()))) return 7;
            }
            return 3;
        }
        case CardSubType::GUO_HE_CHAI_QIAO: {
            for (auto t : engine.getOtherAlivePlayers(self)) {
                if (!isFriend(engine, self, *t) && !t->getAllCards().empty() &&
                    engine.canBeTargeted(t, card, engine.getPlayerById(self.getId()))) return 6;
            }
            return 3;
        }
        case CardSubType::JUE_DOU: {
            int sha = 0;
            for (auto& c : self.getHandCards()) {
                if (c->getSubType() == CardSubType::SHA) ++sha;
            }
            return sha >= 2 ? 6 : 4; // 杀不够时不敢拼，只值普通牌
        }
        case CardSubType::NAN_MAN_RU_QIN:
        case CardSubType::WAN_JIAN_QI_FA: {
            int enemies = 0, friends = 0;
            for (auto p : engine.getOtherAlivePlayers(self)) {
                if (isFriend(engine, self, *p)) friends++; else enemies++;
            }
            return enemies > friends ? 6 : 3;
        }
        case CardSubType::TAO_YUAN_JIE_YI: {
            int woundedFriends = self.isWounded() ? 1 : 0, woundedEnemies = 0;
            for (auto p : engine.getOtherAlivePlayers(self)) {
                if (!p->isWounded()) continue;
                if (isFriend(engine, self, *p)) woundedFriends++; else woundedEnemies++;
            }
            return woundedFriends > woundedEnemies ? 5 : 3;
        }
        case CardSubType::SHAN_DIAN: {
            int enemies = 0, friends = 0;
            for (auto p : engine.getOtherAlivePlayers(self)) {
                if (isFriend(engine, self, *p)) friends++; else enemies++;
            }
            if (enemies > friends &&
                !GameEngine::hasJudgeCardOf(self, CardSubType::SHAN_DIAN)) return 4;
            return 1;
        }
        case CardSubType::WEAPON:
        case CardSubType::ARMOR:
        case CardSubType::OFFENSIVE_HORSE:
        case CardSubType::DEFENSIVE_HORSE:
            return equipmentContextValue(self, card);
        default:
            return cardValue(card); // 杀/闪/桃/酒/无中/五谷/无懈等不看局面
    }
}

CardPtr AIController::chooseResponseCard(GameEngine& engine, Player& self, const std::vector<CardPtr>& candidates, CardSubType requestedType) {
    if (candidates.empty()) return nullptr;

    // 1. 优先打出实体牌，但在实体牌里挑**最不值钱**的一张（B2）：
    //    - 不要随手把【桃】级别的牌当响应打出去；
    //    - 需要打【闪】时，若这张红牌还能被【武圣】类技能当【杀】、且场上有能斩杀的敌人，则尽量留着（A1 联动）。
    {
        CardPtr bestReal;
        int bestVal = 1 << 30;
        Player& mutableSelf = const_cast<Player&>(self);
        bool killAvailable = false;
        for (const auto& t : enemiesOf(engine, self))
            if (canKillWithSha(engine, self, *t)) { killAvailable = true; break; }
        for (const auto& c : candidates) {
            if (!c || c->isVirtual()) continue;
            int v = cardValue(engine, self, c);
            if (requestedType == CardSubType::SHAN && killAvailable && self.getHp() >= 2 &&
                (c->getSuit() == Suit::HEART || c->getSuit() == Suit::DIAMOND)) {
                bool convertsToSha = false;
                for (const auto& sk : engine.getEffectiveSkills(mutableSelf))
                    if (sk && sk->convertCard(engine, mutableSelf, c, CardSubType::SHA)) { convertsToSha = true; break; }
                if (convertsToSha) v += 6; // 留着当【杀】斩杀，别当【闪】打出去
            }
            if (!bestReal || v < bestVal) { bestVal = v; bestReal = c; }
        }
        if (bestReal) return bestReal;
    }
    // 2. 其次考虑技能转化牌，但不轻易拿【桃】去转化（除非濒死自救等紧急情况）
    CardPtr best = nullptr;
    int bestCost = 0;
    for (const auto& c : candidates) {
        int cost;
        if (GameEngine::isZhangBaPlaceholder(c)) {
            // 丈八蛇矛：需要消耗两张手牌，凑不出两张低价值牌就不用
            auto picks = pickZhangBaCards(self);
            if (picks.size() < 2) continue;
            cost = cardValue(engine, self, picks[0]) + cardValue(engine, self, picks[1]);
        } else {
            bool costsTao = false;
            for (const auto& sub : c->getSubCards()) {
                if (sub->getSubType() == CardSubType::TAO) costsTao = true;
            }
            if (costsTao && requestedType != CardSubType::TAO && self.getHp() > 1) continue;
            cost = cardValue(engine, self, c);
        }
        if (!best || cost < bestCost) { best = c; bestCost = cost; }
    }
    return best;
}

// ===================== 选将 AI（身份场 / 斗地主） =====================
//
// 用户裁定（2026-10-05）：选将 AI 不再随机——身份场“主公先选完并亮出将，
// 别人再选针对或辅助”。评分构成（全部可在单测里复现）：
//   ① 强度档：HeroTier::powerScore（强 3 / 中 2 / 弱 1）×100，按场次取表；
//   ② 体力上限 ×5（站场能力）；
//   ③ 身份倾向：把技能文本按 输出/控制/辅助/防御 四类关键词计数后加权；
//   ④ 势力关系：忠臣/主公偏好与已亮出主公同势力（主公技【护驾】【激将】【救援】【血裔】
//      等都要同势力角色配合），反贼偏好异势力（不给主公技供牌）。
namespace {

int countKeyword(const std::string& text, const std::string& kw) {
    int n = 0;
    size_t pos = 0;
    while ((pos = text.find(kw, pos)) != std::string::npos) { ++n; pos += kw.size(); }
    return n;
}

// 单类关键词计数上限：避免“小作文”武将仅凭文本长度碾压（上限 4，使身份倾向与强度档同量级）
int clampCount(int n) { return n > 4 ? 4 : n; }

} // namespace

AIController::StyleProfile AIController::profileHero(const Hero& hero) {
    StyleProfile p;
    for (const auto& s : hero.getSkills()) {
        if (!s) continue;
        const std::string t = s->getDescription();
        p.output  += countKeyword(t, "伤害") + countKeyword(t, "流失") + countKeyword(t, "拼点") +
                     countKeyword(t, "【杀】") + countKeyword(t, "无次数限制") + countKeyword(t, "视为");
        p.control += countKeyword(t, "弃置") + countKeyword(t, "翻面") + countKeyword(t, "跳过") +
                     countKeyword(t, "判定") + countKeyword(t, "失效") + countKeyword(t, "不能") +
                     countKeyword(t, "获得其");
        p.support += countKeyword(t, "回复") + countKeyword(t, "摸") + countKeyword(t, "交给") +
                     countKeyword(t, "获得") + countKeyword(t, "手牌上限") + countKeyword(t, "装备区");
        p.defense += countKeyword(t, "闪") + countKeyword(t, "桃") + countKeyword(t, "防止") +
                     countKeyword(t, "不可被") + countKeyword(t, "无距离限制");
    }
    p.output = clampCount(p.output);
    p.control = clampCount(p.control);
    p.support = clampCount(p.support);
    p.defense = clampCount(p.defense);
    return p;
}

int AIController::draftScore(const HeroPtr& hero, const DraftContext& ctx) {
    if (!hero) return 0;
    int score = HeroTier::powerScore(hero->getId(), ctx.field) * 100;
    score += hero->getMaxHp() * 5;
    StyleProfile p = profileHero(*hero);

    bool lordKnown = static_cast<bool>(ctx.lordHero);
    bool sameCountry = lordKnown && ctx.lordHero->getCountry() == hero->getCountry();
    bool hasLordSkill = false;
    for (const auto& s : hero->getSkills()) if (s && s->hasTag(SkillTag::LORD)) hasLordSkill = true;

    // 技能全文（用于 D3 克制关系判断）
    auto allText = [](const HeroPtr& h) {
        std::string t;
        if (!h) return t;
        for (const auto& sk : h->getSkills()) if (sk) { t += sk->getName(); t += sk->getDescription(); }
        return t;
    };
    const std::string myText = allText(hero);
    const std::string lordText = allText(ctx.lordHero);
    auto lordHas = [&](const char* n) {
        return ctx.lordHero && ctx.lordHero->findSkill(n) != nullptr;
    };

    switch (ctx.role) {
        case Identity::ZHU_GONG:   // 主公：先手亮将，要站得住 + 自带主公技
            score += p.defense * 30 + p.support * 20 + p.output * 15 + p.control * 10;
            if (hasLordSkill) score += 80;   // 主公技（护驾/激将/救援/血裔/暴虐/颂威…）只有主公能吃到
            break;
        case Identity::ZHONG_CHEN: // 忠臣：辅助主公，同势力可吃主公技
            score += p.support * 35 + p.output * 20 + p.defense * 15 + p.control * 10;
            if (sameCountry) score += 120;   // 与主公同势力才能响应主公技
            break;
        case Identity::FAN_ZEI:    // 反贼：针对主公，输出/控制优先，避开主公技势力
            score += p.output * 40 + p.control * 30 + p.defense * 10 + p.support * 5;
            if (lordKnown && !sameCountry) score += 80; // 不给主公技供牌（护驾要魏、激将要蜀、救援要吴）
            // D3 克制关系：按主公的打法挑克制他的将
            if (lordKnown) {
                const bool lordSellBlood = lordHas("反馈") || lordHas("遗计") || lordHas("节命") ||
                                           lordHas("狂骨") || lordHas("恩怨") || lordText.find("卖血") != std::string::npos;
                if (lordSellBlood && (myText.find("不可被响应") != std::string::npos ||
                                      myText.find("无视") != std::string::npos ||
                                      myText.find("伤害+1") != std::string::npos || p.output >= 3))
                    score += 40;   // 主公是卖血流 → 选强命/多刀/加伤，让他“卖了也亏”
                if (lordText.find("判定") != std::string::npos && myText.find("判定") != std::string::npos)
                    score += 40;   // 主公靠判定（八卦阵/洛神/雷击）→ 选能改判或克判定的
                if (lordText.find("装备区") != std::string::npos &&
                    (myText.find("弃置") != std::string::npos || myText.find("获得其") != std::string::npos))
                    score += 40;   // 主公靠装备 → 选拆迁系
            }
            // D4 座次：先手位要能压住场（控制/自保），末位负责收割（输出）
            if (ctx.seatPosition == 0) score += p.control * 15 + p.defense * 10;
            else if (ctx.seatPosition == 2) score += p.output * 15;
            break;
        case Identity::NEI_JIAN:   // 内奸：要能单挑，攻守兼备；纯辅助嘲讽高、后期没用
            score += p.defense * 30 + p.output * 30 + p.support * 10 + p.control * 10;
            if (p.output >= 2 && p.defense >= 2) score += 40;
            if (p.support >= 3 && p.output <= 1) score -= 30;
            // D5 内奸伪装：别选嘲讽过高的（首轮被集火），也别与主公同势力（会被反贼当忠臣打）
            if (lordKnown && sameCountry) score -= 40;
            if (p.output >= 4) score -= 30;
            if (ctx.seatPosition == 0) score += p.defense * 10; // 先手位更要能活
            break;
        case Identity::DI_ZHU:     // 斗地主·地主：1 打 2，必须自给自足
            score += p.output * 40 + p.defense * 35 + p.control * 25 + p.support * 10;
            break;
        case Identity::NONG_MIN:   // 斗地主·农民：配合队友，辅助与输出并重
        default:
            score += p.support * 30 + p.output * 35 + p.control * 20 + p.defense * 15;
            // D4 座次：农民 2 号位先动（压制/控场），3 号位后动（收割/补刀）
            if (ctx.role == Identity::NONG_MIN) {
                if (ctx.seatPosition == 0) score += p.control * 15 + p.support * 10;
                else if (ctx.seatPosition == 2 || ctx.seatPosition == 1) score += p.output * 15;
            }
            break;
    }
    return score;
}

std::vector<size_t> AIController::rankDraftHeroes(const std::vector<HeroPtr>& heroes, const DraftContext& ctx) {
    static const bool trace = std::getenv("THKS_AI_TRACE") != nullptr;
    std::vector<size_t> order;
    order.reserve(heroes.size());
    for (size_t i = 0; i < heroes.size(); ++i) if (heroes[i]) order.push_back(i);
    // 稳定排序：同分保持候选原顺序（亮将顺序），保证可复现
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return draftScore(heroes[a], ctx) > draftScore(heroes[b], ctx);
    });
    if (trace) {   // F1：把选将评分明细打到 stderr，便于调参
        for (size_t rank = 0; rank < order.size(); ++rank) {
            const auto& h = heroes[order[rank]];
            std::fprintf(stderr, "[选将评分] 第%zu名 %s 分数=%d 强度档=%d 体力=%d\n", rank + 1,
                         h->getName().c_str(), draftScore(h, ctx),
                         HeroTier::powerScore(h->getId(), ctx.field), h->getMaxHp());
        }
    }
    return order;
}

size_t AIController::chooseDraftHeroWithVariance(const std::vector<HeroPtr>& heroes, const DraftContext& ctx,
                                                std::mt19937& rng, int secondPickPercent) {
    auto order = rankDraftHeroes(heroes, ctx);
    if (order.empty()) return 0;
    if (order.size() == 1 || secondPickPercent <= 0) return order.front();
    int pct = secondPickPercent > 100 ? 100 : secondPickPercent;
    int roll = std::uniform_int_distribution<int>(0, 99)(rng);
    return roll < pct ? order[1] : order.front();
}

size_t AIController::chooseDraftHero(const std::vector<HeroPtr>& heroes, const DraftContext& ctx) {
    size_t best = 0;
    int bestScore = 0;
    for (size_t i = 0; i < heroes.size(); ++i) {
        if (!heroes[i]) continue;
        int sc = draftScore(heroes[i], ctx);
        if (i == 0 || sc > bestScore) { best = i; bestScore = sc; }
    }
    return best;
}

CardPtr AIController::chooseMostValuableCard(const std::vector<CardPtr>& cards) {
    CardPtr best = nullptr;
    for (const auto& c : cards) {
        if (!best || cardValue(c) > cardValue(best)) best = c;
    }
    return best;
}

CardPtr AIController::chooseMostValuableCard(GameEngine& engine, Player& self, const std::vector<CardPtr>& cards) {
    CardPtr best = nullptr;
    for (const auto& c : cards) {
        if (!best || cardValue(engine, self, c) > cardValue(engine, self, best)) best = c;
    }
    return best;
}

std::vector<CardPtr> AIController::pickZhangBaCards(const Player& self) {
    std::vector<CardPtr> pool;
    for (const auto& c : self.getHandCards()) {
        // 桃、闪、无懈可击留着保命；【杀】本身不用转化；其余低价值牌可以拿来当【杀】
        auto st = c->getSubType();
        if (st == CardSubType::TAO || st == CardSubType::SHAN || st == CardSubType::WU_XIE_KE_JI) continue;
        if (st == CardSubType::SHA) continue;
        if (cardValue(c) > 4) continue;
        pool.push_back(c);
    }
    if (pool.size() < 2) return {};
    std::stable_sort(pool.begin(), pool.end(), [](const CardPtr& a, const CardPtr& b) {
        return cardValue(a) < cardValue(b);
    });
    return {pool[0], pool[1]};
}

// 【杀】目标评分：能击杀者最优先（避免错过斩杀），其次体力低者，再考虑威胁（手牌多者更危险）。
// damage 为本次【杀】的预计伤害（含【酒】/技能加成），默认 1。
std::shared_ptr<Player> AIController::chooseShaTarget(GameEngine& engine, Player& self,
                                                     const std::vector<std::shared_ptr<Player>>& candidates,
                                                     int damage) {
    std::shared_ptr<Player> best = nullptr;
    int bestScore = -(1 << 30);
    const PlayerPtr focus = focusTarget(engine, self);     // E3：本方集火目标（同分取座位小者，稳定）
    for (auto target : candidates) {
        if (!target || !target->isAlive() || isFriend(engine, self, *target)) continue;
        int score = 0;
        // A2：按**真实预计伤害**判断斩杀（含酒/裸衣/烈弓/古锭刀/藤甲），并计入多刀来源（咆哮/丈八/转化）
        const int realDamage = std::max(damage, estimatedShaDamage(engine, self, *target));
        if (canKillWithSha(engine, self, *target)) score += weights().killBonus;
        else if (target->getHp() <= realDamage) score += weights().killBonus;   // 一击致命
        score += weights().hpBase - target->getHp() * weights().hpFactor;          // 体力越低越优先
        score += std::min(target->getHandCardCount(), weights().handThreatCap);    // 手牌多 = 威胁大
        if (isSellBloodHero(*target)) score -= 30;                                 // A6：卖血流打了亏牌
        if (focus && focus->getId() == target->getId()) score += weights().focusTargetBonus; // E3：跟队友集火同一人
        if (target->getId() == engine.livingLordId() &&
            (self.getIdentity() == Identity::FAN_ZEI || self.getIdentity() == Identity::NEI_JIAN ||
             self.getIdentity() == Identity::YE_XIN_JIA))
            score += weights().lordFocusBonus;                                     // 反贼/内奸/野心家压主公
        if (target->getHp() <= 0) score -= weights().killBonus;                    // 理论不会出现
        if (score > bestScore || (score == bestScore && best && target->getId() < best->getId())) {
            bestScore = score;
            best = target;   // 同分取座位小者
        }
    }
    static const bool shaTrace = std::getenv("THKS_AI_TRACE") != nullptr;
    if (shaTrace && best)   // F1：目标评分明细
        std::fprintf(stderr, "[杀目标] %s → %s（预计伤害 %d，其体力 %d）\n", self.getName().c_str(),
                     best->getName().c_str(), estimatedShaDamage(engine, self, *best), best->getHp());
    return best;
}

// 拆顺/延时锦囊目标：敌方中“可见牌量”最多者（手牌数 + 装备数×2 + 判定区牌数×2）；
// E3：与本方集火目标一致时额外加分，避免同一方 AI 各打各的。
std::shared_ptr<Player> AIController::chooseRichEnemy(GameEngine& engine, Player& self,
                                                      const std::vector<std::shared_ptr<Player>>& candidates) {
    std::shared_ptr<Player> best = nullptr;
    int bestScore = -(1 << 30);
    for (auto target : candidates) {
        if (!target || !target->isAlive() || isFriend(engine, self, *target)) continue;
        int score = target->getHandCardCount() +
                    2 * static_cast<int>(target->getAllEquipment().size()) +
                    2 * static_cast<int>(target->getJudgeZone().size());
        if (target->getHp() <= 1) score += 2;            // 濒危者优先削弱（防止其回血反打）
        // 拆顺类不加 E3 集火分：这里的目标函数本来就是“削最能打的那个”，与集火方向一致；
        // 固定分会盖过牌量差（回归 ai/rich_enemy_prefers_more_visible_cards）。
        if (score > bestScore || (score == bestScore && best && target->getId() < best->getId())) {
            bestScore = score;
            best = target;   // 同分取座位小者（确定性、可复现）
        }
    }
    return best;
}

std::shared_ptr<Player> AIController::chooseTrickTarget(GameEngine& engine, Player& self, CardSubType trickType, const std::vector<std::shared_ptr<Player>>& candidates) {
    if (trickType == CardSubType::TAO_YUAN_JIE_YI || trickType == CardSubType::WU_ZHONG_SHENG_YOU) {
        return nullptr;
    }

    if (trickType == CardSubType::GUO_HE_CHAI_QIAO || trickType == CardSubType::SHUN_SHOU_QIAN_YANG ||
        trickType == CardSubType::LE_BU_SI_SHU || trickType == CardSubType::BING_LIANG_CUN_DUAN) {
        return chooseRichEnemy(engine, self, candidates);
    }
    if (trickType == CardSubType::JUE_DOU) {
        // 【决斗】：优先手牌最少的敌人（其打出【杀】的概率最低）
        std::shared_ptr<Player> best = nullptr;
        for (auto target : candidates) {
            if (!target || !target->isAlive() || isFriend(engine, self, *target)) continue;
            if (!best || target->getHandCardCount() < best->getHandCardCount()) best = target;
        }
        return best;
    }
    return nullptr;
}

namespace {
// C3：体力阈值型觉醒技（技能名 → 触发体力 / 已觉醒标记）。目前官网口径明确的只有孙策【魂姿】。
bool holdPeachForAwakening(GameEngine& engine, const Player& self) {
    if (!self.getHero() || self.getHp() != 2) return false;   // 只在“差 1 点体力到阈值”时考虑
    if (self.getMark("魂姿已觉醒") > 0) return false;
    if (!self.getHero()->findSkill("魂姿")) return false;
    int shan = 0;
    for (const auto& c : self.getHandCards()) if (c && c->getSubType() == CardSubType::SHAN) ++shan;
    if (shan == 0 && !GameEngine::hasArmor(self, "八卦阵")) return false; // 没有自保手段就别硬扛
    (void)engine;
    return true;
}
} // namespace

AIController::PlayDecision AIController::makePlayDecision(GameEngine& engine, Player& self) {
    PlayDecision decision;
    const auto& hand = self.getHandCards();
    if (self.isHandCardsBanned()) return decision;
    // A9 弃牌前瞻：手牌超过上限说明弃牌阶段要白扔牌，此时可以放宽出牌门槛
    // （决斗少一张【杀】也打、群体锦囊敌我持平时也放），把低价值牌用掉而不是弃掉。
    PlayerPtr selfPtrForLimit = engine.getPlayerById(self.getId());
    const int handLimit = selfPtrForLimit ? engine.calculateHandLimit(selfPtrForLimit)
                                          : self.getHandCardCount();
    const int overLimit = self.getHandCardCount() - handLimit;
    // E1：记牌器口径的牌堆剩余（只看牌堆与弃牌堆，不偷看别人手牌）——
    //   牌堆里【杀】/【闪】快见底时，南蛮/万箭/决斗的命中率显著提高；【无懈】见底时延时锦囊更安全。
    const int shaLeftInDeck = remainingCardsByName(engine, "杀");
    const int shanLeftInDeck = remainingCardsByName(engine, "闪");
    const int wuxieLeftInDeck = remainingCardsByName(engine, "无懈可击");

    // 1. 受伤时先吃桃
    //    C3 例外：若“体力阈值型觉醒技”还差 1 点体力就能触发（如孙策【魂姿】：准备阶段体力为 1 时觉醒），
    //    且手上还有【闪】可以自保，则本回合先不回血——回血会把觉醒推后一整轮。
    if (self.getHp() < self.getMaxHp() && !holdPeachForAwakening(engine, self)) {
        for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
            if (card->getSubType() == CardSubType::TAO) {
                decision.cardToPlay = card;
                return decision;
            }
        }
    }

    // 2. 装备（A4）：空槽就装；已装同类则只在**明显更好**时替换（替换会把旧装备送去弃牌堆）。
    //    另外：手里【杀】多时优先装上诸葛连弩（不限次数），敌人打不到我们时优先 +1 马。
    for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
        if (card->getType() != CardType::EQUIPMENT) continue;
        CardPtr current;
        switch (card->getSubType()) {
            case CardSubType::WEAPON: current = self.getWeapon(); break;
            case CardSubType::ARMOR: current = self.getArmor(); break;
            case CardSubType::OFFENSIVE_HORSE: current = self.getOffensiveHorse(); break;
            case CardSubType::DEFENSIVE_HORSE: current = self.getDefensiveHorse(); break;
            default: break;
        }
        if (!current) { decision.cardToPlay = card; return decision; }
        int gain = equipmentBaseValue(card) - equipmentBaseValue(current);
        if (card->getName() == "诸葛连弩" && shaSourceCount(engine, self) >= 2) gain += 3; // 多刀时连弩更值
        if (card->getName() == current->getName()) gain = -1;                              // 同名不换
        if (gain > 0) { decision.cardToPlay = card; return decision; }
    }

    // 3. 无中生有 / 五谷丰登
    //    E4：【五谷丰登】会让每人挑一张牌——敌我不少于我方时等于资敌，残局单挑时尤其不该放。
    for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
        if (card->getSubType() == CardSubType::WU_ZHONG_SHENG_YOU) {
            decision.cardToPlay = card;
            return decision;
        }
        if (card->getSubType() == CardSubType::WU_GU_FENG_DENG) {
            const int foes = enemyCount(engine, self), mates = friendCount(engine, self);
            if (foes > mates) continue;                 // 资敌，跳过
            decision.cardToPlay = card;
            return decision;
        }
    }

    // 3.5 酒（A1）：喝完酒后本回合**能否斩杀**某个敌人——不只看实体【杀】和“正好 2 血”，
    // 而是把【武圣】（红牌当杀）/【龙胆】（闪当杀）/丈八蛇矛/咆哮多刀等来源一并计入。
    if (!self.isDrunk() && self.getShaCountThisTurn() < engine.getShaLimit(self)) {
        for (const auto& jiu : hand) {
            if (!jiu || jiu->getSubType() != CardSubType::JIU || !engine.canUseOriginalCard(self, jiu)) continue;
            bool lethal = false;
            for (const auto& t : enemiesOf(engine, self)) {
                if (t && canKillWithSha(engine, self, *t, /*assumeDrunk=*/true)) { lethal = true; break; }
            }
            if (lethal) { decision.cardToPlay = jiu; return decision; }
        }
    }

    // 3.8 A7：铁索连环 + 属性【杀】联动——手里有火/雷【杀】且能连起 2 名未横置的敌人时，先连再杀。
    {
        bool hasElementalSha = false;
        for (const auto& c : hand)
            if (c && c->getSubType() == CardSubType::SHA && c->getShaElement() != ShaElement::NORMAL) {
                hasElementalSha = true;
                break;
            }
        if (hasElementalSha) {
            PlayerPtr me = engine.getPlayerById(self.getId());
            for (const auto& card : hand) {
                if (!card || card->getSubType() != CardSubType::TIE_SUO_LIAN_HUAN) continue;
                if (!engine.canUseOriginalCard(self, card)) continue;
                std::vector<PlayerPtr> picks;
                for (auto t : enemiesOf(engine, self)) {
                    if (!t || t->isChained()) continue;
                    if (!engine.canBeTargeted(t, card, me)) continue;
                    picks.push_back(t);
                    if (picks.size() == 2) break;
                }
                if (picks.size() == 2) {
                    decision.cardToPlay = card;
                    decision.targets = picks;
                    return decision;
                }
            }
        }
    }

    // 4. 杀（含技能转化：武圣的红色牌、龙胆的闪）
    bool canSha = self.getShaCountThisTurn() < engine.getShaLimit(self) ||
                  (self.getHero() && self.getHero()->findSkill("武神"));
    if (canSha) {
        std::vector<CardPtr> shaOptions;
        for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
            if (card->getSubType() == CardSubType::SHA) shaOptions.push_back(card);
        }
        // 转化牌：只在没有实体【杀】时考虑，且不拿【桃】、也不拿最后一张【闪】去转化
        if (shaOptions.empty()) {
            int shanCount = 0;
            for (const auto& card : hand) if (card->getSubType() == CardSubType::SHAN) shanCount++;
            std::vector<CardPtr> pool = hand;
            auto eq = self.getAllEquipment();
            (void)eq; // 装备暂不参与 AI 转化，避免拆掉自己的防具
            for (const auto& card : pool) {
                if (card->getSubType() == CardSubType::TAO && engine.canUseOriginalCard(self,card)) continue;
                if (card->getSubType() == CardSubType::SHAN && shanCount <= 1 &&
                    engine.canUseOriginalCard(self,card)) continue;
                auto conv = engine.getConversionsFor(engine.getPlayerById(self.getId()), card, CardSubType::SHA);
                for (auto& v : conv) shaOptions.push_back(v);
            }
            // 价值低的先用（结合局面：当前用不出去的牌优先拿来转化）
            std::stable_sort(shaOptions.begin(), shaOptions.end(), [&](const CardPtr& a, const CardPtr& b) {
                return cardValue(engine, self, a) < cardValue(engine, self, b);
            });
        }
        // 丈八蛇矛：没有任何【杀】可用时，用两张低价值手牌当【杀】
        if (shaOptions.empty() && GameEngine::hasWeapon(self, "丈八蛇矛")) {
            auto picks = pickZhangBaCards(self);
            if (picks.size() == 2) {
                shaOptions.push_back(Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, picks, "丈八蛇矛"));
            }
        }
        const int shaDamage = 1 + (self.isDrunk() ? 1 : 0); // 【酒】加成后的预计伤害
        for (const auto& sha : shaOptions) {
            auto candidates = engine.getShaTargets(self, sha);
            const bool skillIgnoresShaLimit = sha->getSkillSource() == "莺舞" ||
                                               sha->getSkillSource() == "势-战烈";
            if(self.getShaCountThisTurn()>=engine.getShaLimitForCard(self,sha) && !skillIgnoresShaLimit) {
                candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const auto& target) {
                    return !target || !engine.canUseShaBeyondLimitOn(self,*target,sha);
                }),candidates.end());
            }
            if(candidates.empty())continue;
            if(sha->getSkillSource()=="无谋" && sha->getSubCards().size()==1 &&
               sha->getSubCards().front()->getSubType()==CardSubType::JIE_DAO_SHA_REN) {
                // 借刀杀人的原目标是“持武器者 + 被其杀者”，不可像普通杀一样
                // 随意从候选角色里选两人，否则 AI 会提交无效的目标组合。
                for(auto holder:engine.getJieDaoWeaponHolders(self)) {
                    if(std::find(candidates.begin(),candidates.end(),holder)==candidates.end() ||
                       isFriend(engine, self,*holder))continue;
                    for(auto victim:engine.getJieDaoVictims(*holder)) {
                        if(std::find(candidates.begin(),candidates.end(),victim)==candidates.end() ||
                           isFriend(engine, self,*victim))continue;
                        decision.cardToPlay=sha;
                        decision.targets={holder,victim};
                        return decision;
                    }
                }
                continue;
            }
            auto target = chooseShaTarget(engine, self, candidates, shaDamage);
            if (target) {
                decision.cardToPlay = sha;
                decision.targets.push_back(target);
                // 装备和技能叠加额外【杀】目标。
                int targetLimit=engine.getShaTargetLimit(self,sha);
                if (targetLimit>1) {
                    // A8（方天画戟等多目标【杀】）：额外目标也按“可击杀 > 体力低 > 手牌多”排序挑，
                    // 而不是按候选顺序随手加满。
                    std::vector<PlayerPtr> extras;
                    for (auto& extra : candidates) {
                        if (!extra || extra->getId() == target->getId() || isFriend(engine, self, *extra)) continue;
                        extras.push_back(extra);
                    }
                    std::stable_sort(extras.begin(), extras.end(), [&](const PlayerPtr& a, const PlayerPtr& b) {
                        auto sc = [&](const PlayerPtr& t) {
                            int v = 0;
                            if (canKillWithSha(engine, self, *t)) v += weights().killBonus;
                            v += weights().hpBase - t->getHp() * weights().hpFactor;
                            v += std::min(t->getHandCardCount(), weights().handThreatCap);
                            if (isSellBloodHero(*t)) v -= 30;
                            return v;
                        };
                        return sc(a) > sc(b);
                    });
                    for (auto& extra : extras) {
                        if (static_cast<int>(decision.targets.size()) >= targetLimit) break;
                        decision.targets.push_back(extra);
                    }
                }
                return decision;
            }
        }
    }

    // 5. 拆顺
    for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
        if (card->getSubType() == CardSubType::GUO_HE_CHAI_QIAO) {
            std::vector<std::shared_ptr<Player>> candidates;
            for (auto target : engine.getOtherAlivePlayers(self)) {
                if (!isFriend(engine, self, *target) && !target->getAllCards().empty() &&
                    engine.canBeTargeted(target, card, engine.getPlayerById(self.getId())))
                    candidates.push_back(target);
            }
            auto target = chooseRichEnemy(engine, self, candidates);
            if (target) {
                decision.cardToPlay = card;
                decision.targets.push_back(target);
                return decision;
            }
        } else if (card->getSubType() == CardSubType::MANTIAN_GUOHAI) {
            std::vector<std::shared_ptr<Player>> candidates;
            for (auto target : engine.getOtherAlivePlayers(self)) {
                if (!target->getAllCards().empty() && engine.canBeTargeted(target,card,
                        engine.getPlayerById(self.getId())))
                    candidates.push_back(target);
            }
            // AI 只主动选择有牌的敌方角色，最多两名；不窥视对方隐藏手牌。
            for (auto target : candidates) {
                if (isFriend(engine, self,*target)) continue;
                decision.targets.push_back(target);
                if (decision.targets.size()==2) break;
            }
            if (!decision.targets.empty()) {
                decision.cardToPlay=card;
                return decision;
            }
        } else if (card->getSubType() == CardSubType::SHUN_SHOU_QIAN_YANG) {
            std::vector<std::shared_ptr<Player>> candidates;
            for (auto target : engine.getOtherAlivePlayers(self)) {
                if (!isFriend(engine, self, *target) && engine.canUseShunShouOn(self, *target, card) &&
                    engine.canBeTargeted(target, card, engine.getPlayerById(self.getId())))
                    candidates.push_back(target);
            }
            auto target = chooseRichEnemy(engine, self, candidates);
            if (target) {
                decision.cardToPlay = card;
                decision.targets.push_back(target);
                return decision;
            }
        }
    }

    // 5.5 借刀杀人：让持武器的敌人去杀另一名敌人（不成则收其武器）
    for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
        if (card->getSubType() != CardSubType::JIE_DAO_SHA_REN) continue;
        for (auto holder : engine.getJieDaoWeaponHolders(self)) {
            if (isFriend(engine, self, *holder) ||
                !engine.canBeTargeted(holder, card, engine.getPlayerById(self.getId()))) continue;
            for (auto victim : engine.getJieDaoVictims(*holder)) {
                if (victim->getId() == self.getId() || isFriend(engine, self, *victim)) continue;
                decision.cardToPlay = card;
                decision.targets = {holder, victim};
                return decision;
            }
        }
    }

    // 5.6 延时锦囊：乐不思蜀 / 兵粮寸断 给敌人，闪电在敌人较多时放出
    const PlayerPtr focus = focusTarget(engine, self);   // E3：本方集火目标
    {
        int enemies = 0, friends = 0;
        for (auto p : engine.getOtherAlivePlayers(self)) {
            if (isFriend(engine, self, *p)) friends++; else enemies++;
        }
        for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
            auto st = card->getSubType();
            if (st == CardSubType::LE_BU_SI_SHU || st == CardSubType::BING_LIANG_CUN_DUAN) {
                const bool safeFromWuxie = (wuxieLeftInDeck == 0);   // E1：没人能挡（牌堆已无【无懈】）
                // A5：不再“第一个敌人就贴”，而是按收益挑目标——
                //   手牌多者损失大；能改判定（鬼才/鬼道）者收益低；判定区已满者无效；反贼优先贴主公。
                PlayerPtr me = engine.getPlayerById(self.getId());
                PlayerPtr best;
                int bestScore = -(1 << 30);
                for (auto target : engine.getOtherAlivePlayers(self)) {
                    if (isFriend(engine, self, *target)) continue;
                    if (GameEngine::hasJudgeCardOf(*target, st)) continue;
                    if (!engine.canBeTargeted(target, card, me)) continue;
                    if (st == CardSubType::BING_LIANG_CUN_DUAN && !engine.canUseBingLiangOn(self, *target, card)) continue;
                    int score = std::min(target->getHandCardCount(), weights().handThreatCap);
                    if (canAlterJudgement(engine, *target)) score -= weights().delayedTrickJudgeRisk * 3;
                    if (safeFromWuxie) score += 2;   // 不会被【无懈可击】抵消 → 收益更确定
                    if (target->getId() == engine.livingLordId() && self.getIdentity() == Identity::FAN_ZEI) score += 10;
                    if (focus && focus->getId() == target->getId()) score += 5;   // 与集火目标一致
                    if (score > bestScore || (score == bestScore && best && target->getId() < best->getId())) {
                        bestScore = score;
                        best = target;   // 同分取座位小者
                    }
                }
                if (best) {
                    decision.cardToPlay = card;
                    decision.targets.push_back(best);
                    return decision;
                }
            } else if (st == CardSubType::SHAN_DIAN) {
                if (enemies > friends && !GameEngine::hasJudgeCardOf(self, st)) {
                    decision.cardToPlay = card;
                    return decision;
                }
            }
        }

        // 断粮的黑色基本牌/装备牌（含装备区）也可当兵粮寸断使用。
        auto materials=self.getHandCards();
        for(auto equipped:self.getAllEquipment())materials.push_back(equipped);
        for(auto material:materials) {
            auto converted=engine.getConversionsFor(engine.getPlayerById(self.getId()),
                                                     material,CardSubType::BING_LIANG_CUN_DUAN);
            for(auto v:converted)if(v->getSkillSource()=="断粮")
                for(auto target:engine.getOtherAlivePlayers(self)) {
                    if(isFriend(engine, self,*target) || !engine.canUseBingLiangOn(self,*target,v) ||
                       !engine.canBeTargeted(target,v,engine.getPlayerById(self.getId())))continue;
                    decision.cardToPlay=v;
                    decision.targets={target};
                    return decision;
                }
        }

        // 5.7 群体锦囊：南蛮入侵 / 万箭齐发（敌人多于队友），桃园结义（己方受伤更重）
        int woundedFriends = self.isWounded() ? 1 : 0, woundedEnemies = 0;
        for (auto p : engine.getOtherAlivePlayers(self)) {
            if (!p->isWounded()) continue;
            if (isFriend(engine, self, *p)) woundedFriends++; else woundedEnemies++;
        }
        for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
            auto st = card->getSubType();
            // E1：牌堆里对应响应牌快见底 → 群体锦囊几乎必中，敌我持平也值得放
            const bool responseScarce =
                (st == CardSubType::NAN_MAN_RU_QIN && shaLeftInDeck <= 2) ||
                (st == CardSubType::WAN_JIAN_QI_FA && shanLeftInDeck <= 2);
            if ((st == CardSubType::NAN_MAN_RU_QIN || st == CardSubType::WAN_JIAN_QI_FA) &&
                (enemies + (overLimit > 0 || responseScarce ? 1 : 0) > friends + weights().aoeMargin) &&
                enemies >= (responseScarce ? 1 : 2)) {
                // A3：敌方人数占优且至少 2 名敌人才放（1 名敌人时不如直接用【杀】）；
                // 另外敌方“卖血流”多时不放（打他们反而给他们牌）。
                int sellBlood = 0;
                for (auto p : engine.getOtherAlivePlayers(self))
                    if (!isFriend(engine, self, *p) && isSellBloodHero(*p)) ++sellBlood;
                if (sellBlood * 2 >= enemies) continue;
                decision.cardToPlay = card;
                return decision;
            }
            if (st == CardSubType::TAO_YUAN_JIE_YI && woundedFriends > woundedEnemies) {
                decision.cardToPlay = card;
                return decision;
            }
        }
    }

    // 6. 决斗（A6）：按“【杀】来源数”（实体杀 + 武圣/龙胆/丈八等转化）而不是只数实体杀；
    //    目标是卖血流（反馈/遗计/节命…）时门槛再提高，避免决斗郭嘉/荀彧类武将反而送牌。
    const int myShaCount = shaSourceCount(engine, self);
    // A9（手牌溢出）与 E1（牌堆里【杀】见底 → 对方多半也没【杀】）都会降低决斗门槛
    const int duelNeed = std::max(1, weights().duelShaNeed - (overLimit > 0 ? 1 : 0) - (shaLeftInDeck <= 2 ? 1 : 0));
    if (myShaCount >= duelNeed) {
        for (const auto& card : hand) {
            if(!engine.canUseOriginalCard(self,card))continue;
            if (card->getSubType() == CardSubType::JUE_DOU) {
                auto candidates = engine.getOtherAlivePlayers(self);
                candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const auto& target) {
                    return !target || !engine.canBeTargeted(target, card, engine.getPlayerById(self.getId()));
                }), candidates.end());
                auto target = chooseTrickTarget(engine, self, CardSubType::JUE_DOU, candidates);
                // 卖血流目标需要更多【杀】才值得决斗
                if (target && isSellBloodHero(*target) &&
                    myShaCount < duelNeed + weights().duelAvoidSellBlood) target = nullptr;
                if (target) {
                    decision.cardToPlay = card;
                    decision.targets.push_back(target);
                    return decision;
                }
            }
        }
    }

    return decision;
}

std::vector<CardPtr> AIController::chooseCardsToDiscard(GameEngine& engine, Player& self, int discardCount) {
    std::vector<CardPtr> result;
    std::vector<CardPtr> hand = self.getHandCards();
    if (discardCount <= 0 || hand.empty()) return result;

    std::stable_sort(hand.begin(), hand.end(), [&](const CardPtr& a, const CardPtr& b) {
        return cardValue(engine, self, a) < cardValue(engine, self, b);
    });
    for (int i = 0; i < discardCount && i < static_cast<int>(hand.size()); ++i) {
        result.push_back(hand[i]);
    }
    return result;
}

} // namespace Thks
