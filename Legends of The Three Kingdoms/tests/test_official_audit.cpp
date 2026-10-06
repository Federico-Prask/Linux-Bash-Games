// 官网技能复核（2026-09）回归测试：每项对应 docs/official_skill_audit.md 中记录的差异修正。
#include "test_helpers.h"
#include "HeroRegistry.h"
#include "SkillsStd.h"
#include "Skills.h"
#include "SkillsMyth.h"
#include "SkillsMou.h"
#include "Interaction.h"
#include "AI.h"
#include <algorithm>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <set>
#include <filesystem>
#include <cstdint>

namespace {
SkillPtr skillOf(PlayerPtr p, const std::string& name) {
    return p->getHero() ? p->getHero()->findSkill(name) : nullptr;
}
bool inDiscard(GameEngine& e, CardPtr c) {
    if (!e.getDeck().removeDiscardCard(c)) return false;
    e.getDeck().discardCard(c);
    return true;
}
} // namespace

TEST("audit/every_registered_skill_has_a_verified_description") {
    for (const auto& info : HeroRegistry::all()) {
        auto hero = info.create();
        CHECK(hero != nullptr);
        if (!hero) continue;
        for (const auto& skill : hero->getSkills()) {
            CHECK(!skill->getDescription().empty());
            CHECK(skill->getDescription().find("待核对") == std::string::npos);
        }
    }
}

TEST("audit/myth_descriptions_are_official_wording") {
    auto caopi = HeroRegistry::create("caopi");
    CHECK_EQ(caopi->findSkill("放逐")->getDescription(),
             std::string("当你受到伤害后，你可以令一名其他角色翻面，然后该角色摸X张牌（X为你已损失的体力值）。"));
    auto xiaoqiao = HeroRegistry::create("xiaoqiao");
    CHECK_EQ(xiaoqiao->findSkill("红颜")->getDescription(),
             std::string("锁定技，你的黑桃手牌只能当做红桃牌使用、打出、弃置或交给其他角色。你的黑桃判定牌只能当做红桃判定牌。"));
    auto zhoutai = HeroRegistry::create("zhoutai");
    CHECK(!zhoutai->findSkill("不屈")->isLocked());
    auto dengai = HeroRegistry::create("dengai");
    CHECK(!dengai->findSkill("屯田")->isLocked());
    auto shenZhuge = HeroRegistry::create("shen_zhugeliang");
    CHECK(!shenZhuge->findSkill("七星")->isLocked());
    auto jiaxu = HeroRegistry::create("jiaxu");
    CHECK(jiaxu->findSkill("完杀")->isLocked());
    CHECK(jiaxu->findSkill("帷幕")->isLocked());
}

TEST("audit/yiji_views_top_two_cards_then_gives_them") {
    auto guojia = HeroRegistry::create("guojia");
    auto desc = guojia->findSkill("遗计")->getDescription();
    CHECK(desc.find("观看牌堆顶的两张牌") != std::string::npos);
    CHECK(desc.find("摸两张牌") == std::string::npos);
    GameEngine e; auto log = captureLog(e); e.initGame(2, -1, {"guojia", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    e.applyDamage(ps[1], ps[0], 1);
    CHECK_EQ(ps[0]->getHandCardCount(), 2); // AI 将观看的两张牌交给自己
    CHECK(log->str().find("观看牌堆顶的两张牌") != std::string::npos);
}

TEST("audit/luoyi_does_not_boost_damage_when_xuchu_is_the_duel_target") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"xuchu", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    int draw = 2; skillOf(ps[0], "裸衣")->onDrawCards(e, *ps[0], draw);
    CHECK_EQ(draw, 1);
    // 张飞对许褚使用决斗，许褚打出杀后张飞无杀受伤：伤害不是许褚“使用决斗”造成的。
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    auto duel = makeCard("决斗", Suit::SPADE, 1, CardType::TRICK, CardSubType::JUE_DOU);
    ps[1]->addHandCard(duel);
    int hp = ps[1]->getHp();
    CHECK(e.useCard(ps[1], duel, {ps[0]}));
    CHECK_EQ(ps[1]->getHp(), hp - 1);
    CHECK(e.getActiveDuelUser() == nullptr);
}

TEST("audit/luoyi_boosts_duel_used_by_xuchu_and_sha") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"xuchu", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    int draw = 2; skillOf(ps[0], "裸衣")->onDrawCards(e, *ps[0], draw);
    auto duel = makeCard("决斗", Suit::SPADE, 1, CardType::TRICK, CardSubType::JUE_DOU);
    ps[0]->addHandCard(duel);
    int hp = ps[1]->getHp();
    CHECK(e.useCard(ps[0], duel, {ps[1]}));
    CHECK_EQ(ps[1]->getHp(), hp - 2);
}

TEST("audit/lijian_virtual_duel_has_no_subcards_so_jianxiong_cannot_take_cost") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"diaochan", "caocao", "zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto cost = makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(cost);
    int total = ps[1]->getHp() + ps[2]->getHp();
    skillOf(ps[0], "离间")->activate(e, *ps[0]);
    CHECK_EQ(ps[1]->getHp() + ps[2]->getHp(), total - 1);
    CHECK(!ps[1]->hasHandCard(cost));
    CHECK(!ps[2]->hasHandCard(cost));
    CHECK(inDiscard(e, cost));
}

TEST("audit/lijian_never_targets_kongcheng_zhugeliang") {
    for (unsigned seed = 0; seed < 8; ++seed) {
        GameEngine e; e.setSeed(seed); captureLog(e); e.initGame(3, -1, {"diaochan", "zhugeliang", "zhangfei"}); e.setPhase(TurnPhase::PLAY);
        auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
        auto cost = makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
        ps[0]->addHandCard(cost);
        int zhugeHp = ps[1]->getHp(), zhangfeiHp = ps[2]->getHp();
        skillOf(ps[0], "离间")->activate(e, *ps[0]);
        CHECK_EQ(ps[1]->getHp(), zhugeHp);
        // 只要付出了代价，决斗必然是诸葛亮对张飞。
        if (!ps[0]->hasHandCard(cost)) CHECK_EQ(ps[2]->getHp(), zhangfeiHp - 1);
    }
}

