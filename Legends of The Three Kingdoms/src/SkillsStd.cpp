// SkillsStd.cpp — 三国杀标准包（标包）武将技能实现
#include "SkillsStd.h"
#include "GameEngine.h"
#include "Player.h"
#include "AI.h"
#include <algorithm>
#include <random>

namespace Thks {

namespace {

PlayerPtr selfPtr(GameEngine& engine, const Player& self) {
    return engine.getPlayerById(self.getId());
}

std::string who(const Player& p) {
    return "[" + p.getName() + "]";
}

bool isMale(const Player& p) {
    return p.getHero() && p.getHero()->getGender() == Gender::MALE;
}

// 收集某人的手牌+装备区牌
std::vector<CardPtr> handAndEquip(const Player& p) {
    return p.getHandAndEquipmentCards();
}

// 【制衡】AI 可从手牌或装备区换掉低价值牌；判定区牌不在普通弃牌候选中。
bool isZhiHengFodder(GameEngine& engine, Player& self, CardPtr c) {
    return c && AIController::cardValue(engine, self, c) <= 3;
}

// 令一名同势力角色代为打出 wanted（激将/护驾共用）；成功返回该牌
CardPtr borrowFromAllies(GameEngine& engine, Player& self, Country country, CardSubType wanted,
                         const std::string& skillName) {
    PlayerPtr me = selfPtr(engine, self);
    for (auto& ally : engine.getOtherAlivePlayers(self)) {
        if (!ally->getHero() || ally->getHero()->getCountry() != country) continue;
        if (ally->getHandCardCount() == 0 && ally->getAllEquipment().empty()) continue;
        std::string cardName = wanted == CardSubType::SHA ? "【杀】" : "【闪】";
        std::string prompt = who(self) + " 发动【" + skillName + "】，是否打出一张" + cardName + "？";
        bool aiWants = ally->isAI() ? (AIController::isFriend(engine, *ally, self) &&
                                       !engine.getResponseCandidates(ally, wanted).empty())
                                    : true;
        if (!engine.askConfirm(ally, prompt, aiWants)) continue;
        CardPtr out = engine.askResponseCard(ally, wanted, "请打出一张" + cardName + "（" + skillName + "）");
        if (out) {
            engine.logMessage("  " + who(*ally) + " 替 " + who(self) + " 打出了 " + out->getFormattedName());
            return out;
        }
    }
    return nullptr;
}

} // namespace

// ============================== 蜀 ==============================

// ------------------------------ 仁德 ------------------------------
RenDeSkill::RenDeSkill()
    : ActiveSkill("仁德", "出牌阶段，你可以将任意张手牌交给其他角色，然后若你于此阶段内给出第二张“仁德”牌时，你回复1点体力。", 0) {}

bool RenDeSkill::canActivate(GameEngine& engine, Player& self) {
    return self.getHandCardCount() > 0 && !engine.getOtherAlivePlayers(self).empty();
}

void RenDeSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr target = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self),
                                              "【仁德】选择要交给手牌的角色", true);
    if (!target) return;
    while (self.getHandCardCount() > 0) {
        CardPtr card = engine.askChooseCard(me, self.getHandCards(), "【仁德】交给 " + who(*target) +
                                            " 的手牌（0=结束）", true,
                                            AIController::chooseLeastValuableCard(self.getHandCards()));
        if (!card) break;
        engine.obtainCard(target, card, me);
        ++givenThisPhase;
        if (givenThisPhase == 2 && !healedThisPhase) {
            healedThisPhase = true;
            engine.logMessage("  【仁德】本阶段给出第2张牌，" + who(self) + " 回复1点体力。");
            engine.recoverHp(me, 1, "仁德");
        }
    }
}

bool RenDeSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (self.getHandCardCount() < 3) return false;
    if (healedThisPhase) return false;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (AIController::isFriend(engine, self, *p) && p->isWounded()) return true;
    }
    return false;
}

// ------------------------------ 激将（主公技） ------------------------------
JiJiangSkill::JiJiangSkill()
    : ActiveSkill("激将", "主公技，当你需要使用或打出【杀】时，你可以令其他蜀势力角色选择是否打出一张【杀】（视为由你使用或打出）。", 0, SkillTag::LORD) {}

bool JiJiangSkill::canActivate(GameEngine& engine, Player& self) {
    if (!self.getHero()) return false;
    bool hasAlly = false;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (p->getHero() && p->getHero()->getCountry() == self.getHero()->getCountry()) { hasAlly = true; break; }
    }
    if (!hasAlly) return false;
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "激将");
    return !engine.getShaTargets(self, sha).empty();
}

void JiJiangSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (self.getShaCountThisTurn() >= engine.getShaLimit(self)) {
        engine.logMessage("  本回合使用【杀】的次数已达上限！");
        return;
    }
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "激将");
    auto candidates = engine.getShaTargets(self, sha);
    if (candidates.empty()) {
        engine.logMessage("  攻击范围内没有可选的目标！");
        return;
    }
    PlayerPtr target = engine.askChoosePlayer(me, candidates, "【激将】选择【杀】的目标", true);
    if (!target) return;
    CardPtr borrowed = borrowFromAllies(engine, self, self.getHero()->getCountry(), CardSubType::SHA, "激将");
    if (!borrowed) {
        engine.logMessage("  没有其他蜀势力角色打出【杀】，【激将】无效。");
        return;
    }
    me->incrementShaCount();
    engine.resolveSha(me, borrowed, {target});
}

bool JiJiangSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

