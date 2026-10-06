#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""实现级静态审计（2026-10-06 实现级复核）。

用途：把 Skill 描述（= 官网申明文本）与 src/ 实现做**启发式对照**，把所有可疑点列出来
供人工复核；它不能替代测试，也不作为“已核对”的证据，只用来缩小人工深查范围。

检查项：
  1. tags     描述里的技能类型关键词（锁定技/限定技/觉醒技/主公技/转换技/持恒技）↔ SkillTag
              正式回归用 tests/test_official_audit.cpp 的 audit/tag_keywords_match_description
              （唯一显式例外：谋·马超【谋-马术】，官网漏标，见 docs/mou_appendix.md）。
  2. limits   描述含“限一次/限两次/限定技/每回合限/每轮限”的技能，类内是否真的有限次状态
              （`spent`/`uses`/`hasUsesLeft`/标记/轮次记录等）。
  3. zones    描述里“弃置/选择一张牌”未限定“手牌”时，实现是否只从手牌取牌。
  4. numbers  描述中的数值（摸X张/造成X点/回复X点/失去X点体力/获得X个/弃置X张）与实现调用
              里的常数是否可能不符。
  5. inert    技能类除构造/可发动判定外是否完全没有行为钩子（登记≠实现嫌疑）。

用法：python3 tools/audit_impl.py            # 汇总
      python3 tools/audit_impl.py --all      # 打印每一条可疑明细
