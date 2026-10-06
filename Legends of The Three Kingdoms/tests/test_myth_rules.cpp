#include "test_helpers.h"
#include "HeroRegistry.h"
#include "AI.h"
#include "SkillsMyth.h"
#include "SkillsStd.h"
#include "Interaction.h"
#include <iostream>
#include <set>
#include <algorithm>

TEST("myth/juxiang_only_claims_other_players_nanman") {
    GameEngine e; captureLog(e);e.initGame(2,-1,{"guanyu","zhurong"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto nanman=makeCard("南蛮入侵",Suit::SPADE,7,CardType::TRICK,CardSubType::NAN_MAN_RU_QIN);
    ps[0]->addHandCard(nanman);
    e.useCard(ps[0],nanman,{});
    CHECK(ps[1]->hasHandCard(nanman));
    CHECK(e.getDeck().getDiscardPileSize()==0);
    // 祝融自己使用时不会获得自己的南蛮。
    ps[1]->removeHandCard(nanman);ps[1]->addHandCard(nanman);
    e.useCard(ps[1],nanman,{});
    CHECK(!ps[1]->hasHandCard(nanman));
}

TEST("myth/roulin_applies_in_both_directions") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"diaochan","dongzhuo"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    ps[1]->setHp(4);
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("闪",Suit::DIAMOND,3,CardType::BASIC,CardSubType::SHAN));
    auto sha=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);e.useCard(ps[0],sha,{ps[1]});
    CHECK_EQ(ps[1]->getHp(),4);
    CHECK_EQ(ps[1]->getHandCardCount(),0);
}

TEST("myth/mobile_guhuo_heart_truth_challenger_loses_hp_and_has_no_usage_limit") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"yuji","guanyu"});
    unsigned challengerSeed=0;
    for(unsigned n=1;n<100;n++) {std::mt19937 test(n);if(test()%5==0){challengerSeed=n;break;}}
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto real=makeCard("桃",Suit::HEART,4,CardType::BASIC,CardSubType::TAO);
    ps[0]->addHandCard(real);
    auto bluff=e.getConversionsFor(ps[0],real,CardSubType::TAO);
    CHECK(!bluff.empty());
    e.setSeed(challengerSeed);
    CHECK(e.resolveGuHuo(ps[0],bluff.front()));
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp()-1);
    CHECK(ps[1]->getHero()->findSkill("缠怨")==nullptr);
    e.setSeed(challengerSeed);
    CHECK(e.resolveGuHuo(ps[0],bluff.front()));
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp()-2);
    CHECK(!e.getConversionsFor(ps[0],real,CardSubType::TAO).empty());
}

// 临时向真人交互注入有界脚本；不触发 EOF 的全局自动选择状态。
namespace {
struct MythInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit MythInput(const std::string& text) : script(text), saved(std::cin.rdbuf(script.rdbuf())) { Interaction::resetInputState(); }
    ~MythInput() { std::cin.rdbuf(saved); Interaction::resetInputState(); }
};
}

TEST("myth/mobile_hongyan_and_tianxiang_transfers_original_damage") {
    GameEngine e;captureLog(e);e.initGame(3,1,{"guanyu","xiaoqiao","zhangfei"});
    auto ps=e.getPlayers();
    auto spade=makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA);
    auto club=makeCard("杀",Suit::CLUB,10,CardType::BASIC,CardSubType::SHA);
    CHECK(e.effectiveSuit(*ps[1],spade)==Suit::HEART);
    CHECK(e.effectiveSuit(*ps[0],spade)==Suit::SPADE);
    e.getDeck().putOnTop({spade});
    CHECK(e.effectiveSuit(*ps[1],e.doJudgement(ps[1],"红颜"))==Suit::HEART);
    clearHand(*ps[1]);clearHand(*ps[2]);
    ps[1]->addHandCard(spade);ps[1]->addHandCard(club);
    int hp=ps[1]->getHp(), targetHp=ps[2]->getHp();
    {
        MythInput input("2\n1\n"); // 指定张飞、弃黑桃视为红桃，转移全部伤害
        e.applyDamage(ps[0],ps[1],2);
    }
    CHECK_EQ(ps[1]->getHp(),hp);
    CHECK_EQ(ps[2]->getHp(),targetHp-2);
    CHECK(!ps[2]->hasHandCard(spade));
    CHECK(ps[1]->hasHandCard(club));
}

TEST("myth/yeyan_heavy_three_damage_pays_four_suits_and_three_hp") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhouyu","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);auto skill=ps[0]->getHero()->findSkill("业炎");
    clearHand(*ps[0]);clearHand(*ps[1]);ps[0]->setHp(4);ps[1]->setHp(4);
    for(auto suit:{Suit::SPADE,Suit::HEART,Suit::CLUB,Suit::DIAMOND})
        ps[0]->addHandCard(makeCard("杀",suit,8,CardType::BASIC,CardSubType::SHA));
    {
        MythInput input("2\n3\n1\n1\n1\n1\n"); // 关羽承受3点；弃四色
        skill->activate(e,*ps[0]);
    }
    CHECK_EQ(ps[0]->getHandCardCount(),0);
    CHECK_EQ(ps[0]->getHp(),1);
    CHECK_EQ(ps[1]->getHp(),1);
    CHECK(!skill->canActivate(e,*ps[0]));
}

TEST("myth/yeyan_light_can_split_without_cost") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_zhouyu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);auto skill=ps[0]->getHero()->findSkill("业炎");
    clearHand(*ps[0]);for(auto p:ps)p->setHp(4);
    {
        MythInput input("2\n2\n0\n"); // 关羽和张飞各一点，不选自己
        skill->activate(e,*ps[0]);
    }
    CHECK_EQ(ps[0]->getHp(),4);
    CHECK_EQ(ps[1]->getHp(),3);
    CHECK_EQ(ps[2]->getHp(),3);
}

TEST("myth/mobile_qixing_starts_with_seven_stars_and_may_exchange_after_draw") {
    GameEngine e;captureLog(e);
    {
        MythInput input("n\n"); // 移动版起始可选择不换星
        e.initGame(2,0,{"shen_zhugeliang","guanyu"});
    }
    auto p=e.getPlayers()[0];auto skill=p->getHero()->findSkill("七星");
    CHECK_EQ(p->getHandCardCount(),4);
    CHECK_EQ(p->getPileCount("星"),7);
    auto a=p->getHandCards()[0],b=p->getHandCards()[1];
    auto x=p->getPile("星")[0],y=p->getPile("星")[1];
    {
        MythInput input("y\n2\n1\n1\n1\n1\n");
        skill->onPhaseEnd(e,*p,TurnPhase::DRAW);
    }
    CHECK_EQ(p->getHandCardCount(),4);
    CHECK_EQ(p->getPileCount("星"),7);
    CHECK(p->hasHandCard(x));CHECK(p->hasHandCard(y));
    CHECK(!p->hasHandCard(a));CHECK(!p->hasHandCard(b));
    CHECK(std::find(p->getPile("星").begin(),p->getPile("星").end(),a)!=p->getPile("星").end());
}

TEST("myth/dawu_multi_target_and_kuangfeng_expire_next_turn") {
    GameEngine e;captureLog(e);
    {
        MythInput input("1\n1\n1\n1\n1\n1\n1\n");
        e.initGame(3,0,{"shen_zhugeliang","guanyu","zhangfei"});
    }
    auto ps=e.getPlayers();for(auto p:ps) p->setHp(4);
    auto fog=ps[0]->getHero()->findSkill("大雾"),wind=ps[0]->getHero()->findSkill("狂风");
    {
        MythInput input("y\n2\n1\n1\n"); // 两星，为自己和关羽造雾
        fog->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
    }
    CHECK_EQ(ps[0]->getPileCount("星"),5);
    CHECK_EQ(ps[0]->getMark("雾"),1);CHECK_EQ(ps[1]->getMark("雾"),1);
    e.applyDamage(ps[2],ps[1],2,ShaElement::FIRE);
    CHECK_EQ(ps[1]->getHp(),4);
    e.applyDamage(ps[2],ps[1],1,ShaElement::THUNDER);
    CHECK_EQ(ps[1]->getHp(),3);
    {
        MythInput input("y\n3\n"); // 狂风指定张飞（也可指定自己）
        wind->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
    }
    CHECK_EQ(ps[2]->getMark("风"),1);
    e.applyDamage(ps[1],ps[2],1,ShaElement::FIRE);
    CHECK_EQ(ps[2]->getHp(),2);
    fog->onTurnBoundary(e,*ps[0],*ps[0],true);
    wind->onTurnBoundary(e,*ps[0],*ps[0],true);
    CHECK_EQ(ps[1]->getMark("雾"),0);CHECK_EQ(ps[2]->getMark("风"),0);
}

TEST("myth/huashen_borrows_only_shown_avatar_and_newborn_gains_per_damage") {
    GameEngine e;e.setSeed(13);captureLog(e);e.initGame(2,-1,{"zuoci","guanyu"});
    auto ps=e.getPlayers();auto skill=std::dynamic_pointer_cast<MythSkill>(ps[0]->getHero()->findSkill("化身"));
    CHECK(skill!=nullptr);
    CHECK_EQ(skill->getAvatarIds().size(),size_t(2));
    auto h=HeroRegistry::create(skill->getShownAvatarId());
    CHECK(h!=nullptr);
    if(h) {
        CHECK(ps[0]->getHero()->getCountry()==h->getCountry());
        CHECK(ps[0]->getHero()->getGender()==h->getGender());
        if(!skill->getBorrowedSkillName().empty())
            CHECK(h->findSkill(skill->getBorrowedSkillName())!=nullptr);
    }
    CHECK(HeroRegistry::personKey(skill->getAvatarIds()[0])!=HeroRegistry::personKey(skill->getAvatarIds()[1]));
    ps[0]->setHp(3);e.applyDamage(ps[1],ps[0],2);
    CHECK_EQ(skill->getAvatarIds().size(),size_t(4));
    std::set<std::string> persons;
    for(const auto& id:skill->getAvatarIds())CHECK(persons.insert(HeroRegistry::personKey(id)).second);
}

TEST("myth/hongyan_spade_is_red_for_weimu_and_renwang") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"xiaoqiao","jiaxu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto spadeTrick=makeCard("决斗",Suit::SPADE,7,CardType::TRICK,CardSubType::JUE_DOU);
    auto clubTrick=makeCard("决斗",Suit::CLUB,7,CardType::TRICK,CardSubType::JUE_DOU);
    CHECK(e.canBeTargeted(ps[1],spadeTrick,ps[0]));
    CHECK(!e.canBeTargeted(ps[1],clubTrick,ps[0]));
    ps[1]->equip(makeCard("仁王盾",Suit::CLUB,2,CardType::EQUIPMENT,CardSubType::ARMOR));
    int hp=ps[1]->getHp();
    auto spadeSha=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(spadeSha);e.useCard(ps[0],spadeSha,{ps[1]});
    CHECK_EQ(ps[1]->getHp(),hp-1);
    auto clubSha=makeCard("杀",Suit::CLUB,8,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(clubSha);e.useCard(ps[0],clubSha,{ps[1]});
    CHECK_EQ(ps[1]->getHp(),hp-1);
}

TEST("myth/mobile_tianxiang_transfers_two_damage_and_draws_lost_hp") {
    GameEngine e;captureLog(e);e.initGame(3,1,{"guanyu","xiaoqiao","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[1]->addHandCard(makeCard("桃",Suit::HEART,3,CardType::BASIC,CardSubType::TAO));
    int hp=ps[1]->getHp();ps[2]->setHp(3);
    {
        MythInput input("2\n1\n"); // 全部2点伤害转移；随后摸损失体力张牌
        e.applyDamage(ps[0],ps[1],2);
    }
    CHECK_EQ(ps[1]->getHp(),hp);
    CHECK_EQ(ps[2]->getHp(),1);
    CHECK_EQ(ps[2]->getHandCardCount(),3);
}

TEST("myth/huashen_reveal_switches_only_to_selected_avatar") {
    GameEngine e;e.setSeed(48);captureLog(e);
    {
        MythInput input("1\n1\n"); // 先亮出第一张及其第一技能
        e.initGame(2,0,{"zuoci","guanyu"});
    }
    auto p=e.getPlayers()[0];auto skill=std::dynamic_pointer_cast<MythSkill>(p->getHero()->findSkill("化身"));
    CHECK(skill!=nullptr);
    auto first=skill->getAvatarIds()[0],second=skill->getAvatarIds()[1];
    CHECK_EQ(skill->getShownAvatarId(),first);
    auto before=skill->getBorrowedSkillName();
    {
        MythInput input("y\n2\n1\n");
        skill->onTurnEnd(e,*p,*p);
    }
    CHECK_EQ(skill->getShownAvatarId(),second);
    auto newHero=HeroRegistry::create(second);
    CHECK(p->getHero()->getCountry()==newHero->getCountry());
    CHECK(p->getHero()->getGender()==newHero->getGender());
    if(!skill->getBorrowedSkillName().empty())
        CHECK(newHero->findSkill(skill->getBorrowedSkillName())!=nullptr);
    if(!before.empty() && before!=skill->getBorrowedSkillName())
        CHECK(p->getHero()->findSkill(before)==nullptr);
}

TEST("myth/mobile_buqu_keeps_nonpositive_hp_with_one_unique_wound_per_point") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"zhoutai","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto unique=makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA);
    auto second=makeCard("杀",Suit::HEART,8,CardType::BASIC,CardSubType::SHA);
    auto third=makeCard("杀",Suit::CLUB,7,CardType::BASIC,CardSubType::SHA);
    e.getDeck().putOnTop({unique,second,third});ps[0]->setHp(1);
    e.applyDamage(ps[1],ps[0],3);
    CHECK(ps[0]->isAlive());CHECK_EQ(ps[0]->getHp(),-2);
    CHECK_EQ(ps[0]->getPileCount("创"),3);
    int limit=ps[0]->getHandLimit();
    ps[0]->getHero()->findSkill("不屈")->onCalculateHandLimit(e,*ps[0],limit);
    CHECK_EQ(limit,ps[0]->getHandLimit()); // 官网不含创牌修正手牌上限。
    auto repeated=makeCard("闪",Suit::HEART,9,CardType::BASIC,CardSubType::SHAN);
    e.getDeck().putOnTop({repeated});
    e.applyDamage(ps[1],ps[0],1);
    CHECK(!ps[0]->isAlive());
    CHECK_EQ(ps[0]->getPileCount("创"),0);
}

TEST("myth/yinghun_may_choose_one_draw_then_discard_lost_hp") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"sunjian","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[1]);ps[0]->setHp(2);
    bool skipped=false;
    {
        MythInput input("1\n2\n"); // 目标关羽、摸1弃2；手牌不足时弃所有
        ps[0]->getHero()->findSkill("英魂")->onPhaseStart(e,*ps[0],TurnPhase::PREPARATION,skipped);
    }
    CHECK_EQ(ps[1]->getHandCardCount(),0);
}

TEST("myth/mobile_shensu_does_not_skip_discard_as_third_choice") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"xiahouyuan","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    e.phaseDiscard(ps[0]);
    CHECK(!ps[0]->isTurnedOver());
    CHECK(ps[0]->getHero()->findSkill("神速")!=nullptr);
}

TEST("myth/dimeng_can_pay_with_equipment_and_exchange_zero_hand") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"lusu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("杀",Suit::HEART,1,CardType::BASIC,CardSubType::SHA));
    auto armor=makeCard("八卦阵",Suit::CLUB,3,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[0]->equip(armor);
    ps[2]->addHandCard(makeCard("闪",Suit::CLUB,8,CardType::BASIC,CardSubType::SHAN));
    ps[2]->addHandCard(makeCard("桃",Suit::HEART,9,CardType::BASIC,CardSubType::TAO));
    auto skill=ps[0]->getHero()->findSkill("缔盟");CHECK(skill->canActivate(e,*ps[0]));
    {
        MythInput input("1\n2\n1\n1\n");
        skill->activate(e,*ps[0]);
    }
    CHECK_EQ(ps[0]->getHandCardCount(),0);
    CHECK(ps[0]->getArmor()==nullptr);
    CHECK_EQ(ps[1]->getHandCardCount(),2);
    CHECK_EQ(ps[2]->getHandCardCount(),0);
}