bool JiJiangSkill::onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) {
    if (wanted != CardSubType::SHA || !self.getHero()) return false;
    out = borrowFromAllies(engine, self, self.getHero()->getCountry(), CardSubType::SHA, "激将");
    return out != nullptr;
}

// ------------------------------ 集智 ------------------------------
JiZhiSkill::JiZhiSkill()
    : TriggerSkill("集智", "当你使用一张普通锦囊牌时，你可以摸一张牌。") {}

void JiZhiSkill::onUseCard(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getType() != CardType::TRICK) return;
    PlayerPtr me = selfPtr(engine, self);
    if (engine.askConfirm(me, "【集智】使用了普通锦囊牌 " + card->getFormattedName() + "，是否摸一张牌？", true)) {
        engine.logMessage("  " + who(self) + " 发动【集智】！");
        engine.drawCards(me, 1, "集智");
    }
}

// ------------------------------ 奇才 ------------------------------
QiCaiSkill::QiCaiSkill()
    : StateSkill("奇才", "锁定技，你使用锦囊牌无距离限制；（官网原文如此）", SkillTag::LOCK) {}

// ------------------------------ 观星 ------------------------------
GuanXingSkill::GuanXingSkill()
    : TriggerSkill("观星", "准备阶段，你可以观看牌堆顶的X张牌（X为全场角色数且最多为5），然后将其中任意数量的牌置于牌堆顶，将其余的牌置于牌堆底。") {}

void GuanXingSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& /*skipPhase*/) {
    if (phase != TurnPhase::PREPARATION) return;
    PlayerPtr me = selfPtr(engine, self);
    int aliveCount = static_cast<int>(engine.getAlivePlayers().size());
    int n = std::min(5, aliveCount);
    if (n <= 0 || engine.getDeck().getDrawPileSize() == 0) return;
    if (!engine.askConfirm(me, "【观星】是否观看牌堆顶 " + std::to_string(n) + " 张牌并调整顺序？", true)) return;
    n = std::min(n, engine.getDeck().getDrawPileSize());

    std::vector<CardPtr> cards = engine.getDeck().drawCards(n); // cards[0] = 牌堆顶
    engine.logMessage("  " + who(self) + " 发动【观星】观看了牌堆顶 " + std::to_string(n) + " 张牌。");
    engine.viewCards(me,cards,"【观星】仅你可见的牌堆顶牌面");

    std::vector<CardPtr> top, bottom;
    if (me->isAI()) {
        // AI：价值高的留在牌堆顶（按价值降序），其余置底
        std::vector<CardPtr> rest = cards;
        while (!rest.empty()) {
            CardPtr best = AIController::chooseMostValuableCard(engine, self, rest);
            top.push_back(best);
            rest.erase(std::remove(rest.begin(), rest.end(), best), rest.end());
        }
    } else {
        // 玩家：依次选择置于牌堆顶的牌（按顶→次顶顺序），0/取消结束，其余置底
        std::vector<CardPtr> rest = cards;
        while (!rest.empty()) {
            std::string names;
            for (size_t i = 0; i < rest.size(); ++i) names += "[" + std::to_string(i + 1) + "]" + rest[i]->getFormattedName() + " ";
            CardPtr pick = engine.askChooseCard(me, rest,
                "【观星】选择下一张置于牌堆顶的牌（当前可选: " + names + "，0=结束并将其余置底）", true);
            if (!pick) break;
            top.push_back(pick);
            rest.erase(std::remove(rest.begin(), rest.end(), pick), rest.end());
        }
        // 剩余牌的牌堆底顺序也由玩家决定（从最底到靠近牌堆顶）。
        while (!rest.empty()) {
            CardPtr pick = engine.askChooseCard(me, rest,
                "【观星】选择下一张置于牌堆底的牌（从最底到上）", false);
            if (!pick) pick = rest.front();
            bottom.push_back(pick);
            rest.erase(std::remove(rest.begin(), rest.end(), pick), rest.end());
        }
    }

    engine.getDeck().putOnTop(top);
    engine.getDeck().putOnBottom(bottom);
    engine.logMessage("  " + std::to_string(top.size()) + " 张置于牌堆顶，" + std::to_string(bottom.size()) + " 张置于牌堆底。");
}

// ------------------------------ 空城 ------------------------------
KongChengSkill::KongChengSkill()
    : StateSkill("空城", "锁定技，若你没有手牌，则你不能被选择为【杀】或【决斗】的目标。", SkillTag::LOCK) {}

void KongChengSkill::onCheckCardTarget(GameEngine& /*engine*/, const Player& self, const Player& /*target*/,
                                       CardPtr card, bool& canTarget) {
    if (!canTarget || !card) return;
    if (self.getHandCardCount() == 0 &&
        (card->getSubType() == CardSubType::SHA || card->getSubType() == CardSubType::JUE_DOU)) {
        canTarget = false;
    }
}

// ============================== 魏 ==============================

// ------------------------------ 奸雄 ------------------------------
JianXiongSkill::JianXiongSkill()
    : TriggerSkill("奸雄", "当你受到伤害后，你可以获得造成此伤害的牌。") {}

bool JianXiongSkill::onClaimDamageCard(GameEngine& engine, Player& self, Player* /*source*/, CardPtr cause) {
    if (!cause) return false;
    auto reals = cause->getRealCards(cause);
    if (reals.empty()) return false;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【奸雄】是否获得对你造成伤害的牌 " + cause->getFormattedName() + "？", true)) return false;
    for (auto& r : reals) {
        self.addHandCard(r);
    }
    return true;
}