TEST("audit/jizhi_triggers_when_using_wuxie") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"huangyueying", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto wuxie = makeCard("无懈可击", Suit::SPADE, 11, CardType::TRICK, CardSubType::WU_XIE_KE_JI);
    auto keep = makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO);
    ps[0]->addHandCard(wuxie); ps[0]->addHandCard(keep);
    auto chai = makeCard("过河拆桥", Suit::SPADE, 3, CardType::TRICK, CardSubType::GUO_HE_CHAI_QIAO);
    ps[1]->addHandCard(chai);
    e.useCard(ps[1], chai, {ps[0]});
    CHECK(!ps[0]->hasHandCard(wuxie));
    CHECK(ps[0]->hasHandCard(keep));
    CHECK_EQ(ps[0]->getHandCardCount(), 2); // 桃 + 集智摸的一张
}

TEST("audit/jizhi_does_not_trigger_for_delayed_tricks") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"huangyueying", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto le = makeCard("乐不思蜀", Suit::HEART, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU);
    ps[0]->addHandCard(le);
    CHECK(e.useCard(ps[0], le, {ps[1]}));
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
}

TEST("audit/tieqi_suppresses_unlocked_buqu") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"zhoutai", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    ps[0]->setNonLockSkillsDisabled(true);
    ps[0]->changeHp(-(ps[0]->getHp() - 1));
    e.applyDamage(ps[1], ps[0], 1);
    CHECK(!ps[0]->isAlive());
}

// 标准包（及原版黄忠）技能描述逐字对照官网 https://www.sanguosha.cn/hero-detail-{1..26}.html
TEST("audit/standard_descriptions_match_official_site_verbatim") {
    const std::map<std::string, std::string> official = {
        {"仁德", "出牌阶段，你可以将任意张手牌交给其他角色，然后若你于此阶段内给出第二张“仁德”牌时，你回复1点体力。"},
        {"激将", "主公技，当你需要使用或打出【杀】时，你可以令其他蜀势力角色选择是否打出一张【杀】（视为由你使用或打出）。"},
        {"观星", "准备阶段，你可以观看牌堆顶的X张牌（X为全场角色数且最多为5），然后将其中任意数量的牌置于牌堆顶，将其余的牌置于牌堆底。"},
        {"空城", "锁定技，若你没有手牌，则你不能被选择为【杀】或【决斗】的目标。"},
        {"集智", "当你使用一张普通锦囊牌时，你可以摸一张牌。"},
        {"奇才", "锁定技，你使用锦囊牌无距离限制；"},
        {"奸雄", "当你受到伤害后，你可以获得造成此伤害的牌。"},
        {"护驾", "主公技，当你需要使用或打出【闪】时，你可以令其他魏势力角色选择是否打出一张【闪】（视为由你使用或打出）。"},
        {"反馈", "当你受到伤害后，你可以获得伤害来源的一张牌。"},
        {"鬼才", "当一名角色的判定牌生效前，你可以打出一张手牌代替之。"},
        {"刚烈", "当你受到伤害后，你可以进行判定，若结果不为红桃，则伤害来源选择一项：1.弃置两张手牌；2.受到你造成的1点伤害。"},
        {"突袭", "摸牌阶段，你可以改为获得至多两名角色的各一张手牌。"},
        {"裸衣", "摸牌阶段，你可以少摸一张牌，然后本回合你使用【杀】或【决斗】造成伤害时，此伤害+1。"},
        {"洛神", "准备阶段，你可以进行判定，当黑色判定牌生效后，你获得之。若结果为黑色，你可以重复此流程。"},
        {"倾国", "你可以将一张黑色手牌当【闪】使用或打出。"},
        {"急救", "你的回合外，你可以将一张红色牌当【桃】使用。"},
        {"青囊", "出牌阶段限一次，你可以弃置一张手牌，然后令一名已受伤的角色回复1点体力。"},
        {"无双", "锁定技，你使用的【杀】需两张【闪】才能抵消；与你进行【决斗】的角色每次需打出两张【杀】。"},
        {"咆哮", "锁定技，你使用【杀】无次数限制。"},
        {"龙胆", "你可以将一张【杀】当【闪】、【闪】当【杀】使用或打出。"},
        {"马术", "锁定技，你计算与其他角色的距离-1。"},
        {"铁骑", "当你使用【杀】指定一个目标后，你可以进行判定，若结果为红色，该角色不能使用【闪】响应此【杀】。"},
        {"制衡", "出牌阶段限一次，你可以弃置任意张牌，然后摸等量的牌。"},
        {"救援", "主公技，锁定技，其他吴势力角色对你使用【桃】回复的体力+1。"},
        {"武圣", "你可以将一张红色牌当【杀】使用或打出。"},
        {"奇袭", "你可以将一张黑色牌当【过河拆桥】使用。"},
        {"克己", "若你未于出牌阶段内使用或打出过【杀】，则你可以跳过弃牌阶段。"},
        {"苦肉", "出牌阶段，你可以失去1点体力，然后摸两张牌。"},
        {"英姿", "摸牌阶段，你可以多摸一张牌。"},
        {"反间", "出牌阶段限一次，你可以令一名其他角色选择一种花色，然后该角色获得你的一张手牌并展示之，若此牌的花色与其所选的花色不同，则你对其造成1点伤害。"},
        {"谦逊", "锁定技，你不能被选择为【顺手牵羊】和【乐不思蜀】的目标。"},
        {"连营", "当你失去最后的手牌时，你可以摸一张牌。"},
        {"天妒", "当你的判定牌生效后，你可以获得此牌。"},
        {"遗计", "当你受到1点伤害后，你可以观看牌堆顶的两张牌，然后将这些牌交给任意角色。"},
        {"闭月", "结束阶段，你可以摸一张牌。"},
        {"离间", "出牌阶段限一次，你可以弃置一张牌并选择两名其他男性角色，然后令其中一名男性角色视为对另一名男性角色使用一张【决斗】。"},
        {"枭姬", "当你失去装备区里的一张牌时，你可以摸两张牌。"},
        {"结姻", "出牌阶段限一次，你可以弃置两张手牌并选择一名已受伤的男性角色，然后你与其各回复1点体力。"},
        {"烈弓", "当你于出牌阶段内使用【杀】指定一个目标后，若该角色的手牌数不小于你的体力值或不大于你的攻击范围，则你可以令其不能使用【闪】响应此【杀】。"},
        {"国色", "你可以将一张方块牌当【乐不思蜀】使用。"},
        {"流离", "当你成为【杀】的目标时，你可以弃置一张牌并选择你攻击范围内的一名其他角色，然后将此【杀】转移给该角色。"},
    };
    const std::vector<std::string> ids = {"liubei","guanyu","zhangfei","zhugeliang","zhaoyun","machao","huangyueying",
        "sunquan","ganning","lvmeng","huanggai","zhouyu","daqiao","luxun","caocao","simayi","xiahoudun","zhangliao",
        "xuchu","guojia","zhenji","huatuo","lvbu","diaochan","sunshangxiang","huangzhong"};
    int checked = 0;
    for (const auto& id : ids) {
        auto hero = HeroRegistry::create(id);
        CHECK(hero != nullptr);
        if (!hero) continue;
        for (const auto& skill : hero->getSkills()) {
            auto it = official.find(skill->getName());
            CHECK(it != official.end());
            if (it == official.end()) continue;
            // 官网原文有明显错别字/标点异常时，描述末尾附“（官网原文如此）”（用户 2026-10-04 裁定 C）。
            std::string got = skill->getDescription();
            const std::string typoSuffix = "（官网原文如此）";
            if (got.size() > typoSuffix.size() &&
                got.compare(got.size() - typoSuffix.size(), typoSuffix.size(), typoSuffix) == 0)
                got = got.substr(0, got.size() - typoSuffix.size());
            CHECK_EQ(got, it->second);
            ++checked;
        }
    }
    CHECK_EQ(checked, (int)official.size());
}

