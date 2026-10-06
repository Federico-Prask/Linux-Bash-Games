#include "Card.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>

namespace Thks {

Card::Card(int id, const std::string& name, Suit suit, int rank, CardType type, CardSubType subType,
           ShaElement shaElement, int attackRange, const std::string& desc)
    : id(id), name(name), suit(suit), rank(rank), type(type), subType(subType),
      shaElement(shaElement), attackRange(attackRange), description(desc) {}

std::string Card::getSuitSymbol() const {
    switch (suit) {
        case Suit::SPADE:   return "♠";
        case Suit::HEART:   return "♥";
        case Suit::CLUB:    return "♣";
        case Suit::DIAMOND: return "♦";
        default:            return "";
    }
}

std::string Card::getRankString() const {
    if (rank == 1) return "A";
    if (rank == 11) return "J";
    if (rank == 12) return "Q";
    if (rank == 13) return "K";
    return std::to_string(rank);
}

std::string Card::getFormattedName() const {
    if (isVirtual()) {
        if (subCards.empty()) {
            // 占位转化牌（如丈八蛇矛：选中后再指定两张手牌）
            return "【" + name + "】(" + skillSource + ":任意两张手牌)";
        }
        std::string inner;
        for (size_t i = 0; i < subCards.size(); ++i) {
            if (i) inner += "+";
            inner += subCards[i]->getFormattedName();
        }
        return "【" + name + "】(" + skillSource + ":" + inner + ")";
    }
    // 花色符号（♠♥♦♣）在部分终端字体下按宽字符渲染，后补一个空格避免点数被遮挡
    return "[" + getSuitSymbol() + " " + getRankString() + " " + name + "]";
}

std::string Card::getPublicName() const {
    if(skillSource=="蛊惑")return "【"+name+"】（蛊惑扣置，底牌未公开）";
    return getFormattedName();
}

std::string Card::getTypeString() const {
    switch (type) {
        case CardType::BASIC:     return "基本牌";
        case CardType::TRICK:     return "锦囊牌";
        case CardType::EQUIPMENT: return "装备牌";
        default:                  return "未知";
    }
}

std::vector<CardPtr> Card::getRealCards(const CardPtr& self) const {
    if (isVirtual()) return subCards;
    return {self};
}

CardPtr Card::makeVirtual(const std::string& name, CardType type, CardSubType subType,
                          const std::vector<CardPtr>& subCards, const std::string& skillName,
                          ShaElement element) {
    // 单张实体牌转化：继承花色与点数；多张转化：无花色、无点数（官方规则）
    Suit suit = Suit::NONE;
    int rank = 0;
    if (subCards.size() == 1) {
        suit = subCards[0]->getSuit();
        rank = subCards[0]->getRank();
    }
    auto card = std::make_shared<Card>(0, name, suit, rank, type, subType, element, 1, "由技能【" + skillName + "】转化");
    card->subCards = subCards;
    card->skillSource = skillName;
    return card;
}

// ==================== Deck ====================

Deck::Deck() : shuffleSeed(0) {
    initStandardDeck();
    shuffleDrawPile();
}

void Deck::initStandardDeck() {
    drawPile.clear();
    discardPile.clear();

    int cardId = 1;

    // 1. 基本牌
    for (int r : {7, 8, 8, 9, 9, 10, 10}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "杀", Suit::SPADE, r, CardType::BASIC, CardSubType::SHA));
    }
    for (int r : {2, 3, 4, 5, 6, 7, 8, 8, 9, 9, 10, 10, 11, 11}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "杀", Suit::CLUB, r, CardType::BASIC, CardSubType::SHA));
    }
    for (int r : {10, 10, 11}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "杀", Suit::HEART, r, CardType::BASIC, CardSubType::SHA));
    }
    for (int r : {6, 7, 8, 9, 10, 13}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "杀", Suit::DIAMOND, r, CardType::BASIC, CardSubType::SHA));
    }
    drawPile.push_back(std::make_shared<Card>(cardId++, "雷杀", Suit::SPADE, 4, CardType::BASIC, CardSubType::SHA, ShaElement::THUNDER));
    drawPile.push_back(std::make_shared<Card>(cardId++, "雷杀", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA, ShaElement::THUNDER));
    drawPile.push_back(std::make_shared<Card>(cardId++, "雷杀", Suit::SPADE, 6, CardType::BASIC, CardSubType::SHA, ShaElement::THUNDER));
    drawPile.push_back(std::make_shared<Card>(cardId++, "雷杀", Suit::CLUB, 5, CardType::BASIC, CardSubType::SHA, ShaElement::THUNDER));
    drawPile.push_back(std::make_shared<Card>(cardId++, "雷杀", Suit::CLUB, 6, CardType::BASIC, CardSubType::SHA, ShaElement::THUNDER));

    drawPile.push_back(std::make_shared<Card>(cardId++, "火杀", Suit::HEART, 4, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE));
    drawPile.push_back(std::make_shared<Card>(cardId++, "火杀", Suit::HEART, 7, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE));
    drawPile.push_back(std::make_shared<Card>(cardId++, "火杀", Suit::HEART, 10, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE));
    drawPile.push_back(std::make_shared<Card>(cardId++, "火杀", Suit::DIAMOND, 4, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE));
    drawPile.push_back(std::make_shared<Card>(cardId++, "火杀", Suit::DIAMOND, 5, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE));

    for (int r : {2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "闪", Suit::DIAMOND, r, CardType::BASIC, CardSubType::SHAN));
    }
    for (int r : {2, 2, 13}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "闪", Suit::HEART, r, CardType::BASIC, CardSubType::SHAN));
    }

    for (int r : {3, 4, 5, 6, 7, 8, 9, 12}) {
        drawPile.push_back(std::make_shared<Card>(cardId++, "桃", Suit::HEART, r, CardType::BASIC, CardSubType::TAO));
    }
    drawPile.push_back(std::make_shared<Card>(cardId++, "桃", Suit::DIAMOND, 12, CardType::BASIC, CardSubType::TAO));

    drawPile.push_back(std::make_shared<Card>(cardId++, "酒", Suit::SPADE, 3, CardType::BASIC, CardSubType::JIU));
    // 瞒天过海（势·董昭关键卡牌）：牌面据官网列为关键卡、但未载规则；规则据 secondary source 记录于 docs/shi_you_appendix.md。
    // 黑桃5/红桃5/梅花5/方块5
    const std::string mantianText =
        "此牌不计入手牌上限。出牌阶段，对至多两名区域内有牌的其他角色使用。"
        "你依次获得目标角色区域内的一张牌，然后依次交给目标角色一张牌。";
    drawPile.push_back(std::make_shared<Card>(cardId++, "瞒天过海", Suit::SPADE, 5, CardType::TRICK, CardSubType::MANTIAN_GUOHAI, ShaElement::NORMAL, 1, mantianText));
    drawPile.push_back(std::make_shared<Card>(cardId++, "瞒天过海", Suit::HEART, 5, CardType::TRICK, CardSubType::MANTIAN_GUOHAI, ShaElement::NORMAL, 1, mantianText));
    drawPile.push_back(std::make_shared<Card>(cardId++, "瞒天过海", Suit::CLUB, 5, CardType::TRICK, CardSubType::MANTIAN_GUOHAI, ShaElement::NORMAL, 1, mantianText));
    drawPile.push_back(std::make_shared<Card>(cardId++, "瞒天过海", Suit::DIAMOND, 5, CardType::TRICK, CardSubType::MANTIAN_GUOHAI, ShaElement::NORMAL, 1, mantianText));
    drawPile.push_back(std::make_shared<Card>(cardId++, "酒", Suit::SPADE, 9, CardType::BASIC, CardSubType::JIU));
    drawPile.push_back(std::make_shared<Card>(cardId++, "酒", Suit::CLUB, 3, CardType::BASIC, CardSubType::JIU));
    drawPile.push_back(std::make_shared<Card>(cardId++, "酒", Suit::DIAMOND, 9, CardType::BASIC, CardSubType::JIU));

    // 2. 锦囊牌
    drawPile.push_back(std::make_shared<Card>(cardId++, "过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO, ShaElement::NORMAL, 1, "弃置一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "过河拆桥", Suit::SPADE, 4, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO, ShaElement::NORMAL, 1, "弃置一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "过河拆桥", Suit::SPADE, 12, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO, ShaElement::NORMAL, 1, "弃置一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "过河拆桥", Suit::HEART, 12, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO, ShaElement::NORMAL, 1, "弃置一名角色区域里的一张牌"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "顺手牵羊", Suit::SPADE, 3, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG, ShaElement::NORMAL, 1, "获得距离为1的一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "顺手牵羊", Suit::SPADE, 4, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG, ShaElement::NORMAL, 1, "获得距离为1的一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "顺手牵羊", Suit::SPADE, 11, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG, ShaElement::NORMAL, 1, "获得距离为1的一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "顺手牵羊", Suit::DIAMOND, 3, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG, ShaElement::NORMAL, 1, "获得距离为1的一名角色区域里的一张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "顺手牵羊", Suit::DIAMOND, 4, CardType::TRICK, CardSubType::SHUN_SHOU_QIAN_YANG, ShaElement::NORMAL, 1, "获得距离为1的一名角色区域里的一张牌"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "决斗", Suit::SPADE, 1, CardType::TRICK, CardSubType::JUE_DOU, ShaElement::NORMAL, 1, "目标角色与你轮流打出【杀】，先不打者受1点伤害"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "决斗", Suit::CLUB, 1, CardType::TRICK, CardSubType::JUE_DOU, ShaElement::NORMAL, 1, "目标角色与你轮流打出【杀】，先不打者受1点伤害"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "决斗", Suit::DIAMOND, 1, CardType::TRICK, CardSubType::JUE_DOU, ShaElement::NORMAL, 1, "目标角色与你轮流打出【杀】，先不打者受1点伤害"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "南蛮入侵", Suit::SPADE, 7, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN, ShaElement::NORMAL, 1, "所有其他角色需打出【杀】，否则受到1点伤害"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "南蛮入侵", Suit::SPADE, 13, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN, ShaElement::NORMAL, 1, "所有其他角色需打出【杀】，否则受到1点伤害"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "南蛮入侵", Suit::CLUB, 7, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN, ShaElement::NORMAL, 1, "所有其他角色需打出【杀】，否则受到1点伤害"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "万箭齐发", Suit::HEART, 1, CardType::TRICK, CardSubType::WAN_JIAN_QI_FA, ShaElement::NORMAL, 1, "所有其他角色需打出【闪】，否则受到1点伤害"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "无中生有", Suit::HEART, 7, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU, ShaElement::NORMAL, 1, "摸两张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "无中生有", Suit::HEART, 8, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU, ShaElement::NORMAL, 1, "摸两张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "无中生有", Suit::HEART, 9, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU, ShaElement::NORMAL, 1, "摸两张牌"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "无中生有", Suit::HEART, 11, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU, ShaElement::NORMAL, 1, "摸两张牌"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "桃园结义", Suit::HEART, 1, CardType::TRICK, CardSubType::TAO_YUAN_JIE_YI, ShaElement::NORMAL, 1, "所有受损角色各回复1点体力"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "无懈可击", Suit::SPADE, 11, CardType::TRICK, CardSubType::WU_XIE_KE_JI, ShaElement::NORMAL, 1, "抵消一张锦囊牌的效果"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "无懈可击", Suit::SPADE, 13, CardType::TRICK, CardSubType::WU_XIE_KE_JI, ShaElement::NORMAL, 1, "抵消一张锦囊牌的效果"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "无懈可击", Suit::CLUB, 12, CardType::TRICK, CardSubType::WU_XIE_KE_JI, ShaElement::NORMAL, 1, "抵消一张锦囊牌的效果"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "无懈可击", Suit::CLUB, 13, CardType::TRICK, CardSubType::WU_XIE_KE_JI, ShaElement::NORMAL, 1, "抵消一张锦囊牌的效果"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "五谷丰登", Suit::HEART, 3, CardType::TRICK, CardSubType::WU_GU_FENG_DENG, ShaElement::NORMAL, 1, "亮出等同于存活角色数量的牌，每名角色顺序挑选一张"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "五谷丰登", Suit::HEART, 4, CardType::TRICK, CardSubType::WU_GU_FENG_DENG, ShaElement::NORMAL, 1, "亮出等同于存活角色数量的牌，每名角色顺序挑选一张"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "借刀杀人", Suit::CLUB, 12, CardType::TRICK, CardSubType::JIE_DAO_SHA_REN, ShaElement::NORMAL, 1, "令装备区有武器的角色对其攻击范围内你指定的一名角色使用【杀】，否则将武器交给你"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "借刀杀人", Suit::CLUB, 13, CardType::TRICK, CardSubType::JIE_DAO_SHA_REN, ShaElement::NORMAL, 1, "令装备区有武器的角色对其攻击范围内你指定的一名角色使用【杀】，否则将武器交给你"));
    for (int r : {2, 3}) drawPile.push_back(std::make_shared<Card>(cardId++, "铁索连环", Suit::CLUB, r, CardType::TRICK, CardSubType::TIE_SUO_LIAN_HUAN, ShaElement::NORMAL, 1, "横置或重置至多两名角色；属性伤害传导"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "火攻", Suit::HEART, 2, CardType::TRICK, CardSubType::HUO_GONG, ShaElement::NORMAL, 1, "令目标展示手牌，弃同花色手牌则对其造成1点火焰伤害"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "乐不思蜀", Suit::SPADE, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU, ShaElement::NORMAL, 1, "判定牌不为红桃则跳过出牌阶段"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "乐不思蜀", Suit::CLUB, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU, ShaElement::NORMAL, 1, "判定牌不为红桃则跳过出牌阶段"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU, ShaElement::NORMAL, 1, "判定牌不为红桃则跳过出牌阶段"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "闪电", Suit::SPADE, 1, CardType::TRICK, CardSubType::SHAN_DIAN, ShaElement::NORMAL, 1, "判定牌为黑桃2-9则受到3点雷电伤害"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "闪电", Suit::HEART, 9, CardType::TRICK, CardSubType::SHAN_DIAN, ShaElement::NORMAL, 1, "判定牌为黑桃2-9则受到3点雷电伤害"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "兵粮寸断", Suit::SPADE, 10, CardType::TRICK, CardSubType::BING_LIANG_CUN_DUAN, ShaElement::NORMAL, 1, "判定牌不为草花则跳过摸牌阶段"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "兵粮寸断", Suit::CLUB, 4, CardType::TRICK, CardSubType::BING_LIANG_CUN_DUAN, ShaElement::NORMAL, 1, "判定牌不为草花则跳过摸牌阶段"));

    // 3. 装备牌
    drawPile.push_back(std::make_shared<Card>(cardId++, "诸葛连弩", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 1, "攻击距离1；出牌阶段使用【杀】无次数限制"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "诸葛连弩", Suit::DIAMOND, 1, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 1, "攻击距离1；出牌阶段使用【杀】无次数限制"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "青龙偃月刀", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 3, "攻击距离3；【杀】被【闪】抵消后可对其再使用【杀】"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "丈八蛇矛", Suit::SPADE, 12, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 3, "攻击距离3；可将两张手牌当【杀】使用"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "青釭剑", Suit::SPADE, 6, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 2, "攻击距离2；锁定技，使用【杀】时锁定无视防具"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "贯石斧", Suit::DIAMOND, 5, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 3, "攻击距离3；【杀】被【闪】抵消时可弃两张牌强制命中"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "方天画戟", Suit::DIAMOND, 12, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 4, "攻击距离4；使用最后一张手牌【杀】时可指定额外2个目标"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "麒麟弓", Suit::HEART, 5, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 5, "攻击距离5；【杀】造成伤害时可弃置目标马匹"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "寒冰剑", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 2, "攻击距离2；造成伤害时可防止伤害改弃置其两张牌"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "八卦阵", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::ARMOR, ShaElement::NORMAL, 1, "需要打出【闪】时可判定，红桃或方块视为打出【闪】"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "八卦阵", Suit::CLUB, 2, CardType::EQUIPMENT, CardSubType::ARMOR, ShaElement::NORMAL, 1, "需要打出【闪】时可判定，红桃或方块视为打出【闪】"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "仁王盾", Suit::CLUB, 2, CardType::EQUIPMENT, CardSubType::ARMOR, ShaElement::NORMAL, 1, "锁定技，黑色【杀】对你无效"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "藤甲", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::ARMOR, ShaElement::NORMAL, 1, "普通杀与南蛮万箭无效；受火焰伤害+1"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "白银狮子", Suit::CLUB, 1, CardType::EQUIPMENT, CardSubType::ARMOR, ShaElement::NORMAL, 1, "受单次伤害最多为1；失去装备时回复1体力"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "赤兔 (-1马)", Suit::HEART, 5, CardType::EQUIPMENT, CardSubType::OFFENSIVE_HORSE, ShaElement::NORMAL, 1, "你与其他角色的距离-1"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "大宛 (-1马)", Suit::SPADE, 13, CardType::EQUIPMENT, CardSubType::OFFENSIVE_HORSE, ShaElement::NORMAL, 1, "你与其他角色的距离-1"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "紫骍 (-1马)", Suit::DIAMOND, 13, CardType::EQUIPMENT, CardSubType::OFFENSIVE_HORSE, ShaElement::NORMAL, 1, "你与其他角色的距离-1"));

    drawPile.push_back(std::make_shared<Card>(cardId++, "绝影 (+1马)", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE, ShaElement::NORMAL, 1, "其他角色与你的距离+1"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "狄芦 (+1马)", Suit::CLUB, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE, ShaElement::NORMAL, 1, "其他角色与你的距离+1"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "爪黄飞电 (+1马)", Suit::HEART, 13, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE, ShaElement::NORMAL, 1, "其他角色与你的距离+1"));
    drawPile.push_back(std::make_shared<Card>(cardId++, "骅骝 (+1马)", Suit::DIAMOND, 13, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE, ShaElement::NORMAL, 1, "其他角色与你的距离+1"));
}

