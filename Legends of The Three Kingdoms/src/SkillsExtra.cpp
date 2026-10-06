// SkillsExtra.cpp —— 花鬘 / 友·诸葛亮 / 势系列（官网原文见 docs/shi_you_appendix.md）
#include "SkillsExtra.h"
#include "GameEngine.h"
#include "AI.h"
#include "Interaction.h"
#include <algorithm>
#include <set>
#include <random>
#include <cstdlib> // std::abs（MSVC 不保证传递包含）

namespace Thks {

namespace {

PlayerPtr selfOf(GameEngine& engine, const Player& self) {
    return engine.getPlayerById(self.getId());
}

std::string who(const Player& p) { return "[" + p.getName() + "]"; }

std::mt19937& extraRng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

// 伤害类的牌：杀、决斗、南蛮入侵、万箭齐发、火攻（伤害类卡牌的通行判定）
bool isDamageCard(CardPtr card) {
    if (!card) return false;
    auto st = card->getSubType();
    return st == CardSubType::SHA || st == CardSubType::JUE_DOU ||
           st == CardSubType::NAN_MAN_RU_QIN || st == CardSubType::WAN_JIAN_QI_FA ||
           st == CardSubType::HUO_GONG;
}

bool isRedSuit(Suit s) { return s == Suit::HEART || s == Suit::DIAMOND; }

void setMark(Player& p, const std::string& name, int v) {
    p.addMark(name, v - p.getMark(name));
}

std::string suitName(Suit s) {
    switch (s) {
        case Suit::SPADE: return "♠";
        case Suit::HEART: return "♥";
        case Suit::CLUB: return "♣";
        case Suit::DIAMOND: return "♦";
        default: return "无色";
    }
}

} // namespace

// =====================================================================
//  花鬘（蜀）：象阵 / 芳踪 / 嬉战
// =====================================================================

XiangZhenSkill::XiangZhenSkill()
    : StateSkill("象阵",
                 "锁定技，【南蛮入侵】对你无效；【南蛮入侵】结算结束后，若此牌造成过伤害，你与伤害来源各摸一张牌。",
                 SkillTag::LOCK) {}

void XiangZhenSkill::onCheckCardEffect(GameEngine&, const Player&, CardPtr card, bool& effective) {
    if (card && card->getSubType() == CardSubType::NAN_MAN_RU_QIN) effective = false;
}

void XiangZhenSkill::onGlobalDamage(GameEngine&, Player&, Player* source, Player&, int damage, CardPtr cause) {
    if (damage > 0 && cause && cause->getSubType() == CardSubType::NAN_MAN_RU_QIN) {
        damagedNanman = cause;
        damageSourceRaw = source;
    }
}

void XiangZhenSkill::onCardResolvedByAny(GameEngine& engine, Player& self, Player&, CardPtr card) {
    if (!damagedNanman || !card || card.get() != damagedNanman.get()) return;
    PlayerPtr me = selfOf(engine, self);
    engine.drawCards(me, 1, "象阵");
    if (damageSourceRaw && damageSourceRaw->getId() != self.getId()) {
        PlayerPtr src = engine.getPlayerById(damageSourceRaw->getId());
        if (src && src->isAlive()) engine.drawCards(src, 1, "象阵");
    } else if (damageSourceRaw && damageSourceRaw->getId() == self.getId()) {
        engine.drawCards(me, 1, "象阵"); // 伤害来源即你：再摸一张（你与伤害来源各摸一张）
    }
    engine.logMessage("  【象阵】" + who(self) + " 与伤害来源各摸一张牌。");
    damagedNanman = nullptr;
    damageSourceRaw = nullptr;
}

FangZongSkill::FangZongSkill()
    : StateSkill("芳踪",
                 "锁定技，出牌阶段，你使用伤害类的牌不能指定你攻击范围内的角色为目标。攻击范围内含有你的其他角色使用伤害类卡牌时，不能指定你为目标。结束阶段，你将手牌摸至X张（X为场上存活人数）。",
                 SkillTag::LOCK) {}

void FangZongSkill::onCheckCardTargetAsSource(GameEngine& engine, const Player& self, const Player& target,
                                              CardPtr card, bool& canTarget) {
    if (!canTarget || !isDamageCard(card)) return;
    if (engine.getCurrentPhase() != TurnPhase::PLAY) return;
    if (self.getMark("芳踪失效") > 0) return;
    if (engine.calculateDistance(self, target) <= self.getAttackRange()) canTarget = false;
}

void FangZongSkill::onCheckCardTarget(GameEngine& engine, const Player& self, const Player& target,
                                      CardPtr card, bool& canTarget) {
    // 攻击范围内含有你的其他角色使用伤害类卡牌时，不能指定你为目标
    if (!canTarget || &target != &self) return;
    if (!isDamageCard(card)) return;
    if (self.getMark("芳踪失效") > 0) return;
    auto source = engine.getCheckingSource();
    if (!source || source->getId() == self.getId()) return;
    if (engine.calculateDistance(*source, self) <= source->getAttackRange()) canTarget = false;
}

void FangZongSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    int x = static_cast<int>(engine.getAlivePlayers().size());
    PlayerPtr me = selfOf(engine, self);
    while (static_cast<int>(self.getHandCardCount()) < x && engine.getAlivePlayers().size() > 0) {
        int before = self.getHandCardCount();
        engine.drawCards(me, 1, "芳踪");
        if (self.getHandCardCount() == before) break; // 牌堆耗尽
    }
    engine.logMessage("  【芳踪】" + who(self) + " 将手牌摸至 " + std::to_string(x) + " 张。");
}


XiZhanSkill::XiZhanSkill()
    : StateSkill("嬉战",
                 "锁定技，其他角色回合开始时，你须弃一张牌并令你本回合“芳踪”失效，或流失1点体力。若你以此法弃置了牌，根据弃置牌的花色，执行以下效果：黑桃，其视为使用一张【酒】；红桃，你视为使用【无中生有】；梅花，你视为对其使用【铁索连环】；方块，你视为对其使用一张无距离限制的【火杀】。",
                 SkillTag::LOCK) {}

void XiZhanSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (!starting) {
        // 本回合的“芳踪”失效标记在回合所有者回合结束时清除
        if (turnOwner.getId() != self.getId() && self.getMark("芳踪失效") == turnOwner.getId() + 1)
            self.addMark("芳踪失效", -(turnOwner.getId() + 1));
        return;
    }
    if (turnOwner.getId() == self.getId()) return;
    PlayerPtr me = selfOf(engine, self);
    CardPtr discarded;
    auto discardable = self.getHandAndEquipmentCards();
    std::vector<std::string> choices;
    if (!discardable.empty()) choices.push_back("弃置一张牌并令本回合“芳踪”失效");
    choices.push_back("失去1点体力");
    int opt = engine.askChooseOption(me, choices, "【嬉战】你须选择一项执行", 0);
    if (!discardable.empty() && opt == 0) {
        discarded = engine.askChooseCard(me, discardable,
            "【嬉战】选择要弃置的牌（影响“芳踪”失效后的结算）", false,
            AIController::chooseLeastValuableCard(discardable));
        if (discarded) {
            engine.discardCardOf(me, discarded, "嬉战", me);
            self.addMark("芳踪失效", turnOwner.getId() + 1);
        }
    }
    if (!discarded) {
        // 未弃牌（或选择第二项）：失去1点体力
        engine.loseHp(me, 1, "嬉战");
        engine.logMessage("  【嬉战】" + who(self) + " 失去1点体力。");
        return;
    }
    // 根据弃置牌的花色执行一项
    Suit suit = engine.effectiveSuit(self, discarded);
    engine.logMessage("  【嬉战】" + who(self) + " 弃置了一张" + suitName(suit) + "牌并令本回合“芳踪”失效。");
    if (suit == Suit::SPADE) {
        auto p = engine.getPlayerById(turnOwner.getId());
        if (p && p->isAlive()) {
            auto jiu = Card::makeVirtual("酒", CardType::BASIC, CardSubType::JIU, {}, "嬉战");
            engine.useCard(p, jiu, {p});
            engine.logMessage("  【嬉战】" + who(turnOwner) + " 视为使用一张【酒】。");
        }
    } else if (suit == Suit::HEART) {
        auto wzs = Card::makeVirtual("无中生有", CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU, {}, "嬉战");
        engine.useCard(me, wzs, {me});
        engine.logMessage("  【嬉战】" + who(self) + " 视为使用一张【无中生有】。");
    } else if (suit == Suit::CLUB) {
        auto tiesuo = Card::makeVirtual("铁索连环", CardType::TRICK, CardSubType::TIE_SUO_LIAN_HUAN, {}, "嬉战");
        engine.useCard(me, tiesuo, {engine.getPlayerById(turnOwner.getId())});
        engine.logMessage("  【嬉战】" + who(self) + " 视为对" + who(turnOwner) + " 使用一张【铁索连环】。");
    } else { // DIAMOND
        auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "嬉战", ShaElement::FIRE);
        PlayerPtr t = engine.getPlayerById(turnOwner.getId());
        if (t) engine.useCard(me, sha, {t});
        engine.logMessage("  【嬉战】" + who(self) + " 视为对" + who(turnOwner) + " 使用一张无距离限制的火【杀】。");
    }
}

// 方块效果的火【杀】由【嬉战】亲自结算（引擎直接 useCard），此处只把“无距离限制”补齐：
// 否则目标超出花鬘攻击范围时会被 canBeTargeted 挡下，与官网文本不符（测试锁定 4 人局距离 2）。
void XiZhanSkill::onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr card, bool& canTarget) {
    if (card && card->getSkillSource() == "嬉战") canTarget = true;
}

// =====================================================================
//  友·诸葛亮（友）：演策 / 方遒 / 共砺
// =====================================================================

YouYanCeSkill::YouYanCeSkill()
    : ActiveSkill("友-演策",
                 "每轮限一次，首轮开始时，或准备阶段，你可以选择一项：从牌堆中随机获得一张锦囊牌；执行“卧龙演策”。若你执行“卧龙演策”，当一张牌被使用时，若此牌的类别或颜色与你的预测相同，你摸一张牌（每次执行“卧龙演策”至多因此摸五张牌）。当本次“卧龙演策”的预测全部验证后，或当你再次执行“卧龙演策”时，若你本次“卧龙演策”正确的预测数量：为0，你失去1点体力，此后“卧龙演策”可预测的牌数-1；不足一半，你弃置两张牌；至少一半（向上取整），你根据本次预测的方式，从牌堆中获得一张符合你声明条件的牌；全部正确，你摸两张牌，此后“卧龙演策”可预测的牌数+1（至多为7）。",
            0) {}

void YouYanCeSkill::onRoundStart(GameEngine& engine, Player& self) {
    // 官网：“每轮限一次，首轮开始时，或准备阶段……”
    if (engine.getCurrentRound() > 1) return; // 仅首轮开始时（首轮=第1轮；未开局时记为0）
    activate(engine, self);
}

void YouYanCeSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION || !engine.isPlayerTurn(self)) return; // 自己的准备阶段
    activate(engine, self);
}

void YouYanCeSkill::activate(GameEngine& engine, Player& self) {
    if (lastRoundActed == engine.getCurrentRound()) return; // 每轮限一次
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【友-演策】发动“卧龙演策”？", true)) return;
    lastRoundActed = engine.getCurrentRound();
    int opt = engine.askChooseOption(me, {"从牌堆中随机获得一张锦囊牌", "执行“卧龙演策”"},
                                     "【友-演策】选择一项", 1);
    if (opt == 0) {
        auto c = drawRandomTrick(engine);
        if (c) {
            engine.obtainCard(me, c);
            engine.logMessage("  【友-演策】" + who(self) + " 从牌堆中随机获得一张【" + c->getName() + "】。");
        } else {
            engine.logMessage("  【友-演策】牌堆中没有锦囊牌了。");
        }
        return;
    }
    // 再次执行前先结算未完成的预测
    if (active) settle(engine, self);
    startPrediction(engine, self);
}

CardPtr YouYanCeSkill::drawRandomTrick(GameEngine& engine) {
    return engine.getDeck().drawRandomMatching(
        [](const CardPtr& c) { return c && c->getType() == CardType::TRICK; }, extraRng());
}

void YouYanCeSkill::startPrediction(GameEngine& engine, Player& self) {
    PlayerPtr me = selfOf(engine, self);
    if (active) settle(engine, self);
    active = true;
    verified = 0;
    correct = 0;
    drawnFromMatch = 0;
    fangqiuRevealed = false;
    int n = predictCount;
    // 共砺（排位、斗地主）：友庞统在场且阵营一致 → 可预测的牌数 +1（身份模式无效）。
    for (auto sk : engine.getEffectiveSkills(self))
        n += sk->gongLiPredictionCountBonus(engine, self);
    if (n < 1) n = 1;
    preds.clear();
    engine.logMessage("  【" + name + "】" + who(self) + " 声明 " + std::to_string(n) + " 个预测：");
    for (int i = 0; i < n; i++) {
        int opt = engine.askChooseOption(me,
            {"预测类别：基本牌", "预测类别：锦囊牌", "预测类别：装备牌", "预测颜色：红色", "预测颜色：黑色"},
            "【友-演策】声明第 " + std::to_string(i + 1) + " 项预测", 0);
        Pred p;
        if (opt <= 2) { p.categoryMode = true; p.value = opt; }         // 0=基本 1=锦囊 2=装备（枚举序）
        else { p.categoryMode = false; p.value = (opt == 3) ? 1 : 0; }  // 颜色：1=红 0=黑
        preds.push_back(p);
    }
    engine.logMessage("  【友-演策】预测已声明，此后每有一张牌被使用即验证一项（至多 " +
                      std::to_string(n) + " 项）。");
    // 方遒：执行卧龙演策后可展示
    for (auto sk : engine.getEffectiveSkills(self)) sk->onPredictionStarted(engine, self);
}