TEST("myth/mobile_wuhun_only_marks_the_source_of_damage_taken") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"zhangfei","shen_guanyu","lvmeng"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[1]->setHp(2);ps[2]->setHp(4);
    e.applyDamage(ps[1],ps[2],1);
    CHECK_EQ(ps[2]->getMark("魇"),0);
    e.getDeck().putOnTop({makeCard("杀",Suit::HEART,9,CardType::BASIC,CardSubType::SHA)});
    e.applyDamage(ps[2],ps[1],2);
    CHECK_EQ(ps[2]->getMark("魇"),2);
    CHECK(!ps[1]->isAlive());
    CHECK(!ps[2]->isAlive());
}

TEST("engine/extra_turn_queue_keeps_multiple_grants") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,-1,{"guanyu","zhangfei"});
    for(auto p:e.getPlayers())clearHand(*p);
    auto p=e.getPlayers()[0];
    e.scheduleExtraTurn(p);e.scheduleExtraTurn(p);
    e.runTurn(p);
    auto text=log->str();auto first=text.find("获得一个额外回合！");
    CHECK(first!=std::string::npos);
    CHECK(text.find("获得一个额外回合！",first+1)!=std::string::npos);
}

TEST("myth/jiyang_draws_once_per_red_sha_target") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"sunce","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto sha=makeCard("杀",Suit::HEART,7,CardType::BASIC,CardSubType::SHA);
    ps[0]->equip(makeCard("方天画戟",Suit::DIAMOND,12,CardType::EQUIPMENT,CardSubType::WEAPON));
    ps[0]->addHandCard(sha);
    e.useCard(ps[0],sha,{ps[1],ps[2]});
    CHECK_EQ(ps[0]->getHandCardCount(),2);
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp()-1);
    CHECK_EQ(ps[2]->getHp(),ps[2]->getMaxHp()-1);
}

TEST("myth/huangtian_is_available_during_actor_play_and_once_per_actor_turn") {
    GameEngine e;captureLog(e);e.initGame(2,1,{"zhangjiao","yuji"});
    auto ps=e.getPlayers();clearHand(*ps[1]);
    auto shan=makeCard("闪",Suit::HEART,8,CardType::BASIC,CardSubType::SHAN);
    ps[1]->addHandCard(shan);
    auto skill=ps[0]->getHero()->findSkill("黄天");
    CHECK(skill->canDelegate(e,*ps[0],*ps[1]));
    {
        MythInput input("1\n");
        skill->invokeDelegated(e,*ps[0],*ps[1]);
    }
    CHECK(ps[0]->hasHandCard(shan));
    CHECK(!skill->canDelegate(e,*ps[0],*ps[1]));
}

TEST("myth/zhiba_challenger_loses_and_lord_claims_both_cards") {
    GameEngine e;captureLog(e);e.initGame(2,1,{"sunce","lusu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto large=makeCard("杀",Suit::HEART,13,CardType::BASIC,CardSubType::SHA);
    auto small=makeCard("闪",Suit::SPADE,1,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(large);ps[1]->addHandCard(small);
    auto skill=ps[0]->getHero()->findSkill("制霸");CHECK(skill->canDelegate(e,*ps[0],*ps[1]));
    {
        MythInput input("1\n");
        skill->invokeDelegated(e,*ps[0],*ps[1]);
    }
    CHECK(ps[0]->hasHandCard(large));CHECK(ps[0]->hasHandCard(small));
    CHECK(!skill->canDelegate(e,*ps[0],*ps[1]));
}

TEST("myth/juejing_draws_when_entering_and_leaving_dying_not_during_draw_phase") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_zhaoyun","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    ps[0]->addHandCard(makeCard("桃",Suit::HEART,7,CardType::BASIC,CardSubType::TAO));
    e.getDeck().putOnTop({makeCard("杀",Suit::CLUB,5,CardType::BASIC,CardSubType::SHA),
                          makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA)});
    ps[0]->setHp(1);
    e.applyDamage(ps[1],ps[0],1);
    CHECK(ps[0]->isAlive());CHECK_EQ(ps[0]->getHp(),1);
    CHECK_EQ(ps[0]->getHandCardCount(),2);
    int count=2;ps[0]->getHero()->findSkill("绝境")->onDrawCards(e,*ps[0],count);
    CHECK_EQ(count,2);
}

TEST("myth/mobile_wuqian_persists_after_damage_then_clears_on_missed_sha") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_lvbu","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);clearHand(*ps[0]);clearHand(*ps[1]);
    ps[0]->addMark("暴怒",2);
    ps[1]->equip(makeCard("仁王盾",Suit::CLUB,7,CardType::EQUIPMENT,CardSubType::ARMOR));
    auto skill=ps[0]->getHero()->findSkill("无前");
    {
        MythInput input("2\n");skill->activate(e,*ps[0]);
    }
    CHECK(ps[0]->getHero()->findSkill("无双")!=nullptr);
    auto sha=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);int hp=ps[1]->getHp();
    e.useCard(ps[0],sha,{ps[1]});CHECK_EQ(ps[1]->getHp(),hp-1);
    skill->onTurnBoundary(e,*ps[0],*ps[0],false);
    CHECK(ps[0]->getHero()->findSkill("无双")!=nullptr);
    CHECK_EQ(ps[0]->getMark("无前目标:1"),1);
    auto missed=makeCard("杀",Suit::HEART,8,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(missed);
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("闪",Suit::DIAMOND,3,CardType::BASIC,CardSubType::SHAN));
    CHECK(e.useCard(ps[0],missed,{ps[1]}));
    CHECK_EQ(ps[1]->getHp(),hp-1);
    CHECK_EQ(ps[0]->getMark("无前目标:1"),0);
    CHECK(ps[0]->getHero()->findSkill("无双")==nullptr);
}

TEST("engine/field_movement_enforces_equipment_slot_and_delayed_duplicate") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"zhanghe","guanyu","zhangfei"});
    auto ps=e.getPlayers();
    auto armor=makeCard("仁王盾",Suit::CLUB,7,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[0]->equip(armor);
    ps[1]->equip(makeCard("八卦阵",Suit::SPADE,7,CardType::EQUIPMENT,CardSubType::ARMOR));
    CHECK(!e.canMoveFieldCard(ps[0],ps[1],armor));
    CHECK(e.moveFieldCard(ps[0],ps[2],armor));
    CHECK(ps[2]->getArmor()==armor);
    CHECK(!e.canMoveFieldCard(ps[2],ps[2],armor));
}

TEST("myth/longhun_two_hearts_recover_two_hp_and_consume_both") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhaoyun","zhangfei"});
    auto ps=e.getPlayers();clearHand(*ps[0]);ps[0]->changeMaxHp(2);ps[0]->setHp(1);
    auto a=makeCard("杀",Suit::HEART,3,CardType::BASIC,CardSubType::SHA);
    auto b=makeCard("闪",Suit::HEART,7,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(a);ps[0]->addHandCard(b);
    auto converted=e.getConversionsFor(ps[0],a,CardSubType::TAO);
    CHECK(!converted.empty());
    {
        MythInput input("y\n1\n");e.useCard(ps[0],converted.front(),{});
    }
    CHECK_EQ(ps[0]->getHp(),3);CHECK_EQ(ps[0]->getHandCardCount(),0);
}

TEST("myth/longhun_two_diamonds_fire_damage_plus_one") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhaoyun","zhangfei"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto a=makeCard("闪",Suit::DIAMOND,3,CardType::BASIC,CardSubType::SHAN);
    auto b=makeCard("杀",Suit::DIAMOND,7,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(a);ps[0]->addHandCard(b);
    auto converted=e.getConversionsFor(ps[0],a,CardSubType::SHA);
    CHECK(!converted.empty());int hp=ps[1]->getHp();
    {
        MythInput input("y\n1\n");e.useCard(ps[0],converted.front(),{ps[1]});
    }
    CHECK_EQ(ps[1]->getHp(),hp-2);CHECK_EQ(ps[0]->getHandCardCount(),0);
}

TEST("myth/longhun_two_clubs_can_respond_and_consume_both") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhaoyun","zhangfei"});
    auto ps=e.getPlayers();clearHand(*ps[0]);
    ps[0]->addHandCard(makeCard("杀",Suit::CLUB,3,CardType::BASIC,CardSubType::SHA));
    ps[0]->addHandCard(makeCard("杀",Suit::CLUB,5,CardType::BASIC,CardSubType::SHA));
    {
        MythInput input("1\ny\n1\n");
        CHECK(e.askResponseCard(ps[0],CardSubType::SHAN,"闪")!=nullptr);
    }
    CHECK_EQ(ps[0]->getHandCardCount(),0);
}

TEST("myth/mobile_jushou_draws_three_then_turns_over_without_spending_card") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"caoren","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);
    auto armor=makeCard("八卦阵",Suit::SPADE,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    e.getDeck().putOnTop({armor,
        makeCard("杀",Suit::HEART,3,CardType::BASIC,CardSubType::SHA),
        makeCard("杀",Suit::CLUB,4,CardType::BASIC,CardSubType::SHA)});
    bool skipped=false;
    {MythInput input("y\n");ps[0]->getHero()->findSkill("据守")->onPhaseStart(e,*ps[0],TurnPhase::FINISH,skipped);}
    CHECK(ps[0]->isTurnedOver());
    CHECK(ps[0]->getArmor()==nullptr);
    CHECK(ps[0]->hasHandCard(armor));
    CHECK_EQ(ps[0]->getHandCardCount(),3);
}

TEST("myth/mobile_zhoutai_has_only_buqu_no_fenji") {
    auto hero=HeroRegistry::create("zhoutai");
    CHECK(hero->findSkill("不屈")!=nullptr);
    CHECK(hero->findSkill("奋激")==nullptr);
    CHECK_EQ(hero->getSkills().size(),1u);
}

TEST("myth/guhuo_can_declare_each_basic_or_non_delayed_trick") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"yuji","guanyu"});
    auto p=e.getPlayers()[0];clearHand(*p);
    auto physical=makeCard("闪",Suit::CLUB,4,CardType::BASIC,CardSubType::SHAN);
    p->addHandCard(physical);
    for(CardSubType kind:{CardSubType::SHA,CardSubType::SHAN,CardSubType::TAO,CardSubType::JIU,
            CardSubType::JUE_DOU,CardSubType::HUO_GONG,CardSubType::GUO_HE_CHAI_QIAO,
            CardSubType::SHUN_SHOU_QIAN_YANG,CardSubType::NAN_MAN_RU_QIN,CardSubType::WAN_JIAN_QI_FA,
            CardSubType::WU_ZHONG_SHENG_YOU,CardSubType::TAO_YUAN_JIE_YI,
            CardSubType::WU_XIE_KE_JI,CardSubType::WU_GU_FENG_DENG,
            CardSubType::JIE_DAO_SHA_REN,CardSubType::TIE_SUO_LIAN_HUAN}) {
        auto variants=e.getConversionsFor(p,physical,kind);
        CHECK(!variants.empty());
    }
    CHECK(e.getConversionsFor(p,physical,CardSubType::LE_BU_SI_SHU).empty());
}

TEST("myth/mobile_jixi_keeps_field_card_when_only_target_has_qianxun") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"dengai","luxun","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto field=makeCard("杀",Suit::CLUB,7,CardType::BASIC,CardSubType::SHA);
    auto luxunHand=makeCard("闪",Suit::HEART,4,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addToPile("田",field);ps[1]->addHandCard(luxunHand);
    auto jixi=std::make_shared<MythSkill>("急袭","田当顺手牵羊",SkillKind::ACTIVE);
    ps[0]->getHero()->addSkill(jixi);
    CHECK(!jixi->canActivate(e,*ps[0]));
    jixi->activate(e,*ps[0]);
    CHECK_EQ(ps[0]->getPileCount("田"),1);
    CHECK(ps[1]->hasHandCard(luxunHand));
    auto otherHand=makeCard("桃",Suit::DIAMOND,9,CardType::BASIC,CardSubType::TAO);
    ps[2]->addHandCard(otherHand);
    CHECK(jixi->canActivate(e,*ps[0]));
    jixi->activate(e,*ps[0]);
    CHECK_EQ(ps[0]->getPileCount("田"),0);
    CHECK(ps[0]->hasHandCard(otherHand));
    CHECK(ps[1]->hasHandCard(luxunHand));
}

TEST("myth/mobile_wushen_heart_cards_ignore_distance_but_not_sha_use_limit") {
    GameEngine e;auto log=captureLog(e);e.initGame(4,0,{"shen_guanyu","xunyu","zhangfei","lvmeng"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto a=makeCard("桃",Suit::HEART,4,CardType::BASIC,CardSubType::TAO);
    auto b=makeCard("杀",Suit::HEART,6,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(a);ps[0]->addHandCard(b);
    CHECK(e.getResponseCandidates(ps[0],CardSubType::TAO).empty());
    CHECK(!e.canUseOriginalCard(*ps[0],a));CHECK(!e.canUseOriginalCard(*ps[0],b));
    int hp=ps[2]->getHp();CHECK(e.calculateDistance(*ps[0],*ps[2])>1);
    auto first=e.getConversionsFor(ps[0],a,CardSubType::SHA).front();
    CHECK(e.canUseShaOn(*ps[0],*ps[2],first));
    CHECK_EQ(e.getShaLimitForCard(*ps[0],first),1);
    {MythInput input("1\n2\n1\n0\n");e.runTurn(ps[0]);}
    CHECK_EQ(ps[0]->getShaCountThisTurn(),1);
    CHECK_EQ(ps[2]->getHp(),hp-1);
    CHECK(ps[0]->hasHandCard(b));
    CHECK(log->str().find("本回合使用【杀】的次数已达上限")!=std::string::npos);
}
TEST("myth/longhun_can_use_equipped_card_as_primary_conversion") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhaoyun","zhangfei"});
    auto ps=e.getPlayers();clearHand(*ps[0]);ps[0]->setHp(1);
    auto weapon=makeCard("诸葛连弩",Suit::HEART,1,CardType::EQUIPMENT,CardSubType::WEAPON);
    ps[0]->equip(weapon);
    auto variant=e.getConversionsFor(ps[0],weapon,CardSubType::TAO);
    CHECK(!variant.empty());CHECK(e.useCard(ps[0],variant.front(),{}));
    CHECK_EQ(ps[0]->getHp(),2);CHECK(ps[0]->getWeapon()==nullptr);
}

TEST("myth/qiaobian_skipped_draw_steals_one_hand_card_from_each_of_two_people") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"zhanghe","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,1,CardType::BASIC,CardSubType::SHA));
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    ps[2]->addHandCard(makeCard("闪",Suit::DIAMOND,3,CardType::BASIC,CardSubType::SHAN));
    bool skip=false;
    {
        MythInput input("y\n1\n1\n1\n1\n1\n");
        ps[0]->getHero()->findSkill("巧变")->onPhaseStart(e,*ps[0],TurnPhase::DRAW,skip);
    }
    CHECK(skip);CHECK_EQ(ps[0]->getHandCardCount(),2);
    CHECK_EQ(ps[1]->getHandCardCount(),0);CHECK_EQ(ps[2]->getHandCardCount(),0);
}

TEST("myth/longhun_two_black_shan_discards_active_turn_players_card") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,0,{"guanyu","shen_zhaoyun"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA));
    ps[0]->addHandCard(makeCard("闪",Suit::HEART,6,CardType::BASIC,CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("杀",Suit::CLUB,7,CardType::BASIC,CardSubType::SHA));
    ps[1]->addHandCard(makeCard("杀",Suit::CLUB,8,CardType::BASIC,CardSubType::SHA));
    {
        MythInput input("1\n1\n0\n");e.runTurn(ps[0]);
    }
    CHECK_EQ(ps[1]->getHandCardCount(),0);
    CHECK(log->str().find("（龙魂）")!=std::string::npos);
}

TEST("engine/sha_target_picker_excludes_locked_unselectable_kongcheng") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"zhangfei","zhugeliang"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto sha=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);
    CHECK(e.getShaTargets(*ps[0],sha).empty());
    CHECK(!e.useCard(ps[0],sha,{ps[1]}));
    CHECK(ps[0]->hasHandCard(sha));
}