"""
import re
import sys
import glob
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_GLOB = os.path.join(ROOT, "src", "*.cpp")
HDR_GLOB = os.path.join(ROOT, "include", "*.h")

TAG_RULES = [
    ("锁定技", "LOCK"), ("限定技", "LIMITED"), ("觉醒技", "AWAKEN"),
    ("主公技", "LORD"), ("转换技", "SWITCH"), ("持恒技", "SUSTAINED"),
]
CN_NUM = {"一": 1, "两": 2, "二": 2, "三": 3, "四": 4, "五": 5,
          "六": 6, "七": 7, "八": 8, "九": 9, "十": 10}
NUM = r"([0-9]+|[一二两三四五六七八九十]{1,3})"
LIMIT_KEY = re.compile(r"限一次|限两次|限定技|每回合限|每轮限|每名角色的出牌阶段限")
LIMIT_STATE = ["spent", "uses", "hasUsesLeft", "ThisTurn", "thisTurn", "roundUsed",
               "lastRound", "已用", "本轮", "重置", "markUsed"]
NUMBER_PATTERNS = [
    (re.compile(r"摸" + NUM + r"张"), "draw", ["drawCards"]),
    (re.compile(r"造成" + NUM + r"点"), "damage", ["applyDamage", "addDamage", "onCalculateShaDamage"]),
    (re.compile(r"回复" + NUM + r"点"), "heal", ["recoverHp"]),
    (re.compile(r"失去" + NUM + r"点体力"), "lose", ["loseHp"]),
    (re.compile(r"(?:获得|得到)" + NUM + r"个"), "mark", ["addMark"]),
    (re.compile(r"弃置" + NUM + r"张"), "discard", ["discardCardOf", "discardCards", "loseHandCard"]),
]
BEHAVIOR = re.compile(
    r"^(on[A-Z]\w*|activate|Activate|convertCard|convertCards|delegate|invoke|setLevel|"
    r"rebuild|refresh|apply|handle\w*|drawsFromBottom|isSpellAllowed|isCardAllowed)$")
SKIP_CLASS = {"MythSkill", "StateSkill", "ActiveSkill", "TriggerSkill", "Skill", "BlankHero"}


def cn2int(text):
    if text.isdigit():
        return int(text)
    if text == "十":
        return 10
    if len(text) == 1:
        return CN_NUM.get(text)
    if text.startswith("十"):
        return 10 + CN_NUM.get(text[1], 0)
    if text.endswith("十"):
        return CN_NUM.get(text[0], 0) * 10
    if "十" in text:
        a, b = text.split("十")
        return CN_NUM.get(a, 0) * 10 + CN_NUM.get(b, 0)
    return None


def load_files():
    files = {}
    for path in glob.glob(SRC_GLOB) + glob.glob(HDR_GLOB):
        with open(path, encoding="utf-8") as fh:
            files[path] = fh.read()
    return files


def build_index(files):
    cls_body, cls_meth = {}, {}
    for path, text in files.items():
        for m in re.finditer(r"\n(class|struct)\s+([A-Za-z_]\w*)\b[^\n;]*\{", text):
            start = m.start()
            i = text.index("{", start)
            depth, j = 0, i
            while j < len(text):
                if text[j] == "{":
                    depth += 1
                elif text[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            cls_body.setdefault(m.group(2), []).append((path, text[start:j + 1]))
        for m in re.finditer(r"(?m)^(?:[A-Za-z_][\w:<>,\s&*]*?)\b([A-Za-z_]\w*)::", text):
            seg = text[m.start():m.start() + 4000]
            brace, semi = seg.find("{"), seg.find(";")
            if brace < 0 or (0 <= semi < brace):
                continue
            depth, j = 0, m.start() + brace
            while j < len(text):
                if text[j] == "{":
                    depth += 1
                elif text[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            cls_meth.setdefault(m.group(1), []).append(text[m.start():j + 1])
    return cls_body, cls_meth


def classes_for(cls_meth, skill):
    literal = '"' + skill + '"'
    out = set()
    for cls, methods in cls_meth.items():
        for body in methods:
            if literal in body[:800]:
                out.add(cls)
                break
    return out


def class_text(cls, cls_body, cls_meth):
    return "\n".join(x for _, x in cls_body.get(cls, [])) + "\n" + "\n".join(cls_meth.get(cls, []))


def main():
    verbose = "--all" in sys.argv
    files = load_files()
    cls_body, cls_meth = build_index(files)

    # 技能清单：tools/dump_skills.cpp 的输出 /tmp/skills_dump.tsv 若存在则优先使用
    entries = []
    dump = "/tmp/skills_dump.tsv"
    if os.path.exists(dump):
        with open(dump, encoding="utf-8") as fh:
            for line in fh:
                parts = line.rstrip("\n").split("\t")
                if len(parts) >= 5:
                    entries.append((parts[0], parts[1], parts[3]))
    if not entries:
        # 回退：用 src 内 "技能名","描述" 的注册表（MythSkill texts 等）
        for path, text in files.items():
            for m in re.finditer(r'\{"([^"\n]{2,20})","([^"\n]{10,})"\}', text):
                entries.append(("", m.group(1), m.group(2)))

    findings = {"tags": [], "limits": [], "zones": [], "numbers": [], "inert": []}

    for hero, skill, desc in entries:
        if not desc:
            continue
        declaring = {}
        for kw, tag in TAG_RULES:
            if kw in desc and ("非" + kw) not in desc:
                declaring[kw] = tag
        cls_list = classes_for(cls_meth, skill)
        for cls in sorted(cls_list):
            text = class_text(cls, cls_body, cls_meth)
            if declaring.get("锁定技") and '"锁定技"' not in text and "LOCK" not in text:
                findings["tags"].append((hero, skill, cls, "文本声明锁定技，实现未见 LOCK"))
            if LIMIT_KEY.search(desc) and not any(k in text for k in LIMIT_STATE):
                findings["limits"].append((hero, skill, cls, "描述限次，类内未见限次状态"))
            if "手牌" not in desc and re.search(r"弃置[^。]{0,6}张?牌|将一张牌", desc):
                if re.search(r"askChooseCard\s*\(([^;]{0,120}?)getHandCards\(\)", text) and \
                        not re.search(r"getHandAndEquipmentCards|getAllCards", text):
                    findings["zones"].append((hero, skill, cls, "文本未限手牌，实现只从手牌取牌"))
            for rx, label, calls in NUMBER_PATTERNS:
                for m in rx.finditer(desc):
                    want = cn2int(m.group(1))
                    if not want:
                        continue
                    lits, hit = set(), False
                    for call in calls:
                        for mm in re.finditer(re.escape(call) + r"\s*\(([^;]{0,200}?)\)", text, re.S):
                            for lit in re.findall(r"\b(\d+)\b", mm.group(1)):
                                lits.add(int(lit))
                                hit = hit or int(lit) == want
                    if lits and not hit:
                        findings["numbers"].append((hero, skill, cls,
                                                    "文本 %s，代码常量 %s" % (m.group(0), sorted(lits))))
    # inert：有构造但没有任何行为钩子的技能类（含头文件内联 override 的类不计）
    for cls, bodies in sorted(cls_body.items()):
        if cls in SKIP_CLASS or not cls.endswith("Skill"):
            continue
        methods = cls_meth.get(cls, [])
        if not methods:
            continue
        names = set()
        for body in methods:
            names |= set(re.findall(r"(?m)^([A-Za-z_]\w*)::", body))
        if cls not in names:
            continue  # 只统计在 .cpp 内有定义的类
        inline = []
        for _, body in bodies:
            inline += re.findall(r"\b([A-Za-z_]\w*)\s*\([^)]*\)\s*(?:const\s*)?override", body)
        behavior = [x for x in (set(m for m in methods if BEHAVIOR.match(m)) | set(inline))
                    if x not in ("canActivate", "aiShouldActivate", "setLevel", "resetTurnState")]
        if not behavior:
            findings["inert"].append(("", cls, "", "除构造/可发动判定外无行为钩子"))

    for key, label in [("tags", "标签与文本"), ("limits", "限次状态"), ("zones", "取牌区域"),
                       ("numbers", "数值常量"), ("inert", "空实现嫌疑")]:
        items = findings[key]
        print("== %s：%d 条 ==" % (label, len(items)))
        if verbose:
            for item in items:
                print("   - %s" % " | ".join(str(x) for x in item if x))
        elif items:
            for item in items[:10]:
                print("   - %s" % " | ".join(str(x) for x in item if x))
    print("提示：本工具仅用于缩小人工深查范围；正式回归为 make test（含 audit/* 不变式测试）。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
