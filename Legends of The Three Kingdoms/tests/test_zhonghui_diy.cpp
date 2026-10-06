#include "test_helpers.h"
#include "Skills.h"
#include "SkillsStd.h"
#include "HeroRegistry.h"

namespace {
struct TestGame {
    GameEngine engine;
    std::shared_ptr<std::ostringstream> log;
    explicit TestGame(int n, const std::vector<std::string>& heroIds = {}, unsigned seed = 7) {
        engine.setSeed(seed);
        log = captureLog(engine);
        engine.initGame(n, -1, heroIds);
    }
};
} // namespace

// ---------------- 登记与包分类 ----------------

TEST("diy/registry_entries") {
    const HeroInfo* jie = HeroRegistry::find("jie_zhonghui");
    CHECK(jie != nullptr);
    CHECK_EQ(jie->pack, std::string("界限突破"));
    const HeroInfo* diy = HeroRegistry::find("diy_zhonghui");
    CHECK(diy != nullptr);
    CHECK_EQ(diy->pack, std::string("DIY包"));

    auto hero = diy->create();
    CHECK_EQ(hero->getSkills().size(), size_t(3));
    auto qm = hero->findSkill("diy-权谋");
    // 描述声明“持恒技”（非“锁定技”）：标签与文本一致（2026-10-06 实现级复核）。
    CHECK(qm != nullptr && qm->getKind() == SkillKind::STATE && qm->isSustained());
    auto qj = hero->findSkill("diy-权计");
    CHECK(qj != nullptr && qj->getKind() == SkillKind::STATE && qj->isUsableActively());
    auto zl = hero->findSkill("diy-自立");
    // 描述为“觉醒技”：觉醒技本身即免于“非锁定技失效”，不再额外标持恒技。
    CHECK(zl != nullptr && zl->getKind() == SkillKind::STATE && zl->hasTag(SkillTag::AWAKEN));

    // 界钟会使用官网技能；自定义技能仅属于 DIY 包
    auto jieHero = jie->create();
    CHECK(jieHero->findSkill("权谋") == nullptr);
    CHECK(jieHero->findSkill("界-权计") != nullptr && !jieHero->findSkill("界-权计")->isUsableActively());
    CHECK(jieHero->findSkill("界-自立") != nullptr);

    auto cats = HeroRegistry::packCategories();
    // 一级分类覆盖原有六包及新增的始计篇、神话再临·阴，标风火林山二级列出五个小包（+友/身份武将）。
    CHECK_EQ(cats.size(), size_t(10));
    CHECK_EQ(cats.front().subpacks.size(), size_t(5));
    bool hasDiy = false, hasJie = false, hasBiao = false, hasFeng = false, hasMou = false;
    bool hasShiji = false, hasYin = false;
    for (const auto& c : cats) {
        if (c.name == "DIY包") hasDiy = true;
        if (c.name == "谋攻篇") hasMou = true;
        if (c.name == "始计篇") hasShiji = true;
        if (c.name == "神话再临·阴") hasYin = true;
        if (c.name == "界限突破") hasJie = true;
        if (c.name == "标风火林山") {
            hasBiao = std::find(c.subpacks.begin(), c.subpacks.end(), "标准包") != c.subpacks.end();
            hasFeng = std::find(c.subpacks.begin(), c.subpacks.end(), "风包") != c.subpacks.end();
        }
    }
    CHECK(hasDiy && hasJie && hasBiao && hasFeng && hasMou && hasShiji && hasYin);
    // 标准包应含 25 名官方标将中的 24 名（黄忠官方属风包）
    size_t biaoCount = 0, fengCount = 0;
    for (const auto& info : HeroRegistry::all()) {
        if (info.pack == "标准包") ++biaoCount;
        if (info.pack == "风包") ++fengCount;
    }
    CHECK(biaoCount >= 24);
    CHECK_EQ(fengCount, size_t(10));
}

// ---------------- 权计（持恒版） ----------------