// ------------------------------ 护驾（主公技） ------------------------------
HuJiaSkill::HuJiaSkill()
    : TriggerSkill("护驾", "主公技，当你需要使用或打出【闪】时，你可以令其他魏势力角色选择是否打出一张【闪】（视为由你使用或打出）。", SkillTag::LORD) {}

bool HuJiaSkill::onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) {
    if (wanted != CardSubType::SHAN || !self.getHero()) return false;
    out = borrowFromAllies(engine, self, self.getHero()->getCountry(), CardSubType::SHAN, "护驾");
    return out != nullptr;
}

// ------------------------------ 反馈 ------------------------------
FanKuiSkill::FanKuiSkill()
    : TriggerSkill("反馈", "当你受到伤害后，你可以获得伤害来源的一张牌。") {}

void FanKuiSkill::onAfterDamage(GameEngine& engine, Player& self, Player* source, int /*damage*/,
                                ShaElement /*element*/, CardPtr /*cause*/) {
    if (!source || !source->isAlive()) return;
    if (source->getAllCards().empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【反馈】是否获得 " + who(*source) + " 的一张牌？", true)) return;
    // 手、装备、判定区均属于伤害来源的区域；手牌不可向反馈者明示。
    auto from=engine.getPlayerById(source->getId());
    CardPtr chosen = engine.chooseCardFromPlayer(me,from,"【反馈】选择获得伤害来源的一张牌",true);
    if (chosen) engine.obtainCard(me, chosen, from);
}

// ------------------------------ 鬼才 ------------------------------
GuiCaiSkill::GuiCaiSkill()
    : TriggerSkill("鬼才", "当一名角色的判定牌生效前，你可以打出一张手牌代替之。") {}

void GuiCaiSkill::onBeforeJudge(GameEngine& engine, Player& self, Player& judgeTarget, CardPtr& judgeCard) {
    if (self.getHandCardCount() == 0 || !judgeCard) return;
    PlayerPtr me = selfPtr(engine, self);
    // B6：先算这次改判有没有收益、用哪张牌最省（原来是有牌就改，可能白扔关键牌甚至帮倒忙）
    CardPtr aiPick = AIController::chooseJudgementReplacement(engine, self, judgeTarget, judgeCard, false);
    CardPtr chosen;
    if (me->isAI()) {
        if (!aiPick) return;                 // 没收益就不改
        chosen = aiPick;
    } else {
        if (!engine.askConfirm(me, "【鬼才】是否打出一张手牌替换 " + who(judgeTarget) + " 的判定牌 " +
                                   judgeCard->getFormattedName() + "？", aiPick != nullptr)) return;
        chosen = engine.askChooseCard(me, self.getHandCards(), "【鬼才】选择打出的手牌", false,
                                      aiPick ? aiPick : AIController::chooseLeastValuableCard(self.getHandCards()));
    }
    if (!chosen) chosen = self.getHandCards().front();
    engine.loseHandCard(me,chosen);
    engine.logMessage("  " + who(self) + " 发动【鬼才】，将判定牌替换为 " + chosen->getFormattedName());
    judgeCard = chosen;
}

// ------------------------------ 刚烈 ------------------------------
GangLieSkill::GangLieSkill()
    : TriggerSkill("刚烈", "当你受到伤害后，你可以进行判定，若结果不为红桃，则伤害来源选择一项：1.弃置两张手牌；2.受到你造成的1点伤害。") {}

void GangLieSkill::onAfterDamage(GameEngine& engine, Player& self, Player* source, int /*damage*/,
                                 ShaElement /*element*/, CardPtr /*cause*/) {
    if (!source || !source->isAlive()) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【刚烈】是否对 " + who(*source) + " 发动刚烈？", true)) return;
    engine.logMessage("  " + who(self) + " 发动【刚烈】！");
    CardPtr res = engine.doJudgement(me, "刚烈");
    if (!res || engine.effectiveSuit(self,res) == Suit::HEART) {
        engine.logMessage("  判定为红桃，【刚烈】不生效。");
        return;
    }
    PlayerPtr src = engine.getPlayerById(source->getId());
    bool aiDiscard = src->isAI() ? (src->getHandCardCount() >= 2 && src->getHp() > 1) : false;
    if (src->getHandCardCount() >= 2 &&
        engine.askConfirm(src, "【刚烈】弃置两张手牌（否则受到1点伤害）？", aiDiscard)) {
        for (int i = 0; i < 2; ++i) {
            CardPtr c = engine.askChooseCard(src, src->getHandCards(), "【刚烈】弃置一张手牌", false,
                                             AIController::chooseLeastValuableCard(src->getHandCards()));
            if (!c) c = src->getHandCards().front();
            engine.discardCardOf(src, c, "刚烈");
        }
    } else {
        engine.logMessage("  " + who(*src) + " 受到 " + who(self) + " 造成的1点伤害！");
        engine.applyDamage(me, src, 1);
    }
}

// ------------------------------ 突袭 ------------------------------
TuXiSkill::TuXiSkill()
    : TriggerSkill("突袭", "摸牌阶段，你可以改为获得至多两名角色的各一张手牌。") {}

void TuXiSkill::onDrawCards(GameEngine& engine, Player& self, int& drawCount) {
    std::vector<PlayerPtr> victims;
    // 官网文字为“至多两名角色”，未限定“其他角色”，故本人可作为选择之一。
    for (auto& p : engine.getAlivePlayers()) {
        if (p->getHandCardCount() > 0) victims.push_back(p);
    }
    if (victims.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    bool aiWants = std::any_of(victims.begin(),victims.end(),[&](const PlayerPtr& p) {
        return p && p->getId()!=self.getId();
    });
    if (!engine.askConfirm(me, "【突袭】是否放弃摸牌，改为获得至多两名角色的各一张手牌？", aiWants)) return;

    std::vector<PlayerPtr> chosen;
    for (int k = 0; k < 2 && !victims.empty(); ++k) {
        PlayerPtr aiPick = nullptr;
        if (me->isAI()) {
            for (auto& p : victims) {
                if (p->getId() == self.getId()) continue; // AI 不会主动把自己列为无收益目标
                if (!aiPick || p->getHandCardCount() > aiPick->getHandCardCount()) aiPick = p;
            }
            if (!aiPick) aiPick = victims.front();
        }
        PlayerPtr t = engine.askChoosePlayer(me, victims,
            "【突袭】选择第 " + std::to_string(k + 1) + " 名被夺取手牌的角色（0=结束）", true, aiPick);
        if (!t) break;
        chosen.push_back(t);
        victims.erase(std::remove(victims.begin(), victims.end(), t), victims.end());
    }
    if (chosen.empty()) return; // 放弃发动，仍正常摸牌
    drawCount = 0;
    engine.logMessage("  " + who(self) + " 发动【突袭】放弃摸牌！");
    for (auto& t : chosen) {
        if (!t->isAlive() || t->getHandCardCount() == 0) continue;
        CardPtr c = t->getHandCards()[std::uniform_int_distribution<size_t>(0, t->getHandCardCount()-1)(engine.getRng())];
        engine.obtainCard(me, c, t);
    }
}

// ------------------------------ 裸衣 ------------------------------
LuoYiSkill::LuoYiSkill()
    : TriggerSkill("裸衣", "摸牌阶段，你可以少摸一张牌，然后本回合你使用【杀】或【决斗】造成伤害时，此伤害+1。") {}

void LuoYiSkill::onDrawCards(GameEngine& engine, Player& self, int& drawCount) {
    if (drawCount <= 0) return;
    PlayerPtr me = selfPtr(engine, self);
    bool aiWants = self.getHp() >= 2;
    if (engine.askConfirm(me, "【裸衣】是否少摸一张牌，使本回合【杀】/【决斗】伤害+1？", aiWants)) {
        drawCount -= 1;
        active = true;
        engine.logMessage("  " + who(self) + " 发动【裸衣】，本回合【杀】与【决斗】伤害+1！");
    }
}

void LuoYiSkill::onDealDamage(GameEngine& engine, Player& self, Player& /*target*/, int& damage,
                              ShaElement /*element*/, CardPtr cause) {
    if (!active || !cause) return;
    // 官网：“本回合你使用【杀】或【决斗】造成伤害时”。作为【决斗】目标而造成的伤害不加成。
    bool ownDuel = cause->getSubType() == CardSubType::JUE_DOU &&
                   engine.getActiveDuelUser() && engine.getActiveDuelUser()->getId() == self.getId();
    if (cause->getSubType() == CardSubType::SHA || ownDuel) {
        damage += 1;
        engine.logMessage("  【裸衣】使 " + who(self) + " 造成的伤害+1！");
    }
}

// ------------------------------ 天妒 ------------------------------
TianDuSkill::TianDuSkill()
    : TriggerSkill("天妒", "当你的判定牌生效后，你可以获得此牌。") {}

void TianDuSkill::onAfterJudge(GameEngine& engine, Player& self, Player& judgeTarget, CardPtr judgeCard, bool& claimed) {
    if (&self != &judgeTarget || !judgeCard || claimed) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【天妒】是否获得判定牌 " + judgeCard->getFormattedName() + "？", true)) return;
    self.addHandCard(judgeCard);
    claimed = true;
    engine.logMessage("  " + who(self) + " 发动【天妒】获得了判定牌 " + judgeCard->getFormattedName());
}

// ------------------------------ 遗计 ------------------------------
YiJiSkill::YiJiSkill()
    : TriggerSkill("遗计", "当你受到1点伤害后，你可以观看牌堆顶的两张牌，然后将这些牌交给任意角色。") {}

void YiJiSkill::onAfterDamage(GameEngine& engine, Player& self, Player* /*source*/, int damage,
                              ShaElement /*element*/, CardPtr /*cause*/) {
    if (damage <= 0) return;
    PlayerPtr me = selfPtr(engine, self);
    // 官网“当你受到1点伤害后”按点数分别触发：拒绝其中一点的询问不影响其余各点。
    for (int i = 0; i < damage; ++i) {
        if (!engine.askConfirm(me, "【遗计】是否观看牌堆顶的两张牌并将其交给任意角色？", true)) continue;
        auto drawn = engine.getDeck().drawCards(2);
        if (drawn.empty()) return;
        engine.logMessage("  " + who(self) + " 发动【遗计】，观看牌堆顶的两张牌。");
        // 仅郭嘉本人可见；随后逐张交给任意角色（可以是自己）。
        engine.viewCards(me, drawn, "【遗计】观看牌堆顶的两张牌（仅你可见）");
        for (auto& c : drawn) {
            // B3：默认自己留着，但把“对队友更有用”的牌交出去——
            //   【桃】给受伤的队友；低价值牌给空手的队友（自己不缺这张）。
            PlayerPtr aiKeep;
            if (me->isAI()) {
                aiKeep = me;
                for (const auto& p : engine.getAlivePlayers()) {
                    if (p->getId() == me->getId()) continue;
                    if (!AIController::isFriend(engine, *me, *p)) continue;
                    if (c->getSubType() == CardSubType::TAO && p->isWounded()) { aiKeep = p; break; }
                    if (p->getHandCardCount() == 0 && AIController::cardValue(c) <= 5) { aiKeep = p; break; }
                }
            }
            PlayerPtr to = engine.askChoosePlayer(me, engine.getAlivePlayers(),
                                                  "【遗计】将 " + c->getFormattedName() + " 交给任意角色", true, aiKeep);
            if (!to) to = me;
            engine.obtainCard(to, c, nullptr);
        }
    }
}

// ------------------------------ 倾国 ------------------------------
QingGuoSkill::QingGuoSkill()
    : ActiveSkill("倾国", "你可以将一张黑色手牌当【闪】使用或打出。", 0) {}

bool QingGuoSkill::isConversionSkill() const { return true; }

CardPtr QingGuoSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (wanted != CardSubType::SHAN || !card || card->isVirtual()) return nullptr;
    if (card->getSubType() == CardSubType::SHAN) return nullptr;
    if ((engine.effectiveSuit(self,card)!=Suit::SPADE && engine.effectiveSuit(self,card)!=Suit::CLUB) || !self.hasHandCard(card)) return nullptr; // 限黑色手牌
    return Card::makeVirtual("闪", CardType::BASIC, CardSubType::SHAN, {card}, name);
}

// ------------------------------ 洛神 ------------------------------
LuoShenSkill::LuoShenSkill()
    : TriggerSkill("洛神", "准备阶段，你可以进行判定，当黑色判定牌生效后，你获得之。若结果为黑色，你可以重复此流程。") {}

void LuoShenSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& /*skipPhase*/) {
    if (phase != TurnPhase::PREPARATION) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【洛神】是否发动洛神（判定黑色则获得判定牌并可重复）？", true)) return;
    engine.logMessage("  " + who(self) + " 发动【洛神】！");
    while (self.isAlive()) {
        CardPtr res = engine.doJudgement(me, "洛神", false);
        if (!res) break;
        if (engine.effectiveSuit(self,res)==Suit::SPADE || engine.effectiveSuit(self,res)==Suit::CLUB) {
            self.addHandCard(res);
            engine.logMessage("  判定为黑色，" + who(self) + " 获得 " + res->getFormattedName() + "。");
            if (!engine.askConfirm(me, "【洛神】是否继续判定？", true)) {
                break;
            }
        } else {
            engine.getDeck().discardCard(res);
            engine.logMessage("  判定为红色，【洛神】结束。");
            break;
        }
    }
}

