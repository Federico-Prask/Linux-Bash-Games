#include "test_helpers.h"
#include "HeroRegistry.h"
#include "SkillsMyth.h"
#include <set>
#include <algorithm>

TEST("myth/roster_8_plus_2_in_each_pack") {
    std::vector<std::string> packs = {"风包","火包","林包","山包"};
    for (auto pack : packs) {
        int n=0, gods=0;
        for(const auto& info:HeroRegistry::all()) if(info.pack==pack) {
            ++n; if(info.country==Country::GOD)++gods;
            auto h=info.create();
            CHECK(h!=nullptr && !h->getSkills().empty());
            CHECK_EQ(h->getPack(), pack);
        }
        CHECK_EQ(n,10);
        CHECK_EQ(gods,2);
    }
    auto cats=HeroRegistry::packCategories();
    CHECK_EQ(cats.front().subpacks.size(),size_t(5));
    CHECK_EQ(cats.front().packs.size(),size_t(5));
}

TEST("myth/person_keys_and_random_avoid_variants") {
    CHECK_EQ(HeroRegistry::personKey("jie_huangzhong"),HeroRegistry::personKey("huangzhong"));
    CHECK_EQ(HeroRegistry::personKey("wolong"),HeroRegistry::personKey("zhugeliang"));
    CHECK_EQ(HeroRegistry::personKey("shen_zhugeliang"),HeroRegistry::personKey("zhugeliang"));
    CHECK_EQ(HeroRegistry::personKey("diy_zhonghui"),HeroRegistry::personKey("jie_zhonghui"));
    // 谋·/势·/友· 版本同样视为同一人物（用户 2026-10-04 裁定）。
    CHECK_EQ(HeroRegistry::personKey("mou_machao"),HeroRegistry::personKey("machao"));
    CHECK_EQ(HeroRegistry::personKey("shi_dengai"),HeroRegistry::personKey("dengai"));
    CHECK_EQ(HeroRegistry::personKey("mou_zhugeliang"),HeroRegistry::personKey("zhugeliang"));
    CHECK_EQ(HeroRegistry::personKey("you_zhugeliang"),HeroRegistry::personKey("zhugeliang"));
    CHECK_EQ(HeroRegistry::personKey("shi_zhonghui"),HeroRegistry::personKey("zhonghui"));
    CHECK_EQ(HeroRegistry::personKey("mou_zhouyu"),HeroRegistry::personKey("zhouyu"));
    for(unsigned seed=1;seed<=30;seed++) {
        GameEngine e;e.setSeed(seed);captureLog(e);
        e.initGame(8,-1,{"huangzhong","jie_huangzhong","wolong","shen_zhugeliang",
                             "zhonghui","diy_zhonghui","shen_guanyu","guanyu"});
        CHECK_EQ(e.getPlayers()[0]->getHero()->getId(),std::string("huangzhong"));
        std::set<std::string> persons;
        for(auto p:e.getPlayers()) {
            auto id=p->getHero()->getId();
            CHECK(persons.insert(HeroRegistry::personKey(id)).second);
        }
    }
    // 谋·/势· 与标/界版本同场时，后指定者改随机（不再出现两人同人物）。
    for(unsigned seed=1;seed<=10;seed++) {
        GameEngine e;e.setSeed(seed);captureLog(e);
        e.initGame(4,-1,{"machao","mou_machao","dengai","shi_dengai"});
        std::set<std::string> persons;
        for(auto p:e.getPlayers()) {
            auto id=p->getHero()->getId();
            CHECK(persons.insert(HeroRegistry::personKey(id)).second);
        }
        CHECK_EQ(e.getPlayers()[0]->getHero()->getId(),std::string("machao"));
        CHECK_EQ(e.getPlayers()[2]->getHero()->getId(),std::string("dengai"));
    }
}

TEST("myth/elemental_chain_unlinks_and_hits_each_once") {
    GameEngine e;captureLog(e);e.initGame(3,-1,{"guanyu","zhangfei","lvmeng"});
    auto ps=e.getPlayers();for(auto p:ps){p->setChained(true);p->setHp(4);}
    e.applyDamage(ps[0],ps[1],1,ShaElement::THUNDER);
    CHECK_EQ(ps[0]->getHp(),3);CHECK_EQ(ps[1]->getHp(),3);CHECK_EQ(ps[2]->getHp(),3);
    for(auto p:ps)CHECK(!p->isChained());
    e.applyDamage(ps[0],ps[1],1,ShaElement::THUNDER);
    CHECK_EQ(ps[2]->getHp(),3); // no second transmission
}

