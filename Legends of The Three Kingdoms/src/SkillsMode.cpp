// SkillsMode.cpp —— 玩法专属技能（斗地主 + 身份场新机制：内奸择途 / 主公立储）
// 文本：移动版官网口径（用户提供，2026-10-04），见 docs/doudizhu_rules.md。
#include "SkillsMode.h"
#include "GameEngine.h"
#include "Roles.h"
#include "Player.h"
#include "Card.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Thks {

namespace {
PlayerPtr selfOf(GameEngine& engine, Player& self) { return engine.getPlayerById(self.getId()); }
// 设置标记到指定值（Player 仅有增减接口）。
void setMark(Player& p, const std::string& key, int value) {
    int cur = p.getMark(key);
    if (cur != value) p.addMark(key, value - cur);
}
bool inDoudizhu(GameEngine& engine) { return engine.getGameMode() == GameEngine::GameMode::DOUDIZHU; }
// 【飞扬】【跋扈】的生效范围：斗地主（地主）**或**身份场【择途·自立为主】后的野心家。
bool landlordStyleActive(GameEngine& engine, const Player& self) {
    return inDoudizhu(engine) || self.getIdentity() == Identity::YE_XIN_JIA;
}
} // namespace

// ==================== 地主：飞扬 ====================

DouFeiYangSkill::DouFeiYangSkill()
    : TriggerSkill("飞扬",
                   "判定阶段，可弃2张牌，移除判定区的一张牌（如【乐不思蜀】），每回合限1次。") {}

void DouFeiYangSkill::onTurnStart(GameEngine&, Player& self) {
    setMark(self, "飞扬已用", 0); // 每回合限 1 次
}

void DouFeiYangSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::JUDGEMENT) return;
    if (!landlordStyleActive(engine, self) || !engine.isPlayerTurn(self)) return;
    if (self.getMark("飞扬已用") > 0) return;      // 每回合限 1 次
    if (self.getJudgeZone().empty()) return;        // 判定区没有牌则无需发动
    std::vector<CardPtr> pool = self.getHandCards();
    for (const auto& c : self.getAllEquipment()) pool.push_back(c);
    if (pool.size() < 2) return;                    // 不足以弃 2 张牌

    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【飞扬】判定阶段：弃 2 张牌并移除判定区的一张牌？", true)) return;
    setMark(self, "飞扬已用", 1);

    // 1) 弃 2 张牌（手牌/装备区；模拟官方“弃2张牌”）
    std::vector<CardPtr> remaining = pool;
    for (int i = 0; i < 2; ++i) {
        if (remaining.empty()) break;
        CardPtr fallback = remaining.front();
        CardPtr pick = engine.askChooseCard(me, remaining, "【飞扬】弃置第" + std::to_string(i + 1) + "张牌",
                                            false, fallback);
        if (!pick) break;
        engine.discardCardOf(me, pick, "飞扬");
        remaining.erase(std::remove(remaining.begin(), remaining.end(), pick), remaining.end());
    }
    // 2) 移除自己判定区的一张牌（进入弃牌堆）
    const std::vector<CardPtr>& judge = self.getJudgeZone();
    if (!judge.empty()) {
        CardPtr jc = engine.askChooseCard(me, judge, "【飞扬】移除判定区的一张牌", false, judge.front());
        if (jc) engine.discardCardOf(me, jc, "飞扬");
        engine.logMessage("  【飞扬】" + self.getName() + " 弃 2 张牌并移除了判定区的一张牌。");
    }
}

// ==================== 地主：跋扈（锁定技） ====================

DouBaHuSkill::DouBaHuSkill()
    : TriggerSkill("跋扈", "锁定技，准备阶段摸1牌，出牌阶段可多使用1张【杀】。", SkillTag::LOCK) {}

void DouBaHuSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION) return;
    if (!landlordStyleActive(engine, self) || !engine.isPlayerTurn(self)) return;
    PlayerPtr me = selfOf(engine, self);
    engine.drawCards(me, 1, "跋扈");
    engine.logMessage("  【跋扈】锁定技，" + self.getName() + " 准备阶段摸 1 张牌。");
}

