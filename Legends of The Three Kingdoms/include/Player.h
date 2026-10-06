#ifndef THKS_PLAYER_H
#define THKS_PLAYER_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <algorithm>
#include "Card.h"
#include "Hero.h"

namespace Thks {

enum class Identity {
    ZHU_GONG,  // 主公
    ZHONG_CHEN,// 忠臣
    FAN_ZEI,   // 反贼
    NEI_JIAN,  // 内奸
    DI_ZHU,    // 地主（斗地主）
    NONG_MIN,  // 农民（斗地主）
    YE_XIN_JIA // 野心家（身份场：内奸【择途】“自立为主”后的独立身份）
};

class Player {
private:
    int id;
    std::string name;
    Identity identity;
    bool isAiPlayer;
    HeroPtr hero;

    int hp;
    int maxHp;

    std::vector<CardPtr> handCards;

    CardPtr weapon;
    CardPtr armor;
    CardPtr offensiveHorse;
    CardPtr defensiveHorse;

    std::vector<CardPtr> judgeZone;

    bool alive;
    bool turnedOver;
    bool chained;
    bool drunk;
    int shaCountThisTurn;
    bool shaPlayedThisTurn = false;
    unsigned chenglueUnlimitedSuitMask = 0; // 许攸【成略】本回合获得的无距离/次数限制花色
    std::set<CardType> cardTypesUsedThisTurn; // 当前回合各角色已使用的牌类型（供【恃才】等判定）

    // 武将牌上的牌（如钟会的"权"），按名称分堆
    std::map<std::string, std::vector<CardPtr>> piles;
    std::map<std::string, int> marks; // 非实体牌标记（忍/暴怒等）

    // ---- 回合内临时状态（每回合结束时清除）----
    bool nonLockSkillsDisabled;  // 非锁定技失效（界义绝/界铁骑）
    bool handCardsBanned;        // 不能使用或打出手牌（界义绝）
    int yiJueMarkSourceId;       // 被谁的【义绝】命中（红桃杀伤害+1），-1 表示无

    // ---- 跨回合状态 ----
    bool tiShenActive;           // 【替身】生效中：直到下回合开始，获得未造成伤害的【杀】

public:
    Player(int id, const std::string& name, Identity identity, bool isAi = true, HeroPtr hero = nullptr);

    int getId() const { return id; }
    std::string getName() const { return name; }
    Identity getIdentity() const { return identity; }
    // 身份变更：内奸【择途】（侍奉明君→忠臣 / 自立为主→野心家）、储君继位→主公。
    void setIdentity(Identity newIdentity) { identity = newIdentity; }
    std::string getIdentityString() const;
    bool isAI() const { return isAiPlayer; }

    HeroPtr getHero() const { return hero; }
    void setHero(HeroPtr newHero);

    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    void setHp(int value) { hp = std::min(value, maxHp); }
    void changeHp(int delta);
    void changeMaxHp(int delta);
    bool isWounded() const { return hp < maxHp; }

    bool isAlive() const { return alive; }
    void setAlive(bool state) { alive = state; }

    bool isTurnedOver() const { return turnedOver; }
    void setTurnedOver(bool state) { turnedOver = state; }

    bool isChained() const { return chained; }
    void setChained(bool state) { chained = state; }

    bool isDrunk() const { return drunk; }
    void setDrunk(bool state) { drunk = state; }

    int getShaCountThisTurn() const { return shaCountThisTurn; }
    void resetShaCount() { shaCountThisTurn = 0; shaPlayedThisTurn = false; }
    void incrementShaCount() { shaCountThisTurn++; shaPlayedThisTurn = true; }
    // 本回合是否使用或打出过【杀】（含响应时打出）
    bool hasShaPlayedThisTurn() const { return shaPlayedThisTurn; }
    void markShaPlayed() { shaPlayedThisTurn = true; }

