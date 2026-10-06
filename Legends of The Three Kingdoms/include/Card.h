#ifndef THKS_CARD_H
#define THKS_CARD_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <random>

namespace Thks {

enum class Suit {
    SPADE,   // 黑桃 ♠
    HEART,   // 红桃 ♥
    CLUB,    // 草花 ♣
    DIAMOND, // 方块 ♦
    NONE
};

enum class CardType {
    BASIC,     // 基本牌
    TRICK,     // 锦囊牌
    EQUIPMENT  // 装备牌
};

enum class CardSubType {
    // 基本牌
    SHA,               // 杀
    SHAN,              // 闪
    TAO,               // 桃
    JIU,               // 酒

    // 锦囊牌 - 即时锦囊
    GUO_HE_CHAI_QIAO,  // 过河拆桥
    SHUN_SHOU_QIAN_YANG,// 顺手牵羊
    JUE_DOU,           // 决斗
    NAN_MAN_RU_QIN,    // 南蛮入侵
    WAN_JIAN_QI_FA,     // 万箭齐发
    MANTIAN_GUOHAI,    // 瞒天过海（势-董昭【势-妙略】）
    WU_ZHONG_SHENG_YOU,// 无中生有
    TAO_YUAN_JIE_YI,   // 桃园结义
    WU_XIE_KE_JI,      // 无懈可击
    WU_GU_FENG_DENG,   // 五谷丰登
    JIE_DAO_SHA_REN,     // 借刀杀人
    HUO_GONG,           // 火攻
    TIE_SUO_LIAN_HUAN, // 铁索连环

    // 锦囊牌 - 延时锦囊
    LE_BU_SI_SHU,      // 乐不思蜀
    SHAN_DIAN,         // 闪电
    BING_LIANG_CUN_DUAN,// 兵粮寸断

    // 装备牌 - 武器
    WEAPON,            // 武器类

    // 装备牌 - 防具
    ARMOR,             // 防具类

    // 装备牌 - 坐骑
    OFFENSIVE_HORSE,   // -1马
    DEFENSIVE_HORSE    // +1马
};

enum class ShaElement {
    NORMAL,  // 普通杀
    FIRE,    // 火杀
    THUNDER  // 雷杀
};

class Card;
using CardPtr = std::shared_ptr<Card>;

class Card {
private:
    int id;
    std::string name;
    Suit suit;
    int rank; // 1 (A) - 13 (K)，转化牌无点数时为 0
    CardType type;
    CardSubType subType;
    ShaElement shaElement;
    int attackRange;
    std::string description;

    // ---- 转化牌（视为牌）支持 ----
    // 例如关羽【武圣】把 ♥7桃 当【杀】使用：生成一张 subType=SHA 的转化牌，
    // subCards = {♥7桃}，花色点数继承自实体牌。
    std::vector<CardPtr> subCards;
    std::string skillSource;

public:
    Card(int id, const std::string& name, Suit suit, int rank, CardType type, CardSubType subType,
         ShaElement shaElement = ShaElement::NORMAL, int attackRange = 1, const std::string& desc = "");

    int getId() const { return id; }
    std::string getName() const { return name; }
    Suit getSuit() const { return suit; }
    // 只用于「视为」花色的临时结算视图；实体牌本身及所属牌堆均不修改。
    CardPtr copyWithSuit(Suit effective) const { auto view = std::make_shared<Card>(*this); view->suit = effective; return view; }
    int getRank() const { return rank; }
    CardType getType() const { return type; }
    CardSubType getSubType() const { return subType; }
    ShaElement getShaElement() const { return shaElement; }
    int getAttackRange() const { return attackRange; }
    std::string getDescription() const { return description; }

    bool isRed() const { return suit == Suit::HEART || suit == Suit::DIAMOND; }
    bool isBlack() const { return suit == Suit::SPADE || suit == Suit::CLUB; }

    // 转化牌相关
    bool isVirtual() const { return !skillSource.empty(); }
    const std::vector<CardPtr>& getSubCards() const { return subCards; }
    std::string getSkillSource() const { return skillSource; }
    // 返回真正会进入弃牌堆/被获得的实体牌（实体牌返回自身）
    std::vector<CardPtr> getRealCards(const CardPtr& self) const;

    static CardPtr makeVirtual(const std::string& name, CardType type, CardSubType subType,
                               const std::vector<CardPtr>& subCards, const std::string& skillName,
                               ShaElement element = ShaElement::NORMAL);

    std::string getSuitSymbol() const;
    std::string getRankString() const;
    std::string getFormattedName() const;
    // 公共游戏日志显示名：蛊惑未验真前不泄露扣置牌的花色、点数及牌名。
    std::string getPublicName() const;
    std::string getTypeString() const;
};

class Deck {
private:
    std::vector<CardPtr> drawPile;
    std::vector<CardPtr> discardPile;
    unsigned shuffleSeed;

public:
    Deck();
    void initStandardDeck();
    void setSeed(unsigned seed) { shuffleSeed = seed; }
    void shuffleDrawPile();
    CardPtr drawCard();
    CardPtr drawCardFromBottom();
    // 从牌堆中随机取出符合条件的牌，保留其他牌的相对顺序。
    CardPtr drawRandomMatching(const std::function<bool(const CardPtr&)>& predicate, std::mt19937& rng);
    std::vector<CardPtr> drawCards(int count);
    std::vector<CardPtr> drawCardsFromBottom(int count);
    CardPtr peekTop() const { return drawPile.empty() ? nullptr : drawPile.back(); }
    std::vector<CardPtr> peekTopCards(int count) const;   // 窥视牌堆顶 count 张（不取出）
    void putOnTop(const std::vector<CardPtr>& cards);     // 按给定顺序置于牌堆顶（首张为顶）
    void putOnBottom(const std::vector<CardPtr>& cards);  // 按给定顺序置于牌堆底（首张在最底）
    void discardCard(CardPtr card);
    bool removeDiscardCard(CardPtr card); // 固政：从弃牌堆取回本阶段弃置的牌
    void discardCards(const std::vector<CardPtr>& cards);

    int getDrawPileSize() const { return static_cast<int>(drawPile.size()); }
    int getDiscardPileSize() const { return static_cast<int>(discardPile.size()); }
    const std::vector<CardPtr>& getDrawPile() const { return drawPile; }
    const std::vector<CardPtr>& getDiscardPile() const { return discardPile; }
    void recycleDiscardPile();
};

} // namespace Thks

#endif // THKS_CARD_H