// ============================== 吴 ==============================

// ------------------------------ 制衡 ------------------------------
ZhiHengSkill::ZhiHengSkill()
    : ActiveSkill("制衡", "出牌阶段限一次，你可以弃置任意张牌，然后摸等量的牌。", 1) {}

bool ZhiHengSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self) || !hasUsesLeft()) return false;
    return !self.getHandAndEquipmentCards().empty();
}

void ZhiHengSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> pool = handAndEquip(self);
    std::vector<CardPtr> toDiscard;
    if (me->isAI()) {
        // AI：只换掉低价值弱牌，好牌一张不弃
        for (auto& c : pool) {
            if (isZhiHengFodder(engine, self, c)) toDiscard.push_back(c);
        }
    } else {
        while (!pool.empty()) {
            CardPtr c = engine.askChooseCard(me, pool, "【制衡】选择要弃置的牌（0=结束并摸牌）", true);
            if (!c) break;
            toDiscard.push_back(c);
            pool.erase(std::remove(pool.begin(), pool.end(), c), pool.end());
        }
    }
    if (toDiscard.empty()) return;
    markUsed();
    for (auto& c : toDiscard) {
        engine.discardCardOf(me, c, "制衡");
    }
    engine.drawCards(me, static_cast<int>(toDiscard.size()), "制衡");
}

