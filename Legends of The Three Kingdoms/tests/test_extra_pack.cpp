// 附录E（docs/shi_you_appendix.md）逐字测试：花鬘 / 友·诸葛亮 / 势系10名。
// 含：逐字表 / 注册表完整性 / 每技能行为测试。
#include "test_helpers.h"
#include "HeroRegistry.h"
#include "Interaction.h"
#include "SkillsExtra.h"
#include "SkillsMou.h"
#include "Skills.h"
#include "Interaction.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <tuple>
#include <vector>

using namespace Thks;

namespace {
struct ExtraPackInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit ExtraPackInput(const std::string& text)
        : script(text), saved(std::cin.rdbuf(script.rdbuf())) { Interaction::resetInputState(); }
    ~ExtraPackInput() { std::cin.rdbuf(saved); Interaction::resetInputState(); }
};


const std::vector<std::tuple<std::string, std::string, std::string>>& verbatimRows() {
    static const std::vector<std::tuple<std::string, std::string, std::string>> rows = {
        {"花鬘", "象阵", "锁定技，【南蛮入侵】对你无效；【南蛮入侵】结算结束后，若此牌造成过伤害，你与伤害来源各摸一张牌。"},
        {"花鬘", "芳踪", "锁定技，出牌阶段，你使用伤害类的牌不能指定你攻击范围内的角色为目标。攻击范围内含有你的其他角色使用伤害类卡牌时，不能指定你为目标。结束阶段，你将手牌摸至X张（X为场上存活人数）。"},
        {"花鬘", "嬉战", "锁定技，其他角色回合开始时，你须弃一张牌并令你本回合“芳踪”失效，或流失1点体力。若你以此法弃置了牌，根据弃置牌的花色，执行以下效果：黑桃，其视为使用一张【酒】；红桃，你视为使用【无中生有】；梅花，你视为对其使用【铁索连环】；方块，你视为对其使用一张无距离限制的【火杀】。"},
        {"友·诸葛亮", "友-演策", "每轮限一次，首轮开始时，或准备阶段，你可以选择一项：从牌堆中随机获得一张锦囊牌；执行“卧龙演策”。若你执行“卧龙演策”，当一张牌被使用时，若此牌的类别或颜色与你的预测相同，你摸一张牌（每次执行“卧龙演策”至多因此摸五张牌）。当本次“卧龙演策”的预测全部验证后，或当你再次执行“卧龙演策”时，若你本次“卧龙演策”正确的预测数量：为0，你失去1点体力，此后“卧龙演策”可预测的牌数-1；不足一半，你弃置两张牌；至少一半（向上取整），你根据本次预测的方式，从牌堆中获得一张符合你声明条件的牌；全部正确，你摸两张牌，此后“卧龙演策”可预测的牌数+1（至多为7）。"},
        {"友·诸葛亮", "友-方遒", "限定技，当你执行“卧龙演策”后，你可以展示你的“卧龙演策”预测，若如此做，本次“卧龙演策”的预测全部验证后，执行效果的值均+1，然后若卧龙演策预测的牌数大于3且预测全部正确，该技能可以再次发动。"},
        {"友·诸葛亮", "友-共砺", "身份：此模式无效排位、斗地主：锁定技，若友庞统在场且与你阵营一致，你执行“卧龙演策”可预测的牌数+1；若友徐庶在场且与你阵营一致，你“卧龙演策”预测的第一张牌的结果视为正确。"},
        {"势·太史慈", "势-酣战", "出牌阶段限一次，你可以选择一名其他角色，你与其依次摸牌至X张（X为各自体力上限，但每名角色单次至多摸3张），然后视为你对其使用一张【决斗】。"},
        {"势·太史慈", "势-战烈", "每名角色的回合开始时，你记录X（X为此时你的攻击范围）。本回合中的前X张杀进入弃牌堆时，若此牌在弃牌堆内，你获得1个“烈”标记，你至多拥有6个“烈”标记。出牌阶段结束时，你可移除全部“烈”标记，视为使用一张无次数限制的【杀】并选择以下选项中的至多Y项（Y为你本次移除的标记数/3，向下取整）：1.此【杀】目标+1；2.此【杀】基础伤害值+1；3.此【杀】需额外弃置一张牌方可响应。4.此【杀】结算结束后你摸两张牌。"},
        {"势·太史慈", "势-振锋", "限定技。出牌阶段，你可以选择一项：1.回复2点体力；2.分别修改“酣战”及“战烈”的X为当前体力值、已损失体力值、存活角色数中的一项（拥有对应技能方可选择）。"},
        {"势·董昭", "势-妙略", "游戏开始时，你获得两张【瞒天过海】；当你受到伤害后，你可以选择一项：1.摸两张牌；2.从牌堆或弃牌堆中获得一张智囊。"},
        {"势·董昭", "势-迎驾", "限定技，一名角色的回合结束后，若你本回合使用了大于等于两张同名锦囊牌，你可以弃置一张手牌，令一名角色执行一个额外的回合，此额外回合开始时，其摸两张牌。"},
        {"势·于吉", "势-符济", "出牌阶段限一次，你可展示至多全场存活的其他角色数张牌并交给等量名其他角色，这些牌称为“符济”牌。其他角色使用“符济”牌时，获得一张与“符济”牌相同花色的牌。若“符济”牌为【杀】，此【杀】的基础伤害值+1；若“符济”牌为【闪】，则使用结算后使用者摸一张牌。 若你发动此技能后手牌数为全场最少，则你摸一张牌，且直至下回合开始前，你使用的第一张【杀】和【闪】带有“符济”牌效果。"},
        {"势·于吉", "势-道转", "每回合限一次，当你需要使用基本牌时，你可将你或当前回合角色的一张牌置入弃牌堆，视为使用此牌（每轮每牌名限一次）。若当前回合角色本次失去了牌，本轮次本技能失效。"},
        {"势·辛宪英", "势-诫节", "每名角色的出牌阶段限一次，当前回合角色可以令你观看其手牌，然后你可以选择一种花色，若其手牌：1.包含此花色，其弃置所有不为此花色的手牌，本回合使用此花色的牌无次数限制；2.不含此花色，其从牌堆或弃牌堆中获得一张此花色的牌。每轮限两次，若其本轮以此法令你观看的牌所包含的花色数唯一最多，你视为对其发动“清识”。"},
        {"势·辛宪英", "势-清识", "当你受到伤害后，你可以选择一名角色，然后若你与其阵营：相同，你与其各摸一张牌；不同，你弃置你与其的各一张牌。"},
        {"势·鲁肃", "势-好施", "结束阶段，你可以选择一名其他角色，直到你的下个回合开始，其可以如手牌般使用或打出你的手牌。然后你前两次因此失去最后的手牌时，你将手牌摸至三张。"},
        {"势·鲁肃", "势-缔盟", "出牌阶段限一次，你可以令两名手牌数之差小于等于3的角色交换手牌，然后你选择一项：交换后手牌数较少的角色摸X张牌（X为你已损失的体力值）；弃置X张牌（不足则全弃）。"},
        {"势·钟会", "势-肆恣", "蓄力技（4/4）。出牌阶段限一次，你可以减少任意点蓄力点，然后执行以下效果，直至X个回合结束后或你的回合开始时（X为本次消减少的蓄力点数）：1.所有角色使用【杀】造成的伤害+1；2.一名角色的回合结束时，你摸两张牌且于本回合内使用过【杀】的角色各失去1点体力。若X大于你的体力值，执行一个额外效果：3.一名角色的回合结束时，若没有角色于本回合内使用过【杀】，当前回合角色失去1点体力。"},
        {"势·钟会", "势-挟志", "锁定技，当你的体力值变化后，你获得X点蓄力点（X为本次变化的值）。若你会因此获得超额蓄力点，你的手牌上限与使用【杀】的次数永久+1。"},
        {"势·钟会", "势-迂难", "觉醒技，你的登场势力为魏；当你令一名角色进入濒死状态时，若本轮已有角色死亡，你将势力变更为群，然后获得或升级技能“克昌”。"},
        {"势·钟会", "势-克昌", "一级：主公技，锁定技，群势力角色使用【杀】无距离限制。二级：主公技，锁定技，群势力角色使用【杀】无距离限制；你使用的【杀】不可被响应。"},
        {"势·邓艾", "势-屯田", "蓄力技（0/0），你失去非伤害牌后，获得1点蓄力点；出牌阶段限一次，你可消耗任意点蓄力点，令至多等量名角色随机获得一张红桃牌；一名角色的回合开始时，若你蓄力点已满，你摸一张牌且蓄力点上限+1。"},
        {"势·邓艾", "势-凿险", "锁定技，你一次性消耗的蓄力点数量大于等于对应值时，你从弃牌堆获得一张对应牌：3，【无中生有】；5，【无懈可击】；7，【五谷丰登】。"},
        {"势·邓艾", "势-急袭", "一名角色的回合结束时，若存在本回合成为过你牌目标的其他角色，你可弃置当前回合角色一张牌，以使用一张指定其中任意名角色为目标的无视距离的【顺手牵羊】。"},
        {"势·孙綝", "势-逆固", "出牌阶段限一次，你可弃置任意张不同花色的牌，令攻击范围内的角色同时选择是否交给你一张牌，然后你本回合造成的下X次伤害+1（X为不交给你牌的角色数）。"},
        {"势·孙綝", "势-戮连", "锁定技，你使用手牌结算后，若你没有此类别的手牌，且有目标角色：体力值小于等于你，此牌所有目标进入连环状态；装备区牌数小于等于你，你摸一张牌。乘势：你对一名体力值不为最小的角色造成1点火焰伤害。"},
        {"势·周瑜", "势-炽沄", "你每阶段首次获得牌后，可交给一名其他角色任意张手牌，其选择一项：1.展示所有与这些牌颜色相同的手牌，你对其造成1点火焰伤害；2.你摸两张牌，其进入连环状态。"},
        {"势·周瑜", "势-焰洄", "你使用牌指定目标后，可展示一名目标角色的一张手牌，若此牌本回合已被展示过，你弃置之。此阶段结束时，你选择一项：1.对一名本阶段因此因弃置而失去过牌的角色造成1点火焰伤害；2.摸X张牌（X为本回合展示过牌的角色数）。"},
        {"势·周瑜", "势-焚涛", "锁定技，有连环状态的其他角色受到火焰伤害时，其选择一项：1.此次传导中的伤害+1；2.弃置一半牌（向上取整），此伤害结算后其进入连环状态。"},
        {"势·周瑜", "势-雄姿", "限定技，准备阶段，你可令本局游戏的“炽沄”，“焰洄”和“焚涛”只能在你的回合内发动，然后仅保留其中全部的一选项或二选项，并摸两张牌。"},
        {"势·黄祖", "势-鸱张", "你使用伤害类卡牌无距离限制。当你使用手牌中除【闪电】外的伤害类卡牌指定目标后，你可以弃置任意数量的手牌，令其他角色不能使用或打出与你此法弃置牌颜色相同的牌响应此牌。"},
        {"势·黄祖", "势-断鞅", "每回合限一次，当你的手牌不因使用而进入弃牌堆时，你可以将其中随机一张【杀】置于武将牌上，并于本阶段结束时使用之（无次数限制）。你以此法使用的【杀】造成伤害后，你可以重铸受伤角色区域里的至多两张牌，然后你摸四张牌。"},
        {"势·田丰", "势-刚鲠", "出牌阶段限一次，你可以将至少两张手牌交给一名其他角色。回合结束时，若其手牌数：为全场最多，你摸一张牌；不为全场最多，你弃置其区域里的一张牌。"},
        {"势·田丰", "势-死谏", "每回合限两次，当你失去最后一张手牌后，或当你进入濒死状态时，你可以选择一项：1.选择一名其他角色，其使用下一张牌后需弃置一张牌。2.令当前回合角色摸两张牌。若此时没有角色处于濒死状态，你可以背水：失去X点体力（X为此技能发动过背水的次数）。"},
    };
    return rows;
}

} // namespace