TEST("myth/zhiba_awakened_lord_may_refuse_but_challenge_is_spent") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"sunce","lusu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,4,CardType::BASIC,CardSubType::SHA));
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,6,CardType::BASIC,CardSubType::SHAN));
    ps[0]->addMark("魂姿已觉醒",1);
    auto skill=ps[0]->getHero()->findSkill("制霸");
    CHECK(skill->canDelegate(e,*ps[0],*ps[1]));
    {
        MythInput input("n\n");skill->invokeDelegated(e,*ps[0],*ps[1]);
    }
    CHECK_EQ(ps[0]->getHandCardCount(),1);CHECK_EQ(ps[1]->getHandCardCount(),1);
    CHECK(!skill->canDelegate(e,*ps[0],*ps[1]));
}

TEST("myth/shenfen_deals_damage_before_discarding_equipment_and_up_to_four_hands") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",6);
    for(int i=0;i<6;i++)ps[1]->addHandCard(makeCard("杀",Suit::SPADE,i+1,CardType::BASIC,CardSubType::SHA));
    ps[1]->equip(makeCard("八卦阵",Suit::SPADE,2,CardType::EQUIPMENT,CardSubType::ARMOR));
    int before=ps[1]->getHp();
    ps[0]->getHero()->findSkill("神愤")->activate(e,*ps[0]);
    CHECK_EQ(ps[1]->getHp(),before-1);
    CHECK_EQ(ps[1]->getHandCardCount(),2);
    CHECK(ps[1]->getArmor()==nullptr);
    CHECK(ps[0]->isTurnedOver());
}

TEST("myth/wuqian_ignores_selected_targets_armor_for_non_sha_damage_too") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_lvbu","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);ps[0]->addMark("暴怒",2);
    ps[1]->equip(makeCard("白银狮子",Suit::CLUB,7,CardType::EQUIPMENT,CardSubType::ARMOR));
    {
        MythInput input("2\n");ps[0]->getHero()->findSkill("无前")->activate(e,*ps[0]);
    }
    int hp=ps[1]->getHp();e.applyDamage(ps[0],ps[1],2,ShaElement::NORMAL);
    CHECK_EQ(ps[1]->getHp(),hp-2);
}

TEST("myth/zhiji_offers_healing_before_max_hp_loss_and_grants_real_guanxing") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"jiangwei","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);ps[0]->setHp(2);
    int maxHp=ps[0]->getMaxHp();bool skip=false;
    {
        MythInput input("1\n");
        ps[0]->getHero()->findSkill("志继")->onPhaseStart(e,*ps[0],TurnPhase::PREPARATION,skip);
    }
    CHECK_EQ(ps[0]->getMaxHp(),maxHp-1);
    CHECK_EQ(ps[0]->getHp(),3);
    CHECK(ps[0]->getHero()->findSkill("观星")!=nullptr);
    CHECK_EQ(ps[0]->getHandCardCount(),0);
}

TEST("myth/tuntian_triggers_on_discard_transfer_and_equipment_loss_outside_turn") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"dengai","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto judgement=[&](int rank){
        e.getDeck().putOnTop({makeCard("杀",Suit::SPADE,rank,CardType::BASIC,CardSubType::SHA)});
    };
    auto a=makeCard("闪",Suit::HEART,5,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(a);judgement(5);e.discardCardOf(ps[0],a,"回合外失去手牌");
    CHECK_EQ(ps[0]->getPileCount("田"),1);
    auto b=makeCard("闪",Suit::HEART,6,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(b);judgement(6);e.obtainCard(ps[1],b,ps[0]);
    CHECK_EQ(ps[0]->getPileCount("田"),2);
    auto armor=makeCard("八卦阵",Suit::SPADE,7,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[0]->equip(armor);judgement(7);e.discardCardOf(ps[0],armor,"回合外失去装备");
    CHECK_EQ(ps[0]->getPileCount("田"),3);
}

TEST("myth/tianyi_winning_adds_one_target_and_ignores_distance") {
    GameEngine e;captureLog(e);e.initGame(4,0,{"taishici","zhangfei","guanyu","lvmeng"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("闪",Suit::HEART,13,CardType::BASIC,CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("闪",Suit::CLUB,1,CardType::BASIC,CardSubType::SHAN));
    {
        MythInput input("1\n1\n");ps[0]->getHero()->findSkill("天义")->activate(e,*ps[0]);
    }
    auto sha=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);
    CHECK_EQ(e.getShaTargetLimit(*ps[0],sha),2);
    CHECK(e.calculateDistance(*ps[0],*ps[2])>1);
    CHECK(e.canUseShaOn(*ps[0],*ps[2],sha));
    int hp1=ps[1]->getHp(),hp2=ps[2]->getHp();
    e.useCard(ps[0],sha,{ps[1],ps[2]});
    CHECK_EQ(ps[1]->getHp(),hp1-1);CHECK_EQ(ps[2]->getHp(),hp2-1);
}

TEST("myth/tuntian_is_not_locked_so_distance_and_judgement_stop_when_disabled") {
    GameEngine e;captureLog(e);e.initGame(4,0,{"dengai","guanyu","zhangfei","lvmeng"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto a=makeCard("闪",Suit::SPADE,5,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(a);
    {
        MythInput input("n\n");e.discardCardOf(ps[0],a,"回合外失牌");
    }
    CHECK_EQ(ps[0]->getPileCount("田"),0);
    ps[0]->addToPile("田",makeCard("杀",Suit::CLUB,3,CardType::BASIC,CardSubType::SHA));
    // 官网【屯田】未标注锁定技：非锁定技失效时，距离-X 与判定均失效。
    int enabledDistance=e.calculateDistance(*ps[0],*ps[2]);
    ps[0]->setNonLockSkillsDisabled(true);
    CHECK_EQ(e.calculateDistance(*ps[0],*ps[2]),enabledDistance+1);
    ps[0]->setNonLockSkillsDisabled(false);
    CHECK_EQ(e.calculateDistance(*ps[0],*ps[2]),enabledDistance);
    ps[0]->setNonLockSkillsDisabled(true);
    auto b=makeCard("闪",Suit::SPADE,6,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(b);e.discardCardOf(ps[0],b,"失效期间失牌");
    CHECK_EQ(ps[0]->getPileCount("田"),1);
}

TEST("engine/wuxie_on_one_tiesuo_target_does_not_cancel_other_target") {
    GameEngine e;captureLog(e);e.initGame(3,1,{"wolong","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto trick=makeCard("铁索连环",Suit::CLUB,10,CardType::TRICK,CardSubType::TIE_SUO_LIAN_HUAN);
    ps[0]->addHandCard(trick);
    ps[1]->addHandCard(makeCard("无懈可击",Suit::SPADE,11,CardType::TRICK,CardSubType::WU_XIE_KE_JI));
    {
        MythInput input("1\n");e.useCard(ps[0],trick,{ps[1],ps[2]});
    }
    CHECK(!ps[1]->isChained());CHECK(ps[2]->isChained());
}

TEST("engine/virtual_delayed_trick_keeps_real_card_until_judgement") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"xuhuang","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto cost=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(cost);
    auto conv=e.getConversionsFor(ps[0],cost,CardSubType::BING_LIANG_CUN_DUAN);
    CHECK(!conv.empty());
    e.useCard(ps[0],conv.front(),{ps[1]});
    CHECK(!e.getDeck().removeDiscardCard(cost));
    CHECK_EQ(ps[1]->getJudgeZone().size(),1u);
    e.getDeck().putOnTop({makeCard("闪",Suit::CLUB,5,CardType::BASIC,CardSubType::SHAN)});
    e.runTurn(ps[1]);
    CHECK(e.getDeck().removeDiscardCard(cost));
    // AI 在之后的出牌阶段可能再使用抽到的延时锦囊；只断言旧实体牌已经离场。
    for(auto c:ps[1]->getJudgeZone())
        for(auto real:c->getRealCards(c))CHECK(real!=cost);
}

TEST("engine/delayed_trick_is_nullified_during_judgement_not_when_placed") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,0,{"guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto trick=makeCard("乐不思蜀",Suit::HEART,6,CardType::TRICK,CardSubType::LE_BU_SI_SHU);
    ps[1]->addHandCard(trick);
    ps[0]->addHandCard(makeCard("无懈可击",Suit::SPADE,12,CardType::TRICK,CardSubType::WU_XIE_KE_JI));
    CHECK(e.useCard(ps[1],trick,{ps[0]}));
    CHECK_EQ(ps[0]->getJudgeZone().size(),1u);
    {
        MythInput input("1\n0\n");e.runTurn(ps[0]);
    }
    CHECK(ps[0]->getJudgeZone().empty());
    CHECK(log->str().find("在判定阶段被【无懈可击】抵消")!=std::string::npos);
    CHECK(log->str().find("跳过出牌阶段")==std::string::npos);
    CHECK(e.getDeck().removeDiscardCard(trick));
}

TEST("engine/judgement_claim_prevents_duplicate_field_pile_card") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"dengai","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->getHero()->addSkill(std::make_shared<TianDuSkill>());
    auto judgement=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    e.getDeck().putOnTop({judgement});
    auto lost=makeCard("闪",Suit::HEART,4,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(lost);e.discardCardOf(ps[0],lost,"回合外失牌");
    CHECK_EQ(ps[0]->getPileCount("田"),0);
    CHECK(ps[0]->hasHandCard(judgement));
    CHECK(!e.getDeck().removeDiscardCard(judgement));
}

TEST("engine/forced_sha_rejects_out_of_range_target_before_consuming_card") {
    GameEngine e;captureLog(e);e.initGame(4,-1,{"guanyu","zhangfei","lvmeng","xunyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto sha=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);
    CHECK(e.calculateDistance(*ps[0],*ps[2])>1);
    CHECK(e.askUseSha(ps[0],"强制攻击",true,ps[2])==nullptr);
    CHECK(ps[0]->hasHandCard(sha));
}

TEST("myth/jiyang_triggers_on_duel_target_even_if_wuxie_cancels") {
    GameEngine e;captureLog(e);e.initGame(2,1,{"zhangfei","sunce"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto duel=makeCard("决斗",Suit::SPADE,8,CardType::TRICK,CardSubType::JUE_DOU);
    ps[0]->addHandCard(duel);
    ps[1]->addHandCard(makeCard("无懈可击",Suit::CLUB,12,CardType::TRICK,CardSubType::WU_XIE_KE_JI));
    int hp=ps[1]->getHp();
    {
        MythInput input("y\n1\n");e.useCard(ps[0],duel,{ps[1]});
    }
    CHECK_EQ(ps[1]->getHp(),hp);
    CHECK_EQ(ps[1]->getHandCardCount(),1);
}

TEST("myth/mobile_guixin_acquires_from_each_zone_per_damage_point_and_flips_each_time") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_caocao","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto first=makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN);
    auto second=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    auto armor=makeCard("八卦阵",Suit::CLUB,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[1]->addHandCard(first);ps[1]->addHandCard(second);
    ps[2]->equip(armor);
    e.applyDamage(ps[2],ps[0],2);
    CHECK(ps[0]->hasHandCard(first));CHECK(ps[0]->hasHandCard(second));
    CHECK(ps[0]->hasHandCard(armor));CHECK(ps[2]->getArmor()==nullptr);
    CHECK(!ps[0]->isTurnedOver());
}

TEST("myth/mobile_lianpo_offers_an_extra_turn_for_each_kill") {
    GameEngine e;auto log=captureLog(e);
    e.initGame(4,-1,{"shen_simayi","guanyu","zhangfei","sunquan"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto skill=ps[0]->getHero()->findSkill("连破");
    skill->onTurnStart(e,*ps[0]);
    skill->onPlayerDeath(e,*ps[0],*ps[1],ps[0].get());
    skill->onPlayerDeath(e,*ps[0],*ps[2],ps[0].get());
    skill->onTurnEnd(e,*ps[0],*ps[0]);
    e.runTurn(ps[0]);
    auto text=log->str();auto first=text.find("获得一个额外回合！");
    CHECK(first!=std::string::npos);
    CHECK(text.find("获得一个额外回合！",first+1)!=std::string::npos);
}

TEST("myth/mobile_weiyan_kuanggu_applies_to_self_and_only_distance_one_damage") {
    GameEngine e;captureLog(e);e.initGame(4,-1,{"weiyan","guanyu","zhangfei","sunquan"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->setHp(3);
    e.applyDamage(ps[0],ps[0],1);
    CHECK_EQ(ps[0]->getHp(),3); // 自己也是距自己不大于1的角色
    e.applyDamage(ps[0],ps[2],1);
    CHECK_EQ(ps[0]->getHp(),3);
    e.applyDamage(ps[0],ps[1],1);
    CHECK_EQ(ps[0]->getHp(),4);
}

TEST("myth/mobile_guhuo_all_challengers_true_black_is_discarded") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"yuji","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    unsigned seed=0;
    for(unsigned n=1;n<1000;n++) {
        std::mt19937 random(n);
        if(random()%5==0 && random()%5==0){seed=n;break;}
    }
    CHECK(seed!=0);
    auto real=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(real);
    int hp1=ps[1]->getHp(),hp2=ps[2]->getHp();
    e.setSeed(seed);
    CHECK(!e.useCard(ps[0],e.getConversionsFor(ps[0],real,CardSubType::SHA).front(),{ps[1]}));
    CHECK_EQ(ps[1]->getHp(),hp1-1);
    CHECK_EQ(ps[2]->getHp(),hp2-1);
    CHECK(!ps[0]->hasHandCard(real));
    CHECK(e.getDeck().removeDiscardCard(real));
}

TEST("myth/mobile_guhuo_all_challengers_false_heart_each_draw_and_card_is_discarded") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"yuji","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    unsigned seed=0;
    for(unsigned n=1;n<1000;n++) {
        std::mt19937 random(n);
        if(random()%5==0 && random()%5==0){seed=n;break;}
    }
    CHECK(seed!=0);
    auto real=makeCard("闪",Suit::HEART,9,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(real);
    e.setSeed(seed);
    CHECK(!e.useCard(ps[0],e.getConversionsFor(ps[0],real,CardSubType::SHA).front(),{ps[1]}));
    CHECK_EQ(ps[1]->getHandCardCount(),1);
    CHECK_EQ(ps[2]->getHandCardCount(),1);
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp());
    CHECK_EQ(ps[2]->getHp(),ps[2]->getMaxHp());
    CHECK(!ps[0]->hasHandCard(real));
    CHECK(e.getDeck().removeDiscardCard(real));
}

TEST("myth/mobile_guhuo_response_is_challengeable_and_can_be_invalidated") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"yuji","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    unsigned seed=0;
    for(unsigned n=1;n<100;n++) {
        std::mt19937 random(n);if(random()%5==0){seed=n;break;}
    }
    CHECK(seed!=0);
    auto black=makeCard("闪",Suit::SPADE,7,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(black);
    e.setSeed(seed);
    {MythInput input("2\n");
     CHECK(e.askResponseCard(ps[0],CardSubType::SHAN,"请打出闪")==nullptr);}
    CHECK(e.getDeck().removeDiscardCard(black));
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp()-1);
    auto red=makeCard("闪",Suit::HEART,8,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(red);
    e.setSeed(seed);
    {MythInput input("2\n");
     auto response=e.askResponseCard(ps[0],CardSubType::SHAN,"请打出闪");
     CHECK(response!=nullptr);
     CHECK(response && response->getSkillSource()=="蛊惑");}
    CHECK(e.getDeck().removeDiscardCard(red));
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp()-2);
}

TEST("myth/guhuo_distinguishes_normal_fire_and_thunder_sha_claims") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"yuji","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto real=makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(real);
    auto variants=e.getConversionsFor(ps[0],real,CardSubType::SHA);
    CHECK_EQ(variants.size(),3u);
    CHECK(variants[1]->getShaElement()==ShaElement::FIRE);
    CHECK(variants[2]->getShaElement()==ShaElement::THUNDER);
    unsigned challengerSeed=0;
    for(unsigned n=1;n<100;n++){std::mt19937 generator(n);if(generator()%5==0){challengerSeed=n;break;}}
    e.setSeed(challengerSeed);
    int hp=ps[1]->getHp();
    CHECK(!e.useCard(ps[0],variants[1],{ps[1]}));
    CHECK_EQ(ps[1]->getHp(),hp);
    CHECK(e.getDeck().removeDiscardCard(real));
}

TEST("myth/guhuo_forced_use_sha_still_faces_challenge") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"yuji","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto real=makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(real);
    unsigned challengerSeed=0;
    for(unsigned n=1;n<100;n++){std::mt19937 generator(n);if(generator()%5==0){challengerSeed=n;break;}}
    e.setSeed(challengerSeed);
    {
        MythInput input("3\n");
        CHECK(e.askUseSha(ps[0],"受迫使用杀",true,ps[1])==nullptr);
    }
    CHECK(!ps[0]->hasHandCard(real));
    CHECK(e.getDeck().removeDiscardCard(real));
}

TEST("myth/guhuo_log_hides_unrevealed_subcard_before_challenge") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,-1,{"yuji","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto real=makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(real);
    auto bluff=e.getConversionsFor(ps[0],real,CardSubType::WU_ZHONG_SHENG_YOU).front();
    unsigned seed=0;
    for(unsigned n=1;n<100;n++){std::mt19937 generator(n);if(generator()%5!=0){seed=n;break;}}
    e.setSeed(seed);log->str("");
    CHECK(e.useCard(ps[0],bluff,{}));
    CHECK(log->str().find("底牌未公开")!=std::string::npos);
    CHECK(log->str().find(real->getFormattedName())==std::string::npos);
}

TEST("engine/pindian_hand_loss_uses_out_of_turn_loss_hook") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"taishici","dengai"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,13,CardType::BASIC,CardSubType::SHA));
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,1,CardType::BASIC,CardSubType::SHAN));
    e.getDeck().putOnTop({makeCard("杀",Suit::CLUB,7,CardType::BASIC,CardSubType::SHA)});
    CHECK(e.pindian(ps[0],ps[1],"测试拼点"));
    CHECK_EQ(ps[1]->getPileCount("田"),1);
}

TEST("myth/fangquan_pays_at_turn_end_not_finish_phase_and_queues_extra_turn") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,0,{"liuchan","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("闪",Suit::HEART,7,CardType::BASIC,CardSubType::SHAN));
    auto skill=ps[0]->getHero()->findSkill("放权");bool skip=false;
    {
        MythInput input("y\n1\n1\n");
        skill->onPhaseStart(e,*ps[0],TurnPhase::PLAY,skip);
        CHECK(skip);
        skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
        CHECK_EQ(ps[0]->getHandCardCount(),1); // 回合结束前仍可使用手牌
        skill->onTurnEnd(e,*ps[0],*ps[0]);
    }
    CHECK_EQ(ps[0]->getHandCardCount(),0);
    e.runTurn(ps[0]);
    CHECK(log->str().find("获得一个额外回合！")!=std::string::npos);
}

TEST("myth/mobile_wuqian_armor_suppression_survives_turn_end") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_lvbu","guanyu","zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",2);
    ps[1]->equip(makeCard("仁王盾",Suit::CLUB,7,CardType::EQUIPMENT,CardSubType::ARMOR));
    auto skill=ps[0]->getHero()->findSkill("无前");
    {MythInput input("2\n");skill->activate(e,*ps[0]);}
    auto sha1=makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA);
    ps[2]->addHandCard(sha1);int hp=ps[1]->getHp();
    CHECK_EQ(ps[1]->getMark("无前防具失效"),1);
    e.useCard(ps[2],sha1,{ps[1]});CHECK_EQ(ps[1]->getHp(),hp-1);
    skill->onTurnBoundary(e,*ps[0],*ps[0],false);
    CHECK_EQ(ps[1]->getMark("无前防具失效"),1);
    auto sha2=makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA);
    ps[2]->addHandCard(sha2);e.useCard(ps[2],sha2,{ps[1]});
    CHECK_EQ(ps[1]->getHp(),hp-2);
}

TEST("myth/wumou_and_wuhun_are_locked_even_when_nonlocked_skills_disabled") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","shen_guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->setNonLockSkillsDisabled(true);
    ps[1]->setNonLockSkillsDisabled(true);
    auto trick=makeCard("无中生有",Suit::HEART,5,CardType::TRICK,CardSubType::WU_ZHONG_SHENG_YOU);
    ps[0]->addHandCard(trick);int rage=ps[0]->getMark("暴怒");
    CHECK(!e.canUseOriginalCard(*ps[0],trick));
    CHECK(!e.useCard(ps[0],trick,{}));
    auto sha=e.getConversionsFor(ps[0],trick,CardSubType::SHA);
    CHECK_EQ(sha.size(),size_t(1));
    CHECK_EQ(sha.front()->getSkillSource(),std::string("无谋"));
    CHECK_EQ(ps[0]->getMark("暴怒"),rage);
    e.applyDamage(ps[2],ps[1],1);
    CHECK_EQ(ps[2]->getMark("魇"),1);
}

TEST("myth/wuqian_cleans_up_armor_suppression_if_owner_dies") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_lvbu","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);ps[0]->addMark("暴怒",2);
    {MythInput input("2\n");ps[0]->getHero()->findSkill("无前")->activate(e,*ps[0]);}
    CHECK_EQ(ps[1]->getMark("无前防具失效"),1);
    e.killPlayer(ps[0]);
    CHECK_EQ(ps[1]->getMark("无前防具失效"),0);
}

