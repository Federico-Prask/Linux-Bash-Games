#include "test_framework.h"
#include "Card.h"
using namespace Thks;

// ---------------- 牌堆 ----------------

TEST("card/deck_standard_129") {
    Deck deck;
    CHECK_EQ(deck.getDrawPileSize(), 133);
    CHECK_EQ(deck.getDiscardPileSize(), 0);
}

TEST("card/deck_shuffle_keeps_size_and_seedable") {
    Deck a, b;
    a.setSeed(42);
    a.shuffleDrawPile();
    b.setSeed(42);
    b.shuffleDrawPile();
    CHECK_EQ(a.getDrawPileSize(), 133);
    CHECK_EQ(b.getDrawPileSize(), 133);
}

TEST("card/deck_draw_then_recycle") {
    Deck deck;
    for (int i = 0; i < 133; ++i) {
        auto c = deck.drawCard();
        CHECK(c != nullptr);
        deck.discardCard(c);
    }
    CHECK_EQ(deck.getDrawPileSize(), 0);
    CHECK_EQ(deck.getDiscardPileSize(), 133);
    // 牌堆抽干后自动回收弃牌堆：弃牌堆整堆转入牌堆后再弹 1 张
    auto c = deck.drawCard();
    CHECK(c != nullptr);
    CHECK_EQ(deck.getDiscardPileSize(), 0);
    CHECK_EQ(deck.getDrawPileSize(), 132);
}

TEST("card/deck_draw_cards_partial_when_empty") {
    Deck deck;
    CHECK_EQ(deck.drawCards(5).size(), static_cast<size_t>(5));
    for (int i = 0; i < 133 - 5 - 5; ++i) deck.drawCard();
    // 只剩 5 张：抽 10 只得 5，再抽 1 为 0
    CHECK_EQ(deck.drawCards(10).size(), static_cast<size_t>(5));
    CHECK_EQ(deck.drawCards(1).size(), static_cast<size_t>(0));
}

// ---------------- 牌的格式化 ----------------

TEST("card_formatted_name_and_rank_strings") {
    Card sha(1, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    CHECK_EQ(sha.getFormattedName(), std::string("[♠ 7 杀]"));
    Card tao(2, "桃", Suit::HEART, 1, CardType::BASIC, CardSubType::TAO);
    CHECK_EQ(tao.getFormattedName(), std::string("[♥ A 桃]"));
    Card shan(3, "闪", Suit::DIAMOND, 11, CardType::BASIC, CardSubType::SHAN);
    CHECK_EQ(shan.getFormattedName(), std::string("[♦ J 闪]"));
    Card jiu(4, "酒", Suit::CLUB, 13, CardType::BASIC, CardSubType::JIU);
    CHECK_EQ(jiu.getFormattedName(), std::string("[♣ K 酒]"));
    CHECK(sha.isBlack());
    CHECK(!sha.isRed());
    CHECK(tao.isRed());
}

// ---------------- 转化牌（虚拟牌） ----------------

TEST("card_virtual_single_sub_inherits_suit_rank") {
    auto shan = std::make_shared<Card>(10, "闪", Suit::DIAMOND, 8, CardType::BASIC, CardSubType::SHAN);
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {shan}, "龙胆");
    CHECK(sha->isVirtual());
    CHECK_EQ(sha->getSkillSource(), std::string("龙胆"));
    CHECK(sha->getSuit() == Suit::DIAMOND); // 单张转化继承花色
    CHECK_EQ(sha->getRank(), 8);             // 与点数
    CHECK(sha->getSubType() == CardSubType::SHA);
    CHECK_EQ(sha->getRealCards(sha).size(), static_cast<size_t>(1));
    CHECK_EQ(sha->getRealCards(sha)[0], shan);
    CHECK_EQ(sha->getFormattedName(), std::string("【杀】(龙胆:[♦ 8 闪])"));
}

TEST("card_virtual_multi_sub_no_suit_rank") {
    auto c1 = std::make_shared<Card>(11, "杀", Suit::SPADE, 3, CardType::BASIC, CardSubType::SHA);
    auto c2 = std::make_shared<Card>(12, "杀", Suit::HEART, 4, CardType::BASIC, CardSubType::SHA);
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {c1, c2}, "丈八蛇矛");
    CHECK(sha->getSuit() == Suit::NONE); // 多张转化无花色无点数（官方规则）
    CHECK_EQ(sha->getRank(), 0);
    CHECK_EQ(sha->getRealCards(sha).size(), static_cast<size_t>(2));
}