bool ZhiHengSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    // 至少有一张值得换掉的弱牌才发动
    for (auto& c : self.getHandAndEquipmentCards()) {
        if (isZhiHengFodder(engine, self, c)) return true;
    }
    return false;
}

// ------------------------------ 救援（主公技） ------------------------------
JiuYuanSkill::JiuYuanSkill()
    : StateSkill("救援", "主公技，锁定技，其他吴势力角色对你使用【桃】回复的体力+1。",
                 SkillTag::LOCK | SkillTag::LORD) {}

void JiuYuanSkill::onCalculateRecover(GameEngine& /*engine*/, Player& self, Player* source, int& amount,
                                      const std::string& reason) {
    if (reason != "桃" || !source || source == &self) return;
    if (source->getHero() && source->getHero()->getCountry() == Country::WU) {
        amount += 1;
    }
}

// ------------------------------ 奇袭 ------------------------------
QiXiSkill::QiXiSkill()
    : ActiveSkill("奇袭", "你可以将一张黑色牌当【过河拆桥】使用。", 0) {}

bool QiXiSkill::isConversionSkill() const { return true; }

CardPtr QiXiSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (wanted != CardSubType::GUO_HE_CHAI_QIAO || !card || card->isVirtual()) return nullptr;
    if (card->getSubType() == CardSubType::GUO_HE_CHAI_QIAO) return nullptr;
    if (engine.effectiveSuit(self,card)!=Suit::SPADE && engine.effectiveSuit(self,card)!=Suit::CLUB) return nullptr;
    return Card::makeVirtual("过河拆桥", CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO, {card}, name);
}

