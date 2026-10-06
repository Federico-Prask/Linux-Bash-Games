#include "Skills.h"
#include "GameEngine.h"
#include "Player.h"
#include "AI.h"
#include <algorithm>

namespace Thks {

const char* const PILE_QUAN = "权";

namespace {

PlayerPtr selfPtr(GameEngine& engine, const Player& self) {
    return engine.getPlayerById(self.getId());
}

std::string who(const Player& p) {
    return "[" + p.getName() + "]";
}

// 设置标记到指定值（Player 仅有增减接口）。
void setMark(Player& p, const std::string& key, int value) {
    int cur = p.getMark(key);
    if (cur != value) p.addMark(key, value - cur);
}

// 官方普通/界钟会的【权计】；不依赖任何 DIY 武将状态。
void doOfficialQuanJiEffect(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self);
    engine.logMessage("  " + who(self) + " 发动了【权计】！");
    engine.drawCards(me, 1, "权计");
    if (self.getHandCardCount() == 0) return;
    CardPtr chosen = engine.askChooseCard(me, self.getHandCards(), "【权计】请选择一张手牌置于武将牌上作为\"权\"", false,
                                          AIController::chooseLeastValuableCard(self.getHandCards()));
    if (!chosen) chosen = self.getHandCards().front();
    engine.loseHandCard(me, chosen);
    self.addToPile(PILE_QUAN, chosen);
    engine.logMessage("  " + who(self) + " 将一张手牌置于武将牌上作为\"权\"（当前\"权\"数：" +
                      std::to_string(self.getPileCount(PILE_QUAN)) + "）");
}

// DIY 钟会的独立【权计】：除置“权”外还消耗1点蓄力并摸牌。
void doDiyQuanJiEffect(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self);
    engine.logMessage("  " + who(self) + " 发动了【权计】！");
    engine.drawCards(me, 1, "权计");
    if (self.getHandCardCount() == 0) return;
    CardPtr chosen = engine.askChooseCard(me, self.getHandCards(), "【权计】请选择一张手牌置于武将牌上作为\"权\"", false,
                                          AIController::chooseLeastValuableCard(self.getHandCards()));
    if (!chosen) chosen = self.getHandCards().front();
    engine.loseHandCard(me, chosen);
    self.addToPile(PILE_QUAN, chosen);
    engine.logMessage("  " + who(self) + " 将一张手牌置于武将牌上作为\"权\"（当前\"权\"数：" +
                      std::to_string(self.getPileCount(PILE_QUAN)) + "）");
    if (self.getMark("蓄力") > 0) {
        engine.consumeCharge(me, 1);
        engine.drawCards(me, 1, "权谋消耗蓄力");
        engine.logMessage("  【权谋】消耗1点蓄力并摸一张牌（剩余 " +
                          std::to_string(self.getMark("蓄力")) + " 点）。");
    }
}

// 官方普通/界钟会的【自立】（官网两版规则相同）
void ziLiAwaken(GameEngine& engine, Player& self, bool& awakened) {
    if (awakened || self.getPileCount(PILE_QUAN) < 3) return;
    awakened = true;
    PlayerPtr me = selfPtr(engine, self);
    engine.logMessage("  ✨ " + who(self) + " 的觉醒技【自立】发动！");

    int choice;
    if (!self.isWounded()) {
        choice = 1; // 未受伤时不能选择回复体力（官方FAQ）
        engine.logMessage("  " + who(self) + " 未受伤，只能选择摸两张牌。");
    } else {
        choice = engine.askChooseOption(me, {"回复1点体力", "摸两张牌"}, "【自立】请选择一项", 0);
    }
    if (choice == 0) engine.recoverHp(me, 1, "自立");
    else engine.drawCards(me, 2, "自立");

    self.changeMaxHp(-1);
    engine.logMessage("  " + who(self) + " 减1点体力上限（当前 " + std::to_string(self.getHp()) + "/" +
                      std::to_string(self.getMaxHp()) + "），获得技能【排异】！");
    if (self.getHero()) {
        bool jie = self.getHero()->findSkill("界-自立") != nullptr;
        self.getHero()->addSkill(std::make_shared<PaiYiSkill>(jie));
    }
}

// DIY 钟会使用独立的觉醒流程和觉醒后技能，不共用官方武将技能对象。
void diyZiLiAwaken(GameEngine& engine, Player& self, bool& awakened) {
    if (awakened || self.getPileCount(PILE_QUAN) < 3) return;
    awakened = true;
    PlayerPtr me = selfPtr(engine, self);
    engine.logMessage("  ✨ " + who(self) + " 的 DIY 觉醒技【自立】发动！");
    int choice = self.isWounded()
        ? engine.askChooseOption(me, {"回复1点体力", "摸两张牌"}, "【自立】请选择一项", 0)
        : 1;
    if (choice == 0) engine.recoverHp(me, 1, "自立");
    else engine.drawCards(me, 2, "自立");
    self.changeMaxHp(-1);
    engine.logMessage("  " + who(self) + " 减1点体力上限，获得 DIY 技能【排异】！");
    if (self.getHero()) self.getHero()->addSkill(std::make_shared<DiyPaiYiSkill>());
}

} // namespace

