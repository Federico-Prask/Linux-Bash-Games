#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 include/HeroTier.h / src/HeroTier.cpp / docs/zhizun_field_rules.md

数据来源（详见 docs/zhizun_field_rules.md 的“资料来源”一节）：
  [军争星级] 视游手游网《三国杀武将技能大全及武将强度排行2020（上）》——八人身份军争场 1~10 星
  [DDZ地主]  B 站专栏 cv12319817《三国杀斗地主武将强度（地主篇）》
  [DDZ农民]  B 站专栏 cv12305288《三国杀斗地主武将强度（农民篇上）》
  [神将榜]   游侠手游《三国杀神将强度排行》(2025-08)
  [梯度榜]   欢乐三国杀身份场梯度排行（当快软件园 / 多多软件站，2026）
  [2026解析] 818ku《三国杀最强武将全解析（2026 最新版）》
  [知乎平民] 知乎《三国杀（手杀）有哪些平民玩家必须获得的强将呢？》
  [官方论坛] xianhua.sanguosha.cn 帖子（势·周瑜 / 标·周瑜 评述）
  [BWIKI]    biligame 三国杀移动版 WIKI（势钟会页）
  [月度总结] 小米游戏中心《斗地主月度总结》（骆统农民胜率）
  [最弱盘点] 九游《三国杀最弱武将排名》、TapTap《官方钦点最弱武将都有谁》

分档规则（写死在此，代码与文档共用）：
  身份场（军争）：星级 >=7 → 强；4~6 → 中；<=3 → 弱。无星级数据的新包按机制评估。
  斗地主：地主/农民任一榜“一级”或有明确强评 → 强；“二级”/可用 → 中；
          “三级/四级”、未入榜且同包已有明确榜单、或被资料点名弱 → 弱。
  至尊场 = 两个模式的弱将**并集**同时移除（用户 2026-10-05 裁定）。