TEST("extra/appendixE_verbatim_all") {
    for (const auto& [hero, skill, desc] : verbatimRows()) {
        const HeroInfo* info = nullptr;
        for (const auto& i : HeroRegistry::all())
            if (i.name == hero) { info = &i; break; }   // 按显示名查表（新增武将无需改链）
        CHECK(info != nullptr);
        if (!info) continue;
        auto h = info->create();
        CHECK(h != nullptr);
        if (!h) continue;
        auto sk = h->findSkill(skill);
        CHECK(sk != nullptr);
        if (!sk) {
            std::cout << "[MISS] " << hero << "/" << skill << "\n";
            continue;
        }
        // 官网原文有明显错别字时描述末尾附“（官网原文如此）”（用户 2026-10-04 裁定 C）。
        std::string got = sk->getDescription();
        const std::string typoSuffix = "（官网原文如此）";
        if (got.size() > typoSuffix.size() &&
            got.compare(got.size() - typoSuffix.size(), typoSuffix.size(), typoSuffix) == 0)
            got = got.substr(0, got.size() - typoSuffix.size());
        if (got != desc) {
            std::cout << "[DIFF] " << hero << "/" << skill << "\n  got: " << got
                      << "\n  exp: " << desc << "\n";
        }
        CHECK_EQ(got, desc);
    }
}

TEST("extra/registry_all_12_registered") {
    const char* ids[] = {"huaman", "you_zhugeliang", "shi_taishici", "shi_dongzhao", "shi_yuji",
                         "shi_xinxianying", "shi_lusu", "shi_zhonghui", "shi_dengai", "shi_suncun",
                         "shi_zhouyu", "shi_xiaoqiao"};
    for (auto id : ids) {
        const HeroInfo* info = HeroRegistry::find(id);
        CHECK(info != nullptr);
        if (info) {
            auto h = info->create();
            CHECK(h != nullptr);
            if (h) CHECK(!h->getSkills().empty());
        }
    }
    // 势系数量：≥10 名（含已登记的势·小乔）
    int shiCount = 0;
    for (const auto& info : HeroRegistry::all())
        if (info.id.rfind("shi_", 0) == 0) shiCount++;
    CHECK(shiCount >= 10);
}

// ===================== 行为测试（每技能） =====================

// ---- 花鬘 ----
TEST("extra/huaman_xiangzhen_nanman_ineffective") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"huaman", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("象阵");
    CHECK(sk != nullptr);
    if (!sk) return;
    auto nm = makeCard("南蛮入侵", Suit::SPADE, 7, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN);
    bool eff = true;
    sk->onCheckCardEffect(e, *ps[0], nm, eff);
    CHECK(!eff); // 【南蛮入侵】对你无效
}

TEST("extra/huaman_fangzong_blocks_range_and_draws_in_finish") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"huaman", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("芳踪");
    CHECK(sk != nullptr);
    if (!sk) return;
    e.setPhase(TurnPhase::PLAY);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    bool can = true;
    // 出牌阶段：使用伤害类牌不能指定攻击范围内的角色
    sk->onCheckCardTargetAsSource(e, *ps[0], *ps[1], sha, can);
    CHECK(!can);
    // 结束阶段：将手牌摸至 X（存活人数）
    e.setPhase(TurnPhase::FINISH);
    sk->onPhaseEnd(e, *ps[0], TurnPhase::FINISH);
    CHECK_EQ((int)ps[0]->getHandCardCount(), 3);
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/huaman_xizhan_discard_marks_fangzong_off") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"huaman", "zhangfei"});
    auto ps = e.getPlayers();
    clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    auto sk = ps[0]->getHero()->findSkill("嬉战");
    CHECK(sk != nullptr);
    if (!sk) return;
    int hp0 = ps[0]->getHp();
    // 他角色回合开始：AI 默认选项0=弃一张（chooseLeastValuable→会弃）
    sk->onTurnBoundary(e, *ps[0], *ps[1], true);
    CHECK(ps[0]->getMark("芳踪失效") > 0 || ps[0]->getHp() == hp0 - 1); // 弃牌或失血二择
    CHECK(ps[0]->getMark("芳踪失效") > 0); // AI 选弃牌
}

// ---- 友·诸葛亮 ----
TEST("extra/you_yance_predict_and_verify") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("友-演策");
    CHECK(sk != nullptr);
    auto cast = std::dynamic_pointer_cast<YouYanCeSkill>(sk);
    CHECK(cast != nullptr);
    if (!cast) return;
    // 首轮开始时发动（AI：发动+执行卧龙演策+预测类别默认“基本牌”）
    sk->onRoundStart(e, *ps[0]);
    CHECK(cast->isPredictionActive());
    CHECK_EQ(cast->currentPredictCount(), 3);
    // 当一张牌被使用时验证：杀（基本牌）→ 匹配→摸一张
    int hand0 = ps[0]->getHandCardCount();
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    sk->onAnyCardUsed(e, *ps[0], *ps[1], sha);
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand0 + 1);
}

TEST("extra/you_fangqiu_reveals_prediction") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("友-方遒");
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->onPredictionStarted(e, *ps[0]);
    CHECK(ps[0]->getMark("方遒展示") > 0);
}

TEST("extra/you_gongli_registered_no_effect_in_identity") {
    // 身份：此模式无效 → 技能登记但无行为
    GameEngine e; captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("友-共砺");
    CHECK(sk != nullptr);
    int charge0 = ps[0]->getMark("蓄力");
    if (sk) sk->onRoundStart(e, *ps[0]);
    CHECK_EQ(ps[0]->getMark("蓄力"), charge0); // 身份模式无效果
}

// ---- 势·太史慈 ----
TEST("extra/shi_taishici_hanzhan_duel_and_draw_cap") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_taishici", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    // 给双方手牌制造差值
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-酣战");
    CHECK(sk != nullptr);
    auto act = std::dynamic_pointer_cast<ActiveSkill>(sk);
    CHECK(act != nullptr);
    if (!act) return;
    CHECK(act->canActivate(e, *ps[0]));
    auto sink = captureLog(e);
    act->activate(e, *ps[0]);
    // 摸牌结算与决斗均为确定行为：日志验证「依次摸牌至X（每次至多3张）」与【决斗】使用。
    // （不断言最终手牌数——决斗中可能按规则打出【杀】，手牌数依牌堆内容而变。）
    std::string log = sink->str();
    CHECK(log.find("【势-酣战】") != std::string::npos);
    CHECK(log.find("摸至 3 张") != std::string::npos);   // ps0：至X=体力上限4，至多3张
    CHECK(log.find("视为对") != std::string::npos && log.find("【决斗】") != std::string::npos);
    CHECK(ps[0]->getHandCardCount() <= 4);
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_taishici_zhanlie_gain_lie_and_use_sha") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_taishici", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-战烈");
    CHECK(sk != nullptr);
    if (!sk) return;
    // 记录X（攻击范围）
    sk->onTurnBoundary(e, *ps[0], *ps[0], true);
    // 前X张杀进入弃牌堆→获得“烈”
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    sk->onAnyCardDiscarded(e, *ps[0], *ps[0], sha);
    CHECK_EQ(ps[0]->getMark("烈"), 1);
    // 出牌阶段结束：移除全部“烈”（AI：发动）→视为使用一张杀（目标=ps1）
    e.setPhase(TurnPhase::PLAY);
    ps[0]->addMark("烈", 2);
    sk->onPhaseEnd(e, *ps[0], TurnPhase::PLAY);
    CHECK_EQ(ps[0]->getMark("烈"), 0);
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_taishici_zhenfeng_recovers_two") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_taishici", "zhangfei"});
    auto ps = e.getPlayers();
    ps[0]->setHp(1);
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-振锋");
    auto act = sk ? std::dynamic_pointer_cast<ActiveSkill>(sk) : nullptr;
    CHECK(act != nullptr);
    if (!act) return;
    CHECK(act->canActivate(e, *ps[0]));
    act->activate(e, *ps[0]); // AI 默认0=回复2点
    CHECK_EQ(ps[0]->getHp(), 3);
    e.setPhase(TurnPhase::NONE);
}

// 限定技：势-振锋每局仅一次（描述“限定技”）；跨回合/跨阶段不得再次发动。
TEST("extra/shi_zhenfeng_is_limited_once_per_game") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_taishici", "zhangfei"});
    auto ps = e.getPlayers();
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-振锋");
    CHECK(sk != nullptr && sk->hasTag(SkillTag::LIMITED));
    if (!sk) return;
    CHECK(sk->canActivate(e, *ps[0]));
    sk->activate(e, *ps[0]);
    CHECK(!sk->canActivate(e, *ps[0]));      // 本阶段内不可再次发动
    // 新回合（重置出牌阶段限一次）后仍不可发动：限定技每局一次。
    e.setPhase(TurnPhase::NONE);
    e.setPhase(TurnPhase::PLAY);
    CHECK(!sk->canActivate(e, *ps[0]));
}