// =====================================================================
//                              主动技
// =====================================================================

// ------------------------------ 武圣 ------------------------------
WuShengSkill::WuShengSkill(bool jieVersion)
    : ActiveSkill(jieVersion ? "界-武圣" : "武圣",
                  jieVersion ? "你可以将一张红色牌当【杀】使用或打出。你使用方块【杀】无距离限制。"
                             : "你可以将一张红色牌当【杀】使用或打出。",
                  0),
      jie(jieVersion) {}

CardPtr WuShengSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (wanted != CardSubType::SHA || !card || card->isVirtual()) return nullptr;
    if (engine.effectiveSuit(self,card)!=Suit::HEART && engine.effectiveSuit(self,card)!=Suit::DIAMOND) return nullptr;
    if (card->getSubType() == CardSubType::SHA) return nullptr; // 本来就是【杀】，无需转化
    return Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {card}, name);
}

void WuShengSkill::onCheckShaTarget(GameEngine& engine, const Player& self, const Player& /*target*/, CardPtr sha, bool& canTarget) {
    if (jie && sha && engine.effectiveSuit(self, sha) == Suit::DIAMOND) {
        canTarget = true;
    }
}

// ------------------------------ 龙胆 ------------------------------
LongDanSkill::LongDanSkill()
    : ActiveSkill("龙胆", "你可以将一张【杀】当【闪】、【闪】当【杀】使用或打出。", 0) {}

CardPtr LongDanSkill::convertCard(GameEngine& /*engine*/, Player& /*self*/, CardPtr card, CardSubType wanted) {
    if (!card || card->isVirtual()) return nullptr;
    if (wanted == CardSubType::SHA && card->getSubType() == CardSubType::SHAN) {
        return Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {card}, name);
    }
    if (wanted == CardSubType::SHAN && card->getSubType() == CardSubType::SHA) {
        return Card::makeVirtual("闪", CardType::BASIC, CardSubType::SHAN, {card}, name);
    }
    return nullptr;
}

// ------------------------------ 义绝 ------------------------------
YiJueSkill::YiJueSkill()
    : ActiveSkill("界-义绝",
                  "出牌阶段限一次，你可以弃置一张牌，然后令一名其他角色展示一张手牌。"
                  "若此牌为黑色，则其本回合非锁定技失效且不能使用或打出手牌，你对其使用的红桃【杀】伤害+1；"
                  "若此牌为红色，则你获得之，然后你可令该角色回复1点体力。",
                  1) {}

bool YiJueSkill::canActivate(GameEngine& engine, Player& self) {
    if (!hasUsesLeft()) return false;
    if (self.getAllCards().empty() || (self.getHandCardCount() == 0 && self.getAllEquipment().empty())) return false;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (p->getHandCardCount() > 0) return true;
    }
    return false;
}

bool YiJueSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (self.getHandCardCount() < 2) return false;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (!AIController::isFriend(engine, self, *p) && p->getHandCardCount() > 0) return true;
    }
    return false;
}

void YiJueSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    if (!hasUsesLeft()) return;
    PlayerPtr me = selfPtr(engine, self);

    // 1. 弃置一张牌（手牌或装备）
    std::vector<CardPtr> mine = self.getHandCards();
    auto eq = self.getAllEquipment();
    mine.insert(mine.end(), eq.begin(), eq.end());
    CardPtr cost = engine.askChooseCard(me, mine, "【义绝】请选择要弃置的一张牌", true,
                                        AIController::chooseLeastValuableCard(mine));
    if (!cost) return;

    // 2. 选择一名有手牌的其他角色
    std::vector<PlayerPtr> candidates;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (p->getHandCardCount() > 0) candidates.push_back(p);
    }
    PlayerPtr aiTarget = nullptr;
    for (auto& p : candidates) {
        if (!AIController::isFriend(engine, self, *p)) { aiTarget = p; break; }
    }
    PlayerPtr target = engine.askChoosePlayer(me, candidates, "【义绝】请选择一名其他角色", true, aiTarget);
    if (!target) return;

    markUsed();
    engine.discardCardOf(me, cost, "义绝");
    engine.logMessage("  " + who(self) + " 对 " + who(*target) + " 发动了【义绝】！");

    // 3. 目标展示一张手牌（AI：有红色牌则展示红色，避免被封锁）
    CardPtr aiShow = nullptr;
    for (auto& c : target->getHandCards()) {
        if (engine.effectiveSuit(*target,c)==Suit::HEART || engine.effectiveSuit(*target,c)==Suit::DIAMOND) { aiShow = c; break; }
    }
    if (!aiShow) aiShow = target->getHandCards().front();
    CardPtr shown = engine.askChooseCard(target, target->getHandCards(),
                                         who(self) + " 对你发动【义绝】，请展示一张手牌", false, aiShow);
    if (!shown) shown = target->getHandCards().front();
    engine.logMessage("  " + who(*target) + " 展示了手牌 " + shown->getFormattedName());

    if (engine.effectiveSuit(*target,shown)==Suit::SPADE || engine.effectiveSuit(*target,shown)==Suit::CLUB) {
        target->setNonLockSkillsDisabled(true);
        target->setHandCardsBanned(true);
        target->setYiJueMarkSourceId(self.getId());
        engine.logMessage("  展示的是黑色牌！" + who(*target) + " 本回合非锁定技失效且不能使用或打出手牌，"
                          + who(self) + " 对其使用的红桃【杀】伤害+1！");
    } else {
        engine.obtainCard(me, shown, target);
        if (target->isWounded()) {
            bool heal = engine.askConfirm(me, "是否令 " + who(*target) + " 回复1点体力？",
                                          AIController::isFriend(engine, self, *target));
            if (heal) engine.recoverHp(target, 1, "义绝");
        }
    }
}

void YiJueSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId() || !ctx.target || !ctx.card) return;
    if (ctx.target->getYiJueMarkSourceId() == self.getId() && engine.effectiveSuit(self,ctx.card) == Suit::HEART) {
        ctx.extraDamage += 1;
        engine.logMessage("  【义绝】效果：" + who(self) + " 对 " + who(*ctx.target) + " 使用的红桃【杀】伤害+1！");
    }
}

// ------------------------------ 排异 ------------------------------
PaiYiSkill::PaiYiSkill(bool jieVersion)
    : ActiveSkill(jieVersion ? "界-排异" : "一-排异",
                  // 官网 122 自立括号内原文（“权”不带引号）。
                  "出牌阶段限一次，你可以将一张权置入弃牌堆并选择一名角色，然后其摸两张牌。"
                  "若该角色的手牌多于你，则你对其造成1点伤害。",
                  1) {}

bool PaiYiSkill::canActivate(GameEngine& /*engine*/, Player& self) {
    return hasUsesLeft() && self.getPileCount(PILE_QUAN) > 0;
}

bool PaiYiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (!AIController::isFriend(engine, self, *p) && p->getHandCardCount() + 2 > self.getHandCardCount()) return true;
    }
    // 权较多时用于补给队友/自己
    return self.getPileCount(PILE_QUAN) >= 2 || self.getHandCardCount() <= 2;
}

void PaiYiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    const auto& quan = self.getPile(PILE_QUAN);
    CardPtr chosenQuan = engine.askChooseCard(me, quan, "【排异】请选择要移去的一张\"权\"", true, quan.front());
    if (!chosenQuan) return;

    auto alive = engine.getAlivePlayers();
    // AI：优先能造成伤害的敌人（体力最少者），否则手牌最少的队友，否则自己
    PlayerPtr aiTarget = nullptr;
    for (auto& p : alive) {
        if (p->getId() == self.getId() || AIController::isFriend(engine, self, *p)) continue;
        if (p->getHandCardCount() + 2 > self.getHandCardCount()) {
            if (!aiTarget || p->getHp() < aiTarget->getHp()) aiTarget = p;
        }
    }
    if (!aiTarget) {
        for (auto& p : alive) {
            if (p->getId() != self.getId() && AIController::isFriend(engine, self, *p) &&
                p->getHandCardCount() + 2 <= self.getHandCardCount()) {
                if (!aiTarget || p->getHandCardCount() < aiTarget->getHandCardCount()) aiTarget = p;
            }
        }
    }
    if (!aiTarget) aiTarget = me;

    PlayerPtr target = engine.askChoosePlayer(me, alive, "【排异】请选择一名角色（其摸两张牌，若其手牌多于你则受到1点伤害）", true, aiTarget);
    if (!target) return;

    markUsed();
    self.removeFromPile(PILE_QUAN, chosenQuan);
    engine.getDeck().discardCard(chosenQuan);
    engine.logMessage("  " + who(self) + " 发动【排异】，移去了\"权\" " + chosenQuan->getFormattedName() + "，目标 " + who(*target));
    engine.drawCards(target, 2, "排异");
    if (target->getHandCardCount() > self.getHandCardCount()) {
        engine.logMessage("  " + who(*target) + " 的手牌数多于 " + who(self) + "，受到1点伤害！");
        engine.applyDamage(me, target, 1);
    }
}

// DIY 钟会的独立【排异】实现：与官方技能不共享技能状态和实现。
DiyPaiYiSkill::DiyPaiYiSkill()
    : ActiveSkill("diy-排异",
                  "出牌阶段限一次，你可以将一张\"权\"置入弃牌堆并选择一名角色，然后其摸两张牌。"
                  "若该角色的手牌多于你，则你对其造成1点伤害。",
                  1) {}