TEST("myth/pindian_and_niepan_once") {
    GameEngine e;captureLog(e);e.initGame(2,-1,{"pangtong","guanyu"});
    auto ps=e.getPlayers();for(auto p:ps)clearHand(*p);
    ps[0]->addHandCard(makeCard("杀",Suit::SPADE,13,CardType::BASIC,CardSubType::SHA));
    ps[1]->addHandCard(makeCard("杀",Suit::HEART,1,CardType::BASIC,CardSubType::SHA));
    CHECK(e.pindian(ps[0],ps[1],"测试"));
    CHECK_EQ(ps[0]->getHandCardCount(),0);
    ps[0]->setHp(1); e.applyDamage(ps[1],ps[0],2);
    CHECK(ps[0]->isAlive()); CHECK_EQ(ps[0]->getHp(),3);
    CHECK_EQ(ps[0]->getHandCardCount(),3);
}

TEST("myth/conversions_and_limited_metadata") {
    GameEngine e;captureLog(e);e.initGame(4,-1,{"wolong","pangtong","dianwei","shen_zhaoyun"});
    auto ps=e.getPlayers();
    auto black=makeCard("杀",Suit::CLUB,7,CardType::BASIC,CardSubType::SHA);
    auto red=makeCard("桃",Suit::HEART,4,CardType::BASIC,CardSubType::TAO);
    ps[0]->addHandCard(black);ps[0]->addHandCard(red);
    CHECK(e.getConversionsFor(ps[0],black,CardSubType::WU_XIE_KE_JI).size()==1);
    CHECK(e.getConversionsFor(ps[0],red,CardSubType::HUO_GONG).size()==1);
    ps[1]->addHandCard(black);
    CHECK(e.getConversionsFor(ps[1],black,CardSubType::TIE_SUO_LIAN_HUAN).size()==1);
    CHECK(ps[1]->getHero()->findSkill("涅槃")->hasTag(SkillTag::LIMITED));
    CHECK(HeroRegistry::create("jiaxu")->findSkill("乱武")->hasTag(SkillTag::LIMITED));
}

TEST("roster/mobile_official_packages_and_hero_versions") {
    // 移动版官网武将列表 1..57、139..146；神将依项目要求归回四篇各 2 名。
    const std::vector<std::pair<std::string,std::vector<std::string>>> expected = {
        {"标准包",{"liubei","guanyu","zhangfei","zhugeliang","zhaoyun","machao","huangyueying",
                    "sunquan","ganning","lvmeng","huanggai","zhouyu","daqiao","luxun",
                    "caocao","simayi","xiahoudun","zhangliao","xuchu","guojia","zhenji",
                    "huatuo","lvbu","diaochan","sunshangxiang"}},
        {"风包",{"huangzhong","weiyan","xiahouyuan","caoren","xiaoqiao","zhoutai","zhangjiao","yuji",
                  "shen_guanyu","shen_lvmeng"}},
        {"火包",{"dianwei","xunyu","pangtong","wolong","taishici","pangde","yanliang_wenchou","yuanshao",
                  "shen_zhouyu","shen_zhugeliang"}},
        {"林包",{"xuhuang","caopi","sunjian","dongzhuo","zhurong","menghuo","jiaxu","lusu",
                  "shen_caocao","shen_lvbu"}},
        {"山包",{"zhanghe","dengai","jiangwei","liuchan","sunce","zhangzhao_zhanghong","zuoci","caiwenji",
                  "shen_zhaoyun","shen_simayi"}},
        {"界限突破",{"jie_guanyu","jie_zhangfei","jie_zhaoyun","jie_machao","jie_huangzhong","jie_zhonghui"}},
        {"一将成名",{"zhonghui"}},
        {"DIY包",{"diy_zhonghui"}},
        {"谋攻篇",{"mou_liucheng","mou_lvmeng","mou_huangzhong","mou_huaxiong","mou_yangwan","mou_machao","mou_zhangfei","mou_zhaoyun","mou_sunshangxiang","mou_xiahoushi","mou_zhouyu","mou_diaochan","mou_yuanshao","mou_pangtong","mou_liubei","mou_jiangwei","mou_fazheng","mou_chengong","mou_ganning","mou_huanggai","mou_sunquan","mou_daqiao","mou_menghuo","mou_sunce","mou_zhurong","mou_luzhi","mou_zhugeliang","mou_guanyu","mou_huangyueying","mou_xiaoqiao","mou_gongsunzan","mou_handang","mou_luxun","mou_jiaxu","mou_zhugejin","mou_lvbu","mou_zhuran"}},
        {"势包",{"shi_xiaoqiao","shi_taishici","shi_dongzhao","shi_yuji","shi_xinxianying","shi_lusu","shi_zhonghui","shi_dengai","shi_suncun","shi_zhouyu","shi_huangzu","shi_tianfeng"}},
        {"始计篇·智",{"luotong"}},
        {"神话再临·阴",{"xuyou"}},
        {"身份武将",{"huaman"}},
        {"友",{"you_zhugeliang"}},
    };
    std::set<std::string> ids;
    for(const auto& [pack,names]:expected)for(const auto& id:names) {
        CHECK(ids.insert(id).second);
        auto hero=HeroRegistry::find(id);
        CHECK(hero!=nullptr);
        if(hero)CHECK_EQ(hero->pack,pack);
    }
    CHECK_EQ(ids.size(),HeroRegistry::all().size());
}