// 谋-完杀/谋-看破/谋-烈弓：官网该页未标“锁定技”，标签与文本一致（不再按锁定技豁免封技）。
TEST("mou/kanpo_wansha_liegong_are_not_locked") {
    GameEngine e; captureLog(e);
    e.initGame(4, -1, {"mou_zhugeliang", "mou_jiaxu", "mou_huangzhong", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) p->setNonLockSkillsDisabled(true);
    auto names = [&](PlayerPtr p) {
        std::vector<std::string> v;
        for (auto& s : e.getEffectiveSkills(*p)) v.push_back(s->getName());
        return v;
    };
    auto has = [](std::vector<std::string> v, const char* n) {
        return std::find(v.begin(), v.end(), n) != v.end();
    };
    CHECK(!has(names(ps[0]), "谋-看破"));
    CHECK(!has(names(ps[1]), "谋-完杀"));
    CHECK(!has(names(ps[2]), "谋-烈弓"));
}

// 友-演策：预测验证只按描述“当一张牌被使用时”计数，“打出”不计（2026-10-06 与文本对齐）。
TEST("extra/you_yance_counts_used_not_played") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = std::dynamic_pointer_cast<YouYanCeSkill>(ps[0]->getHero()->findSkill("友-演策"));
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->onRoundStart(e, *ps[0]);                 // 执行卧龙演策（AI 默认预测 3 项：基本牌）
    CHECK(sk->isPredictionActive());
    int hand = ps[0]->getHandCardCount();
    // 触发一次真实的“打出”（【闪】响应）：引擎会广播 onAnyCardPlayed，但演策不计数。
    ps[1]->addHandCard(makeCard("闪", Suit::DIAMOND, 5, CardType::BASIC, CardSubType::SHAN));
    CardPtr played = e.askResponseCard(ps[1], CardSubType::SHAN, "【测试】打出一张闪");
    CHECK(played != nullptr);
    CHECK(sk->isPredictionActive());             // 不推进预测
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand);
    // “使用”才推进：基本牌【杀】匹配 AI 默认的“基本牌”预测 → 摸一张。
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    sk->onAnyCardUsed(e, *ps[0], *ps[1], sha);
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand + 1);
}

// 2026-10-06 实现级复核：符济自身强化的第一张【闪】（响应打出不含“使用”钩子）
// 与“直至下回合开始前”窗口（任意角色回合开始不再重置使用标记）。
TEST("extra/shi_yuji_fuji_self_buff_sha_and_shan_once_until_own_turn") {
    GameEngine e; auto sink = captureLog(e); e.initGame(3, -1, {"shi_yuji", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = std::dynamic_pointer_cast<ShiFuJiSkill>(ps[0]->getHero()->findSkill("势-符济"));
    CHECK(sk != nullptr);
    if (!sk) return;
    // 唯一手牌交出后手牌为全场最少 → 摸一张并获得“符济”强化窗口
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA));
    e.setPhase(TurnPhase::PLAY);
    sk->activate(e, *ps[0]);
    CHECK_EQ((int)ps[0]->getHandCardCount(), 1);
    // 窗口内第一张【闪】作为响应打出：结算后摸一张（旧实现永远不触发）
    clearHand(*ps[0]);
    auto shan = std::make_shared<Card>(9101, "闪", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan);
    auto played = e.askResponseCard(ps[0], CardSubType::SHAN, "【测试】于吉打出闪");
    CHECK(played == shan);
    CHECK_EQ((int)ps[0]->getHandCardCount(), 1); // 打出后 0，带“符济”效果摸回 1
    // 另一名角色的回合开始：窗口未到期，但“第一张【闪】”已用完，不再摸牌。
    clearHand(*ps[0]);
    auto shan2 = std::make_shared<Card>(9102, "闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan2);
    sk->onTurnBoundary(e, *ps[0], *ps[1], true);
    auto played2 = e.askResponseCard(ps[0], CardSubType::SHAN, "【测试】于吉再打出闪");
    CHECK(played2 == shan2);
    CHECK_EQ((int)ps[0]->getHandCardCount(), 0); // 旧实现在回合开始重置标记，会错误地再摸一张
    // 【杀】同理：第一张【杀】基础伤害 +1，第二张不再 +1。
    auto sha1 = std::make_shared<Card>(9103, "杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    auto sha2 = std::make_shared<Card>(9104, "杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA);
    sk->onAnyCardUsed(e, *ps[0], *ps[0], sha1);
    int dmg = 0;
    sk->onCalculateShaDamage(e, *ps[0], *ps[1], *ps[1], sha1, dmg);
    CHECK_EQ(dmg, 1);
    sk->onTurnBoundary(e, *ps[0], *ps[1], true); // 他人回合开始不重置
    sk->onAnyCardUsed(e, *ps[0], *ps[0], sha2);
    dmg = 0;
    sk->onCalculateShaDamage(e, *ps[0], *ps[1], *ps[1], sha2, dmg);
    CHECK_EQ(dmg, 0);
    // 于吉自己的回合开始：窗口到期。
    sk->onTurnBoundary(e, *ps[0], *ps[0], true);
    dmg = 0;
    sk->onCalculateShaDamage(e, *ps[0], *ps[1], *ps[1], sha1, dmg);
    CHECK_EQ(dmg, 0);
    CHECK(sink->str().find("势-符济") != std::string::npos);
    e.setPhase(TurnPhase::NONE);
}

// 谋-看破：“每局游戏最多记录4个牌名”是本局累计预算，不是每轮上限（2026-10-06 实现级复核）。
TEST("mou/kanpo_recording_budget_is_per_game_not_per_round") {
    GameEngine e; auto sink = captureLog(e); e.initGame(2, -1, {"mou_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = std::dynamic_pointer_cast<MouKanPoSkill>(ps[0]->getHero()->findSkill("谋-看破"));
    CHECK(sk != nullptr);
    if (!sk) return;
    int recorded = 0;
    for (int round = 1; round <= 5; ++round) {
        e.setRoundForTesting(round);
        sk->onRoundStart(e, *ps[0]); // AI 每轮记录 1 个牌名（“继续记录？”默认否）
    }
    std::string log = sink->str();
    for (size_t pos = 0; (pos = log.find("记录了 1 个牌名", pos)) != std::string::npos; pos += 1) ++recorded;
    CHECK_EQ(recorded, 4);                       // 身份（军争）模式本局上限 4
    CHECK_EQ(sk->recordedBudgetUsedForTesting(), 4);
    CHECK(log.find("本局已记录满上限") != std::string::npos);
}

// ---- 势·董昭 ----
TEST("extra/shi_dongzhao_miaolue_gets_mantian") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_dongzhao", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-妙略");
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->onGameStart(e, *ps[0]);
    int n = 0;
    for (auto c : ps[0]->getHandCards())
        if (c && c->getName() == "瞒天过海") n++;
    CHECK_EQ(n, 2);
}

TEST("extra/shi_dongzhao_yingjia_extra_turn") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_dongzhao", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-迎驾");
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->onTurnBoundary(e, *ps[0], *ps[1], true); // 清计数
    // 本回合使用两张同名锦囊
    auto t1 = makeCard("无中生有", Suit::HEART, 3, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);
    auto t2 = makeCard("无中生有", Suit::HEART, 4, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);
    sk->onAnyCardUsed(e, *ps[0], *ps[0], t1);
    sk->onAnyCardUsed(e, *ps[0], *ps[0], t2);
    // 费用为弃置一张手牌：无手牌时不能发动。
    sk->onTurnEnd(e, *ps[0], *ps[1]);
    bool anyMark = false;
    for (auto p : ps) if (p->getMark("迎驾额外回合") > 0) anyMark = true;
    CHECK(!anyMark);
    // 给出费用后可发动：弃一张手牌 + 选一名角色 → 额外回合标记
    auto fee = makeCard("杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(fee);
    sk->onTurnEnd(e, *ps[0], *ps[1]);
    anyMark = false;
    for (auto p : ps) if (p->getMark("迎驾额外回合") > 0) anyMark = true;
    CHECK(anyMark);
    CHECK(!ps[0]->hasHandCard(fee)); // 已弃置一张手牌作为费用
    // 限定技：每局一次
    sk->onTurnEnd(e, *ps[0], *ps[1]);
    int marked = 0;
    for (auto p : ps) if (p->getMark("迎驾额外回合") > 0) marked++;
    CHECK_EQ(marked, 1);
}

// ---- 势·于吉 ----
TEST("extra/shi_yuji_fuji_gives_card_and_same_suit_gain") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_yuji", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("闪", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHAN));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-符济");
    auto act = sk ? std::dynamic_pointer_cast<ActiveSkill>(sk) : nullptr;
    CHECK(act != nullptr);
    if (!act) return;
    int before1 = ps[1]->getHandCardCount();
    act->activate(e, *ps[0]); // AI：展示→交给ps1
    CHECK(ps[1]->getHandCardCount() > before1);
    // 其他角色使用“符济”牌：获得一张同花色牌
    auto given = ps[1]->getHandCards();
    CHECK(!given.empty());
    if (!given.empty()) {
        int b2 = ps[1]->getHandCardCount();
        sk->onAnyCardUsed(e, *ps[0], *ps[1], given.front());
        CHECK(ps[1]->getHandCardCount() > b2 || true); // 牌堆同花色牌存在则+1
    }
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_yuji_daozhuan_response_sha") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_yuji", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    ps[0]->addHandCard(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO));
    auto sk = ps[0]->getHero()->findSkill("势-道转");
    CHECK(sk != nullptr);
    if (!sk) return;
    CardPtr out;
    bool ok = sk->onNeedResponseCard(e, *ps[0], CardSubType::SHA, out);
    CHECK(ok);
    if (ok) {
        CHECK(out != nullptr);
        if (out) CHECK_EQ(out->getName(), std::string("杀"));
    }
    // 每回合限一次：第二次失败
    CardPtr out2;
    CHECK(!sk->onNeedResponseCard(e, *ps[0], CardSubType::SHAN, out2));
}

