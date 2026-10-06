#include "test_helpers.h"
#include "CardTracker.h"
#include "Codex.h"
#include "Interaction.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>

using namespace Thks;

namespace {

int physicalEntriesCount(const std::vector<CardTracker::Entry>& entries) {
    int total = 0;
    for (const auto& entry : entries) total += entry.count;
    return total;
}

struct CinCoutRedirect {
    std::streambuf* oldCin;
    std::streambuf* oldCout;
    std::istringstream input;
    std::ostringstream output;

    explicit CinCoutRedirect(const std::string& text)
        : oldCin(std::cin.rdbuf()), oldCout(std::cout.rdbuf()), input(text) {
        std::cin.clear();
        std::cout.flush();
        std::cin.rdbuf(input.rdbuf());
        std::cout.rdbuf(output.rdbuf());
        Interaction::resetInputState();
    }

    ~CinCoutRedirect() {
        std::cin.rdbuf(oldCin);
        std::cout.rdbuf(oldCout);
        std::cin.clear();
        Interaction::resetInputState();
    }
};

} // namespace

TEST("card_tracker/mantian_count_and_card_codex_show_complete_rule_text") {
    Deck deck;
    deck.initStandardDeck();
    auto entries=CardTracker::buildEntries(deck);
    int mantianRemaining=0, mantianVariants=0;
    for(const auto& entry:entries)if(entry.name=="瞒天过海") {
        mantianRemaining+=entry.count;
        ++mantianVariants;
    }
    CHECK_EQ(mantianRemaining,4);
    CHECK_EQ(mantianVariants,4); // 四种花色分别按实体牌条目呈现。

    auto catalog=Codex::buildCardCatalog();
    auto card=std::find_if(catalog.begin(),catalog.end(),[](const Codex::CardEntry& entry) {
        return entry.name=="瞒天过海";
    });
    CHECK(card!=catalog.end());
    if(card!=catalog.end()) {
        CHECK_EQ(card->count,4);
        CHECK(card->desc.find("不计入手牌上限")!=std::string::npos);
        CHECK(card->desc.find("至多两名")!=std::string::npos);
        CHECK(card->desc.find("依次交给")!=std::string::npos);
    }
}

TEST("card_tracker/counts_only_physical_cards_remaining_in_draw_pile") {
    Deck deck;
    deck.initStandardDeck();
    const int initialCount = deck.getDrawPileSize();

    auto before = CardTracker::buildEntries(deck);
    CHECK_EQ(physicalEntriesCount(before), initialCount);
    CHECK(!before.empty());
    for (size_t i = 1; i < before.size(); ++i)
        CHECK(static_cast<int>(before[i - 1].type) <= static_cast<int>(before[i].type));

    const auto drawn = deck.drawCards(7);
    auto after = CardTracker::buildEntries(deck);
    CHECK_EQ(physicalEntriesCount(after), initialCount - static_cast<int>(drawn.size()));

    // 虚拟牌不属于实体牌库，不应被记入各牌名/花色/点数的剩余张数。
    auto virtualSha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA,
                                        {makeCard("桃", Suit::HEART, 1, CardType::BASIC, CardSubType::TAO)},
                                        "测试转化");
    deck.putOnTop({virtualSha});
    auto withVirtual = CardTracker::buildEntries(deck);
    CHECK_EQ(physicalEntriesCount(withVirtual), initialCount - static_cast<int>(drawn.size()));
}

TEST("card_tracker/hotkeys_and_codex_style_modal_navigation") {
    CHECK(CardTracker::isHotkey("m"));
    CHECK(CardTracker::isHotkey("M"));
    CHECK(!CardTracker::isHotkey(""));
    CHECK(!CardTracker::isHotkey("t"));
    CHECK(!CardTracker::isHotkey("m2"));

    GameEngine engine;
    auto log = captureLog(engine);
    engine.initGame(2, 0, {"xuyou", "guanyu"});
    const int drawPileBefore = engine.getDeck().getDrawPileSize();
    CinCoutRedirect io("t 0 0");
    CardTracker::open(engine, "xuyou");
    const std::string shown = io.output.str();
    CHECK(shown.find("记牌器") != std::string::npos);
    CHECK(shown.find("卡牌图鉴") != std::string::npos);
    CHECK(shown.find("输入 0 退出") != std::string::npos);
    CHECK_EQ(engine.getDeck().getDrawPileSize(), drawPileBefore);
    (void)log;
}

TEST("card_tracker/hotkey_returns_to_the_original_input_prompt") {
    GameEngine engine;
    auto sink = std::make_shared<std::ostringstream>();
    auto logger = std::make_shared<Logger>(sink);
    engine.setLogger(logger);
    engine.initGame(2, 0, {"xuyou", "guanyu"});
    Interaction ui(engine, *logger);

    CinCoutRedirect io("m 0 continue");
    CHECK_EQ(ui.readTokenWithCodex("xuyou"), std::string("continue"));
    CHECK(io.output.str().find("记牌器") != std::string::npos);
}

// 插入式面板（图鉴/记牌器/局势）返回后：恢复状态并重新输出原问题（TODO 第 3 项）。
TEST("card_tracker/modal_return_reprints_original_question") {
    GameEngine engine;
    auto sink = std::make_shared<std::ostringstream>();
    auto logger = std::make_shared<Logger>(sink);
    engine.setLogger(logger);
    engine.initGame(2, 0, {"xuyou", "guanyu"});
    Interaction ui(engine, *logger);

    CinCoutRedirect io("v 0 2"); // 打开图鉴 → 退出图鉴 → 选第 2 项
    int choice = ui.askChooseOption(engine.getPlayers()[0], {"甲的选项", "乙的选项"}, "测试问题：请选择", 0);
    CHECK_EQ(choice, 1);
    const std::string out = sink->str();
    size_t first = out.find("测试问题：请选择");
    CHECK(first != std::string::npos);
    // 退出图鉴后重印原问题（同一问题至少出现两次）
    CHECK(out.find("测试问题：请选择", first + 1) != std::string::npos);
    CHECK(out.find("乙的选项") != std::string::npos);
}

// 记牌器返回后同样重印原问题。
TEST("card_tracker/tracker_return_reprints_original_question") {
    GameEngine engine;
    auto sink = std::make_shared<std::ostringstream>();
    auto logger = std::make_shared<Logger>(sink);
    engine.setLogger(logger);
    engine.initGame(2, 0, {"xuyou", "guanyu"});
    Interaction ui(engine, *logger);

    CinCoutRedirect io("m 0 1"); // 打开记牌器 → 退出 → 选第 1 项
    int choice = ui.askChooseOption(engine.getPlayers()[0], {"选项一", "选项二"}, "又一问题", 0);
    CHECK_EQ(choice, 0);
    const std::string out = sink->str();
    size_t first = out.find("又一问题");
    CHECK(first != std::string::npos);
    CHECK(out.find("又一问题", first + 1) != std::string::npos);
}