TEST("myth/mobile_guhuo_repeat_challenge_never_grants_chanyuan") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,1,{"yuji","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    for(int i=0;i<2;i++) {
        auto card=makeCard("桃",Suit::HEART,i+3,CardType::BASIC,CardSubType::TAO);
        ps[0]->addHandCard(card);
        {MythInput input("y\n");
         CHECK(e.useCard(ps[0],e.getConversionsFor(ps[0],card,CardSubType::TAO).front(),{}));}
    }
    CHECK_EQ(ps[1]->getHp(),ps[1]->getMaxHp()-2);
    CHECK(ps[1]->getHero()->findSkill("缠怨")==nullptr);
    CHECK_EQ(ps[1]->getMark("缠怨"),0);
    CHECK(log->str().find("揭示底牌",log->str().find("揭示底牌")+1)!=std::string::npos);
}

TEST("engine/removing_skill_or_its_owner_cleans_up_persistent_weather_marks") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_zhugeliang","guanyu","zhangfei"});
    auto ps=e.getPlayers();auto skill=ps[0]->getHero()->findSkill("大雾");
    CHECK(ps[0]->getPileCount("星")>0);
    {MythInput input("y\n1\n2\n");skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);}
    CHECK_EQ(ps[1]->getMark("雾"),1);
    e.removeHeroSkills(ps[0]);
    CHECK_EQ(ps[1]->getMark("雾"),0);
}

TEST("myth/weather_expires_if_shen_zhugeliang_dies_early") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_zhugeliang","guanyu","zhangfei"});
    auto ps=e.getPlayers();auto skill=ps[0]->getHero()->findSkill("大雾");
    {MythInput input("y\n1\n2\n");skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);}
    CHECK_EQ(ps[1]->getMark("雾"),1);
    e.killPlayer(ps[0]);
    CHECK_EQ(ps[1]->getMark("雾"),0);
}

TEST("myth/wansha_and_xueyi_remain_locked_but_unlocked_buqu_is_disabled") {
    GameEngine e;captureLog(e);e.initGame(4,0,{"yuanshao","jiaxu","zhoutai","yuji"});
    auto ps=e.getPlayers();for(auto p:ps){clearHand(*p);p->setNonLockSkillsDisabled(true);}
    auto has=[&](PlayerPtr p,const std::string& name){
        for(auto skill:e.getEffectiveSkills(*p))if(skill->getName()==name)return true;
        return false;
    };
    CHECK(has(ps[0],"血裔"));
    CHECK_EQ(e.calculateHandLimit(ps[0]),ps[0]->getMaxHp()+4);
    CHECK(has(ps[1],"完杀"));
    // 官网【不屈】未标注锁定技（界马超【铁骑】等可令其失效）。
    CHECK(!has(ps[2],"不屈"));
    CHECK(!has(ps[3],"蛊惑"));
}

TEST("myth/kuanggu_checks_distance_before_the_victim_dies") {
    GameEngine e;captureLog(e);e.initGame(4,0,{"weiyan","guanyu","zhangfei","zhouyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->setHp(ps[0]->getMaxHp()-1);ps[1]->setHp(1);
    int hp=ps[0]->getHp();
    {MythInput input("y\n1\n");e.applyDamage(ps[0],ps[1],1);}
    CHECK(!ps[1]->isAlive());
    CHECK_EQ(ps[0]->getHp(),hp+1);
}

TEST("myth/beige_cannot_trigger_on_a_dead_damage_victim") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"caiwenji","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto cost=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(cost);ps[1]->setHp(1);
    {MythInput input("y\n");e.applyDamage(ps[2],ps[1],1,ShaElement::NORMAL,false,cost);}
    CHECK(!ps[1]->isAlive());
    CHECK_EQ(ps[0]->getHandCardCount(),1);
}

TEST("engine/duanchang_cleans_up_wuqian_when_the_skill_is_removed") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_lvbu","caiwenji","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);ps[0]->addMark("暴怒",2);
    {MythInput input("3\n");ps[0]->getHero()->findSkill("无前")->activate(e,*ps[0]);}
    CHECK_EQ(ps[2]->getMark("无前防具失效"),1);
    e.killPlayer(ps[1],ps[0]);
    CHECK_EQ(ps[2]->getMark("无前防具失效"),0);
    CHECK(ps[0]->getHero()->findSkill("无前")==nullptr);
}

TEST("myth/mobile_mengjin_discards_target_equipment_when_sha_is_dodged") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"pangde","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto armor=makeCard("八卦阵",Suit::SPADE,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    auto shan=makeCard("闪",Suit::HEART,6,CardType::BASIC,CardSubType::SHAN);
    auto sha=makeCard("杀",Suit::HEART,8,CardType::BASIC,CardSubType::SHA);
    ps[1]->equip(armor);ps[1]->addHandCard(shan);ps[0]->addHandCard(sha);
    e.getDeck().putOnTop({makeCard("杀",Suit::SPADE,4,CardType::BASIC,CardSubType::SHA)});
    int hp=ps[1]->getHp();
    {MythInput input("y\n1\n");CHECK(e.useCard(ps[0],sha,{ps[1]}));}
    CHECK_EQ(ps[1]->getHp(),hp);
    CHECK(ps[1]->getArmor()==nullptr);
    CHECK(!ps[1]->hasHandCard(shan));
}

TEST("myth/mobile_mengjin_does_not_trigger_when_sha_hits") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"pangde","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto tao=makeCard("桃",Suit::CLUB,2,CardType::BASIC,CardSubType::TAO);
    auto sha=makeCard("杀",Suit::HEART,8,CardType::BASIC,CardSubType::SHA);
    ps[1]->addHandCard(tao);ps[0]->addHandCard(sha);
    int hp=ps[1]->getHp();CHECK(e.useCard(ps[0],sha,{ps[1]}));
    CHECK_EQ(ps[1]->getHp(),hp-1);
    CHECK(ps[1]->hasHandCard(tao));
    CHECK(!ps[1]->hasHandCard(sha));
    CHECK(e.getDeck().removeDiscardCard(sha));
}

TEST("myth/luanji_lets_player_choose_which_matching_cards_to_spend") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"yuanshao","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto a=makeCard("杀",Suit::CLUB,3,CardType::BASIC,CardSubType::SHA);
    auto b=makeCard("杀",Suit::CLUB,4,CardType::BASIC,CardSubType::SHA);
    auto c=makeCard("桃",Suit::HEART,5,CardType::BASIC,CardSubType::TAO);
    auto d=makeCard("桃",Suit::HEART,6,CardType::BASIC,CardSubType::TAO);
    ps[0]->addHandCard(a);ps[0]->addHandCard(b);ps[0]->addHandCard(c);ps[0]->addHandCard(d);
    {MythInput input("3\n1\n");ps[0]->getHero()->findSkill("乱击")->activate(e,*ps[0]);}
    CHECK(ps[0]->hasHandCard(a));CHECK(ps[0]->hasHandCard(b));
    CHECK(!ps[0]->hasHandCard(c));CHECK(!ps[0]->hasHandCard(d));
}

TEST("engine/once_per_turn_delegations_reset_even_for_standard_heroes") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"sunce","sunquan","yuji"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    // 孙权是标准武将，不能依赖其拥有神话技能才能重置制霸次数。
    ps[1]->addMark("制霸已挑战",1);
    ps[1]->addMark("黄天已献",1);
    ps[2]->setNonLockSkillsDisabled(true);
    e.runTurn(ps[1]);
    CHECK_EQ(ps[1]->getMark("制霸已挑战"),0);
    CHECK_EQ(ps[1]->getMark("黄天已献"),0);
}

TEST("myth/qiangxi_can_pay_with_weapon_in_hand") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"dianwei","guanyu"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto weapon=makeCard("青龙偃月刀",Suit::SPADE,5,CardType::EQUIPMENT,CardSubType::WEAPON);
    ps[0]->addHandCard(weapon);
    int hp=ps[0]->getHp(),enemyHp=ps[1]->getHp();
    {MythInput input("1\ny\n1\n");ps[0]->getHero()->findSkill("强袭")->activate(e,*ps[0]);}
    CHECK_EQ(ps[0]->getHp(),hp);
    CHECK_EQ(ps[1]->getHp(),enemyHp-1);
    CHECK(!ps[0]->hasHandCard(weapon));
}

TEST("myth/mobile_gongxin_discards_revealed_heart_instead_of_obtaining") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_lvmeng","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto heart=makeCard("桃",Suit::HEART,5,CardType::BASIC,CardSubType::TAO);
    ps[2]->addHandCard(heart);
    {MythInput input("1\n1\n1\n");ps[0]->getHero()->findSkill("攻心")->activate(e,*ps[0]);}
    CHECK(!ps[0]->hasHandCard(heart));
    CHECK(!ps[2]->hasHandCard(heart));
    CHECK(e.getDeck().removeDiscardCard(heart));
}

TEST("myth/tianyi_only_offers_players_who_can_pindian") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"taishici","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,13,CardType::BASIC,CardSubType::SHA));
    ps[2]->addHandCard(makeCard("闪",Suit::HEART,1,CardType::BASIC,CardSubType::SHAN));
    {MythInput input("1\n1\n");ps[0]->getHero()->findSkill("天义")->activate(e,*ps[0]);}
    CHECK_EQ(e.getShaLimit(*ps[0]),2);
}

TEST("engine/mobile_wuqian_not_automatically_removed_at_turn_end") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",2);
    auto skill=ps[0]->getHero()->findSkill("无前");
    skill->activate(e,*ps[0]);
    int target=-1;
    for(auto p:e.getOtherAlivePlayers(*ps[0]))if(p->getMark("无前防具失效")>0)target=p->getId();
    CHECK(target>=0);
    ps[0]->setNonLockSkillsDisabled(true);
    e.runTurn(ps[0]);
    CHECK_EQ(e.getPlayerById(target)->getMark("无前防具失效"),1);
    CHECK(ps[0]->getHero()->findSkill("无双")!=nullptr);
    skill->onRemoved(e,*ps[0]);
    CHECK_EQ(e.getPlayerById(target)->getMark("无前防具失效"),0);
}

TEST("engine/weather_expires_at_turn_start_even_if_source_skill_is_suppressed") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_zhugeliang","guanyu","zhangfei"});
    auto ps=e.getPlayers();auto skill=ps[0]->getHero()->findSkill("大雾");
    {MythInput input("y\n1\n2\n");skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);}
    CHECK_EQ(ps[1]->getMark("雾"),1);
    ps[0]->setNonLockSkillsDisabled(true);
    {MythInput input("0\n");e.runTurn(ps[0]);}
    CHECK_EQ(ps[1]->getMark("雾"),0);
}