"""
import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

S, M, W = "STRONG", "MEDIUM", "WEAK"

# ---------------------------------------------------------------------------
# 身份场（军争）分档：八人军争 1~10 星（[军争星级]）
#   >=7 星 → 强；4~6 星 → 中；<=3 星 → 弱（含官方强力指数一星半、资料点名“白板/废物”）
#   无公开数据的新包（谋攻篇/势包/友/花鬘）→ 中（不移除），已在文档披露
# ---------------------------------------------------------------------------
IDENTITY = {
    # 标准包
    "guanyu": W, "zhangfei": W, "zhaoyun": W, "machao": W, "liubei": M, "zhugeliang": M,
    "huangyueying": M, "caocao": W, "simayi": M, "xiahoudun": W, "zhangliao": S, "xuchu": W,
    "guojia": M, "zhenji": M, "sunquan": M, "ganning": W, "lvmeng": W, "huanggai": W,
    "zhouyu": M, "daqiao": M, "sunshangxiang": M, "luxun": W, "huatuo": M, "lvbu": W,
    "diaochan": M,
    # 界限突破 / 一将成名 / DIY
    "jie_guanyu": M, "jie_zhangfei": M, "jie_zhaoyun": M, "jie_machao": M, "jie_huangzhong": M,
    "jie_zhonghui": S, "zhonghui": S, "diy_zhonghui": M,
    # 风包
    "huangzhong": S, "xiahouyuan": W, "caoren": S, "weiyan": M, "xiaoqiao": M, "zhoutai": S,
    "zhangjiao": M, "yuji": M, "shen_guanyu": W, "shen_lvmeng": M,
    # 火包
    "dianwei": M, "xunyu": S, "wolong": M, "pangtong": M, "taishici": M, "pangde": M,
    "yanliang_wenchou": W, "yuanshao": S, "shen_zhouyu": M, "shen_zhugeliang": S,
    # 林包
    "caopi": S, "xuhuang": M, "menghuo": W, "zhurong": W, "sunjian": W, "lusu": S,
    "dongzhuo": M, "jiaxu": M, "shen_caocao": S, "shen_lvbu": S,
    # 山包
    "zhanghe": M, "dengai": M, "jiangwei": M, "liuchan": M, "sunce": M,
    "zhangzhao_zhanghong": M, "zuoci": M, "caiwenji": W, "shen_zhaoyun": S, "shen_simayi": S,
    # 谋攻篇（有评述的按评述，其余中档）
    "mou_zhouyu": S, "mou_huanggai": S, "mou_liubei": S, "mou_yuanshao": S,
    "mou_zhugeliang": S, "mou_sunce": S,
    # 势包 / 友 / 其他
    "shi_zhouyu": S, "shi_suncun": S, "shi_lusu": S, "xuyou": M,
}

# ---------------------------------------------------------------------------
# 斗地主分档（**双状态**：地主档 + 农民档）——用户 2026-10-05 裁定：
#   “斗地主弱将排除标准是**双状态弱将**”，即地主档弱 **且** 农民档弱才算斗地主弱将。
#
# 地主档规则：[DDZ地主] 明确“标风火林山包（没有一级）二级：孙权、黄盖、黄月英、张辽、
#   颜良文丑”，并把界将/阴雷包/神将包可用者逐一点名 → **同包未入榜＝地主档弱**；
#   一级/明确强评＝强；二级三级＝中；无公开榜单的新包（谋攻篇/势包/友/花鬘/DIY）＝中。
# 农民档规则：[DDZ农民] 一级＝强、二级三级＝中；四级或资料点名弱（“鶸/不入流/自爆卡车/
#   一星半/及格线是界将”）＝弱；无数据＝中（不移除）。
# ---------------------------------------------------------------------------

# 地主篇点名可用（标风火林山包二级；界将/阴雷/神将按榜单）
DDZ_LANDLORD_OK = {
    # 标风火林山包二级（没有一级）
    "sunquan": M, "huanggai": M, "huangyueying": M, "zhangliao": M, "yanliang_wenchou": M,
    # 神将包：一级 神诸葛（强二流）/ 神吕蒙（二流）；二级 神司马、神吕布
    "shen_zhugeliang": S, "shen_lvmeng": M, "shen_simayi": M, "shen_lvbu": M,
    # 神曹操/神赵云：地主篇未列，但归心 1v2 收益高、龙魂转桃酒为常青树 → 中（不按未入榜判弱）
    "shen_caocao": M, "shen_zhaoyun": M,
    # 界将：界钟会“强一流，硬挨十五分钟”；界黄忠二级
    "jie_zhonghui": S, "jie_huangzhong": M,
    # 阴雷包一级：许攸“一流地主，攻守兼备，注意控顶”
    "xuyou": S,
    # 一将成名：钟会属“节奏较慢的地主”，可用
    "zhonghui": M,
    # 势包：势·周瑜“极致输出顶阴武将，先手秒三率接近 100%”
    "shi_zhouyu": S,
}
# 地主档弱的显式补充：神将包未入榜且神将榜 T3；骆统出自[月度总结]的**地主胜率榜**倒数第四
DDZ_LANDLORD_WEAK = {"shen_guanyu", "shen_zhouyu", "luotong"}
# 地主档不按“未入榜＝弱”处理的包（缺公开榜单）
DDZ_LANDLORD_NO_DATA_PACKS = {"谋攻篇", "势包", "友", "身份武将", "DIY包", "始计篇·智"}

# 农民篇：一级＝强
DDZ_FARMER_STRONG = {
    "liubei", "sunquan", "huanggai", "ganning", "zhouyu",   # 标风包一级
    "jie_guanyu",                                           # 公认的“农民及格线”
    "mou_huanggai",                                         # 综合榜 T1“谋黄盖”，AK 流爆发
}
# 农民档弱（四级或资料点名弱）
DDZ_FARMER_WEAK = {
    "guanyu",      # 农民及格线是界关羽，标关羽未入榜
    "zhaoyun", "machao", "xiahoudun", "lvmeng", "luxun", "lvbu", "zhurong",  # 军争 1~3 星白板，两榜皆无
    "guojia",      # 知乎“郭嘉袁术军八也能上场”＝斗地主不入流
    "yuji",        # 官方强力指数一星半，斗地主极不稳定
    "shen_guanyu", # 神将榜 T3“难评”
    "shen_zhouyu", # 神将榜 T3“自爆卡车”，琴音伤及队友
}

# 逐条依据（写入代码注释与 docs 表格）
NOTES = {
    "guanyu": "军争 3 星[军争星级]；斗地主地主未入榜、农民及格线是界关羽[DDZ地主][DDZ农民][知乎平民] → 双状态弱",
    "zhangfei": "军争 2 星[军争星级]；斗地主农民二级“队友就是两张牌”[DDZ农民]（地主未入榜）",
    "zhaoyun": "军争 2 星“没人理你”[军争星级]；斗地主两榜均未入 → 双状态弱",
    "machao": "军争 3 星、官方强力指数一星半[军争星级][最弱盘点]；斗地主两榜均未入 → 双状态弱",
    "huangzhong": "军争 7 星“菜刀巅峰之一，烈弓不吃距离且强命”[军争星级]；斗地主地主未入榜、农民无数据（强命可用）",
    "liubei": "军争 5 星[军争星级]；斗地主农民一级（吃队友，二号位最好）[DDZ农民]",
    "zhugeliang": "军争 5 星[军争星级]；斗地主农民三级“勉强能用”[DDZ农民]、地主未入榜 → 非双状态弱，保留",
    "huangyueying": "军争 4 星[军争星级]；斗地主地主二级（三流）+ 农民二级[DDZ地主][DDZ农民]",
    "caocao": "军争 3 星“奸雄已脱离军争节奏”[军争星级]；斗地主地主未入榜、农民无数据",
    "simayi": "军争 4 星[军争星级]；斗地主农民二级（至少己方防兵乐）[DDZ农民]",
    "xiahoudun": "军争 1 星“完全白板”[军争星级]；斗地主两榜均未入 → 双状态弱",
    "zhangliao": "军争 7 星[军争星级]；斗地主地主二级（勉强）+ 知乎“标准包能用的有张辽貂蝉”[DDZ地主][知乎平民]",
    "xuchu": "军争 2 星[军争星级]；斗地主农民二级（勉强）[DDZ农民]",
    "guojia": "军争 5 星[军争星级]；斗地主知乎“郭嘉军八也能上场”＝不入流[知乎平民]、地主未入榜 → 双状态弱",
    "zhenji": "军争 4 星[军争星级]；斗地主农民二级[DDZ农民]",
    "sunquan": "军争 5 星[军争星级]；斗地主农民一级 + 地主二级（单挑之王）[DDZ农民][DDZ地主]",
    "ganning": "军争 3 星[军争星级]（身份弱将）；斗地主农民一级（二号位最好）[DDZ农民]——按“同时移除”仍被移除",
    "lvmeng": "军争 1 星“克己是负收益”[军争星级]；斗地主两榜均未入 → 双状态弱",
    "huanggai": "军争 2 星[军争星级]（身份弱将）；斗地主农民一级 + 地主二级（强三流）[DDZ农民][DDZ地主]",
    "zhouyu": "军争 4 星[军争星级]；斗地主农民一级[DDZ农民] 与官方论坛“农民更是弱的没话说”[官方论坛] 冲突 → 保守记中",
    "daqiao": "军争 4 星（国色控制）[军争星级]；斗地主无明确评级（控制型农民）",
    "sunshangxiang": "军争 5 星[军争星级]；斗地主农民二级[DDZ农民]",
    "luxun": "军争 3 星[军争星级]；斗地主两榜均未入（“张春华＝大陆逊”为四级调侃）[DDZ农民] → 双状态弱",
    "huatuo": "军争 4 星（嘲讽高）[军争星级]；斗地主农民二级[DDZ农民]",
    "lvbu": "军争 2 星“伪强命”[军争星级]；标吕布斗地主两榜均未入（界吕布才是地主一级）[DDZ地主] → 双状态弱",
    "diaochan": "军争 6 星[军争星级]；斗地主知乎“标准包能用的有张辽貂蝉”[知乎平民]",
    "jie_guanyu": "身份场稳定菜刀；斗地主公认“农民及格线”[知乎平民][农民及格线]",
    "jie_zhangfei": "咆哮无次数限制，身份场可用；斗地主地主未入榜、农民无数据",
    "jie_zhaoyun": "身份场中庸；斗地主地主未入榜、农民无数据",
    "jie_machao": "身份场盾将/克星定位[新浪博客]；斗地主地主未入榜、农民无数据",
    "jie_huangzhong": "身份场强命；斗地主地主二流“怕被控血、断杀”[DDZ地主]",
    "jie_zhonghui": "身份场盾将觉醒；斗地主地主“强一流，硬挨了十五分钟的打”[DDZ地主]",
    "zhonghui": "身份场盾将/权计体系强；斗地主属“节奏较慢的地主”[163庞羲文]",
    "diy_zhonghui": "项目 DIY 武将，无官方数据，三档均记中（不参与移除）",
    "xiahouyuan": "军争 3 星“位置尴尬”[军争星级]；斗地主农民二级（三号位有奇效）[DDZ农民]",
    "caoren": "军争 8 星（据守/解围控场）[军争星级]；斗地主无榜单数据",
    "weiyan": "军争 6 星（狂骨+奇谋）[军争星级]；地主天花板是势魏延，风魏延不同档[2026解析]",
    "xiaoqiao": "军争 4 星（不动白）[军争星级]；斗地主天香可转移伤害",
    "zhoutai": "军争 7 星（辅助神将）[军争星级]；斗地主无榜单数据",
    "zhangjiao": "军争 4 星[军争星级]；斗地主农民二级（都来杀我，勉强）[DDZ农民]",
    "yuji": "军争 5 星（上限高）[军争星级]；官方强力指数一星半、斗地主极不稳定[最弱盘点][BWIKI] → 双状态弱",
    "shen_guanyu": "军争 1 星“基本不能看”[军争星级]；神将榜 T3“难评”[神将榜] → 双状态弱",
    "shen_lvmeng": "军争 6 星[军争星级]；斗地主地主一级/二级 + 知乎“五人标准之神”[DDZ地主][知乎平民]",
    "dianwei": "军争 4 星[军争星级]；斗地主农民二级（来对爆吧）[DDZ农民]",
    "xunyu": "军争 8 星（卖血流巅峰）[军争星级]；斗地主农民二级 + 知乎“火包荀彧可以用”[DDZ农民][知乎平民]",
    "wolong": "军争 5 星[军争星级]；斗地主农民二级（三花色火攻不中）[DDZ农民]",
    "pangtong": "军争 5 星（连环+涅槃）[军争星级]；斗地主无榜单数据",
    "taishici": "军争 6 星[军争星级]；斗地主农民二级[DDZ农民]（神太史慈才是农民前五[神将榜]）",
    "pangde": "军争 6 星[军争星级]；斗地主农民二级（不断杀就是一级）[DDZ农民]",
    "yanliang_wenchou": "军争 3 星“脱离节奏”[军争星级]（身份弱将）；斗地主地主二级（勉强）+ 农民二级",
    "yuanshao": "军争 7 星（乱击可怕，资料称其在至尊场常被禁）[军争星级]；斗地主农民二级[DDZ农民]",
    "shen_zhouyu": "军争 5 星（忠臣位可用）[军争星级]；神将榜 T3“自爆卡车”、地主未入榜[神将榜] → 双状态弱",
    "shen_zhugeliang": "军争 8 星（七星/大雾）[军争星级]；斗地主地主一级（强二流）[DDZ地主]",
    "caopi": "军争 7 星（控制流巅峰）[军争星级]；斗地主知乎“林包曹丕和鲁肃都还可以”[知乎平民]",
    "xuhuang": "军争 6 星[军争星级]；斗地主断粮压制地主，知乎列界徐晃可用[知乎平民]",
    "menghuo": "军争 2 星“还不如老婆”[军争星级]（身份弱将）；斗地主界孟获“玩起来还行”[知乎平民]",
    "zhurong": "军争 3 星“非常一般的菜刀”[军争星级]；标祝融斗地主两榜未入、界祝融仅弱二流[DDZ地主] → 双状态弱",
    "sunjian": "军争 3 星“敌方很可能不管孙坚”[军争星级]（身份弱将）；斗地主英魂可给队友牌",
    "lusu": "军争 8 星（好施+缔盟无出其右）[军争星级]；斗地主三人局缔盟收益受限[知乎平民]",
    "dongzhuo": "军争 6 星（常备主公）[军争星级]；梯度榜列董卓 T1 主公[梯度榜]；斗地主崩坏拖节奏",
    "jiaxu": "军争 6 星（完杀/乱武）[军争星级]；斗地主无榜单数据",
    "shen_caocao": "军争 10 星（最强神武将）[军争星级]；斗地主地主榜未列，归心在 1v2 收益高 → 记中",
    "shen_lvbu": "军争 7 星[军争星级]；斗地主地主二级[DDZ地主]",
    "zhanghe": "军争 6 星（巧变灵活）[军争星级]；斗地主无榜单数据",
    "dengai": "军争 5 星（节奏慢）[军争星级]；斗地主知乎列界山包邓艾可用[知乎平民]",
    "jiangwei": "军争 5 星[军争星级]；斗地主知乎列界山包姜维可用[知乎平民]",
    "liuchan": "军争 4 星（防速推主公）[军争星级]；斗地主知乎列界山包刘禅可用[知乎平民]",
    "sunce": "军争 6 星（很强主公但优缺点明显）[军争星级]；斗地主觉醒偏慢",
    "zhangzhao_zhanghong": "军争 6 星（直谏/固政）[军争星级]；斗地主可辅助队友装备",
    "zuoci": "军争 4 星（强度不稳定）[军争星级]；斗地主地主篇称界左慈“？流、但是快乐”[DDZ地主]",
    "caiwenji": "军争 3 星（缺牌、断肠易被利用）[军争星级]（身份弱将）；斗地主农民篇“曾经的神级农民”二级[DDZ农民]",
    "shen_zhaoyun": "军争 7 星[军争星级]；神将榜 T1 / 综合榜 T2“环境常青树”，龙魂转桃酒[神将榜][最强排行]",
    "shen_simayi": "改版后军八大杀四方、神将榜 T0[神将榜]；斗地主地主二级（弱二流）[DDZ地主]",
    "mou_zhouyu": "“谋周瑜强度倒是不错…发育后的强度非常高”[年度总结]；斗地主无公开数据",
    "mou_huanggai": "综合榜 T1“谋黄盖”、AK 流爆发上限高[最强排行][最强英雄排行]",
    "mou_liubei": "主公技【激将】体系，身份场主公位强；斗地主无公开数据",
    "mou_yuanshao": "主公技【血裔】+【乱击】群体输出；斗地主无公开数据",
    "mou_zhugeliang": "观星/空城体系在新版收益更高；斗地主无公开数据",
    "mou_sunce": "【魂姿】觉醒后强度直线上升；斗地主无公开数据",
    "shi_zhouyu": "“极致输出顶阴武将，先手秒三率接近 100%”[官方论坛][233]",
    "shi_suncun": "【戮连】群体输出+乘势，身份场强；斗地主无公开数据",
    "shi_lusu": "好施/缔盟新版收益高；斗地主三人局缔盟受限",
    "shi_zhonghui": "BWIKI 评“迂难和克昌是十分臃肿的添头技”，定位辅助[BWIKI]",
    "xuyou": "斗地主“阴雷包一级：许攸（一流地主，攻守兼备，注意控顶）”[DDZ地主]",
    "luotong": "斗地主**地主**胜率榜倒数第四[月度总结]（更正上一轮误记为农民榜）；农民无数据 → 非双状态弱，保留",
}

DEFAULT_NOTE_MOU = "谋攻篇新包，无公开分模式统计；按机制评估为中档保留（不移除）"
DEFAULT_NOTE_NEW = "新包（势/友/身份武将等）无公开分模式统计；按机制评估为中档保留（不移除）"
DEFAULT_NOTE_OLD = "斗地主无该状态的公开榜单数据，按机制记中档（不因缺数据移除）"


def ddz_landlord(hero_id, pack):
    """地主档：一级/强评＝强，二三級＝中，标风火林山与神将包未入榜＝弱，缺榜单的新包＝中"""
    if hero_id in DDZ_LANDLORD_WEAK:
        return W
    if hero_id in DDZ_LANDLORD_OK:
        return DDZ_LANDLORD_OK[hero_id]
    if pack in DDZ_LANDLORD_NO_DATA_PACKS:
        return M
    if pack in ("标准包", "风包", "火包", "林包", "山包", "界限突破", "一将成名", "神话再临·阴"):
        return W  # [DDZ地主] 已把可用者逐一点名，同包未入榜即弱
    return M


def ddz_farmer(hero_id):
    if hero_id in DDZ_FARMER_WEAK:
        return W
    if hero_id in DDZ_FARMER_STRONG:
        return S
    return M


# ---------------------------------------------------------------------------
# 至尊场移除门槛（用户 2026-10-06：「弱将门槛可以高一点，估计每个模式都各禁 30 个以上，
# 两个模式禁止池可以不一样」）——**比上面的强度分档更严**：
#   身份场：军争星级 ≤4 星，或 5 星但资料有明确负面评述
#           （白板/不动白/脱离节奏/不入流/一星半/自爆/节奏慢/不稳定/缺点明显/嘲讽高/
#             位置尴尬/极不稳定/伪强命/难评/尴尬/添头）。
#   斗地主：地主档弱（该包已点名可用者而未入榜）**且**农民档没有“明确可用”评级
#           （农民一/二/三级、及格线、可用、有奇效、防兵乐、神级农民等）。
#           比旧的“双状态弱将”更严：农民档**无数据**且地主档弱也算弱。
#   无公开数据的新包（谋攻篇/势包/友/身份武将/DIY/始计篇·智）地主档记中 → 不参与移除。
# ---------------------------------------------------------------------------
IDENTITY_BAN_NEGATIVE = re.compile(
    r"白板|不动白|脱离|不入流|一星半|自爆|节奏慢|不稳定|缺点明显|嘲讽高|位置尴尬|极不稳定|伪强命|难评|尴尬|添头")
FARMER_CLEARLY_USABLE = re.compile(
    r"农民(一|二|三)级|及格线|可用|有奇效|防兵乐|队友就是两张牌|神级农民|稳定")
STAR_PATTERN = re.compile(r"军争\s*([0-9]+)\s*星")


def identity_banned(hero_id):
    m = STAR_PATTERN.search(NOTES.get(hero_id, ""))
    if not m:
        return False
    stars = int(m.group(1))
    if stars <= 4:
        return True
    return stars == 5 and bool(IDENTITY_BAN_NEGATIVE.search(NOTES[hero_id]))


def ddz_banned(hero_id, pack):
    if ddz_landlord(hero_id, pack) != W:
        return False
    if ddz_farmer(hero_id) == W:
        return True
    return not FARMER_CLEARLY_USABLE.search(NOTES.get(hero_id, ""))


def identity_tier(hero_id):
    return IDENTITY.get(hero_id, M)


def note_for(hero_id, pack):
    if hero_id in NOTES:
        return NOTES[hero_id]
    if pack == "谋攻篇":
        return DEFAULT_NOTE_MOU
    if pack in ("势包", "友", "身份武将", "DIY包", "始计篇·智"):
        return DEFAULT_NOTE_NEW
    return DEFAULT_NOTE_OLD


def load_heroes():
    """读取武将登记表（id/名称/包）。TSV 缺失时用 tools/dump_heroes.cpp 现编译现生成。"""
    rows = []
    tsv = os.environ.get("THKS_HERO_TSV", "/tmp/heroes.tsv")
    if not os.path.exists(tsv):
        import glob, subprocess, tempfile
        objs = sorted(glob.glob(os.path.join(ROOT, "src/*.o")))
        if not objs:
            raise SystemExit("缺少 %s 且未找到 src/*.o（请先 make）" % tsv)
        exe = os.path.join(tempfile.gettempdir(), "dump_heroes")
        subprocess.check_call(["g++", "-std=c++17", "-I" + os.path.join(ROOT, "include"),
                               os.path.join(ROOT, "tools/dump_heroes.cpp")] + objs + ["-o", exe])
        with io.open(tsv, "w", encoding="utf-8") as out:
            subprocess.check_call([exe], stdout=out)
    with io.open(tsv, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 3:
                rows.append((parts[0], parts[1], parts[2]))
    return rows


def tier_cn(t):
    return {"STRONG": "强", "MEDIUM": "中", "WEAK": "弱"}[t]


HEADER = """#ifndef THKS_HERO_TIER_H
#define THKS_HERO_TIER_H