bool DiyPaiYiSkill::canActivate(GameEngine& engine, Player& self) {
    return self.isAlive() && hasUsesLeft() && self.getPileCount(PILE_QUAN) > 0 &&
           engine.getCurrentPhase() == TurnPhase::PLAY && engine.isPlayerTurn(self) &&
           !engine.getAlivePlayers().empty();
}

bool DiyPaiYiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (!AIController::isFriend(engine, self, *p) && p->getHandCardCount() + 2 > self.getHandCardCount()) return true;
    }
    // 权较多时用于补给队友/自己
    return self.getPileCount(PILE_QUAN) >= 2 || self.getHandCardCount() <= 2;
}

void DiyPaiYiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    const auto& quan = self.getPile(PILE_QUAN);
    CardPtr chosenQuan = engine.askChooseCard(me, quan, "【排异】请选择要移去的一张\"权\"", true, quan.front());
    if (!chosenQuan) return;

    auto alive = engine.getAlivePlayers();
    // AI：优先能造成伤害的敌人（体力最少者），否则手牌最少的队友，否则自己
    PlayerPtr aiTarget = nullptr;
    for (auto& p : alive) {
        if (p->getId() == self.getId() || AIController::isFriend(engine, self, *p)) continue;
        if (p->getHandCardCount() + 2 > self.getHandCardCount()) {
            if (!aiTarget || p->getHp() < aiTarget->getHp()) aiTarget = p;
        }
    }
    if (!aiTarget) {
        for (auto& p : alive) {
            if (p->getId() != self.getId() && AIController::isFriend(engine, self, *p) &&
                p->getHandCardCount() + 2 <= self.getHandCardCount()) {
                if (!aiTarget || p->getHandCardCount() < aiTarget->getHandCardCount()) aiTarget = p;
            }
        }
    }
    if (!aiTarget) aiTarget = me;

    PlayerPtr target = engine.askChoosePlayer(me, alive, "【排异】请选择一名角色（其摸两张牌，若其手牌多于你则受到1点伤害）", true, aiTarget);
    if (!target) return;

    markUsed();
    self.removeFromPile(PILE_QUAN, chosenQuan);
    engine.getDeck().discardCard(chosenQuan);
    engine.logMessage("  " + who(self) + " 发动【排异】，移去了\"权\" " + chosenQuan->getFormattedName() + "，目标 " + who(*target));
    engine.drawCards(target, 2, "排异");
    if (target->getHandCardCount() > self.getHandCardCount()) {
        engine.logMessage("  " + who(*target) + " 的手牌数多于 " + who(self) + "，受到1点伤害！");
        engine.applyDamage(me, target, 1);
    }
}

// =====================================================================
//                              触发技
// =====================================================================

// ------------------------------ 铁骑（标） ------------------------------
TieQiSkill::TieQiSkill()
    : TriggerSkill("铁骑", "当你使用【杀】指定一个目标后，你可以进行判定，若结果为红色，该角色不能使用【闪】响应此【杀】。") {}

void TieQiSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId() || ctx.cannotDodge) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "是否发动【铁骑】进行判定？", true)) return;
    CardPtr judge = engine.doJudgement(me, "铁骑");
    if (judge && (engine.effectiveSuit(self,judge)==Suit::HEART || engine.effectiveSuit(self,judge)==Suit::DIAMOND)) {
        ctx.cannotDodge = true;
        engine.logMessage("  【铁骑】判定为红色！" + who(*ctx.target) + " 不能使用【闪】响应此【杀】！");
    } else {
        engine.logMessage("  【铁骑】判定为黑色，未生效。");
    }
}

// ------------------------------ 铁骑（界） ------------------------------
JieTieQiSkill::JieTieQiSkill()
    : TriggerSkill("界-铁骑", "当你使用【杀】指定一个目标后，你可令其本回合内非锁定技失效，然后你进行判定，"
                           "除非该角色弃置与结果花色相同的一张牌，否则不能使用【闪】响应此【杀】。") {}

void JieTieQiSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId() || ctx.cannotDodge) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr target = ctx.target;
    if (!engine.askConfirm(me, "是否发动【铁骑】？", true)) return;

    target->setNonLockSkillsDisabled(true);
    engine.logMessage("  【铁骑】发动！" + who(*target) + " 本回合内非锁定技失效。");

    CardPtr judge = engine.doJudgement(me, "铁骑");
    if (!judge) return;
    Suit suit = engine.effectiveSuit(self,judge);

    std::vector<CardPtr> sameSuit;
    for (auto& c : target->getHandCards()) if (engine.effectiveSuit(*target,c) == suit) sameSuit.push_back(c);
    for (auto& c : target->getAllEquipment()) if (engine.effectiveSuit(*target,c) == suit) sameSuit.push_back(c);

    if (sameSuit.empty()) {
        ctx.cannotDodge = true;
        engine.logMessage("  " + who(*target) + " 没有" + judge->getSuitSymbol() + "花色的牌，不能使用【闪】响应此【杀】！");
        return;
    }

    // AI：手上有【闪】（或可转化的闪）且有可弃的同花色牌时，弃牌保留响应权
    CardPtr aiChoice = nullptr;
    if (!engine.getResponseCandidates(target, CardSubType::SHAN).empty()) {
        aiChoice = AIController::chooseLeastValuableCard(sameSuit);
    }
    CardPtr toDiscard = engine.askChooseCard(target, sameSuit,
        "【铁骑】判定为 " + judge->getFormattedName() + "，弃置一张" + judge->getSuitSymbol() + "花色的牌才能使用【闪】响应，是否弃置？",
        true, aiChoice);
    if (toDiscard) {
        engine.discardCardOf(target, toDiscard, "铁骑");
    } else {
        ctx.cannotDodge = true;
        engine.logMessage("  " + who(*target) + " 未弃置同花色牌，不能使用【闪】响应此【杀】！");
    }
}