TEST("diy/quanjiheng_active_draw_and_bank") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    g.engine.setPhase(TurnPhase::PLAY);
    auto quanji = self->getHero()->findSkill("diy-权计");
    CHECK(quanji->isUsableActively());
    CHECK(quanji->canActivate(g.engine, *self));

    int pileBefore = self->getPileCount(PILE_QUAN);
    int handBefore = self->getHandCardCount();
    auto quanmou = std::dynamic_pointer_cast<QuanMouSkill>(self->getHero()->findSkill("diy-权谋"));
    CHECK_EQ(quanmou->getCharge(*self), 1);
    quanji->activate(g.engine, *self);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 1); // 置 1 张"权"
    // 摸1置1后，消耗1点蓄力再摸1，净+1手牌
    CHECK_EQ(self->getHandCardCount(), handBefore + 1);
    CHECK_EQ(quanmou->getCharge(*self), 0);
    CHECK(!quanji->canActivate(g.engine, *self));            // 出牌阶段限一次
    quanji->resetTurnState();
    CHECK(quanji->canActivate(g.engine, *self)); // 下回合可再用
}

TEST("diy/quanji_spends_quanmou_charge_on_any_trigger") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    auto quanmou = std::dynamic_pointer_cast<QuanMouSkill>(self->getHero()->findSkill("diy-权谋"));
    // 受到伤害触发的权计同样消耗1点蓄力并摸牌
    int handBefore = self->getHandCardCount();
    int pileBefore = self->getPileCount(PILE_QUAN);
    quanmou->onAfterDamage(g.engine, *self, nullptr, 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 1);
    CHECK_EQ(self->getHandCardCount(), handBefore + 1);
    CHECK_EQ(quanmou->getCharge(*self), 0);
    CHECK(g.log->str().find("【权谋】消耗1点蓄力") != std::string::npos);
    // 蓄力已归零：再触发只摸1置1，不再额外摸牌
    handBefore = self->getHandCardCount();
    pileBefore = self->getPileCount(PILE_QUAN);
    quanmou->onAfterDamage(g.engine, *self, nullptr, 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 1);
    CHECK_EQ(self->getHandCardCount(), handBefore);
    CHECK_EQ(quanmou->getCharge(*self), 0);
}

TEST("diy/quanjiheng_hand_limit_plus_quan") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto lord = g.engine.getPlayers()[0];
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
    auto quanji = lord->getHero()->findSkill("diy-权计");
    int limit = lord->getHandLimit();
    quanji->onCalculateHandLimit(g.engine, *lord, limit);
    CHECK_EQ(limit, lord->getHp() + 2); // 手牌上限 + 权数
}

// ---------------- 权谋：蓄力成长 ----------------

TEST("diy/quanmou_charge_growth") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    QuanMouSkill quanmou; // 独立实例，直接驱动钩子
    bool skip = false;
    CHECK_EQ(quanmou.getCharge(*self), 1); // 游戏开始时获得首轮起始蓄力
    quanmou.onRoundStart(g.engine, *self);
    CHECK_EQ(quanmou.getCharge(*self), 1); // 游戏开始广播的首轮事件不重复加蓄力
    quanmou.onRoundStart(g.engine, *self);
    CHECK_EQ(quanmou.getCharge(*self), 3); // 第2轮开始 +2 蓄力
    quanmou.onPhaseStart(g.engine, *self, TurnPhase::PLAY, skip);
    CHECK_EQ(quanmou.getCharge(*self), 4); // 自己出牌阶段开始 +1
    quanmou.onPhaseStart(g.engine, *self, TurnPhase::JUDGEMENT, skip);
    CHECK_EQ(quanmou.getCharge(*self), 4); // 其他阶段不加
    quanmou.onRoundStart(g.engine, *self);
    CHECK_EQ(quanmou.getCharge(*self), 5); // 4+2 截到5
    quanmou.onPhaseStart(g.engine, *self, TurnPhase::PLAY, skip);
    CHECK_EQ(quanmou.getCharge(*self), 5); // 蓄力上限5
}

// ---------------- 权谋：伤害触发 ----------------