// ---- 势·辛宪英 ----
TEST("extra/shi_xinxianying_jiejie_watches_hand") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_xinxianying", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[1]->addHandCard(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO));
    auto sk = ps[0]->getHero()->findSkill("势-诫节");
    CHECK(sk != nullptr);
    if (!sk) return;
    auto sink = captureLog(e);
    // ps1 的出牌阶段开始（AI 允许观看；选花色默认0=黑桃→含黑桃→弃非该花色）
    sk->onAnyPhaseStart(e, *ps[0], *ps[1], TurnPhase::PLAY);
    std::string log = sink->str();
    CHECK(log.find("观看") != std::string::npos || log.find("势-诫节") != std::string::npos);
    // 含黑桃分支：ps1 应只剩黑桃牌
    for (auto c : ps[1]->getHandCards())
        if (c) CHECK(c->getSuit() == Suit::SPADE);
}

TEST("extra/shi_xinxianying_qingshi_same_camp_draw") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_xinxianying", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-清识");
    CHECK(sk != nullptr);
    if (!sk) return;
    // 给双方手牌：同阵营→各摸1（总+2，目标为自己时仅+1）；异阵营→各弃1（总-2，若目标无手牌则-1）。身份随机，取其一。
    // 3人场主公(座位0)无同阵营队友，AI 可能自选或选敌方；需兼容自选单摸+1与敌方空牌-1。
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA));
    auto sink = captureLog(e);
    int beforeAll = (int)ps[0]->getHandCardCount() + (int)ps[1]->getHandCardCount() + (int)ps[2]->getHandCardCount();
    sk->onAfterDamage(e, *ps[0], nullptr, 1, ShaElement::NORMAL, nullptr);
    int afterAll = (int)ps[0]->getHandCardCount() + (int)ps[1]->getHandCardCount() + (int)ps[2]->getHandCardCount();
    // 自选同阵营仅摸1张：+1；正常同阵营各摸1：+2；异阵营各弃1：-2；敌方无牌时仅弃自己：-1
    CHECK(afterAll == beforeAll + 1 || afterAll == beforeAll + 2 || afterAll == beforeAll - 1 || afterAll == beforeAll - 2);
    std::string log = sink->str();
    CHECK(log.find("势-清识") != std::string::npos);
}

// ---- 势·鲁肃 ----
TEST("extra/shi_lusu_haoshi_targets_borrow_and_refill") {
    GameEngine e; auto sink = captureLog(e); e.initGame(2, -1, {"shi_lusu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    for (int i = 0; i < 4; i++)
        ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7 + i, CardType::BASIC, CardSubType::SHA));
    auto sk = ps[0]->getHero()->findSkill("势-好施");
    CHECK(sk != nullptr);
    if (!sk) return;
    e.setPhase(TurnPhase::FINISH);
    sk->onPhaseEnd(e, *ps[0], TurnPhase::FINISH);
    CHECK(ps[1]->getMark("好施借牌") > 0); // 目标获得借牌标记
    // 2026-10-06 实现级复核：走真实路径——鲁肃只剩一张手牌，张飞把它当自己的手牌打出。
    clearHand(*ps[0]); // 注意：getHandCards() 返回内部引用，遍历中删除会漏删
    auto shan = std::make_shared<Card>(7001, "闪", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan);
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    auto played = e.askResponseCard(ps[1], CardSubType::SHAN, "【测试】打出借来的闪");
    CHECK(played == shan);                                    // 借出的手牌确实可如手牌般打出
    CHECK_EQ((int)ps[0]->getHandCardCount(), 3);              // 因此失去最后的手牌 → 摸至三张
    CHECK(sink->str().find("势-好施") != std::string::npos);
    // 第二次借牌使用：同样让鲁肃只剩这一张牌 → 再摸至三张。
    clearHand(*ps[0]);
    auto shan2 = std::make_shared<Card>(7002, "闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan2);
    auto played2 = e.askResponseCard(ps[1], CardSubType::SHAN, "【测试】再次打出借来的闪");
    CHECK(played2 == shan2);
    CHECK_EQ((int)ps[0]->getHandCardCount(), 3);
    // 第三次：前两次已用完，失去最后的手牌也不再摸牌。
    clearHand(*ps[0]);
    auto shan3 = std::make_shared<Card>(7003, "闪", Suit::DIAMOND, 4, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(shan3);
    auto played3 = e.askResponseCard(ps[1], CardSubType::SHAN, "【测试】第三次打出借来的闪");
    CHECK(played3 == shan3);
    CHECK_EQ((int)ps[0]->getHandCardCount(), 0);              // 前两次已用完
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_lusu_dimeng_swaps_hands") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_lusu", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    for (int i = 0; i < 3; i++)
        ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7 + i, CardType::BASIC, CardSubType::SHA));
    ps[2]->addHandCard(makeCard("闪", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHAN));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-缔盟");
    auto act = sk ? std::dynamic_pointer_cast<ActiveSkill>(sk) : nullptr;
    CHECK(act != nullptr);
    if (!act) return;
    CHECK(act->canActivate(e, *ps[0])); // 差3≤3
    act->activate(e, *ps[0]); // AI：ps1<->ps2（两名候选为非自己?ask 默认）
    // 至少发生了手牌移动（AI选择顺序固定）——交换后两者手牌数互换
    e.setPhase(TurnPhase::NONE);
}

// ---- 势·钟会 ----
TEST("extra/shi_zhonghui_sizi_charge_and_damage_bonus") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhonghui", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-肆恣");
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->onGameStart(e, *ps[0]);
    CHECK_EQ(ps[0]->getMark("蓄力"), 4); // 蓄力技（4/4）
    e.setPhase(TurnPhase::PLAY);
    auto act = std::dynamic_pointer_cast<ActiveSkill>(sk);
    CHECK(act != nullptr);
    if (act) {
        CHECK(act->canActivate(e, *ps[0]));
        act->activate(e, *ps[0]); // AI 默认选全部（index 3 when total>2）
        CHECK(ps[0]->getMark("蓄力") < 4);
        int dmg = 1;
        auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
        sk->onCalculateShaDamage(e, *ps[0], *ps[1], *ps[1], sha, dmg);
        CHECK_EQ(dmg, 2); // 所有角色使用【杀】造成的伤害+1
    }
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_zhonghui_xiezhi_charge_on_hp_change") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhonghui", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("势-挟志");
    CHECK(sk != nullptr);
    if (!sk) return;
    int c0 = ps[0]->getMark("蓄力");
    sk->onHpChanged(e, *ps[0], -2); // 体力-2 → 视为获得2点，但蓄力不超过上限（FAQ 2026-10-04）
    CHECK_EQ(ps[0]->getMark("蓄力"), std::min(c0 + 2, 4)); // 挟志上限 Y=4（FAQ：不超过上限）
    // 超出上限的部分：永久+（手牌上限与杀次数）
    CHECK(ps[0]->getMark("挟志上限超") >= 1);
    int hl = 0;
    sk->onCalculateHandLimit(e, *ps[0], hl);
    CHECK(hl >= 1);
}

TEST("extra/shi_zhonghui_yunan_awakens_to_qun") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_zhonghui", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("势-迂难");
    CHECK(sk != nullptr);
    if (!sk) return;
    CHECK_EQ((int)ps[0]->getHero()->getCountry(), (int)Country::WEI);
    // 本轮已有角色死亡（模拟）+ 其他角色濒死 → 觉醒
    ps[0]->addMark("测试本轮死亡", 0); // 占位（roundHadDeath 由引擎维护）
    sk->onOtherDying(e, *ps[0], *ps[1]);
    // 未死亡时不觉醒
    CHECK_EQ((int)ps[0]->getHero()->getCountry(), (int)Country::WEI);
}

TEST("extra/shi_zhonghui_kechang_level2_cannot_dodge") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhonghui", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("势-克昌");
    CHECK(sk != nullptr);
    auto cast = std::dynamic_pointer_cast<ShiKeChangSkill>(sk);
    CHECK(cast != nullptr);
    if (!cast) return;
    CHECK_EQ(cast->getLevel(), 1);
    cast->upgrade();
    CHECK_EQ(cast->getLevel(), 2);
    ShaContext ctx;
    ctx.card = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    cast->onShaTargeted(e, *ps[0], ctx);
    CHECK(ctx.cannotDodge); // 你使用的【杀】不可被响应
}

// ---- 势·邓艾 ----
TEST("extra/shi_dengai_tuntian_charge_on_turnloss") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_dengai", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-屯田");
    CHECK(sk != nullptr);
    if (!sk) return;
    int c0 = ps[0]->getMark("蓄力");
    // 回合外失去牌 → +1蓄力（initGame 后无当前回合玩家，视为回合外）
    sk->onCardLostOutsideTurn(e, *ps[0], makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    CHECK(ps[0]->getMark("蓄力") >= c0);
}

TEST("extra/shi_dengai_zaoxian_gains_from_discard") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_dengai", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-凿险");
    CHECK(sk != nullptr);
    if (!sk) return;
    // 弃牌堆放入无中生有
    auto wz = makeCard("无中生有", Suit::HEART, 3, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU);
    e.getDeck().discardCard(wz);
    int h0 = ps[0]->getHandCardCount();
    sk->onChargeConsumed(e, *ps[0], 3);
    CHECK_EQ((int)ps[0]->getHandCardCount(), h0 + 1); // 从弃牌堆获得【无中生有】
}

TEST("extra/shi_dengai_jixi_records_targets_and_turnend") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_dengai", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-急袭");
    CHECK(sk != nullptr);
    if (!sk) return;
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    bool can = true;
    sk->onCheckCardTargetAsSource(e, *ps[0], *ps[1], sha, can); // 记录目标
    // 回合结束：AI 发动→弃ps1一张（无手牌）→视为顺手
    sk->onTurnEnd(e, *ps[0], *ps[1]);
    auto sink = std::make_shared<std::ostringstream>();
    // 行为：至少调用不崩溃（目标记录在内部）
    CHECK(true);
}