// ------------------------------ 烈弓（标） ------------------------------
LieGongSkill::LieGongSkill()
    : TriggerSkill("烈弓", "当你于出牌阶段内使用【杀】指定一个目标后，若该角色的手牌数不小于你的体力值或不大于你的攻击范围，"
                           "则你可以令其不能使用【闪】响应此【杀】。") {}

void LieGongSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId() || ctx.cannotDodge) return;
    if (!engine.isPlayerTurn(self) || engine.getCurrentPhase() != TurnPhase::PLAY) return;
    int hand = ctx.target->getHandCardCount();
    if (hand >= self.getHp() || hand <= self.getAttackRange()) {
        PlayerPtr me = selfPtr(engine, self);
        if (engine.askConfirm(me, "是否发动【烈弓】令 " + who(*ctx.target) + " 不能使用【闪】响应此【杀】？", true)) {
            ctx.cannotDodge = true;
            engine.logMessage("  【烈弓】发动！" + who(*ctx.target) + " 不能使用【闪】响应此【杀】！");
        }
    }
}

// ------------------------------ 烈弓（界） ------------------------------
JieLieGongSkill::JieLieGongSkill()
    : TriggerSkill("界-烈弓", "你使用【杀】可选择在此【杀】点数距离内的角色为目标。当你使用【杀】指定目标后，你可以根据下列条件执行相应的效果："
                           "1.若你的手牌数大于等于其手牌数，该角色不能使用【闪】；2.若你的体力值小于等于其体力值，此【杀】伤害+1。") {}

void JieLieGongSkill::onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr sha, bool& canTarget) {
    if (canTarget || !sha || sha->getRank() <= 0) return;
    if (engine.calculateDistance(self, target) <= sha->getRank()) {
        canTarget = true;
    }
}

void JieLieGongSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId()) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!ctx.cannotDodge && ctx.target->getHandCardCount() <= self.getHandCardCount()) {
        if (engine.askConfirm(me, "【烈弓】目标手牌数不大于你，是否令其不能使用【闪】响应此【杀】？", true)) {
            ctx.cannotDodge = true;
            engine.logMessage("  【烈弓】发动！" + who(*ctx.target) + " 不能使用【闪】响应此【杀】！");
        }
    }
    if (ctx.target->getHp() >= self.getHp()) {
        if (engine.askConfirm(me, "【烈弓】目标体力值不小于你，是否令此【杀】伤害+1？", true)) {
            ctx.extraDamage += 1;
            engine.logMessage("  【烈弓】发动！此【杀】伤害+1！");
        }
    }
}

// ------------------------------ 替身 ------------------------------
TiShenSkill::TiShenSkill()
    : TriggerSkill("界-替身", "出牌阶段结束时，你可发动此技能。你弃置所有锦囊牌和坐骑牌。"
                           "然后直到你的下回合开始，获得所有以你为目标且未对你造成伤害的【杀】。") {}

void TiShenSkill::onTurnStart(GameEngine& engine, Player& self) {
    if (!engine.isPlayerTurn(self)) return; // "直到你的下回合开始"：只在自己的回合开始时清除
    if (self.isTiShenActive()) {
        self.setTiShenActive(false);
        engine.logMessage("  " + who(self) + " 的【替身】效果结束。");
    }
}

void TiShenSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY || !self.isAlive()) return;
    if (!engine.isPlayerTurn(self)) return; // "出牌阶段结束时"：仅自己的出牌阶段
    PlayerPtr me = selfPtr(engine, self);

    std::vector<CardPtr> toDiscard;
    for (auto& c : self.getHandCards()) if (c->getType() == CardType::TRICK) toDiscard.push_back(c);
    if (self.getOffensiveHorse()) toDiscard.push_back(self.getOffensiveHorse());
    if (self.getDefensiveHorse()) toDiscard.push_back(self.getDefensiveHorse());

    bool aiWants = toDiscard.size() <= 1 || self.getHp() <= 2;
    if (!engine.askConfirm(me, "出牌阶段结束，是否发动【替身】？（弃置所有锦囊牌和坐骑牌，直到下回合开始获得未对你造成伤害的【杀】）", aiWants)) {
        return;
    }
    engine.logMessage("  " + who(self) + " 发动了【替身】！");
    for (auto& c : toDiscard) engine.discardCardOf(me, c, "替身");
    self.setTiShenActive(true);
}

void TiShenSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.target || ctx.target->getId() != self.getId()) return;
    if (ctx.cardClaimed || !self.isTiShenActive() || !ctx.card) return;
    // “未对你造成伤害”以体力是否因本【杀】下降为准：大雾防止、天香转移、寒冰改弃均视为未造成伤害。
    if (ctx.hpBeforeDamage >= 0 && self.getHp() < ctx.hpBeforeDamage) return;
    PlayerPtr me = selfPtr(engine, self);
    auto reals = ctx.card->getRealCards(ctx.card);
    for (auto& c : reals) {
        engine.obtainCard(me, c, nullptr);
    }
    ctx.cardClaimed = true;
    engine.logMessage("  【替身】效果：" + who(self) + " 获得了未对其造成伤害的【杀】。");
}

// ------------------------------ 涯角 ------------------------------
YaJiaoSkill::YaJiaoSkill()
    : TriggerSkill("界-涯角", "当你于回合外使用或打出手牌时，你可以展示牌堆顶一张牌并将其交给任意一名角色。"
                           "若这两张牌类别不同，你弃置一张牌。") {}

void YaJiaoSkill::onCardUsedOutsideTurn(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || !self.isAlive()) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "是否发动【涯角】展示牌堆顶一张牌并交给一名角色？", true)) return;

    CardPtr top = engine.getDeck().drawCard();
    if (!top) return;
    engine.logMessage("  " + who(self) + " 发动【涯角】，展示了牌堆顶的 " + top->getFormattedName()
                      + "（" + top->getTypeString() + "）");

    auto alive = engine.getAlivePlayers();
    // AI：手牌最少的队友（含自己）
    PlayerPtr aiTarget = me;
    for (auto& p : alive) {
        if (AIController::isFriend(engine, self, *p) && p->getHandCardCount() < aiTarget->getHandCardCount()) aiTarget = p;
    }
    PlayerPtr receiver = engine.askChoosePlayer(me, alive, "【涯角】请选择获得这张牌的角色", false, aiTarget);
    if (!receiver) receiver = me;
    receiver->addHandCard(top);
    engine.logMessage("  " + who(*receiver) + " 获得了 " + top->getFormattedName());

    if (top->getType() != card->getType()) {
        std::vector<CardPtr> mine = self.getHandCards();
        auto eq = self.getAllEquipment();
        mine.insert(mine.end(), eq.begin(), eq.end());
        if (mine.empty()) return;
        CardPtr toDiscard = engine.askChooseCard(me, mine, "【涯角】两张牌类别不同，请弃置一张牌", false,
                                                 AIController::chooseLeastValuableCard(mine));
        if (!toDiscard) toDiscard = mine.front();
        engine.discardCardOf(me, toDiscard, "涯角");
    }
}

// ------------------------------ 权计 ------------------------------
QuanJiSkill::QuanJiSkill(bool jieVersion)
    // 官网 122（标准）与 368（界）措辞不同，分别照录：
    // 标准：“你可以摸一张牌。若如此做，你将一张手牌置于武将牌上，称为权……”
    // 界：“……你可以摸一张牌，然后你将一张手牌置于武将牌上，称为“权”……”（官网无句尾句号）
    : TriggerSkill(jieVersion ? "界-权计" : "一-权计", jieVersion
        ? std::string("出牌阶段结束时，若你的手牌数大于你的体力值，或当你受到1点伤害后，你可以摸一张牌，"
                      "然后你将一张手牌置于武将牌上，称为\"权\"；你的手牌上限+X（X为\"权\"的数量）")
        : std::string("当你受到1点伤害后，你可以摸一张牌。若如此做，你将一张手牌置于武将牌上，"
                      "称为权；你的手牌上限+X（X为权的数量）。")), jie(jieVersion) {}

void QuanJiSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (!jie || phase != TurnPhase::PLAY || engine.getCurrentPlayer().get() != &self ||
        !self.isAlive() || self.getHandCardCount() <= self.getHp()) return;
    if (engine.askConfirm(selfPtr(engine, self), "是否发动【权计】（出牌阶段结束，摸一张牌并置一张为权）？", true))
        doOfficialQuanJiEffect(engine, self);
}

void QuanJiSkill::onAfterDamage(GameEngine& engine, Player& self, Player* /*source*/, int damage, ShaElement /*element*/, CardPtr /*cause*/) {
    PlayerPtr me = selfPtr(engine, self);
    for (int i = 0; i < damage && self.isAlive(); ++i) {
        if (!engine.askConfirm(me, "是否发动【权计】（摸一张牌，然后将一张手牌置为\"权\"）？", true)) continue;
        doOfficialQuanJiEffect(engine, self);
    }
}