TEST("diy/quanmou_take_damage_per_point") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    auto quanmou = self->getHero()->findSkill("diy-权谋");
    int pileBefore = self->getPileCount(PILE_QUAN);
    quanmou->onAfterDamage(g.engine, *self, nullptr, 2, ShaElement::NORMAL, nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 2); // 每 1 点伤害发动一次
    CHECK(g.log->str().find("因受到伤害发动【权谋】") != std::string::npos);
}

TEST("diy/quanmou_deal_damage_once_per_turn") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    auto other = g.engine.getPlayers()[1];
    auto quanmou = self->getHero()->findSkill("diy-权谋");
    int pileBefore = self->getPileCount(PILE_QUAN);
    quanmou->onAfterDealDamage(g.engine, *self, other.get(), 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 1);
    quanmou->onAfterDealDamage(g.engine, *self, other.get(), 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 1); // 一回合一次
    quanmou->onTurnStart(g.engine, *self);                   // 新回合重置
    quanmou->onAfterDealDamage(g.engine, *self, other.get(), 1, ShaElement::NORMAL, nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 2);
}

// ---------------- 权谋：出牌阶段结束弃两张牌 ----------------

TEST("diy/quanmou_phase_end_discard_two") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    clearHand(*self);
    for (int i = 0; i < 5; ++i) {
        self->addHandCard(makeCard("杀", Suit::SPADE, 7 + i, CardType::BASIC, CardSubType::SHA));
    }
    auto quanmou = std::dynamic_pointer_cast<QuanMouSkill>(self->getHero()->findSkill("diy-权谋"));
    CHECK(quanmou != nullptr);
    int pileBefore = self->getPileCount(PILE_QUAN);
    quanmou->onPhaseEnd(g.engine, *self, TurnPhase::PLAY);
    // 弃2、权计摸1置1，再消耗1点蓄力摸1；净-1手牌、+1权
    CHECK_EQ(self->getHandCardCount(), 4);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore + 1);
    CHECK_EQ(quanmou->getCharge(*self), 0);
    CHECK(g.log->str().find("弃置两张牌，发动【权谋】") != std::string::npos);
}

TEST("diy/quanmou_phase_end_needs_two_cards") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    clearHand(*self);
    self->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    auto quanmou = std::dynamic_pointer_cast<QuanMouSkill>(self->getHero()->findSkill("diy-权谋"));
    int pileBefore = self->getPileCount(PILE_QUAN);
    quanmou->onPhaseEnd(g.engine, *self, TurnPhase::PLAY);
    CHECK_EQ(self->getPileCount(PILE_QUAN), pileBefore); // 不足两张，不发动
}

// ---------------- 自立（持恒版） ----------------

TEST("diy/zili_const_awakens_and_grants_paiyi") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto lord = g.engine.getPlayers()[0];
    lord->changeHp(-1); // 受伤，觉醒时选择回血
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    lord->addToPile(PILE_QUAN, makeCard("杀", Suit::CLUB, 8, CardType::BASIC, CardSubType::SHA));
    lord->addToPile(PILE_QUAN, makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO));

    auto hero = lord->getHero();
    auto zili = hero->findSkill("diy-自立");
    CHECK(zili->getKind() == SkillKind::STATE); // 持恒技
    bool skip = false;
    zili->onPhaseStart(g.engine, *lord, TurnPhase::PREPARATION, skip);
    CHECK_EQ(lord->getMaxHp(), 4);
    CHECK_EQ(lord->getHp(), 4);
    CHECK(hero->findSkill("diy-排异") != nullptr);
    zili->onPhaseStart(g.engine, *lord, TurnPhase::PREPARATION, skip);
    CHECK_EQ(lord->getMaxHp(), 4); // 觉醒技一次性
    CHECK(g.log->str().find("觉醒技【自立】发动") != std::string::npos);
}

// ---------------- 持恒技不受"非锁定技失效"影响 ----------------

