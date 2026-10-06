// =====================================================================
//  装备牌效果（武器 / 防具）
//
//  装备不属于武将技能，因此不走 Skill 体系，而是由引擎在固定时机直接结算：
//    - 攻击范围 / 次数：Player::getAttackRange、GameEngine::getShaLimit（诸葛连弩）
//    - 【杀】结算：GameEngine::handleSha 中依次调用
//        青釭剑（无视防具）→ 仁王盾 / 藤甲（使【杀】无效）→ 八卦阵 / 【闪】
//        → weaponAfterShaDodged（贯石斧）→ weaponBeforeShaDamage（麒麟弓 / 寒冰剑）
//        → weaponQingLongChase（青龙偃月刀）
//    - 伤害：GameEngine::applyDamage（藤甲火焰 +1、白银狮子减伤）
//    - 转化：getResponseCandidates / humanPlayLoop / AI（丈八蛇矛）
//    - 目标数：canUseFangTianExtraTargets（方天画戟）
//    - 失去装备：afterEquipmentLost（白银狮子回血）
// =====================================================================
#include "GameEngine.h"
#include "AI.h"
#include <algorithm>

namespace Thks {

namespace {

std::string who(const Player& p) {
    return "[" + p.getName() + "]";
}

std::vector<CardPtr> without(const std::vector<CardPtr>& cards, const std::vector<CardPtr>& removed) {
    std::vector<CardPtr> result;
    for (const auto& c : cards) {
        if (std::find(removed.begin(), removed.end(), c) == removed.end()) result.push_back(c);
    }
    return result;
}

} // namespace

bool GameEngine::hasWeapon(const Player& p, const std::string& name) {
    return p.getWeapon() && p.getWeapon()->getName() == name;
}

bool GameEngine::hasArmor(const Player& p, const std::string& name) {
    return p.getMark("无前防具失效")==0 && p.getArmor() && p.getArmor()->getName() == name;
}

// ------------------------------ 装备离开装备区后（白银狮子 / 枭姬等） ------------------------------
void GameEngine::afterEquipmentLost(PlayerPtr owner, CardPtr card) {
    if (!owner || !card || !owner->isAlive()) return;
    if (owner->getMark("无前防具失效")==0 &&
        card->getSubType() == CardSubType::ARMOR && card->getName() == "白银狮子") {
        if (owner->isWounded()) {
            logMessage("  【白银狮子】离开装备区，" + who(*owner) + " 回复 1 点体力。");
            recoverHp(owner, 1, "白银狮子");
        }
    }
    for (const auto& s : getEffectiveSkills(*owner)) {
        s->onEquipmentLost(*this, *owner, card);
    }
}

// ------------------------------ 丈八蛇矛 ------------------------------
bool GameEngine::isZhangBaPlaceholder(const CardPtr& card) {
    return card && card->isVirtual() && card->getSkillSource() == "丈八蛇矛" && card->getSubCards().empty();
}

CardPtr GameEngine::materializeZhangBaSha(PlayerPtr player, CardPtr placeholder) {
    if (!isZhangBaPlaceholder(placeholder)) return placeholder;
    if (!player || player->getHandCardCount() < 2 || player->isHandCardsBanned()) return nullptr;

    std::vector<CardPtr> picks;
    if (player->isAI()) {
        picks = AIController::pickZhangBaCards(*player);
        if (picks.size() < 2) return nullptr;
    } else {
        for (int i = 0; i < 2; ++i) {
            auto candidates = without(player->getHandCards(), picks);
            CardPtr c = askChooseCard(player, candidates,
                                      "【丈八蛇矛】请选择第 " + std::to_string(i + 1) + "/2 张要当作【杀】的手牌", false);
            if (!c) return nullptr;
            picks.push_back(c);
        }
    }
    return Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, picks, "丈八蛇矛");
}

// ------------------------------ 方天画戟 ------------------------------
bool GameEngine::canUseFangTianExtraTargets(const Player& source, CardPtr sha) const {
    if (!sha || !hasWeapon(source, "方天画戟")) return false;
    // 此【杀】（含转化牌的实体牌）必须恰好是你全部的手牌
    int fromHand = 0;
    for (const auto& real : sha->getRealCards(sha)) {
        if (source.hasHandCard(real)) fromHand++;
    }
    return fromHand > 0 && fromHand == source.getHandCardCount();
}

// ------------------------------ 借刀杀人 ------------------------------
std::vector<PlayerPtr> GameEngine::getJieDaoVictims(const Player& holder) const {
    std::vector<PlayerPtr> result;
    if (!holder.isAlive() || !holder.getWeapon()) return result;
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "借刀杀人");
    PlayerPtr source = getPlayerById(holder.getId());
    for (const auto& p : getOtherAlivePlayers(holder)) {
        if (!canUseShaOn(holder, *p, sha) ||
            !const_cast<GameEngine*>(this)->canBeTargeted(p, sha, source)) continue;
        result.push_back(p);
    }
    return result;
}