// 当一张牌被使用或打出时验证预测（官网“使用或打出”均计；类别或颜色与预测相同则摸一张牌，每次至多五张）
void YouYanCeSkill::onAnyCardUsed(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    (void)user;
    if (!active || !card || verified >= (int)preds.size()) return;
    const Pred& p = preds[verified];
    bool match;
    if (p.categoryMode) match = ((int)card->getType() == p.value);
    else {
        bool red = card->getSuit() == Suit::HEART || card->getSuit() == Suit::DIAMOND;
        match = (red == (p.value == 1));
    }
    // 共砺（排位、斗地主）：友徐庶在场且阵营一致 → 第一张牌的结果视为正确。
    for (auto sk : engine.getEffectiveSkills(self))
        if (verified == 0 && sk->gongLiFirstPredictionAutoCorrect(engine, self)) match = true;
    engine.logMessage("  【友-演策】" + who(self) + " 的第 " + std::to_string(verified + 1) +
                      " 项预测（【" + card->getName() + "】被使用）→ " + (match ? "匹配。" : "不匹配。"));
    if (match) {
        correct++;
        if (drawnFromMatch < 5) { engine.drawCards(selfOf(engine, self), 1, "友-演策"); drawnFromMatch++; }
    }
    verified++;
    if (verified >= (int)preds.size()) settle(engine, self);
}
void YouYanCeSkill::settle(GameEngine& engine, Player& self) {
    PlayerPtr me = selfOf(engine, self);
    int total = (int)preds.size();
    if (total == 0) { active = false; return; }
    bool allCorrect = (correct == total);
    int bonus = (self.getMark("方遒展示") > 0) ? 1 : 0;
    self.addMark("方遒展示", -self.getMark("方遒展示"));
    if (correct == 0) {
        engine.loseHp(me, 1 + bonus, "卧龙演策");
        engine.logMessage("  【友-演策】正确的预测数量为零，" + who(self) + " 失去 " + std::to_string(1 + bonus) + " 点体力。");
        predictCount = std::max(1, predictCount - 1 - bonus);
    } else if (correct < (total + 1) / 2) {
        int n = 2 + bonus;
        int discarded = 0;
        for (int i = 0; i < n; ++i) {
            auto discardable = self.getHandAndEquipmentCards();
            if (discardable.empty()) break;
            auto c = AIController::chooseLeastValuableCard(discardable);
            if (!c) break;
            engine.discardCardOf(me, c, "卧龙演策", me);
            ++discarded;
        }
        engine.logMessage("  【友-演策】不足一半正确，" + who(self) + " 弃置 " + std::to_string(discarded) + " 张牌。");
    } else if (!allCorrect) {
        // 至少一半：获得一张符合“本次预测的方式”声明条件的牌（以第一项预测为声明条件）。
        // 方遒已展示时“执行效果的值均+1” → 1+bonus 张。
        int want = 1 + bonus, gained = 0;
        for (int i = 0; i < want; ++i) {
            auto c = drawMatchingDeclared(engine);
            if (!c) break;
            engine.obtainCard(me, c);
            ++gained;
        }
        engine.logMessage("  【友-演策】" + who(self) + " 根据预测方式从牌堆获得 " + std::to_string(gained) +
                          " 张符合声明条件的牌。");
    }
    lastSettleAllCorrect = allCorrect; // 方遒“可以再次发动”的判定依据（牌数>3 且全部正确）
    lastSettleTotal = total;
    if (allCorrect) {
        engine.drawCards(me, 2 + bonus, "卧龙演策");
        predictCount = std::min(7, predictCount + 1 + bonus);
        engine.logMessage("  【友-演策】全部正确，" + who(self) + " 摸 " + std::to_string(2 + bonus) +
                          " 张牌，可预测的牌数增至 " + std::to_string(predictCount) + "。");
        if (total > 3 && bonus > 0)
            self.addMark("方遒可再发动", 1); // 授权方遒再发动一次（本次“卧龙演策”预测数>3 且全对）
    } else {
        self.addMark("方遒可再发动", -self.getMark("方遒可再发动"));
    }
    preds.clear();
    active = false;
}

CardPtr YouYanCeSkill::drawMatchingDeclared(GameEngine& engine) {
    if (preds.empty()) return nullptr;
    const Pred& p = preds[0];
    auto matchFn = [&p](const CardPtr& c) {
        if (!c) return false;
        if (p.categoryMode) return (int)c->getType() == p.value;
        bool red = c->getSuit() == Suit::HEART || c->getSuit() == Suit::DIAMOND;
        return red == (p.value == 1);
    };
    // 官网为“从牌堆中获得”；牌堆无符合条件牌时不获得（不回退弃牌堆）。
    return engine.getDeck().drawRandomMatching(matchFn, extraRng());
}

YouFangQiuSkill::YouFangQiuSkill()
    : StateSkill("友-方遒",
                 "限定技，当你执行“卧龙演策”后，你可以展示你的“卧龙演策”预测，若如此做，本次“卧龙演策”的预测全部验证后，执行效果的值均+1，然后若卧龙演策预测的牌数大于3且预测全部正确，该技能可以再次发动。",
                   SkillTag::LIMITED) {}

void YouFangQiuSkill::onPredictionStarted(GameEngine& engine, Player& self) {
    PlayerPtr me = selfOf(engine, self);
    bool canAct = !spent || self.getMark("方遒可再发动") > 0;
    if (!canAct) return;
    if (!engine.askConfirm(me, "【友-方遒】展示你的“卧龙演策”预测（执行效果的值均+1）？", true)) return;
    // “该技能可以再次发动”：由满足条件的“卧龙演策”结算（牌数>3 且全部正确）授权一次，
    // 本次再发动即消耗该授权（限定技本局只发动一次，额外授权按次计）。
    if (spent && self.getMark("方遒可再发动") > 0)
        self.addMark("方遒可再发动", -1);
    spent = true;
    self.addMark("方遒展示", 1);
    engine.logMessage("  【友-方遒】" + who(self) + " 展示了“卧龙演策”预测，执行效果的值均+1。");
}

YouGongLiSkill::YouGongLiSkill()
    : StateSkill("友-共砺",
                 "身份：此模式无效排位、斗地主：锁定技，若友庞统在场且与你阵营一致，你执行“卧龙演策”可预测的牌数+1；若友徐庶在场且与你阵营一致，你“卧龙演策”预测的第一张牌的结果视为正确。",
                 SkillTag::LOCK) {}

namespace {
// 官网写作“友庞统 / 友徐庶”：本名册暂无这两名武将，故按武将 id 与显示名共同匹配，
// 将来加入 友·庞统（you_pangtong）/ 友·徐庶（you_xushu）后此分支自动生效。
bool heroMatches(const HeroPtr& hero, const std::string& id, const std::string& namePrefix) {
    if (!hero) return false;
    if (hero->getId() == id) return true;
    const std::string& n = hero->getName();
    return n == namePrefix || n.rfind(namePrefix + "·", 0) == 0;
}
// “与你阵营一致”：斗地主按 地主/农民 判定（身份模式下共砺整体无效，不参与计算）。
bool sameCampInDoudizhu(GameEngine& engine, const Player& self, const Player& other) {
    return engine.getGameMode() == GameEngine::GameMode::DOUDIZHU &&
           self.getIdentity() == other.getIdentity();
}
} // namespace

int YouGongLiSkill::gongLiPredictionCountBonus(GameEngine& engine, Player& self) const {
    if (engine.getGameMode() != GameEngine::GameMode::DOUDIZHU) return 0; // 身份：此模式无效
    for (const auto& p : engine.getAlivePlayers()) {
        if (!p || p->getId() == self.getId()) continue;
        if (heroMatches(p->getHero(), "you_pangtong", "友庞统") && sameCampInDoudizhu(engine, self, *p))
            return 1;
    }
    return 0;
}

bool YouGongLiSkill::gongLiFirstPredictionAutoCorrect(GameEngine& engine, Player& self) const {
    if (engine.getGameMode() != GameEngine::GameMode::DOUDIZHU) return false;
    for (const auto& p : engine.getAlivePlayers()) {
        if (!p || p->getId() == self.getId()) continue;
        if (heroMatches(p->getHero(), "you_xushu", "友徐庶") && sameCampInDoudizhu(engine, self, *p))
            return true;
    }
    return false;
}


// =====================================================================
//  太史慈（势）：酣战 / 战烈 / 振锋
// =====================================================================

ShiHanZhanSkill::ShiHanZhanSkill()
    : ActiveSkill("势-酣战",
                 "出牌阶段限一次，你可以选择一名其他角色，你与其依次摸牌至X张（X为各自体力上限，但每名角色单次至多摸3张），然后视为你对其使用一张【决斗】。", 1) {}

bool ShiHanZhanSkill::canActivate(GameEngine& engine, Player& self) {
    return engine.getCurrentPhase() == TurnPhase::PLAY && hasUsesLeft() &&
           !engine.getOtherAlivePlayers(self).empty();
}

// 酣战（AI）：己方手牌/体力不劣时发动（双方各摸至 X 后视为决斗，手牌多者占优）。
bool ShiHanZhanSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    for (auto t : engine.getOtherAlivePlayers(self)) {
        if (AIController::isFriend(engine, self, *t)) continue;
        if (self.getHandCardCount() >= t->getHandCardCount()) return true;
    }
    return false;
}

void ShiHanZhanSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    auto hzCands = engine.getOtherAlivePlayers(self);
    PlayerPtr t = engine.askChoosePlayer(me, hzCands, "【势-酣战】选择一名其他角色", false,
                                         hzCands.empty() ? nullptr : hzCands.front());
    if (!t) return;
    markUsed();
    // 依次摸牌至X（单次至多摸3张）
    auto xFor = [&](const Player& p) {
        switch (xMode) {
            case 1: return p.getHp();
            case 2: return p.getMaxHp() - p.getHp();
            case 3: return (int)engine.getAlivePlayers().size();
            default: return p.getMaxHp();
        }
    };
    for (int pass = 0; pass < 2; pass++) {
        PlayerPtr p = (pass == 0) ? me : t;
        int need = std::max(0, xFor(*p) - (int)p->getHandCardCount());
        int n = std::min(3, need);
        if (n > 0) engine.drawCards(p, n, "势-酣战");
        engine.logMessage("  【势-酣战】" + who(*p) + " 摸至 " + std::to_string(n) + " 张（至多3张）。");
    }
    auto jue = Card::makeVirtual("决斗", CardType::TRICK, CardSubType::JUE_DOU, {}, "势-酣战");
    engine.useCard(me, jue, {t});
    engine.logMessage("  【势-酣战】" + who(self) + " 视为对" + who(*t) + " 使用一张【决斗】。");
}

ShiZhanLieSkill::ShiZhanLieSkill()
    : TriggerSkill("势-战烈",
                 "每名角色的回合开始时，你记录X（X为此时你的攻击范围）。本回合中的前X张杀进入弃牌堆时，若此牌在弃牌堆内，你获得1个“烈”标记，你至多拥有6个“烈”标记。出牌阶段结束时，你可移除全部“烈”标记，视为使用一张无次数限制的【杀】并选择以下选项中的至多Y项（Y为你本次移除的标记数/3，向下取整）：1.此【杀】目标+1；2.此【杀】基础伤害值+1；3.此【杀】需额外弃置一张牌方可响应。4.此【杀】结算结束后你摸两张牌。") {}

void ShiZhanLieSkill::onTurnBoundary(GameEngine& engine, Player& self, Player&, bool starting) {
    if (starting) {
        // 每名角色的回合开始时，记录X（X为此时你的攻击范围/振锋修改后的X）
        recordX = currentX(engine, self);
        discardedShaThisTurn = 0;
    }
}

int ShiZhanLieSkill::currentX(GameEngine& engine, const Player& self) const {
    switch (xMode) {
        case 1: return self.getHp();
        case 2: return self.getMaxHp() - self.getHp();
        case 3: return (int)engine.getAlivePlayers().size();
        default: return self.getAttackRange();
    }
}

void ShiZhanLieSkill::onAnyCardDiscarded(GameEngine& engine, Player& self, Player&, CardPtr card) {
    if (!card || card->getSubType() != CardSubType::SHA) return;
    if (discardedShaThisTurn >= recordX) return;
    discardedShaThisTurn++;
    if (self.getMark("烈") < 6) {
        self.addMark("烈", 1);
        engine.logMessage("  【势-战烈】" + who(self) + " 获得1枚“烈”（" +
                          std::to_string(self.getMark("烈")) + "）。");
    }
}

void ShiZhanLieSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY) return;
    int lie = self.getMark("烈");
    if (lie <= 0) return;
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【势-战烈】移除全部“烈”视为使用一张【杀】？", true)) return;
    self.addMark("烈", -lie);
    int Y = lie / 3;
    engine.logMessage("  【势-战烈】" + who(self) + " 移除 " + std::to_string(lie) +
                      " 枚“烈”，Y=" + std::to_string(Y) + "。");
    // 至多 Y 项选择（固定顺序询问）
    int chosen = 0;
    bool extraTarget = false, extraDamage = false, extraCost = false, extraDraw = false;
    // 逐项询问（人类可任意组合至多 Y 项）；AI 按价值优先：基础伤害+1 > 额外弃置响应 > 目标+1 > 摸2
    auto ask = [&](const std::string& label, bool aiValue) {
        if (chosen >= Y) return false;
        if (engine.askConfirm(me, "【势-战烈】执行：" + label + "？", aiValue)) { chosen++; return true; }
        return false;
    };
    extraTarget = ask("目标+1（对一名其他角色使用）", false);
    extraDamage = ask("基础伤害值+1", true);
    extraCost = ask("响应此【杀】的角色需额外弃置一张牌方可响应", true);
    extraDraw = ask("结算结束后摸2张牌", false);
    // 目标选择
    std::vector<PlayerPtr> cands = engine.getOtherAlivePlayers(self);
    PlayerPtr t1 = engine.askChoosePlayer(me, cands, "【势-战烈】选择【杀】的目标", true);
    if (!t1) return;
    std::vector<PlayerPtr> targets{t1};
    if (extraTarget) {
        std::vector<PlayerPtr> rest;
        for (auto x : cands) if (x->getId() != t1->getId()) rest.push_back(x);
        if (!rest.empty()) {
            PlayerPtr t2 = engine.askChoosePlayer(me, rest, "【势-战烈】目标+1：再选择一名角色", true);
            if (t2) targets.push_back(t2);
        }
    }
    // 通过标记向 onShaTargeted/onShaFinished 传递强化
    self.addMark("战烈伤1", extraDamage ? 1 : 0);
    self.addMark("战烈弃应", extraCost ? 1 : 0);
    self.addMark("战烈摸2", extraDraw ? 1 : 0);
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "势-战烈");
    engine.useCard(me, sha, targets);
    engine.logMessage("  【势-战烈】" + who(self) + " 视为使用一张【杀】。");
}

void ShiZhanLieSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (ctx.card && ctx.card->getSkillSource() == "势-战烈") {
        if (self.getMark("战烈伤1")) {
            ctx.extraDamage += 1;
            engine.logMessage("  【势-战烈】基础伤害值+1！");
        }
        if (self.getMark("战烈弃应")) {
            ctx.extraResponseCost = 1;
            engine.logMessage("  【势-战烈】响应此【杀】的角色需额外弃置一张牌方可响应！");
        }
    }
}

void ShiZhanLieSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (ctx.card && ctx.card->getSkillSource() == "势-战烈") {
        self.addMark("战烈伤1", -self.getMark("战烈伤1"));
        if (self.getMark("战烈摸2")) {
            engine.drawCards(selfOf(engine, self), 2, "势-战烈");
            self.addMark("战烈摸2", -self.getMark("战烈摸2"));
        }
    }
    self.addMark("战烈弃应", -self.getMark("战烈弃应")); // 由响应侧读取后复位
}

ShiZhenFengSkill::ShiZhenFengSkill()
    : ActiveSkill("势-振锋",
                 "限定技。出牌阶段，你可以选择一项：1.回复2点体力；2.分别修改“酣战”及“战烈”的X为当前体力值、已损失体力值、存活角色数中的一项（拥有对应技能方可选择）。", 1, SkillTag::LIMITED) {}

