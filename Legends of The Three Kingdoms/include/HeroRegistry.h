#ifndef THKS_HERO_REGISTRY_H
#define THKS_HERO_REGISTRY_H

#include <string>
#include <vector>
#include <functional>
#include "Hero.h"

namespace Thks {

// 武将池中的一条登记信息（武将按扩展包归类）
struct HeroInfo {
    std::string id;
    std::string name;
    std::string title;
    std::string pack;      // 标准包 / 界限突破 / 一将成名
    Country country;
    Gender gender;
    int maxHp;
    std::function<HeroPtr()> create;  // 每次调用生成全新实例（技能状态互不干扰）
};

// 选将界面的包分类（分类名 → 包含的扩展包）
struct PackCategory {
    std::string name;
    std::vector<std::string> packs;
    // 非空时在选将和图鉴中显示第二级小包。
    std::vector<std::string> subpacks;
};

class HeroRegistry {
public:
    static const std::vector<HeroInfo>& all();
    static const HeroInfo* find(const std::string& id);
    static HeroPtr create(const std::string& id);
    static std::vector<std::string> allIds();
    // 不同版本及神武将共用人物身份；同一局只允许一个版本。
    static std::string personKey(const std::string& id);
    // 按扩展包顺序返回所有包名（去重、保持登记顺序）
    static std::vector<std::string> packs();
    // 选将界面的包分类（官方扩展包及项目内 DIY 包）
    static std::vector<PackCategory> packCategories();
    static std::string countryName(Country c);
};

} // namespace Thks

#endif // THKS_HERO_REGISTRY_H