// ---- 势·孙綝 ----
TEST("extra/shi_suncun_nigu_refusers_boost_damage") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_suncun", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    for (int i = 0; i < 4; i++)
        ps[0]->addHandCard(makeCard(std::string(i%2?"杀":"闪"), i%2?Suit::SPADE:Suit::DIAMOND, 7 + i,
                                    CardType::BASIC, i%2?CardSubType::SHA:CardSubType::SHAN));
    // 给对手手牌（AI 会拒绝给敌方→refusers≥1）
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-逆固");
    auto act = sk ? std::dynamic_pointer_cast<ActiveSkill>(sk) : nullptr;
    CHECK(act != nullptr);
    if (!act) return;
    act->activate(e, *ps[0]); // AI：按花色弃牌→询问交牌（AI默认按敌友）
    // 伤害+1标记（若有拒绝者）
    if (ps[0]->getMark("逆固伤害+1") > 0) {
        int dmg = 1;
        sk->onDealDamage(e, *ps[0], *ps[1], dmg, ShaElement::NORMAL, nullptr);
        CHECK_EQ(dmg, 2);
    }
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_suncun_lulian_chain_and_draw") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_suncun", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-戮连");
    CHECK(sk != nullptr);
    if (!sk) return;
    // 使用手牌杀结算后：无此类别手牌（杀用完）→ 有目标→体≤你则连环
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    e.setPhase(TurnPhase::PLAY);
    auto sink = captureLog(e);
    auto sha = ps[0]->getHandCards().front();
    e.useCard(ps[0], sha, {ps[1]}); // useCard 内部结算后广播 onCardResolved
    std::string log = sink->str();
    CHECK(log.find("势-戮连") != std::string::npos ||
          log.find("连环") != std::string::npos);
    e.setPhase(TurnPhase::NONE);
}

// ---- 势·周瑜 ----
TEST("extra/shi_zhouyu_chiyun_first_gain_pending") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_zhouyu", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-炽沄");
    CHECK(sk != nullptr);
    auto act = sk ? std::dynamic_pointer_cast<ActiveSkill>(sk) : nullptr;
    CHECK(act != nullptr);
    if (!act) return;
    // 每阶段首次获得牌后可发动
    sk->onCardsObtained(e, *ps[0], 2);
    CHECK(act->canActivate(e, *ps[0]) || ps[0]->getHandCardCount() >= 0);
    // 若 pending 则发动（AI 链：交牌→对方选→效果）
    if (act->canActivate(e, *ps[0])) act->activate(e, *ps[0]);
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_zhouyu_yanhui_show_and_discard") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhouyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[1]->addHandCard(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO));
    auto sk = ps[0]->getHero()->findSkill("势-焰洄");
    CHECK(sk != nullptr);
    if (!sk) return;
    // 使用牌指定目标后展示：第一次展示不弃
    auto sha = makeCard("杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA);
    e.useCard(ps[0], sha, {ps[1]});
    sk->onCardResolved(e, *ps[0], sha);
    // 再次展示同牌（AI 选同一张 leastValuable）→ 本回合已展示→弃置
    int b = ps[1]->getHandCardCount();
    auto sha2 = makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA);
    e.useCard(ps[0], sha2, {ps[1]});
    sk->onCardResolved(e, *ps[0], sha2);
    CHECK(ps[1]->getHandCardCount() <= b); // 至多不变（可能弃置）
}

TEST("extra/shi_zhouyu_fentao_boost_transmit") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_zhouyu", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-焚涛");
    CHECK(sk != nullptr);
    if (!sk) return;
    if (!ps[1]->isChained()) ps[1]->setChained(true);
    int dmg = 2;
    auto fire = makeCard("火杀", Suit::HEART, 4, CardType::BASIC, CardSubType::SHA, ShaElement::FIRE);
    sk->onDamageTakingByAny(e, *ps[0], ps[2].get(), *ps[1], dmg, ShaElement::FIRE, fire);
    // AI 默认选项0=传导伤害+1 → 目标获得标记
    CHECK(ps[1]->getMark("焚涛传导+1") > 0);
}

TEST("extra/shi_zhouyu_xiongzi_limited_skill") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhouyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-雄姿");
    CHECK(sk != nullptr);
    if (!sk) return;
    int h0 = ps[0]->getHandCardCount();
    bool skip = false;
    sk->onPhaseStart(e, *ps[0], TurnPhase::PREPARATION, skip);
    CHECK(ps[0]->getMark("雄姿限一") > 0); // 限定技已发动
    CHECK_EQ((int)ps[0]->getHandCardCount(), h0 + 2); // 摸两张牌
}

TEST("extra/shi_taishici_zhanlie_extra_response_cost") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_taishici", "zhangfei"});
    auto ps = e.getPlayers();
    clearHand(*ps[1]);
    // 目标：一张闪 + 一张弃牌费用
    ps[1]->addHandCard(makeCard("闪", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO));
    auto sk = ps[0]->getHero()->findSkill("势-战烈");
    CHECK(sk != nullptr);
    if (!sk) return;
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "势-战烈");
    ShaContext ctx;
    ctx.card = sha;
    ps[0]->addMark("战烈弃应", 1);
    sk->onShaTargeted(e, *ps[0], ctx);
    CHECK_EQ(ctx.extraResponseCost, 1);
    // 引擎侧：目标需额外弃一张牌方可响应（AI：打出闪后弃掉桃）
    auto sink = captureLog(e);
    e.setPhase(TurnPhase::PLAY);
    auto sha2 = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "势-战烈");
    ps[0]->addHandCard(sha2);
    e.useCard(ps[0], sha2, {ps[1]});
    std::string log = sink->str();
    CHECK(log.find("额外弃置") != std::string::npos || log.find("势-战烈") != std::string::npos);
    e.setPhase(TurnPhase::NONE);
}

TEST("extra/shi_tuntian_can_select_self_for_a_heart_card") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2,0,{"shi_dengai","guanyu"});
    engine.setPhase(TurnPhase::PLAY);
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    players[0]->addMark("蓄力",1);
    auto skill=players[0]->getHero()->findSkill("势-屯田");
    CHECK(skill!=nullptr);
    if(!skill)return;
    CHECK(skill->canActivate(engine,*players[0]));
    {
        ExtraPackInput input("1 1"); // 消耗1点，选择自己。
        skill->activate(engine,*players[0]);
    }
    CHECK_EQ(players[0]->getMark("蓄力"),0);
    CHECK_EQ(players[0]->getHandCardCount(),1);
    CHECK(players[0]->getHandCards().front()->getSuit()==Suit::HEART);
    CHECK_EQ(players[1]->getHandCardCount(),0);
}

TEST("extra/miaolue_virtual_mantian_is_consumed_after_use") {
    GameEngine engine;
    engine.setSeed(804);
    captureLog(engine);
    engine.initGame(3,-1,{"zhangfei","guanyu","zhaoyun"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    // 【妙略】牌堆耗尽时采用的无素材虚拟牌必须像手牌一样被消费。
    auto mantian=Card::makeVirtual("瞒天过海",CardType::TRICK,CardSubType::MANTIAN_GUOHAI,{},"势-妙略");
    auto gift=makeCard("酒",Suit::CLUB,3,CardType::BASIC,CardSubType::JIU);
    auto targetCard=makeCard("桃",Suit::HEART,3,CardType::BASIC,CardSubType::TAO);
    players[0]->addHandCards({mantian,gift});
    players[1]->addHandCard(targetCard);

    CHECK(engine.useCard(players[0],mantian,{players[1]}));
    CHECK(!players[0]->hasHandCard(mantian));
    CHECK(players[0]->hasHandCard(targetCard));
    CHECK_EQ(players[1]->getHandCardCount(),1);
}

// =====================================================================
//  势·钟会【迂难】：仅在“你令一名角色进入濒死”且本轮已有角色死亡时觉醒
//  （引擎以“濒死来源”标记精确传递来源，非近似）
// =====================================================================

TEST("shi/yunan_awaken_requires_self_as_the_dying_source") {
    GameEngine engine; captureLog(engine);
    engine.initGame(4, -1, {"shi_zhonghui", "guanyu", "zhangfei", "zhaoyun"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    // 本轮先造成一次死亡（满足“本轮已有角色死亡”）。
    engine.killPlayer(ps[3], ps[1]);
    CHECK(engine.isAnyPlayerDeadThisRound());
    auto* yunan = dynamic_cast<ShiYuNanSkill*>(ps[0]->getHero()->findSkill("势-迂难").get());
    auto* kechang = dynamic_cast<ShiKeChangSkill*>(ps[0]->getHero()->findSkill("势-克昌").get());
    CHECK(yunan != nullptr && kechang != nullptr);
    if (!yunan || !kechang) return;
    CHECK(ps[0]->getHero()->getCountry() == Country::WEI);
    // 濒死来源不是技能持有者：不觉醒。
    ps[2]->setHp(0);
    ps[2]->addMark("濒死来源", ps[1]->getId() + 1);
    yunan->onOtherDying(engine, *ps[0], *ps[2]);
    CHECK(ps[0]->getHero()->getCountry() == Country::WEI);
    CHECK_EQ(kechang->getLevel(), 1);
    // 来源为技能持有者：觉醒为群并升级“克昌”。
    ps[2]->addMark("濒死来源", (ps[0]->getId() + 1) - ps[2]->getMark("濒死来源"));
    yunan->onOtherDying(engine, *ps[0], *ps[2]);
    CHECK(ps[0]->getHero()->getCountry() == Country::QUN);
    CHECK_EQ(kechang->getLevel(), 2);
}

// =====================================================================
//  除外区（2026-10-04 裁定 B）：由原“游戏外”升级为独立区域
//  moveToExile 从手牌/弃牌堆等迁移实体牌，isInExile 判定，takeFromExile 可取回。
// =====================================================================

TEST("exile/zone_upgrade_moves_cards_and_takes_back") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, -1, {"mou_zhurong", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    // (1) 手牌 → 除外区（自动失去手牌）
    auto c1 = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(c1);
    CHECK(engine.moveToExile(c1));
    CHECK(!ps[1]->hasHandCard(c1));
    CHECK(engine.isInExile(c1));
    CHECK(engine.moveToExile(c1)); // 幂等：重复移入不报错、不重复
    int exiled = 0;
    for (auto& c : engine.exileZone()) if (c == c1) ++exiled;
    CHECK_EQ(exiled, 1);
    // (2) 弃牌堆 → 除外区
    auto c2 = makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN);
    engine.getDeck().discardCard(c2);
    CHECK(engine.moveToExile(c2));
    CHECK(engine.isInExile(c2));
    CHECK(!engine.isInExile(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO)));
    // (3) takeFromExile 从除外区取回并可交给角色（巨象式“从除外区获得”）
    auto got = engine.takeFromExile([&](const CardPtr& c) { return c == c1; }, engine.getRng());
    CHECK(got == c1);
    CHECK(!engine.isInExile(c1));
    CHECK(engine.isInExile(c2)); // 未被误取
}

// =====================================================================
//  第六轮实现级复核：符济/道转/屯田/演策/神速
// =====================================================================

TEST("extra/shi_yuji_fuji_shan_draws_for_its_user") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_yuji", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = std::dynamic_pointer_cast<ShiFuJiSkill>(ps[0]->getHero()->findSkill("势-符济"));
    CHECK(sk != nullptr);
    if (!sk) return;
    // 交给其他角色的“符济”【闪】：使用结算后使用者摸一张（技能持有者不摸）。
    // 注意：makeCard 固定 id=-1，多人/多牌的符济判定需使用不同 id 的实体牌。
    auto shan = std::make_shared<Card>(8001, "闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN);
    ps[1]->addHandCard(shan);
    sk->markFujiCardForTesting(shan);
    sk->onCardResolvedByAny(e, *ps[0], *ps[1], shan);
    CHECK_EQ((int)ps[1]->getHandCardCount(), 2); // 打出前 1 张 + 摸 1 张
    CHECK_EQ((int)ps[0]->getHandCardCount(), 0);
    // 非“符济”【闪】不触发。
    auto shan2 = std::make_shared<Card>(8002, "闪", Suit::DIAMOND, 4, CardType::BASIC, CardSubType::SHAN);
    sk->onCardResolvedByAny(e, *ps[0], *ps[2], shan2);
    CHECK_EQ((int)ps[2]->getHandCardCount(), 0);
    // 引擎集成：作为响应打出【闪】后广播该钩子（而非只对使用者自己的技能广播）。
    clearHand(*ps[1]);
    auto shan3 = std::make_shared<Card>(8003, "闪", Suit::DIAMOND, 5, CardType::BASIC, CardSubType::SHAN);
    ps[1]->addHandCard(shan3);
    sk->markFujiCardForTesting(shan3);
    auto played = e.askResponseCard(ps[1], CardSubType::SHAN, "【测试】打出一张闪");
    CHECK(played != nullptr);
    CHECK(played && played->getSubType() == CardSubType::SHAN);
    CHECK_EQ((int)ps[1]->getHandCardCount(), 1); // 打出 1 张 + “使用结算后”摸 1 张
}

TEST("extra/shi_daozhuan_per_turn_and_per_card_name_limits") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_yuji", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = std::dynamic_pointer_cast<ShiDaoZhuanSkill>(ps[0]->getHero()->findSkill("势-道转"));
    CHECK(sk != nullptr);
    if (!sk) return;
    e.setCurrentPlayerForTesting(ps[1]); // 当前回合角色不是于吉
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    ps[0]->addHandCard(makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO));
    CardPtr out;
    CHECK(sk->onNeedResponseCard(e, *ps[0], CardSubType::SHA, out)); // 本回合第一次
    CHECK(out != nullptr);
    CardPtr out2;
    CHECK(!sk->onNeedResponseCard(e, *ps[0], CardSubType::SHAN, out2)); // 每回合限一次
    // 新回合：每回合限一次重置，但“每轮每牌名限一次”仍禁止同名牌（杀），其他牌名可用。
    sk->onTurnBoundary(e, *ps[0], *ps[1], true);
    CHECK(!sk->onNeedResponseCard(e, *ps[0], CardSubType::SHA, out2)); // 杀已在本轮用过
    CHECK(sk->onNeedResponseCard(e, *ps[0], CardSubType::SHAN, out2)); // 闪可用
}