bool ShiZhenFengSkill::canActivate(GameEngine& engine, Player& self) {
    return !spent && engine.getCurrentPhase() == TurnPhase::PLAY && hasUsesLeft();
}

// 振锋（AI，限定技）：受伤时优先回血；否则仅当同时拥有酣战与战烈（修改 X）时使用。
bool ShiZhenFengSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (self.isWounded()) return true;
    return self.getHero() && self.getHero()->findSkill("势-酣战") && self.getHero()->findSkill("势-战烈");
}

void ShiZhenFengSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    int opt = engine.askChooseOption(me, {"回复2点体力", "修改“酣战”及“战烈”的X"},
                                     "【势-振锋】选择一项", self.isWounded() ? 0 : 1);
    markUsed();
    spent = true; // 限定技：每局游戏限一次
    if (opt == 0) {
        engine.recoverHp(me, 2, "势-振锋");
        engine.logMessage("  【势-振锋】" + who(self) + " 回复2点体力。");
        return;
    }
    int mode = engine.askChooseOption(me, {"当前体力值", "已损失体力值", "存活角色数"},
                                      "【势-振锋】修改X为", 0);
    engine.logMessage("  【势-振锋】" + who(self) + " 修改X为" +
                      std::string(mode == 0 ? "当前体力值" : mode == 1 ? "已损失体力值" : "存活角色数") + "。");
    // 修改同武将的“酣战”与“战烈”的X
    if (self.getHero()) {
        if (auto* h = dynamic_cast<ShiHanZhanSkill*>(self.getHero()->findSkill("势-酣战").get()))
            h->xMode = mode + 1;
        if (auto* l = dynamic_cast<ShiZhanLieSkill*>(self.getHero()->findSkill("势-战烈").get()))
            l->xMode = mode + 1;
    }
}

// =====================================================================
//  董昭（势）：妙略 / 迎驾
// =====================================================================

ShiMiaoLveSkill::ShiMiaoLveSkill()
    : StateSkill("势-妙略",
                 "游戏开始时，你获得两张【瞒天过海】；当你受到伤害后，你可以选择一项：1.摸两张牌；2.从牌堆或弃牌堆中获得一张智囊。",
                 SkillTag::NONE) {}

void ShiMiaoLveSkill::onGameStart(GameEngine& engine, Player& self) {
    PlayerPtr me = selfOf(engine, self);
    for (int i = 0; i < 2; i++) {
        auto match = [](const CardPtr& c) {
            return c && c->getName() == "瞒天过海" && c->getSubType() == CardSubType::MANTIAN_GUOHAI;
        };
        CardPtr c = engine.getDeck().drawRandomMatching(match, extraRng());
        if (c) engine.obtainCard(me, c);
        else {
            auto v = Card::makeVirtual("瞒天过海", CardType::TRICK, CardSubType::MANTIAN_GUOHAI, {}, "势-妙略");
            me->addHandCards({v});
            engine.notifyCardsObtained(me, 1);
        }
    }
    engine.logMessage("  【势-妙略】" + who(self) + " 获得两张【瞒天过海】。");
}

void ShiMiaoLveSkill::onAfterDamage(GameEngine& engine, Player& self, Player*, int, ShaElement, CardPtr) {
    PlayerPtr me = selfOf(engine, self);
    int opt = engine.askChooseOption(me, {"摸两张牌", "从牌堆或弃牌堆随机获得一张“智囊”"},
                                     "【势-妙略】选择一项", self.getHandCardCount() < 2 ? 0 : 1);
    if (opt == 0) {
        engine.drawCards(me, 2, "势-妙略");
        return;
    }
    auto isZhiNang = [](const CardPtr& c) {
        if (!c) return false;
        auto st = c->getSubType();
        return st == CardSubType::GUO_HE_CHAI_QIAO || st == CardSubType::WU_XIE_KE_JI ||
               st == CardSubType::WU_ZHONG_SHENG_YOU;
    };
    CardPtr c = engine.getDeck().drawRandomMatching(isZhiNang, extraRng());
    if (!c) {
        std::vector<CardPtr> cands;
        for (auto x : engine.getDeck().getDiscardPile()) if (isZhiNang(x)) cands.push_back(x);
        if (!cands.empty()) {
            std::uniform_int_distribution<size_t> d(0, cands.size() - 1);
            c = cands[d(extraRng())];
            engine.getDeck().removeDiscardCard(c);
        }
    }
    if (c) {
        engine.obtainCard(me, c);
        engine.logMessage("  【势-妙略】" + who(self) + " 随机获得一张“智囊”【" + c->getName() + "】。");
    } else engine.logMessage("  【势-妙略】牌堆与弃牌堆中没有“智囊”了。");
}

ShiYingJiaSkill::ShiYingJiaSkill()
    : StateSkill("势-迎驾",
                 "限定技，一名角色的回合结束后，若你本回合使用了大于等于两张同名锦囊牌，你可以弃置一张手牌，令一名角色执行一个额外的回合，此额外回合开始时，其摸两张牌。",
                 SkillTag::LIMITED) {}

void ShiYingJiaSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (starting) {
        // “你本回合使用了≥2张同名锦囊牌”：计数按每个回合（任意角色）重置。
        usedTrickCountA = usedTrickCountB = 0;
        nameA.clear(); nameB.clear();
        (void)self;
        // 额外回合开始时摸两张（若为迎驾指定的角色）
        if (turnOwner.getMark("迎驾额外回合") > 0) {
            turnOwner.addMark("迎驾额外回合", -turnOwner.getMark("迎驾额外回合"));
            PlayerPtr p = engine.getPlayerById(turnOwner.getId());
            engine.drawCards(p, 2, "势-迎驾");
            engine.logMessage("  【势-迎驾】" + who(turnOwner) + " 额外回合开始，摸两张牌。");
        }
    }
}

void ShiYingJiaSkill::onAnyCardUsed(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    if (!card || card->getType() != CardType::TRICK) return;
    if (user.getId() != self.getId()) return; // 官网：“你本回合使用了…”仅统计自己使用的锦囊
    (void)engine;
    // 记录本回合内使用的锦囊名称（只可能有一个名称达到2张，故记录两个候选）
    const std::string& n = card->getName();
    if (n == nameA) usedTrickCountA++;
    else if (n == nameB) usedTrickCountB++;
    else if (nameA.empty()) { nameA = n; usedTrickCountA = 1; }
    else if (nameB.empty()) { nameB = n; usedTrickCountB = 1; }
    else if (usedTrickCountA <= usedTrickCountB) { nameA = n; usedTrickCountA = 1; }
    else { nameB = n; usedTrickCountB = 1; }
}

void ShiYingJiaSkill::onTurnEnd(GameEngine& engine, Player& self, Player& turnOwner) {
    if (spent) return; // 限定技：每局游戏限一次
    bool twice = (usedTrickCountA >= 2) || (usedTrickCountB >= 2);
    if (!twice) return;
    if (self.getHandCardCount() <= 0) return; // 费用为弃置一张手牌
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【势-迎驾】弃置一张手牌并令一名角色获得额外回合？", true)) return;
    auto c = AIController::chooseLeastValuableCard(self.getHandCards());
    if (!c) return;
    engine.discardCardOf(me, c, "势-迎驾", me);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getAlivePlayers(),
                                         "【势-迎驾】选择获得额外回合的角色", true, me);
    if (!t) return;
    spent = true;
    t->addMark("迎驾额外回合", 1);
    engine.scheduleExtraTurn(t);
    engine.logMessage("  【势-迎驾】" + who(self) + " 令" + who(*t) + " 获得一个额外的回合。");
    (void)turnOwner;
}


// =====================================================================
//  于吉（势）：符济 / 道转
// =====================================================================

ShiFuJiSkill::ShiFuJiSkill()
    : ActiveSkill("势-符济",
                 "出牌阶段限一次，你可展示至多全场存活的其他角色数张牌并交给等量名其他角色，这些牌称为“符济”牌。其他角色使用“符济”牌时，获得一张与“符济”牌相同花色的牌。若“符济”牌为【杀】，此【杀】的基础伤害值+1；若“符济”牌为【闪】，则使用结算后使用者摸一张牌。 若你发动此技能后手牌数为全场最少，则你摸一张牌，且直至下回合开始前，你使用的第一张【杀】和【闪】带有“符济”牌效果。", 1) {}

bool ShiFuJiSkill::canActivate(GameEngine& engine, Player& self) {
    return engine.getCurrentPhase() == TurnPhase::PLAY && hasUsesLeft() && self.getHandCardCount() > 0 &&
           !engine.getOtherAlivePlayers(self).empty();
}

// 符济（AI）：有牌可交且场上有其他角色即可发动（把牌给友方、以“符济”效果增益团队）。
bool ShiFuJiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

void ShiFuJiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine,self)) return;
    PlayerPtr me = selfOf(engine, self);
    int maxN = (int)engine.getOtherAlivePlayers(self).size();
    if (maxN <= 0) return;
    // 官网：“你可展示至多全场存活的其他角色数张牌并交给等量名其他角色”——张数与具体牌都由你选择。
    int limit = std::min(maxN, (int)self.getHandCardCount());
    std::vector<std::string> countOptions;
    for (int i = 1; i <= limit; i++) countOptions.push_back(std::to_string(i) + " 张");
    int n = engine.askChooseOption(me, countOptions, "【势-符济】选择展示并交出的牌数",
                                   limit - 1) + 1; // AI 默认取上限
    n = std::max(1, std::min(limit, n));
    std::vector<CardPtr> shown;
    auto hand = self.getHandCards();
    for (int i = 0; i < n && !hand.empty(); i++) {
        CardPtr c = engine.askChooseCard(me, hand, "【势-符济】选择一张要展示并交出的牌", false,
                                         AIController::chooseLeastValuableCard(hand));
        if (!c) break;
        shown.push_back(c);
        hand.erase(std::find(hand.begin(), hand.end(), c));
    }
    if (shown.empty()) return;
    engine.logMessage("  【势-符济】" + who(self) + " 展示 " + std::to_string(shown.size()) +
                      " 张牌作为“符济”牌。");
    // “交给等量名其他角色”：每张牌交给不同的其他角色。
    std::set<int> usedTargets;
    for (size_t i = 0; i < shown.size(); i++) {
        auto all = engine.getOtherAlivePlayers(self);
        std::vector<PlayerPtr> others;
        for (auto& p : all) if (!usedTargets.count(p->getId())) others.push_back(p);
        if (others.empty()) break;
        PlayerPtr t = engine.askChoosePlayer(me, others,
            "【势-符济】将 " + shown[i]->getFormattedName() + " 交给一名其他角色", true);
        if (!t) t = others.front();
        usedTargets.insert(t->getId());
        engine.obtainCard(t, shown[i], me);
        fujiCardIds.push_back(shown[i]->getId());
        engine.logMessage("  【势-符济】" + shown[i]->getFormattedName() + " 交给了 " + who(*t) + "。");
    }
    markUsed();
    // 发动后手牌数全场最少：摸一张并强化下回合的第一张杀/闪
    bool least = true;
    for (auto& p : engine.getAlivePlayers())
        if (p->getId() != self.getId() && (int)p->getHandCardCount() < (int)self.getHandCardCount())
            least = false;
    if (least) {
        engine.drawCards(me, 1, "势-符济");
        selfBuff = true;
        engine.logMessage("  【势-符济】" + who(self) + " 手牌数为全场最少，摸一张牌并强化下回合前第一张【杀】/【闪】。");
    }
}

void ShiFuJiSkill::onAnyCardUsed(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    if (!card) return;
    // 自身强化：直至你下回合开始前，你使用的第一张【杀】带“符济”效果
    if (selfBuff && !selfBuffShaUsed && user.getId() == self.getId() &&
        card->getSubType() == CardSubType::SHA) {
        selfBuffShaUsed = true;
        selfBuffSha = card;
        engine.logMessage("  【势-符济】" + who(self) + " 本回合第一张【杀】带有“符济”效果。");
    }
    // 其他角色使用“符济”牌时：获得一张与“符济”牌相同花色的牌
    bool isFuji = std::find(fujiCardIds.begin(), fujiCardIds.end(), card->getId()) != fujiCardIds.end();
    if (!isFuji) return;
    Suit s = engine.effectiveSuit(user, card);
    // 指定用户获得同花色牌：从牌堆/弃牌堆找
    auto match = [s](const CardPtr& c) {
        if (!c) return false;
        return (c->getSuit() == s);
    };
    CardPtr gain = engine.getDeck().drawRandomMatching(match, extraRng());
    if (!gain) {
        std::vector<CardPtr> cands;
        for (auto x : engine.getDeck().getDiscardPile()) if (match(x)) cands.push_back(x);
        if (!cands.empty()) {
            std::uniform_int_distribution<size_t> d(0, cands.size() - 1);
            gain = cands[d(extraRng())];
            engine.getDeck().removeDiscardCard(gain);
        }
    }
    if (gain) {
        PlayerPtr u = engine.getPlayerById(user.getId());
        engine.obtainCard(u, gain);
        engine.logMessage("  【势-符济】" + who(user) + " 使用“符济”牌，获得一张同花色【" +
                          gain->getName() + "】。");
    }
}

void ShiFuJiSkill::onAnyCardPlayed(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    if (!card) return;
    // 2026-10-06 实现级复核：引擎对响应打出的【闪】只广播 onAnyCardPlayed /
    // onCardResolvedByAny（“使用”钩子不触发），原先写在 onAnyCardUsed 里的
    // “自身强化的第一张【闪】”永远不会生效——在此捕获。
    if (!selfBuff || selfBuffShanUsed || user.getId() != self.getId()) return;
    if (card->getSubType() != CardSubType::SHAN) return;
    selfBuffShanUsed = true;
    selfBuffShanCard = card;
    engine.logMessage("  【势-符济】" + who(self) + " 第一张【闪】带有“符济”效果。");
}

void ShiFuJiSkill::onCardResolvedByAny(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    // 官网：“若‘符济’牌为【闪】，则使用结算后使用者摸一张牌。”
    // 引擎在【闪】打出后向全场广播本钩子（自身强化的第一张【闪】同样带此效果）。
    if (!card || card->getSubType() != CardSubType::SHAN) return;
    bool isGift = std::find(fujiCardIds.begin(), fujiCardIds.end(), card->getId()) != fujiCardIds.end();
    bool isOwnBuff = selfBuffShanCard && card == selfBuffShanCard;
    if (!isGift && !isOwnBuff) return;
    if (isOwnBuff) selfBuffShanCard = nullptr; // 只结算一次
    PlayerPtr u = engine.getPlayerById(user.getId());
    if (!u || !u->isAlive()) return;
    engine.drawCards(u, 1, "势-符济");
    engine.logMessage("  【势-符济】" + who(user) + " 使用“符济”【闪】，摸一张牌。");
}

