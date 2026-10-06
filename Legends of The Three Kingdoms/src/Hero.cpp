#include "Hero.h"
#include "GameEngine.h"
#include "Player.h"
#include <sstream>
#include <algorithm>

namespace Thks {

// ==================== Skill ====================

bool ActiveSkill::canActivate(GameEngine& /*engine*/, Player& self) {
    return self.isAlive() && hasUsesLeft();
}

void Skill::activate(GameEngine& engine, Player& self) {
    if (engine.isGameOver()) return; // 用户 2026-10-05：游戏结束后不存在技能发动
    if (!self.isAlive() || isConversionSkill()) return;
    if (const auto* active = dynamic_cast<const ActiveSkill*>(this); active && !active->hasUsesLeft()) return;

    // Ordinary主动技只能在出牌阶段发动。显式的事件结算型技能须自行重写
    // canActivateOutsidePlayPhase()，并由 canActivate() 验证其待处理事件。
    if (isUsableActively() && !canActivateOutsidePlayPhase() &&
        engine.getCurrentPhase() != TurnPhase::PLAY) return;
    if (!canActivate(engine, self)) return;

    onActivate(engine, self);
}

std::string Skill::getKindString() const {
    switch (kind) {
        case SkillKind::ACTIVE:  return "主动技";
        case SkillKind::TRIGGER: return "触发技";
        case SkillKind::STATE:   return "状态技";
        default:                 return "未知";
    }
}

std::string Skill::getTagString() const {
    std::vector<std::string> parts;
    if (hasTag(SkillTag::LOCK))    parts.push_back("锁定技");
    if (hasTag(SkillTag::LIMITED)) parts.push_back("限定技");
    if (hasTag(SkillTag::AWAKEN))  parts.push_back("觉醒技");
    if (hasTag(SkillTag::LORD))    parts.push_back("主公技");
    if (hasTag(SkillTag::SWITCH))  parts.push_back("转换技");
    if (hasTag(SkillTag::SUSTAINED) && !declaresSustainedTagInDescription())
        parts.push_back("持恒技");
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) result += "/";
        result += parts[i];
    }
    return result;
}

std::string Skill::getFullDescription() const {
    std::ostringstream ss;
    ss << "【" << name << "】(" << getKindString();
    std::string tagStr = getTagString();
    if (!tagStr.empty()) ss << "·" << tagStr;
    ss << ") " << description;
    return ss.str();
}

// ==================== Hero ====================

Hero::Hero(const std::string& id, const std::string& name, const std::string& title, const std::string& pack,
           Country country, Gender gender, int maxHp)
    : id(id), name(name), title(title), pack(pack), country(country), gender(gender), maxHp(maxHp) {}

bool Hero::removeSkill(const std::string& skillName) {
    auto it = std::find_if(skills.begin(), skills.end(), [&](const SkillPtr& s) { return s->getName() == skillName; });
    if (it != skills.end()) {
        // 持恒技不是普通状态技：其技能牌不能被移除。
        if ((*it)->isSustained()) return false;
        skills.erase(it);
        return true;
    }
    return false;
}

SkillPtr Hero::findSkill(const std::string& skillName) const {
    for (const auto& s : skills) {
        if (s->getName() == skillName) return s;
    }
    return nullptr;
}

std::string Hero::getCountryString() const {
    switch (getCountry()) {
        case Country::WEI: return "魏";
        case Country::SHU: return "蜀";
        case Country::WU:  return "吴";
        case Country::QUN: return "群";
        case Country::GOD: return "神";
        default:           return "?";
    }
}

std::string Hero::getGenderString() const {
    switch (getGender()) {
        case Gender::MALE:   return "男";
        case Gender::FEMALE: return "女";
        default:             return "未知";
    }
}

std::string Hero::getSkillSummary() const {
    std::ostringstream ss;
    if (skills.empty()) {
        ss << "    （无技能）\n";
        return ss.str();
    }
    for (const auto& s : skills) {
        ss << "    " << s->getFullDescription() << "\n";
    }
    return ss.str();
}

} // namespace Thks