void QuanJiSkill::onCalculateHandLimit(GameEngine& /*engine*/, const Player& self, int& handLimit) {
    handLimit += self.getPileCount(PILE_QUAN);
}

// ------------------------------ 自立 ------------------------------
ZiLiSkill::ZiLiSkill(bool jieVersion)
    // 官网 122（标准）“获得‘排异’” / 368（界）“获得技能‘排异※’”，分别照录。
    : TriggerSkill(jieVersion ? "界-自立" : "一-自立", jieVersion
        ? "觉醒技，准备阶段，若\"权\"的数量不小于3，你选择一项：1.回复1点体力；2.摸两张牌。"
          "然后你减1点体力上限，获得技能\"排异※\"。"
        : "觉醒技，准备阶段，若\"权\"的数量不小于3，你选择一项：1.回复1点体力；2.摸两张牌。"
          "然后你减1点体力上限，获得\"排异\"。", SkillTag::AWAKEN),
      awakened(false) {}

void ZiLiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& /*skipPhase*/) {
    if (phase != TurnPhase::PREPARATION) return;
    ziLiAwaken(engine, self, awakened);
}

// =====================================================================
//                              状态技
// =====================================================================

// ------------------------------ 咆哮 ------------------------------
PaoXiaoSkill::PaoXiaoSkill(bool jieVersion)
    : StateSkill(jieVersion ? "界-咆哮" : "咆哮",
                 jieVersion ? "锁定技，你使用【杀】无次数限制。你的出牌阶段，若你于当前阶段内使用过【杀】，你于此阶段使用【杀】无距离限制。"
                            : "锁定技，你使用【杀】无次数限制。",
                 SkillTag::LOCK),
      jie(jieVersion) {}

void PaoXiaoSkill::onCalculateShaLimit(GameEngine& /*engine*/, const Player& /*self*/, int& shaLimit) {
    shaLimit = 999;
}

void PaoXiaoSkill::onCheckShaTarget(GameEngine& engine, const Player& self, const Player& /*target*/, CardPtr /*sha*/, bool& canTarget) {
    if (!jie || canTarget) return;
    if (engine.isPlayerTurn(self) && engine.getCurrentPhase() == TurnPhase::PLAY && self.getShaCountThisTurn() >= 1) {
        canTarget = true;
    }
}

// ------------------------------ 马术 ------------------------------
MaShuSkill::MaShuSkill()
    : StateSkill("马术", "锁定技，你计算与其他角色的距离-1。", SkillTag::LOCK) {}

void MaShuSkill::onCalculateDistance(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*target*/, int& distance) {
    distance = std::max(1, distance - 1);
}

// =====================================================================
//                    DIY 钟会专属（持恒技组）
// =====================================================================

// ------------------------------ 权谋 ------------------------------
QuanMouSkill::QuanMouSkill()
    : StateSkill("diy-权谋", "持恒技，蓄力技（1/5）。游戏开始时视为首轮开始，获得1点蓄力；此后每轮开始时获得2点蓄力，你的出牌阶段开始时获得1点蓄力，至多5点。"
                         "当满足以下条件时，你可以发动一次【权计】：1、当你造成伤害后（每回合一次），或每受到1点伤害后；"
                         "2、一名玩家的出牌阶段结束时，你可以弃置两张牌，以此法发动。", SkillTag::NONE),
      roundsSeen(0), dealTriggered(false) {}

int QuanMouSkill::getCharge(const Player& self) const {
    return self.getMark("蓄力");
}

void QuanMouSkill::onGameStart(GameEngine& engine, Player& self) {
    setMark(self, "蓄力上限", MAX_CHARGE); // 蓄力技（1/5）：上限 5（供局势显示与跨技能引用）
    const int current = self.getMark("蓄力");
    if (current < 1) {
        self.addMark("蓄力", 1 - current);
        engine.logMessage("  " + who(self) + " 的【权谋】于游戏开始时获得1点蓄力（作为首轮起始蓄力）。");
    }
}

void QuanMouSkill::onRoundStart(GameEngine& engine, Player& self) {
    const int current = self.getMark("蓄力");
    int next = current;
    if (roundsSeen == 0) {
        // 游戏开始时的首次 onRoundStart 就是第1轮开始；将原 X=1 转为1点起始蓄力。
        next = std::max(current, 1);
    } else {
        next = std::min(MAX_CHARGE, current + 2);
    }
    if (next > current) {
        self.addMark("蓄力", next - current);
        engine.logMessage("  " + who(self) + " 的【权谋】获得 " + std::to_string(next - current) +
                          " 点蓄力（当前 " + std::to_string(next) + "/" + std::to_string(MAX_CHARGE) + "）。");
    }
    ++roundsSeen;
}

