#include "test_helpers.h"

TEST("player/lord_gets_bonus_max_hp") {
    HeroPtr hero = std::make_shared<Hero>("h", "测试", "称号", "测试包", Country::SHU, Gender::MALE, 4);
    Player lord(0, "主", Identity::ZHU_GONG, false, hero);
    CHECK_EQ(lord.getMaxHp(), 5); // 主公 +1
    CHECK_EQ(lord.getHp(), 5);

    Player rebel(1, "反", Identity::FAN_ZEI, true, hero);
    CHECK_EQ(rebel.getMaxHp(), 4);
}

TEST("player/hp_clamp_and_change") {
    Player p(0, "p", Identity::ZHU_GONG, true, blankHero(4));
    CHECK_EQ(p.getMaxHp(), 5);
    p.changeHp(-2);
    CHECK_EQ(p.getHp(), 3);
    CHECK(p.isWounded());
    p.changeHp(+10);
    CHECK_EQ(p.getHp(), 5); // 不超过上限
    p.setHp(99);
    CHECK_EQ(p.getHp(), 5);
}

TEST("player/hand_limit_equals_hp") {
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero(4));
    CHECK_EQ(p.getHandLimit(), 4);
    p.changeHp(-3);
    CHECK_EQ(p.getHandLimit(), 1);
    p.changeHp(-5);
    CHECK_EQ(p.getHandLimit(), 0); // max(0, hp)
}

TEST("player/equip_replace_returns_old") {
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero());
    auto w1 = makeCard("青龙偃月刀", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 3);
    auto w2 = makeCard("寒冰剑", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::WEAPON, ShaElement::NORMAL, 2);
    CHECK(p.equip(w1) == nullptr);
    CHECK_EQ(p.getAttackRange(), 3);
    CardPtr old = p.equip(w2);
    CHECK(old == w1);       // 返回被替换的旧装备
    CHECK_EQ(p.getAttackRange(), 2);
    CHECK(p.hasEquipment(w2));
    CHECK_EQ(p.removeEquipment(CardSubType::WEAPON), w2);
    CHECK_EQ(p.getAttackRange(), 1); // 无武器时基础攻击范围 1
}

TEST("player/pile_add_remove_take_all") {
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero());
    auto c1 = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto c2 = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);
    p.addToPile("权", c1);
    p.addToPile("权", c2);
    CHECK_EQ(p.getPileCount("权"), 2);
    CHECK_EQ(p.getPile("权").size(), static_cast<size_t>(2));
    CHECK(p.removeFromPile("权", c1));
    CHECK(!p.removeFromPile("权", c1)); // 重复移除失败
    auto rest = p.takeAllPiles();
    CHECK_EQ(rest.size(), static_cast<size_t>(1));
    CHECK(rest[0] == c2);
    CHECK_EQ(p.getPileCount("权"), 0);
}

TEST("player/turn_effects_cleared") {
    Player p(0, "p", Identity::FAN_ZEI, true, blankHero());
    p.setNonLockSkillsDisabled(true);
    p.setHandCardsBanned(true);
    p.setYiJueMarkSourceId(3);
    p.setTiShenActive(true);
    p.clearTurnEffects();
    CHECK(!p.isNonLockSkillsDisabled());
    CHECK(!p.isHandCardsBanned());
    CHECK_EQ(p.getYiJueMarkSourceId(), -1);
    CHECK(p.isTiShenActive()); // 替身是跨回合状态，不清除
}

TEST("player/formatted_status_contains_info") {
    HeroPtr hero = std::make_shared<Hero>("h", "钟会", "桀骜的野心家", "一将成名", Country::WEI, Gender::MALE, 4);
    Player p(2, "电脑2", Identity::NEI_JIAN, true, hero);
    auto s = p.getFormattedStatus(true);
    CHECK(s.find("电脑2") != std::string::npos);
    CHECK(s.find("钟会") != std::string::npos);
    CHECK(s.find("内奸") != std::string::npos);
    CHECK(s.find("HP: 4/4") != std::string::npos);
    CHECK(p.getFormattedEquipment() == std::string("（无装备）"));
}
