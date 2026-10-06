# 全量官技能逐项核对报告（2026-10-03）—— 结论摘要（已压缩）

> **压缩说明（2026-10-05）**：`logs/` 为滚动日志区（整目录 ≤100KB，测试 `docs/logs_size_within_limit` 校验）。
> 本文件原为逐包逐将逐技能的核对明细（约 27KB），明细的**权威副本长期保存在 `docs/`**：
> `docs/official_skill_audit.md`（附录 A/B 逐字）、`docs/mou_appendix.md`（附录 C，谋攻篇 37 名 81+3 条）、
> `docs/shi_you_appendix.md`（附录 E，势/友/花鬘 30 条）、`docs/full_124_detailed_2026-10-04.md`（124 将明细）。
> 这里只保留仍有价值的**结论**：本轮代码变更、内核扩展（K1~K6）、覆盖关系与来源、仍存披露、合并门槛。

## 0. 本轮变更（结论）

| 变更 | 文件 | 结论 |
|---|---|---|
| hint.md 加入“不计时间花费完全完成任务” | `hint.md` | 标题与正文同步 |
| 主动技内置校验补齐（40 处） | `SkillsMou` 27 / `SkillsExtra` 5 / `SkillsStd` 7 / `Skills` 1 | 所有非转化主动技 `onActivate` 首行 `if (!canActivate(engine,self)) return;`，禁止空发；转化类（武圣/龙胆等）走用牌/响应路径 |
| 【结姻】目标含自己 | `SkillsStd.cpp: JieYinSkill` | 候选由 `getOtherAlivePlayers` 改为 `getAlivePlayers`（官网无“其他”二字 → 含持有者）；取消选择不消耗次数 |
| 分包与选将/图鉴一致性 | `HeroRegistry.cpp` | `友`（友·诸葛亮）与 `身份武将`（花鬘）纳入一级分类，`packCategories` 8→10 项 |
| 测试期望同步 | `test_zhonghui_diy.cpp`、`test_myth.cpp` | 分类数 8→10；124 名总数一致 |

> 当轮 `make test`：**415 测试 / 0 失败 / 5064 断言**。

## 0b. 深度内核扩展与官网裁定根治（K1~K6，结论）

| # | 官网裁定与问题 | 内核扩展 / 修正 |
|---|---|---|
| K1 | 势-克昌是**全局主公技**：群势力角色使用【杀】无距离限制对**所有**群势力角色生效（hero-detail-640）；原实现只对持有者且误判目标势力 | `calculateDistance` 全局遍历（`from` 为群且场上有有效克昌持有者 → `rawDist=1`）；`onCheckShaTarget` 改校验 `self.getCountry()==QUN`；Lv2 保持不可被响应 |
| K2 | 势-迂难“当**你令**一名角色进入濒死…若本轮已有角色死亡”需精确来源 | `processDying` 新增 `濒死来源=sourceId+1` 标记；`loseHp` 扩展 `source` 形参并透传；`applyDamage` 用 `DamageGuard` 精确开关 `inDamage` |
| K3 | 势-肆恣效果 3“每回合结束时若本回合无人用杀则当前回合角色失去 1 体力”原只在最后一回合触发 | 移除 `siZhiTurns==1` 限制，改为每回合判定；`loseHp(..., holder)` 透传来源（迂难可感知） |
| K4 | 势-屯田“失去**非伤害**牌后获得蓄力”原近似为“仅回合外” | 新增 `inDamage` 与通用钩子 `onAnyCardLost`/`notifyAnyCardLost`（discard/loseHandCard/obtain/swap/move/装备替换全路径）；屯田统一走 `gainCharge` |
| K5 | 伤害期统一标识 | `inDamage`（供屯田及后续“非伤害”判定） |
| K6 | 通用“任意失去”钩子 | `Skill::onAnyCardLost` + `GameEngine::notifyAnyCardLost` |

> 本轮后仍 **415/0/5064**；K1~K6 均为官网原文驱动的内核扩展，无简化替代。

---

## 1. 覆盖关系与来源