// 附录 B（风火林山、神武将、界黄忠、钟会/界钟会、蔡文姬）官网原文逐字比对。
// 附录以“- 页面 武将 技能: 原文”逐行记录；官网弯引号在比对前统一归一为直引号。
// 标注 partial 的技能按披露规则允许追加简化版说明后缀（当前 partial 为空：
// 无谋已按最终裁定完整实现并移出，其描述必须与官网原文完全一致）。
TEST("audit/nonstandard_descriptions_match_official_appendix") {
    auto normalizeQuotes = [](std::string s) {
        auto repl = [&s](const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
        };
        repl("\xE2\x80\x9C", "\""); // “
        repl("\xE2\x80\x9D", "\""); // ”
        return s;
    };
    std::ifstream in("docs/official_skill_audit.md");
    CHECK(in.good());
    int checked = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows CRLF 容错
        if (line.rfind("- ", 0) != 0 || line.size() < 3 || !std::isdigit((unsigned char)line[2])) continue;
        size_t sp1 = line.find(' ', 2);
        if (sp1 == std::string::npos) continue;
        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) continue;
        size_t colon = line.find(": ", sp2 + 1);
        if (colon == std::string::npos) continue;
        std::string heroName = line.substr(sp1 + 1, sp2 - sp1 - 1);
        std::string skillName = line.substr(sp2 + 1, colon - sp2 - 1);
        std::string official = normalizeQuotes(line.substr(colon + 2));

        HeroPtr hero = nullptr;
        for (const auto& info : HeroRegistry::all())
            if (info.name == heroName) { hero = info.create(); break; }
        CHECK(hero != nullptr);
        if (!hero) continue;
        auto skill = hero->findSkill(skillName);
        if (!skill) {
            // 非标包技能名带包前缀（界-/一-/diy-/谋-/势-）：按前缀候选回退查找。
            for (const char* pfx : {"界-", "一-", "diy-", "谋-", "势-"}) {
                skill = hero->findSkill(std::string(pfx) + skillName);
                if (skill) break;
            }
        }
        if (!skill) {
            // 由觉醒技获得的技能不在注册技能列表中：按游戏内同一构造来源取描述。
            if (skillName == "排异") skill = std::make_shared<PaiYiSkill>();
            else if (skillName == "夺荆") skill = std::make_shared<MouDuoJingSkill>();
            else if (skillName == "英姿※" || skillName == "英姿") skill = std::make_shared<MouXingYingZiSkill>();
            else if (skillName == "英魂※" || skillName == "英魂") skill = std::make_shared<MouXingYingHunSkill>();
            else if (skillName == "观星※" || skillName == "观星") skill = std::make_shared<MouGuanXingXinSkill>();
            else if (skillName == "空城※" || skillName == "空城") skill = std::make_shared<MouKongChengXinSkill>();
            else if (skillName == "排异※") skill = std::make_shared<PaiYiSkill>(true);
            else {
                auto granted = createMythSkills(skillName.c_str());
                if (!granted.empty() && granted[0]->getDescription() != "技能资料待核对。") skill = granted[0];
            }
        }
        if (!skill) {
            std::cerr << " [附录B缺技能] " << heroName << "·" << skillName << std::endl;
        }
        CHECK(skill != nullptr);
        if (!skill) continue;
        std::string got = normalizeQuotes(skill->getDescription());
        const std::string partialSuffix = "（当前结算为简化版，尚未覆盖官方全部细则）";
        // 官网原文存在明显错字/漏字时，描述照录并在末尾追加“（官网原文如此）”（用户 2026-10-04 裁定 C）。
        const std::string typoSuffix = "（官网原文如此）";
        if (got != official && !(got == official + partialSuffix) && !(got == official + typoSuffix)) {
            std::cerr << " [附录B差异] " << heroName << "·" << skillName << "\n   官网: " << official
                      << "\n    代码: " << got << std::endl;
        }
        CHECK(got == official || got == official + partialSuffix || got == official + typoSuffix);
        ++checked;
    }
    CHECK(checked >= 80); // 解析覆盖度：附录 B 现有 86 条
}

// ==================== 官网复核修复回归 + 技能覆盖 ====================
namespace {
// 控制真人席位的交互输入（析构时还原 cin，与 test_myth_rules 的 MythInput 等价）
struct AuditInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit AuditInput(const std::string& text)
        : script(text), saved(std::cin.rdbuf(script.rdbuf())) { Interaction::resetInputState(); }
    ~AuditInput() { std::cin.rdbuf(saved); Interaction::resetInputState(); }
};
int countSub(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (size_t pos = 0; (pos = hay.find(needle, pos)) != std::string::npos; pos += needle.size()) ++n;
    return n;
}
} // namespace