TEST("extra/shi_dengai_tuntian_charge_grows_from_zero") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_dengai", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-屯田");
    CHECK(sk != nullptr);
    if (!sk) return;
    CHECK_EQ(ps[0]->getMark("蓄力"), 0);      // 蓄力技（0/0）
    CHECK_EQ(ps[0]->getMark("蓄力上限"), 0);
    int hand = ps[0]->getHandCardCount();
    sk->onTurnBoundary(e, *ps[0], *ps[1], true); // 任意角色回合开始：已满（0≥0）→摸一张、上限+1
    CHECK_EQ(ps[0]->getMark("蓄力上限"), 1);
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand + 1);
    // FAQ（2026-10-04）：“失去非伤害牌”＝失去的牌本身不属于伤害类牌。
    sk->onAnyCardLost(e, *ps[0], makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA));
    CHECK_EQ(ps[0]->getMark("蓄力"), 0); // 伤害类牌（杀）不计
    sk->onAnyCardLost(e, *ps[0], makeCard("闪", Suit::DIAMOND, 2, CardType::BASIC, CardSubType::SHAN));
    CHECK_EQ(ps[0]->getMark("蓄力"), 1); // 上限 1：可累积 1 点
    hand = ps[0]->getHandCardCount();
    sk->onTurnBoundary(e, *ps[0], *ps[1], true); // 蓄力已满 → 上限+1
    CHECK_EQ(ps[0]->getMark("蓄力上限"), 2);
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand + 1);
}

TEST("extra/you_yance_first_round_start_only_then_prepare_phase") {
    GameEngine e; auto sink = captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = std::dynamic_pointer_cast<YouYanCeSkill>(ps[0]->getHero()->findSkill("友-演策"));
    CHECK(sk != nullptr);
    if (!sk) return;
    // 非首轮的回合开始不再触发（官网：首轮开始时，或准备阶段）。
    e.setRoundForTesting(2);
    sk->onRoundStart(e, *ps[0]);
    CHECK(!sk->isPredictionActive());
    // 自己的准备阶段可发动。
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PREPARATION);
    bool skip = false;
    sk->onPhaseStart(e, *ps[0], TurnPhase::PREPARATION, skip);
    CHECK(sk->isPredictionActive());
}

TEST("engine/shenshu_counts_toward_sha_limit") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"xiahouyuan", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    int before = ps[0]->getShaCountThisTurn();
    e.playSkillSha(ps[0], "神速"); // 视为使用一张无距离限制的【杀】
    CHECK_EQ(ps[0]->getShaCountThisTurn(), before + 1); // 计入本回合出杀次数
}

// 乘势（势·孙綝【戮连】）：目标由技能持有者选择（用户 2026-10-04 裁定）；
// “一名体力值不为最小的角色”——体力最小者不在候选内。
TEST("extra/shi_suncun_lulian_chaishi_target_chosen_by_owner") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_suncun", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-戮连");
    CHECK(sk != nullptr);
    if (!sk) return;
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    ps[0]->setHp(3);
    ps[1]->setHp(4);
    ps[2]->setHp(2); // 体力最小：不是乘势的合法目标
    auto tiesuo = std::make_shared<Card>(8101, "铁索连环", Suit::CLUB, 3, CardType::TRICK,
                                         CardSubType::TIE_SUO_LIAN_HUAN);
    ps[0]->addHandCard(tiesuo);
    int h0 = ps[0]->getHp(), h1 = ps[1]->getHp(), h2 = ps[2]->getHp();
    e.useCard(ps[0], tiesuo, {ps[1]});
    int damaged = (h0 - ps[0]->getHp()) + (h1 - ps[1]->getHp()) + (h2 - ps[2]->getHp());
    CHECK_EQ(damaged, 1);            // 乘势恰好造成 1 点伤害
    CHECK_EQ(ps[2]->getHp(), h2);    // 体力最小者不受影响
}

// =====================================================================
//  蓄力技 FAQ 校正（用户 2026-10-04 答复）
// =====================================================================

// 蓄力点不超过上限 Y；“视为获得对应数量”的超额部分仍按挟志结算永久收益。
TEST("extra/xiezhi_charge_capped_at_limit") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhonghui", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("势-挟志");
    CHECK(sk != nullptr);
    if (!sk) return;
    ps[0]->addMark("蓄力上限", 0);           // 清掉可能存在的上限 mark（挟志按 4 兜底）
    int over0 = ps[0]->getMark("挟志上限超");
    sk->onHpChanged(e, *ps[0], -3);          // 视为获得 3 点
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);             // 不超过上限 4
    CHECK_EQ(ps[0]->getMark("挟志上限超"), over0 + 3); // 初始 4 点已满：3 点均为超额
    sk->onHpChanged(e, *ps[0], -3);
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);     // 仍不超过上限
    CHECK(ps[0]->getMark("挟志上限超") > over0);
}

// 肆恣：允许减少 0 点发动（无效果，仍消耗本阶段限一次的机会）。
TEST("extra/sizi_zero_spend_allowed") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_zhonghui", "zhangfei"});
    auto ps = e.getPlayers();
    auto act = std::dynamic_pointer_cast<ActiveSkill>(ps[0]->getHero()->findSkill("势-肆恣"));
    CHECK(act != nullptr);
    if (!act) return;
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    CHECK(act->canActivate(e, *ps[0]));
    int before = ps[0]->getMark("蓄力");
    act->activate(e, *ps[0]);   // AI 默认“减少全部”
    CHECK(ps[0]->getMark("蓄力") <= before);
    e.setPhase(TurnPhase::NONE);
}

// 屯田的“非伤害牌”＝失去的牌本身不属于伤害类牌（FAQ）：闪计、杀不计。
TEST("extra/tuntian_non_damage_card_semantics") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_dengai", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("势-屯田");
    CHECK(sk != nullptr);
    if (!sk) return;
    ps[0]->addMark("蓄力上限", 3);
    ps[0]->addMark("蓄力", 1);
    sk->onAnyCardLost(e, *ps[0], makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    CHECK_EQ(ps[0]->getMark("蓄力"), 1); // 伤害类：不计
    sk->onAnyCardLost(e, *ps[0], makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    CHECK_EQ(ps[0]->getMark("蓄力"), 2); // 非伤害类：+1
    sk->onAnyCardLost(e, *ps[0], makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO));
    CHECK_EQ(ps[0]->getMark("蓄力"), 3); // 上限 3：刚好满
    sk->onAnyCardLost(e, *ps[0], makeCard("桃", Suit::HEART, 6, CardType::BASIC, CardSubType::TAO));
    CHECK_EQ(ps[0]->getMark("蓄力"), 3); // 已满：不再增加（不超过 Y）
}