void DouBaHuSkill::onCalculateShaLimit(GameEngine& engine, const Player& self, int& shaLimit) {
    if (!landlordStyleActive(engine, self)) return;
    if (engine.getCurrentPhase() != TurnPhase::PLAY) return;
    if (!engine.isPlayerTurn(self)) return;
    shaLimit += 1; // 出牌阶段可多使用 1 张【杀】
}

// ==================== 农民：共苦 ====================

DouGongKuSkill::DouGongKuSkill()
    : TriggerSkill("共苦", "当队友阵亡时，存活的农民可选择回复1点体力或摸2张牌。") {}

void DouGongKuSkill::onPlayerDeath(GameEngine& engine, Player& self, Player& dead, Player*) {
    if (!inDoudizhu(engine)) return;
    if (!self.isAlive()) return;                       // 只有存活的农民可以响应
    if (self.getIdentity() != Identity::NONG_MIN) return;
    if (dead.getIdentity() != Identity::NONG_MIN) return; // 队友（另一名农民）阵亡
    if (dead.getId() == self.getId()) return;
    if (self.getMark("共苦已用") > 0) return;           // 队友仅一名，理论至多一次

    PlayerPtr me = selfOf(engine, self);
    if (!engine.askConfirm(me, "【共苦】队友阵亡：回复 1 点体力或摸 2 张牌？", true)) return;
    setMark(self, "共苦已用", 1);
    // AI 默认项：受伤时回血，否则摸 2 张牌（手牌收益更高）
    int choice = engine.askChooseOption(me, {"回复1点体力", "摸2张牌"}, "【共苦】选择一项",
                                        self.isWounded() ? 0 : 1);
    if (choice == 0) {
        engine.recoverHp(me, 1, "共苦");
        engine.logMessage("  【共苦】" + self.getName() + " 选择回复 1 点体力。");
    } else {
        engine.drawCards(me, 2, "共苦");
        engine.logMessage("  【共苦】" + self.getName() + " 选择摸 2 张牌。");
    }
}

// ==================== 分发 ====================

// ==================== 身份场：内奸【择途】 ====================
//
// 用户提供的规则原文（2026-10-05）：
//   “当场上存活角色大于4人时，内奸可以在任意时刻发动一次‘择途’，在以下两种道路中做出选择：
//    1. 侍奉明君（转变为忠臣）——身份牌立即变为忠臣，胜利条件同步变更为主忠方的胜利条件；
//    2. 自立为主（转变为野心家）——成为独立的‘野心家’，并获得〖飞扬〗和〖跋扈〗（源自地主模式）；
//       即使主公死亡也不会立即失败，可以继续游戏，目标是击败场上所有其他角色，成为唯一的幸存者。”
// 工程落实（已在 docs/identity_field_rules.md 披露）：“自己的回合内任意时刻”＝①回合开始
// ②自己每个阶段开始（AI 静默评估；人类在弃牌阶段被再问一次）③出牌阶段主动技菜单
// ④自己回合内有角色阵亡后；一局仅一次，发动时明置身份牌。

ZheTuSkill::ZheTuSkill()
    : ActiveSkill("择途",
                  "当场上存活角色大于4人时，你可以在任意时刻发动一次，选择一项：1.侍奉明君——你的身份牌立即变为“忠臣”，"
                  "胜利条件同步变更为主忠方的胜利条件；2.自立为主——你成为独立的“野心家”，获得“飞扬”和“跋扈”；"
                  "即使主公死亡，你也不会立即失败，你可以继续游戏，目标是击败场上所有其他角色，成为唯一的幸存者。",
                  1) {}

bool ZheTuSkill::canActivate(GameEngine& engine, Player& self) {
    if (!ActiveSkill::canActivate(engine, self)) return false;              // 存活 + 本回合次数
    if (engine.getGameMode() != GameEngine::GameMode::JUNZHENG) return false; // 仅身份场
    if (self.getIdentity() != Identity::NEI_JIAN) return false;             // 仅内奸
    if (self.getMark("择途已用") > 0) return false;                          // 一局仅一次
    return static_cast<int>(engine.getAlivePlayers().size()) > 4;           // 存活角色大于 4 人
}

