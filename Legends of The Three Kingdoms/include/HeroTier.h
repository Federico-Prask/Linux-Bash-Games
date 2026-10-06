#ifndef THKS_HERO_TIER_H
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