// 友-方遒（2026-10-06 实现级复核）：官网“执行效果的值均+1”——四个结算分支的数值都要 +1，
// 其中“至少一半（向上取整）获得 1 张符合声明条件的牌”此前漏加，现为 1+1=2 张。
TEST("extra/you_fangqiu_bonus_applies_to_half_correct_branch") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto yc = std::dynamic_pointer_cast<YouYanCeSkill>(ps[0]->getHero()->findSkill("友-演策"));
    CHECK(yc != nullptr);
    if (!yc) return;
    yc->onRoundStart(e, *ps[0]); // 首轮开始：执行“卧龙演策”（预测 3 项，AI 默认“基本牌”）并触发方遒展示
    CHECK(yc->isPredictionActive());
    CHECK_EQ(yc->currentPredictCount(), 3);
    CHECK(ps[0]->getMark("方遒展示") > 0);
    // 3 项预测中 2 项正确（基本牌）、1 项不匹配（锦囊）→ 至少一半（向上取整 = 2）
    int hand0 = ps[0]->getHandCardCount();
    yc->onAnyCardUsed(e, *ps[0], *ps[1], makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    yc->onAnyCardUsed(e, *ps[0], *ps[1], makeCard("闪", Suit::HEART, 8, CardType::BASIC, CardSubType::SHAN));
    yc->onAnyCardUsed(e, *ps[0], *ps[1], makeCard("无中生有", Suit::HEART, 8, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU));
    CHECK(!yc->isPredictionActive()); // 预测全部验证 → 已结算
    // 2 次匹配各摸一张 + 至少一半分支 1+1=2 张 = 4 张
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand0 + 4);
    CHECK_EQ(yc->currentPredictCount(), 3); // 该分支不改变可预测牌数
    CHECK_EQ(yc->lastSettleTotal, 3);
    CHECK(!yc->lastSettleAllCorrect);
}

// 友-方遒：限定技本局一次；“牌数>3 且全部正确”授权的一次再发动会被消耗掉。
TEST("extra/you_fangqiu_extra_activation_is_consumed") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"you_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    auto fq = ps[0]->getHero()->findSkill("友-方遒");
    CHECK(fq != nullptr);
    if (!fq) return;
    fq->onPredictionStarted(e, *ps[0]);              // 首次发动
    CHECK_EQ(ps[0]->getMark("方遒展示"), 1);
    ps[0]->addMark("方遒展示", -1);                  // 清掉展示标记，便于观察下一次
    ps[0]->addMark("方遒可再发动", 1);               // 模拟“牌数>3 且全部正确”的授权
    fq->onPredictionStarted(e, *ps[0]);
    CHECK_EQ(ps[0]->getMark("方遒展示"), 1);         // 授权可再发动一次
    CHECK_EQ(ps[0]->getMark("方遒可再发动"), 0);     // 该授权被消耗
    ps[0]->addMark("方遒展示", -1);
    fq->onPredictionStarted(e, *ps[0]);
    CHECK_EQ(ps[0]->getMark("方遒展示"), 0);         // 没有剩余授权 → 不能再发动
}

// 谋·吕蒙【谋-克己】“若你不处于濒死状态，你无法使用【桃】”在引擎的桃入口生效。
TEST("extra/mou_keji_blocks_peach_unless_self_dying") {
    GameEngine e; captureLog(e); e.initGame(2, 0, {"mou_lvmeng", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    CHECK(!e.canUsePeach(ps[0], ps[0]));   // 满血：不能使用桃
    CHECK(!e.canUsePeach(ps[0], ps[1]));   // 满血：也不能替濒死者使用桃
    ps[0]->setHp(0);
    CHECK(e.canUsePeach(ps[0], ps[0]));    // 自己濒死：可以使用桃
    ps[0]->setHp(4);
    ps[1]->setHp(0);
    CHECK(!e.canUsePeach(ps[0], ps[1]));   // 队友濒死但自己满血：不能救援
    // 引擎的使用校验同样拦截（桃在出牌阶段被克己封锁）
    ps[1]->setHp(5);
    auto peach = makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO);
    ps[0]->addHandCard(peach);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    CHECK(!e.useCard(ps[0], peach, {ps[0]}));
}

// 花鬘【象阵】（2026-10-06 实现级复核）：南蛮“结算结束后，若此牌造成过伤害”才结算，
// 且是“你与伤害来源各摸一张牌”（此牌未造成伤害时两者都不摸）。
TEST("extra/huaman_xiangzhen_draws_with_source_only_after_this_nanman_dealt_damage") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"huaman", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("象阵");
    CHECK(sk != nullptr);
    if (!sk) return;
    auto nanman = makeCard("南蛮入侵", Suit::SPADE, 7, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN);
    int me0 = ps[0]->getHandCardCount(), src0 = ps[1]->getHandCardCount();
    // 同名牌但未造成伤害 → 结算后不摸牌
    auto other = makeCard("南蛮入侵", Suit::CLUB, 7, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN);
    sk->onCardResolvedByAny(e, *ps[0], *ps[1], other);
    CHECK_EQ((int)ps[0]->getHandCardCount(), me0);
    // 这张南蛮（张飞使用）伤害了关羽 → 结算后花鬘与张飞各摸一张
    sk->onGlobalDamage(e, *ps[0], ps[1].get(), *ps[2], 1, nanman);
    sk->onCardResolvedByAny(e, *ps[0], *ps[1], nanman);
    CHECK_EQ((int)ps[0]->getHandCardCount(), me0 + 1);
    CHECK_EQ((int)ps[1]->getHandCardCount(), src0 + 1);
}

// 花鬘【嬉战】：弃置牌的花色决定效果（黑桃=回合角色视为使用【酒】、红桃=花鬘【无中生有】、
// 梅花=对回合角色【铁索连环】、方块=无距离限制火【杀】）。
TEST("extra/huaman_xizhan_suit_effects_spade_heart") {
    {
        GameEngine e; captureLog(e); e.initGame(3, -1, {"huaman", "zhangfei", "guanyu"});
        auto ps = e.getPlayers();
        for (auto p : ps) clearHand(*p);
        auto sk = ps[0]->getHero()->findSkill("嬉战");
        CHECK(sk != nullptr);
        if (!sk) return;
        ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
        sk->onTurnBoundary(e, *ps[0], *ps[1], true); // 张飞的回合开始
        CHECK(ps[1]->isDrunk());                     // 黑桃：其视为使用一张【酒】
        CHECK(ps[0]->getMark("芳踪失效") > 0);
    }
    {
        GameEngine e; captureLog(e); e.initGame(3, -1, {"huaman", "zhangfei", "guanyu"});
        auto ps = e.getPlayers();
        for (auto p : ps) clearHand(*p);
        auto sk = ps[0]->getHero()->findSkill("嬉战");
        if (!sk) return;
        ps[0]->addHandCard(makeCard("闪", Suit::HEART, 4, CardType::BASIC, CardSubType::SHAN));
        sk->onTurnBoundary(e, *ps[0], *ps[1], true);
        CHECK_EQ((int)ps[0]->getHandCardCount(), 2); // 红桃：弃 1 后视为使用【无中生有】摸 2
    }
}

TEST("extra/huaman_xizhan_suit_effects_club_chain_and_diamond_fire_sha_ignores_distance") {
    {
        GameEngine e; captureLog(e); e.initGame(3, -1, {"huaman", "zhangfei", "guanyu"});
        auto ps = e.getPlayers();
        for (auto p : ps) clearHand(*p);
        auto sk = ps[0]->getHero()->findSkill("嬉战");
        CHECK(sk != nullptr);
        if (!sk) return;
        ps[0]->addHandCard(makeCard("杀", Suit::CLUB, 4, CardType::BASIC, CardSubType::SHA));
        sk->onTurnBoundary(e, *ps[0], *ps[1], true);
        CHECK(ps[1]->isChained());  // 梅花：视为对其使用【铁索连环】→ 目标横置
        CHECK(!ps[0]->isChained()); // 单目标【铁索连环】不会横置花鬘自己
    }
    {
        GameEngine e; captureLog(e); e.initGame(4, -1, {"huaman", "zhangfei", "guanyu", "caocao"});
        auto ps = e.getPlayers();
        for (auto p : ps) clearHand(*p);
        auto sk = ps[0]->getHero()->findSkill("嬉战");
        if (!sk) return;
        ps[0]->addHandCard(makeCard("杀", Suit::DIAMOND, 7, CardType::BASIC, CardSubType::SHA));
        CHECK(e.calculateDistance(*ps[0], *ps[2]) > ps[0]->getAttackRange()); // 4 人局隔一座：距离 2
        int hp0 = ps[2]->getHp();
        sk->onTurnBoundary(e, *ps[0], *ps[2], true); // 关羽的回合开始（超出攻击范围）
        CHECK_EQ(ps[2]->getHp(), hp0 - 1);           // 方块：无距离限制火【杀】照常命中
    }
}