TEST("myth/huashen_never_draws_unofficial_diy_heroes") {
    for(unsigned seed=0;seed<40;seed++) {
        GameEngine e;e.setSeed(seed);captureLog(e);e.initGame(2,-1,{"zuoci","guanyu"});
        auto avatar=std::dynamic_pointer_cast<MythSkill>(e.getPlayers()[0]->getHero()->findSkill("化身"));
        CHECK(avatar!=nullptr);
        for(const auto& id:avatar->getAvatarIds()) {
            auto info=HeroRegistry::find(id);
            CHECK(info!=nullptr);
            CHECK(info->pack!="DIY包");
        }
    }
}

TEST("myth/baonue_judgement_belongs_to_the_damage_source_not_dongzhuo") {
    GameEngine e;auto log=captureLog(e);e.initGame(3,1,{"dongzhuo","zhangjiao","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->setHp(ps[0]->getMaxHp()-1);
    auto spade=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    e.getDeck().putOnTop({spade});
    int hp=ps[0]->getHp();
    {MythInput input("y\n");e.applyDamage(ps[1],ps[2],1);}
    CHECK_EQ(ps[0]->getHp(),hp+1);
    CHECK(log->str().find("[玩家(你)] 为【暴虐】进行判定")!=std::string::npos);
}

TEST("myth/baonue_may_be_judged_even_if_lord_is_not_wounded") {
    GameEngine e;auto log=captureLog(e);e.initGame(3,1,{"dongzhuo","zhangjiao","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    e.getDeck().putOnTop({makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA)});
    {MythInput input("y\n");e.applyDamage(ps[1],ps[2],1);}
    CHECK(log->str().find("[玩家(你)] 为【暴虐】进行判定")!=std::string::npos);
}

TEST("myth/yeyan_can_select_its_owner_as_a_target") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhouyu","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    int hp=ps[0]->getHp(),other=ps[1]->getHp();
    {MythInput input("1\n0\n");ps[0]->getHero()->findSkill("业炎")->activate(e,*ps[0]);}
    CHECK_EQ(ps[0]->getHp(),hp-1);
    CHECK_EQ(ps[1]->getHp(),other);
}

TEST("myth/jieming_can_replenish_its_own_hand") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"xunyu","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    {MythInput input("y\n1\n");e.applyDamage(ps[1],ps[0],1);}
    CHECK_EQ(ps[0]->getHandCardCount(),std::min(5,ps[0]->getMaxHp()));
}

TEST("engine/illegal_black_trick_target_is_not_used_or_charged_wumou") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_lvbu","jiaxu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto black=makeCard("过河拆桥",Suit::SPADE,6,CardType::TRICK,CardSubType::GUO_HE_CHAI_QIAO);
    ps[0]->addHandCard(black);
    int hp=ps[0]->getHp();
    CHECK(!e.useCard(ps[0],black,{ps[1]}));
    CHECK(ps[0]->hasHandCard(black));
    CHECK_EQ(ps[0]->getHp(),hp);
}

TEST("myth/kuangfeng_can_choose_its_owner") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_zhugeliang","guanyu"});
    auto ps=e.getPlayers();auto skill=ps[0]->getHero()->findSkill("狂风");
    {MythInput input("y\n1\n");skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);}
    CHECK_EQ(ps[0]->getMark("风"),1);
    int hp=ps[0]->getHp();
    e.applyDamage(ps[1],ps[0],1,ShaElement::FIRE);
    CHECK_EQ(ps[0]->getHp(),hp-2);
}

TEST("myth/qixing_is_not_locked_stars_stay_but_exchange_is_lost_when_suppressed") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_zhugeliang","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);
    CHECK_EQ(ps[0]->getPileCount("星"),7);
    ps[0]->setNonLockSkillsDisabled(true);
    auto active=e.getEffectiveSkills(*ps[0]);
    // 官网【七星】未标注锁定技：失效期间不在有效技能中，已有的“星”保留。
    CHECK(std::none_of(active.begin(),active.end(),[](const SkillPtr& sk){return sk->getName()=="七星";}));
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,3,CardType::BASIC,CardSubType::SHA));
    ps[0]->getHero()->findSkill("七星")->onPhaseEnd(e,*ps[0],TurnPhase::DRAW);
    CHECK_EQ(ps[0]->getPileCount("星"),7);
}

TEST("myth/leiji_can_trigger_from_bagua_virtual_shan") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"guanyu","zhangjiao"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[1]->equip(makeCard("八卦阵",Suit::HEART,5,CardType::EQUIPMENT,CardSubType::ARMOR));
    auto sha=makeCard("杀",Suit::SPADE,3,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);
    e.getDeck().putOnTop({makeCard("桃",Suit::HEART,2,CardType::BASIC,CardSubType::TAO),
                          makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA)});
    int attackerHp=ps[0]->getHp(),defenderHp=ps[1]->getHp();
    CHECK(e.useCard(ps[0],sha,{ps[1]}));
    CHECK_EQ(ps[0]->getHp(),attackerHp-2);
    CHECK_EQ(ps[1]->getHp(),defenderHp);
}

TEST("engine/bagua_supplies_only_one_shan_against_wushuang") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"lvbu","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[1]->equip(makeCard("八卦阵",Suit::HEART,5,CardType::EQUIPMENT,CardSubType::ARMOR));
    auto shan=makeCard("闪",Suit::HEART,8,CardType::BASIC,CardSubType::SHAN);
    auto sha=makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA);
    ps[1]->addHandCard(shan);ps[0]->addHandCard(sha);
    e.getDeck().putOnTop({makeCard("桃",Suit::HEART,2,CardType::BASIC,CardSubType::TAO)});
    int hp=ps[1]->getHp();
    CHECK(e.useCard(ps[0],sha,{ps[1]}));
    CHECK_EQ(ps[1]->getHp(),hp);
    CHECK(!ps[1]->hasHandCard(shan));
}

TEST("engine/mobile_wumou_disallows_tiesuo_recast_and_keeps_original_card") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_lvbu","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto tiesuo=makeCard("铁索连环",Suit::CLUB,8,CardType::TRICK,CardSubType::TIE_SUO_LIAN_HUAN);
    ps[0]->addHandCard(tiesuo);
    int hp=ps[0]->getHp(),rage=ps[0]->getMark("暴怒");
    CHECK(!e.useCard(ps[0],tiesuo,{}));
    CHECK_EQ(e.getConversionsFor(ps[0],tiesuo,CardSubType::SHA).size(),size_t(1));
    CHECK_EQ(ps[0]->getHp(),hp);
    CHECK_EQ(ps[0]->getMark("暴怒"),rage);
    CHECK_EQ(ps[0]->getHandCardCount(),1);
}

TEST("myth/wuqian_can_choose_self_as_official_text_allows_a_character") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_lvbu","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->equip(makeCard("白银狮子",Suit::CLUB,1,CardType::EQUIPMENT,CardSubType::ARMOR));
    ps[0]->addMark("暴怒",2);
    auto skill=ps[0]->getHero()->findSkill("无前");
    {MythInput input("1\n");skill->activate(e,*ps[0]);}
    CHECK_EQ(ps[0]->getMark("无前防具失效"),1);
    int hp=ps[0]->getHp();e.applyDamage(ps[1],ps[0],2);
    CHECK_EQ(ps[0]->getHp(),hp-2);
    skill->onTurnBoundary(e,*ps[0],*ps[0],false);
    CHECK_EQ(ps[0]->getMark("无前防具失效"),1);
    skill->onRemoved(e,*ps[0]);
    CHECK_EQ(ps[0]->getMark("无前防具失效"),0);
}

TEST("myth/benghuai_triggers_when_someone_has_strictly_less_hp") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"dongzhuo","guanyu","zhangfei"});
    auto ps=e.getPlayers();int before=ps[0]->getHp();
    ps[0]->getHero()->findSkill("崩坏")->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
    CHECK_EQ(ps[0]->getHp(),before-1);
    ps[0]->setHp(2);ps[1]->setHp(2);ps[2]->setHp(3);
    ps[0]->getHero()->findSkill("崩坏")->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
    CHECK_EQ(ps[0]->getHp(),2);
}

TEST("myth/shuangxiong_obtains_its_judgement_card_instead_of_discarding_it") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"yanliang_wenchou","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto red=makeCard("桃",Suit::HEART,7,CardType::BASIC,CardSubType::TAO);
    auto black=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    e.getDeck().putOnTop({red});ps[0]->addHandCard(black);
    int n=2;ps[0]->getHero()->findSkill("双雄")->onDrawCards(e,*ps[0],n);
    CHECK_EQ(n,0);
    CHECK(ps[0]->hasHandCard(red));
    CHECK(!e.getDeck().removeDiscardCard(red));
    CHECK(ps[0]->getHero()->findSkill("双雄")->convertCard(e,*ps[0],black,CardSubType::JUE_DOU)!=nullptr);
}

TEST("myth/mobile_duanliang_allows_distance_two_but_not_three") {
    GameEngine e;captureLog(e);e.initGame(6,-1,{"xuhuang","guanyu","zhangfei","lusu","liubei","zhaoyun"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto black=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(black);
    auto converted=e.getConversionsFor(ps[0],black,CardSubType::BING_LIANG_CUN_DUAN);
    CHECK_EQ(e.calculateDistance(*ps[0],*ps[3]),3);
    CHECK(!e.useCard(ps[0],converted.front(),{ps[3]}));
    CHECK(ps[0]->hasHandCard(black));
    CHECK_EQ(e.calculateDistance(*ps[0],*ps[2]),2);
    CHECK(e.useCard(ps[0],converted.front(),{ps[2]}));
    CHECK_EQ(ps[2]->getJudgeZone().size(),1u);
}

TEST("myth/mobile_xuhuang_has_no_jiezi_on_skipped_draw") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"xuhuang","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    CHECK(ps[0]->getHero()->findSkill("截辎")==nullptr);
    auto trick=makeCard("兵粮寸断",Suit::SPADE,4,CardType::TRICK,CardSubType::BING_LIANG_CUN_DUAN);
    ps[0]->addHandCard(trick);CHECK(e.useCard(ps[0],trick,{ps[1]}));
    e.getDeck().putOnTop({makeCard("杀",Suit::SPADE,3,CardType::BASIC,CardSubType::SHA)});
    e.runTurn(ps[1]);
    CHECK_EQ(ps[0]->getHandCardCount(),0);
}

TEST("myth/duanliang_can_consume_black_equipment_in_equip_zone") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"xuhuang","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto armor=makeCard("仁王盾",Suit::CLUB,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[0]->equip(armor);
    auto conversions=e.getConversionsFor(ps[0],armor,CardSubType::BING_LIANG_CUN_DUAN);
    CHECK(!conversions.empty());
    CHECK(e.useCard(ps[0],conversions.front(),{ps[1]}));
    CHECK(ps[0]->getArmor()==nullptr);
    CHECK_EQ(ps[1]->getJudgeZone().size(),1u);
}

TEST("myth/juxiang_obtains_real_cards_of_virtual_nanman") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"yuanshao","zhurong","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto first=makeCard("杀",Suit::CLUB,2,CardType::BASIC,CardSubType::SHA);
    auto second=makeCard("杀",Suit::CLUB,4,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(first);ps[0]->addHandCard(second);
    auto card=Card::makeVirtual("南蛮入侵",CardType::TRICK,CardSubType::NAN_MAN_RU_QIN,{first,second},"乱击");
    CHECK(e.useCard(ps[0],card,{}));
    CHECK(ps[1]->hasHandCard(first));CHECK(ps[1]->hasHandCard(second));
    CHECK(!e.getDeck().removeDiscardCard(first));
    CHECK(!e.getDeck().removeDiscardCard(second));
}

TEST("myth/mobile_zhijian_equips_other_but_owner_draws") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"zhangzhao_zhanghong","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto armor=makeCard("八卦阵",Suit::SPADE,8,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[0]->addHandCard(armor);
    auto skill=ps[0]->getHero()->findSkill("直谏");
    CHECK(skill->canActivate(e,*ps[0]));
    skill->activate(e,*ps[0]);
    CHECK(ps[1]->getArmor()==armor);
    CHECK_EQ(ps[1]->getHandCardCount(),0);
    CHECK_EQ(ps[0]->getHandCardCount(),1);
}

TEST("myth/mobile_wumou_converts_tricks_without_spending_rage_and_keeps_original_targets") {
    GameEngine e;captureLog(e);e.initGame(4,-1,{"shen_lvbu","guanyu","zhangfei","lvmeng"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto aoe=makeCard("南蛮入侵",Suit::SPADE,7,CardType::TRICK,CardSubType::NAN_MAN_RU_QIN);
    ps[0]->addHandCard(aoe);
    CHECK(!e.canUseOriginalCard(*ps[0],aoe));
    int rage=ps[0]->getMark("暴怒");
    auto variants=e.getConversionsFor(ps[0],aoe,CardSubType::SHA);
    CHECK(!variants.empty());
    if(variants.empty())return;
    auto sha=variants.front();
    CHECK_EQ(e.getShaTargetLimit(*ps[0],sha),4);
    int distant=e.calculateDistance(*ps[0],*ps[2]);CHECK(distant>1);
    CHECK(e.canUseShaOn(*ps[0],*ps[2],sha));
    int hp1=ps[1]->getHp(),hp2=ps[2]->getHp(),hp3=ps[3]->getHp();
    CHECK(e.useCard(ps[0],sha,{ps[1]})); // 即使调用方只指定一人，按原南蛮的目标集结算。
    CHECK_EQ(ps[1]->getHp(),hp1-1);
    CHECK_EQ(ps[2]->getHp(),hp2-1);
    CHECK_EQ(ps[3]->getHp(),hp3-1);
    CHECK_EQ(ps[0]->getMark("暴怒"),rage+3);
}

TEST("myth/mobile_wumou_wuzhong_sha_targets_original_self_not_an_enemy") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_lvbu","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto draw=makeCard("无中生有",Suit::HEART,7,CardType::TRICK,CardSubType::WU_ZHONG_SHENG_YOU);
    ps[0]->addHandCard(draw);
    auto sha=e.getConversionsFor(ps[0],draw,CardSubType::SHA).front();
    CHECK(e.canUseShaOn(*ps[0],*ps[0],sha));
    CHECK(!e.canUseShaOn(*ps[0],*ps[1],sha));
    int ownerHp=ps[0]->getHp(),otherHp=ps[1]->getHp();
    CHECK(e.useCard(ps[0],sha,{ps[1]}));
    CHECK_EQ(ps[0]->getHp(),ownerHp-1);
    CHECK_EQ(ps[1]->getHp(),otherHp);
}

TEST("myth/mobile_wuqian_adds_sha_uses_and_finish_draws_damage_card_only_when_needed") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_lvbu","guanyu","zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",4);
    auto skill=ps[0]->getHero()->findSkill("无前");
    {MythInput input("2\n3\n");skill->activate(e,*ps[0]);skill->activate(e,*ps[0]);}
    CHECK_EQ(e.getShaLimit(*ps[0]),3);
    CHECK_EQ(ps[0]->getMark("暴怒"),2); // 初始自带两枚暴怒。
    skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
    CHECK_EQ(ps[0]->getHandCardCount(),1);
    auto card=ps[0]->getHandCards().front();
    CHECK(card->getSubType()==CardSubType::SHA || card->getSubType()==CardSubType::JUE_DOU ||
          card->getSubType()==CardSubType::HUO_GONG || card->getSubType()==CardSubType::NAN_MAN_RU_QIN ||
          card->getSubType()==CardSubType::WAN_JIAN_QI_FA || card->getSubType()==CardSubType::SHAN_DIAN);
    skill->onPhaseEnd(e,*ps[0],TurnPhase::FINISH);
    CHECK_EQ(ps[0]->getHandCardCount(),1);
}

TEST("myth/mobile_buqu_healing_removes_excess_wounds_without_faking_hp") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"zhoutai","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    e.getDeck().putOnTop({makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA),
                          makeCard("杀",Suit::CLUB,8,CardType::BASIC,CardSubType::SHA)});
    ps[0]->setHp(1);
    e.applyDamage(ps[1],ps[0],2);
    CHECK(ps[0]->isAlive());CHECK_EQ(ps[0]->getHp(),-1);
    CHECK_EQ(ps[0]->getPileCount("创"),2);
    e.recoverHp(ps[0],1,"测试");
    CHECK_EQ(ps[0]->getHp(),0);CHECK_EQ(ps[0]->getPileCount("创"),1);
    e.recoverHp(ps[0],1,"测试");
    CHECK_EQ(ps[0]->getHp(),1);CHECK_EQ(ps[0]->getPileCount("创"),0);
}