// 【乱击】官网无“每回合一次”限制：首用后仍可再次发动，消费第二对手牌。
TEST("audit/luanji_has_no_per_turn_activation_limit") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"yuanshao", "zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("乱击");
    ps[0]->addHandCard(makeCard("杀", Suit::CLUB, 3, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("杀", Suit::CLUB, 4, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 6, CardType::BASIC, CardSubType::SHA));
    int hp = ps[1]->getHp();
    CHECK(sk->canActivate(e, *ps[0]));
    sk->activate(e, *ps[0]);
    CHECK_EQ(ps[1]->getHp(), hp - 1);                 // 第一次万箭齐发命中
    CHECK(sk->canActivate(e, *ps[0]));                // 回归：官网未限次，首用后仍可发动
    sk->activate(e, *ps[0]);
    CHECK_EQ(ps[1]->getHp(), hp - 2);                 // 第二次同样结算
    CHECK_EQ(ps[0]->getHandCardCount(), 0);           // 两对手牌均已消耗
}

// 【直谏】官网无“每回合一次”限制：每张装备牌都可依次直谏。
TEST("audit/zhijian_has_no_per_turn_activation_limit") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"zhangzhao_zhanghong", "zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("直谏");
    auto w1 = makeCard("诸葛连弩", Suit::DIAMOND, 1, CardType::EQUIPMENT, CardSubType::WEAPON);
    auto w2 = makeCard("青龙偃月刀", Suit::SPADE, 12, CardType::EQUIPMENT, CardSubType::WEAPON);
    ps[0]->addHandCard(w1); ps[0]->addHandCard(w2);
    // 预放两张基本牌作摸牌，保证两次发动的装备候选唯一。
    e.getDeck().putOnTop({makeCard("杀", Suit::HEART, 9, CardType::BASIC, CardSubType::SHA),
                          makeCard("闪", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHAN)});
    CHECK(sk->canActivate(e, *ps[0]));
    sk->activate(e, *ps[0]);
    CHECK(!ps[0]->hasHandCard(w1));
    CHECK(sk->canActivate(e, *ps[0]));                // 回归：首用后仍可发动
    sk->activate(e, *ps[0]);
    CHECK(!ps[0]->hasHandCard(w2));                   // 第二张装备也已直谏出去
    CHECK_EQ(ps[1]->getWeapon(), w2);                 // 装备进入目标装备区（替换时旧武器弃置）
}

// 【乱武】目标中途阵亡后，其余角色仍须逐个继续结算（不能 break 中断）。
TEST("audit/luanwu_resolves_remaining_targets_after_one_dies") {
    bool verified = false;
    for (unsigned seed = 0; seed < 60 && !verified; ++seed) {
        GameEngine e; captureLog(e); e.setSeed(seed);
        e.initGame(4, -1, {"jiaxu", "zhangfei", "zhouyu", "zhangliao"}); e.setPhase(TurnPhase::PLAY);
        auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
        // 需要 p2（乱武第 2 名受害者）是忠臣：其阵亡不直接结束游戏（座位 0 恒为主公）。
        if (ps[2]->getIdentity() != Identity::ZHONG_CHEN) continue;
        ps[0]->equip(makeCard("绝影", Suit::SPADE, 5, CardType::EQUIPMENT, CardSubType::DEFENSIVE_HORSE));
        ps[2]->setHp(1); // p1 的最近目标锁定为 p2，可被一张杀带走
        ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        int p3hp = ps[3]->getHp();
        auto sk = ps[0]->getHero()->findSkill("乱武");
        CHECK(sk->canActivate(e, *ps[0]));
        sk->activate(e, *ps[0]);
        CHECK(!ps[2]->isAlive());                     // 第 2 名受害者阵亡
        CHECK_EQ(ps[3]->getHp(), p3hp - 1);           // 回归：第 3 名角色仍被结算（旧实现会漏掉）
        verified = true;
    }
    CHECK(verified); // 至少找到一个“p2=忠臣”的身份种子
}