// AI 决策（可在单测中复现，逐条对应下面的注释）：
//   0＝侍奉明君（洗白成忠臣，随主忠方获胜）；1＝自立为主（转野心家，独自清场）；-1＝暂不发动。
// 思路：内奸要独胜本就得清场，而野心家清场更强（多【飞扬】【跋扈】、主公阵亡也不判负），
// 所以“主忠方要输”时一定自立为主；只有“主忠方大概率赢”时洗白才是捷径。
int ZheTuSkill::aiChoice(GameEngine& engine, Player& self) {
    const auto& ps = engine.getPlayers();
    PlayerPtr lord;
    for (const auto& p : ps) if (p->isAlive() && p->getIdentity() == Identity::ZHU_GONG) lord = p;
    // 用户 2026-10-05：“AI 需分辨队友等”——按**公开构成 + 公开身份 + 行为证据**估计双方人数，
    // 不读真实身份；未被识别的人按剩余构成比例分摊（身份构成开局即公示）。
    int loyal = 0, rebels = 0;
    engine.aiEstimateSides(self, loyal, rebels);
    int alive = static_cast<int>(engine.getAlivePlayers().size());
    int total = static_cast<int>(ps.size());

    int decision = -2;                   // -2 表示尚未判定（仅用于下面的可选跟踪输出）
    auto trace = [&](int result) {
        // 可选决策跟踪（docs/ai_todo.md F1）：THKS_AI_TRACE=1 时把评估输入与结论打到 stderr，
        // 便于调参；不影响游戏输出与测试断言。
        static const bool on = std::getenv("THKS_AI_TRACE") != nullptr;
        if (!on) return;
        std::fprintf(stderr, "[择途AI] %s alive=%d total=%d loyal=%d rebels=%d lordHp=%d selfHp=%d -> %s\n",
                     self.getName().c_str(), alive, total, loyal, rebels,
                     lord ? lord->getHp() : -1, self.getHp(),
                     result == 0 ? "侍奉明君" : (result == 1 ? "自立为主" : "不发动"));
    };
    (void)decision;
    if (!lord) { trace(1); return 1; }   // 主公已阵亡：内奸身份已无胜利路径，只能自立为主
    if (self.getHp() <= 1) { trace(1); return 1; } // 自己将死：靠【跋扈】额外过牌与【飞扬】解判定挣扎
    // D7 深化：手牌与装备（能打持久战的本钱）、场上是否已有储君（主忠方继承体系是否搭好）
    const int myAssets = self.getHandCardCount() + static_cast<int>(self.getAllEquipment().size());
    bool heirExists = false;
    for (const auto& p : ps) if (p->isAlive() && p->getMark("储君") > 0) { heirExists = true; break; }
    // 反贼已全灭而主公在世：洗白即刻达成“反贼与内奸全灭且主公存活”→ 主忠方（含自己）获胜；
    // 继续当内奸/野心家则要独自清场，收益远低 → 一律侍奉明君。
    if (rebels == 0) { trace(0); return 0; }
    // 主公将死而反贼尚在：洗白会随主忠方一起输 → 先自立为主（主公阵亡也不判负）。
    if (lord->getHp() <= 1 && rebels > 0) { trace(1); return 1; }
    // 主忠方人数不劣于反贼且主公还站得住（体力 >= 2）→ 洗白是捷径（团队获胜远比独自清场容易）。
    // 已有储君时更该洗白：主忠方的继承体系已经搭好，跟着走胜率更高。
    if (lord->getHp() >= 2 && loyal >= rebels) { trace(0); return 0; }
    if (heirExists && loyal >= rebels && lord->getHp() >= 1) { trace(0); return 0; }
    // 自己本钱厚（手牌+装备 >= 8）而主公已经残血 → 转野心家打持久战更划算
    if (myAssets >= 8 && lord->getHp() <= 2) { trace(1); return 1; }
    // 窗口判断：存活必须 >4，所以“剩 5 人”是最后机会；5 人局开局即是最后机会。
    if (alive == 5 || total <= 5) {
        int r = (loyal >= rebels) ? 0 : 1; // 主忠不劣→洗白；反贼压场→自立为主等他们互耗
        trace(r);
        return r;
    }

    if (self.getHp() <= 2 && loyal >= rebels) { trace(0); return 0; } // 自己残血而主忠不劣：抱团队
    trace(-1);
    return -1;                         // 局势未明：保留选择权（不发动）
}

