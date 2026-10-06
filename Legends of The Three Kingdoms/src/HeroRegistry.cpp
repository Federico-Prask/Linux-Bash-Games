#include "HeroRegistry.h"
#include "Skills.h"
#include "SkillsStd.h"
#include "SkillsMyth.h"
#include "SkillsMou.h"
#include "SkillsExtra.h"
#include "SkillsNewHeroes.h"

namespace Thks {

namespace {

std::vector<HeroInfo> buildRegistry() {
    std::vector<HeroInfo> list;

    auto add = [&](const std::string& id, const std::string& name, const std::string& title, const std::string& pack,
                   Country country, Gender gender, int maxHp, std::function<std::vector<SkillPtr>()> skillFactory) {
        HeroInfo info{id, name, title, pack, country, gender, maxHp, nullptr};
        info.create = [info, skillFactory]() {
            auto hero = std::make_shared<Hero>(info.id, info.name, info.title, info.pack, info.country, info.gender, info.maxHp);
            for (auto& s : skillFactory()) hero->addSkill(s);
            return hero;
        };
        list.push_back(info);
    };

    // ===================== 标准包 · 五虎上将 =====================
    add("guanyu", "关羽", "美髯公", "标准包", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<WuShengSkill>(false) };
    });
    add("zhangfei", "张飞", "万夫不当", "标准包", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<PaoXiaoSkill>(false) };
    });
    add("zhaoyun", "赵云", "少年将军", "标准包", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<LongDanSkill>() };
    });
    add("machao", "马超", "一骑当千", "标准包", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MaShuSkill>(), std::make_shared<TieQiSkill>() };
    });
    add("huangzhong", "黄忠", "老当益壮", "风包", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<LieGongSkill>() };
    });

    // ===================== 标准包 · 蜀 =====================
    add("liubei", "刘备", "乱世的枭雄", "标准包", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<RenDeSkill>(), std::make_shared<JiJiangSkill>() };
    });
    add("zhugeliang", "诸葛亮", "迟暮的丞相", "标准包", Country::SHU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<GuanXingSkill>(), std::make_shared<KongChengSkill>() };
    });
    add("huangyueying", "黄月英", "归隐的杰女", "标准包", Country::SHU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<JiZhiSkill>(), std::make_shared<QiCaiSkill>() };
    });

    // ===================== 标准包 · 魏 =====================
    add("caocao", "曹操", "魏武帝", "标准包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<JianXiongSkill>(), std::make_shared<HuJiaSkill>() };
    });
    add("simayi", "司马懿", "狼顾之鬼", "标准包", Country::WEI, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<FanKuiSkill>(), std::make_shared<GuiCaiSkill>() };
    });
    add("xiahoudun", "夏侯惇", "独眼的罗刹", "标准包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<GangLieSkill>() };
    });
    add("zhangliao", "张辽", "前将军", "标准包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<TuXiSkill>() };
    });
    add("xuchu", "许褚", "虎痴", "标准包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<LuoYiSkill>() };
    });
    add("guojia", "郭嘉", "早终的先知", "标准包", Country::WEI, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<TianDuSkill>(), std::make_shared<YiJiSkill>() };
    });
    add("zhenji", "甄姬", "薄幸的美人", "标准包", Country::WEI, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<QingGuoSkill>(), std::make_shared<LuoShenSkill>() };
    });

    // ===================== 标准包 · 吴 =====================
    add("sunquan", "孙权", "年轻的贤君", "标准包", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<ZhiHengSkill>(), std::make_shared<JiuYuanSkill>() };
    });
    add("ganning", "甘宁", "锦帆游侠", "标准包", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<QiXiSkill>() };
    });
    add("lvmeng", "吕蒙", "白衣渡江", "标准包", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<KeJiSkill>() };
    });
    add("huanggai", "黄盖", "轻身为国", "标准包", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<KuRouSkill>() };
    });
    add("zhouyu", "周瑜", "大都督", "标准包", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<YingZiSkill>(), std::make_shared<FanJianSkill>() };
    });
    add("daqiao", "大乔", "矜持之花", "标准包", Country::WU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<GuoSeSkill>(), std::make_shared<LiuLiSkill>() };
    });
    add("sunshangxiang", "孙尚香", "弓腰姬", "标准包", Country::WU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<JieYinSkill>(), std::make_shared<XiaoJiSkill>() };
    });
    add("luxun", "陆逊", "儒生雄才", "标准包", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<QianXunSkill>(), std::make_shared<LianYingSkill>() };
    });

    // ===================== 标准包 · 群 =====================
    add("huatuo", "华佗", "神医", "标准包", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<JiJiuSkill>(), std::make_shared<QingNangSkill>() };
    });
    add("lvbu", "吕布", "战神", "标准包", Country::QUN, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<WuShuangSkill>() };
    });
    add("diaochan", "貂蝉", "绝世的舞姬", "标准包", Country::QUN, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<LiJianSkill>(), std::make_shared<BiYueSkill>() };
    });

    // ===================== 界限突破 · 五虎上将 =====================
    add("jie_guanyu", "界关羽", "美髯公", "界限突破", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<WuShengSkill>(true), std::make_shared<YiJueSkill>() };
    });
    add("jie_zhangfei", "界张飞", "万夫不当", "界限突破", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<PaoXiaoSkill>(true), std::make_shared<TiShenSkill>() };
    });
    add("jie_zhaoyun", "界赵云", "少年将军", "界限突破", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<LongDanSkill>(), std::make_shared<YaJiaoSkill>() };
    });
    add("jie_machao", "界马超", "一骑当千", "界限突破", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MaShuSkill>(), std::make_shared<JieTieQiSkill>() };
    });
    add("jie_huangzhong", "界黄忠", "老当益壮", "界限突破", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<JieLieGongSkill>() };
    });
    add("jie_zhonghui", "界钟会", "桀骜的野心家", "界限突破", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<QuanJiSkill>(true), std::make_shared<ZiLiSkill>(true) };
    });

    // ===================== 一将成名 =====================
    add("zhonghui", "钟会", "桀骜的野心家", "一将成名", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<QuanJiSkill>(), std::make_shared<ZiLiSkill>() };
    });

    // ===================== DIY 包 =====================
    // DIY-钟会：权谋/权计/自立均为持恒技（自立觉醒效果不变，觉醒后获得排异）
    add("diy_zhonghui", "DIY钟会", "桀骜的野心家", "DIY包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<QuanMouSkill>(), std::make_shared<QuanJiConstSkill>(),
                                      std::make_shared<ZiLiConstSkill>() };
    });

    // 神话再临：每篇8普通+2神武将；黄忠已在风包，不重复注册。
    for (const auto& m : mythHeroes()) {
        if (std::string(m.id) == "huangzhong") continue;
        add(m.id,m.name,m.title,m.pack,m.country,m.gender,m.hp,
            [skills = std::string(m.skills)] {return createMythSkills(skills.c_str());});
    }

    // ===================== 谋攻篇（37名，官网 hero-detail 附录C） =====================
    add("mou_liucheng", "谋·刘赪", "", "谋攻篇", Country::QUN, Gender::FEMALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLueYingSkill>(), std::make_shared<MouYingWuSkill>() };
    });

    add("mou_lvmeng", "谋·吕蒙", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouKeJiSkill>(), std::make_shared<MouDuJiangSkill>() }; // 夺荆由渡江觉醒获得
    });

    add("mou_huangzhong", "谋·黄忠", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLieGongSkill>() };
    });

    add("mou_huaxiong", "谋·华雄", "", "谋攻篇", Country::QUN, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouYaoWuSkill>(), std::make_shared<MouYangWeiSkill>() };
    });

    add("mou_yangwan", "谋·杨婉", "", "谋攻篇", Country::QUN, Gender::FEMALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouMingXuanSkill>(), std::make_shared<MouXianChouSkill>() };
    });

    add("mou_machao", "谋·马超", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouTieQiSkill>(), std::make_shared<MouMaShuSkill>() };
    });

    add("mou_zhangfei", "谋·张飞", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouPaoXiaoSkill>(), std::make_shared<MouXieJiSkill>() };
    });

    add("mou_zhaoyun", "谋·赵云", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLongDanSkill>(), std::make_shared<MouJiZhuSkill>() };
    });

    add("mou_sunshangxiang", "谋·孙尚香", "", "谋攻篇", Country::SHU, Gender::FEMALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouJieYinSkill>(), std::make_shared<MouLiangZhuSkill>(), std::make_shared<MouXiaoJiSkill>() };
    });

    add("mou_xiahoushi", "谋·夏侯氏", "", "谋攻篇", Country::SHU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouYanYuSkill>(), std::make_shared<MouQiaoShiSkill>() };
    });

    add("mou_zhouyu", "谋·周瑜", "", "谋攻篇", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouYingZiSkill>(), std::make_shared<MouFanJianSkill>() };
    });

    add("mou_diaochan", "谋·貂蝉", "", "谋攻篇", Country::QUN, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLiJianSkill>(), std::make_shared<MouBiYueSkill>() };
    });

    add("mou_yuanshao", "谋·袁绍", "", "谋攻篇", Country::QUN, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLuanJiSkill>(), std::make_shared<MouXueYiSkill>() };
    });

    add("mou_pangtong", "谋·庞统", "", "谋攻篇", Country::SHU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLianHuanSkill>(), std::make_shared<MouNiePanSkill>() };
    });

    add("mou_liubei", "谋·刘备", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouRenDeSkill>(), std::make_shared<MouZhangWuSkill>(), std::make_shared<MouJiJiangSkill>() };
    });

    add("mou_jiangwei", "谋·姜维", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouTiaoXinSkill>(), std::make_shared<MouZhiJiSkill>() };
    });

    add("mou_fazheng", "谋·法正", "", "谋攻篇", Country::SHU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouXuanHuoSkill>(), std::make_shared<MouEnYuanSkill>() };
    });

    add("mou_chengong", "谋·陈宫", "", "谋攻篇", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouMingCeSkill>(), std::make_shared<MouZhiChiSkill>() };
    });

    add("mou_ganning", "谋·甘宁", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouQiXiSkill>(), std::make_shared<MouFenWeiSkill>() };
    });

    add("mou_huanggai", "谋·黄盖", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouKuRouSkill>(), std::make_shared<MouZhaXiangSkill>() };
    });

    add("mou_sunquan", "谋·孙权", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouZhiHengSkill>(), std::make_shared<MouTongYeSkill>(), std::make_shared<MouJiuYuanSkill>() };
    });

    add("mou_daqiao", "谋·大乔", "", "谋攻篇", Country::WU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouGuoSeSkill>(), std::make_shared<MouLiuLiSkill>() };
    });

    add("mou_menghuo", "谋·孟获", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouHuoShouSkill>(), std::make_shared<MouZaiQiSkill>() };
    });

    add("mou_sunce", "谋·孙策", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouJiAngSkill>(), std::make_shared<MouHunZiSkill>(), std::make_shared<MouZhiBaSkill>() }; // 英姿/英魂由魂姿觉醒获得
    });

    add("mou_zhurong", "谋·祝融", "", "谋攻篇", Country::SHU, Gender::FEMALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouLieRenSkill>(), std::make_shared<MouJuXiangSkill>() };
    });

    add("mou_luzhi", "谋·卢植", "", "谋攻篇", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouMingRenSkill>(), std::make_shared<MouZhenLiangSkill>() };
    });

    add("mou_zhugeliang", "谋·诸葛亮", "", "谋攻篇", Country::SHU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouHuoJiSkill>(), std::make_shared<MouKanPoSkill>(), std::make_shared<MouGuanXingXinSkill>(), std::make_shared<MouKongChengXinSkill>() };
    });

    add("mou_guanyu", "谋·关羽", "", "谋攻篇", Country::SHU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouWuShengSkill>(), std::make_shared<MouYiJueSkill>() };
    });

    add("mou_huangyueying", "谋·黄月英", "", "谋攻篇", Country::SHU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouJiZhiSkill>(), std::make_shared<MouQiCaiSkill>() };
    });

    add("mou_xiaoqiao", "谋·小乔", "", "谋攻篇", Country::WU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouTianXiangSkill>(), std::make_shared<MouHongYanSkill>() };
    });

    add("mou_gongsunzan", "谋·公孙瓒", "", "谋攻篇", Country::QUN, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouYiCongSkill>(), std::make_shared<MouQiaoMengSkill>() };
    });

    add("mou_handang", "谋·韩当", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouGongQiSkill>(), std::make_shared<MouJieFanSkill>() };
    });

    add("mou_luxun", "谋·陆逊", "", "谋攻篇", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouQianXunSkill>(), std::make_shared<MouLianYingSkill>() };
    });

    add("mou_jiaxu", "谋·贾诩", "", "谋攻篇", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouWanShaSkill>(), std::make_shared<MouLuanWuSkill>(), std::make_shared<MouWeiMuSkill>() };
    });

    add("mou_zhugejin", "谋·诸葛瑾", "", "谋攻篇", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouHuanShiSkill>(), std::make_shared<MouHongYuanSkill>(), std::make_shared<MouMingZheSkill>() };
    });

    add("mou_lvbu", "谋·吕布", "", "谋攻篇", Country::QUN, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouWuShuangSkill>(), std::make_shared<MouLiYuSkill>() };
    });

    add("mou_zhuran", "谋·朱然", "", "谋攻篇", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<MouZhenWeiSkill>(), std::make_shared<MouHeYuanSkill>() };
    });



    // ===================== 花鬘 / 友系 / 势系（docs/shi_you_appendix.md，官网逐字） =====================
    // 花鬘：官网详情页无包标注且与标包无同名技能 → 技能名不加前缀（终案裁定）。
    add("huaman", "花鬘", "", "身份武将", Country::SHU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<XiangZhenSkill>(), std::make_shared<FangZongSkill>(),
                                      std::make_shared<XiZhanSkill>() };
    });

    // 友·诸葛亮：技能名「友-」前缀（中文一字规则）。
    add("you_zhugeliang", "友·诸葛亮", "", "友", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<YouYanCeSkill>(), std::make_shared<YouFangQiuSkill>(),
                                      std::make_shared<YouGongLiSkill>() };
    });

    add("shi_taishici", "势·太史慈", "", "势包", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiHanZhanSkill>(), std::make_shared<ShiZhanLieSkill>(),
                                      std::make_shared<ShiZhenFengSkill>() };
    });

    add("shi_dongzhao", "势·董昭", "", "势包", Country::WEI, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiMiaoLveSkill>(), std::make_shared<ShiYingJiaSkill>() };
    });

    add("shi_yuji", "势·于吉", "", "势包", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiFuJiSkill>(), std::make_shared<ShiDaoZhuanSkill>() };
    });

    add("shi_xinxianying", "势·辛宪英", "", "势包", Country::WEI, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiJieJieSkill>(), std::make_shared<ShiQingShiSkill>() };
    });

    // 与标准包同名技能但官网原文完全不同 → 加势-前缀。
    add("shi_lusu", "势·鲁肃", "", "势包", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiHaoShiSkill>(), std::make_shared<ShiDiMengSkill>() };
    });

    add("shi_zhonghui", "势·钟会", "", "势包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiSiZhiSkill>(), std::make_shared<ShiXieZhiSkill>(),
                                      std::make_shared<ShiYuNanSkill>(), std::make_shared<ShiKeChangSkill>() };
    });

    // 与标准包同名技能但官网原文完全不同 → 加势-前缀。
    add("shi_dengai", "势·邓艾", "", "势包", Country::WEI, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiTunTianSkill>(), std::make_shared<ShiZaoXianSkill>(),
                                      std::make_shared<ShiJiXiSkill>() };
    });

    add("shi_suncun", "势·孙綝", "", "势包", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiNiGuSkill>(), std::make_shared<ShiLuLianSkill>() };
    });

    add("shi_zhouyu", "势·周瑜", "", "势包", Country::WU, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiChiYunSkill>(), std::make_shared<ShiYanHuiSkill>(),
                                      std::make_shared<ShiFenTaoSkill>(), std::make_shared<ShiXiongZiSkill>() };
    });

    // ===================== 势包（势·小乔，hero-detail-666） =====================
    add("shi_xiaoqiao", "势·小乔", "", "势包", Country::WU, Gender::FEMALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiHeYunSkill>(), std::make_shared<ShiYinHuiSkill>() };
    });

    // ===================== 势包（续，hero-detail-620/623） =====================
    add("shi_huangzu", "势·黄祖", "", "势包", Country::QUN, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiChiZhangSkill>(), std::make_shared<ShiDuanYangSkill>() };
    });
    add("shi_tianfeng", "势·田丰", "", "势包", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<ShiGangGengSkill>(), std::make_shared<ShiSiJianSkill>() };
    });

    // ===================== 始计篇·智包 =====================
    add("luotong", "骆统", "力政人臣", "始计篇·智", Country::WU, Gender::MALE, 4, [] {
        return std::vector<SkillPtr>{ std::make_shared<LuoTongQinZhengSkill>() };
    });

    // ===================== 神话再临·阴包 =====================
    add("xuyou", "许攸", "朝秦暮楚", "神话再临·阴", Country::QUN, Gender::MALE, 3, [] {
        return std::vector<SkillPtr>{ std::make_shared<XuYouChengLueSkill>(),
                                      std::make_shared<XuYouShiCaiSkill>(),
                                      std::make_shared<XuYouCunMuSkill>() };
    });
    return list;
}

} // namespace