// 【连破】额外回合报价只发生在击杀所在的该回合结束时，拒绝/授予都当场消费、不顺延。
TEST("audit/lianpo_does_not_reoffer_extra_turn_on_later_turn_ends") {
    GameEngine e; auto log = captureLog(e); e.initGame(2, -1, {"shen_simayi", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("连破");
    sk->onTurnStart(e, *ps[0]);
    sk->onPlayerDeath(e, *ps[0], *ps[1], ps[0].get()); // 击杀发生在 p1 的回合
    sk->onTurnEnd(e, *ps[0], *ps[1]);                  // 该回合结束：报价一次（AI 接受）
    sk->onTurnEnd(e, *ps[0], *ps[0]);                  // 之后的回合结束：不得再次报价
    log->str("");
    e.runTurn(ps[0]);                                  // 排队的额外回合在此结算
    auto text = log->str();
    auto first = text.find("获得一个额外回合！");
    CHECK(first != std::string::npos);
    CHECK(text.find("获得一个额外回合！", first + 1) == std::string::npos); // 回归：只获得一个
}

// 【遗计】按点数分别询问：拒绝第一点不影响第二点（真人交互）。
TEST("audit/yiji_invites_each_damage_point_independently") {
    GameEngine e; auto log = captureLog(e); e.initGame(2, 0, {"guojia", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    {
        AuditInput input("n\ny\n0\n0\n"); // 第 1 点拒绝；第 2 点观看，两张牌均交给自己
        e.applyDamage(ps[1], ps[0], 2);
    }
    CHECK_EQ(ps[0]->getHandCardCount(), 2);            // 回归：拒绝第一点后仍询问并结算第二点
    CHECK_EQ(countSub(log->str(), "发动【遗计】"), 1);
    CHECK(!ps[0]->isAlive() == false);
}

// 【替身】未造成伤害的【杀】获得之；造成伤害不获得；至自己下回合开始失效。
TEST("audit/tishen_gains_sha_dealt_no_damage_and_expires_next_turn") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"jie_zhangfei", "guanyu"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("界-替身");
    e.setPhase(TurnPhase::PLAY);                      // 模拟进行到出牌阶段
    sk->onPhaseEnd(e, *ps[0], TurnPhase::PLAY);        // 出牌阶段结束时发动（AI 确认）
    CHECK(ps[0]->isTiShenActive());

    // 被【闪】抵消：未造成伤害 → 获得该【杀】
    auto shan = makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    auto sha1 = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(shan); ps[1]->addHandCard(sha1);
    CHECK(e.useCard(ps[1], sha1, {ps[0]}));
    CHECK(ps[0]->hasHandCard(sha1));
    CHECK(!ps[0]->hasHandCard(shan));

    // 造成伤害：不获得
    auto sha2 = makeCard("杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(sha2);
    ps[1]->resetShaCount();
    int hp = ps[0]->getHp();
    CHECK(e.useCard(ps[1], sha2, {ps[0]}));
    CHECK_EQ(ps[0]->getHp(), hp - 1);
    CHECK(!ps[0]->hasHandCard(sha2));

    // 自己回合开始后失效
    sk->onTurnStart(e, *ps[0]);
    CHECK(!ps[0]->isTiShenActive());
    auto sha3 = makeCard("杀", Suit::DIAMOND, 9, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(sha3);
    ps[1]->resetShaCount();
    CHECK(e.useCard(ps[1], sha3, {ps[0]}));
    CHECK(!ps[0]->hasHandCard(sha3));
}

// 【涯角】回合外用牌展示顶牌：类别不同弃一张，类别相同不弃（2 人局恒为自己获得）。
TEST("audit/yajiao_discards_only_on_card_type_mismatch") {
    GameEngine e; auto log = captureLog(e); e.initGame(2, -1, {"jie_zhaoyun", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("界-涯角");
    auto used = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);

    // 异类别：展示装备 → 获得后弃置 → 手牌仍为空
    auto topEq = makeCard("八卦阵", Suit::DIAMOND, 2, CardType::EQUIPMENT, CardSubType::ARMOR);
    e.getDeck().putOnTop({topEq});
    sk->onCardUsedOutsideTurn(e, *ps[0], used);
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
    CHECK(log->str().find("涯角") != std::string::npos);

    // 同类别：展示基本牌 → 获得且不弃
    auto topBasic = makeCard("杀", Suit::CLUB, 5, CardType::BASIC, CardSubType::SHA);
    e.getDeck().putOnTop({topBasic});
    sk->onCardUsedOutsideTurn(e, *ps[0], used);
    CHECK(ps[0]->hasHandCard(topBasic));
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
}

// 【驱虎】赢：被拼者对其攻击范围内你选择的角色造成伤害；输：被拼者对你造成伤害；出牌阶段限一次。
TEST("audit/quhu_winner_deals_damage_from_challenger_range_and_loses_costs_self") {
    {   // 赢：荀彧 13 点拼掉典韦 2 点 → 受害者（座位最前的合法角色）掉血，荀彧无损
        GameEngine e; captureLog(e); e.initGame(3, -1, {"zhangfei", "xunyu", "dianwei"}); e.setPhase(TurnPhase::PLAY);
        auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
        ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 13, CardType::BASIC, CardSubType::SHA));
        ps[2]->addHandCard(makeCard("桃", Suit::HEART, 2, CardType::BASIC, CardSubType::TAO));
        auto sk = ps[1]->getHero()->findSkill("驱虎");
        int zhangfeiHp = ps[0]->getHp(), xunyuHp = ps[1]->getHp(), dianweiHp = ps[2]->getHp();
        CHECK(sk->canActivate(e, *ps[1]));
        sk->activate(e, *ps[1]);
        CHECK_EQ(ps[0]->getHp(), zhangfeiHp - 1);   // 攻击范围内的前排受害者掉血
        CHECK_EQ(ps[1]->getHp(), xunyuHp);          // 挑战者不掉血
        CHECK_EQ(ps[2]->getHp(), dianweiHp);        // 赢方（被拼者）不掉血
        CHECK(!sk->canActivate(e, *ps[1]));         // 出牌阶段限一次
    }
    {   // 输：拼点失利 → 被拼者对荀彧造成 1 点伤害
        GameEngine e; captureLog(e); e.initGame(3, -1, {"zhangfei", "xunyu", "dianwei"}); e.setPhase(TurnPhase::PLAY);
        auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
        ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 2, CardType::BASIC, CardSubType::SHA));
        ps[2]->addHandCard(makeCard("桃", Suit::HEART, 13, CardType::BASIC, CardSubType::TAO));
        auto sk = ps[1]->getHero()->findSkill("驱虎");
        int zhangfeiHp = ps[0]->getHp(), xunyuHp = ps[1]->getHp();
        sk->activate(e, *ps[1]);
        CHECK_EQ(ps[1]->getHp(), xunyuHp - 1);      // 未赢 → 荀彧受伤
        CHECK_EQ(ps[0]->getHp(), zhangfeiHp);       // 无关角色不掉血
    }
}

// 【涉猎】摸牌阶段改为亮出顶 5 张、每种花色各取一张。
TEST("audit/shelie_replaces_draw_with_one_card_per_suit") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shen_lvmeng", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("涉猎");
    // 顶 5 张：黑桃×2、红桃、梅花、方块 → 4 种花色，黑桃只取一张
    e.getDeck().putOnTop({
        makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA),
        makeCard("杀", Suit::SPADE, 10, CardType::BASIC, CardSubType::SHA),
        makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN),
        makeCard("杀", Suit::CLUB, 3, CardType::BASIC, CardSubType::SHA),
        makeCard("杀", Suit::DIAMOND, 5, CardType::BASIC, CardSubType::SHA)});
    int n = 2;
    sk->onDrawCards(e, *ps[0], n);
    CHECK_EQ(n, 0);
    CHECK_EQ(ps[0]->getHandCardCount(), 4);
}

// 【行殇】其他角色死亡时获得其所有牌（手牌与装备区一并获得）。
TEST("audit/xingshang_obtains_every_card_of_other_dead_character") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"caopi", "zhangfei", "guanyu"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto h = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto w = makeCard("青龙偃月刀", Suit::SPADE, 12, CardType::EQUIPMENT, CardSubType::WEAPON);
    ps[1]->addHandCard(h);
    ps[1]->equip(w);
    e.applyDamage(ps[0], ps[1], 99);                 // 曹丕（座位 0 恒为主公）击杀
    CHECK(!ps[1]->isAlive());
    CHECK(ps[0]->hasHandCard(h));
    CHECK(ps[0]->hasHandCard(w));
}