// ------------------------------ 克己 ------------------------------
KeJiSkill::KeJiSkill()
    : TriggerSkill("克己", "若你未于出牌阶段内使用或打出过【杀】，则你可以跳过弃牌阶段。") {}

void KeJiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool& skipPhase) {
    if (phase != TurnPhase::DISCARD) return;
    if (self.hasShaPlayedThisTurn()) return;
    PlayerPtr me = selfPtr(engine, self);
    if (engine.askConfirm(me, "【克己】本回合未使用或打出过【杀】，是否跳过弃牌阶段？", true)) {
        skipPhase = true;
        engine.logMessage("  " + who(self) + " 发动【克己】！");
    }
}

// ------------------------------ 苦肉 ------------------------------
KuRouSkill::KuRouSkill()
    : ActiveSkill("苦肉", "出牌阶段，你可以失去1点体力，然后摸两张牌。", 0) {}

void KuRouSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    engine.logMessage("  " + who(self) + " 发动【苦肉】！");
    engine.loseHp(me, 1, "苦肉");
    if (self.isAlive()) {
        engine.drawCards(me, 2, "苦肉");
    }
}

bool KuRouSkill::aiShouldActivate(GameEngine& /*engine*/, Player& self) {
    return self.getHp() >= 3;
}

// ------------------------------ 英姿 ------------------------------
YingZiSkill::YingZiSkill()
    : TriggerSkill("英姿", "摸牌阶段，你可以多摸一张牌。") {}

void YingZiSkill::onDrawCards(GameEngine& engine, Player& self, int& drawCount) {
    if (!engine.askConfirm(selfPtr(engine,self),"【英姿】是否额外摸一张牌？",true)) return;
    drawCount += 1;
    engine.logMessage("  " + who(self) + " 发动【英姿】，摸牌数+1！");
}

// ------------------------------ 反间 ------------------------------
FanJianSkill::FanJianSkill()
    : ActiveSkill("反间", "出牌阶段限一次，你可以令一名其他角色选择一种花色，然后该角色获得你的一张手牌并展示之，若此牌的花色与其所选的花色不同，则你对其造成1点伤害。", 1) {}

bool FanJianSkill::canActivate(GameEngine& engine, Player& self) {
    if (!hasUsesLeft()) return false;
    return self.getHandCardCount() > 0 && !engine.getOtherAlivePlayers(self).empty();
}

void FanJianSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    if (!hasUsesLeft()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr aiTarget = nullptr;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (!AIController::isFriend(engine, self, *p)) { aiTarget = p; break; }
    }
    PlayerPtr target = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self),
                                              "【反间】选择一名其他角色", true, aiTarget);
    if (!target) return;
    markUsed();

    // 目标选择花色
    static const Suit kSuits[] = {Suit::SPADE, Suit::HEART, Suit::CLUB, Suit::DIAMOND};
    Suit guessed;
    if (target->isAI()) {
        guessed = kSuits[std::uniform_int_distribution<int>(0, 3)(engine.getRng())];
    } else {
        int opt = engine.askChooseOption(target, {"黑桃", "红桃", "梅花", "方块"}, "【反间】请选择一种花色");
        guessed = kSuits[std::max(0, std::min(3, opt))];
    }

    // 手牌仍属周瑜的隐藏区域：只能选牌背序号，展示发生在获得之后。
    CardPtr shown = engine.chooseHiddenHandCard(target, me, "【反间】选择获得并展示的一张隐藏手牌");
    if (!shown) return;
    engine.obtainCard(target, shown, me);
    engine.logMessage("  " + who(*target) + " 选择花色后展示获得的牌: " + shown->getFormattedName());

    if (engine.effectiveSuit(*target,shown) != guessed) {
        engine.logMessage("  花色不符！" + who(self) + " 对 " + who(*target) + " 造成1点伤害！");
        engine.applyDamage(me, target, 1);
    } else {
        engine.logMessage("  花色相符，" + who(*target) + " 免于伤害。");
    }
}

bool FanJianSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

// ============================== 吴（大乔/孙尚香） ==============================

// ------------------------------ 国色 ------------------------------
GuoSeSkill::GuoSeSkill()
    : ActiveSkill("国色", "你可以将一张方块牌当【乐不思蜀】使用。", 0) {}

bool GuoSeSkill::isConversionSkill() const { return true; }

CardPtr GuoSeSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (wanted != CardSubType::LE_BU_SI_SHU || !card || card->isVirtual()) return nullptr;
    if (card->getSubType() == CardSubType::LE_BU_SI_SHU) return nullptr;
    if (engine.effectiveSuit(self, card) != Suit::DIAMOND) return nullptr;
    return Card::makeVirtual("乐不思蜀", CardType::TRICK, CardSubType::LE_BU_SI_SHU, {card}, name);
}

// ------------------------------ 流离 ------------------------------
LiuLiSkill::LiuLiSkill()
    : TriggerSkill("流离", "当你成为【杀】的目标时，你可以弃置一张牌并选择你攻击范围内的一名其他角色，然后将此【杀】转移给该角色。") {}

void LiuLiSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (&self != ctx.target.get() || !ctx.card) { return; }
    if (ctx.redirect) return; // 已重定过（防环）
    auto pool = handAndEquip(self);
    if (pool.empty()) { return; }

    // 可转移目标：自己攻击范围内的其他角色（排除使用者）
    std::vector<PlayerPtr> candidates;
    auto reachable = engine.getShaTargets(self, ctx.card);
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (ctx.source && p->getId() == ctx.source->getId()) continue;
        bool inRange = false;
        for (auto& q : reachable) {
            if (q->getId() == p->getId()) { inRange = true; break; }
        }
        if (inRange) candidates.push_back(p);
    }
    if (candidates.empty()) return;

    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr aiPick = nullptr;
    for (auto& p : candidates) {
        if (!AIController::isFriend(engine, self, *p)) { aiPick = p; break; }
    }
    bool aiWants = aiPick && ctx.source && !AIController::isFriend(engine, self, *ctx.source);
    if (!engine.askConfirm(me, "【流离】是否弃置一张牌将此【杀】转移给攻击范围内的一名其他角色？", aiWants)) { return; }

    PlayerPtr target = engine.askChoosePlayer(me, candidates, "【流离】选择【杀】转移的目标", true, aiPick);
    if (!target) return;

    CardPtr cost = engine.askChooseCard(me, pool, "【流离】弃置一张牌", true,
                                        AIController::chooseLeastValuableCard(pool));
    if (!cost) cost = pool.front();
    engine.discardCardOf(me, cost, "流离");
    ctx.redirect = target;
}

// ------------------------------ 结姻 ------------------------------
JieYinSkill::JieYinSkill()
    : ActiveSkill("结姻", "出牌阶段限一次，你可以弃置两张手牌并选择一名已受伤的男性角色，然后你与其各回复1点体力。", 1) {}

bool JieYinSkill::canActivate(GameEngine& engine, Player& self) {
    if (!hasUsesLeft()) return false;
    if (self.getHandCardCount() < 2) return false;
    for (auto& p : engine.getAlivePlayers()) {
        if (p->isAlive() && isMale(*p) && p->isWounded()) return true;
    }
    return false;
}

void JieYinSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    if (!hasUsesLeft()) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<PlayerPtr> candidates;
    for (auto& p : engine.getAlivePlayers()) {
        if (p->isAlive() && isMale(*p) && p->isWounded()) candidates.push_back(p);
    }
    PlayerPtr aiTarget = nullptr;
    // 简单 AI 选择：最受伤的朋友优先
    for (auto& p : candidates) {
        if (AIController::isFriend(engine, self, *p)) {
            if (!aiTarget || p->getHp() < aiTarget->getHp()) aiTarget = p;
        }
    }
    PlayerPtr target = engine.askChoosePlayer(me, candidates, "【结姻】选择一名已受伤的男性角色", true, aiTarget);
    if (!target) return;

    std::vector<CardPtr> hand = self.getHandCards();
    std::vector<CardPtr> costs;
    for (int i = 0; i < 2 && !hand.empty(); ++i) {
        CardPtr c = engine.askChooseCard(me, hand, "【结姻】弃置第 " + std::to_string(i + 1) + " 张手牌", false,
                                         AIController::chooseLeastValuableCard(hand));
        if (!c) c = hand.front();
        costs.push_back(c);
        hand.erase(std::remove(hand.begin(), hand.end(), c), hand.end());
    }
    if (costs.size() < 2) return;
    markUsed();
    for (auto& c : costs) engine.discardCardOf(me, c, "结姻");
    engine.logMessage("  " + who(self) + " 发动【结姻】！");
    engine.recoverHp(me, 1, "结姻");
    engine.recoverHp(target, 1, "结姻");
}

bool JieYinSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

// ------------------------------ 枭姬 ------------------------------
XiaoJiSkill::XiaoJiSkill()
    : TriggerSkill("枭姬", "当你失去装备区里的一张牌时，你可以摸两张牌。") {}

void XiaoJiSkill::onEquipmentLost(GameEngine& engine, Player& self, CardPtr /*card*/) {
    PlayerPtr me = selfPtr(engine, self);
    if (engine.askConfirm(me, "【枭姬】失去装备区的牌，是否摸两张牌？", true)) {
        engine.logMessage("  " + who(self) + " 发动【枭姬】！");
        engine.drawCards(me, 2, "枭姬");
    }
}

// ============================== 群 ==============================

// ------------------------------ 急救 ------------------------------
JiJiuSkill::JiJiuSkill()
    : ActiveSkill("急救", "你的回合外，你可以将一张红色牌当【桃】使用。", 0) {}

bool JiJiuSkill::isConversionSkill() const { return true; }

CardPtr JiJiuSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (wanted != CardSubType::TAO || !card || card->isVirtual()) return nullptr;
    if (card->getSubType() == CardSubType::TAO) return nullptr;
    if (engine.isPlayerTurn(self)) return nullptr; // 仅回合外
    if (engine.effectiveSuit(self,card)!=Suit::HEART && engine.effectiveSuit(self,card)!=Suit::DIAMOND) return nullptr;
    return Card::makeVirtual("桃", CardType::BASIC, CardSubType::TAO, {card}, name);
}

// ------------------------------ 青囊 ------------------------------
QingNangSkill::QingNangSkill()
    : ActiveSkill("青囊", "出牌阶段限一次，你可以弃置一张手牌，然后令一名已受伤的角色回复1点体力。", 1) {}

bool QingNangSkill::canActivate(GameEngine& engine, Player& self) {
    if (!hasUsesLeft() || engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getHandCardCount() == 0) return false;
    if (self.isWounded()) return true;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (p->isWounded()) return true;
    }
    return false;
}

void QingNangSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    if (!hasUsesLeft()) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<PlayerPtr> candidates;
    if (self.isWounded()) candidates.push_back(me);
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (p->isWounded()) candidates.push_back(p);
    }
    PlayerPtr aiTarget = nullptr;
    for (auto& p : candidates) {
        if (AIController::isFriend(engine, self, *p) || p.get() == &self) {
            if (!aiTarget || p->getHp() < aiTarget->getHp()) aiTarget = p;
        }
    }
    PlayerPtr target = engine.askChoosePlayer(me, candidates, "【青囊】选择一名已受伤的角色", true, aiTarget);
    if (!target) return;
    CardPtr cost = engine.askChooseCard(me, self.getHandCards(), "【青囊】弃置一张手牌", false,
                                        AIController::chooseLeastValuableCard(self.getHandCards()));
    if (!cost) cost = self.getHandCards().front();
    markUsed();
    engine.discardCardOf(me, cost, "青囊");
    engine.logMessage("  " + who(self) + " 发动【青囊】！");
    engine.recoverHp(target, 1, "青囊");
}

bool QingNangSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

// ------------------------------ 无双 ------------------------------
WuShuangSkill::WuShuangSkill()
    : StateSkill("无双", "锁定技，你使用的【杀】需两张【闪】才能抵消；与你进行【决斗】的角色每次需打出两张【杀】。", SkillTag::LOCK) {}

void WuShuangSkill::onCalculateResponseCount(GameEngine& /*engine*/, const Player& self, const Player& responder,
                                              CardSubType wanted, int& count) {
    if (&responder == &self) return;
    if (wanted == CardSubType::SHAN || wanted == CardSubType::SHA) {
        count = 2;
    }
}

// ------------------------------ 离间 ------------------------------
LiJianSkill::LiJianSkill()
    : ActiveSkill("离间", "出牌阶段限一次，你可以弃置一张牌并选择两名其他男性角色，然后令其中一名男性角色视为对另一名男性角色使用一张【决斗】。", 1) {}

bool LiJianSkill::canActivate(GameEngine& engine, Player& self) {
    if (!hasUsesLeft() || engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getHandCardCount() + static_cast<int>(self.getAllEquipment().size()) == 0) return false;
    int males = 0;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (isMale(*p)) ++males;
    }
    return males >= 2;
}

void LiJianSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    if (!hasUsesLeft()) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<PlayerPtr> males;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (isMale(*p)) males.push_back(p);
    }
    // 官网：令其中一名男性角色“视为”对另一名使用【决斗】。视为使用的牌没有实体子牌，
    // 弃置的代价牌已进入弃牌堆，不能被【奸雄】等技能当作决斗牌获得。
    PlayerPtr a = engine.askChoosePlayer(me, males, "【离间】选择视为使用【决斗】的男性角色", true);
    if (!a) return;
    auto duel = Card::makeVirtual("决斗", CardType::TRICK, CardSubType::JUE_DOU, {}, "离间");
    std::vector<PlayerPtr> defenders;
    for (auto& p : males)
        if (p != a && engine.canBeTargeted(p, duel, a)) defenders.push_back(p);
    PlayerPtr b = engine.askChoosePlayer(me, defenders, "【离间】选择成为【决斗】目标的男性角色", true);
    if (!b) return;

    // 弃置一张牌
    auto pool = handAndEquip(self);
    CardPtr cost = engine.askChooseCard(me, pool, "【离间】弃置一张牌", true,
                                        AIController::chooseLeastValuableCard(pool));
    if (!cost) return;
    markUsed();
    engine.discardCardOf(me, cost, "离间");
    engine.logMessage("  " + who(self) + " 发动【离间】！" + who(*a) + " 视为对 " + who(*b) + " 使用【决斗】");
    // 走完整的使用流程：目标合法性、无懈可击、激昂等“成为决斗目标”的时机均正常结算。
    engine.useCard(a, duel, {b});
}

bool LiJianSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

// ------------------------------ 闭月 ------------------------------
BiYueSkill::BiYueSkill()
    : TriggerSkill("闭月", "结束阶段，你可以摸一张牌。") {}

void BiYueSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH || !engine.isPlayerTurn(self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (engine.askConfirm(me, "【闭月】是否摸一张牌？", true)) {
        engine.logMessage("  " + who(self) + " 发动【闭月】！");
        engine.drawCards(me, 1, "闭月");
    }
}

// ------------------------------ 谦逊 ------------------------------
QianXunSkill::QianXunSkill()
    : StateSkill("谦逊", "锁定技，你不能被选择为【顺手牵羊】和【乐不思蜀】的目标。", SkillTag::LOCK) {}

void QianXunSkill::onCheckCardTarget(GameEngine& /*engine*/, const Player& /*self*/, const Player& /*target*/,
                                     CardPtr card, bool& canTarget) {
    if (!canTarget || !card) return;
    if (card->getSubType() == CardSubType::SHUN_SHOU_QIAN_YANG || card->getSubType() == CardSubType::LE_BU_SI_SHU) {
        canTarget = false;
    }
}

// ------------------------------ 连营 ------------------------------
LianYingSkill::LianYingSkill()
    : TriggerSkill("连营", "当你失去最后的手牌时，你可以摸一张牌。") {}

void LianYingSkill::onHandEmpty(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self);
    if (engine.askConfirm(me, "【连营】失去了最后一张手牌，是否摸一张牌？", true)) {
        engine.logMessage("  " + who(self) + " 发动【连营】！");
        engine.drawCards(me, 1, "连营");
    }
}

} // namespace Thks