void Deck::shuffleDrawPile() {
    unsigned seed = shuffleSeed;
    if (seed == 0) {
        seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
    }
    std::shuffle(drawPile.begin(), drawPile.end(), std::default_random_engine(seed));
    shuffleSeed = seed * 1664525u + 1013904223u; // 下次洗牌使用不同但可复现的种子
}

CardPtr Deck::drawCard() {
    if (drawPile.empty()) {
        recycleDiscardPile();
    }
    if (drawPile.empty()) {
        return nullptr;
    }
    CardPtr card = drawPile.back();
    drawPile.pop_back();
    return card;
}

CardPtr Deck::drawCardFromBottom() {
    if (drawPile.empty()) recycleDiscardPile();
    if (drawPile.empty()) return nullptr;
    CardPtr card = drawPile.front();
    drawPile.erase(drawPile.begin());
    return card;
}

CardPtr Deck::drawRandomMatching(const std::function<bool(const CardPtr&)>& predicate, std::mt19937& rng) {
    std::vector<size_t> matches;
    for(size_t i=0;i<drawPile.size();++i)if(predicate(drawPile[i]))matches.push_back(i);
    if(matches.empty())return nullptr;
    std::uniform_int_distribution<size_t> pick(0,matches.size()-1);
    size_t index=matches[pick(rng)];
    auto card=drawPile[index];
    drawPile.erase(drawPile.begin()+index);
    return card;
}