void ShiFuJiSkill::onCalculateShaDamage(GameEngine&, const Player&, const Player&, const Player&, CardPtr card, int& damage) {
    if (!card || card->getSubType() != CardSubType::SHA) return;
    // “符济”【杀】基础伤害+1；自身强化的第一张【杀】同样带此效果。
    if (std::find(fujiCardIds.begin(), fujiCardIds.end(), card->getId()) != fujiCardIds.end() ||
        (selfBuffSha && card == selfBuffSha)) {
        damage += 1;
    }
}

void ShiFuJiSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    // 2026-10-06 实现级复核：文本是“直至下回合开始前，你使用的**第一张**【杀】和【闪】”，
    // 原先在任意角色的回合开始都清空使用标记，会让强化在窗口内每个回合都能再用一次；
    // 且按轮次比较在“同轮次内获得额外回合”的边缘情况下会漏掉到期。
    // 现在只在**你自己的回合开始**时到期（窗口内两张牌各仅一次）。
    if (!starting || turnOwner.getId() != self.getId()) return;
    bool hadBuff = selfBuff;
    selfBuff = false;
    selfBuffShaUsed = false;
    selfBuffShanUsed = false;
    selfBuffSha = nullptr;
    selfBuffShanCard = nullptr;
    if (hadBuff) engine.logMessage("  【势-符济】" + who(self) + " 的“符济”强化已到期。");
}

ShiDaoZhuanSkill::ShiDaoZhuanSkill()
    : TriggerSkill("势-道转",
                 "每回合限一次，当你需要使用基本牌时，你可将你或当前回合角色的一张牌置入弃牌堆，视为使用此牌（每轮每牌名限一次）。若当前回合角色本次失去了牌，本轮次本技能失效。") {}

void ShiDaoZhuanSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool starting) {
    if (starting) usedThisTurnFlag = false; // 每回合限一次：任意角色的新回合开始时重置
}

bool ShiDaoZhuanSkill::onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) {
    if (wanted != CardSubType::SHA && wanted != CardSubType::SHAN && wanted != CardSubType::TAO &&
        wanted != CardSubType::JIU) return false;
    if (usedThisTurnFlag) return false;                          // 每回合限一次
    if (roundDisabled == engine.getCurrentRound()) return false; // 当前回合角色失去过牌：本轮失效
    std::string name = cardNameFor(wanted);
    auto it = nameRoundUsed.find(name);
    if (it != nameRoundUsed.end() && it->second == engine.getCurrentRound()) return false; // 每轮每牌名限一次
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【势-道转】将一张牌置入弃牌堆视为使用基本牌？", true)) return false;
    // “一张牌”可从手牌或装备区选择；未写“区域内”，故不选判定区。
    std::vector<CardPtr> cands = self.getHandAndEquipmentCards();
    PlayerPtr cur = engine.getCurrentPlayer();
    if (cur && cur->getId() != self.getId()) {
        auto currentCards = cur->getHandAndEquipmentCards();
        cands.insert(cands.end(), currentCards.begin(), currentCards.end());
    }
    if (cands.empty()) return false;
    CardPtr c = engine.askChooseCard(me, cands, "【势-道转】选择置入弃牌堆的牌", false,
                                     AIController::chooseLeastValuableCard(cands));
    if (!c) return false;
    // 按实际持有者移动牌：装备也可作为代价；不允许把未在候选区的牌送入弃牌堆。
    bool currentTurnPlayerLost = false;
    if (self.hasHandCard(c) || self.hasEquipment(c)) {
        engine.discardCardOf(me, c, "势-道转", me);
        currentTurnPlayerLost = (cur && cur->getId() == self.getId());
    } else if (cur && (cur->hasHandCard(c) || cur->hasEquipment(c))) {
        engine.discardCardOf(cur, c, "势-道转", me);
        currentTurnPlayerLost = true;
    } else {
        return false;
    }
    usedThisTurnFlag = true;
    nameRoundUsed[name] = engine.getCurrentRound();
    // 官网：“若当前回合角色本次失去了牌，本轮次本技能失效。”
    if (currentTurnPlayerLost) {
        roundDisabled = engine.getCurrentRound();
        engine.logMessage("  【势-道转】当前回合角色失去了牌，本轮次本技能失效。");
    }
    out = Card::makeVirtual(cardNameFor(wanted), CardType::BASIC, wanted, {}, "势-道转");
    engine.logMessage("  【势-道转】" + who(self) + " 视为使用一张【" + out->getName() + "】。");
    return true;
}

std::string ShiDaoZhuanSkill::cardNameFor(CardSubType st) {
    if (st == CardSubType::SHA) return "杀";
    if (st == CardSubType::SHAN) return "闪";
    if (st == CardSubType::TAO) return "桃";
    if (st == CardSubType::JIU) return "酒";
    return "基本牌";
}

// =====================================================================
//  辛宪英（势）：诫节 / 清识
// =====================================================================

namespace {

// 清识效果：选择一名角色，阵营相同各摸一张，不同各弃一张
void performQingShiDirect(GameEngine& engine, Player& self, Player& target) {
    PlayerPtr me = engine.getPlayerById(self.getId());
    PlayerPtr t = engine.getPlayerById(target.getId());
    if (!me || !t) return;
    // 若目标与发动者为同一人（理论上不应发生于“视为发动”路径），则视为阵营相同但需避免对同一人重复摸2的歧义日志
    if (t->getId() == me->getId()) {
        engine.drawCards(me, 1, "势-清识");
        engine.logMessage("  【势-清识】阵营相同，" + who(self) + " 与 " + who(*t) + " 各摸一张牌。（同角色仅摸一张）");
        return;
    }
    if (AIController::isFriend(engine, self, *t)) {
        engine.drawCards(me, 1, "势-清识");
        engine.drawCards(t, 1, "势-清识");
        engine.logMessage("  【势-清识】阵营相同，" + who(self) + " 与 " + who(*t) + " 各摸一张牌。");
    } else {
        for (auto p : {me, t}) {
            auto discardable = p->getHandAndEquipmentCards();
            auto c = AIController::chooseLeastValuableCard(discardable);
            if (c) engine.discardCardOf(p, c, "势-清识", me);
        }
        engine.logMessage("  【势-清识】阵营不同，" + who(self) + " 与 " + who(*t) + " 各弃置一张牌。");
    }
}
void performQingShi(GameEngine& engine, Player& self) {
    PlayerPtr me = engine.getPlayerById(self.getId());
    // 优先选阵营相同且非自己的角色，避免 AI 自选导致“与自己各摸一张”的歧义
    PlayerPtr prefer;
    for (auto& x : engine.getAlivePlayers()) {
        if (x->getId() == self.getId()) continue;
        if (AIController::isFriend(engine, self, *x)) { prefer = x; break; }
    }
    if (!prefer) {
        // 若无其他同阵营角色，才考虑自己（符合“一名角色”可含自己）
        for (auto& x : engine.getAlivePlayers())
            if (AIController::isFriend(engine, self, *x)) { prefer = x; break; }
    }
    // 候选包含全部存活角色（含自己），但 AI 默认不选自己
    PlayerPtr t = engine.askChoosePlayer(me, engine.getAlivePlayers(), "【势-清识】选择一名角色", false,
                                         prefer ? prefer : engine.getAlivePlayers().front());
    if (!t) return;
    performQingShiDirect(engine, self, *t);
}

} // namespace

ShiJieJieSkill::ShiJieJieSkill()
    : TriggerSkill("势-诫节",
                 "每名角色的出牌阶段限一次，当前回合角色可以令你观看其手牌，然后你可以选择一种花色，若其手牌：1.包含此花色，其弃置所有不为此花色的手牌，本回合使用此花色的牌无次数限制；2.不含此花色，其从牌堆或弃牌堆中获得一张此花色的牌。每轮限两次，若其本轮以此法令你观看的牌所包含的花色数唯一最多，你视为对其发动“清识”。") {}

void ShiJieJieSkill::onRoundStart(GameEngine& engine, Player& self) {
    qingShiUsed = 0;               // “每轮限两次”限制的是视为发动清识的次数
    watchedThisRound.clear();
    watchedSeats.clear();
    (void)engine; (void)self;
}

void ShiJieJieSkill::onAnyPhaseStart(GameEngine& engine, Player& self, Player& turnOwner, TurnPhase phase) {
    if (phase != TurnPhase::PLAY) return;
    PlayerPtr cur = engine.getPlayerById(turnOwner.getId());
    if (!cur || cur->getId() == self.getId()) return;
    // “每名角色的出牌阶段限一次”：同一角色本轮再次获得出牌阶段时不再触发。
    if (watchedSeats.count(cur->getId())) return;
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(cur, "【势-诫节】令 " + who(self) + " 观看你的手牌？", true)) return;
    watchedSeats.insert(cur->getId());
    auto hand = cur->getHandCards();
    std::string seen;
    std::set<int> suits;
    for (auto c : hand) {
        if (!c) continue;
        int s = (int)c->getSuit();
        suits.insert(s);
        seen += c->getFormattedName() + " ";
    }
    engine.logMessage("  【势-诫节】" + who(*cur) + " 的手牌：" + (seen.empty() ? "（无）" : seen));
    if (hand.empty()) return;
    int suitOpt = engine.askChooseOption(me, {"黑桃", "红桃", "梅花", "方块"}, "【势-诫节】选择一种花色", 0);
    Suit chosen = (Suit)suitOpt;
    bool has = suits.count(suitOpt) > 0;
    if (has) {
        // 其弃置所有不为此花色的手牌
        std::vector<CardPtr> toDiscard;
        for (auto c : hand) if (c && c->getSuit() != chosen) toDiscard.push_back(c);
        for (auto c : toDiscard) engine.discardCardOf(cur, c, "势-诫节", me);
        cur->addMark("诫节花色", (int)chosen + 1); // 本回合使用此花色的牌无次数限制
        engine.logMessage("  【势-诫节】" + who(*cur) + " 弃置所有不为该花色的手牌，本回合该花色牌无次数限制。");
    } else {
        auto match = [chosen](const CardPtr& c) { return c && c->getSuit() == chosen; };
        CardPtr g = engine.getDeck().drawRandomMatching(match, extraRng());
        if (!g) {
            std::vector<CardPtr> cands;
            for (auto x : engine.getDeck().getDiscardPile()) if (match(x)) cands.push_back(x);
            if (!cands.empty()) {
                std::uniform_int_distribution<size_t> d(0, cands.size() - 1);
                g = cands[d(extraRng())];
                engine.getDeck().removeDiscardCard(g);
            }
        }
        if (g) {
            engine.obtainCard(cur, g);
            engine.logMessage("  【势-诫节】" + who(*cur) + " 获得一张" +
                              std::string(g->getSuit() == Suit::SPADE ? "♠" : g->getSuit() == Suit::HEART ? "♥" :
                              g->getSuit() == Suit::CLUB ? "♣" : "♦") + "牌。");
        }
    }
    // 记录本轮该角色观看的花色集合；若花色数唯一最多→视为发动清识
    watchedThisRound.push_back({cur->getId(), (int)suits.size()});
    int maxSize = 0;
    for (auto& [id, n] : watchedThisRound) if (n > maxSize) maxSize = n;
    int maxCount = 0;
    for (auto& [id, n] : watchedThisRound) if (n == maxSize) maxCount++;
    bool uniqueMax = false;
    for (auto& [id, n] : watchedThisRound)
        if (id == cur->getId() && n == maxSize && maxCount == 1) uniqueMax = true;
    if (uniqueMax && qingShiUsed < 2) { // “每轮限两次……视为对其发动清识”
        qingShiUsed++;
        engine.logMessage("  【势-诫节】" + who(*cur) + " 花色数唯一最多，视为对其发动“清识”。");
        // “对其”指被观看的当回合角色，直接以 cur 为目标，而非自由选人（避免 AI 自选导致“与自己各摸一张”）
        performQingShiDirect(engine, self, *cur);
    }
}

ShiQingShiSkill::ShiQingShiSkill()
    : TriggerSkill("势-清识",
                 "当你受到伤害后，你可以选择一名角色，然后若你与其阵营：相同，你与其各摸一张牌；不同，你弃置你与其的各一张牌。") {}

void ShiQingShiSkill::onAfterDamage(GameEngine& engine, Player& self, Player*, int, ShaElement, CardPtr) {
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【势-清识】发动？", true)) return;
    performQingShi(engine, self);
}

// =====================================================================
//  鲁肃（势）：好施 / 缔盟
// =====================================================================

ShiHaoShiSkill::ShiHaoShiSkill()
    : StateSkill("势-好施",
                 "结束阶段，你可以选择一名其他角色，直到你的下个回合开始，其可以如手牌般使用或打出你的手牌。然后你前两次因此失去最后的手牌时，你将手牌摸至三张。",
                 SkillTag::NONE) {}

void ShiHaoShiSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    PlayerPtr me = selfOf(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self),
                                         "【势-好施】选择一名其他角色使用你的手牌", true);
    if (!t) return;
    t->addMark("好施借牌", self.getId() + 1);
    engine.logMessage("  【势-好施】" + who(*t) + " 直到" + who(self) +
                      " 下个回合开始前可以使用或打出 " + who(self) + " 的手牌。");
}

void ShiHaoShiSkill::onBorrowedHandCardUsed(GameEngine& engine, Player& self, Player& borrower, CardPtr card) {
    (void)card;
    // 2026-10-06 实现级复核：官网“然后你前两次**因此**失去最后的手牌时”——“因此”指
    // 因【好施】借出的牌被对方使用/打出。原先挂在“一次失去≥2张牌”的批量钩子上，
    // 既漏掉最常见的一次借牌使用，又会把其它原因的成批失去牌误计在内。
    if (self.getHandCardCount() != 0) return;         // 失去的是最后的手牌
    if (lostLastTurnCount >= 2) return;               // 前两次
    lostLastTurnCount++;
    engine.drawCards(selfOf(engine, self), 3, "势-好施");
    engine.logMessage("  【势-好施】" + who(self) + " 因借出的牌被 " + who(borrower) +
                      " 使用而失去最后的手牌（第 " + std::to_string(lostLastTurnCount) + " 次），将手牌摸至三张。");
}

void ShiHaoShiSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (!starting || turnOwner.getId() != self.getId()) return;
    // 直到你的下个回合开始：清除所有角色的“好施借牌”标记
    for (auto& p : engine.getAlivePlayers()) {
        if (p->getMark("好施借牌") > 0) {
            p->addMark("好施借牌", -p->getMark("好施借牌"));
            engine.logMessage("  【势-好施】" + who(*p) + " 使用" + who(self) + " 手牌的效果结束。");
        }
    }
}

ShiDiMengSkill::ShiDiMengSkill()
    : ActiveSkill("势-缔盟",
                 "出牌阶段限一次，你可以令两名手牌数之差小于等于3的角色交换手牌，然后你选择一项：交换后手牌数较少的角色摸X张牌（X为你已损失的体力值）；弃置X张牌（不足则全弃）。", 1) {}