void QuanMouSkill::onTurnStart(GameEngine& /*engine*/, Player& /*self*/) {
    dealTriggered = false;
}

void QuanMouSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& /*skipPhase*/) {
    if (phase != TurnPhase::PLAY) return;
    const int current = self.getMark("蓄力");
    if (current >= MAX_CHARGE) return;
    self.addMark("蓄力", 1);
    engine.logMessage("  " + who(self) + " 的【权谋】在出牌阶段开始时获得1点蓄力（当前 " +
                      std::to_string(self.getMark("蓄力")) + "/" + std::to_string(MAX_CHARGE) + "）。");
}

void QuanMouSkill::onAfterDealDamage(GameEngine& engine, Player& self, Player* /*target*/, int /*damage*/, ShaElement /*element*/, CardPtr /*cause*/) {
    if (dealTriggered || !self.isAlive()) return;
    dealTriggered = true;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "是否发动【权谋】→【权计】？（摸一张牌，然后将一张手牌置为\"权\"）", true)) return;
    engine.logMessage("  " + who(self) + " 因造成伤害发动【权谋】→【权计】！");
    doDiyQuanJiEffect(engine, self);
}

void QuanMouSkill::onAfterDamage(GameEngine& engine, Player& self, Player* /*source*/, int damage, ShaElement /*element*/, CardPtr /*cause*/) {
    PlayerPtr me = selfPtr(engine, self);
    for (int i = 0; i < damage && self.isAlive(); ++i) {
        if (!engine.askConfirm(me, "是否因受到伤害发动【权谋】→【权计】？（摸一张牌，然后将一张手牌置为\"权\"）", true)) continue;
        engine.logMessage("  " + who(self) + " 因受到伤害发动【权谋】→【权计】！");
        doDiyQuanJiEffect(engine, self);
    }
}

void QuanMouSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY || !self.isAlive()) return;
    if (self.getHandAndEquipmentCards().size() < 2) return;
    PlayerPtr me = selfPtr(engine, self);
    bool aiWants = self.getHandAndEquipmentCards().size() >= 3 && self.getMark("蓄力") > 0;
    if (!engine.askConfirm(me, "一名玩家的出牌阶段结束，是否发动【权谋】？（弃置两张牌发动一次【权计】）", aiWants)) return;
    for (int i = 0; i < 2; ++i) {
        auto discardable = self.getHandAndEquipmentCards();
        if (discardable.empty()) break;
        CardPtr c = engine.askChooseCard(me, discardable, "【权谋】弃置第 " + std::to_string(i + 1) + " 张牌（共2张）", false,
                                         AIController::chooseLeastValuableCard(discardable));
        if (!c) c = discardable.front();
        engine.discardCardOf(me, c, "权谋");
    }
    engine.logMessage("  " + who(self) + " 弃置两张牌，发动【权谋】→【权计】！");
    doDiyQuanJiEffect(engine, self); // X结算已移入【权计】内
}

// ------------------------------ 权计（持恒版） ------------------------------
QuanJiConstSkill::QuanJiConstSkill()
    : StateSkill("diy-权计", "持恒技。出牌阶段限一次，摸一张牌并将一张手牌置于武将牌上，称为\"权\"。"
                         "每次发动【权计】后，若你有蓄力点，你消耗1点蓄力并摸一张牌。"
                         "你的手牌上限加上\"权\"的数量。", SkillTag::NONE),
      usesThisTurn(0) {}

bool QuanJiConstSkill::canActivate(GameEngine& engine, Player& self) {
    if (!self.isAlive() || usesThisTurn >= 1 || engine.getCurrentPhase() != TurnPhase::PLAY ||
        !engine.isPlayerTurn(self)) return false;
    const int availableToDraw = engine.getDeck().getDrawPileSize() + engine.getDeck().getDiscardPileSize();
    return self.getHandCardCount() + std::min(1, availableToDraw) > 0;
}

void QuanJiConstSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    usesThisTurn++;
    doDiyQuanJiEffect(engine, self);
}

bool QuanJiConstSkill::aiShouldActivate(GameEngine& /*engine*/, Player& /*self*/) {
    return true; // 净手牌无损地赚一张"权"，总是值得
}

void QuanJiConstSkill::onCalculateHandLimit(GameEngine& /*engine*/, const Player& self, int& handLimit) {
    handLimit += self.getPileCount(PILE_QUAN);
}

// ------------------------------ 自立（持恒版） ------------------------------
ZiLiConstSkill::ZiLiConstSkill()
    : StateSkill("diy-自立", "觉醒技，准备阶段，若\"权\"的数量不小于3，你选择一项：1.回复1点体力；2.摸两张牌。"
                         "然后你减1点体力上限，获得\"排异\"。", SkillTag::AWAKEN),
      awakened(false) {}

void ZiLiConstSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& /*skipPhase*/) {
    if (phase != TurnPhase::PREPARATION) return;
    diyZiLiAwaken(engine, self, awakened);
}

} // namespace Thks