const std::vector<HeroInfo>& HeroRegistry::all() {
    static const std::vector<HeroInfo> registry = buildRegistry();
    return registry;
}

const HeroInfo* HeroRegistry::find(const std::string& id) {
    for (const auto& info : all()) {
        if (info.id == id) return &info;
    }
    return nullptr;
}

HeroPtr HeroRegistry::create(const std::string& id) {
    const HeroInfo* info = find(id);
    if (!info) return nullptr;
    return info->create();
}

std::string HeroRegistry::personKey(const std::string& id) {
    if (id == "wolong" || id == "shen_zhugeliang") return "zhugeliang";
    if (id == "yanliang_wenchou") return "yanliang_wenchou";
    if (id == "zhangzhao_zhanghong") return "zhangzhao_zhanghong";
    std::string key = id;
    // 版本前缀统一并回同一人物：界/DIY/神/谋/势/友（用户 2026-10-04 裁定：
    // 谋·与势· 版本同样视为同一人物，不会与标/界版本同场出现）。
    for (const std::string prefix : {"jie_", "diy_", "shen_", "mou_", "shi_", "you_"}) {
        if (key.compare(0, prefix.size(), prefix) == 0) {
            key.erase(0, prefix.size());
            break;
        }
    }
    return key;
}

std::vector<std::string> HeroRegistry::allIds() {
    std::vector<std::string> ids;
    for (const auto& info : all()) ids.push_back(info.id);
    return ids;
}