bool ShiDiMengSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !hasUsesLeft()) return false;
    auto all = engine.getAlivePlayers();
    for (size_t i = 0; i < all.size(); i++)
        for (size_t j = i + 1; j < all.size(); j++)
            if (std::abs((int)all[i]->getHandCardCount() - (int)all[j]->getHandCardCount()) <= 3)
                return true;
    return false;
}

// 缔盟（AI，C2）：只有存在“敌方手牌多、己方手牌少且手牌差 ≤3”的一对时才发动——
// 否则交换手牌等于随机洗牌，可能资敌。
namespace {
// 找出一对 (aiA=敌方手牌多者, aiB=己方手牌少者)，要求 |手牌差| ≤ 3；找不到返回 false。
bool findDiMengPair(GameEngine& engine, Player& self, PlayerPtr& aiA, PlayerPtr& aiB) {
    int bestGain = 0;
    aiA = aiB = nullptr;
    for (auto& x : engine.getAlivePlayers()) {
        if (!x || x->getId() == self.getId()) continue;
        if (AIController::isFriend(engine, self, *x)) continue;          // x 必须是敌方
        for (auto& y : engine.getAlivePlayers()) {
            if (!y || y->getId() == x->getId()) continue;
            const bool yMine = (y->getId() == self.getId()) || AIController::isFriend(engine, self, *y);
            if (!yMine) continue;                                        // y 是自己或队友
            if (std::abs(static_cast<int>(x->getHandCardCount()) - static_cast<int>(y->getHandCardCount())) > 3) continue;
            int gain = x->getHandCardCount() - y->getHandCardCount();     // 从牌多的敌人换给牌少的队友
            if (gain > bestGain) { bestGain = gain; aiA = x; aiB = y; }
        }
    }
    return static_cast<bool>(aiA && aiB);
}
} // namespace

bool ShiDiMengSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    PlayerPtr a, b;
    return findDiMengPair(engine, self, a, b);
}

void ShiDiMengSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    // C2：AI 用上面找到的那一对（敌方手牌多者 + 己方手牌少者），人类仍可自由选择
    PlayerPtr aiA, aiB;
    if (me->isAI()) {
        if (!findDiMengPair(engine, self, aiA, aiB)) return;   // 没有划算的一对就不空发
    }
    PlayerPtr a = engine.askChoosePlayer(me, engine.getAlivePlayers(), "【势-缔盟】选择第一名角色", true, aiA);
    if (!a) return;
    std::vector<PlayerPtr> bCands;
    for (auto& x : engine.getAlivePlayers()) if (x->getId() != a->getId()) bCands.push_back(x);
    PlayerPtr bDefault = aiB ? aiB : (bCands.empty() ? nullptr : bCands.front());
    if (aiA && aiB && a->getId() == aiB->getId()) bDefault = aiA; // 人类/AI 先选了“少牌的一方”也能配上对
    PlayerPtr b = engine.askChoosePlayer(me, bCands, "【势-缔盟】选择第二名角色", false, bDefault);
    if (!b || a->getId() == b->getId()) return;
    if (std::abs((int)a->getHandCardCount() - (int)b->getHandCardCount()) > 3) {
        engine.logMessage("  【势-缔盟】两名角色手牌数之差大于3，发动失败。");
        return;
    }
    markUsed();
    // 交换手牌
    auto ha = a->takeAllHandCards();
    auto hb = b->takeAllHandCards();
    a->addHandCards(hb);
    b->addHandCards(ha);
    engine.logMessage("  【势-缔盟】" + who(*a) + " 与 " + who(*b) + " 交换了手牌。");
    int X = self.getMaxHp() - self.getHp();
    if (X <= 0) return;
    int opt = engine.askChooseOption(me, {"交换后手牌数较少的角色摸X张牌", "弃置X张牌（不足则全弃）"},
                                     "【势-缔盟】选择一项", 0);
    if (opt == 0) {
        PlayerPtr less = (a->getHandCardCount() < b->getHandCardCount()) ? a
                       : (b->getHandCardCount() < a->getHandCardCount()) ? b : a;
        engine.drawCards(less, X, "势-缔盟");
        engine.logMessage("  【势-缔盟】" + who(*less) + " 摸 " + std::to_string(X) + " 张牌。");
    } else {
        PlayerPtr more = (a->getHandCardCount() > b->getHandCardCount()) ? a
                       : (b->getHandCardCount() > a->getHandCardCount()) ? b : a;
        int n = std::min(X, static_cast<int>(more->getHandAndEquipmentCards().size()));
        for (int i = 0; i < n; i++) {
            auto discardable = more->getHandAndEquipmentCards();
            auto c = AIController::chooseLeastValuableCard(discardable);
            if (!c) break;
            engine.discardCardOf(more, c, "势-缔盟", me);
        }
        engine.logMessage("  【势-缔盟】" + who(*more) + " 弃置 " + std::to_string(n) + " 张牌。");
    }
}

// =====================================================================
//  钟会（势）：肆恣 / 挟志 / 迂难 / 克昌
// =====================================================================

ShiSiZhiSkill::ShiSiZhiSkill()
    : ActiveSkill("势-肆恣",
                 "蓄力技（4/4）。出牌阶段限一次，你可以减少任意点蓄力点，然后执行以下效果，直至X个回合结束后或你的回合开始时（X为本次消减少的蓄力点数）：1.所有角色使用【杀】造成的伤害+1；2.一名角色的回合结束时，你摸两张牌且于本回合内使用过【杀】的角色各失去1点体力。若X大于你的体力值，执行一个额外效果：3.一名角色的回合结束时，若没有角色于本回合内使用过【杀】，当前回合角色失去1点体力。", 1) {}

void ShiSiZhiSkill::onGameStart(GameEngine&, Player& self) {
    setMark(self, "蓄力上限", 4); // 蓄力技（4/4）：上限 4（供局势显示与跨技能引用）
    if (self.getMark("蓄力") < 4) setMark(self, "蓄力", 4);
}

bool ShiSiZhiSkill::canActivate(GameEngine& engine, Player& self) {
    // FAQ（用户 2026-10-04）：“减少任意点蓄力点”允许 x=0（此时只有“X=0”的结算窗口，无任何效果）。
    return engine.getCurrentPhase() == TurnPhase::PLAY && hasUsesLeft();
}

// 肆恣（AI）：有蓄力点时发动（X>0 才有实际效果；X=0 只会浪费阶段限一次）。
bool ShiSiZhiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self) && self.getMark("蓄力") > 0;
}

void ShiSiZhiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    int total = self.getMark("蓄力");
    // FAQ（用户 2026-10-04）：允许减少 0 点（X=0）；选项索引即减少的点数。
    std::vector<std::string> sizeOpts;
    for (int i = 0; i <= total; i++) {
        sizeOpts.push_back(i == total ? ("减少全部蓄力点（" + std::to_string(total) + "点）")
                                      : ("减少" + std::to_string(i) + "点蓄力点"));
    }
    int x = engine.askChooseOption(me, sizeOpts, "【势-肆恣】选择本次减少的蓄力点数", total);
    int spend = std::max(0, std::min(x, total));
    if (spend <= 0) {
        engine.logMessage("  【势-肆恣】" + who(self) + " 减少 0 点蓄力点，本次无效果。");
        return;
    }
    markUsed();
    engine.consumeCharge(me, spend);
    // 记录效果状态
    siZhiActive = true;
    siZhiTurns = spend;
    siZhiEffect1 = true;
    siZhiEffect2 = true;
    siZhiEffect3 = (spend > self.getHp());
    self.addMark("肆恣伤1", 1);
    engine.logMessage("  【势-肆恣】" + who(self) + " 减少 " + std::to_string(spend) +
                      " 点蓄力点，" + std::to_string(spend) + " 个回合内杀伤害+1" +
                      (siZhiEffect3 ? "（含额外效果3）" : "") + "。");
}

void ShiSiZhiSkill::onCalculateShaDamage(GameEngine& engine, const Player& self, const Player&,
                                         const Player&, CardPtr card, int& damage) {
    (void)engine;
    if (siZhiActive && siZhiEffect1 && card && card->getSubType() == CardSubType::SHA && self.getMark("肆恣伤1")) {
        damage += 1;
    }
}

void ShiSiZhiSkill::onTurnEnd(GameEngine& engine, Player& self, Player& turnOwner) {
    if (!siZhiActive) return;
    // 效果2：你摸两张牌且于本回合内使用过【杀】的角色各失去1点体力（每回合结束时）
    // 效果3：若X>体力值时，额外在无杀回合令当前回合角色失去1点体力（每回合均判定，非仅最后一回合）
    if (siZhiEffect2 && self.getMark("肆恣伤1")) {
        PlayerPtr holder = selfOf(engine, self);
        engine.drawCards(holder, 2, "势-肆恣");
        bool anySha = false;
        for (auto& p : engine.getAlivePlayers()) {
            if (p->getMark("本回合用过杀") > 0) {
                anySha = true;
                PlayerPtr pp = engine.getPlayerById(p->getId());
                engine.loseHp(pp, 1, "势-肆恣", holder);
            }
        }
        engine.logMessage("  【势-肆恣】" + who(self) + " 摸两张牌。");
        if (siZhiEffect3 && !anySha) {
            PlayerPtr cur = engine.getPlayerById(turnOwner.getId());
            if (cur && cur->isAlive()) {
                engine.loseHp(cur, 1, "势-肆恣", holder);
                engine.logMessage("  【势-肆恣】本回合没有角色使用【杀】，" + who(turnOwner) + " 失去1点体力。");
            }
        }
    }
}

void ShiSiZhiSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (!siZhiActive) return;
    if (starting && turnOwner.getId() == self.getId()) {
        // 你的回合开始时效果结束
        endSiZhi(engine, self, "你的回合开始");
        return;
    }
    if (!starting) {
        siZhiTurns--;
        if (siZhiTurns <= 0) endSiZhi(engine, self, "X个回合结束");
    }
    (void)engine;
}

void ShiSiZhiSkill::endSiZhi(GameEngine& engine, Player& self, const std::string& why) {
    siZhiActive = false;
    siZhiTurns = 0;
    self.addMark("肆恣伤1", -self.getMark("肆恣伤1"));
    engine.logMessage("  【势-肆恣】效果因" + why + "而结束。");
}

ShiXieZhiSkill::ShiXieZhiSkill()
    : TriggerSkill("势-挟志",
                 "锁定技，当你的体力值变化后，你获得X点蓄力点（X为本次变化的值）。若你会因此获得超额蓄力点，你的手牌上限与使用【杀】的次数永久+1。", SkillTag::LOCK) {}

void ShiXieZhiSkill::onHpChanged(GameEngine& engine, Player& self, int delta) {
    if (delta == 0) return;
    int x = std::abs(delta);
    int cap = self.getMark("蓄力上限") > 0 ? self.getMark("蓄力上限") : 4;
    int cur = self.getMark("蓄力");
    int would = cur + x; // 视为“获得了对应数量”，只是最终不超过上限 Y
    if (would > cap) {
        // 超出的部分带来永久收益（手牌上限与使用【杀】的次数永久+超出值）
        int over = would - cap;
        self.addMark("挟志上限超", over);
        engine.logMessage("  【势-挟志】" + who(self) + " 获得 " + std::to_string(x) +
                          " 点蓄力点（超出上限），手牌上限与使用【杀】的次数永久+" +
                          std::to_string(over) + "。");
    }
    // FAQ（用户 2026-10-04）：蓄力点不超过蓄力上限 Y。
    setMark(self, "蓄力", std::min(cap, cur + x));
    engine.logMessage("  【势-挟志】" + who(self) + " 获得 " + std::to_string(x) + " 点蓄力点（当前 " +
                      std::to_string(self.getMark("蓄力")) + "/" + std::to_string(cap) + "）。");
}

void ShiXieZhiSkill::onCalculateHandLimit(GameEngine&, const Player& self, int& handLimit) {
    handLimit += self.getMark("挟志上限超");
}

void ShiXieZhiSkill::onCalculateShaLimit(GameEngine&, const Player& self, int& shaLimit) {
    shaLimit += self.getMark("挟志上限超");
}

ShiYuNanSkill::ShiYuNanSkill()
    : StateSkill("势-迂难",
                 "觉醒技，你的登场势力为魏；当你令一名角色进入濒死状态时，若本轮已有角色死亡，你将势力变更为群，然后获得或升级技能“克昌”。", SkillTag::AWAKEN) {}

void ShiYuNanSkill::tryAwaken(GameEngine& engine, Player& self, Player& dyingPlayer) {
    if (awakened) return;
    if (!engine.isAnyPlayerDeadThisRound()) return;
    int src = dyingPlayer.getMark("濒死来源");
    if (src != self.getId() + 1) return;
    awakened = true;
    if (self.getHero()) {
        self.getHero()->setAvatarIdentity(Country::QUN, Gender::MALE);
        if (auto k = dynamic_cast<ShiKeChangSkill*>(self.getHero()->findSkill("势-克昌").get())) {
            k->upgrade();
            engine.logMessage("  【势-迂难】" + who(self) + " 觉醒：势力变更为群，技能“克昌”升级为二级。");
        } else {
            // 【克昌】是主公技：主公技当且仅当你是主公时才会在游戏开始时获得（用户 2026-10-05），
            // 非主公的势·钟会身上没有克昌，觉醒只改势力。
            engine.logMessage("  【势-迂难】" + who(self) + " 觉醒：势力变更为群（“克昌”为主公技，你不是主公，故无此技能可升级）。");
        }
    }
}
void ShiYuNanSkill::onOtherDying(GameEngine& engine, Player& self, Player& dyingPlayer) {
    tryAwaken(engine, self, dyingPlayer);
}
void ShiYuNanSkill::onDying(GameEngine& engine, Player& self, Player& dyingPlayer) {
    tryAwaken(engine, self, dyingPlayer);
}

ShiKeChangSkill::ShiKeChangSkill()
    : StateSkill("势-克昌",
                 "一级：主公技，锁定技，群势力角色使用【杀】无距离限制。二级：主公技，锁定技，群势力角色使用【杀】无距离限制；你使用的【杀】不可被响应。",
        SkillTag::LOCK | SkillTag::LORD) {}   // 官网原文写明“主公技”→ 带 LORD 标签（用户 2026-10-05 的主公技口径）

void ShiKeChangSkill::onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target,
                                       CardPtr card, bool& can) {
    if (!card || card->getSubType() != CardSubType::SHA) return;
    // 群势力角色使用【杀】无距离限制（self 为使用者，校验自身势力而非目标）
    // 全局主公技的兜底：若未通过 calculateDistance 的全局修正，再此处补正
    if (!can && self.getHero() && self.getHero()->getCountry() == Country::QUN) {
        // 距离>攻击范围时仍可指定为杀目标
        if (engine.calculateDistance(self, target) > self.getAttackRange()) can = true;
    }
}

void ShiKeChangSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (level >= 2 && ctx.card && ctx.card->getSubType() == CardSubType::SHA) {
        ctx.cannotDodge = true;
        engine.logMessage("  【势-克昌】" + who(self) + " 使用的【杀】不可被响应。");
    }
}