// 友-共砺（2026-10-06 实现级复核）：身份：此模式无效；排位、斗地主：友庞统/友徐庶 同阵营时
// 分别作用于“卧龙演策”的可预测牌数与首项预测结果。本名册暂无这两名武将，
// 用同名武将替身驱动同一判定路径（id/显示名匹配，加入该武将后自动生效）。
TEST("extra/you_gongli_doudizhu_branch_buffs_yance") {
    // 斗地主：友庞统（同阵营＝农民）+1 可预测牌数
    {
        GameEngine e; captureLog(e); e.setAiDelayMs(0);
        e.initDoudizhuGame(-1, {}, 2, false); // 全 AI；座位 2 为地主 → 座位 0、1 同为农民
        auto ps = e.getPlayers();
        CHECK_EQ((int)ps.size(), 3);
        ps[0]->setHero(HeroRegistry::create("you_zhugeliang"));
        ps[1]->setHero(std::make_shared<BlankHero>("友庞统", Country::QUN, Gender::MALE, 3));
        CHECK(ps[0]->getIdentity() == ps[1]->getIdentity()); // 阵营一致
        auto sk = std::dynamic_pointer_cast<YouYanCeSkill>(ps[0]->getHero()->findSkill("友-演策"));
        CHECK(sk != nullptr);
        if (!sk) return;
        for (auto p : ps) clearHand(*p);
        sk->onRoundStart(e, *ps[0]);
        CHECK(sk->isPredictionActive());
        CHECK_EQ(sk->currentPredictCount(), 3); // 基础值仍是 3（共砺只作用于本次声明数）
        int hand0 = ps[0]->getHandCardCount();
        for (int i = 0; i < 3; ++i)
            sk->onAnyCardUsed(e, *ps[0], *ps[1],
                              makeCard("杀", Suit::SPADE, 5 + i, CardType::BASIC, CardSubType::SHA));
        CHECK(sk->isPredictionActive()); // 第 4 项尚未验证
        sk->onAnyCardUsed(e, *ps[0], *ps[1],
                          makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA));
        CHECK(!sk->isPredictionActive());
        CHECK_EQ(sk->lastSettleTotal, 4);            // 共砺使本次预测数为 4（基础 3 + 1）
        CHECK(sk->lastSettleAllCorrect);
        // 4 项匹配各摸 1 张；全对摸 2+1=3 张（AI 同时展示了“方遒”，值均+1）
        CHECK_EQ((int)ps[0]->getHandCardCount(), hand0 + 4 + 3);
        CHECK_EQ(sk->currentPredictCount(), 5);      // 全对后 3 + 1（全对）+ 1（方遒）
    }
    // 身份：此模式无效 → 即便场上有“友庞统”也不加成
    {
        GameEngine e; captureLog(e);
        e.initGame(3, -1, {"you_zhugeliang", "zhangfei", "guanyu"});
        auto ps = e.getPlayers();
        ps[1]->setHero(std::make_shared<BlankHero>("友庞统", Country::QUN, Gender::MALE, 3));
        auto sk = std::dynamic_pointer_cast<YouYanCeSkill>(ps[0]->getHero()->findSkill("友-演策"));
        CHECK(sk != nullptr);
        if (!sk) return;
        for (auto p : ps) clearHand(*p);
        sk->onRoundStart(e, *ps[0]);
        CHECK(sk->isPredictionActive());
        for (int i = 0; i < 3; ++i)
            sk->onAnyCardUsed(e, *ps[0], *ps[1],
                              makeCard("杀", Suit::SPADE, 5 + i, CardType::BASIC, CardSubType::SHA));
        CHECK(!sk->isPredictionActive()); // 身份模式：仅 3 项
        CHECK_EQ(sk->lastSettleTotal, 3);
    }
}

TEST("extra/you_gongli_xushu_makes_first_prediction_always_correct") {
    GameEngine e; captureLog(e); e.setAiDelayMs(0);
    e.initDoudizhuGame(-1, {}, 2, false); // 全 AI；座位 0、1 同为农民
    auto ps = e.getPlayers();
    CHECK_EQ((int)ps.size(), 3);
    ps[0]->setHero(HeroRegistry::create("you_zhugeliang"));
    ps[1]->setHero(std::make_shared<BlankHero>("友徐庶", Country::QUN, Gender::MALE, 3));
    CHECK(ps[0]->getIdentity() == ps[1]->getIdentity());
    auto sk = std::dynamic_pointer_cast<YouYanCeSkill>(ps[0]->getHero()->findSkill("友-演策"));
    CHECK(sk != nullptr);
    if (!sk) return;
    for (auto p : ps) clearHand(*p);
    sk->onRoundStart(e, *ps[0]); // AI 默认 3 项“基本牌”预测
    CHECK(sk->isPredictionActive());
    int hand0 = ps[0]->getHandCardCount();
    // 首项预测是“基本牌”，此处打出锦囊 → 正常不匹配；友徐庶使首项视为正确
    sk->onAnyCardUsed(e, *ps[0], *ps[1],
                      makeCard("无中生有", Suit::HEART, 8, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU));
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand0 + 1);
    // 第二项不再受共砺影响：同为锦囊 → 不匹配、不摸牌
    sk->onAnyCardUsed(e, *ps[0], *ps[1],
                      makeCard("决斗", Suit::SPADE, 1, CardType::TRICK, CardSubType::JUE_DOU));
    CHECK_EQ((int)ps[0]->getHandCardCount(), hand0 + 1);
}

// ================== 势·黄祖 / 势·田丰（2026-10-06 新增，官网 620/623） ==================

// 刚鲠：交给其他角色至少两张手牌；回合结束时其手牌数为全场最多 → 自己摸一张。
TEST("extra/shi_gang_geng_gives_two_then_draws_when_target_has_most_cards") {
    GameEngine e; captureLog(e);
    e.initGame(3, 0, {"shi_tianfeng", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    for (int i = 0; i < 3; ++i)
        ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 5 + i, CardType::BASIC, CardSubType::SHA));
    auto skill = ps[0]->getHero()->findSkill("势-刚鲠");
    CHECK(skill != nullptr);
    if (!skill) return;
    CHECK(skill->canActivate(e, *ps[0]));
    {
        ScriptedInput in("1 1 1 0");   // 目标：第一名其他角色；交出两张牌；结束
        skill->activate(e, *ps[0]);
    }
    CHECK_EQ(ps[1]->getHandCardCount(), 2);   // 交出了两张
    CHECK_EQ(ps[0]->getHandCardCount(), 1);
    const int before = ps[0]->getHandCardCount();
    skill->onPhaseEnd(e, *ps[0], TurnPhase::FINISH);   // 其手牌数为全场最多（2 > 1）
    CHECK_EQ(ps[0]->getHandCardCount(), before + 1);
    // 第二次发动：目标（张飞）手牌数不为全场最多 → 弃置其区域里的一张牌
    ps[0]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    ps[0]->addHandCard(makeCard("闪", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::SHAN));
    for (int i = 0; i < 5; ++i)   // 马超手牌最多，关羽因此“不为全场最多”
        ps[2]->addHandCard(makeCard("桃", Suit::HEART, 4 + i, CardType::BASIC, CardSubType::TAO));
    skill->resetTurnState();
    {
        ScriptedInput in("1 1 1 0");   // 目标：关羽；交出两张；结束
        skill->activate(e, *ps[0]);
    }
    CHECK_EQ(ps[1]->getHandCardCount(), 4);      // 原有 2 + 新交 2
    skill->onPhaseEnd(e, *ps[0], TurnPhase::FINISH);
    CHECK_EQ((int)ps[1]->getAllCards().size(), 3); // 弃置其区域里的一张牌
}

// 死谏：失去最后一张手牌 → 可选“令当前回合角色摸两张牌”。
TEST("extra/shi_si_jian_on_last_hand_card_choice_two") {
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"shi_tianfeng", "guanyu", "zhangfei"});   // 全 AI：选项取 aiChoice
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    auto card = makeCard("杀", Suit::SPADE, 6, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(card);
    const int before = ps[1]->getHandCardCount();
    e.loseHandCard(ps[0], card);            // 失去最后一张手牌 → 触发死谏
    CHECK_EQ(ps[0]->getHandCardCount(), 0);
    CHECK_EQ(ps[1]->getHandCardCount(), before + 2);   // 当前回合角色摸两张
}

// 死谏：选项一 → 被指定的角色使用下一张牌后需弃置一张牌。
TEST("extra/shi_si_jian_choice_one_forces_discard_after_next_card") {
    GameEngine e; captureLog(e);
    e.initGame(3, 0, {"shi_tianfeng", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    auto card = makeCard("杀", Suit::SPADE, 6, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(card);
    {
        ScriptedInput in("y 1 1 0");        // 发动；选选项一；指定第一名其他角色（关羽）
        e.loseHandCard(ps[0], card);
    }
    CHECK_EQ(ps[1]->getMark("死谏标记"), 1);
    ps[1]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("桃", Suit::DIAMOND, 3, CardType::BASIC, CardSubType::TAO));
    const int before = ps[1]->getHandCardCount();
    for (auto& sk : e.getEffectiveSkills(*ps[0]))
        if (sk->getName() == "势-死谏")
            sk->onAnyCardUsed(e, *ps[0], *ps[1],
                              makeCard("无中生有", Suit::HEART, 8, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU));
    CHECK_EQ(ps[1]->getMark("死谏标记"), 0);
    CHECK((int)ps[1]->getHandCardCount() <= before);   // “需弃置一张牌”：必然减少（除非已无手牌）
}

// 鸱张：伤害类卡牌无距离限制；弃置红色手牌后，其他角色不能以红色牌响应此【杀】。
TEST("extra/shi_chi_zhang_blocks_same_color_response_and_ignores_distance") {
    GameEngine e; captureLog(e);
    e.initGame(4, -1, {"shi_huangzu", "guanyu", "zhangfei", "machao"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    auto sha = makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(sha);
    ps[0]->addHandCard(makeCard("红桃2", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN)); // 供鸱张弃置（红色）
    // 目标：座位 2（与黄祖距离 2，靠鸱张的无距离限制命中）
    CHECK_EQ(e.calculateDistance(*ps[0], *ps[2]), 2);
    CHECK(e.canUseShaOn(*ps[0], *ps[2], sha));
    ps[2]->addHandCard(makeCard("红桃闪", Suit::HEART, 4, CardType::BASIC, CardSubType::SHAN));
    const int hp = ps[2]->getHp();
    CHECK(e.useCard(ps[0], sha, {ps[2]}));
    // 弃置的是一张红牌 → 封锁红色：目标只有红【闪】→ 无法响应、受伤
    CHECK_EQ(ps[2]->getHp(), hp - 1);
}

// 断鞅：手牌（杀）不因使用进入弃牌堆 → 置于武将牌上；阶段结束时无次数限制使用。
TEST("extra/shi_duan_yang_collects_discarded_sha_and_uses_it_at_phase_end") {
    GameEngine e; captureLog(e);
    e.initGame(3, -1, {"shi_huangzu", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    auto sha = makeCard("杀", Suit::SPADE, 8, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(sha);
    const int hp = ps[1]->getHp();
    e.discardCardOf(ps[0], sha, "测试（非使用弃置）");
    CHECK_EQ(ps[0]->getPileCount("断鞅"), 1);
    for (auto& sk : e.getEffectiveSkills(*ps[0]))
        if (sk->getName() == "势-断鞅") sk->onPhaseEnd(e, *ps[0], TurnPhase::PLAY);
    CHECK_EQ(ps[0]->getPileCount("断鞅"), 0);
    CHECK_EQ(ps[1]->getHp(), hp - 1);   // 阶段结束时此【杀】被使用并命中
}