std::vector<PlayerPtr> GameEngine::getJieDaoWeaponHolders(const Player& user) const {
    std::vector<PlayerPtr> result;
    for (const auto& p : getOtherAlivePlayers(user)) {
        if (p->getWeapon() && !getJieDaoVictims(*p).empty()) result.push_back(p);
    }
    return result;
}

// ------------------------------ 贯石斧 ------------------------------
bool GameEngine::weaponAfterShaDodged(ShaContext& ctx) {
    PlayerPtr source = ctx.source;
    PlayerPtr target = ctx.target;
    if (!source || !target || !source->isAlive() || !target->isAlive()) return false;
    if (!hasWeapon(*source, "贯石斧")) return false;

    // 可弃置的牌：手牌 + 装备区（不含贯石斧本身）
    std::vector<CardPtr> pool = source->getHandCards();
    for (const auto& e : source->getAllEquipment()) {
        if (e != source->getWeapon()) pool.push_back(e);
    }
    if (pool.size() < 2) return false;

    bool aiWants = pool.size() >= 4 || target->getHp() <= 1;
    if (!askConfirm(source, "【杀】被抵消，是否发动【贯石斧】弃置两张牌令此【杀】依然造成伤害？", aiWants)) return false;

    std::vector<CardPtr> picks;
    for (int i = 0; i < 2; ++i) {
        auto candidates = without(pool, picks);
        CardPtr c = askChooseCard(source, candidates, "【贯石斧】请选择第 " + std::to_string(i + 1) + "/2 张要弃置的牌", false,
                                  AIController::chooseLeastValuableCard(candidates));
        if (!c) return false;
        picks.push_back(c);
    }
    logMessage("  " + who(*source) + " 发动【贯石斧】！");
    for (const auto& c : picks) discardCardOf(source, c, "贯石斧");
    logMessage("  【贯石斧】效果：此【杀】依然对 " + who(*target) + " 造成伤害！");
    return true;
}

// ------------------------------ 麒麟弓 / 寒冰剑 ------------------------------
bool GameEngine::weaponBeforeShaDamage(ShaContext& ctx) {
    PlayerPtr source = ctx.source;
    PlayerPtr target = ctx.target;
    if (!source || !target || !source->isAlive() || !target->isAlive()) return false;

    if (hasWeapon(*source, "麒麟弓")) {
        std::vector<CardPtr> horses;
        if (target->getOffensiveHorse()) horses.push_back(target->getOffensiveHorse());
        if (target->getDefensiveHorse()) horses.push_back(target->getDefensiveHorse());
        if (!horses.empty() && askConfirm(source, "是否发动【麒麟弓】弃置 " + who(*target) + " 装备区的一张坐骑牌？", true)) {
            // AI 优先拆 +1 马
            CardPtr aiPick = target->getDefensiveHorse() ? target->getDefensiveHorse() : horses.front();
            CardPtr horse = horses.size() == 1 ? horses.front()
                                               : askChooseCard(source, horses, "【麒麟弓】选择要弃置的坐骑牌", false, aiPick);
            if (horse) {
                logMessage("  " + who(*source) + " 发动【麒麟弓】！");
                discardCardOf(target, horse, "麒麟弓");
            }
        }
        return false;
    }

    if (hasWeapon(*source, "寒冰剑")) {
        auto pool = target->getAllCards();
        if (pool.empty()) return false;
        // AI：目标牌多且不是一击可杀时，拆牌比伤害更划算
        bool aiWants = pool.size() >= 2 && target->getHp() > 1;
        if (!askConfirm(source, "是否发动【寒冰剑】防止此伤害，改为依次弃置 " + who(*target) + " 的两张牌？", aiWants)) return false;
        logMessage("  " + who(*source) + " 发动【寒冰剑】，防止了此【杀】的伤害！");
        for (int i = 0; i < 2; ++i) {
            auto candidates = target->getAllCards();
            if (candidates.empty()) break;
            CardPtr c = chooseCardFromPlayer(source,target,"【寒冰剑】选择要弃置的 " + who(*target) + " 的第 " + std::to_string(i + 1) + " 张牌");
            if (!c) break;
            discardCardOf(target, c, "寒冰剑");
        }
        return true;
    }
    return false;
}

// ------------------------------ 青龙偃月刀 ------------------------------
void GameEngine::weaponQingLongChase(ShaContext& ctx) {
    PlayerPtr source = ctx.source;
    PlayerPtr target = ctx.target;
    if (!source || !target || !source->isAlive() || !target->isAlive() || gameOver) return;
    if (!hasWeapon(*source, "青龙偃月刀")) return;

    // 官网牌面只说“再使用一张【杀】”，距离等使用规则照常结算，不视为无距离限制。
    CardPtr sha = askUseSha(source, "【杀】被抵消，是否发动【青龙偃月刀】对 " + who(*target) + " 再使用一张【杀】？", true);
    if (!sha) return;
    logMessage("  " + who(*source) + " 发动【青龙偃月刀】，对 " + who(*target) + " 再使用 " + sha->getFormattedName());
    resolveSha(source, sha, {target});
}

} // namespace Thks
