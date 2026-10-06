#include "CardTracker.h"
#include "Codex.h"
#include "GameEngine.h"
#include "Interaction.h"
#include "Platform.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <tuple>

namespace Thks {
namespace {

std::string cardTypeLabel(CardType type) {
    switch (type) {
        case CardType::BASIC: return "基本牌";
        case CardType::TRICK: return "锦囊牌";
        case CardType::EQUIPMENT: return "装备牌";
        default: return "其他牌";
    }
}

} // namespace

bool CardTracker::isHotkey(const std::string& token) {
    return token == "m" || token == "M";
}

std::vector<CardTracker::Entry> CardTracker::buildEntries(const Deck& deck) {
    using Key = std::tuple<int, std::string, int, int>;
    std::map<Key, Entry> grouped;
    for (const auto& card : deck.getDrawPile()) {
        if (!card || card->isVirtual()) continue;
        const Key key{static_cast<int>(card->getType()), card->getName(),
                      static_cast<int>(card->getSuit()), card->getRank()};
        auto& entry = grouped[key];
        entry.name = card->getName();
        entry.formattedName = card->getFormattedName();
        entry.type = card->getType();
        entry.suit = card->getSuit();
        entry.rank = card->getRank();
        ++entry.count;
    }

    std::vector<Entry> entries;
    entries.reserve(grouped.size());
    for (const auto& pair : grouped) entries.push_back(pair.second);
    return entries;
}

void CardTracker::open(const GameEngine& engine, const std::string& /*currentHeroId*/) {
    const auto entries = buildEntries(engine.getDeck());
    const auto& pile = engine.getDeck().getDrawPile();
    const int total = static_cast<int>(pile.size());

    std::cout << "\n============================= 记牌器 =============================\n";
    std::cout << "当前牌堆剩余 " << total << " 张（仅列出牌堆中的实体牌）\n";
    for (CardType type : {CardType::BASIC, CardType::TRICK, CardType::EQUIPMENT}) {
        std::cout << "\n----- " << cardTypeLabel(type) << " -----\n";
        bool found = false;
        for (const auto& entry : entries) {
            if (entry.type != type) continue;
            found = true;
            // 与图鉴一致，以实体牌格式显示花色、点数和剩余张数。
            std::cout << "  " << Platform::padRight(entry.formattedName, 20)
                      << "×" << entry.count << "\n";
        }
        if (!found) std::cout << "  （无）\n";
    }
    std::cout << "\n记牌器不会改变对局状态。输入 0 退出并返回原提示";
    if (Interaction::inputClosed()) {
        std::cout << "（输入已关闭）";
    } else {
        std::cout << "（t/v 图鉴，c 局势）";
    }
    std::cout << "\n> " << std::flush;

    while (!Interaction::inputClosed()) {
        const std::string token = Interaction::readToken();
        if (token == "0" || Interaction::inputClosed()) break;
        if (Codex::isCodexHotkey(token)) {
            Codex::open();
            std::cout << "\n记牌器仍保持打开。输入 0 退出并返回原提示。\n> " << std::flush;
        } else if (Interaction::isGameStateHotkey(token)) {
            engine.printGameState();
            std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器\n> " << std::flush;
            // 嵌套的局势查看：同 Interaction 的模态，简易复用
            while (!Interaction::inputClosed()) {
                std::string inner = Interaction::readToken();
                if (inner == "0" || Interaction::inputClosed()) break;
                if (Codex::isCodexHotkey(inner)) {
                    Codex::open();
                    std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器\n> " << std::flush;
                } else if (CardTracker::isHotkey(inner)) {
                    CardTracker::open(engine, "");
                    std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器\n> " << std::flush;
                } else if (Interaction::isGameStateHotkey(inner)) {
                    engine.printGameState();
                    std::cout << "\n[局势查看] 输入 0 返回 | t/v 图鉴 | m 记牌器\n> " << std::flush;
                } else {
                    std::cout << "输入 0 返回局势查看。\n> " << std::flush;
                }
            }
            std::cout << "\n记牌器仍保持打开。输入 0 退出并返回原提示。\n> " << std::flush;
        } else {
            std::cout << "输入 0 退出记牌器。\n> " << std::flush;
        }
    }
    if (!Interaction::inputClosed()) std::cout << "已退出记牌器，回到对局。\n";
}

} // namespace Thks