// 【烈刃】杀造成伤害后拼点，赢则获得目标一张牌。
TEST("audit/lieren_pindian_after_sha_gains_a_card_when_winning") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"zhurong", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sha = makeCard("杀", Suit::SPADE, 13, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(sha);
    ps[0]->addHandCard(makeCard("闪", Suit::DIAMOND, 5, CardType::BASIC, CardSubType::SHAN)); // 拼点备牌
    ps[1]->addHandCard(makeCard("桃", Suit::HEART, 2, CardType::BASIC, CardSubType::TAO));   // 无闪必中
    auto w = makeCard("青龙偃月刀", Suit::SPADE, 12, CardType::EQUIPMENT, CardSubType::WEAPON);
    ps[1]->equip(w);
    int hp = ps[1]->getHp();
    CHECK(e.useCard(ps[0], sha, {ps[1]}));
    CHECK_EQ(ps[1]->getHp(), hp - 1);                // 杀命中
    CHECK(!ps[1]->hasEquipment(w));                  // 拼点（13>2）赢 → 装备被曹…祝融获得
    CHECK(ps[0]->hasHandCard(w));
}

// 【断肠】杀死蔡文姬的角色失去所有武将技能。
// 注意（2026-10-04 裁定“胜利优先级大于一切”）：须在**不终止游戏**的死亡中验证，
// 否则胜负优先结算，死亡技能不发动。
TEST("audit/duanchang_killer_loses_all_hero_skills") {
    GameEngine e; captureLog(e);
    // 4 人：主公(关羽) / 反贼(蔡文姬) / 反贼(张飞) / 内奸(赵云)
    e.setupGame(4, -1, {"guanyu", "caiwenji", "zhangfei", "zhaoyun"},
                {Identity::ZHU_GONG, Identity::FAN_ZEI, Identity::FAN_ZEI, Identity::NEI_JIAN});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    CHECK(ps[2]->getHero()->findSkill("咆哮") != nullptr);
    ps[1]->setHp(1);
    e.applyDamage(ps[2], ps[1], 99);   // 张飞杀死蔡文姬；场上仍有反贼与内奸 → 游戏继续
    CHECK(!ps[1]->isAlive());
    CHECK(!e.isGameOver());
    CHECK(ps[2]->getHero()->getSkills().empty());    // 杀人者技能被清空（断肠发动）
}

// 规则（用户 2026-10-04）：“胜利优先级大于一切”——死亡一旦满足胜负条件立即结束，
// 不再触发“死亡时”技能与击杀奖励。
TEST("rules/victory_priority_over_death_skills_and_rewards") {
    GameEngine e; auto sink = captureLog(e);
    e.initGame(2, -1, {"guanyu", "caiwenji"}); // 2 人局：座位0 主公(关羽)、座位1 反贼(蔡文姬)
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    CHECK(ps[0]->getIdentity() == Identity::ZHU_GONG);
    CHECK(ps[1]->getIdentity() == Identity::FAN_ZEI);
    CHECK(ps[0]->getHero()->findSkill("武圣") != nullptr);
    int lordHand = ps[0]->getHandCardCount();
    ps[1]->setHp(1);
    e.applyDamage(ps[0], ps[1], 99); // 主公杀死最后一名反贼 → 立即胜利
    CHECK(!ps[1]->isAlive());
    CHECK(e.isGameOver());
    CHECK_EQ(e.getWinningFaction(), std::string("主公与忠臣胜"));
    // ① 胜利优先：断肠不发动（杀人者技能保留）
    CHECK(ps[0]->getHero()->findSkill("武圣") != nullptr);
    CHECK(!ps[0]->getHero()->getSkills().empty());
    // ② 胜利优先：击杀反贼的摸 3 奖励不再结算
    CHECK_EQ(ps[0]->getHandCardCount(), lordHand);
    CHECK(sink->str().find("胜利优先") != std::string::npos);
    // ③ 死亡后的区域清理仍完成（状态一致）
    CHECK_EQ(ps[1]->getHandCardCount(), 0);
    CHECK(ps[1]->getJudgeZone().empty());
}

// 规则（用户 2026-10-04）：“死亡后非特殊技能不发动”——已阵亡角色的技能不再生效。
TEST("rules/dead_player_skills_stop_triggering") {
    GameEngine e; captureLog(e);
    // 4 人：主公(关羽) / 反贼(势·钟会) / 反贼(张飞) / 内奸(赵云)
    e.setupGame(4, -1, {"guanyu", "shi_zhonghui", "zhangfei", "zhaoyun"},
                {Identity::ZHU_GONG, Identity::FAN_ZEI, Identity::FAN_ZEI, Identity::NEI_JIAN});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sizi = ps[1]->getHero()->findSkill("势-肆恣");
    CHECK(sizi != nullptr);
    if (!sizi) return;
    // 钟会发动肆恣：本局中所有角色使用【杀】伤害 +1
    sizi->onGameStart(e, *ps[1]);
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    auto act = std::dynamic_pointer_cast<ActiveSkill>(sizi);
    CHECK(act != nullptr);
    if (!act) return;
    act->activate(e, *ps[1]);
    e.setPhase(TurnPhase::NONE);
    // 未死亡时：引擎实际【杀】结算伤害 +1（张飞对主公的【杀】造成 2 点）
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[2]->addHandCard(sha);
    int hp0 = ps[3]->getHp();               // 目标取相邻座位（距离 1），确保【杀】可用
    CHECK(e.useCard(ps[2], sha, {ps[3]}));
    CHECK_EQ(ps[3]->getHp(), hp0 - 2);      // 肆恣：所有角色使用【杀】造成的伤害+1
    // 钟会阵亡（场上仍有反贼与内奸，游戏继续）
    e.killPlayer(ps[1], ps[0]);
    CHECK(!ps[1]->isAlive());
    CHECK(!e.isGameOver());
    // ① 死者的技能不再被引擎收集（非特殊技能不发动）
    CHECK(e.getEffectiveSkills(*ps[1]).empty());
    // ② 引擎广播路径不再采用死者的肆恣：实际【杀】结算伤害恢复为 1
    auto sha2 = makeCard("杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA);
    ps[2]->addHandCard(sha2);
    ps[3]->setHp(std::min(ps[3]->getMaxHp(), ps[3]->getHp() + 2)); // 先回复，避免结算溢出干扰
    int hpBefore = ps[3]->getHp();
    CHECK(e.useCard(ps[2], sha2, {ps[3]}));
    CHECK_EQ(ps[3]->getHp(), hpBefore - 1); // 无 +1（若死者肆恣仍生效则为 -2）
}

// 【若愚】仅当你是体力值最小的角色才觉醒：加 1 上限、回 1 体力、获得激将。
TEST("audit/ruoyu_awakens_only_when_lowest_and_grants_jijiang") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"liuchan", "zhangfei", "guanyu"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("若愚");
    bool skip = false;
    // 非最低：不觉醒
    ps[0]->setHp(4); ps[1]->setHp(3); ps[2]->setHp(4);
    sk->onPhaseStart(e, *ps[0], TurnPhase::PREPARATION, skip);
    CHECK(ps[0]->getHero()->findSkill("激将") == nullptr);
    CHECK_EQ(ps[0]->getMaxHp(), 4);
    // 最低：觉醒
    ps[0]->setHp(3);
    sk->onPhaseStart(e, *ps[0], TurnPhase::PREPARATION, skip);
    CHECK(ps[0]->getHero()->findSkill("激将") != nullptr);
    CHECK_EQ(ps[0]->getMaxHp(), 5);
    CHECK_EQ(ps[0]->getHp(), 4);
}

