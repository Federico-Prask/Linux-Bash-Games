#include "Player.h"
#include <algorithm>
#include <sstream>

namespace Thks {

Player::Player(int id, const std::string& name, Identity identity, bool isAi, HeroPtr hero)
    : id(id), name(name), identity(identity), isAiPlayer(isAi), hero(hero),
      hp(4), maxHp(4), alive(true), turnedOver(false), chained(false), drunk(false), shaCountThisTurn(0),
      nonLockSkillsDisabled(false), handCardsBanned(false), yiJueMarkSourceId(-1), tiShenActive(false) {
    if (hero) {
        maxHp = hero->getMaxHp();
        if (identity == Identity::ZHU_GONG || identity == Identity::DI_ZHU) {
            maxHp += 1; // 主公/地主 +1 体力上限
        }
        hp = maxHp;
    }
}

std::string Player::getIdentityString() const {
    switch (identity) {
        case Identity::ZHU_GONG:   return "主公";
        case Identity::ZHONG_CHEN: return "忠臣";
        case Identity::FAN_ZEI:    return "反贼";
        case Identity::NEI_JIAN:   return "内奸";
        case Identity::DI_ZHU:     return "地主";
        case Identity::NONG_MIN:   return "农民";
        case Identity::YE_XIN_JIA: return "野心家";
        default:                   return "未知";
    }
}

void Player::setHero(HeroPtr newHero) {
    hero = newHero;
    if (hero) {
        maxHp = hero->getMaxHp();
        if (identity == Identity::ZHU_GONG || identity == Identity::DI_ZHU) {
            maxHp += 1; // 主公/地主 +1 体力上限
        }
        hp = maxHp;
    }
}

void Player::changeHp(int delta) {
    hp += delta;
    if (hp > maxHp) hp = maxHp;
}

void Player::changeMaxHp(int delta) {
    maxHp += delta;
    if (maxHp < 0) maxHp = 0;
    if (hp > maxHp) hp = maxHp;
}

// ==================== 武将牌上的牌 ====================

const std::vector<CardPtr>& Player::getPile(const std::string& pileName) const {
    static const std::vector<CardPtr> empty;
    auto it = piles.find(pileName);
    return it == piles.end() ? empty : it->second;
}

int Player::getPileCount(const std::string& pileName) const {
    return static_cast<int>(getPile(pileName).size());
}

void Player::addToPile(const std::string& pileName, CardPtr card) {
    if (card) piles[pileName].push_back(card);
}

bool Player::removeFromPile(const std::string& pileName, CardPtr card) {
    auto it = piles.find(pileName);
    if (it == piles.end()) return false;
    auto& vec = it->second;
    auto pos = std::find(vec.begin(), vec.end(), card);
    if (pos == vec.end()) return false;
    vec.erase(pos);
    return true;
}

std::vector<CardPtr> Player::takeAllPiles() {
    std::vector<CardPtr> all;
    for (auto& kv : piles) {
        all.insert(all.end(), kv.second.begin(), kv.second.end());
    }
    piles.clear();
    return all;
}

void Player::clearTurnEffects() {
    nonLockSkillsDisabled = false;
    handCardsBanned = false;
    yiJueMarkSourceId = -1;
    chenglueUnlimitedSuitMask = 0;
}

bool Player::hasHandCard(const CardPtr& card) const {
    return std::find(handCards.begin(), handCards.end(), card) != handCards.end();
}

bool Player::hasEquipment(const CardPtr& card) const {
    return card && (weapon == card || armor == card || offensiveHorse == card || defensiveHorse == card);
}

bool Player::hasCardInAnyPile(const CardPtr& card) const {
    if (!card) return false;
    for (const auto& [_, pile] : piles)
        if (std::find(pile.begin(), pile.end(), card) != pile.end()) return true;
    return false;
}

bool Player::removeCardFromAnyPile(const CardPtr& card) {
    if (!card) return false;
    for (auto& [_, pile] : piles) {
        auto it = std::find(pile.begin(), pile.end(), card);
        if (it == pile.end()) continue;
        pile.erase(it);
        return true;
    }
    return false;
}

void Player::addHandCard(CardPtr card) {
    if (card) handCards.push_back(card);
}

void Player::addHandCards(const std::vector<CardPtr>& cards) {
    for (const auto& card : cards) {
        addHandCard(card);
    }
}

bool Player::removeHandCard(CardPtr card) {
    auto it = std::find(handCards.begin(), handCards.end(), card);
    if (it != handCards.end()) {
        handCards.erase(it);
        return true;
    }
    return false;
}

CardPtr Player::removeHandCardByIndex(int index) {
    if (index >= 0 && index < static_cast<int>(handCards.size())) {
        CardPtr card = handCards[index];
        handCards.erase(handCards.begin() + index);
        return card;
    }
    return nullptr;
}

CardPtr Player::equip(CardPtr card) {
    if (!card || card->getType() != CardType::EQUIPMENT) return nullptr;

    CardPtr oldEquip = nullptr;

    if (card->getSubType() == CardSubType::WEAPON) {
        oldEquip = weapon;
        weapon = card;
    } else if (card->getSubType() == CardSubType::ARMOR) {
        oldEquip = armor;
        armor = card;
    } else if (card->getSubType() == CardSubType::OFFENSIVE_HORSE) {
        oldEquip = offensiveHorse;
        offensiveHorse = card;
    } else if (card->getSubType() == CardSubType::DEFENSIVE_HORSE) {
        oldEquip = defensiveHorse;
        defensiveHorse = card;
    }

    return oldEquip;
}

CardPtr Player::removeEquipment(CardSubType type) {
    CardPtr removed = nullptr;
    if (type == CardSubType::WEAPON) {
        removed = weapon;
        weapon = nullptr;
    } else if (type == CardSubType::ARMOR) {
        removed = armor;
        armor = nullptr;
    } else if (type == CardSubType::OFFENSIVE_HORSE) {
        removed = offensiveHorse;
        offensiveHorse = nullptr;
    } else if (type == CardSubType::DEFENSIVE_HORSE) {
        removed = defensiveHorse;
        defensiveHorse = nullptr;
    }
    return removed;
}

std::vector<CardPtr> Player::getAllEquipment() const {
    std::vector<CardPtr> result;
    if (weapon) result.push_back(weapon);
    if (armor) result.push_back(armor);
    if (offensiveHorse) result.push_back(offensiveHorse);
    if (defensiveHorse) result.push_back(defensiveHorse);
    return result;
}

bool Player::removeJudgeCard(CardPtr card) {
    auto it = std::find(judgeZone.begin(), judgeZone.end(), card);
    if (it != judgeZone.end()) {
        judgeZone.erase(it);
        return true;
    }
    return false;
}

int Player::getAttackRange() const {
    if (weapon) {
        return weapon->getAttackRange();
    }
    return 1;
}

int Player::getHandLimit() const {
    return std::max(0, hp);
}

std::vector<CardPtr> Player::getHandAndEquipmentCards() const {
    std::vector<CardPtr> cards = handCards;
    auto equipment = getAllEquipment();
    cards.insert(cards.end(), equipment.begin(), equipment.end());
    return cards;
}

std::vector<CardPtr> Player::getAllCards() const {
    auto all = getHandAndEquipmentCards();
    all.insert(all.end(), judgeZone.begin(), judgeZone.end());
    return all;
}

std::string Player::getFormattedStatus(bool showIdentity) const {
    std::ostringstream ss;
    ss << "[" << id << "] " << name << " ";
    if (hero) {
        ss << "<" << hero->getName() << " - " << hero->getCountryString() << "> ";
    }
    if (showIdentity) {
        ss << "(" << getIdentityString() << ") ";
    }
    ss << "HP: " << hp << "/" << maxHp << " | 手牌: " << handCards.size() << "张";
    for (const auto& kv : piles) {
        if (!kv.second.empty()) ss << " [" << kv.first << ":" << kv.second.size() << "]";
    }
    if (turnedOver) ss << " [翻面]";
    if (chained) ss << " [横置]";
    if (drunk) ss << " [醉酒]";
    if (nonLockSkillsDisabled) ss << " [技能失效]";
    if (handCardsBanned) ss << " [禁用手牌]";
    if (tiShenActive) ss << " [替身]";
    if (!alive) ss << " [阵亡]";
    return ss.str();
}

std::string Player::getFormattedHandCards() const {
    if (handCards.empty()) return "（空手牌）";
    std::ostringstream ss;
    for (size_t i = 0; i < handCards.size(); ++i) {
        ss << "(" << (i + 1) << ")" << handCards[i]->getFormattedName() << " ";
    }
    return ss.str();
}

std::vector<std::string> Player::getPileNames() const {
    std::vector<std::string> names;
    for (const auto& kv : piles) if (!kv.second.empty()) names.push_back(kv.first);
    return names;
}

std::string Player::getFormattedEquipment() const {
    std::ostringstream ss;
    bool hasEquip = false;
    if (weapon) { ss << "[武器:" << weapon->getName() << "] "; hasEquip = true; }
    if (armor)  { ss << "[防具:" << armor->getName() << "] "; hasEquip = true; }
    if (offensiveHorse) { ss << "[-1马:" << offensiveHorse->getName() << "] "; hasEquip = true; }
    if (defensiveHorse) { ss << "[+1马:" << defensiveHorse->getName() << "] "; hasEquip = true; }
    if (!hasEquip) return "（无装备）";
    return ss.str();
}

} // namespace Thks