bool ZheTuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return aiChoice(engine, self) >= 0;
}

// 统一询问入口：确认后立即发动（activate 内部会重新校验 canActivate，禁止空发）。
void ZheTuSkill::offer(GameEngine& engine, Player& self, const std::string& when) {
    if (!canActivate(engine, self)) return;
    int want = aiChoice(engine, self);
    PlayerPtr me = selfOf(engine, self);
    if (me->isAI()) {
        if (want < 0) return; // AI：局势未明则保留选择权（静默，不打扰日志）
        activate(engine, self);
        return;
    }
    if (!engine.askConfirm(me, "【择途】" + when + "：存活角色大于 4 人，是否发动【择途】并明置身份牌？", false)) return;
    activate(engine, self);
}

void ZheTuSkill::onTurnStart(GameEngine& engine, Player& self) {
    // 用户 2026-10-05 细化：“你可以在**自己的回合内**任意时刻主动发动” → 不再在他人回合询问。
    if (!engine.isPlayerTurn(self)) return;
    offer(engine, self, "你的回合开始");
}

void ZheTuSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    // “回合内任意时刻”的落实：自己回合的每个阶段开始都评估一次。
    if (!engine.isPlayerTurn(self)) return;
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfOf(engine, self);
    if (me->isAI()) {
        if (aiChoice(engine, self) >= 0) activate(engine, self);
        return;
    }
    // 人类：回合开始已问过，出牌阶段有主动技菜单；这里只在**弃牌阶段**（回合内最后机会）再问一次。
    if (phase == TurnPhase::DISCARD) offer(engine, self, "你的弃牌阶段（本回合最后机会）");
}

void ZheTuSkill::onPlayerDeath(GameEngine& engine, Player& self, Player&, Player*) {
    // 自己回合内有角色阵亡（存活人数刚变化）时也可立刻表态；他人回合不再打断。
    if (!engine.isPlayerTurn(self)) return;
    offer(engine, self, "你的回合内有角色阵亡");
}

void ZheTuSkill::onActivate(GameEngine& engine, Player& self) {
    PlayerPtr me = selfOf(engine, self);
    // 操作方式（用户原文）：发动时需要**明置自己的身份牌**，并立即选择道路。
    setMark(self, "身份已明置", 1);
    engine.logMessage("  【择途】" + self.getName() + " 明置身份牌：【内奸】（存活 " +
                      std::to_string(engine.getAlivePlayers().size()) + " 人 > 4）。");
    int want = aiChoice(engine, self);
    int choice = engine.askChooseOption(
        me, {"侍奉明主（身份牌直接变为忠臣，随主忠方获胜）",
             "自立为主（成为野心家，获得【飞扬】【跋扈】，主公阵亡也不立即失败）"},
        "【择途】选择一条道路", want >= 0 ? want : 0);
    setMark(self, "择途已用", 1);
    if (choice == 0) {
        me->setIdentity(Identity::ZHONG_CHEN);
        engine.logMessage("  【择途】" + self.getName() + " 选择「侍奉明主」：身份牌直接变为【忠臣】，"
                          "胜利条件同步变更为主忠方。");
    } else {
        me->setIdentity(Identity::YE_XIN_JIA);
        applyAmbitionSkills(engine, *me);
        engine.logMessage("  【择途】" + self.getName() + " 选择「自立为主」：成为独立的【野心家】，获得【飞扬】【跋扈】（地主版同名技能）；"
                          "即使主公阵亡也不会立即失败，目标是击败所有其他角色、成为唯一幸存者。");
    }
}

// ==================== 身份场：主公【立储】 ====================
//
// 用户提供的规则原文（2026-10-05）：
//   “主公可以在第一轮的任意角色结束阶段，明选一名角色为‘储君’（太子）。
//    如果主公阵亡，储君若为忠臣，将继位成为新的主公，延续主忠方的游戏。”

LiChuSkill::LiChuSkill()
    : TriggerSkill("立储",
                   "第一轮的任意角色结束阶段，你可以明选一名角色为“储君”（太子）。当你阵亡时，若储君为忠臣，"
                   "其继位成为新的主公（延续主忠方的游戏）。") {}

