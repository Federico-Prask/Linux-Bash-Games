#ifndef THKS_SKILLS_MYTH_H
#define THKS_SKILLS_MYTH_H
#include "Hero.h"
namespace Thks {
// 风火林山技能：每名武将创建独立实例，限定技/觉醒技状态不共享。
class MythSkill : public Skill {
    bool spent = false;
    int uses = 0;
    int bonus = 0;
    int recentDamageTargetId = -1;
    bool recentDamageInRange = false;
    std::vector<int> markedTargets;
    std::vector<std::pair<CardPtr,bool>> damageCardsInFlight;
    bool grantedWuShuang = false;
    bool grantedWanSha = false;
    bool yinghunModeFirst = false;
    std::vector<std::string> avatars;
    std::string borrowedSkill;
    std::string shownAvatar;
    void gainAvatar(GameEngine&);
    void changeAvatar(GameEngine&, Player&);
public:
    MythSkill(const std::string& name, const std::string& description, SkillKind kind, unsigned tags = 0,
              bool yinghunModeFirst = false);
    const std::vector<std::string>& getAvatarIds() const { return avatars; }
    const std::string& getShownAvatarId() const { return shownAvatar; }
    const std::string& getBorrowedSkillName() const { return borrowedSkill; }
    bool hasEffectiveLockedComponent() const override;
    bool isUsableActively() const override;
    bool isConversionSkill() const override;
    bool canActivate(GameEngine&, Player&) override;
    void onActivate(GameEngine&, Player&) override;
    bool aiShouldActivate(GameEngine&, Player&) override;
    void resetTurnState() override { uses = 0; bonus = 0; }
    CardPtr convertCard(GameEngine&, Player&, CardPtr, CardSubType) override;
    std::vector<CardPtr> convertCards(GameEngine&, Player&, CardPtr, CardSubType) override;
    bool canDelegate(GameEngine&, Player&, Player&) override;
    void invokeDelegated(GameEngine&, Player&, Player&) override;
    void onTurnStart(GameEngine&, Player&) override;
    void onTurnEnd(GameEngine&, Player&, Player&) override;
    void onTurnBoundary(GameEngine&, Player&, Player&, bool starting) override;
    void onPhaseStart(GameEngine&, Player&, TurnPhase, bool&) override;
    void onPhaseEnd(GameEngine&, Player&, TurnPhase) override;
    void onPhaseSkipped(GameEngine&, Player&, Player&, TurnPhase) override;
    void onDrawCards(GameEngine&, Player&, int&) override;
    void onCalculateDistance(GameEngine&, const Player&, const Player&, int&) override;
    void onCalculateShaLimit(GameEngine&, const Player&, int&) override;
    void onCalculateShaTargets(GameEngine&, const Player&, CardPtr, int&) override;
    void onCheckShaTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onDuelTargeted(GameEngine&, Player&, Player&, Player&) override;
    void onCalculateHandLimit(GameEngine&, const Player&, int&) override;
    void onCheckCardTarget(GameEngine&, const Player&, const Player&, CardPtr, bool&) override;
    void onCheckCardEffect(GameEngine&, const Player&, CardPtr, bool&) override;
    void onCalculateResponseCount(GameEngine&, const Player&, const Player&, CardSubType, int&) override;
    void onShaTargeted(GameEngine&, Player&, ShaContext&) override;
    void onShaFinished(GameEngine&, Player&, ShaContext&) override;
    void onTakeDamage(GameEngine&, Player&, Player*, int&, ShaElement) override;
    void onTakeDamageFromCard(GameEngine&, Player&, Player*, int&, ShaElement, CardPtr, bool*) override;
    void onAfterDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
    void onDamageApplied(GameEngine&, Player&, Player*, Player&, int, CardPtr) override;
    void onAfterDealDamage(GameEngine&, Player&, Player*, int, ShaElement, CardPtr) override;
    void onGlobalDamage(GameEngine&, Player&, Player*, Player&, int, CardPtr) override;
    void onDying(GameEngine&, Player&, Player&) override;
    void onLeaveDying(GameEngine&, Player&) override;
    void onAfterRecover(GameEngine&, Player&, int) override;
    void onPlayerDeath(GameEngine&, Player&, Player&, Player*) override;
    void onRemoved(GameEngine&, Player&) override;
    void onBeforeJudge(GameEngine&, Player&, Player&, CardPtr&) override;
    void onAfterJudge(GameEngine&, Player&, Player&, CardPtr, bool&) override;
    void onCardLostOutsideTurn(GameEngine&, Player&, CardPtr) override;
    void onCardPlayed(GameEngine&, Player&, CardPtr) override;
    void onCardResponded(GameEngine&, Player&, CardPtr) override;
    void onCardResolved(GameEngine&, Player&, CardPtr) override;
    void onUseCard(GameEngine&, Player&, CardPtr) override;
    void onGameStart(GameEngine&, Player&) override;
    void onHandCardLostToOther(GameEngine&, Player&, Player&, Player&) override;
    void onDiscardedInDiscardPhase(GameEngine&, Player&, CardPtr) override;
    void onOtherDiscardPhaseEnd(GameEngine&, Player&, Player&, const std::vector<CardPtr>&) override;
};
// 完整四篇（含每篇两位神将）的静态登记；追加到标准/界限/DIY池。
struct MythHeroSpec {
    const char* id; const char* name; const char* title; const char* pack;
    Country country; Gender gender; int hp;
    const char* skills; // 用逗号分隔技能名
};
const std::vector<MythHeroSpec>& mythHeroes();
std::vector<SkillPtr> createMythSkills(const char* names);
} // namespace Thks
#endif