// =====================================================================
//  邓艾（势）：屯田 / 凿险 / 急袭
// =====================================================================

ShiTunTianSkill::ShiTunTianSkill()
    : ActiveSkill("势-屯田",
                 "蓄力技（0/0），你失去非伤害牌后，获得1点蓄力点；出牌阶段限一次，你可消耗任意点蓄力点，令至多等量名角色随机获得一张红桃牌；一名角色的回合开始时，若你蓄力点已满，你摸一张牌且蓄力点上限+1。", 1) {}

bool ShiTunTianSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self) ||
        !hasUsesLeft() || self.getMark("蓄力") <= 0)
        return false;
    auto isHeart = [](const CardPtr& card) { return card && card->getSuit() == Suit::HEART; };
    const auto& deck = engine.getDeck();
    return std::any_of(deck.getDrawPile().begin(), deck.getDrawPile().end(), isHeart) ||
           std::any_of(deck.getDiscardPile().begin(), deck.getDiscardPile().end(), isHeart);
}

// 屯田（AI）：有蓄力点且牌堆/弃牌堆存在红桃牌时发动（否则空耗次数）。
bool ShiTunTianSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self); // canActivate 已要求：蓄力>0 且存在红桃牌
}

void ShiTunTianSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine,self)) return;
    PlayerPtr me = selfOf(engine, self);
    if (!me) return;
    int total = self.getMark("蓄力");
    int opt = engine.askChooseOption(me,
        {"消耗1点", "消耗2点", "消耗全部（" + std::to_string(total) + "点）"},
        "【势-屯田】消耗任意点蓄力点", total >= 2 ? 2 : 0);
    if (opt < 0 || opt > 2) return;
    int spend = (opt == 2) ? total : std::min(total, opt + 1);
    if (spend <= 0) return;

    // “至多等量名角色”没有“其他”限制，且目标不得重复；先选择再消费，避免空发动。
    std::vector<PlayerPtr> candidates = engine.getAlivePlayers();
    std::vector<PlayerPtr> targets;
    auto isHeart = [](const CardPtr& card) { return card && card->getSuit() == Suit::HEART; };
    int hearts = static_cast<int>(std::count_if(engine.getDeck().getDrawPile().begin(),
                                                engine.getDeck().getDrawPile().end(), isHeart)) +
                 static_cast<int>(std::count_if(engine.getDeck().getDiscardPile().begin(),
                                                engine.getDeck().getDiscardPile().end(), isHeart));
    int limit = std::min({spend, static_cast<int>(candidates.size()), hearts});
    for (int i = 0; i < limit && !candidates.empty(); ++i) {
        PlayerPtr aiTarget;
        for (auto candidate : candidates) if (AIController::isFriend(engine, self,*candidate)) {
            aiTarget = candidate;
            break;
        }
        if (!aiTarget) aiTarget = candidates.front();
        PlayerPtr target = engine.askChoosePlayer(me,candidates,
            "【势-屯田】选择一名角色随机获得一张红桃牌（0=结束）",true,aiTarget);
        if (!target) break;
        targets.push_back(target);
        candidates.erase(std::remove(candidates.begin(),candidates.end(),target),candidates.end());
    }
    if (targets.empty()) return;

    markUsed();
    engine.consumeCharge(me, spend);
    for (const auto& target : targets) {
        CardPtr gain = engine.getDeck().drawRandomMatching(isHeart, extraRng());
        if (!gain) {
            std::vector<CardPtr> cands;
            for (auto card : engine.getDeck().getDiscardPile()) if (isHeart(card)) cands.push_back(card);
            if (!cands.empty()) {
                std::uniform_int_distribution<size_t> distribution(0,cands.size()-1);
                gain = cands[distribution(extraRng())];
                engine.getDeck().removeDiscardCard(gain);
            }
        }
        if (gain) {
            engine.obtainCard(target,gain);
            engine.logMessage("  【势-屯田】" + who(*target) + " 随机获得一张红桃牌。");
        }
    }
}

void ShiTunTianSkill::gainCharge(GameEngine& engine, Player& self, CardPtr card) {
    // FAQ（用户 2026-10-04）：“失去非伤害牌”＝失去的牌本身不属于伤害类牌（不论失去原因）。
    if (!card || isDamageCard(card)) return;
    int cap = self.getMark("蓄力上限");
    if (self.getMark("蓄力") >= cap) return; // 已满（蓄力技（0/0）初始上限为 0，故初始不再累积）
    self.addMark("蓄力", 1);
    engine.logMessage("  【势-屯田】" + who(self) + " 失去非伤害牌，获得1点蓄力点（" +
                      std::to_string(self.getMark("蓄力")) + "）。");
}
void ShiTunTianSkill::onCardLostOutsideTurn(GameEngine&, Player&, CardPtr) {
    // 已由 onAnyCardLost 统一处理，避免双重触发
}
void ShiTunTianSkill::onAnyCardLost(GameEngine& engine, Player& self, CardPtr card) {
    gainCharge(engine, self, card);
}

void ShiTunTianSkill::onTurnBoundary(GameEngine& engine, Player& self, Player&, bool starting) {
    if (!starting) return;
    // 一名角色的回合开始时，若蓄力点已满（蓄力点≥上限；上限可为 0），摸一张且上限+1
    int cap = self.getMark("蓄力上限");
    if (self.getMark("蓄力") >= cap) {
        engine.drawCards(selfOf(engine, self), 1, "势-屯田");
        self.addMark("蓄力上限", 1);
        engine.logMessage("  【势-屯田】" + who(self) + " 蓄力点已满，摸一张牌且蓄力点上限+1（" +
                          std::to_string(self.getMark("蓄力上限")) + "）。");
    }
}

ShiZaoXianSkill::ShiZaoXianSkill()
    : TriggerSkill("势-凿险",
                 "锁定技，你一次性消耗的蓄力点数量大于等于对应值时，你从弃牌堆获得一张对应牌：3，【无中生有】；5，【无懈可击】；7，【五谷丰登】。", SkillTag::LOCK) {}

void ShiZaoXianSkill::onChargeConsumed(GameEngine& engine, Player& self, int count) {
    struct Spec { int need; CardSubType st; const char* name; };
    const Spec specs[] = {{3, CardSubType::WU_ZHONG_SHENG_YOU, "无中生有"},
                          {5, CardSubType::WU_XIE_KE_JI, "无懈可击"},
                          {7, CardSubType::WU_GU_FENG_DENG, "五谷丰登"}};
    for (auto& sp : specs) {
        if (count < sp.need) continue;
        auto match = [sp](const CardPtr& c) { return c && c->getSubType() == sp.st; };
        CardPtr g = nullptr;
        std::vector<CardPtr> cands;
        for (auto x : engine.getDeck().getDiscardPile()) if (match(x)) cands.push_back(x);
        if (!cands.empty()) {
            std::uniform_int_distribution<size_t> d(0, cands.size() - 1);
            g = cands[d(extraRng())];
            engine.getDeck().removeDiscardCard(g);
            engine.obtainCard(selfOf(engine, self), g);
            engine.logMessage("  【势-凿险】" + who(self) + " 从弃牌堆获得一张【" + sp.name + "】。");
        }
    }
}

ShiJiXiSkill::ShiJiXiSkill()
    : TriggerSkill("势-急袭",
                 "一名角色的回合结束时，若存在本回合成为过你牌目标的其他角色，你可弃置当前回合角色一张牌，以使用一张指定其中任意名角色为目标的无视距离的【顺手牵羊】。") {}

void ShiJiXiSkill::onCardTargetConfirmed(GameEngine&, Player& self, Player* source, CardPtr,
                                              const std::vector<PlayerPtr>& targets) {
    if (!source || source->getId() != self.getId()) return;
    for (const auto& target : targets) {
        if (!target || !target->isAlive() || target->getId() == self.getId()) continue;
        if (std::find(targetsThisTurn.begin(), targetsThisTurn.end(), target->getId()) == targetsThisTurn.end())
            targetsThisTurn.push_back(target->getId());
    }
}

void ShiJiXiSkill::onTurnEnd(GameEngine& engine, Player& self, Player& turnOwner) {
    if (targetsThisTurn.empty()) return;
    const std::vector<int> recordedTargets = targetsThisTurn;
    targetsThisTurn.clear();
    PlayerPtr me = selfOf(engine, self);
    auto shun = Card::makeVirtual("顺手牵羊", CardType::TRICK,
                                  CardSubType::SHUN_SHOU_QIAN_YANG, {}, "势-急袭");
    std::vector<PlayerPtr> cands;
    for (int id : recordedTargets) {
        auto target = engine.getPlayerById(id);
        if (!target || !target->isAlive() || target->getId() == self.getId() ||
            target->getAllCards().empty() || !engine.canUseShunShouOn(self, *target, shun) ||
            !engine.canBeTargeted(target, shun, me)) continue;
        cands.push_back(target);
    }
    if (cands.empty()) return;
    auto discardable = turnOwner.getHandAndEquipmentCards();
    if (discardable.empty()) return;
    if (!engine.askConfirm(me, "【势-急袭】弃置当前回合角色一张牌并视为使用【顺手牵羊】？", true)) return;

    // 先锁定至少一个仍有区域牌且合法的目标，避免付费后无法使用；之后可继续选择任意名不同目标。
    auto bestTarget = cands.front();
    for (const auto& candidate : cands)
        if (!AIController::isFriend(engine, self, *candidate)) { bestTarget = candidate; break; }
    std::vector<PlayerPtr> selected;
    PlayerPtr target = engine.askChoosePlayer(me, cands, "【势-急袭】选择至少一名【顺手牵羊】目标", false,
                                               bestTarget);
    if (!target) return;
    selected.push_back(target);
    cands.erase(std::remove(cands.begin(), cands.end(), target), cands.end());
    while (!cands.empty()) {
        PlayerPtr preferred = cands.front();
        for (const auto& candidate : cands)
            if (!AIController::isFriend(engine, self, *candidate)) { preferred = candidate; break; }
        target = engine.askChoosePlayer(me, cands, "【势-急袭】可再选择一名不同目标（0结束）", true,
                                        preferred);
        if (!target) break;
        selected.push_back(target);
        cands.erase(std::remove(cands.begin(), cands.end(), target), cands.end());
    }

    // 若当前回合角色本身也是目标，不能将其唯一一张区域牌同时作为弃置费用。
    if (turnOwner.isAlive() && std::find(selected.begin(), selected.end(), engine.getPlayerById(turnOwner.getId())) != selected.end()) {
        auto area = turnOwner.getAllCards();
        if (area.size() == 1)
            discardable.erase(std::remove(discardable.begin(), discardable.end(), area.front()), discardable.end());
    }
    if (discardable.empty()) return;
    CardPtr cost = engine.askChooseCard(me, discardable,
        "【势-急袭】选择弃置当前回合角色 " + who(turnOwner) + " 的一张牌", false,
        AIController::chooseLeastValuableCard(discardable));
    if (!cost || (!turnOwner.hasHandCard(cost) && !turnOwner.hasEquipment(cost))) return;
    engine.discardCardOf(engine.getPlayerById(turnOwner.getId()), cost, "势-急袭", me);
    if (!self.isAlive() || engine.isGameOver()) return;
    if (!engine.useCard(me, shun, selected)) return;
    engine.logMessage("  【势-急袭】" + who(self) + " 对 " + std::to_string(selected.size()) +
                      " 名本回合曾成为其牌目标的角色使用无视距离的【顺手牵羊】。");
}

void ShiJiXiSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool starting) {
    if (starting) targetsThisTurn.clear();
}

// =====================================================================
//  孙綝（势）：逆固 / 戮连
// =====================================================================

ShiNiGuSkill::ShiNiGuSkill()
    : ActiveSkill("势-逆固",
                 "出牌阶段限一次，你可弃置任意张不同花色的牌，令攻击范围内的角色同时选择是否交给你一张牌，然后你本回合造成的下X次伤害+1（X为不交给你牌的角色数）。", 1) {}

bool ShiNiGuSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self) || !hasUsesLeft()) return false;
    std::set<int> suits;
    for (auto c : self.getHandAndEquipmentCards()) if (c) suits.insert((int)c->getSuit());
    return !suits.empty() && !engine.getOtherAlivePlayers(self).empty();
}

// 逆固（AI）：手牌 ≥2 且花色 ≥2 时发动（弃不同花色换“伤害+1”次数；牌太少不值）。
bool ShiNiGuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (self.getHandCardCount() < 2) return false;
    std::set<int> suits;
    for (auto c : self.getHandCards()) if (c) suits.insert((int)c->getSuit());
    return suits.size() >= 2;
}

void ShiNiGuSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    // 弃置任意张不同花色的牌（AI：每花色弃一张最没用的）
    std::set<int> used;
    std::vector<CardPtr> toDiscard;
    auto hand = self.getHandAndEquipmentCards();
    while (true) {
        CardPtr best = nullptr;
        for (auto c : hand) {
            if (!c || used.count((int)c->getSuit())) continue;
            if (!best || AIController::chooseLeastValuableCard({c, best}) == c) best = c;
        }
        if (!best) break;
        if (!engine.askConfirm(me, "【势-逆固】弃置 " + best->getFormattedName() + "（已弃 " +
                                std::to_string(toDiscard.size()) + " 张）？", true)) break;
        toDiscard.push_back(best);
        used.insert((int)best->getSuit());
        hand.erase(std::find(hand.begin(), hand.end(), best));
    }
    if (toDiscard.empty()) return;
    markUsed();
    for (auto c : toDiscard) engine.discardCardOf(me, c, "势-逆固", me);
    // 攻击范围内的角色同时选择是否交给你一张牌
    int refusers = 0;
    for (auto& t : engine.getOtherAlivePlayers(self)) {
        if (engine.calculateDistance(*t, self) > t->getAttackRange()) continue;
        if (t->getHandCardCount() == 0) continue;
        bool give = engine.askConfirm(t, "【势-逆固】是否交给 " + who(self) + " 一张牌？",
                                      AIController::isFriend(engine, *t, self));
        if (give) {
            auto c = AIController::chooseLeastValuableCard(t->getHandCards());
            if (c) {
                engine.obtainCard(me, c, t);
                engine.logMessage("  【势-逆固】" + who(*t) + " 交给" + who(self) + " 一张牌。");
            }
        } else {
            refusers++;
            engine.logMessage("  【势-逆固】" + who(*t) + " 拒绝交牌。");
        }
    }
    niGuCharges += refusers;
    self.addMark("逆固伤害+1", refusers);
    engine.logMessage("  【势-逆固】" + who(self) + " 本回合接下来的 " + std::to_string(refusers) +
                      " 次伤害+1。");
}

void ShiNiGuSkill::onDealDamage(GameEngine&, Player& self, Player&, int& damage, ShaElement, CardPtr) {
    if (self.getMark("逆固伤害+1") > 0) {
        self.addMark("逆固伤害+1", -1);
        damage += 1;
        niGuCharges = self.getMark("逆固伤害+1");
    }
}