#include <string>
#include <vector>

namespace Thks {

// =====================================================================
//  武将强度分档与「至尊场」弱将移除表
//
//  用户裁定：
//    2026-10-05 ① 至尊场＝**按场次移除弱将**；
//               ② 斗地主弱将的**分档**口径是“双状态弱将”（地主档弱 且 农民档弱）。
//    2026-10-06 ③ 「弱将门槛可以高一点，估计每个模式都各禁 30 个以上，
//                   将（种）分武将两个模式禁止池可以不一样，玩家……点将可以用全将」：
//                 → **身份场与斗地主各有自己的禁止池**（不再互相牵连）；
//                 → 门槛抬高：身份池 = 军争 ≤4 星，或 5 星且资料有明确负面评述；
//                    斗地主池 = 地主档弱（未入榜）且农民档无“明确可用”评级。
//                 → 随机候选/换将（含玩家）都从去弱将的池里抽；**点将（显式指定武将）
//                    可以用全将**（见 GameEngine 的 initGame/initDoudizhuGame 入口）。
//
//  分档依据（全部为公开资料，逐条见 docs/zhizun_field_rules.md）：
//    身份场：八人军争 1~10 星评级，>=7 强 / 4~6 中 / <=3 弱；
//    斗地主·地主：地主篇榜单，一级或明确强评＝强，二级三级＝中，
//                 同包已点名可用者而未入榜＝弱（该篇明确“标风火林山没有一级”）；
//    斗地主·农民：农民篇榜单，一级或明确强评（如“农民及格线”界关羽）＝强，
//                 二级三级＝中，四级或资料点名弱＝弱；
//    无公开数据的新包（谋攻篇/势包/友/花鬘/DIY）一律中档保留（不移除），已在文档披露。
//
//  本文件由 tools/gen_hero_tier.py 生成，请勿手工编辑数据表。
// =====================================================================
namespace HeroTier {

// 场次/状态（决定用哪一档数据）
enum class Field { IDENTITY, DDZ_LANDLORD, DDZ_FARMER };

// 至尊场的场次（决定用哪一个禁止池；两个模式互不相同）
enum class Mode { IDENTITY, DOUDIZHU };

// 强度档：强 / 中 / 弱
enum class Tier { STRONG, MEDIUM, WEAK };

// 某武将在某场次/状态下的强度档；未登记的武将按 MEDIUM 处理
Tier tierOf(const std::string& heroId, Field field);

// 选将 AI 用的数值分：强 3 / 中 2 / 弱 1
int powerScore(const std::string& heroId, Field field);

// 斗地主**分档口径**的双状态弱将（地主档弱 且 农民档弱；文档/统计用）
bool isDoudizhuWeak(const std::string& heroId);

// 至尊场移除门槛（比强度分档更严，见文件头）：身份池 / 斗地主池
bool isBannedInZhizun(const std::string& heroId, Mode mode);

// 指定场次的移除名单（按武将登记顺序）
std::vector<std::string> bannedInZhizun(Mode mode);

// 某场次/状态下的弱将名单（文档与测试用）
std::vector<std::string> weakHeroes(Field field);

// 斗地主双状态弱将名单（分档口径）
std::vector<std::string> weakDoudizhuHeroes();

// 表中登记条目数（覆盖自检：应等于武将登记总数）
size_t tableSize();

// 该武将的分档依据（文档同步用；未登记返回空串）
std::string rationale(const std::string& heroId);

std::string tierName(Tier t);
std::string fieldName(Field f);
std::string modeName(Mode m);

} // namespace HeroTier

} // namespace Thks