// AI 选储君（用户 2026-10-05：“AI 需分辨队友等”）：**不读真实身份**，
// 只看公开证据（`aiSideEstimate`：谁替主公挡过牌/打过反贼、谁是已明置的忠臣、谁被反贼集火）。
// 没有偏向主忠方的证据时不立储——立错人（非忠臣）无法继位，还会公开暴露主公的判断。
PlayerPtr LiChuSkill::aiChooseHeir(GameEngine& engine, Player& self) {
    PlayerPtr best;
    int bestSide = 0;
    for (const auto& p : engine.getAlivePlayers()) {
        if (p->getId() == self.getId()) continue;
        int side = engine.aiSideEstimate(*p);
        if (side <= 0) continue;                    // 无证据偏向主忠方 → 不立
        if (!best || side > bestSide ||
            (side == bestSide && (p->getHp() > best->getHp() ||
                                  (p->getHp() == best->getHp() &&
                                   p->getHandCardCount() > best->getHandCardCount())))) {
            best = p;
            bestSide = side;
        }
    }
    return best;
}

void LiChuSkill::onAnyPhaseEnd(GameEngine& engine, Player& self, Player&, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    if (engine.getGameMode() != GameEngine::GameMode::JUNZHENG) return;
    if (self.getIdentity() != Identity::ZHU_GONG || !self.isAlive()) return;
    if (engine.getCurrentRound() != 1) return;      // 仅第一轮
    if (self.getMark("立储已用") > 0) return;       // 只立一次
    PlayerPtr me = selfOf(engine, self);
    PlayerPtr aiHeir = aiChooseHeir(engine, self);
    // AI：没有忠臣可立就不发动（非忠臣储君无法继位）
    if (me->isAI() && !aiHeir) return;
    if (!engine.askConfirm(me, "【立储】第一轮的结束阶段：是否明选一名角色为储君（太子）？",
                           static_cast<bool>(aiHeir))) return;

    std::vector<PlayerPtr> cands;
    for (const auto& p : engine.getAlivePlayers()) if (p->getId() != self.getId()) cands.push_back(p);
    if (cands.empty()) return;
    PlayerPtr heir = engine.askChoosePlayer(me, cands, "【立储】明选储君（太子）", false, aiHeir ? aiHeir : cands.front());
    if (!heir) return;
    setMark(self, "立储已用", 1);
    heir->addMark("储君", 1);
    engine.logMessage("  【立储】" + self.getName() + " 明选 " + heir->getName() + " 为储君（太子）："
                      "若主公阵亡且储君为忠臣，储君继位成为新的主公。");
}

// ==================== 分发 ====================

bool isModeSkillName(const std::string& name) {
    return name == "飞扬" || name == "跋扈" || name == "共苦" || name == "择途" || name == "立储";
}

void applyIdentityModeSkills(GameEngine& engine, Player& player) {
    if (!player.getHero()) return;
    if (engine.getGameMode() != GameEngine::GameMode::JUNZHENG) return;
    if (player.getIdentity() == Identity::NEI_JIAN) {
        if (!player.getHero()->findSkill("择途"))
            player.getHero()->addSkill(std::make_shared<ZheTuSkill>());
    } else if (player.getIdentity() == Identity::ZHU_GONG) {
        if (!player.getHero()->findSkill("立储"))
            player.getHero()->addSkill(std::make_shared<LiChuSkill>());
    }
}

void applyAmbitionSkills(GameEngine& engine, Player& player) {
    if (!player.getHero()) return;
    if (!player.getHero()->findSkill("飞扬"))
        player.getHero()->addSkill(std::make_shared<DouFeiYangSkill>());
    if (!player.getHero()->findSkill("跋扈"))
        player.getHero()->addSkill(std::make_shared<DouBaHuSkill>());
    (void)engine;
}

void applyDoudizhuModeSkills(GameEngine& engine, Player& player) {
    if (!player.getHero()) return;
    if (player.getIdentity() == Identity::DI_ZHU) {
        player.getHero()->addSkill(std::make_shared<DouFeiYangSkill>());
        player.getHero()->addSkill(std::make_shared<DouBaHuSkill>());
    } else if (player.getIdentity() == Identity::NONG_MIN) {
        player.getHero()->addSkill(std::make_shared<DouGongKuSkill>());
    }
    (void)engine;
}

} // namespace Thks