TEST("myth/mobile_tianxiang_preserves_damage_card_and_does_not_repeat_source_modifiers") {
    GameEngine e;captureLog(e);e.initGame(3,1,{"xuchu","xiaoqiao","caocao"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    int drawCount=2;
    ps[0]->getHero()->findSkill("裸衣")->onDrawCards(e,*ps[0],drawCount);
    ps[1]->addHandCard(makeCard("桃",Suit::HEART,3,CardType::BASIC,CardSubType::TAO));
    auto sha=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    int before=ps[2]->getHp(),xiaoHp=ps[1]->getHp();
    bool claimed=false;
    {MythInput input("2\n1\n");e.applyDamage(ps[0],ps[1],1,ShaElement::NORMAL,false,sha,&claimed);}
    CHECK_EQ(ps[1]->getHp(),xiaoHp);
    CHECK_EQ(ps[2]->getHp(),before-2); // 裸衣只对同一次伤害加一回。
    CHECK(claimed);
    CHECK(ps[2]->hasHandCard(sha));
}

TEST("myth/mobile_guzheng_may_return_one_discard_without_taking_the_others") {
    GameEngine e;captureLog(e);e.initGame(2,1,{"guanyu","zhangzhao_zhanghong"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto one=makeCard("杀",Suit::SPADE,3,CardType::BASIC,CardSubType::SHA);
    auto two=makeCard("闪",Suit::HEART,4,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(one);ps[0]->addHandCard(two);
    e.discardCardOf(ps[0],one,"弃牌阶段");e.discardCardOf(ps[0],two,"弃牌阶段");
    {MythInput input("y\n1\nn\n");
     ps[1]->getHero()->findSkill("固政")->onOtherDiscardPhaseEnd(e,*ps[1],*ps[0],{one,two});}
    CHECK(ps[0]->hasHandCard(one));
    CHECK(!ps[1]->hasHandCard(two));
    CHECK(e.getDeck().removeDiscardCard(two));
}

TEST("myth/mobile_wuqian_counts_distinct_affected_characters_for_sha_limit") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_lvbu","guanyu","zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();ps[0]->addMark("暴怒",4);
    auto skill=ps[0]->getHero()->findSkill("无前");
    {MythInput input("2\n2\n");skill->activate(e,*ps[0]);skill->activate(e,*ps[0]);}
    CHECK_EQ(e.getShaLimit(*ps[0]),2); // 同一角色被选择两次仍仅有一名受影响角色。
    skill->onRemoved(e,*ps[0]);
    CHECK_EQ(ps[1]->getMark("无前防具失效"),0);
}

TEST("myth/mobile_guixin_chooses_a_public_zone_without_revealing_hidden_hands") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_caocao","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto hidden=makeCard("闪",Suit::DIAMOND,9,CardType::BASIC,CardSubType::SHAN);
    auto armor=makeCard("仁王盾",Suit::CLUB,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    auto other=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    ps[1]->addHandCard(hidden);ps[1]->equip(armor);
    ps[2]->addHandCard(other);
    {MythInput input("y\n2\n1\n1\n");e.applyDamage(ps[2],ps[0],1);}
    CHECK(ps[0]->hasHandCard(armor));
    CHECK(ps[0]->hasHandCard(other));
    CHECK(!ps[0]->hasHandCard(hidden));
    CHECK(ps[0]->isTurnedOver());
}

TEST("myth/mobile_hongyan_does_not_recolor_equipment_or_delayed_cards") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"xiaoqiao","guanyu"});
    auto ps=e.getPlayers();
    auto hand=makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA);
    auto armor=makeCard("八卦阵",Suit::SPADE,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    auto delayed=makeCard("闪电",Suit::SPADE,1,CardType::TRICK,CardSubType::SHAN_DIAN);
    ps[0]->addHandCard(hand);ps[0]->equip(armor);ps[0]->addJudgeCard(delayed);
    CHECK(e.effectiveSuit(*ps[0],hand)==Suit::HEART);
    CHECK(e.effectiveSuit(*ps[0],armor)==Suit::SPADE);
    CHECK(e.effectiveSuit(*ps[0],delayed)==Suit::SPADE);
}

TEST("myth/mobile_hunzi_requires_exactly_one_hp_not_nonpositive_hp") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"sunce","guanyu"});
    auto p=e.getPlayers()[0];auto skill=p->getHero()->findSkill("魂姿");
    bool skip=false;p->setHp(0);int max=p->getMaxHp();
    skill->onPhaseStart(e,*p,TurnPhase::PREPARATION,skip);
    CHECK_EQ(p->getMaxHp(),max);
    CHECK(p->getHero()->findSkill("英姿")==nullptr);
    p->setHp(1);
    skill->onPhaseStart(e,*p,TurnPhase::PREPARATION,skip);
    CHECK_EQ(p->getMaxHp(),max-1);
    CHECK(p->getHero()->findSkill("英姿")!=nullptr);
}

TEST("myth/mobile_wumou_jiedao_sha_uses_both_original_trick_targets") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[1]->equip(makeCard("青釭剑",Suit::SPADE,6,CardType::EQUIPMENT,CardSubType::WEAPON));
    auto material=makeCard("借刀杀人",Suit::CLUB,12,CardType::TRICK,CardSubType::JIE_DAO_SHA_REN);
    ps[0]->addHandCard(material);
    auto sha=e.getConversionsFor(ps[0],material,CardSubType::SHA).front();
    int a=ps[1]->getHp(),b=ps[2]->getHp();
    CHECK(!e.useCard(ps[0],sha,{ps[1]})); // 少了第二目标，牌不能消耗。
    CHECK(ps[0]->hasHandCard(material));
    CHECK(e.useCard(ps[0],sha,{ps[1],ps[2]}));
    CHECK_EQ(ps[1]->getHp(),a-1);
    CHECK_EQ(ps[2]->getHp(),b-1);
}

TEST("myth/mobile_huashen_can_choose_a_lord_skill_and_replaces_borrowed_skill") {
    bool found=false;
    for(unsigned seed=1;seed<800 && !found;seed++) {
        GameEngine e;e.setSeed(seed);captureLog(e);
        {MythInput input("1\n1\n");e.initGame(2,0,{"zuoci","guanyu"});}
        auto p=e.getPlayers()[0];
        auto skill=std::dynamic_pointer_cast<MythSkill>(p->getHero()->findSkill("化身"));
        auto ids=skill->getAvatarIds();
        auto it=std::find(ids.begin(),ids.end(),"liubei");
        if(it==ids.end())continue;
        found=true;
        std::string script="y\n"+std::to_string(1+it-ids.begin())+"\n2\n";
        {MythInput input(script);skill->onTurnEnd(e,*p,*p);}
        CHECK_EQ(skill->getBorrowedSkillName(),std::string("激将"));
        CHECK(p->getHero()->findSkill("激将")!=nullptr);
        CHECK(p->getHero()->getCountry()==Country::SHU);
    }
    CHECK(found);
}

TEST("std/mobile_yingzi_is_optional_for_zhouyu_and_awakened_sunce") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"zhouyu","sunce"});
    auto ps=e.getPlayers();int draw=2;
    {MythInput input("n\n");ps[0]->getHero()->findSkill("英姿")->onDrawCards(e,*ps[0],draw);}
    CHECK_EQ(draw,2);
    {MythInput input("y\n");ps[0]->getHero()->findSkill("英姿")->onDrawCards(e,*ps[0],draw);}
    CHECK_EQ(draw,3);
    GameEngine awakened;captureLog(awakened);
    awakened.initGame(2,1,{"guanyu","sunce"});
    auto sunce=awakened.getPlayers()[1];
    sunce->setHp(1);bool skip=false;
    sunce->getHero()->findSkill("魂姿")->onPhaseStart(awakened,*sunce,TurnPhase::PREPARATION,skip);
    auto yingzi=sunce->getHero()->findSkill("英姿");
    CHECK(yingzi!=nullptr);
    int awakenedDraw=2;
    {MythInput input("n\n");yingzi->onDrawCards(awakened,*sunce,awakenedDraw);}
    CHECK_EQ(awakenedDraw,2);
    {MythInput input("y\n");yingzi->onDrawCards(awakened,*sunce,awakenedDraw);}
    CHECK_EQ(awakenedDraw,3);
}

TEST("std/guanxing_allows_reordering_cards_put_at_bottom") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"zhugeliang","zhangfei"});
    auto& deck=e.getDeck();deck.drawCards(deck.getDrawPileSize());
    auto a=makeCard("甲",Suit::SPADE,1,CardType::BASIC,CardSubType::SHA);
    auto b=makeCard("乙",Suit::CLUB,2,CardType::BASIC,CardSubType::SHAN);
    deck.putOnTop({a,b});
    bool skip=false;
    {MythInput input("y\n0\n2\n1\n");
     e.getPlayers()[0]->getHero()->findSkill("观星")->onPhaseStart(e,*e.getPlayers()[0],TurnPhase::PREPARATION,skip);}
    CHECK_EQ(deck.getDrawPileSize(),2);
    CHECK(deck.drawCard()==a); // 两张均置底：玩家先选乙为最底，甲在其上
    CHECK(deck.drawCard()==b);
}

TEST("myth/mobile_songwei_is_chosen_by_judged_wei_character_not_caopi") {
    GameEngine e;captureLog(e);e.initGame(3,1,{"caopi","guojia","guanyu"});
    auto ps=e.getPlayers();auto skill=ps[0]->getHero()->findSkill("颂威");
    CHECK(skill!=nullptr);
    auto black=makeCard("判定",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    bool claimed=false;
    int before=ps[0]->getHandCardCount();
    {MythInput input("n\n");skill->onAfterJudge(e,*ps[0],*ps[1],black,claimed);}
    CHECK_EQ(ps[0]->getHandCardCount(),before);
    {MythInput input("y\n");skill->onAfterJudge(e,*ps[0],*ps[1],black,claimed);}
    CHECK_EQ(ps[0]->getHandCardCount(),before+1);
    // 非魏角色和红色判定都不能触发颂威。
    {MythInput input("y\ny\n");
      skill->onAfterJudge(e,*ps[0],*ps[2],black,claimed);
      skill->onAfterJudge(e,*ps[0],*ps[1],makeCard("判定",Suit::HEART,5,CardType::BASIC,CardSubType::SHA),claimed);}
    CHECK_EQ(ps[0]->getHandCardCount(),before+1);
}

TEST("myth/mobile_huashen_can_roll_official_god_but_never_diy_zhonghui") {
    bool hasGod=false;
    for(unsigned seed=1;seed<180 && !hasGod;seed++) {
        GameEngine e;e.setSeed(seed);captureLog(e);
        e.initGame(2,-1,{"zuoci","guanyu"});
        auto avatar=std::dynamic_pointer_cast<MythSkill>(e.getPlayers()[0]->getHero()->findSkill("化身"));
        CHECK(avatar!=nullptr);
        for(const auto& id:avatar->getAvatarIds()) {
            auto info=HeroRegistry::find(id);
            CHECK(info!=nullptr && info->pack!="DIY包");
            if(info && info->country==Country::GOD)hasGod=true;
        }
    }
    CHECK(hasGod);
}

TEST("myth/mobile_beige_club_lets_damage_source_choose_two_discardable_cards") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"guanyu","zhangfei","caiwenji"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto a=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    auto b=makeCard("闪",Suit::HEART,8,CardType::BASIC,CardSubType::SHAN);
    auto weapon=makeCard("青釭剑",Suit::SPADE,6,CardType::EQUIPMENT,CardSubType::WEAPON);
    auto delayed=makeCard("闪电",Suit::SPADE,1,CardType::TRICK,CardSubType::SHAN_DIAN);
    ps[0]->addHandCard(a);ps[0]->addHandCard(b);ps[0]->equip(weapon);ps[0]->addJudgeCard(delayed);
    ps[2]->addHandCard(makeCard("闪",Suit::DIAMOND,2,CardType::BASIC,CardSubType::SHAN));
    e.getDeck().putOnTop({makeCard("判定",Suit::CLUB,7,CardType::BASIC,CardSubType::SHA)});
    {MythInput input("3\n1\n");
     ps[2]->getHero()->findSkill("悲歌")->onGlobalDamage(e,*ps[2],ps[0].get(),*ps[1],1,a);}
    CHECK(!ps[0]->hasEquipment(weapon));
    CHECK(!ps[0]->hasHandCard(a));
    CHECK(ps[0]->hasHandCard(b));
    CHECK_EQ(ps[0]->getJudgeZone().size(),size_t(1));
    CHECK(ps[0]->getJudgeZone()[0]==delayed);
}

TEST("myth/mobile_guixin_claims_each_other_players_zone_for_each_damage_point") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_caocao","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto a=makeCard("乐不思蜀",Suit::HEART,6,CardType::TRICK,CardSubType::LE_BU_SI_SHU);
    auto b=makeCard("闪电",Suit::SPADE,1,CardType::TRICK,CardSubType::SHAN_DIAN);
    auto c=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    auto d=makeCard("闪",Suit::DIAMOND,7,CardType::BASIC,CardSubType::SHAN);
    ps[1]->addJudgeCard(a);ps[1]->addJudgeCard(b);
    ps[2]->addHandCard(c);ps[2]->addHandCard(d);
    auto guixin=ps[0]->getHero()->findSkill("归心");
    CHECK(!ps[0]->isTurnedOver());
    guixin->onAfterDamage(e,*ps[0],ps[2].get(),2,ShaElement::NORMAL,nullptr);
    CHECK_EQ(ps[0]->getHandCardCount(),4);
    CHECK(ps[0]->hasHandCard(a));CHECK(ps[0]->hasHandCard(b));
    CHECK(ps[0]->hasHandCard(c));CHECK(ps[0]->hasHandCard(d));
    CHECK(ps[1]->getJudgeZone().empty());CHECK_EQ(ps[2]->getHandCardCount(),0);
    CHECK(!ps[0]->isTurnedOver()); // 每一点伤害结算一次翻面
}

TEST("myth/mobile_menghuo_zaiqi_replaces_draw_with_recovery_and_non_hearts") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"menghuo","guanyu"});
    auto p=e.getPlayers()[0];clearHand(*p);p->setHp(p->getMaxHp()-2);
    auto heart=makeCard("桃",Suit::HEART,8,CardType::BASIC,CardSubType::TAO);
    auto spade=makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA);
    e.getDeck().putOnTop({heart,spade});
    int n=2;
    p->getHero()->findSkill("再起")->onDrawCards(e,*p,n);
    CHECK_EQ(n,0);CHECK_EQ(p->getHp(),p->getMaxHp()-1);
    CHECK_EQ(p->getHandCardCount(),1);CHECK(p->hasHandCard(spade));
    CHECK(!p->hasHandCard(heart));
}

TEST("myth/mobile_gongxin_views_entire_hand_even_without_hearts_without_public_ai_leak") {
    auto black=makeCard("杀",Suit::SPADE,5,CardType::BASIC,CardSubType::SHA);
    {
        GameEngine e;auto output=captureLog(e);e.initGame(2,0,{"shen_lvmeng","guanyu"}); e.setPhase(TurnPhase::PLAY);
        auto ps=e.getPlayers();clearHand(*ps[1]);ps[1]->addHandCard(black);
        {MythInput input("1\n");ps[0]->getHero()->findSkill("攻心")->activate(e,*ps[0]);}
        CHECK(output->str().find("【攻心】观看"+ps[1]->getName()+"的所有手牌")!=std::string::npos);
        CHECK(output->str().find(black->getFormattedName())!=std::string::npos);
        CHECK(ps[1]->hasHandCard(black));
    }
    {
        GameEngine e;auto output=captureLog(e);e.initGame(2,-1,{"shen_lvmeng","guanyu"}); e.setPhase(TurnPhase::PLAY);
        auto ps=e.getPlayers();clearHand(*ps[1]);ps[1]->addHandCard(black);
        ps[0]->getHero()->findSkill("攻心")->activate(e,*ps[0]);
        CHECK(output->str().find("【攻心】观看"+ps[1]->getName()+"的所有手牌")==std::string::npos);
    }
}