std::vector<std::string> HeroRegistry::packs() {
    // 包列表显示次序与一级分类下的五个小包次序一致，而非登记代码的插入位置。
    std::vector<std::string> result;
    for (const std::string pack : {"标准包", "风包", "火包", "林包", "山包", "界限突破", "一将成名", "DIY包", "谋攻篇", "势包", "友", "身份武将", "始计篇·智", "神话再临·阴"})
        for (const auto& info : all()) if (info.pack == pack) {
            result.push_back(pack); break;
        }
    for (const auto& info : all()) {
        bool exists = false;
        for (const auto& p : result) if (p == info.pack) { exists = true; break; }
        if (!exists) result.push_back(info.pack);
    }
    return result;
}

std::vector<PackCategory> HeroRegistry::packCategories() {
    return {
        {"标风火林山", {"标准包", "风包", "火包", "林包", "山包"},
         {"标准包", "风包", "火包", "林包", "山包"}},
        {"界限突破", {"界限突破"}, {}},
        {"一将成名", {"一将成名"}, {}},
        {"DIY包", {"DIY包"}, {}},
        {"谋攻篇", {"谋攻篇"}, {}},
        {"势包", {"势包"}, {}},
        {"友", {"友"}, {}},
        {"身份武将", {"身份武将"}, {}},
        {"始计篇", {"始计篇·智"}, {}},
        {"神话再临·阴", {"神话再临·阴"}, {}},
    };
}

std::string HeroRegistry::countryName(Country c) {
    switch (c) {
        case Country::WEI: return "魏";
        case Country::SHU: return "蜀";
        case Country::WU:  return "吴";
        case Country::QUN: return "群";
        case Country::GOD: return "神";
        default:           return "?";
    }
}

} // namespace Thks