    void addChenglueUnlimitedSuit(Suit suit) {
        const int index = static_cast<int>(suit);
        if (index >= 0 && index < 4) chenglueUnlimitedSuitMask |= (1u << index);
    }
    bool hasChenglueUnlimitedSuit(Suit suit) const {
        const int index = static_cast<int>(suit);
        return index >= 0 && index < 4 && (chenglueUnlimitedSuitMask & (1u << index)) != 0;
    }
    bool hasAnyChenglueUnlimitedSuit() const { return chenglueUnlimitedSuitMask != 0; }
    void clearCardTypesUsedThisTurn() { cardTypesUsedThisTurn.clear(); }
    bool hasUsedCardTypeThisTurn(CardType type) const { return cardTypesUsedThisTurn.count(type) != 0; }
    void recordCardTypeUsedThisTurn(CardType type) { cardTypesUsedThisTurn.insert(type); }

    int getMark(const std::string& name) const {
        auto it = marks.find(name); return it == marks.end() ? 0 : it->second;
    }
    void addMark(const std::string& name, int amount) { marks[name] = std::max(0, getMark(name) + amount); }
    // 观察局势：枚举公开标记（技能状态/资源）
    const std::map<std::string, int>& getMarks() const { return marks; }

    // ---- 武将牌上的牌 ----
    const std::vector<CardPtr>& getPile(const std::string& pileName) const;
    std::vector<std::string> getPileNames() const;
    int getPileCount(const std::string& pileName) const;
    void addToPile(const std::string& pileName, CardPtr card);
    bool removeFromPile(const std::string& pileName, CardPtr card);
    std::vector<CardPtr> takeAllPiles();

    // ---- 回合内临时状态 ----
    bool isNonLockSkillsDisabled() const { return nonLockSkillsDisabled; }
    void setNonLockSkillsDisabled(bool v) { nonLockSkillsDisabled = v; }
    bool isHandCardsBanned() const { return handCardsBanned; }
    void setHandCardsBanned(bool v) { handCardsBanned = v; }
    int getYiJueMarkSourceId() const { return yiJueMarkSourceId; }
    void setYiJueMarkSourceId(int id) { yiJueMarkSourceId = id; }
    void clearTurnEffects();

    bool isTiShenActive() const { return tiShenActive; }
    void setTiShenActive(bool v) { tiShenActive = v; }

    bool hasHandCard(const CardPtr& card) const;
    bool hasEquipment(const CardPtr& card) const;
    bool hasCardInAnyPile(const CardPtr& card) const;
    bool removeCardFromAnyPile(const CardPtr& card);

    const std::vector<CardPtr>& getHandCards() const { return handCards; }
    int getHandCardCount() const { return handCards.size(); }
    void addHandCard(CardPtr card);
    void addHandCards(const std::vector<CardPtr>& cards);
    // 取走全部手牌并清空（缔盟交换手牌等）
    std::vector<CardPtr> takeAllHandCards() { auto v = handCards; handCards.clear(); return v; }
    bool removeHandCard(CardPtr card);
    CardPtr removeHandCardByIndex(int index);

    CardPtr getWeapon() const { return weapon; }
    CardPtr getArmor() const { return armor; }
    CardPtr getOffensiveHorse() const { return offensiveHorse; }
    CardPtr getDefensiveHorse() const { return defensiveHorse; }

    CardPtr equip(CardPtr card);
    CardPtr removeEquipment(CardSubType type);
    std::vector<CardPtr> getAllEquipment() const;
    // 普通“弃置一张牌”候选：手牌+装备区，不含判定区。
    std::vector<CardPtr> getHandAndEquipmentCards() const;

    const std::vector<CardPtr>& getJudgeZone() const { return judgeZone; }
    void addJudgeCard(CardPtr card) { judgeZone.push_back(card); }
    bool removeJudgeCard(CardPtr card);

    int getAttackRange() const;
    int getHandLimit() const;

    std::vector<CardPtr> getAllCards() const;

    std::string getFormattedStatus(bool showIdentity = true) const;
    std::string getFormattedHandCards() const;
    std::string getFormattedEquipment() const;
};

using PlayerPtr = std::shared_ptr<Player>;

} // namespace Thks

#endif // THKS_PLAYER_H