- **标准包 + 风包黄忠（26名，40技能）**：`docs/official_skill_audit.md` 附录A（标准包 40条逐字）+ 附录B（风火林山/神/界黄忠/钟会/蔡文姬等 95条逐字）。测试 `audit/standard_descriptions_match_official_site_verbatim` 与 `audit/nonstandard_descriptions_match_official_appendix` 逐字比对并做反向覆盖 `audit/all_nonstandard_registry_skills_are_verbatim_covered`。
- **风火林山 32普通 + 8神（40名）**：同上附录B，包归属按官网人物页登记（`HeroRegistry` 风=10/火=10/林=10/山=10，四篇各8普+2神），已校验 `myth/roster_8_plus_2_in_each_pack`。
- **界限突破（6名）+ 一将成名钟会（1名）+ DIY钟会（1名）**：`Skills.h` 按官网文字分版（`WuShengSkill(true/false)`、`PaoXiaoSkill(true/false)`、`QuanJiSkill(true/false)`、`ZiLiSkill(true/false)` 与 DIY `QuanMou/QuanJiConst/ZiLiConst` 独立实现），已校验 `diy/*` 15项与 `official_same_name_skills_share_rules_only_if_official_versions_agree`。
- **谋攻篇 37名（81+3技能）**：`docs/mou_appendix.md` 附录C，37名编号与官网 `hero-detail-{440..652}` 逐页照录；从属技能夺荆/英姿※/英魂※来源为百度百科词条已披露；测试 `test_mou_pack.cpp` 38项（含附录C逐字81+3、注册表、同名对象隔离、渡江觉醒、掠影椎等）+ `test_residuals.cpp` 11项（残留简化补全）。
- **势系·友系·花鬘（12名，30条逐字）**：`docs/shi_you_appendix.md`（花鬘424、友·诸葛亮604、势10名 600/602/610/633/638/640/643/660/662/666），本文件首轮即完成 `extra/appendixE_verbatim_all_30` 与 `extra/registry_all_12_registered` 及每技能行为测试 35项。
- **新增骆统、许攸（2名，4技能）**：`include/SkillsNewHeroes.h`，官网核验：骆统401、许攸254 三项已逐字，称号/体力等未在官网详情页载明的字段依二手资料并披露，测试 `test_new_heroes.cpp` 12项。

> **反向覆盖断言**：`tests/test_official_audit.cpp: audit/all_nonstandard_registry_skills_are_verbatim_covered` 解析三份附录原文并按 `Skill::getName()` 去前缀回查，当前 **253 技能对象全部有逐字来源**（含 DIY 与百度百科披露项）。

---

---

## 9. 仍存披露（诚实标注，非简化替代）

> 依 hint“如实披露与合并门槛”：未逐项官网验收或存在引擎近似的，已在三份审计文档中如实列出，本轮无新增未披露简化。

- **夺荆/英姿※/英魂※**：官网详情页不载原文，来源百度百科词条（`mou_appendix.md` 已标注）。
- **势·瞒天过海牌面**：官网仅列关键卡牌，未给牌面规则；二手资料 `suhu` 转述已披露。
- **势·小乔音洄共享对象**：计数型状态可能串用，待技能克隆机制（`mou_appendix.md` 已披露）。
- **体力/称号/性别**：标准包已全量核对；谋攻篇/势系等其余官网详情页未载体力等字段（已复核五页+列表），按同名惯例并持续披露。
- **复杂交互**：蛊惑质疑、化身时机、乱武/完杀多人濒死等仅单元场景测试，未做对局级验收（`official_skill_audit.md` 已披露）。

> 除上表“仍存披露”外，**无未披露的简化实现**；所有主动技已内置 `canActivate` 守卫，所有“一名角色”类目标已按字面含自己（卡牌自身合法性再过滤），所有“弃置一张牌”类已按手牌+装备区处理。

---

## 10. 合并门槛

> `hint.md` 明确：**只有全部技能按官网验收通过后才能将 PR 合并至 `main`**。本轮已将**全部 253 技能描述**与**全部 124 名登记**按官网附录逐字覆盖（415测试/0失败），并将 40处主动技空发、1处目标自选、2处分包分类不一致等按 hint 与官网修复；剩余披露项均为“官网不载/来源二手/复杂交互待对局级验收”类客观限制，已在对应审计文档中如实列出，不冒称官网规则。DIY 钟会保持独立设计。

---

*生成：2026-10-03，基于 `HeroRegistry::all()` 实时 dump（`/tmp/skills_dump.tsv` 253行）与 `tests/*` 415用例全绿。来源页面：`https://www.sanguosha.cn/pc/hero-detail-{1..666}.html`（详见三份附录编号）。*
