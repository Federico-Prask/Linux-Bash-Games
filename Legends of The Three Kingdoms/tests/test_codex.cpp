#include "test_framework.h"
#include "Codex.h"
#include "Card.h"

using namespace Thks;

namespace {
const Codex::CardEntry* findEntry(const std::vector<Codex::CardEntry>& catalog, const std::string& name) {
    for (const auto& e : catalog) {
        if (e.name == name) return &e;
    }
    return nullptr;
}
} // namespace

TEST("codex/catalog_groups_and_counts") {
    auto catalog = Codex::buildCardCatalog();
    CHECK(!catalog.empty());
    // 去重前后总数一致（与真实牌堆张数对上）
    Deck deck;
    deck.initStandardDeck();
    int sum = 0;
    for (const auto& e : catalog) sum += e.count;
    CHECK_EQ(sum, deck.getDrawPileSize());
    // 关键牌存在、有张数、有描述（含基本牌的补默认描述）
    const auto* sha = findEntry(catalog, "杀");
    CHECK(sha != nullptr && sha->count > 0 && !sha->desc.empty());
    if (sha) CHECK(sha->type == CardType::BASIC);
    const auto* wuzhong = findEntry(catalog, "无中生有");
    CHECK(wuzhong != nullptr && wuzhong->count == 4);
    if (wuzhong) {
        CHECK(wuzhong->type == CardType::TRICK);
        CHECK_EQ(wuzhong->desc, std::string("摸两张牌"));
    }
    const auto* lianu = findEntry(catalog, "诸葛连弩");
    CHECK(lianu != nullptr && lianu->count == 2);
    if (lianu) CHECK(lianu->type == CardType::EQUIPMENT);
    const auto* bagua = findEntry(catalog, "八卦阵");
    CHECK(bagua != nullptr && bagua->count == 2 && !bagua->desc.empty());
    // 分组顺序：基本→锦囊→装备（与牌堆初始化顺序一致）
    auto orderOf = [](CardType t) { return t == CardType::BASIC ? 0 : (t == CardType::TRICK ? 1 : 2); };
    for (size_t i = 1; i < catalog.size(); ++i) {
        CHECK(orderOf(catalog[i].type) >= orderOf(catalog[i - 1].type));
    }
}

TEST("codex/hotkeys") {
    CHECK(Codex::isCodexHotkey("t"));
    CHECK(Codex::isCodexHotkey("T"));
    CHECK(Codex::isCodexHotkey("v"));
    CHECK(Codex::isCodexHotkey("V"));
    CHECK(!Codex::isCodexHotkey(""));
    CHECK(!Codex::isCodexHotkey("0"));
    CHECK(!Codex::isCodexHotkey("1"));
    CHECK(!Codex::isCodexHotkey("y"));
    CHECK(!Codex::isCodexHotkey("v5")); // 选将界面的 v编号命令不受影响
    CHECK(!Codex::isCodexHotkey("tt"));
}
