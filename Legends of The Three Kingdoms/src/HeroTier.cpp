#include "HeroTier.h"
#include "HeroRegistry.h"
#include <unordered_map>

namespace Thks {
namespace HeroTier {

namespace {

struct Entry {
    const char* id;
    Tier identity;     // 身份场（军争）
    Tier ddzLandlord;  // 斗地主·地主状态
    Tier ddzFarmer;    // 斗地主·农民状态
    bool banIdentity;  // 至尊场·身份场禁止池（门槛比分档更严）
    bool banDoudizhu;  // 至尊场·斗地主禁止池
    const char* note;
};

// 数据表：与 docs/zhizun_field_rules.md 一一对应（由 tools/gen_hero_tier.py 生成）
const Entry kTable[] = {
    {"guanyu", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 3 星[军争星级]；斗地主地主未入榜、农民及格线是界关羽[DDZ地主][DDZ农民][知乎平民] → 双状态弱"}, // 关羽·标准包
    {"zhangfei", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, false, "军争 2 星[军争星级]；斗地主农民二级“队友就是两张牌”[DDZ农民]（地主未入榜）"}, // 张飞·标准包
    {"zhaoyun", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 2 星“没人理你”[军争星级]；斗地主两榜均未入 → 双状态弱"}, // 赵云·标准包
    {"machao", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 3 星、官方强力指数一星半[军争星级][最弱盘点]；斗地主两榜均未入 → 双状态弱"}, // 马超·标准包
    {"huangzhong", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, false, "军争 7 星“菜刀巅峰之一，烈弓不吃距离且强命”[军争星级]；斗地主地主未入榜、农民无数据（强命可用）"}, // 黄忠·风包
    {"liubei", Tier::MEDIUM, Tier::WEAK, Tier::STRONG, false, false, "军争 5 星[军争星级]；斗地主农民一级（吃队友，二号位最好）[DDZ农民]"}, // 刘备·标准包
    {"zhugeliang", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 5 星[军争星级]；斗地主农民三级“勉强能用”[DDZ农民]、地主未入榜 → 非双状态弱，保留"}, // 诸葛亮·标准包
    {"huangyueying", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, true, false, "军争 4 星[军争星级]；斗地主地主二级（三流）+ 农民二级[DDZ地主][DDZ农民]"}, // 黄月英·标准包
    {"caocao", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, true, "军争 3 星“奸雄已脱离军争节奏”[军争星级]；斗地主地主未入榜、农民无数据"}, // 曹操·标准包
    {"simayi", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星[军争星级]；斗地主农民二级（至少己方防兵乐）[DDZ农民]"}, // 司马懿·标准包
    {"xiahoudun", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 1 星“完全白板”[军争星级]；斗地主两榜均未入 → 双状态弱"}, // 夏侯惇·标准包
    {"zhangliao", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "军争 7 星[军争星级]；斗地主地主二级（勉强）+ 知乎“标准包能用的有张辽貂蝉”[DDZ地主][知乎平民]"}, // 张辽·标准包
    {"xuchu", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, false, "军争 2 星[军争星级]；斗地主农民二级（勉强）[DDZ农民]"}, // 许褚·标准包
    {"guojia", Tier::MEDIUM, Tier::WEAK, Tier::WEAK, true, true, "军争 5 星[军争星级]；斗地主知乎“郭嘉军八也能上场”＝不入流[知乎平民]、地主未入榜 → 双状态弱"}, // 郭嘉·标准包
    {"zhenji", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星[军争星级]；斗地主农民二级[DDZ农民]"}, // 甄姬·标准包
    {"sunquan", Tier::MEDIUM, Tier::MEDIUM, Tier::STRONG, false, false, "军争 5 星[军争星级]；斗地主农民一级 + 地主二级（单挑之王）[DDZ农民][DDZ地主]"}, // 孙权·标准包
    {"ganning", Tier::WEAK, Tier::WEAK, Tier::STRONG, true, false, "军争 3 星[军争星级]（身份弱将）；斗地主农民一级（二号位最好）[DDZ农民]——按“同时移除”仍被移除"}, // 甘宁·标准包
    {"lvmeng", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 1 星“克己是负收益”[军争星级]；斗地主两榜均未入 → 双状态弱"}, // 吕蒙·标准包
    {"huanggai", Tier::WEAK, Tier::MEDIUM, Tier::STRONG, true, false, "军争 2 星[军争星级]（身份弱将）；斗地主农民一级 + 地主二级（强三流）[DDZ农民][DDZ地主]"}, // 黄盖·标准包
    {"zhouyu", Tier::MEDIUM, Tier::WEAK, Tier::STRONG, true, false, "军争 4 星[军争星级]；斗地主农民一级[DDZ农民] 与官方论坛“农民更是弱的没话说”[官方论坛] 冲突 → 保守记中"}, // 周瑜·标准包
    {"daqiao", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, true, "军争 4 星（国色控制）[军争星级]；斗地主无明确评级（控制型农民）"}, // 大乔·标准包
    {"sunshangxiang", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 5 星[军争星级]；斗地主农民二级[DDZ农民]"}, // 孙尚香·标准包
    {"luxun", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 3 星[军争星级]；斗地主两榜均未入（“张春华＝大陆逊”为四级调侃）[DDZ农民] → 双状态弱"}, // 陆逊·标准包
    {"huatuo", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星（嘲讽高）[军争星级]；斗地主农民二级[DDZ农民]"}, // 华佗·标准包
    {"lvbu", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 2 星“伪强命”[军争星级]；标吕布斗地主两榜均未入（界吕布才是地主一级）[DDZ地主] → 双状态弱"}, // 吕布·标准包
    {"diaochan", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星[军争星级]；斗地主知乎“标准包能用的有张辽貂蝉”[知乎平民]"}, // 貂蝉·标准包
    {"jie_guanyu", Tier::MEDIUM, Tier::WEAK, Tier::STRONG, false, false, "身份场稳定菜刀；斗地主公认“农民及格线”[知乎平民][农民及格线]"}, // 界关羽·界限突破
    {"jie_zhangfei", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "咆哮无次数限制，身份场可用；斗地主地主未入榜、农民无数据"}, // 界张飞·界限突破
    {"jie_zhaoyun", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "身份场中庸；斗地主地主未入榜、农民无数据"}, // 界赵云·界限突破
    {"jie_machao", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "身份场盾将/克星定位[新浪博客]；斗地主地主未入榜、农民无数据"}, // 界马超·界限突破
    {"jie_huangzhong", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "身份场强命；斗地主地主二流“怕被控血、断杀”[DDZ地主]"}, // 界黄忠·界限突破
    {"jie_zhonghui", Tier::STRONG, Tier::STRONG, Tier::MEDIUM, false, false, "身份场盾将觉醒；斗地主地主“强一流，硬挨了十五分钟的打”[DDZ地主]"}, // 界钟会·界限突破
    {"zhonghui", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "身份场盾将/权计体系强；斗地主属“节奏较慢的地主”[163庞羲文]"}, // 钟会·一将成名
    {"diy_zhonghui", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "项目 DIY 武将，无官方数据，三档均记中（不参与移除）"}, // DIY钟会·DIY包
    {"xiahouyuan", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, false, "军争 3 星“位置尴尬”[军争星级]；斗地主农民二级（三号位有奇效）[DDZ农民]"}, // 夏侯渊·风包
    {"caoren", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, true, "军争 8 星（据守/解围控场）[军争星级]；斗地主无榜单数据"}, // 曹仁·风包
    {"weiyan", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星（狂骨+奇谋）[军争星级]；地主天花板是势魏延，风魏延不同档[2026解析]"}, // 魏延·风包
    {"xiaoqiao", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, true, "军争 4 星（不动白）[军争星级]；斗地主天香可转移伤害"}, // 小乔·风包
    {"zhoutai", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, true, "军争 7 星（辅助神将）[军争星级]；斗地主无榜单数据"}, // 周泰·风包
    {"zhangjiao", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星[军争星级]；斗地主农民二级（都来杀我，勉强）[DDZ农民]"}, // 张角·风包
    {"yuji", Tier::MEDIUM, Tier::WEAK, Tier::WEAK, true, true, "军争 5 星（上限高）[军争星级]；官方强力指数一星半、斗地主极不稳定[最弱盘点][BWIKI] → 双状态弱"}, // 于吉·风包
    {"shen_guanyu", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 1 星“基本不能看”[军争星级]；神将榜 T3“难评”[神将榜] → 双状态弱"}, // 神关羽·风包
    {"shen_lvmeng", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "军争 6 星[军争星级]；斗地主地主一级/二级 + 知乎“五人标准之神”[DDZ地主][知乎平民]"}, // 神吕蒙·风包
    {"dianwei", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星[军争星级]；斗地主农民二级（来对爆吧）[DDZ农民]"}, // 典韦·火包
    {"xunyu", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, false, "军争 8 星（卖血流巅峰）[军争星级]；斗地主农民二级 + 知乎“火包荀彧可以用”[DDZ农民][知乎平民]"}, // 荀彧·火包
    {"wolong", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 5 星[军争星级]；斗地主农民二级（三花色火攻不中）[DDZ农民]"}, // 卧龙诸葛亮·火包
    {"pangtong", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 5 星（连环+涅槃）[军争星级]；斗地主无榜单数据"}, // 庞统·火包
    {"taishici", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 6 星[军争星级]；斗地主农民二级[DDZ农民]（神太史慈才是农民前五[神将榜]）"}, // 太史慈·火包
    {"pangde", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 6 星[军争星级]；斗地主农民二级（不断杀就是一级）[DDZ农民]"}, // 庞德·火包
    {"yanliang_wenchou", Tier::WEAK, Tier::MEDIUM, Tier::MEDIUM, true, false, "军争 3 星“脱离节奏”[军争星级]（身份弱将）；斗地主地主二级（勉强）+ 农民二级"}, // 颜良文丑·火包
    {"yuanshao", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, false, "军争 7 星（乱击可怕，资料称其在至尊场常被禁）[军争星级]；斗地主农民二级[DDZ农民]"}, // 袁绍·火包
    {"shen_zhouyu", Tier::MEDIUM, Tier::WEAK, Tier::WEAK, true, true, "军争 5 星（忠臣位可用）[军争星级]；神将榜 T3“自爆卡车”、地主未入榜[神将榜] → 双状态弱"}, // 神周瑜·火包
    {"shen_zhugeliang", Tier::STRONG, Tier::STRONG, Tier::MEDIUM, false, false, "军争 8 星（七星/大雾）[军争星级]；斗地主地主一级（强二流）[DDZ地主]"}, // 神诸葛亮·火包
    {"caopi", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, true, "军争 7 星（控制流巅峰）[军争星级]；斗地主知乎“林包曹丕和鲁肃都还可以”[知乎平民]"}, // 曹丕·林包
    {"xuhuang", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 6 星[军争星级]；斗地主断粮压制地主，知乎列界徐晃可用[知乎平民]"}, // 徐晃·林包
    {"menghuo", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, true, "军争 2 星“还不如老婆”[军争星级]（身份弱将）；斗地主界孟获“玩起来还行”[知乎平民]"}, // 孟获·林包
    {"zhurong", Tier::WEAK, Tier::WEAK, Tier::WEAK, true, true, "军争 3 星“非常一般的菜刀”[军争星级]；标祝融斗地主两榜未入、界祝融仅弱二流[DDZ地主] → 双状态弱"}, // 祝融·林包
    {"sunjian", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, true, "军争 3 星“敌方很可能不管孙坚”[军争星级]（身份弱将）；斗地主英魂可给队友牌"}, // 孙坚·林包
    {"lusu", Tier::STRONG, Tier::WEAK, Tier::MEDIUM, false, true, "军争 8 星（好施+缔盟无出其右）[军争星级]；斗地主三人局缔盟收益受限[知乎平民]"}, // 鲁肃·林包
    {"dongzhuo", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星（常备主公）[军争星级]；梯度榜列董卓 T1 主公[梯度榜]；斗地主崩坏拖节奏"}, // 董卓·林包
    {"jiaxu", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星（完杀/乱武）[军争星级]；斗地主无榜单数据"}, // 贾诩·林包
    {"shen_caocao", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "军争 10 星（最强神武将）[军争星级]；斗地主地主榜未列，归心在 1v2 收益高 → 记中"}, // 神曹操·林包
    {"shen_lvbu", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "军争 7 星[军争星级]；斗地主地主二级[DDZ地主]"}, // 神吕布·林包
    {"zhanghe", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星（巧变灵活）[军争星级]；斗地主无榜单数据"}, // 张郃·山包
    {"dengai", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 5 星（节奏慢）[军争星级]；斗地主知乎列界山包邓艾可用[知乎平民]"}, // 邓艾·山包
    {"jiangwei", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, false, "军争 5 星[军争星级]；斗地主知乎列界山包姜维可用[知乎平民]"}, // 姜维·山包
    {"liuchan", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星（防速推主公）[军争星级]；斗地主知乎列界山包刘禅可用[知乎平民]"}, // 刘禅·山包
    {"sunce", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星（很强主公但优缺点明显）[军争星级]；斗地主觉醒偏慢"}, // 孙策·山包
    {"zhangzhao_zhanghong", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "军争 6 星（直谏/固政）[军争星级]；斗地主可辅助队友装备"}, // 张昭张纮·山包
    {"zuoci", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, true, false, "军争 4 星（强度不稳定）[军争星级]；斗地主地主篇称界左慈“？流、但是快乐”[DDZ地主]"}, // 左慈·山包
    {"caiwenji", Tier::WEAK, Tier::WEAK, Tier::MEDIUM, true, false, "军争 3 星（缺牌、断肠易被利用）[军争星级]（身份弱将）；斗地主农民篇“曾经的神级农民”二级[DDZ农民]"}, // 蔡文姬·山包
    {"shen_zhaoyun", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "军争 7 星[军争星级]；神将榜 T1 / 综合榜 T2“环境常青树”，龙魂转桃酒[神将榜][最强排行]"}, // 神赵云·山包
    {"shen_simayi", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "改版后军八大杀四方、神将榜 T0[神将榜]；斗地主地主二级（弱二流）[DDZ地主]"}, // 神司马懿·山包
    {"mou_liucheng", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·刘赪·谋攻篇
    {"mou_lvmeng", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·吕蒙·谋攻篇
    {"mou_huangzhong", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·黄忠·谋攻篇
    {"mou_huaxiong", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·华雄·谋攻篇
    {"mou_yangwan", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·杨婉·谋攻篇
    {"mou_machao", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·马超·谋攻篇
    {"mou_zhangfei", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·张飞·谋攻篇
    {"mou_zhaoyun", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·赵云·谋攻篇
    {"mou_sunshangxiang", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·孙尚香·谋攻篇
    {"mou_xiahoushi", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·夏侯氏·谋攻篇
    {"mou_zhouyu", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "“谋周瑜强度倒是不错…发育后的强度非常高”[年度总结]；斗地主无公开数据"}, // 谋·周瑜·谋攻篇
    {"mou_diaochan", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·貂蝉·谋攻篇
    {"mou_yuanshao", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "主公技【血裔】+【乱击】群体输出；斗地主无公开数据"}, // 谋·袁绍·谋攻篇
    {"mou_pangtong", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·庞统·谋攻篇
    {"mou_liubei", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "主公技【激将】体系，身份场主公位强；斗地主无公开数据"}, // 谋·刘备·谋攻篇
    {"mou_jiangwei", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·姜维·谋攻篇
    {"mou_fazheng", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·法正·谋攻篇
    {"mou_chengong", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·陈宫·谋攻篇
    {"mou_ganning", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·甘宁·谋攻篇
    {"mou_huanggai", Tier::STRONG, Tier::MEDIUM, Tier::STRONG, false, false, "综合榜 T1“谋黄盖”、AK 流爆发上限高[最强排行][最强英雄排行]"}, // 谋·黄盖·谋攻篇
    {"mou_sunquan", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·孙权·谋攻篇
    {"mou_daqiao", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·大乔·谋攻篇
    {"mou_menghuo", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·孟获·谋攻篇
    {"mou_sunce", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "【魂姿】觉醒后强度直线上升；斗地主无公开数据"}, // 谋·孙策·谋攻篇
    {"mou_zhurong", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·祝融·谋攻篇
    {"mou_luzhi", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·卢植·谋攻篇
    {"mou_zhugeliang", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "观星/空城体系在新版收益更高；斗地主无公开数据"}, // 谋·诸葛亮·谋攻篇
    {"mou_guanyu", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·关羽·谋攻篇
    {"mou_huangyueying", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·黄月英·谋攻篇
    {"mou_xiaoqiao", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·小乔·谋攻篇
    {"mou_gongsunzan", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·公孙瓒·谋攻篇
    {"mou_handang", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·韩当·谋攻篇
    {"mou_luxun", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·陆逊·谋攻篇
    {"mou_jiaxu", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·贾诩·谋攻篇
    {"mou_zhugejin", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·诸葛瑾·谋攻篇
    {"mou_lvbu", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·吕布·谋攻篇
    {"mou_zhuran", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"}, // 谋·朱然·谋攻篇
    {"huaman", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 花鬘·身份武将
    {"you_zhugeliang", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 友·诸葛亮·友
    {"shi_taishici", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·太史慈·势包
    {"shi_dongzhao", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·董昭·势包
    {"shi_yuji", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·于吉·势包
    {"shi_xinxianying", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·辛宪英·势包
    {"shi_lusu", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "好施/缔盟新版收益高；斗地主三人局缔盟受限"}, // 势·鲁肃·势包
    {"shi_zhonghui", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "BWIKI 评“迂难和克昌是十分臃肿的添头技”，定位辅助[BWIKI]"}, // 势·钟会·势包
    {"shi_dengai", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·邓艾·势包
    {"shi_suncun", Tier::STRONG, Tier::MEDIUM, Tier::MEDIUM, false, false, "【戮连】群体输出+乘势，身份场强；斗地主无公开数据"}, // 势·孙綝·势包
    {"shi_zhouyu", Tier::STRONG, Tier::STRONG, Tier::MEDIUM, false, false, "“极致输出顶阴武将，先手秒三率接近 100%”[官方论坛][233]"}, // 势·周瑜·势包
    {"shi_xiaoqiao", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·小乔·势包
    {"shi_huangzu", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·黄祖·势包
    {"shi_tianfeng", Tier::MEDIUM, Tier::MEDIUM, Tier::MEDIUM, false, false, "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"}, // 势·田丰·势包
    {"luotong", Tier::MEDIUM, Tier::WEAK, Tier::MEDIUM, false, true, "斗地主**地主**胜率榜倒数第四[月度总结]（更正上一轮误记为农民榜）；农民无数据 → 非双状态弱，保留"}, // 骆统·始计篇·智
    {"xuyou", Tier::MEDIUM, Tier::STRONG, Tier::MEDIUM, false, false, "斗地主“阴雷包一级：许攸（一流地主，攻守兼备，注意控顶）”[DDZ地主]"}, // 许攸·神话再临·阴
};

const std::unordered_map<std::string, const Entry*>& index() {
    static std::unordered_map<std::string, const Entry*> idx = [] {
        std::unordered_map<std::string, const Entry*> m;
        for (const auto& e : kTable) m[e.id] = &e;
        return m;
    }();
    return idx;
}

} // namespace

Tier tierOf(const std::string& heroId, Field field) {
    auto it = index().find(heroId);
    if (it == index().end()) return Tier::MEDIUM;
    switch (field) {
        case Field::IDENTITY:     return it->second->identity;
        case Field::DDZ_LANDLORD: return it->second->ddzLandlord;
        default:                  return it->second->ddzFarmer;
    }
}

int powerScore(const std::string& heroId, Field field) {
    switch (tierOf(heroId, field)) {
        case Tier::STRONG: return 3;
        case Tier::WEAK:   return 1;
        default:           return 2;
    }
}

// 用户裁定：斗地主弱将＝双状态弱将（地主档弱 且 农民档弱）
bool isDoudizhuWeak(const std::string& heroId) {
    return tierOf(heroId, Field::DDZ_LANDLORD) == Tier::WEAK &&
           tierOf(heroId, Field::DDZ_FARMER) == Tier::WEAK;
}

// 至尊场移除门槛（用户 2026-10-06 抬高门槛并按场次分池；见 HeroTier.h 文件头注释）
bool isBannedInZhizun(const std::string& heroId, Mode mode) {
    auto it = index().find(heroId);
    if (it == index().end()) return false;
    return mode == Mode::IDENTITY ? it->second->banIdentity : it->second->banDoudizhu;
}

std::vector<std::string> weakHeroes(Field field) {
    std::vector<std::string> out;
    for (const auto& e : kTable) {
        Tier t = (field == Field::IDENTITY) ? e.identity
                 : (field == Field::DDZ_LANDLORD ? e.ddzLandlord : e.ddzFarmer);
        if (t == Tier::WEAK) out.push_back(e.id);
    }
    return out;
}

std::vector<std::string> weakDoudizhuHeroes() {
    std::vector<std::string> out;
    for (const auto& e : kTable)
        if (e.ddzLandlord == Tier::WEAK && e.ddzFarmer == Tier::WEAK) out.push_back(e.id);
    return out;
}

std::vector<std::string> bannedInZhizun(Mode mode) {
    std::vector<std::string> out;
    for (const auto& id : HeroRegistry::allIds())
        if (isBannedInZhizun(id, mode)) out.push_back(id);
    return out;
}

size_t tableSize() { return sizeof(kTable) / sizeof(kTable[0]); }

std::string rationale(const std::string& heroId) {
    auto it = index().find(heroId);
    return it == index().end() ? std::string() : std::string(it->second->note);
}

std::string tierName(Tier t) {
    switch (t) {
        case Tier::STRONG: return "强";
        case Tier::WEAK:   return "弱";
        default:           return "中";
    }
}

std::string fieldName(Field f) {
    switch (f) {
        case Field::IDENTITY:     return "身份场（军争）";
        case Field::DDZ_LANDLORD: return "斗地主·地主";
        default:                  return "斗地主·农民";
    }
}

std::string modeName(Mode m) {
    return m == Mode::IDENTITY ? "身份场禁止池" : "斗地主禁止池";
}

} // namespace HeroTier
} // namespace Thks