TEST("std/mobile_fanjian_target_picks_hidden_hand_back_not_visible_card_face") {
    GameEngine e;auto out=captureLog(e);e.initGame(2,1,{"zhouyu","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    auto first=makeCard("杀",Suit::SPADE,6,CardType::BASIC,CardSubType::SHA);
    auto second=makeCard("闪",Suit::DIAMOND,8,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCard(first);ps[0]->addHandCard(second);
    {
        MythInput input("2\n");
        CHECK(e.chooseHiddenHandCard(ps[1],ps[0],"选择隐藏的牌")==second);
    }
    CHECK(out->str().find("隐藏手牌第1张")!=std::string::npos);
    CHECK(out->str().find("隐藏手牌第2张")!=std::string::npos);
    CHECK(out->str().find(first->getFormattedName())==std::string::npos);
    CHECK(out->str().find(second->getFormattedName())==std::string::npos);
    int hp=ps[1]->getHp();
    {
        MythInput input("1\n2\n"); // 猜黑桃、选择第二张牌背（实际方块）
        ps[0]->getHero()->findSkill("反间")->activate(e,*ps[0]);
    }
    CHECK(ps[1]->hasHandCard(second));CHECK(!ps[1]->hasHandCard(first));
    CHECK_EQ(ps[1]->getHp(),hp-1);
}

TEST("engine/mobile_steal_and_discard_tricks_do_not_offer_visible_opponent_hands") {
    for(auto subtype:{CardSubType::SHUN_SHOU_QIAN_YANG,CardSubType::GUO_HE_CHAI_QIAO}) {
        GameEngine e;auto output=captureLog(e);e.initGame(2,0,{"guanyu","zhangfei"});
        auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
        auto trick=makeCard("锦囊",Suit::CLUB,3,CardType::TRICK,subtype);
        auto hidden=makeCard("私牌",Suit::HEART,11,CardType::BASIC,CardSubType::TAO);
        ps[0]->addHandCard(trick);ps[1]->addHandCard(hidden);
        {
            MythInput input("1\n"); // 只显示手牌区，而不是目标的手牌名称
            CHECK(e.useCard(ps[0],trick,{ps[1]}));
        }
        CHECK(!ps[1]->hasHandCard(hidden));
        if(subtype==CardSubType::SHUN_SHOU_QIAN_YANG)CHECK(ps[0]->hasHandCard(hidden));
        auto log=output->str();
        CHECK(log.find("手牌区（1张，随机）")!=std::string::npos);
        CHECK(log.find("选择要获得的")!=std::string::npos ||
              log.find("选择要弃置的")!=std::string::npos);
    }
}

TEST("std/mobile_guanxing_does_not_publicly_reveal_an_ai_players_deck_view") {
    GameEngine e;auto out=captureLog(e);e.initGame(2,-1,{"zhugeliang","guanyu"});
    auto top=makeCard("私有测试牌",Suit::DIAMOND,13,CardType::BASIC,CardSubType::SHA);
    e.getDeck().putOnTop({top});
    bool skip=false;
    e.getPlayers()[0]->getHero()->findSkill("观星")->onPhaseStart(e,*e.getPlayers()[0],TurnPhase::PREPARATION,skip);
    CHECK(out->str().find("发动【观星】观看了牌堆顶")!=std::string::npos);
    CHECK(out->str().find("私有测试牌")==std::string::npos);
}

TEST("myth/mobile_hunzi_yinghun_does_not_retroactively_trigger_preparation_start") {
    GameEngine e;auto output=captureLog(e);e.initGame(2,-1,{"sunce","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[1]);ps[0]->setHp(1);
    bool skip=false;e.phasePreparation(ps[0],skip);
    CHECK(ps[0]->getHero()->findSkill("英魂")!=nullptr);
    CHECK(output->str().find("（英魂）")==std::string::npos);
    e.phasePreparation(ps[0],skip);
    CHECK(output->str().find("（英魂）")!=std::string::npos);
}

namespace {
struct PeachBanProbe : TriggerSkill {
    PlayerPtr other;
    bool othersSelfPeachWasBlocked=false;
    bool ownerPeachWasAllowed=false;
    explicit PeachBanProbe(PlayerPtr target):TriggerSkill("验收探针",""),other(target){}
    void onTurnStart(GameEngine& e,Player& self) override {
        othersSelfPeachWasBlocked=!e.canUsePeach(other,other);
        ownerPeachWasAllowed=e.canUsePeach(e.getPlayerById(self.getId()),other);
    }
};
}
TEST("myth/mobile_wansha_bans_non_dying_other_players_own_peaches") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"jiaxu","guanyu"});
    auto ps=e.getPlayers();auto probe=std::make_shared<PeachBanProbe>(ps[1]);
    ps[0]->getHero()->addSkill(probe);
    CHECK(e.canUsePeach(ps[1],ps[1])); // 回合外不受完杀限制
    e.runTurn(ps[0]);
    CHECK(probe->othersSelfPeachWasBlocked);
    CHECK(probe->ownerPeachWasAllowed);
}

TEST("myth/mobile_sunce_jiang_red_sha_and_duel_target_both_draw") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"sunce","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto skill=ps[0]->getHero()->findSkill("激昂");
    auto red=makeCard("杀",Suit::HEART,7,CardType::BASIC,CardSubType::SHA);
    auto black=makeCard("杀",Suit::SPADE,8,CardType::BASIC,CardSubType::SHA);
    ShaContext ctx;ctx.source=ps[1];ctx.target=ps[0];ctx.card=black;
    skill->onShaTargeted(e,*ps[0],ctx);
    CHECK_EQ(ps[0]->getHandCardCount(),0);
    ctx.card=red;skill->onShaTargeted(e,*ps[0],ctx);
    CHECK_EQ(ps[0]->getHandCardCount(),1);
    ctx.source=ps[0];ctx.target=ps[1];skill->onShaTargeted(e,*ps[0],ctx);
    CHECK_EQ(ps[0]->getHandCardCount(),2);
    skill->onDuelTargeted(e,*ps[0],*ps[1],*ps[0]);
    CHECK_EQ(ps[0]->getHandCardCount(),3);
}

TEST("myth/mobile_yinghun_sunce_and_sunjian_keep_official_choice_order") {
    for(const auto& hero:{"sunce","sunjian"}) {
        GameEngine e;auto out=captureLog(e);e.initGame(2,0,{hero,"guanyu"});
        auto ps=e.getPlayers();ps[0]->setHp(ps[0]->getMaxHp()-2);
        if(std::string(hero)=="sunce") {
            bool skip=false;ps[0]->setHp(1);
            {MythInput input("1\n");e.phasePreparation(ps[0],skip);}
            CHECK(ps[0]->getHero()->findSkill("英魂")!=nullptr);
        }
        auto skill=ps[0]->getHero()->findSkill("英魂");
        bool skip=false;
        {MythInput input("2\n1\n");skill->onPhaseStart(e,*ps[0],TurnPhase::PREPARATION,skip);}
        auto text=out->str();
        auto option=text.find("【英魂】选择结算方式");
        auto target=text.find("【英魂】选择目标");
        CHECK(option!=std::string::npos);CHECK(target!=std::string::npos);
        if(std::string(hero)=="sunce") CHECK(option<target);
        else CHECK(target<option);
    }
}

TEST("myth/mobile_renjie_counts_damage_and_each_hand_card_discarded_in_own_discard_phase") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_simayi","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    e.applyDamage(ps[1],ps[0],2);
    CHECK_EQ(ps[0]->getMark("忍"),2);
    for(int i=1;i<=5;i++)ps[0]->addHandCard(makeCard("闪",Suit::DIAMOND,i,CardType::BASIC,CardSubType::SHAN));
    e.getDeck().putOnTop({makeCard("闪",Suit::HEART,10,CardType::BASIC,CardSubType::SHAN),
                          makeCard("闪",Suit::HEART,11,CardType::BASIC,CardSubType::SHAN)});
    e.runTurn(ps[0]);
    // 两人局座位 0 是主公，起始体力上限+1：受伤后 3 HP，摸后 7 张，弃 4 张。
    CHECK_EQ(ps[0]->getMark("忍"),6);
}

TEST("myth/mobile_buqu_keeps_duplicate_wounds_until_recovery_or_death") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"zhoutai","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto a=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    auto b=makeCard("闪",Suit::HEART,7,CardType::BASIC,CardSubType::SHAN);
    ps[0]->setHp(-1);e.getDeck().putOnTop({a,b});
    ps[0]->getHero()->findSkill("不屈")->onDying(e,*ps[0],*ps[0]);
    CHECK_EQ(ps[0]->getPileCount("创"),2);
    CHECK(ps[0]->getPile("创")[0]==a);
    CHECK(ps[0]->getPile("创")[1]==b);
    e.recoverHp(ps[0],1,"测试");
    CHECK_EQ(ps[0]->getHp(),0);
    CHECK_EQ(ps[0]->getPileCount("创"),1);
}

TEST("myth/mobile_tianyi_losing_prohibits_sha_even_with_crossbow") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"taishici","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("闪",Suit::CLUB,1,CardType::BASIC,CardSubType::SHAN));
    ps[1]->addHandCard(makeCard("闪",Suit::SPADE,13,CardType::BASIC,CardSubType::SHAN));
    ps[0]->equip(makeCard("诸葛连弩",Suit::DIAMOND,1,CardType::EQUIPMENT,CardSubType::WEAPON));
    ps[0]->getHero()->findSkill("天义")->activate(e,*ps[0]);
    CHECK_EQ(e.getShaLimit(*ps[0]),0);
    auto sha=makeCard("杀",Suit::HEART,10,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(sha);
    // 处于出牌阶段时应由次数限制拒绝；此处直接验证规则入口。
    CHECK_EQ(e.getShaLimitForCard(*ps[0],sha),0);
}

namespace {
struct TurnEndDiscardProbe : TriggerSkill {
    TurnEndDiscardProbe():TriggerSkill("回合结束费用探针",""){}
    void onTurnEnd(GameEngine& e,Player& self,Player& turnOwner) override {
        if(&self==&turnOwner && !self.getHandCards().empty())
            e.discardCardOf(e.getPlayerById(self.getId()),self.getHandCards().front(),"回合结束时支付费用");
    }
};
}
TEST("engine/mobile_turn_end_payment_is_not_outside_turn_for_tuntian") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"dengai","guanyu"});
    auto ps=e.getPlayers();clearHand(*ps[0]);clearHand(*ps[1]);
    ps[0]->getHero()->addSkill(std::make_shared<TurnEndDiscardProbe>());
    ps[0]->addHandCard(makeCard("闪",Suit::DIAMOND,2,CardType::BASIC,CardSubType::SHAN));
    e.getDeck().putOnTop({makeCard("闪",Suit::DIAMOND,3,CardType::BASIC,CardSubType::SHAN),
                          makeCard("闪",Suit::DIAMOND,4,CardType::BASIC,CardSubType::SHAN)});
    e.runTurn(ps[0]);
    CHECK_EQ(ps[0]->getPileCount("田"),0);
}

TEST("myth/mobile_shenfen_resolves_everyones_damage_before_equipment_then_hand_discard") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",6);
    for(int n=1;n<=2;n++) {
        for(int i=0;i<6;i++)ps[n]->addHandCard(makeCard("闪",Suit::HEART,i+1,CardType::BASIC,CardSubType::SHAN));
        ps[n]->equip(makeCard("八卦阵",Suit::CLUB,3,CardType::EQUIPMENT,CardSubType::ARMOR));
    }
    int h1=ps[1]->getHp(),h2=ps[2]->getHp();
    ps[0]->getHero()->findSkill("神愤")->activate(e,*ps[0]);
    CHECK_EQ(ps[1]->getHp(),h1-1);CHECK_EQ(ps[2]->getHp(),h2-1);
    CHECK(ps[1]->getArmor()==nullptr);CHECK(ps[2]->getArmor()==nullptr);
    CHECK_EQ(ps[1]->getHandCardCount(),2);CHECK_EQ(ps[2]->getHandCardCount(),2);
    CHECK(ps[0]->isTurnedOver());
    CHECK_EQ(ps[0]->getMark("暴怒"),4); // 初始两枚 + 测试补六枚 - 支付六枚 + 造成两点伤害
}

TEST("myth/mobile_jilue_can_pay_for_wansha_during_play_even_without_cards") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_simayi","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("忍",4);
    int max=ps[0]->getMaxHp();bool skip=false;e.phasePreparation(ps[0],skip);
    CHECK_EQ(ps[0]->getMaxHp(),max-1);
    auto jilue=ps[0]->getHero()->findSkill("极略");CHECK(jilue!=nullptr);
    CHECK(jilue->canActivate(e,*ps[0]));
    {MythInput input("1\n");jilue->activate(e,*ps[0]);}
    CHECK_EQ(ps[0]->getMark("忍"),3);
    CHECK(ps[0]->getHero()->findSkill("完杀")!=nullptr);
    jilue->onTurnBoundary(e,*ps[0],*ps[0],false);
    CHECK(ps[0]->getHero()->findSkill("完杀")==nullptr);
}

TEST("myth/mobile_jilue_zhiheng_once_does_not_block_later_wansha") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_simayi","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("忍",4);bool skip=false;e.phasePreparation(ps[0],skip);
    auto jilue=ps[0]->getHero()->findSkill("极略");
    ps[0]->addHandCard(makeCard("闪",Suit::HEART,6,CardType::BASIC,CardSubType::SHAN));
    {MythInput input("1\n1\n1\n");jilue->activate(e,*ps[0]);}
    CHECK_EQ(ps[0]->getMark("忍"),3);
    CHECK(jilue->canActivate(e,*ps[0]));
    {MythInput input("1\n");jilue->activate(e,*ps[0]);}
    CHECK_EQ(ps[0]->getMark("忍"),2);
    CHECK(ps[0]->getHero()->findSkill("完杀")!=nullptr);
    CHECK(!jilue->canActivate(e,*ps[0]));
}

TEST("myth/mobile_jilue_spends_separate_ren_for_guicai_fangzhu_and_jizhi") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_simayi","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("忍",4);bool skip=false;e.phasePreparation(ps[0],skip);
    auto jilue=ps[0]->getHero()->findSkill("极略");
    auto hand=makeCard("杀",Suit::SPADE,9,CardType::BASIC,CardSubType::SHA);
    ps[0]->addHandCard(hand);
    // B6 之后要有收益才改判：敌人判定【八卦阵】且判定牌是红色（他会白得一张【闪】）→ 用黑色牌改判
    e.setJudgeReasonForTesting("八卦阵");
    auto judged=makeCard("闪",Suit::HEART,3,CardType::BASIC,CardSubType::SHAN);
    jilue->onBeforeJudge(e,*ps[0],*ps[1],judged);
    CHECK(judged==hand);CHECK_EQ(ps[0]->getMark("忍"),3);
    e.setJudgeReasonForTesting("");
    ps[0]->setHp(ps[0]->getMaxHp()-2);
    jilue->onAfterDamage(e,*ps[0],ps[1].get(),1,ShaElement::NORMAL,nullptr);
    CHECK(ps[1]->isTurnedOver());CHECK_EQ(ps[1]->getHandCardCount(),2);
    CHECK_EQ(ps[0]->getMark("忍"),2);
    auto trick=makeCard("过河拆桥",Suit::SPADE,8,CardType::TRICK,CardSubType::GUO_HE_CHAI_QIAO);
    jilue->onUseCard(e,*ps[0],trick);
    CHECK_EQ(ps[0]->getMark("忍"),1);
    CHECK_EQ(ps[0]->getHandCardCount(),1);
}