#endif // THKS_HERO_TIER_H
"""


def main():
    heroes = load_heroes()
    ids = [h[0] for h in heroes]
    extra = [k for k in set(list(IDENTITY) + list(DDZ_LANDLORD_OK) + list(DDZ_LANDLORD_WEAK) +
                            list(DDZ_FARMER_STRONG) + list(DDZ_FARMER_WEAK) + list(NOTES))
             if k not in ids]
    if extra:
        print("表中存在未登记武将:", extra, file=sys.stderr)
        return 1

    rows = []
    for hid, name, pack in heroes:
        it = identity_tier(hid)
        lt = ddz_landlord(hid, pack)
        ft = ddz_farmer(hid)
        rows.append((hid, name, pack, it, lt, ft, note_for(hid, pack),
                     identity_banned(hid), ddz_banned(hid, pack)))

    ban_identity = [r for r in rows if r[7]]
    ban_ddz = [r for r in rows if r[8]]
    id_weak = [r for r in rows if r[3] == W]
    ddz_weak = [r for r in rows if r[4] == W and r[5] == W]

    # ---------------- include/HeroTier.h ----------------
    with io.open(os.path.join(ROOT, "include/HeroTier.h"), "w", encoding="utf-8") as f:
        f.write(HEADER)

    # ---------------- src/HeroTier.cpp ----------------
    L = []
    L.append('#include "HeroTier.h"')
    L.append('#include "HeroRegistry.h"')
    L.append('#include <unordered_map>')
    L.append('')
    L.append('namespace Thks {')
    L.append('namespace HeroTier {')
    L.append('')
    L.append('namespace {')
    L.append('')
    L.append('struct Entry {')
    L.append('    const char* id;')
    L.append('    Tier identity;     // 身份场（军争）')
    L.append('    Tier ddzLandlord;  // 斗地主·地主状态')
    L.append('    Tier ddzFarmer;    // 斗地主·农民状态')
    L.append('    bool banIdentity;  // 至尊场·身份场禁止池（门槛比分档更严）')
    L.append('    bool banDoudizhu;  // 至尊场·斗地主禁止池')
    L.append('    const char* note;')
    L.append('};')
    L.append('')
    L.append('// 数据表：与 docs/zhizun_field_rules.md 一一对应（由 tools/gen_hero_tier.py 生成）')
    L.append('const Entry kTable[] = {')
    for hid, name, pack, it, lt, ft, note, bi, bd in rows:
        L.append('    {"%s", Tier::%s, Tier::%s, Tier::%s, %s, %s, "%s"}, // %s·%s'
                 % (hid, it, lt, ft, 'true' if bi else 'false', 'true' if bd else 'false',
                    note.replace('"', '\\"'), name, pack))
    L.append('};')
    L.append('')
    L.append('const std::unordered_map<std::string, const Entry*>& index() {')
    L.append('    static std::unordered_map<std::string, const Entry*> idx = [] {')
    L.append('        std::unordered_map<std::string, const Entry*> m;')
    L.append('        for (const auto& e : kTable) m[e.id] = &e;')
    L.append('        return m;')
    L.append('    }();')
    L.append('    return idx;')
    L.append('}')
    L.append('')
    L.append('} // namespace')
    L.append('')
    L.append('Tier tierOf(const std::string& heroId, Field field) {')
    L.append('    auto it = index().find(heroId);')
    L.append('    if (it == index().end()) return Tier::MEDIUM;')
    L.append('    switch (field) {')
    L.append('        case Field::IDENTITY:     return it->second->identity;')
    L.append('        case Field::DDZ_LANDLORD: return it->second->ddzLandlord;')
    L.append('        default:                  return it->second->ddzFarmer;')
    L.append('    }')
    L.append('}')
    L.append('')
    L.append('int powerScore(const std::string& heroId, Field field) {')
    L.append('    switch (tierOf(heroId, field)) {')
    L.append('        case Tier::STRONG: return 3;')
    L.append('        case Tier::WEAK:   return 1;')
    L.append('        default:           return 2;')
    L.append('    }')
    L.append('}')
    L.append('')
    L.append('// 用户裁定：斗地主弱将＝双状态弱将（地主档弱 且 农民档弱）')
    L.append('bool isDoudizhuWeak(const std::string& heroId) {')
    L.append('    return tierOf(heroId, Field::DDZ_LANDLORD) == Tier::WEAK &&')
    L.append('           tierOf(heroId, Field::DDZ_FARMER) == Tier::WEAK;')
    L.append('}')
    L.append('')
    L.append('// 至尊场移除门槛（用户 2026-10-06 抬高门槛并按场次分池；见 HeroTier.h 文件头注释）')
    L.append('bool isBannedInZhizun(const std::string& heroId, Mode mode) {')
    L.append('    auto it = index().find(heroId);')
    L.append('    if (it == index().end()) return false;')
    L.append('    return mode == Mode::IDENTITY ? it->second->banIdentity : it->second->banDoudizhu;')
    L.append('}')
    L.append('')
    L.append('std::vector<std::string> weakHeroes(Field field) {')
    L.append('    std::vector<std::string> out;')
    L.append('    for (const auto& e : kTable) {')
    L.append('        Tier t = (field == Field::IDENTITY) ? e.identity')
    L.append('                 : (field == Field::DDZ_LANDLORD ? e.ddzLandlord : e.ddzFarmer);')
    L.append('        if (t == Tier::WEAK) out.push_back(e.id);')
    L.append('    }')
    L.append('    return out;')
    L.append('}')
    L.append('')
    L.append('std::vector<std::string> weakDoudizhuHeroes() {')
    L.append('    std::vector<std::string> out;')
    L.append('    for (const auto& e : kTable)')
    L.append('        if (e.ddzLandlord == Tier::WEAK && e.ddzFarmer == Tier::WEAK) out.push_back(e.id);')
    L.append('    return out;')
    L.append('}')
    L.append('')
    L.append('std::vector<std::string> bannedInZhizun(Mode mode) {')
    L.append('    std::vector<std::string> out;')
    L.append('    for (const auto& id : HeroRegistry::allIds())')
    L.append('        if (isBannedInZhizun(id, mode)) out.push_back(id);')
    L.append('    return out;')
    L.append('}')
    L.append('')
    L.append('size_t tableSize() { return sizeof(kTable) / sizeof(kTable[0]); }')
    L.append('')
    L.append('std::string rationale(const std::string& heroId) {')
    L.append('    auto it = index().find(heroId);')
    L.append('    return it == index().end() ? std::string() : std::string(it->second->note);')
    L.append('}')
    L.append('')
    L.append('std::string tierName(Tier t) {')
    L.append('    switch (t) {')
    L.append('        case Tier::STRONG: return "强";')
    L.append('        case Tier::WEAK:   return "弱";')
    L.append('        default:           return "中";')
    L.append('    }')
    L.append('}')
    L.append('')
    L.append('std::string fieldName(Field f) {')
    L.append('    switch (f) {')
    L.append('        case Field::IDENTITY:     return "身份场（军争）";')
    L.append('        case Field::DDZ_LANDLORD: return "斗地主·地主";')
    L.append('        default:                  return "斗地主·农民";')
    L.append('    }')
    L.append('}')
    L.append('')
    L.append('std::string modeName(Mode m) {')
    L.append('    return m == Mode::IDENTITY ? "身份场禁止池" : "斗地主禁止池";')
    L.append('}')
    L.append('')
    L.append('} // namespace HeroTier')
    L.append('} // namespace Thks')
    L.append('')
    with io.open(os.path.join(ROOT, "src/HeroTier.cpp"), "w", encoding="utf-8") as f:
        f.write("\n".join(L))

    # ---------------- docs/zhizun_field_rules.md ----------------
    D = []
    D.append("# 至尊场规则与武将强度分档表")
    D.append("")
    D.append("> 本文件与 `include/HeroTier.h`、`src/HeroTier.cpp` 由 `tools/gen_hero_tier.py` 同步生成；")
    D.append("> 修改分档请改脚本后重跑（`python3 tools/gen_hero_tier.py`），避免文档与实现脱节。")
    D.append("")
    D.append("## 一、至尊场是什么（本项目定义）")
    D.append("")
    D.append("用户裁定：")
    D.append("")
    D.append("- （2026-10-05）「**至尊场。就是每个模式同时移除弱将**（可以在网上查找资料分辨哪些是身份、哪些是斗地主强）」")
    D.append("- （2026-10-05）「特别的，**斗地主弱将排除标准是双状态弱将**」")
    D.append("- （2026-10-06）「**弱将门槛可以高一点，估计每个模式都各禁 30 个以上**，将（种）分武将两个模式禁止池可以不一样，玩家虽也不用禁止将池，但**点将可以用全将**」")
    D.append("")
    D.append("落地规则（2026-10-06 修订）：")
    D.append("")
    D.append("1. **身份场与斗地主各有独立的禁止池**（不再互相牵连）：身份场用身份池，斗地主用斗地主池。")
    D.append("2. 门槛比“强度分档”更严，两个池都保证 **30 名以上**：")
    D.append("   - 身份池：军争星级 **≤4 星**，或 5 星但资料有明确负面评述（白板/脱离节奏/不入流/自爆/难评…）。")
    D.append("   - 斗地主池：**地主档弱**（该包已点名可用者而未入榜）**且农民档没有“明确可用”评级**")
    D.append("     （农民一/二/三级、及格线、可用、有奇效、防兵乐、神级农民等）。")
    D.append("3. 禁止池只作用于**随机池**：随机分配、身份场选将框、斗地主亮将/换将候选（含玩家）都不出现禁将；")
    D.append("   **点将（显式指定武将）可以用全将**——弱将入场时日志会注明“点将可用”。")
    D.append("4. 普通场（默认）不做任何移除。")
    D.append("")
    D.append("> 说明：官方《三国杀移动版》的“至尊场”实为高门槛房间（1V1 至尊 / 国战至尊 / 8 人至尊，")
    D.append("> 等级与胜率门槛，武将范围为标准、风林火山、界限突破、SP、一将成名），并不移除弱将")
    D.append("> （来源：九游《至尊场》词条 https://www.9game.cn/sgs/1909743.html ）。")
    D.append("> 本项目按用户裁定实现为“移除弱将”的场次开关，与官方房间门槛不是一回事，特此披露。")
    D.append("")
    D.append("## 二、分档规则与移除门槛")
    D.append("")
    D.append("强度分档（`HeroTier::tierOf`，用于资料展示与选将 AI 评分）：")
    D.append("")
    D.append("| 场次/状态 | 强 | 中 | 弱 |")
    D.append("| --- | --- | --- | --- |")
    D.append("| 身份场（军争） | 八人军争星级 ≥7 | 4~6 星 | ≤3 星（含官方强力指数一星半、资料点名“白板/废物”） |")
    D.append("| 斗地主·地主 | 地主篇“一级”或明确强评 | “二级/三级”、或缺榜单但机制可用 | 同包已点名可用者而**未入榜**、或被资料点名弱 |")
    D.append("| 斗地主·农民 | 农民篇“一级”或明确强评（如“农民及格线”界关羽） | “二级/三级”、或无数据 | “四级”或被资料点名弱（鶸/不入流/自爆卡车/一星半） |")
    D.append("")
    D.append("至尊场**移除门槛**（`HeroTier::isBannedInZhizun(id, mode)`，比上表更严）：")
    D.append("")
    D.append("| 禁止池 | 判据 | 本表命中 |")
    D.append("| --- | --- | --- |")
    D.append("| 身份场 | 星级 ≤4，或 5 星且资料有明确负面评述 | **%d 名** |" % len(ban_identity))
    D.append("| 斗地主 | 地主档弱 且 农民档无明确可用评级（含农民无数据） | **%d 名** |" % len(ban_ddz))
    D.append("")
    D.append("**没有公开数据的武将一律记“中”并保留**（谋攻篇 37 名、势包 10 名、友·诸葛亮、花鬘、DIY 钟会）；")
    D.append("“地主档未入榜＝弱”只用于地主篇**已逐一点名可用者**的包（标风火林山、界限突破、一将成名、阴雷、神将）。")
    D.append("")
    D.append("## 三、至尊场两张移除名单（身份 %d 名 / 斗地主 %d 名）" % (len(ban_identity), len(ban_ddz)))
    D.append("")
    D.append("- 身份场禁止池 %d 名：%s" % (len(ban_identity), "、".join(r[1] for r in ban_identity)))
    D.append("- 斗地主禁止池 %d 名：%s" % (len(ban_ddz), "、".join(r[1] for r in ban_ddz)))
    D.append("- 两池并集 %d 名，剩余（至少有一个模式可用）%d / %d 名。"
             % (len(set(r[0] for r in ban_identity) | set(r[0] for r in ban_ddz)),
                len(rows) - len(set(r[0] for r in ban_identity) | set(r[0] for r in ban_ddz)), len(rows)))
    D.append("- 对照：**分档口径**的身份弱将 %d 名、斗地主双状态弱将 %d 名（分档≠移除门槛）。"
             % (len(id_weak), len(ddz_weak)))
    D.append("")
    D.append("| 武将 | 包 | 身份场 | 地主 | 农民 | 身份池 | 斗地主池 | 依据 |")
    D.append("| --- | --- | --- | --- | --- | --- | --- | --- |")
    for hid, name, pack, it, lt, ft, note, bi, bd in rows:
        if not (bi or bd):
            continue
        D.append("| %s | %s | %s | %s | %s | %s | %s | %s |"
                 % (name, pack, tier_cn(it), tier_cn(lt), tier_cn(ft),
                    "**移除**" if bi else "保留", "**移除**" if bd else "保留", note))
    D.append("")
    D.append("## 四、全武将分档总表（%d 名）" % len(rows))
    D.append("")
    D.append("| 武将 | 包 | 身份场 | 斗地主·地主 | 斗地主·农民 | 身份禁止池 | 斗地主禁止池 | 依据 |")
    D.append("| --- | --- | --- | --- | --- | --- | --- | --- |")
    for hid, name, pack, it, lt, ft, note, bi, bd in rows:
        D.append("| %s | %s | %s | %s | %s | %s | %s | %s |"
                 % (name, pack, tier_cn(it), tier_cn(lt), tier_cn(ft),
                    "**移除**" if bi else "保留", "**移除**" if bd else "保留", note))
    D.append("")
    D.append("## 五、资料来源与已知缺口")
    D.append("")
    D.append("资料（检索日 2026-10-05）：")
    D.append("")
    D.append("- [军争星级] 视游手游网《三国杀武将技能大全及武将强度排行2020（上）》（更新 2024-09-21）")
    D.append("  https://www.shiyouhome.com/sgs/gonglue/3596.html ——八人身份军争场 1~10 星，覆盖标准/风/火/林/山/神。")
    D.append("- [DDZ地主] B 站专栏《三国杀斗地主武将强度（地主篇）》 https://www.bilibili.com/read/cv12319817")
    D.append("- [DDZ农民] B 站专栏《三国杀斗地主武将强度（农民篇上）》 https://www.bilibili.com/read/cv12305288")
    D.append("- [神将榜] 游侠手游《三国杀神将强度排行》（2025-08-14） https://m.ali213.net/news/gl2508/1686219.html")
    D.append("- [梯度榜] 欢乐三国杀身份场梯度排行（主公/忠臣/反贼/内奸 T0~T2） https://www.downkuai.com/android/160487.html")
    D.append("- [2026解析] 《三国杀最强武将全解析（2026 最新版）》 https://www.818ku.com/yxzx/yxgl/2386.html")
    D.append("- [最强排行] 第一手游网《三国杀武将强度排行》（2025-04） https://www.diyiyou.com/sgs/gl/384973.html")
    D.append("- [最强英雄排行] 起点《三国杀界最强英雄排行图表》（T1“谋黄盖”） https://m.qidian.com/ask/qnjgtnjurtu")
    D.append("- [知乎平民] 知乎《三国杀（手杀）有哪些平民玩家必须获得的强将呢？》 https://www.zhihu.com/question/512989560")
    D.append("- [农民及格线] 小米游戏中心/TapTap《斗地主哪些武将强度可以当农民，参考这两个农民及格线》（界关羽、马良）")
    D.append("  https://game.xiaomi.com/viewpoint/1195705742_1667098659258_16")
    D.append("- [官方论坛] 三国杀移动版官方社区帖（势·周瑜强度、标·周瑜斗地主评述） https://xianhua.sanguosha.cn/postDetails?id=12482001")
    D.append("- [BWIKI] biligame《三国杀移动版 WIKI·势钟会》 https://wiki.biligame.com/msgs/势钟会")
    D.append("- [月度总结] 小米游戏中心《三国杀：斗地主月度总结》（**地主**胜率榜倒数第四为骆统）")
    D.append("  https://game.xiaomi.com/viewpoint/1195705742_1667899856133_16")
    D.append("- [最弱盘点] 九游《三国杀最弱武将排名》 https://www.9game.cn/sgs/11609787.html ；")
    D.append("  TapTap《官方钦点最弱武将都有谁》 https://www.taptap.cn/moment/425733019395952462")
    D.append("- [年度总结] 游侠手游《三国杀移动版2023年度总结》（谋·周瑜评述） https://m.ali213.net/news/240104/207869.html")
    D.append("- [163庞羲文] 网易《手杀新武将庞羲来袭》（界钟会“节奏较慢的地主”） https://m.163.com/dy/article/KDUV7AHS0546I6UX.html")
    D.append("- [233] 233 乐园《势周瑜强度怎么样》 https://www.233leyuan.com/post-detail/2073999297883119616")
    D.append("- [新浪博客] 《三国杀：玩斗地主胜率高的武将有哪些？》（界马超等盾将/克星） https://blog.sina.com.cn/s/blog_198fc1b800102y3ji.html")
    D.append("- [至尊场词条] 九游《至尊场》 https://www.9game.cn/sgs/1909743.html")
    D.append("")
    D.append("已知缺口与更正（如实披露）：")
    D.append("")
    D.append("1. **谋攻篇 37 名 / 势包 10 名 / 友·诸葛亮 / 花鬘 / DIY 钟会 缺公开分模式统计**：三档一律记“中”保留，")
    D.append("   因此至尊场移除的几乎全是老包武将。拿到官方 6 项评分后应整表回填本脚本。")
    D.append("2. **官方游戏内 6 项评分（主公/忠臣/反贼/内奸/地主/农民）未取到数值**：贴吧《477 名武将强度评分汇总表》")
    D.append("   （https://tieba.baidu.com/p/9069404088 ）为图片表，沙箱无法抓取。")
    D.append("3. **更正上一轮记录**：骆统的“胜率倒数第四”出自[月度总结]的**地主胜率榜**（上一轮误记为农民榜）。")
    D.append("   按 2026-10-06 抬高的门槛：骆统地主弱且农民无数据 → **斗地主池移除**；标诸葛亮农民三级“勉强能用” → 两池都保留（身份 5 星无负面评述）。")
    D.append("4. **资料年份跨度大**（2020~2026），武将改版频繁；冲突处（如标周瑜斗地主农民一级 vs 官方论坛“农民很弱”）")
    D.append("   取保守值“中”，不移除。")
    D.append("5. 本表是**强度分档**，不是官网原文，不参与“官网逐字比对”验收；改动只需同步脚本与文档，不影响技能实现。")
    D.append("")
    with io.open(os.path.join(ROOT, "docs/zhizun_field_rules.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(D))

    print("武将 %d 名 | 身份禁止池 %d 名 | 斗地主禁止池 %d 名 | 并集 %d | 分档：身份弱 %d / 斗地主双状态弱 %d"
          % (len(rows), len(ban_identity), len(ban_ddz),
             len(set(r[0] for r in ban_identity) | set(r[0] for r in ban_ddz)),
             len(id_weak), len(ddz_weak)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