std::vector<CardPtr> Deck::drawCards(int count) {
    std::vector<CardPtr> result;
    for (int i = 0; i < count; ++i) {
        CardPtr card = drawCard();
        if (card) result.push_back(card);
        else break;
    }
    return result;
}

std::vector<CardPtr> Deck::drawCardsFromBottom(int count) {
    std::vector<CardPtr> result;
    for (int i = 0; i < count; ++i) {
        CardPtr card = drawCardFromBottom();
        if (card) result.push_back(card);
        else break;
    }
    return result;
}

std::vector<CardPtr> Deck::peekTopCards(int count) const {
    std::vector<CardPtr> result;
    for (int i = static_cast<int>(drawPile.size()) - 1; i >= 0 && static_cast<int>(result.size()) < count; --i) {
        result.push_back(drawPile[i]);
    }
    return result; // result[0] = 牌堆顶
}

void Deck::putOnTop(const std::vector<CardPtr>& cards) {
    // cards[0] 为最终的顶牌
    for (auto it = cards.rbegin(); it != cards.rend(); ++it) {
        if (*it) drawPile.push_back(*it);
    }
}

void Deck::putOnBottom(const std::vector<CardPtr>& cards) {
    // 从底到顶依次插入：cards[0] 在最底
    size_t insertPos = 0;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (cards[i]) drawPile.insert(drawPile.begin() + insertPos++, cards[i]);
    }
}

void Deck::discardCard(CardPtr card) {
    if (card) {
        discardPile.push_back(card);
    }
}

bool Deck::removeDiscardCard(CardPtr card) {
    auto it=std::find(discardPile.begin(),discardPile.end(),card);
    if(it==discardPile.end())return false;
    discardPile.erase(it);return true;
}
void Deck::discardCards(const std::vector<CardPtr>& cards) {
    for (const auto& card : cards) {
        discardCard(card);
    }
}

void Deck::recycleDiscardPile() {
    if (discardPile.empty()) return;
    drawPile = std::move(discardPile);
    discardPile.clear();
    shuffleDrawPile();
}

} // namespace Thks