TEST("myth/mobile_qiaobian_skips_judgement_and_can_move_public_field_card") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"zhanghe","guanyu","zhangfei"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto skill=ps[0]->getHero()->findSkill("巧变");
    auto delayed=makeCard("乐不思蜀",Suit::HEART,6,CardType::TRICK,CardSubType::LE_BU_SI_SHU);
    ps[0]->addJudgeCard(delayed);
    ps[0]->addHandCard(makeCard("闪",Suit::HEART,7,CardType::BASIC,CardSubType::SHAN));
    bool skip=false;
    {MythInput input("y\n1\n");skill->onPhaseStart(e,*ps[0],TurnPhase::JUDGEMENT,skip);}
    CHECK(skip);CHECK_EQ(ps[0]->getJudgeZone().size(),1u);
    auto armor=makeCard("八卦阵",Suit::CLUB,2,CardType::EQUIPMENT,CardSubType::ARMOR);
    ps[1]->equip(armor);
    ps[0]->addHandCard(makeCard("闪",Suit::DIAMOND,8,CardType::BASIC,CardSubType::SHAN));
    skip=false;
    {MythInput input("y\n1\n2\n1\n2\n");skill->onPhaseStart(e,*ps[0],TurnPhase::PLAY,skip);}
    CHECK(skip);CHECK(ps[1]->getArmor()==nullptr);CHECK(ps[2]->getArmor()==armor);
}

TEST("myth/mobile_wuqian_expires_on_damage_card_that_did_not_deal_damage") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_lvbu","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",2);
    auto skill=ps[0]->getHero()->findSkill("无前");skill->activate(e,*ps[0]);
    CHECK(ps[0]->getHero()->findSkill("无双")!=nullptr);
    auto lightning=makeCard("闪电",Suit::SPADE,1,CardType::TRICK,CardSubType::SHAN_DIAN);
    ps[0]->addHandCard(lightning);
    CHECK(e.useCard(ps[0],lightning,{}));
    CHECK(ps[0]->getHero()->findSkill("无双")==nullptr);
    CHECK_EQ(ps[1]->getMark("无前防具失效"),0);
    CHECK_EQ(ps[0]->getJudgeZone().size(),1u);
}

TEST("myth/mobile_wuqian_sha_bonus_counts_only_living_marked_targets") {
    GameEngine e;captureLog(e);e.initGame(3,0,{"shen_lvbu","guanyu","zhangfei"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",2);
    {MythInput input("2\n");ps[0]->getHero()->findSkill("无前")->activate(e,*ps[0]);}
    CHECK_EQ(e.getShaLimit(*ps[0]),2);
    e.killPlayer(ps[1],ps[0]);
    CHECK_EQ(e.getShaLimit(*ps[0]),1);
}

TEST("myth/mobile_xinsheng_is_compulsory_per_point_of_damage") {
    GameEngine e;e.setSeed(13);captureLog(e);
    {MythInput input("1\n1\n");e.initGame(2,-1,{"zuoci","guanyu"});}
    auto ps=e.getPlayers();auto skill=std::dynamic_pointer_cast<MythSkill>(ps[0]->getHero()->findSkill("化身"));
    CHECK_EQ(skill->getAvatarIds().size(),size_t(2));
    ps[0]->setHp(3);
    {MythInput input("n\nn\n");e.applyDamage(ps[1],ps[0],2);}
    CHECK_EQ(skill->getAvatarIds().size(),size_t(4));
}

TEST("myth/mobile_wumou_wuxie_cannot_invent_a_character_target") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"shen_lvbu","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto wuxie=makeCard("无懈可击",Suit::SPADE,11,CardType::TRICK,CardSubType::WU_XIE_KE_JI);
    ps[0]->addHandCard(wuxie);
    auto sha=e.getConversionsFor(ps[0],wuxie,CardSubType::SHA).front();
    CHECK(e.getShaTargets(*ps[0],sha).empty());
    CHECK(!e.useCard(ps[0],sha,{ps[1]}));
    CHECK(ps[0]->hasHandCard(wuxie));
    auto response=e.getResponseCandidates(ps[0],CardSubType::SHA);
    CHECK(std::any_of(response.begin(),response.end(),[](CardPtr c){return c->getSkillSource()=="无谋";}));
}

TEST("myth/mobile_wumou_ai_uses_legal_two_target_jiedao_sha") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    ps[1]->equip(makeCard("青釭剑",Suit::SPADE,6,CardType::EQUIPMENT,CardSubType::WEAPON));
    auto jiedao=makeCard("借刀杀人",Suit::CLUB,12,CardType::TRICK,CardSubType::JIE_DAO_SHA_REN);
    ps[0]->addHandCard(jiedao);
    auto choice=AIController::makePlayDecision(e,*ps[0]);
    CHECK(choice.cardToPlay!=nullptr);
    CHECK_EQ(choice.targets.size(),size_t(2));
    CHECK(choice.targets[0]==ps[1]);
    CHECK(choice.targets[1]==ps[2]);
    CHECK(e.useCard(ps[0],choice.cardToPlay,choice.targets));
    CHECK(!ps[0]->hasHandCard(jiedao));
}

TEST("myth/mobile_haoshi_declined_next_draw_does_not_force_old_turns_split") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"lusu","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto skill=ps[0]->getHero()->findSkill("好施");
    int n=2;
    {MythInput input("y\n");skill->onDrawCards(e,*ps[0],n);}
    CHECK_EQ(n,4);
    n=2;
    {MythInput input("n\n");skill->onDrawCards(e,*ps[0],n);}
    CHECK_EQ(n,2);
    for(int i=0;i<6;i++)ps[0]->addHandCard(makeCard("杀",Suit::SPADE,i+1,CardType::BASIC,CardSubType::SHA));
    skill->onPhaseEnd(e,*ps[0],TurnPhase::DRAW);
    CHECK_EQ(ps[0]->getHandCardCount(),6);
    CHECK_EQ(ps[1]->getHandCardCount(),0);
}

TEST("myth/mobile_wumou_original_black_trick_target_restricted_by_weimu") {
    GameEngine e;captureLog(e);e.initGame(4,-1,{"shen_lvbu","jiaxu","menghuo","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto nanman=makeCard("南蛮入侵",Suit::SPADE,7,CardType::TRICK,CardSubType::NAN_MAN_RU_QIN);
    ps[0]->addHandCard(nanman);
    auto converted=e.getConversionsFor(ps[0],nanman,CardSubType::SHA).front();
    CHECK(!e.canUseShaOn(*ps[0],*ps[1],converted)); // 原黑色锦囊不能指定帷幕
    CHECK(e.canUseShaOn(*ps[0],*ps[2],converted));  // 祸首是效果无效，不是不成目标
    auto targets=e.getShaTargets(*ps[0],converted);
    CHECK(std::find(targets.begin(),targets.end(),ps[1])==targets.end());
    CHECK(std::find(targets.begin(),targets.end(),ps[2])!=targets.end());
    int weimuHp=ps[1]->getHp(),huoshouHp=ps[2]->getHp();
    CHECK(e.useCard(ps[0],converted,{}));
    CHECK_EQ(ps[1]->getHp(),weimuHp);
    CHECK_EQ(ps[2]->getHp(),huoshouHp-1);
}

TEST("myth/mobile_nanman_huoshou_is_effect_immunity_not_target_ban") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"guanyu","menghuo"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto nanman=makeCard("南蛮入侵",Suit::HEART,7,CardType::TRICK,CardSubType::NAN_MAN_RU_QIN);
    CHECK(e.canBeTargeted(ps[1],nanman,ps[0]));
    CHECK(!e.canTakeCardEffect(ps[1],nanman,ps[0]));
    int hp=ps[1]->getHp();ps[0]->addHandCard(nanman);
    CHECK(e.useCard(ps[0],nanman,{}));
    CHECK_EQ(ps[1]->getHp(),hp);
}

TEST("myth/mobile_wumou_keeps_original_shunshou_distance_and_hand_constraints") {
    GameEngine e;captureLog(e);e.initGame(4,-1,{"shen_lvbu","guanyu","zhangfei","lvmeng"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto snatch=makeCard("顺手牵羊",Suit::DIAMOND,3,CardType::TRICK,CardSubType::SHUN_SHOU_QIAN_YANG);
    ps[0]->addHandCard(snatch);
    auto sha=e.getConversionsFor(ps[0],snatch,CardSubType::SHA).front();
    CHECK(!e.canUseShaOn(*ps[0],*ps[1],sha)); // 没有任何可获得的牌
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    ps[2]->addHandCard(makeCard("闪",Suit::HEART,3,CardType::BASIC,CardSubType::SHAN));
    CHECK(e.canUseShaOn(*ps[0],*ps[1],sha));
    CHECK(!e.canUseShaOn(*ps[0],*ps[2],sha)); // 即使【杀】可无视距离，也须满足原锦囊的距离
    CHECK(!e.useCard(ps[0],sha,{ps[2]}));
    CHECK(ps[0]->hasHandCard(snatch));
}

TEST("myth/mobile_wumou_tiesuo_keeps_optional_original_one_or_two_targets") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto tiesuo=makeCard("铁索连环",Suit::CLUB,9,CardType::TRICK,CardSubType::TIE_SUO_LIAN_HUAN);
    ps[0]->addHandCard(tiesuo);
    auto sha=e.getConversionsFor(ps[0],tiesuo,CardSubType::SHA).front();
    CHECK_EQ(e.getShaTargetLimit(*ps[0],sha),2);
    CHECK(e.canUseShaOn(*ps[0],*ps[0],sha));
    CHECK(!e.useCard(ps[0],sha,{})); // 原锦囊的重铸不是使用，不能转化为无目标的杀
    CHECK(ps[0]->hasHandCard(tiesuo));
    int ownerHp=ps[0]->getHp(),otherHp=ps[1]->getHp();
    CHECK(e.useCard(ps[0],sha,{ps[0],ps[1]}));
    CHECK_EQ(ps[0]->getHp(),ownerHp-1);
    CHECK_EQ(ps[1]->getHp(),otherHp-1);
}

TEST("myth/mobile_jushou_runs_at_finish_phase_start_in_real_turn") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"caoren","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    {MythInput input("0\ny\n");e.runTurn(ps[0]);}
    CHECK(ps[0]->isTurnedOver());
    CHECK_EQ(ps[0]->getHandCardCount(),5);
}

TEST("myth/mobile_niepan_draws_before_recovery_through_engine_hook") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,0,{"pangtong","guanyu"});
    auto p=e.getPlayers()[0];clearHand(*p);
    p->setHp(0);p->setTurnedOver(true);p->setChained(true);
    p->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    {MythInput input("y\n");p->getHero()->findSkill("涅槃")->onDying(e,*p,*p);}
    CHECK_EQ(p->getHp(),3);
    CHECK_EQ(p->getHandCardCount(),3);
    CHECK(!p->isTurnedOver());CHECK(!p->isChained());
    auto text=log->str();
    auto drawn=text.find("摸了 3 张牌");auto recovered=text.find("回复了 3 点体力");
    CHECK(drawn!=std::string::npos);
    CHECK(recovered!=std::string::npos);
    CHECK(drawn<recovered);
}

TEST("myth/mobile_qiaobian_can_skip_preparation_and_finish_as_official_any_phase") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"zhanghe","guanyu"});
    auto p=e.getPlayers()[0];clearHand(*p);
    auto skill=p->getHero()->findSkill("巧变");
    p->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    bool prep=false;
    {MythInput input("y\n1\n");skill->onPhaseStart(e,*p,TurnPhase::PREPARATION,prep);}
    CHECK(prep);CHECK_EQ(p->getHandCardCount(),0);
    p->addHandCard(makeCard("闪",Suit::HEART,3,CardType::BASIC,CardSubType::SHAN));
    bool finish=false;
    {MythInput input("y\n1\n");skill->onPhaseStart(e,*p,TurnPhase::FINISH,finish);}
    CHECK(finish);CHECK_EQ(p->getHandCardCount(),0);
}

TEST("engine/mobile_qiaobian_skip_preparation_keeps_rest_of_turn") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"zhanghe","guanyu"});
    auto p=e.getPlayers()[0];clearHand(*p);
    p->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    {MythInput input("y\n1\n");e.runTurn(p);}
    // 跳过准备阶段不会误当作跳过整个回合；摸牌阶段仍然摸牌。
    CHECK(p->getHandCardCount()>=2);
}

TEST("engine/mobile_qiaobian_skip_finish_is_an_actual_skipped_stage") {
    GameEngine e;auto log=captureLog(e);e.initGame(2,0,{"zhanghe","guanyu"});
    auto p=e.getPlayers()[0];clearHand(*p);
    p->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    {MythInput input("n\nn\nn\nn\n0\nn\ny\n1\n");e.runTurn(p);}
    CHECK(log->str().find("跳过了结束阶段")!=std::string::npos);
    CHECK_EQ(p->getHandCardCount(),2);
}

TEST("myth/mobile_wuqian_only_tracks_used_damage_cards_not_sha_played_as_response") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"shen_lvbu","guanyu"}); e.setPhase(TurnPhase::PLAY);
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addMark("暴怒",2);
    auto skill=ps[0]->getHero()->findSkill("无前");
    {MythInput input("2\n");skill->activate(e,*ps[0]);}
    CHECK_EQ(ps[1]->getMark("无前防具失效"),1);
    auto duel=makeCard("决斗",Suit::HEART,8,CardType::TRICK,CardSubType::JUE_DOU);
    ps[0]->addHandCard(duel);
    CardPtr response;
    {MythInput input("1\n");response=e.askResponseCard(ps[0],CardSubType::SHA,"决斗请打出杀");}
    CHECK(response!=nullptr);
    CHECK_EQ(response->getSkillSource(),std::string("无谋"));
    skill->onCardResolved(e,*ps[0],response); // 纯打出的牌不在“使用伤害牌”的跟踪集内
    CHECK_EQ(ps[1]->getMark("无前防具失效"),1);
}

TEST("myth/mobile_wumou_original_global_benefit_trick_has_all_living_targets") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto harvest=makeCard("五谷丰登",Suit::HEART,3,CardType::TRICK,CardSubType::WU_GU_FENG_DENG);
    ps[0]->addHandCard(harvest);
    auto sha=e.getConversionsFor(ps[0],harvest,CardSubType::SHA).front();
    CHECK_EQ(e.getShaTargetLimit(*ps[0],sha),3);
    auto targets=e.getShaTargets(*ps[0],sha);
    CHECK_EQ(targets.size(),size_t(3));
    CHECK(std::find(targets.begin(),targets.end(),ps[0])!=targets.end());
    int selfHp=ps[0]->getHp(),enemyHp=ps[1]->getHp();
    CHECK(e.useCard(ps[0],sha,{ps[1]}));
    CHECK_EQ(ps[0]->getHp(),selfHp-1); // 原锦囊指定所有人，不能只选择敌人
    CHECK_EQ(ps[1]->getHp(),enemyHp-1);
}

TEST("myth/mobile_wumou_fire_attack_sha_keeps_target_hand_condition") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"shen_lvbu","guanyu","zhangfei"});
    auto ps=e.getPlayers();e.setPhase(TurnPhase::PLAY);for(auto p:ps)clearHand(*p);
    auto fire=makeCard("火攻",Suit::DIAMOND,2,CardType::TRICK,CardSubType::HUO_GONG);
    ps[0]->addHandCard(fire);
    auto sha=e.getConversionsFor(ps[0],fire,CardSubType::SHA).front();
    CHECK(!e.canUseShaOn(*ps[0],*ps[1],sha));
    ps[1]->addHandCard(makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN));
    CHECK(e.canUseShaOn(*ps[0],*ps[1],sha));
    CHECK(!e.canUseShaOn(*ps[0],*ps[2],sha));
}

TEST("myth/mobile_qiaobian_draw_mode_can_select_self_when_wording_does_not_say_other") {
    GameEngine e;captureLog(e);e.initGame(2,0,{"zhanghe","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    auto cost=makeCard("杀",Suit::SPADE,1,CardType::BASIC,CardSubType::SHA);
    auto own=makeCard("闪",Suit::HEART,2,CardType::BASIC,CardSubType::SHAN);
    auto other=makeCard("闪",Suit::DIAMOND,3,CardType::BASIC,CardSubType::SHAN);
    ps[0]->addHandCards({cost,own});ps[1]->addHandCard(other);
    bool skip=false;
    {
        MythInput input("y 1 1 1 0"); // 发动、弃置代价、选自己、选自己的唯一手牌、结束选择。
        ps[0]->getHero()->findSkill("巧变")->onPhaseStart(e,*ps[0],TurnPhase::DRAW,skip);
    }
    CHECK(skip);
    CHECK(ps[0]->hasHandCard(own));
    CHECK(ps[1]->hasHandCard(other));
    CHECK_EQ(ps[1]->getHandCardCount(),1);
}