// 【鬼道】官网 FAQ：判定【八卦阵】自身时不能用正在判定的这张装备牌替换它。
TEST("audit/guidao_cannot_expend_the_eight_trigrams_being_judged") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"zhangjiao", "zhangfei"});
    auto ps = e.getPlayers(); for (auto p : ps) clearHand(*p);
    auto bagua = makeCard("八卦阵", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::ARMOR);
    ps[0]->equip(bagua);                              // 黑桃装备：修复前会被鬼道换走
    auto sha = makeCard("杀", Suit::CLUB, 7, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(sha);
    // 顶牌放黑色：判定直接失败，不会经“视为打出【闪】→雷击”链路引入其它判定干扰。
    e.getDeck().putOnTop({makeCard("杀", Suit::SPADE, 12, CardType::BASIC, CardSubType::SHA)});
    CHECK(e.useCard(ps[1], sha, {ps[0]}));            // 触发【八卦阵】判定（手牌中无其他黑牌）
    CHECK_EQ(ps[0]->getArmor(), bagua);               // 回归：装备八卦阵未被用于替换它自己的判定
}

// 反向覆盖：除标准包（附录 A 逐字）与 DIY 包（自有设计）外，登记表中每名武将的每个技能
// 都必须在附录 B 有官网原文条目——保证“每名武将均被核对”，新登记武将若未补录会立刻失败。
TEST("audit/all_nonstandard_registry_skills_are_verbatim_covered") {
    std::set<std::string> appendixSkills;
    std::ifstream in("docs/official_skill_audit.md");
    CHECK(in.good());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows CRLF 容错
        if (line.rfind("- ", 0) != 0 || line.size() < 3 || !std::isdigit((unsigned char)line[2])) continue;
        size_t sp1 = line.find(' ', 2);
        if (sp1 == std::string::npos) continue;
        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) continue;
        size_t colon = line.find(": ", sp2 + 1);
        if (colon == std::string::npos) continue;
        appendixSkills.insert(line.substr(sp2 + 1, colon - sp2 - 1));
    }
    // 谋攻篇：附录C（docs/mou_appendix.md）逐字原文，格式 “- 技能名：原文”。
    std::ifstream in2("docs/mou_appendix.md");
    CHECK(in2.good());
    while (std::getline(in2, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows CRLF 容错
        if (line.rfind("- ", 0) != 0) continue;
        size_t full = line.find("：");
        if (full == std::string::npos || full < 2) continue;
        appendixSkills.insert(line.substr(2, full - 2));
        if (line.find(": ") != std::string::npos) {
            size_t en = line.find(": ");
            appendixSkills.insert(line.substr(2, en - 2));
        }
    }
    // 势系/友系/花鬘：附录E（docs/shi_you_appendix.md）逐字原文，格式 “- 技能名：原文”。
    std::ifstream in3("docs/shi_you_appendix.md");
    CHECK(in3.good());
    while (std::getline(in3, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows CRLF 容错
        if (line.rfind("- ", 0) != 0) continue;
        size_t full = line.find("：");
        if (full == std::string::npos || full < 2) continue;
        appendixSkills.insert(line.substr(2, full - 2));
    }
    int checkedSkills = 0;
    for (const auto& info : HeroRegistry::all()) {
        if (info.pack == "标准包" || info.pack == "DIY包" || info.id == "huangzhong") continue;
        auto hero = info.create();
        CHECK(hero != nullptr);
        if (!hero) continue;
        for (const auto& skill : hero->getSkills()) {
            ++checkedSkills;
            // 技能对象名带包前缀（谋-/界-/一-/diy-/势-），比对时按官网原名回查附录。
            std::string base = skill->getName();
            size_t dash = base.find('-');
            if (dash != std::string::npos && dash <= 3) base = base.substr(dash + 1);
            if (!appendixSkills.count(base))
                std::cerr << " [未纳入官网逐字核对] " << info.name << "·" << skill->getName() << std::endl;
            CHECK(appendixSkills.count(base) > 0);
        }
    }
    std::cerr << " [覆盖统计] 非标准武将技能数=" << checkedSkills << " 附录B条目数=" << appendixSkills.size() << std::endl;
    CHECK(checkedSkills >= 90); // 风火林山 32 + 神 8 + 一将 + 界 6 等全部非标准武将技能
}

// 【无谋】已按最终裁定完整实现（杀对应原锦囊的目标、形态任意；无懈转化的杀无目标不能用于使用），
// 移出 partial：描述必须与官网原文完全一致，不再携带“简化版”后缀。
TEST("audit/wumou_fully_implemented_per_ruling_without_partial_suffix") {
    auto lvbu = HeroRegistry::create("shen_lvbu");
    CHECK(lvbu != nullptr);
    auto skill = lvbu->findSkill("无谋");
    CHECK(skill != nullptr);
    CHECK_EQ(skill->getDescription(),
             std::string("锁定技，你的普通锦囊牌只能当普通【杀】使用或打出，此【杀】的目标改为原锦囊的目标。"));
}

// 日志区体积上限（用户 2026-10-04）：logs/ 为滚动日志，整目录不超过 100KB；
// 超出即应压缩旧日志（见 logs/README.md）。资料性质的内容移入 docs/（资料区）。
TEST("docs/logs_size_within_limit") {
    const std::uintmax_t kLimit = 100 * 1024; // 100KB
    std::uintmax_t total = 0;
    CHECK(std::filesystem::exists("logs"));
    for (auto& entry : std::filesystem::recursive_directory_iterator("logs")) {
        if (entry.is_regular_file()) total += entry.file_size();
    }
    CHECK(total <= kLimit);
    if (total > kLimit)
        std::cerr << " [logs 体积超限] " << total << " > " << kLimit << "，请压缩旧日志\n";
}

// docs/ 为资料区：官网逐字附录等基准文件必须长期保留（今后新增资料也存此处）。
TEST("docs/reference_materials_present") {
    CHECK(std::filesystem::exists("docs/official_skill_audit.md"));
    CHECK(std::filesystem::exists("docs/mou_appendix.md"));
    CHECK(std::filesystem::exists("docs/shi_you_appendix.md"));
    // 2026-10-05 新增资料（用户指示：AI 待办与至尊场分档都写进 docs 的新文档）
    CHECK(std::filesystem::exists("docs/ai_todo.md"));              // 「完善 AI」明细清单（TODO.md 只留一条并置底）
    CHECK(std::filesystem::exists("docs/zhizun_field_rules.md"));   // 至尊场规则 + 全武将分档表 + 来源
    CHECK(std::filesystem::exists("docs/identity_field_rules.md")); // 身份场：选将框/择途/立储/胜负判定
    CHECK(std::filesystem::exists("docs/doudizhu_rules.md"));
}

// 蓄力技不变式：技能文本声明“蓄力技（x/y）”的武将，开局后"蓄力"=x、"蓄力上限"=y。
// 覆盖 DIY 钟会【权谋】(1/5)、势·肆恣(4/4)、势·屯田(0/0)、谋·再起(0/7)、谋·义从(2/4)、谋·弘援(1/3)。
// 回归来源：2026-10-04 用户发现 DIY 钟会局势显示“蓄力 0/0”（上限未登记）。
TEST("audit/charge_skill_declared_start_and_cap_match") {
    int checked = 0;
    for (const auto& info : HeroRegistry::all()) {
        if (info.pack == "标准包") continue;
        auto hero = HeroRegistry::create(info.id);
        if (!hero) continue;
        int declaredStart = -1, declaredCap = -1;
        for (const auto& skill : hero->getSkills()) {
            const std::string& d = skill->getDescription();
            size_t pos = d.find("蓄力技（");
            if (pos == std::string::npos) continue;
            size_t l = pos + std::string("蓄力技（").size();
            size_t slash = d.find('/', l);
            size_t rp = d.find("）", slash == std::string::npos ? l : slash); // 多字节字符要用字符串字面量（clang 不接受 char 字面量）
            if (slash == std::string::npos || rp == std::string::npos) continue;
            declaredStart = std::atoi(d.substr(l, slash - l).c_str());
            declaredCap = std::atoi(d.substr(slash + 1, rp - slash - 1).c_str());
            break;
        }
        if (declaredCap < 0) continue;
        // 凑一个不同人物的第二座位
        std::string other;
        for (const auto& cand : HeroRegistry::all()) {
            if (HeroRegistry::personKey(cand.id) != HeroRegistry::personKey(info.id)) { other = cand.id; break; }
        }
        CHECK(!other.empty());
        if (other.empty()) continue;
        GameEngine e; captureLog(e);
        e.initGame(2, -1, {info.id, other});
        auto p = e.getPlayers()[0];
        std::cout << " [蓄力技核对] " << info.name << " 声明 " << declaredStart << "/" << declaredCap
                  << " 实际 " << p->getMark("蓄力") << "/" << p->getMark("蓄力上限") << std::endl;
        CHECK_EQ(p->getMark("蓄力上限"), declaredCap);
        CHECK_EQ(p->getMark("蓄力"), declaredStart);
        ++checked;
    }
    CHECK_EQ(checked, 6); // 上述 6 条蓄力技声明（新增蓄力技须同步更新）
}

// 类型标签不变式：描述中声明的技能类型关键词（锁定技/限定技/觉醒技/主公技/转换技/持恒技）
// 必须与 SkillTag 登记完全一致（“非锁定技”等否定用法不计）。
// 回归来源：2026-10-06 实现级复核——StateSkill 默认 LOCK 使谋-看破/谋-妙略/势-好施/DIY 权谋等
// 描述未声明“锁定技”却被当作锁定技；势-振锋声明“限定技”却未登记且未限一局一次；
// 友-演策登记了描述中没有的“限定技”。
TEST("audit/tag_keywords_match_description") {
    auto declares = [](const std::string& s, const std::string& kw) {
        size_t pos = 0;
        while ((pos = s.find(kw, pos)) != std::string::npos) {
            if (pos >= 3 && s.compare(pos - 3, 3, "非") == 0) { pos += kw.size(); continue; }
            return true;
        }
        return false;
    };
    struct Rule { const char* kw; unsigned tag; };
    const Rule rules[] = {
        {"锁定技", SkillTag::LOCK}, {"限定技", SkillTag::LIMITED},
        {"觉醒技", SkillTag::AWAKEN}, {"主公技", SkillTag::LORD},
        {"转换技", SkillTag::SWITCH}, {"持恒技", SkillTag::SUSTAINED},
    };
    int flagged = 0;
    for (const auto& info : HeroRegistry::all()) {
        auto hero = HeroRegistry::create(info.id);
        if (!hero) continue;
        for (const auto& sk : hero->getSkills()) {
            const std::string d = sk->getDescription();
            // 已申明例外：谋·马超【谋-马术】官网该页漏标“锁定技”（同技能的标准/界/庞德版本
            // 与移动版 WIKI 均标锁定技），实现按正确规则保留 LOCK——见
            // docs/official_text_audit_2026-10-06.md「已申明互见」。
            const bool maShuOfficialOmitsLock =
                (info.id == "mou_machao" && sk->getName() == "谋-马术");
            for (const auto& r : rules) {
                if (maShuOfficialOmitsLock && r.tag == SkillTag::LOCK) continue;
                bool inText = declares(d, r.kw);
                bool inTag = sk->hasTag(r.tag);
                if (inText != inTag) {
                    ++flagged;
                    std::cerr << " [标签不一致] " << info.name << "·" << sk->getName() << " " << r.kw
                              << (inText ? " 文本有/标签无" : " 文本无/标签有") << std::endl;
                }
            }
        }
    }
    CHECK_EQ(flagged, 0);
}