TEST("diy/constant_skills_survive_disable") {
    TestGame g(2, {"diy_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    self->setNonLockSkillsDisabled(true);
    auto eff = g.engine.getEffectiveSkills(*self);
    bool hasQuanMou = false, hasQuanJi = false, hasZiLi = false;
    for (const auto& s : eff) {
        if (s->getName() == "diy-权谋") hasQuanMou = true;
        if (s->getName() == "diy-权计") hasQuanJi = true;
        if (s->getName() == "diy-自立") hasZiLi = true;
    }
    CHECK(hasQuanMou);
    CHECK(hasQuanJi);
    CHECK(hasZiLi);
}

TEST("jie_zhonghui_quanji_only_after_own_play_with_more_cards_than_hp") {
    TestGame g(2, {"jie_zhonghui", "guanyu"});
    auto self = g.engine.getPlayers()[0];
    auto quanji = self->getHero()->findSkill("界-权计");
    clearHand(*self);
    self->setHp(1);
    auto card = makeCard("闪", Suit::HEART, 4, CardType::BASIC, CardSubType::SHAN);
    self->addHandCard(card);
    int before = self->getPileCount(PILE_QUAN);
    quanji->onPhaseEnd(g.engine,*self,TurnPhase::PLAY);
    CHECK_EQ(self->getPileCount(PILE_QUAN),before); // 手牌数等于体力不触发
    self->addHandCard(makeCard("闪",Suit::DIAMOND,5,CardType::BASIC,CardSubType::SHAN));
    quanji->onPhaseEnd(g.engine,*self,TurnPhase::DRAW);
    CHECK_EQ(self->getPileCount(PILE_QUAN),before);
    quanji->onPhaseEnd(g.engine,*self,TurnPhase::PLAY);
    CHECK_EQ(self->getPileCount(PILE_QUAN),before+1);
    CHECK_EQ(self->getHandCardCount(),2); // 摸1置1
    // 他人回合结束时不能触发：即使广播这一钩子。
    quanji->onPhaseEnd(g.engine,*g.engine.getPlayers()[1],TurnPhase::PLAY);
    CHECK_EQ(g.engine.getPlayers()[1]->getPileCount(PILE_QUAN),0);
}

TEST("jie_zhonghui_damage_triggers_quanji_without_diy_quanmou") {
    TestGame g(2,{"jie_zhonghui","guanyu"});
    auto self=g.engine.getPlayers()[0];
    auto skill=self->getHero()->findSkill("界-权计");
    CHECK(self->getHero()->findSkill("diy-权谋")==nullptr);
    int initialHand=self->getHandCardCount();
    skill->onAfterDamage(g.engine,*self,nullptr,2,ShaElement::NORMAL,nullptr);
    CHECK_EQ(self->getPileCount(PILE_QUAN),2);
    CHECK_EQ(self->getHandCardCount(),initialHand);
}

TEST("zhonghui_three_versions_keep_skills_and_awakened_paiyi_isolated") {
    auto normal = HeroRegistry::create("zhonghui");
    auto jie = HeroRegistry::create("jie_zhonghui");
    auto diy = HeroRegistry::create("diy_zhonghui");
    CHECK(normal->findSkill("权谋") == nullptr);
    CHECK(jie->findSkill("权谋") == nullptr);
    CHECK(diy->findSkill("diy-权谋") != nullptr);
    CHECK(std::dynamic_pointer_cast<QuanJiSkill>(normal->findSkill("一-权计")) != nullptr);
    CHECK(std::dynamic_pointer_cast<QuanJiSkill>(jie->findSkill("界-权计")) != nullptr);
    CHECK(std::dynamic_pointer_cast<QuanJiConstSkill>(diy->findSkill("diy-权计")) != nullptr);
    CHECK(std::dynamic_pointer_cast<ZiLiSkill>(normal->findSkill("一-自立")) != nullptr);
    CHECK(std::dynamic_pointer_cast<ZiLiSkill>(jie->findSkill("界-自立")) != nullptr);
    CHECK(std::dynamic_pointer_cast<ZiLiConstSkill>(diy->findSkill("diy-自立")) != nullptr);
    CHECK(normal->findSkill("一-权计")->getDescription() != jie->findSkill("界-权计")->getDescription());
    CHECK(diy->findSkill("diy-权计")->getDescription() != jie->findSkill("界-权计")->getDescription());
    for (const char* id : {"zhonghui", "jie_zhonghui", "diy_zhonghui"}) {
        TestGame g(2,{id,"guanyu"});
        auto self=g.engine.getPlayers()[0];
        for (int i=0;i<3;i++) self->addToPile(PILE_QUAN,
            makeCard("闪",Suit::SPADE,i+1,CardType::BASIC,CardSubType::SHAN));
        bool skip=false;
        std::string pfx = std::string(id)=="diy_zhonghui" ? "diy-" : (std::string(id)=="jie_zhonghui" ? "界-" : "一-");
        auto zl = self->getHero()->findSkill(pfx+"自立");
        CHECK(zl != nullptr);
        if (zl) zl->onPhaseStart(g.engine,*self,TurnPhase::PREPARATION,skip);
        auto py = self->getHero()->findSkill(pfx+"排异");
        CHECK(py!=nullptr);
        if (py) CHECK_EQ(py->getName(), pfx+"排异");
        if (std::string(id)=="diy_zhonghui") {
            CHECK(std::dynamic_pointer_cast<DiyPaiYiSkill>(py)!=nullptr);
            CHECK(std::dynamic_pointer_cast<PaiYiSkill>(py)==nullptr);
        } else {
            CHECK(std::dynamic_pointer_cast<PaiYiSkill>(py)!=nullptr);
            CHECK(std::dynamic_pointer_cast<DiyPaiYiSkill>(py)==nullptr);
        }
    }
}

TEST("official_same_name_skills_share_rules_only_if_official_versions_agree") {
    auto standardZhao=HeroRegistry::create("zhaoyun");
    auto jieZhao=HeroRegistry::create("jie_zhaoyun");
    CHECK_EQ(standardZhao->findSkill("龙胆")->getDescription(),
             jieZhao->findSkill("龙胆")->getDescription()); // 官网 3 / 155
    auto standardMa=HeroRegistry::create("machao");
    auto jieMa=HeroRegistry::create("jie_machao");
    CHECK_EQ(standardMa->findSkill("马术")->getDescription(),
             jieMa->findSkill("马术")->getDescription()); // 官网 6 / 156
    CHECK(standardMa->findSkill("铁骑")->getDescription()!=
          jieMa->findSkill("界-铁骑")->getDescription());
    CHECK(HeroRegistry::create("guanyu")->findSkill("武圣")->getDescription()!=
          HeroRegistry::create("jie_guanyu")->findSkill("界-武圣")->getDescription());
    CHECK(HeroRegistry::create("zhangfei")->findSkill("咆哮")->getDescription()!=
          HeroRegistry::create("jie_zhangfei")->findSkill("界-咆哮")->getDescription());
    CHECK_EQ(HeroRegistry::create("zhouyu")->findSkill("英姿")->getDescription(),
             YingZiSkill().getDescription()); // 孙策【魂姿】获与周瑜相同【英姿】
}

// 回归（2026-10-04 用户反馈）：DIY 钟会【权谋】为蓄力技（1/5），局势查看须显示“蓄力 1/5”
// 而非“蓄力 0/0”（此前未登记“蓄力上限”）。
TEST("diy/zhonghui_charge_display_uses_declared_cap") {
    GameEngine e; captureLog(e);
    e.initGame(2, -1, {"diy_zhonghui", "zhangfei"});
    auto p = e.getPlayers()[0];
    CHECK_EQ(p->getMark("蓄力上限"), 5);
    CHECK_EQ(p->getMark("蓄力"), 1);
    std::string st = e.describePublicState(*p);
    CHECK(st.find("蓄力 1/5") != std::string::npos);
    CHECK(st.find("蓄力 0/0") == std::string::npos);
    // 蓄力技上限 5 不随消耗变化；消耗 1 点后显示 0/5（若已无蓄力）
    p->addMark("蓄力", -p->getMark("蓄力"));
    st = e.describePublicState(*p);
    CHECK(st.find("蓄力 0/5") != std::string::npos);
}