void ShiNiGuSkill::onTurnBoundary(GameEngine&, Player& self, Player&, bool starting) {
    if (starting) {
        niGuCharges = 0;
        self.addMark("逆固伤害+1", -self.getMark("逆固伤害+1"));
    }
}

ShiLuLianSkill::ShiLuLianSkill()
    : StateSkill("势-戮连",
                 "锁定技，你使用手牌结算后，若你没有此类别的手牌，且有目标角色：体力值小于等于你，此牌所有目标进入连环状态；装备区牌数小于等于你，你摸一张牌。乘势：你对一名体力值不为最小的角色造成1点火焰伤害。", SkillTag::LOCK) {}

void ShiLuLianSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getType() == CardType::EQUIPMENT) return;
    if (!card->getSkillSource().empty()) return; // 仅手牌（非转化/非技能牌）
    // 使用后你没有此类别的手牌
    bool hasSame = false;
    for (auto c : self.getHandCards())
        if (c && c->getType() == card->getType()) { hasSame = true; break; }
    if (hasSame) return;
    auto targets = engine.getCardTargets(card);
    if (targets.empty()) return;
    PlayerPtr me = selfOf(engine, self);
    bool anyLink = false, anyEquip = false;
    for (auto& t : targets) {
        if (!t || !t->isAlive()) continue;
        if (t->getHp() <= self.getHp()) anyLink = true;
        if ((int)t->getAllEquipment().size() <= (int)self.getAllEquipment().size()) anyEquip = true;
    }
    if (anyLink) {
        for (auto& t : targets) {
            if (t && t->isAlive() && !t->isChained()) t->setChained(true);
        }
        engine.logMessage("  【势-戮连】" + who(self) + " 的手牌类别已用尽，目标角色进入连环状态。");
    }
    if (anyEquip) {
        engine.drawCards(selfOf(engine, self), 1, "势-戮连");
        engine.logMessage("  【势-戮连】" + who(self) + " 摸一张牌。");
    }
    // 乘势：你对一名体力值不为最小的角色造成1点火焰伤害。
    // 官网未写明由谁选择目标；用户 2026-10-04 裁定：由技能持有者选择（“一名角色”含自己）。
    int minHp = 1000;
    for (auto& p : engine.getAlivePlayers()) minHp = std::min(minHp, p->getHp());
    std::vector<PlayerPtr> chaiShiCands;
    for (auto& p : engine.getAlivePlayers())
        if (p->getHp() > minHp) chaiShiCands.push_back(p);
    if (!chaiShiCands.empty()) {
        PlayerPtr ai = nullptr;
        for (auto& p : chaiShiCands) {
            if (p->getId() == self.getId() || AIController::isFriend(engine, self, *p)) continue;
            ai = p;
            break;
        }
        if (!ai) for (auto& p : chaiShiCands) if (p->getId() != self.getId()) { ai = p; break; }
        if (!ai) ai = chaiShiCands.front();
        PlayerPtr victim = engine.askChoosePlayer(me, chaiShiCands,
            "【势-戮连·乘势】选择一名体力值不为最小的角色，对其造成1点火焰伤害", false, ai);
        if (victim && victim->isAlive()) {
            engine.logMessage("  【势-戮连·乘势】" + who(self) + " 对" + who(*victim) + " 造成1点火焰伤害。");
            engine.applyDamage(selfOf(engine, self), victim, 1, ShaElement::FIRE, false, card, nullptr);
        }
    }
    (void)anyLink; (void)anyEquip;
}

// =====================================================================
//  周瑜（势）：炽沄 / 焰洄 / 焚涛 / 雄姿
// =====================================================================

ShiChiYunSkill::ShiChiYunSkill()
    : ActiveSkill("势-炽沄",
                 "你每阶段首次获得牌后，可交给一名其他角色任意张手牌，其选择一项：1.展示所有与这些牌颜色相同的手牌，你对其造成1点火焰伤害；2.你摸两张牌，其进入连环状态。") {}

bool ShiChiYunSkill::canActivate(GameEngine& engine, Player& self) {
    return pending && self.isAlive() && self.getHandCardCount() > 0 &&
           !engine.getOtherAlivePlayers(self).empty();
}

// 炽沄（AI）：阶段首次获得牌后，若有可交给友方的低价值牌则发动。
bool ShiChiYunSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    bool hasFriend = false;
    for (auto t : engine.getOtherAlivePlayers(self)) if (AIController::isFriend(engine, self, *t)) hasFriend = true;
    return hasFriend || self.getHandCardCount() >= 2;
}

void ShiChiYunSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    pending = false;
    if (self.getMark("雄姿限一") > 0 && engine.getCurrentPlayer() &&
        engine.getCurrentPlayer()->getId() != self.getId()) return;
    auto cyCands = engine.getOtherAlivePlayers(self);
    PlayerPtr t = engine.askChoosePlayer(me, cyCands, "【势-炽沄】选择一名其他角色", false,
                                         cyCands.empty() ? nullptr : cyCands.front());
    if (!t) return;
    int n = static_cast<int>(self.getHandCardCount());
    if (n <= 0) return;
    std::vector<std::string> giveOptions;
    for (int count = 1; count <= n; ++count)
        giveOptions.push_back("交给" + std::to_string(count) + "张手牌");
    int give = engine.askChooseOption(me, giveOptions,
                                     "【势-炽沄】交给 " + who(*t) + " 任意张手牌",
                                     std::min(n - 1, 1));
    int gn = give + 1;
    std::vector<CardPtr> given;
    auto hand = self.getHandCards();
    for (int i = 0; i < gn && !hand.empty(); i++) {
        CardPtr c = engine.askChooseCard(me, hand, "【势-炽沄】选择第 " + std::to_string(i + 1) +
                                         "/" + std::to_string(gn) + " 张交出的手牌", false,
                                         AIController::chooseLeastValuableCard(hand));
        if (!c) break;
        given.push_back(c);
        hand.erase(std::remove(hand.begin(), hand.end(), c), hand.end());
    }
    for (auto c : given) engine.obtainCard(t, c, me);
    if (given.empty()) return;
    // 其选择一项（雄姿：仅保留一选项或二选项）
    int opt;
    if (t->getMark("雄姿选一") > 0 || self.getMark("雄姿选一") > 0) opt = 0;
    else if (t->getMark("雄姿选二") > 0 || self.getMark("雄姿选二") > 0) opt = 1;
    else opt = engine.askChooseOption(t,
        {"展示所有与这些牌颜色相同的手牌，然后受到1点火焰伤害", "对方摸两张牌，你进入连环状态"},
        "【势-炽沄】选择一项", t->getHandCardCount() >= 4 ? 0 : 1);
    if (opt == 0) {
        bool red = given.front()->getSuit() == Suit::HEART || given.front()->getSuit() == Suit::DIAMOND;
        for (auto c : t->getHandCards())
            if (c && ((c->getSuit() == Suit::HEART || c->getSuit() == Suit::DIAMOND) == red))
                engine.logMessage("  【势-炽沄】展示 " + c->getFormattedName());
        engine.logMessage("  【势-炽沄】" + who(*t) + " 展示了同色手牌。");
        engine.applyDamage(me, t, 1, ShaElement::FIRE, false, nullptr, nullptr);
    } else {
        engine.drawCards(me, 2, "势-炽沄");
        if (!t->isChained()) t->setChained(true);
        engine.logMessage("  【势-炽沄】" + who(self) + " 摸两张牌，" + who(*t) + " 进入连环状态。");
    }
}

void ShiChiYunSkill::onCardsObtained(GameEngine& engine, Player& self, int count) {
    if (count <= 0) return;
    if (engine.getCurrentPhase() == TurnPhase::NONE) return;
    if (stageSerial != engine.getCurrentRound() * 10 + (int)engine.getCurrentPhase()) {
        stageSerial = engine.getCurrentRound() * 10 + (int)engine.getCurrentPhase();
        gotThisStage = false;
    }
    if (gotThisStage) return;
    if (self.getMark("雄姿限一") > 0 && engine.getCurrentPlayer() &&
        engine.getCurrentPlayer()->getId() != self.getId()) return;
    if (self.getHandCardCount() == 0 || engine.getOtherAlivePlayers(self).empty()) return;
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【势-炽沄】本阶段首次获得牌，是否发动（交给其他角色手牌）？", true)) return;
    gotThisStage = true;
    pending = true;
    if (canActivate(engine, self)) activate(engine, self);
    else pending = false;
}

void ShiChiYunSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool starting) {
    if (starting) gotThisStage = false;
}

ShiYanHuiSkill::ShiYanHuiSkill()
    : TriggerSkill("势-焰洄",
                 "你使用牌指定目标后，可展示一名目标角色的一张手牌，若此牌本回合已被展示过，你弃置之。此阶段结束时，你选择一项：1.对一名本阶段因此因弃置而失去过牌的角色造成1点火焰伤害；2.摸X张牌（X为本回合展示过牌的角色数）。（官网原文如此）") {}

void ShiYanHuiSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (!card) return;
    auto targets = engine.getCardTargets(card);
    if (targets.empty()) return;
    // 仅限自己使用的牌（self 为使用者）
    if (self.getMark("雄姿限一") > 0 && engine.getCurrentPlayer() &&
        engine.getCurrentPlayer()->getId() != self.getId()) return;
    // 展示一名目标角色的一张手牌
    if (!engine.askConfirm(selfOf(engine, self), "【势-焰洄】展示一名目标角色的一张手牌？", true)) return;
    PlayerPtr t = targets.front();
    if (!t || t->getHandCardCount() == 0) return;
    auto shown = engine.askChooseCard(selfOf(engine, self), t->getHandCards(),
                                      "【势-焰洄】选择要展示的手牌", false,
                                      AIController::chooseLeastValuableCard(t->getHandCards()));
    if (!shown) return;
    engine.logMessage("  【势-焰洄】展示 " + who(*t) + " 的 " + shown->getFormattedName());
    shownPlayers.insert(t->getId());
    if (shownCardThisTurn.count(shown->getId())) {
        engine.discardCardOf(t, shown, "势-焰洄", selfOf(engine, self));
        lostPlayers.insert(t->getId());
        engine.logMessage("  【势-焰洄】此牌本回合已被展示过，弃置之。");
    } else {
        shownCardThisTurn.insert(shown->getId());
    }
}

void ShiYanHuiSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY) return;
    PlayerPtr me = selfOf(engine, self);
    int opt;
    if (self.getMark("雄姿选一") > 0) opt = 0;
    else if (self.getMark("雄姿选二") > 0) opt = 1;
    else opt = engine.askChooseOption(me,
        {"对一名本阶段因此因弃置而失去过牌的角色造成1点火焰伤害",
         "摸" + std::to_string((int)shownPlayers.size()) + "张牌"},
        "【势-焰洄】选择一项", (int)shownPlayers.size() > 0 ? 1 : 0);
    if (opt == 0 && !lostPlayers.empty()) {
        PlayerPtr t = lostPlayers.empty() ? nullptr : engine.getPlayerById(*lostPlayers.begin());
        if (t && t->isAlive()) {
            engine.applyDamage(me, t, 1, ShaElement::FIRE, false, nullptr, nullptr);
            engine.logMessage("  【势-焰洄】" + who(self) + " 对" + who(*t) + " 造成1点火焰伤害。");
        }
    } else {
        engine.drawCards(me, (int)shownPlayers.size(), "势-焰洄");
        engine.logMessage("  【势-焰洄】" + who(self) + " 摸 " +
                          std::to_string((int)shownPlayers.size()) + " 张牌。");
    }
    shownPlayers.clear();
    lostPlayers.clear();
    shownCardThisTurn.clear();
}

ShiFenTaoSkill::ShiFenTaoSkill()
    : StateSkill("势-焚涛",
                 "锁定技，有连环状态的其他角色受到火焰伤害时，其选择一项：1.此次传导中的伤害+1；2.弃置一半牌（向上取整），此伤害结算后其进入连环状态。", SkillTag::LOCK) {}

void ShiFenTaoSkill::onDamageTakingByAny(GameEngine& engine, Player& self, Player*, Player& target,
                                         int&, ShaElement element, CardPtr) {
    if (&target == &self) return;                 // 有连环状态的**其他**角色
    if (!target.isChained()) return;
    if (element != ShaElement::FIRE) return;
    if (self.getMark("雄姿限一") > 0 && engine.getCurrentPlayer() &&
        engine.getCurrentPlayer()->getId() != self.getId()) return;
    PlayerPtr tp = engine.getPlayerById(target.getId());
    int opt = engine.askChooseOption(tp, {"此次传导中的伤害+1", "弃置一半牌（向上取整），此伤害结算后进入连环"},
                                     "【势-焚涛】选择一项", 0);
    if (opt == 0) {
        target.addMark("焚涛传导+1", 1);
        engine.logMessage("  【势-焚涛】" + who(target) + " 选择此次传导中的伤害+1。");
    } else {
        const int half = (static_cast<int>(tp->getHandAndEquipmentCards().size()) + 1) / 2;
        int n = 0;
        while (n < half) {
            auto discardable = tp->getHandAndEquipmentCards();
            if (discardable.empty()) break;
            auto c = AIController::chooseLeastValuableCard(discardable);
            if (!c) break;
            engine.discardCardOf(tp, c, "势-焚涛", tp);
            n++;
        }
        target.addMark("焚涛重连", 1);
        engine.logMessage("  【势-焚涛】" + who(target) + " 弃置 " + std::to_string(n) +
                          " 张牌，此伤害结算后其进入连环状态。");
    }
}

void ShiFenTaoSkill::onTakeDamage(GameEngine&, Player&, Player*, int&, ShaElement) {
    // 选择在 onDamageTakingByAny 完成；此钩子保留占位。
}

ShiXiongZiSkill::ShiXiongZiSkill()
    : StateSkill("势-雄姿",
                 "限定技，准备阶段，你可令本局游戏的“炽沄”，“焰洄”和“焚涛”只能在你的回合内发动，然后仅保留其中全部的一选项或二选项，并摸两张牌。", SkillTag::LIMITED) {}

void ShiXiongZiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) {
    skipPhase = false;
    if (phase != TurnPhase::PREPARATION) return;
    if (spent) return;
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【势-雄姿】发动（限定技）？", true)) return;
    spent = true;
    int opt = engine.askChooseOption(me, {"仅保留一选项", "仅保留二选项"},
                                     "【势-雄姿】选择保留的选项", 0);
    self.addMark("雄姿限一", 1);
    self.addMark(opt == 0 ? "雄姿选一" : "雄姿选二", 1);
    engine.drawCards(me, 2, "势-雄姿");
    engine.logMessage("  【势-雄姿】" + who(self) + " 发动，三技能仅在你的回合发动，仅保留" +
                      std::string(opt == 0 ? "一" : "二") + "选项，并摸两张牌。");
}

// =====================================================================
//  田丰（势）：刚鲠 / 死谏（官网 hero-detail-623，见 docs/shi_you_appendix.md）
// =====================================================================

ShiGangGengSkill::ShiGangGengSkill()
    : ActiveSkill("势-刚鲠",
                  "出牌阶段限一次，你可以将至少两张手牌交给一名其他角色。回合结束时，若其手牌数：为全场最多，你摸一张牌；不为全场最多，你弃置其区域里的一张牌。",
                  1) {}

bool ShiGangGengSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getHandCardCount() < 2) return false;
    return !engine.getOtherAlivePlayers(self).empty();
}

void ShiGangGengSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    auto others = engine.getOtherAlivePlayers(self);
    std::vector<PlayerPtr> rich = others;
    std::sort(rich.begin(), rich.end(), [](const PlayerPtr& a, const PlayerPtr& b) {
        return a->getHandCardCount() < b->getHandCardCount();
    });
    PlayerPtr target = engine.askChoosePlayer(me, others, "【刚鲠】选择一名其他角色（交给其至少两张手牌）",
                                              false, rich.back());
    if (!target) return;
    std::vector<CardPtr> chosen;
    while (true) {
        std::vector<CardPtr> pool;
        for (const auto& c : self.getHandCards())
            if (std::find(chosen.begin(), chosen.end(), c) == chosen.end()) pool.push_back(c);
        if (pool.empty()) break;
        CardPtr c = engine.askChooseCard(me, pool,
                                         "【刚鲠】选择交给 " + who(*target) + " 的手牌（至少两张；0=结束）",
                                         static_cast<int>(chosen.size()) >= 2,
                                         AIController::chooseLeastValuableCard(pool));
        if (!c) break;
        chosen.push_back(c);
        if (chosen.size() >= self.getHandCardCount()) break;
    }
    if (chosen.size() < 2) return; // 官网“至少两张”：不足则不发动（不空发）
    for (const auto& c : chosen) engine.obtainCard(target, c, me);
    targetId = target->getId();
    markUsed();
    engine.logMessage("  【势-刚鲠】" + who(self) + " 交给 " + who(*target) + " " +
                      std::to_string(chosen.size()) + " 张牌。");
}

bool ShiGangGengSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    // 手牌溢出或队友需要牌时给牌：给手牌最少的队友。
    if (self.getHandCardCount() < 4 && !self.isWounded()) return false;
    for (const auto& p : engine.getOtherAlivePlayers(self))
        if (AIController::isFriend(engine, self, *p)) return true;
    return false;
}

void ShiGangGengSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH || !engine.isPlayerTurn(self) || targetId < 0) return;
    PlayerPtr target = engine.getPlayerById(targetId);
    targetId = -1;
    if (!target || !target->isAlive()) return;
    bool most = true;
    for (const auto& p : engine.getAlivePlayers())
        if (p->getId() != target->getId() && p->getHandCardCount() > target->getHandCardCount()) most = false;
    PlayerPtr me = selfOf(engine, self);
    if (most) {
        engine.drawCards(me, 1, "势-刚鲠");
        engine.logMessage("  【势-刚鲠】" + who(*target) + " 手牌数为全场最多，" + who(self) + " 摸一张牌。");
        return;
    }
    auto cards = target->getAllCards();
    if (cards.empty()) return;
    CardPtr c = engine.askChooseCard(me, cards, "【刚鲠】弃置 " + who(*target) + " 区域里的一张牌", false,
                                     AIController::chooseLeastValuableCard(cards));
    if (!c) return;
    engine.discardCardOf(target, c, "刚鲠", me);
    engine.logMessage("  【势-刚鲠】" + who(*target) + " 手牌数不为全场最多，" + who(self) + " 弃置其一张牌。");
}

ShiSiJianSkill::ShiSiJianSkill()
    : TriggerSkill("势-死谏",
                   "每回合限两次，当你失去最后一张手牌后，或当你进入濒死状态时，你可以选择一项：1.选择一名其他角色，其使用下一张牌后需弃置一张牌。2.令当前回合角色摸两张牌。若此时没有角色处于濒死状态，你可以背水：失去X点体力（X为此技能发动过背水的次数）。") {}

void ShiSiJianSkill::onHandEmpty(GameEngine& engine, Player& self) {
    tryActivate(engine, self, true);
}

void ShiSiJianSkill::onDying(GameEngine& engine, Player& self, Player& dyingPlayer) {
    if (dyingPlayer.getId() != self.getId()) return;
    tryActivate(engine, self, false); // 自己正处于濒死：官网“若此时没有角色处于濒死状态”不成立
}

void ShiSiJianSkill::tryActivate(GameEngine& engine, Player& self, bool allowBeishui) {
    if (usedThisTurn >= 2 || !self.isAlive()) return;
    for (const auto& p : engine.getAlivePlayers()) if (p->getHp() <= 0) allowBeishui = false;
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【死谏】是否发动？", true)) return;
    std::vector<std::string> opts = {"选择一名其他角色，其使用下一张牌后需弃置一张牌",
                                     "令当前回合角色摸两张牌"};
    if (allowBeishui)
        opts.push_back("背水：失去" + std::to_string(beishuiCount) + "点体力，同时执行两项");
    int opt = engine.askChooseOption(me, opts, "【死谏】选择一项", self.getHandCardCount() == 0 ? 1 : 0);
    ++usedThisTurn;
    auto option1 = [&]() {
        auto others = engine.getOtherAlivePlayers(self);
        if (others.empty()) return;
        std::vector<PlayerPtr> enemies;
        for (const auto& p : others) if (!AIController::isFriend(engine, self, *p)) enemies.push_back(p);
        PlayerPtr t = engine.askChoosePlayer(me, others, "【死谏】选择一名其他角色（其下一张牌后弃一张牌）",
                                             false, enemies.empty() ? others.front() : enemies.front());
        if (!t) return;
        t->addMark("死谏标记", 1);
        engine.logMessage("  【势-死谏】" + who(*t) + " 使用下一张牌后需弃置一张牌。");
    };
    auto option2 = [&]() {
        PlayerPtr cur = engine.getCurrentPlayer();
        if (!cur) cur = me;
        engine.drawCards(cur, 2, "势-死谏");
        engine.logMessage("  【势-死谏】" + who(*cur) + " 摸两张牌。");
    };
    if (opt == 0) option1();
    else if (opt == 1) option2();
    else {
        engine.loseHp(me, beishuiCount, "势-死谏（背水）");
        ++beishuiCount;
        option1();
        option2();
        engine.logMessage("  【势-死谏】背水发动（第 " + std::to_string(beishuiCount) + " 次）。");
    }
}

void ShiSiJianSkill::onAnyCardUsed(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    (void)self;
    (void)card;
    if (user.getMark("死谏标记") <= 0) return;
    user.addMark("死谏标记", -user.getMark("死谏标记"));
    auto cards = user.getHandCards();
    if (cards.empty()) return;
    PlayerPtr up = engine.getPlayerById(user.getId());
    CardPtr c = engine.askChooseCard(up, cards, "【死谏】弃置一张牌", true,
                                     AIController::chooseLeastValuableCard(cards));
    if (!c) c = cards.front(); // “需弃置”：不可拒绝
    engine.discardCardOf(up, c, "死谏", selfOf(engine, self));
    engine.logMessage("  【势-死谏】" + who(user) + " 弃置一张牌。");
}

// =====================================================================
//  黄祖（势）：鸱张 / 断鞅（官网 hero-detail-620）
// =====================================================================

ShiChiZhangSkill::ShiChiZhangSkill()
    : TriggerSkill("势-鸱张",
                   "你使用伤害类卡牌无距离限制。当你使用手牌中除【闪电】外的伤害类卡牌指定目标后，你可以弃置任意数量的手牌，令其他角色不能使用或打出与你此法弃置牌颜色相同的牌响应此牌。") {}

void ShiChiZhangSkill::onCheckShaTarget(GameEngine&, const Player& self, const Player& target,
                                        CardPtr sha, bool& canTarget) {
    if (!sha || sha->getType() != CardType::BASIC || sha->getSubType() != CardSubType::SHA) return;
    if (self.getId() == target.getId()) return;
    canTarget = true; // 伤害类卡牌（此引擎中仅【杀】受距离限制）无距离限制
}

void ShiChiZhangSkill::askBlock(GameEngine& engine, Player& self, CardPtr card) {
    PlayerPtr me = selfOf(engine, self);
    if (self.getHandCardCount() <= 0) return;
    if (!engine.askConfirm(me, "【鸱张】弃置手牌令其他角色无法以同色牌响应此牌？", true)) return;
    std::vector<CardPtr> chosen;
    while (true) {
        std::vector<CardPtr> pool;
        for (const auto& c : self.getHandCards())
            if (std::find(chosen.begin(), chosen.end(), c) == chosen.end()) pool.push_back(c);
        if (pool.empty()) break;
        CardPtr c = engine.askChooseCard(me, pool, "【鸱张】选择弃置的手牌（0=结束）", true,
                                         AIController::chooseLeastValuableCard(pool));
        if (!c) break;
        chosen.push_back(c);
        if (!engine.askConfirm(me, "【鸱张】继续弃置手牌？", false)) break;
    }
    if (chosen.empty()) return;
    blockRed = blockBlack = false;
    for (const auto& c : chosen) {
        Suit s = engine.effectiveSuit(self, c);
        if (isRedSuit(s)) blockRed = true;
        else if (s == Suit::SPADE || s == Suit::CLUB) blockBlack = true;
    }
    for (const auto& c : chosen) engine.discardCardOf(me, c, "鸱张", me);
    blocking = true;
    engine.logMessage("  【势-鸱张】" + who(self) + " 弃置 " + std::to_string(chosen.size()) +
                      " 张牌，其他角色不能以" +
                      std::string(blockRed && blockBlack ? "红黑两色" : (blockRed ? "红色" : "黑色")) +
                      "牌响应【" + card->getName() + "】。");
}

void ShiChiZhangSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.card || ctx.source.get() != &self) return;
    // 官网为“手牌中…的伤害类卡牌”：此钩子在牌已被消耗后触发（手牌里已无此牌），
    // 故按“非装备区来源”处理——装备区的牌不会是【杀】/伤害类锦囊。
    askBlock(engine, self, ctx.card);
}

void ShiChiZhangSkill::onCardTargetConfirmed(GameEngine& engine, Player& self, Player* source,
                                             CardPtr card, const std::vector<PlayerPtr>&) {
    if (!source || source->getId() != self.getId() || !card) return;
    if (card->getSubType() == CardSubType::SHA) return; // 【杀】走 onShaTargeted
    if (!isDamageCard(card)) return;
    askBlock(engine, self, card);
}

void ShiChiZhangSkill::onCheckResponseCard(const GameEngine& engine, const Player& self,
                                           const Player& responder, CardPtr card, CardSubType,
                                           bool& allowed) const {
    if (!blocking || !card) return;
    if (responder.getId() == self.getId()) return; // 不影响自己
    Suit s = engine.effectiveSuit(responder, card);
    if ((blockRed && isRedSuit(s)) || (blockBlack && (s == Suit::SPADE || s == Suit::CLUB))) allowed = false;
}

void ShiChiZhangSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (!blocking || !card) return;
    (void)engine;
    blocking = false;
    blockRed = blockBlack = false;
    (void)self;
}

ShiDuanYangSkill::ShiDuanYangSkill()
    : TriggerSkill("势-断鞅",
                   "每回合限一次，当你的手牌不因使用而进入弃牌堆时，你可以将其中随机一张【杀】置于武将牌上，并于本阶段结束时使用之（无次数限制）。你以此法使用的【杀】造成伤害后，你可以重铸受伤角色区域里的至多两张牌，然后你摸四张牌。") {}

void ShiDuanYangSkill::onAnyCardDiscarded(GameEngine& engine, Player& self, Player& owner, CardPtr card) {
    if (owner.getId() != self.getId() || usedThisTurn > 0) return;
    if (!card || card->getSubType() != CardSubType::SHA || card->getType() != CardType::BASIC) return;
    if (!self.isAlive()) return;
    // 该牌已进入弃牌堆（discardCardOf 的广播）；可收取一张【杀】。
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【断鞅】将此【杀】置于武将牌上，本阶段结束时使用？", true)) return;
    engine.getDeck().removeDiscardCard(card);
    self.addToPile("断鞅", card);
    pilePhase = static_cast<int>(engine.getCurrentPhase());
    ++usedThisTurn;
    engine.logMessage("  【势-断鞅】" + who(self) + " 将【杀】置于武将牌上（本阶段结束时使用）。");
}

bool ShiDuanYangSkill::canUseShaBeyondLimitOn(GameEngine&, const Player&, const Player&, CardPtr sha) {
    return sha && sha->getSkillSource() == "势-断鞅";
}

void ShiDuanYangSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (pilePhase < 0 || static_cast<int>(phase) != pilePhase) return;
    if (!engine.isPlayerTurn(self)) return;
    pilePhase = -1;
    auto pile = self.getPile("断鞅");
    if (pile.empty()) return;
    PlayerPtr me = selfOf(engine, self);
    for (const auto& card : pile) {
        self.removeFromPile("断鞅", card);
        auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {card}, "势-断鞅");
        auto targets = engine.getShaTargets(self, sha);
        if (targets.empty()) {
            engine.getDeck().discardCard(card);
            continue;
        }
        std::vector<PlayerPtr> enemies;
        for (const auto& p : targets)
            if (!AIController::isFriend(engine, self, *p)) enemies.push_back(p);
        PlayerPtr t = engine.askChoosePlayer(me, targets, "【断鞅】选择此【杀】的目标", true,
                                             enemies.empty() ? targets.front() : enemies.front());
        if (!t) {
            engine.getDeck().discardCard(card);
            continue;
        }
        engine.useCard(me, sha, {t}); // 以此法使用无次数限制（见 canUseShaBeyondLimitOn）
    }
}

void ShiDuanYangSkill::onAfterDealDamage(GameEngine& engine, Player& self, Player* target,
                                         int damage, ShaElement, CardPtr cause) {
    if (!target || damage <= 0) return;
    if (!cause || cause->getSkillSource() != "势-断鞅") return;
    if (cause->getSubType() != CardSubType::SHA) return;
    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【断鞅】重铸 " + who(*target) + " 区域里的至多两张牌，然后摸四张牌？", true))
        return;
    PlayerPtr victim = engine.getPlayerById(target->getId());
    for (int i = 0; i < 2; ++i) {
        auto cards = victim->getAllCards();
        if (cards.empty()) break;
        CardPtr c = engine.askChooseCard(me, cards, "【断鞅】重铸其区域里的一张牌（0=结束）", true,
                                         AIController::chooseLeastValuableCard(cards));
        if (!c) break;
        engine.discardCardOf(victim, c, "断鞅（重铸）", me);
        if (victim->isAlive()) engine.drawCards(victim, 1, "断鞅（重铸）"); // 重铸：弃置该牌然后其摸一张牌
    }
    engine.drawCards(me, 4, "势-断鞅");
    engine.logMessage("  【势-断鞅】" + who(self) + " 重铸后摸四张牌。");
}

} // namespace Thks
