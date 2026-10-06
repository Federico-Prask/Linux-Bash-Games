// SkillsMou.cpp — 谋攻篇（谋）武将技能实现
// 技能文本：docs/mou_appendix.md 附录C（官网 hero-detail 逐字；夺荆/英姿※/英魂※见该文件来源说明）
#include "SkillsMou.h"
#include "SkillsMode.h"
#include "HeroRegistry.h"
#include "GameEngine.h"
#include "Player.h"
#include "AI.h"
#include <algorithm>
#include <limits>
#include <random>

namespace Thks {

namespace {

PlayerPtr selfPtr(GameEngine& engine, const Player& self) {
    return engine.getPlayerById(self.getId());
}

bool hasLegalShaOption(GameEngine& engine, PlayerPtr user, PlayerPtr target, bool ignoreDistance) {
    if (!user || !target || !user->isAlive() || !target->isAlive()) return false;
    for (const auto& sha : engine.getResponseCandidates(user, CardSubType::SHA)) {
        if (engine.canUseShaOn(*user, *target, sha, ignoreDistance) &&
            engine.canBeTargeted(target, sha, user)) return true;
    }
    return false;
}

std::string who(const Player& p) {
    return "[" + p.getName() + "]";
}

std::string textKey(const std::string& hero, const std::string& skill) {
    return hero + "/" + skill;
}

} // namespace

const std::string& mouText(const std::string& hero, const std::string& skill) {
    static const std::map<std::string, std::string> table = {
        // ==== 附录C 官网原文（逐字） ====
        {textKey("谋·刘赪", "谋-掠影"), "出牌阶段内，你使用【杀】指定其他角色为目标时，你获得一个“椎”标记，出牌阶段限两次。你使用【杀】结算结束后，若你拥有至少两个“椎”标记，则你移除两个“椎”标记，然后摸一张牌，且可以选择一名角色视为对其使用一张【过河拆桥】。"},
        {textKey("谋·刘赪", "谋-莺舞"), "出牌阶段内，你使用非伤害类普通锦囊指定一名角色为目标时，若你拥有技能“掠影”，则你获得一个“椎”标记，出牌阶段限两次。你使用非伤害类普通锦囊结算结束后，若你拥有至少两个“椎”标记，则你移除两个“椎”标记，然后摸一张牌，且可以选择一名角色视为对其使用一张不受次数限制的普通【杀】。"},
        {textKey("谋·吕蒙", "谋-克己"), "出牌阶段各限一次（若你已发动过“渡江”，则修改为出牌阶段限一次），你可以选择一项执行对应效果：1. 弃置一张手牌，获得1点“护甲”；2. 流失1点体力，获得2点护甲。你的手牌上限+X(X为你的护甲值)。若你不处于濒死状态，你无法使用【桃】。"},
        {textKey("谋·吕蒙", "谋-渡江"), "觉醒技，准备阶段，若你的“护甲”值不少于3，则获得技能“夺荆”。"},
        {textKey("谋·吕蒙", "谋-夺荆"), "当你使用【杀】指定一名角色为目标时，你可以失去1点护甲，令此【杀】无视该角色的防具，然后你获得该角色的一张牌且你本阶段使用【杀】的次数上限+1。"},
        {textKey("谋·黄忠", "谋-烈弓"), "若你未装备武器，你的【杀】只能当作普通【杀】使用或打出。你使用牌时或成为其他角色使用牌的目标后，若此牌的花色未被“烈弓”记录，则记录此种花色。当你使用【杀】指定唯一目标后，你可以展示牌堆顶的X张牌（X为你记录的花色数-1，且至少为0），然后每有一张牌花色与“烈弓”记录的花色相同，你令此【杀】伤害+1，且其不能使用“烈弓”记录花色的牌响应此【杀】。若如此做，此【杀】结算结束后，清除“烈弓”记录的花色。"},
        {textKey("谋·华雄", "谋-耀武"), "锁定技，当你受到【杀】造成的伤害时，若此【杀】为红色，伤害来源回复1点体力或摸一张牌；若此【杀】不为红色，则你摸一张牌。"},
        {textKey("谋·华雄", "谋-扬威"), "出牌阶段限一次，你可以摸两张牌并获得“威”标记直到此阶段结束，然后此技能失效直到下个回合的结束阶段。拥有“威”标记的角色出牌阶段可以额外使用一张【杀】、使用【杀】无距离限制且无视防具。"},
        {textKey("谋·杨婉", "谋-暝眩"), "出牌阶段开始时，选择数张牌然后随机交给其他角色，然后其选择一项：对你使用一张【杀】；2.交给你一张牌，且你摸一张牌（官网原文如此）"},
        {textKey("谋·杨婉", "谋-陷仇"), "当你受到伤害后，可以选择另一名角色，其可以弃置一张牌，视为对伤害来源使用一张【杀】。若此【杀】造成伤害，则你回复1点体力值。"},
        {textKey("谋·马超", "谋-铁骑"), "使用杀指定的目标角色，非锁定技失效，不能使用闪响应此杀，通过谋弈获得牌或摸牌"},
        {textKey("谋·马超", "谋-马术"), "你计算与其他角色的距离-1。"},
        {textKey("谋·张飞", "谋-咆哮"), "锁定技，你使用【杀】无次数限制。若你装备了武器，你使用【杀】无距离限制。你的出牌阶段，若你于当前阶段内使用过【杀】，你于此阶段使用【杀】具有以下效果：此【杀】指定的目标本回合非锁定技失效；此【杀】不可被响应且伤害值+1；此【杀】对一名角色造成伤害后若其未死亡，你失去1点体力并随机弃置一张手牌。"},
        {textKey("谋·张飞", "谋-协击"), "准备阶段，你可以选择一名其他角色，与其进行“协力”。其回合的结束阶段，若你与其“协力”成功，则你可以选择至多三名角色，依次视为对其使用一张普通【杀】，你以此【杀】造成伤害后，你摸等同于此【杀】造成伤害数的牌。"},

        {textKey("谋·赵云", "谋-龙胆"), "剩余可用X次（×初始为1且最大为3，每名角色的回合结束后X加1)，你可以将一张【杀】当【闪】、【闪】当普通【杀】使用或打出，若如此做,你摸一张牌。"},
        {textKey("谋·赵云", "谋-积著"), "准备阶段,你可以选择一名其他角色，与其进行“协力”。其回合结束后，若你与其“协力”成功，则直到你的下个回合结束后，你修改龙胆为“剩余可用次数×次（×初始为1且最大为3，每名角色的回合结束后X加一），你可以将一张基本牌当做任意基本牌使用或打出，若如此做，你摸一张牌”。"},
        {textKey("谋·孙尚香", "谋-结姻"), "使命技，你的登场势力为“蜀”。游戏开始时，你选择一名其他角色，令其获得“助”标记。出牌阶段开始时，有“助”标记的角色选择一项：1. 若其有手牌，交给你两张手牌（若其手牌不足两张则交给你所有手牌），然后其获得一点“护甲”；2、令你移动或移除助标记（若其不是第一次获得“助”标记，则你只能移除“助”标记。）失败：当“助”标记被移除时，你回复1点体力并获得你武将牌上所有“妆”牌，移除“助”标记，你将势力修改为“吴”，减1点体力上限。"},
        {textKey("谋·孙尚香", "谋-良助"), "蜀势力技，出牌阶段限一次，你可以将其他角色装备区内的一张牌置于你的武将牌上，称为“妆”，然后令拥有“助”标记的角色选择一项：1.回复1点体力值；2.摸两张牌。"},
        {textKey("谋·孙尚香", "谋-枭姬"), "吴势力技，当你失去装备区内的一张牌时，你摸两张牌，然后可以弃置场上的一张牌。"},
        {textKey("谋·夏侯氏", "谋-燕语"), "出牌阶段限两次，你可以弃置一张【杀】并摸一张牌。出牌阶段结束时，你可以令一名其他角色摸X张牌（X为你本回合以此法弃置的【杀】的数量的三倍）。"},
        {textKey("谋·夏侯氏", "谋-樵拾"), "每回合限一次，你受到其他角色造成的伤害后，伤害来源可选择令你回复等同此次伤害值的体力，若如此做，其摸两张牌。"},
        {textKey("谋·周瑜", "谋-英姿"), "锁定技，摸牌阶段，你以下的数值每满足一项，你多摸一张牌，且本回合手牌上限+1:1.你的手牌数不少于2;2.你的体力值不低于2;3.你装备区的牌数不低于1。"},
        {textKey("谋·周瑜", "谋-反间"), "出牌阶段,你可以选择一名其他角色、扣置一张本回合未以此法扣置过的花色的手牌，并声明一个花色，其须选择一项:1.猜测此牌花色与声明花色是否一致;2.其翻面，且此技能失效直到回合结束。然后你展示此牌令其获得之。若其选择猜测，则:若猜对，此技能失效直到回合结束;若猜错，其失去1点体力。"},
        {textKey("谋·貂蝉", "谋-离间"), "出牌阶段限一次，你可以选择至少两名其他角色并弃置X张牌（X为你选择的角色数-1），然后他们依次对逆时针最近座次的你选择的另一名角色视为使用一张【决斗】。"},
        {textKey("谋·貂蝉", "谋-闭月"), "锁定技，结束阶段，你摸X张牌。（X为本回合受到伤害的角色数+1，至多为4）"},
        {textKey("谋·袁绍", "谋-乱击"), "出牌阶段限一次，你可以将两张手牌当【万箭齐发】使用。其他角色因响应你使用的【万箭齐发】而打出【闪】时，你摸一张牌（每回合你至多以此法获得3张牌）。"},
        {textKey("谋·袁绍", "谋-血裔"), "主公技，锁定技，你的手牌上限+X（X为其他群势力角色数的两倍）。你使用牌指定其他群势力角色为目标后，你摸一张牌（每回合你至多以此法获得2张牌）。"},
        {textKey("谋·庞统", "谋-连环"), "一级：出牌阶段，你可以将一张梅花手牌当【铁索连环】使用（每个出牌阶段限1次），或重铸一张梅花手牌。你使用【铁索连环】时，你可以失去一点体力，若如此做，你指定一名角色为目标后，若其不处于连环状态，随机弃置其一张手牌。二级：出牌阶段，你可以将一张梅花手牌当【铁索连环】使用（每个出牌阶段限1次），或重铸一张梅花手牌。你使用【铁索连环】可以额外指定任意名目标。你使用【铁索连环】指定一名角色为目标后，若其不处于连环状态，随机弃置其一张手牌。"},
        {textKey("谋·庞统", "谋-涅槃"), "限定技，当你处于濒死状态时，你可以弃置区域里的所有牌，摸两张牌，将体力回复至2点，复原武将牌，并升级“连环”。"},
        {textKey("谋·刘备", "谋-仁德"), "出牌阶段开始时，你获得2个“仁望”标记。出牌阶段，你可以将任意张牌交给一名本阶段未获得过“仁德”牌的其他角色，然后你获得等量的“仁望”标记（你至多拥有8个“仁望”标记）。每回合限一次，当你需要使用或打出一张基本牌时，你可以弃置2个“仁望”标记视为使用或打出之。"},
        {textKey("谋·刘备", "谋-章武"), "限定技，出牌阶段，你可以令本局游戏中所有获得过“仁德”牌的角色依次交给你Y张牌（Y为游戏轮数-1，且最大为3），若如此做，你回复3点体力，然后失去“仁德”。"},
        {textKey("谋·刘备", "谋-激将"), "主公技，出牌阶段结束时，你可指定一名角色，并令另一名攻击范围内含有该角色且体力值不小于你的其他蜀势力角色选择一项：1.视为对你指定的角色使用一张普通【杀】；2.跳过下一个出牌阶段。"},
        {textKey("谋·姜维", "谋-挑衅"), "蓄力技，出牌阶段限一次，你可以至多选择X名其他角色（X为你拥有的蓄力点数量），令这些角色依次选择一项：1.对你使用一张无距离限制的【杀】；2.交给你一张牌。然后你每选择一名角色，减少1点蓄力点。弃牌阶段，你每弃置一张牌，获得1点蓄力点。"},
        {textKey("谋·姜维", "谋-志继"), "觉醒技，准备阶段，若你发动“挑衅”选择过至少4名角色，你减少1点体力值上限，令任意名角色直到你的下个回合开始时获得“北伐”标记。拥有“北伐”标记的角色使用牌只能选择你或其为目标。"},
        {textKey("谋·法正", "谋-眩惑"), "出牌阶段限一次，你可以交给一名没有“眩”标记的其他角色一张牌并令其获得“眩”标记。有“眩”标记的角色于摸牌阶段外获得牌时，你随机获得其一张手牌（每个“眩”标记最多令你获得五张牌）。"},
        {textKey("谋·法正", "谋-恩怨"), "锁定技，准备阶段，你令有“眩”标记的角色执行以下效果：自其获得“眩”标记开始，若你获得其至少三张牌，则你移除其“眩”标记，然后交给其三张牌；否则其流失1点体力值，然后你回复1点体力并移除其“眩”标记。"},
        {textKey("谋·陈宫", "谋-明策"), "出牌阶段限一次，你可以将一张牌交给一名其他角色，然后其选择一项：1. 其流失1点体力，你摸两张牌并获得一个“策”标记；2.其摸一张牌。出牌阶段开始时，若你拥有“策”标记，你可以选择一名其他角色，对其造成X点伤害并移除所有“策”标记（X为你拥有的“策”标记数量）。"},
        {textKey("谋·陈宫", "谋-智迟"), "锁定技，当你受到伤害后，本回合接下来你受到伤害时，防止之。"},
        {textKey("谋·甘宁", "谋-奇袭"), "出牌阶段限一次，你可以选择一名其他角色，令其猜测你手牌中某种花色的牌最多（或之一）。若其猜错，你可令其再次猜测（其无法选择此阶段已猜测过的花色）；否则你展示所有手牌。然后你弃置其区域内X张牌。（X为其此阶段猜错的次数，若不足则全弃）"},
        {textKey("谋·甘宁", "谋-奋威"), "限定技，出牌阶段，你可以将至多三张牌置于任意名角色的武将牌上（每名角色各一张），称为“威”，然后你摸等量的牌。有“威”的角色成为锦囊牌的目标时，你须选择一项：1.令其获得“威”牌；2.弃置其“威”牌，取消其作为此锦囊牌的目标。"},
        {textKey("谋·黄盖", "谋-苦肉"), "出牌阶段开始时，你可以交给其他角色一张牌，然后失去1点体力（若你交出的牌是【桃】或【酒】，则改为失去2点体力）。当你失去1点体力后，你获得2点护甲。"},
        {textKey("谋·黄盖", "谋-诈降"), "锁定技，你于每个回合使用的前X张牌无距离和次数限制且不可被响应。摸牌阶段，你多摸X张牌。（X为你已损失体力值）"},
        {textKey("谋·孙权", "谋-制衡"), "出牌阶段限一次，你可以弃置任意张牌，然后摸等量的牌。若你弃置了所有手牌，则额外摸X+1张牌（X为你拥有的“业”标记数量），然后移除一个“业”标记。"},
        {textKey("谋·孙权", "谋-统业"), "锁定技，结束阶段，你须选择一项，直到下回合准备阶段：1.若场上的装备数变化，则你获得一个“业”标记，否则失去一个“业”标记；2.若场上的装备数不变，则你获得一个“业”标记，否则失去一个“业”标记。你至多拥有2个“业”标记。"},
        {textKey("谋·孙权", "谋-救援"), "主公技，锁定技，其他吴势力角色使用【桃】时，你摸一张牌。其他吴势力角色对你使用【桃】回复的体力+1。"},
        {textKey("谋·大乔", "谋-国色"), "出牌阶段限四次，你可以将一张方块牌当【乐不思蜀】使用，或弃置场上一张【乐不思蜀】。然后你摸一张牌。"},
        {textKey("谋·大乔", "谋-流离"), "当你成为【杀】的目标时，你可以弃置一张牌并选择你攻击范围内的一名其他角色（不能是此【杀】的使用者），然后将此【杀】转移给该角色。每名角色的回合限一次，若你弃置的是红桃牌，你可令一名其他角色（不能是此【杀】的使用者）获得“流离”标记（若场上已有“流离”标记则改为转移给该角色）。拥有“流离”标记的角色回合开始时，执行一个额外的出牌阶段并令其移除“流离”标记。"},
        {textKey("谋·孟获", "谋-祸首"), "锁定技，【南蛮入侵】对你无效；当其他角色使用【南蛮入侵】指定目标后，你代替其成为此牌造成的伤害的来源。出牌阶段开始时，你随机获得弃牌堆中一张【南蛮入侵】。出牌阶段，若你使用过【南蛮入侵】，则你此阶段不能使用【南蛮入侵】。"},
        {textKey("谋·孟获", "谋-再起"), "蓄力技（0/7），弃牌阶段结束时，你可以选择任意名角色并扣除等量蓄力点，然后令你选择的角色各选择一项：1.令你摸一张牌；2.弃置一张牌，然后你回复1点体力。当你造成伤害后，获得1点蓄力点（每回合限获得1点蓄力点）。"},
        {textKey("谋·孙策", "谋-激昂"), "一级：你使用【决斗】可以额外指定一名目标，若如此做，你流失1点体力。当你使用【决斗】或红色【杀】指定一名目标后，或成为【决斗】或红色【杀】的目标后，你摸一张牌。出牌阶段限一次，你可以将所有手牌当【决斗】使用。二级：你使用【决斗】可以额外指定一名目标，若如此做，你流失1点体力。当你使用【决斗】或红色【杀】指定一名目标后，或成为【决斗】或红色【杀】的目标后，你摸一张牌。出牌阶段限X次（X为场上吴势力角色数），你可以将所有手牌当【决斗】使用。"},
        {textKey("谋·孙策", "谋-魂姿"), "觉醒技，你脱离濒死状态时，你减1点体力上限、获得1点护甲、摸三张牌，然后获得技能“英姿※”和“英魂※”。"},
        {textKey("谋·孙策", "谋-制霸"), "主公技，限定技，当你进入濒死状态时，你可回复X点体力（X为场上吴势力角色数量-1）并升级技能“激昂”，然后其他吴势力角色依次受到1点无来源伤害，若其因此伤害死亡，则其死亡后，你摸三张牌。"},
        {textKey("谋·孙策", "谋-英姿※"), "锁定技，摸牌阶段，你每满足以下一项，你便多摸一张牌且本回合手牌上限+1：1.手牌数大于等于二；2.体力值大于等于２；3.装备区的牌数大于等于１。"},
        {textKey("谋·孙策", "谋-英魂※"), "准备阶段，若你已受伤，你可以选择一名其他角色并选择一项：1.令其摸X张牌，然后弃置一张牌；2.令其摸一张牌，然后弃置X张牌（X为你已损失的体力值）。"},
        {textKey("谋·祝融", "谋-烈刃"), "当你使用【杀】指定一名其他角色为唯一目标后，你可与其拼点，若你赢，此【杀】结算结束后，你可对另一名其他角色造成1点伤害。"},
        {textKey("谋·祝融", "谋-巨象"), "锁定技，【南蛮入侵】对你无效；当其他角色使用的【南蛮入侵】结算结束后，你获得之。结束阶段，若你本回合未使用过【南蛮入侵】，你随机从游戏外将一张【南蛮入侵】交给一名角色。"},
        {textKey("谋·卢植", "谋-明任"), "明任：游戏开始时，你摸两张牌，然后将你的一张手牌扣置于你的武将牌上，称为“任”。结束阶段，你可以用手牌替换“任”。"},
        {textKey("谋·卢植", "谋-贞良"), "贞良：转换技，阳：出牌阶段限一次，你可以选择一名攻击范围内的其他角色并弃置x张与“任”颜色相同的牌对其造成1点伤害（x为你与其体力值之差且至少为1） 阴：你的回合外，当一名角色使用或打出的牌结算结束后，若此牌与“任”类型相同，则你可令一名角色摸两张牌。"},
        {textKey("谋·诸葛亮", "谋-火计"), "使命技，出牌阶段限一次，你可以选择一名其他角色，对其及其同势力的其他角色各造成1点火焰伤害。成功：准备阶段，若你本局游戏对其他角色造成过至少X点火焰伤害（X为本局游戏人数），你失去“火计”和“看破”，获得“观星※”和“空城※”。失败：成功达成使命前，进入濒死状态。"},
        {textKey("谋·诸葛亮", "谋-看破"), "看破：每轮开始时，你清除“看破”记录的牌名，然后你可以选择并记录任意个数的牌名（不可选择上次发动此技能记录过的牌名；每局游戏最多记录4个牌名，若为斗地主和排位赛模式则修改为2）。其他角色使用与你记录牌名相同的牌时，你可以移除一个对应牌名的记录，然后令此牌无效，且你摸一张牌。"},
        {textKey("谋·诸葛亮", "谋-观星※"), "观星：准备阶段，你移去所有的“星”，并将牌堆顶的X张牌置于武将牌上（X为7-此前此技能准备阶段发动次数的三倍），称为“星”。然后你可以将任意张“星”牌置于牌堆顶。结束阶段，若你未于准备阶段将“星”牌置于牌堆顶，则你可以将任意张“星”牌置于牌堆顶。当你需要使用或打出手牌时，你可以将“星”视为你的牌使用或打出。"},
        {textKey("谋·诸葛亮", "谋-空城※"), "空城：锁定技，当你受到伤害时，若你有技能“观星”且你的武将牌上有“星”，你进行一次判定，若判定结果点数小于等于你“星”牌的数量，则此伤害-1；若你有技能“观星”且你武将牌上没有“星”，你受到的伤害+1。"},
        {textKey("谋·关羽", "谋-武圣"), "武圣：你可以将一张手牌当作【杀】使用或打出。出牌阶段开始时，你可以指定一名主公以外的角色。此阶段：你对其使用【杀】无距离和次数限制；你使用【杀】指定其为目标后，你摸一张牌（若为身份场则修改为摸两张牌）；你对其使用三张【杀】后，不可再指定其为你使用【杀】的目标。"},
        {textKey("谋·关羽", "谋-义绝"), "义绝：锁定技。一名其他角色于你的回合内受到你造成的伤害时，若此伤害会令其进入濒死状态，防止之（本局游戏每名角色限一次）。若如此做，直到回合结束，你使用牌指定其为目标时，取消之。"},
        {textKey("谋·黄月英", "谋-集智"), "锁定技，当你使用一张普通锦囊牌时，你摸一张牌。以此法获得的牌本回合不计入手牌上限。"},
        {textKey("谋·黄月英", "谋-奇才"), "（身份场、团战类）你使用锦囊牌没有距离限制。出牌阶段限一次，你可以选择一名其他角色，将手牌或弃牌堆中的一张装备牌置入其装备区，然后其获得“奇”标记。拥有“奇”标记的角色接下来获得的三张普通锦囊牌须交给你。（斗地主）你使用锦囊牌没有距离限制。出牌阶段限一次，你可以选择一名其他角色，将手牌或弃牌堆中一张防具牌置入其装备区（每局游戏每个防具名限一次），然后其获得“奇”标记。拥有“奇”标记的角色接下来获得的三张普通锦囊牌须交给你。"},
        {textKey("谋·小乔", "谋-天香"), "（身份场、斗地主）准备阶段，若场上有“天香”标记，则你清除场上所有“天香”标记，并摸等量的牌。出牌阶段限三次，你可将一张红色手牌交给一名没有“天香”标记的其他角色，并令其获得对应花色的“天香”标记。当你受到伤害时，你可以选择一名拥有“天香”标记的角色，移除其“天香”标记，并根据移除的“天香”花色发动：红桃，你防止此伤害，然后令其受到防止伤害的来源角色造成的1点伤害；方块，其交给你两张牌。（团战类）准备阶段，若场上有“天香”标记，则你清除场上所有“天香”标记，并摸x张牌（x为本次清除的“天香”标记数+2）。出牌阶段限三次，你可将一张红色手牌交给一名没有“天香”标记的其他角色，并令其获得对应花色的“天香”标记。当你受到伤害时，你可以选择一名拥有“天香”标记的角色，移除其“天香”标记，并根据移除的“天香”花色发动：红桃，你防止此伤害，然后令其受到防止伤害的来源角色造成的1点伤害；方块，其交给你两张牌。"},
        {textKey("谋·小乔", "谋-红颜"), "锁定技。你的黑桃手牌只能当做红桃牌使用、打出、弃置或交给其他角色。你的黑桃判定牌只能当做红桃判定牌。当一张判定牌生效前，如果此判定牌为红桃，你将判定结果改为由你指定的一种花色。"},
        {textKey("谋·公孙瓒", "谋-义从"), "蓄力技（2/4）。每轮开始时，你可消耗至多x点蓄力点并选择一项：直至本轮结束，你与其他角色距离-1，并将牌堆中的x张【杀】置于武将牌上，称为“扈”；直至本轮结束，其他角色与你距离+1，并将牌堆中的x张【闪】置于武将牌上，称为“扈”。你至多拥有四张“扈”，当你需要使用或打出手牌时，你可以将”扈”视为你的牌使用或打出。"},
        {textKey("谋·公孙瓒", "谋-趫猛"), "你使用【杀】对一名角色造成伤害后，若你拥有技能“义从”，你可选择一项：1.弃置其区域内的一张牌并摸一张牌 2.获得3蓄力点。"},
        {textKey("谋·韩当", "谋-弓骑"), "你的攻击范围+4。出牌阶段开始时，你可弃一张牌，若如此做，则此阶段你使用的牌其他角色只能使用或打出虚拟牌或与你弃置牌颜色相同的手牌响应。"},
        {textKey("谋·韩当", "谋-解烦"), "出牌阶段限一次，你可指定一名角色，令其选择一项：1.攻击范围内含有其的角色依次弃一张牌;2.其摸此时攻击范围内有其的角色数的牌；背水：此技能失效直至你杀死一名角色。"},
        {textKey("谋·陆逊", "谋-谦逊"), "当一张锦囊牌对你生效时，若此牌名未记录且你不是使用者，则你记录之，然后可将至多X张牌置于你的武将牌上（X为“谦逊”记录的牌名数且至多为5）；若如此做，此回合结束时，你获得武将牌上的所有牌。出牌阶段开始时，你可移去一个记录的牌名，若为普通锦囊牌的牌名，则你可视为使用此牌。"},
        {textKey("谋·陆逊", "谋-连营"), "身份、团战：其他角色的回合结束时，你可观看牌堆顶的x张牌，然后将这些牌交给任意角色（x为你本回合失去的牌数，且至多为5）。斗地主：其他角色的回合结束时，你可观看牌堆顶的x张牌，然后将这些牌交给任意角色（x为你本回合失去的牌数+1，且至多为5）。"},
        {textKey("谋·贾诩", "谋-完杀"), "一级：你的回合内，不处于濒死状态的其他角色不能使用【桃】。每轮限一次，一名角色进入濒死状态时，你可观看其手牌并选择其中的零至两张牌，然后其须选择一项：1、由你将被选择的牌分配给其以外的角色；2、弃置所有未被选择的牌。二级：你的回合内，不处于濒死状态的其他角色不能使用【桃】。每轮限一次，一名角色进入濒死状态时，你可观看其手牌并选择其区域内的零至两张牌，然后其须选择一项：1、由你将被选择的牌分配给其以外的角色；2、弃置所有未被选择的牌。"},
        {textKey("谋·贾诩", "谋-乱武"), "限定技，出牌阶段，你可令所有其他角色除非对各自距离最小的另一名其他角色使用一张【杀】，否则失去1点体力。每有一名角色因此失去体力时，你便可以选择“完杀”、“帷幕”中的一个进行升级。"},
        {textKey("谋·贾诩", "谋-帷幕"), "一级：锁定技，你成为黑色锦囊牌的目标时，取消之。二级：锁定技，你成为黑色锦囊牌的目标时，取消之。每轮开始时，若你上一轮成为其他角色使用牌的目标的次数不大于一次，则你从弃牌堆随机获得一张黑色锦囊牌或防具牌。"},
        {textKey("谋·诸葛瑾", "谋-缓释"), "当一名角色的判定牌生效前，你可以观看牌堆顶的一张牌，然后你可以用此牌代替之，或用手牌中的一张替换之。"},
        {textKey("谋·诸葛瑾", "谋-弘援"), "军争：蓄力技（1/3）。当你一次获得不少于两张牌时，你可以消耗1点蓄力点令至多两名角色各摸一张牌。当一名其他角色一次失去不少于两张牌时，你可以消耗1点蓄力点令其摸一张牌。排位、斗地主：蓄力技（1/3）。当你一次获得不少于两张牌时，你可以消耗1点蓄力点令至多两名角色各摸一张牌。当一名其他角色一次失去不少于两张牌时，你可以消耗1点蓄力点令其摸两张牌。"},
        {textKey("谋·诸葛瑾", "谋-明哲"), "锁定技，每轮限两次。当你于回合外失去牌时，你选择一名角色，若其有蓄力技，则其获得1点蓄力点；若你失去的牌中有非基本牌，则其摸一张牌。"},
        {textKey("谋·吕布", "谋-无双"), "锁定技，你使用的【杀】需两张【闪】才能抵消；与你进行【决斗】的角色每次需打出两张【杀】。每回合限一次，若对方没有使用或打出【杀】或【闪】，则此【杀】或【决斗】对其造成的伤害+1。"},
        {textKey("谋·吕布", "谋-利驭"), "当你使用【杀】对一名其他角色造成伤害后，你可以获得其区域里的至多等同于伤害数张牌，然后其摸等量张牌。若你与其因此获得了全部类别的牌，其选择一项：令你视为对由其指定的另一名其他角色使用一张【决斗】；其获得技能“无双”直至其下个回合结束。"},
        {textKey("谋·朱然", "谋-镇围"), "出牌阶段限一次，你可与一名其他角色同时选择是否弃置任意张牌。然后你可执行至多X项（X为你弃置牌大于等于其的条件数：1.牌数；2.花色数）：1.对其造成1点伤害；2.摸三张牌。"},
        {textKey("谋·朱然", "谋-合援"), "每名角色限一次，结束阶段，你可选择一名已受伤角色并弃置X张牌（X为你上次发动镇围时弃置的牌数），令其执行上次“镇围”执行的最后一项，且此后你对除其以外的角色发动“镇围”时，该角色也可选择弃置牌（视为你弃置的牌）。"},

        // ---------------- 势·小乔（势包） ----------------
        {textKey("势·小乔", "势-合韵"), "出牌阶段限两次，你可选择一名与你有相同技能的角色，然后你失去一个技能并令其摸两张牌。"},
        {textKey("势·小乔", "势-音洄"), "每轮开始时，你可清除因此获得的技能，然后你选择一名其他角色当前拥有的一个技能获得之。"},
    };
    auto it = table.find(textKey(hero, skill));
    if (it == table.end()) {
        static const std::string fallback = "";
        return fallback;
    }
    return it->second;
}

// =====================================================================
//  通用辅助
// =====================================================================
namespace {

// 普通（非延时）且非伤害类锦囊。
bool isNonDamageNormalTrick(CardPtr card) {
    if (!card || card->getType() != CardType::TRICK) return false;
    switch (card->getSubType()) {
        case CardSubType::LE_BU_SI_SHU:
        case CardSubType::SHAN_DIAN:
        case CardSubType::BING_LIANG_CUN_DUAN:
            return false;
        case CardSubType::JUE_DOU:
        case CardSubType::NAN_MAN_RU_QIN:
        case CardSubType::WAN_JIAN_QI_FA:
        case CardSubType::JIE_DAO_SHA_REN:
        case CardSubType::HUO_GONG:
            return false;
        default:
            return true;
    }
}

// 任意锦囊牌（含延时锦囊：乐不思蜀/兵粮寸断/闪电）。
// 用户 2026-10-05：谋·陆逊【谦逊】“也可应延时锦囊”，官网原文正是“当一张**锦囊牌**对你生效时”。
bool isAnyTrick(CardPtr card) { return card && card->getType() == CardType::TRICK; }

// 延时锦囊牌的牌名（【谦逊】可以记录，但“移去牌名视为使用”只限普通锦囊牌）
bool isDelayedTrickName(const std::string& n) {
    return n == "乐不思蜀" || n == "兵粮寸断" || n == "闪电";
}

bool isNormalTrick(CardPtr card) {
    if (!card || card->getType() != CardType::TRICK) return false;
    switch (card->getSubType()) {
        case CardSubType::LE_BU_SI_SHU:
        case CardSubType::SHAN_DIAN:
        case CardSubType::BING_LIANG_CUN_DUAN:
            return false;
        default:
            return true;
    }
}

// 设置标记到指定值（Player 仅有增减接口）。
void setMark(Player& p, const std::string& key, int value) {
    int cur = p.getMark(key);
    if (cur != value) p.addMark(key, value - cur);
}

} // namespace

// =====================================================================
//  蜀
// =====================================================================

// ---------------- 谋·刘赪 ----------------

MouLueYingSkill::MouLueYingSkill()
    : TriggerSkill("谋-掠影", mouText("谋·刘赪", "谋-掠影")) {}

void MouLueYingSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY ||
        (engine.getCurrentPhase() == TurnPhase::PLAY && engine.getCurrentPlayer() && !engine.isPlayerTurn(self)) ||
        (ctx.source && ctx.source->getId() != self.getId()) || !ctx.target ||
        ctx.target->getId() == self.getId()) return;
    if (self.getMark("掠影得椎") >= 2) return;
    self.addMark("掠影得椎", 1);
    self.addMark("椎", 1);
    engine.logMessage("  【掠影】" + who(self) + " 获得一个“椎”标记。");
}

void MouLueYingSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext&) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY) return;
    if (self.getMark("椎") < 2) return;
    self.addMark("椎", -2);
    PlayerPtr me = selfPtr(engine, self);
    engine.drawCards(me, 1, "掠影");

    // “一名角色”本身不排除自己，但实体【过河拆桥】只能指定其他角色，
    // 且目标必须有区域牌并能成为此锦囊的目标。
    auto guohe = Card::makeVirtual("过河拆桥", CardType::TRICK,
                                    CardSubType::GUO_HE_CHAI_QIAO, {}, "掠影");
    std::vector<PlayerPtr> cands;
    for (const auto& p : engine.getOtherAlivePlayers(self))
        if (!p->getAllCards().empty() && engine.canBeTargeted(p, guohe, me))
            cands.push_back(p);
    if (cands.empty()) return;

    PlayerPtr ai;
    for (auto& p : cands) if (!AIController::isFriend(engine, self, *p)) { ai = p; break; }
    if (!ai) ai = cands.front();
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【掠影】选择一名角色视为对其使用【过河拆桥】", true, ai);
    if (!t) return;
    engine.logMessage("  【掠影】视为对 " + who(*t) + " 使用一张【过河拆桥】。");
    engine.useCard(me, guohe, {t});
}

void MouLueYingSkill::onCardResolved(GameEngine&, Player&, CardPtr) {}

void MouLueYingSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) setMark(self, "掠影得椎", 0);
}

MouYingWuSkill::MouYingWuSkill()
    : TriggerSkill("谋-莺舞", mouText("谋·刘赪", "谋-莺舞")) {}

void MouYingWuSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return;
    if (!isNonDamageNormalTrick(card)) return;
    if (self.getMark("莺舞得椎") >= 2) return;
    self.addMark("莺舞得椎", 1);
    self.addMark("椎", 1);
    engine.logMessage("  【莺舞】" + who(self) + " 获得一个“椎”标记。");
    if (self.getMark("椎") < 2) return;
    self.addMark("椎", -2);
    PlayerPtr me = selfPtr(engine, self);
    engine.drawCards(me, 1, "莺舞");

    // “一名角色”不额外排除自己；但作为普通【杀】，必须服从杀的
    // 距离、目标免疫等规则，因此候选直接取引擎的合法杀目标集。
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "莺舞");
    auto cands = engine.getShaTargets(self, sha);
    if (cands.empty()) return;
    PlayerPtr ai;
    for (const auto& p : cands)
        if (!AIController::isFriend(engine, self, *p)) { ai = p; break; }
    if (!ai) ai = cands.front();
    PlayerPtr t = engine.askChoosePlayer(me, cands,
        "【莺舞】选择一名角色视为对其使用一张不受次数限制的普通【杀】", true, ai);
    if (!t) return;
    engine.logMessage("  【莺舞】视为对 " + who(*t) + " 使用一张不受次数限制的普通【杀】。");
    engine.useCard(me, sha, {t});
}

void MouYingWuSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) setMark(self, "莺舞得椎", 0);
}

// ---------------- 谋·吕蒙 ----------------

MouKeJiSkill::MouKeJiSkill()
    : ActiveSkill("谋-克己", mouText("谋·吕蒙", "谋-克己")) {}

bool MouKeJiSkill::canActivate(GameEngine&, Player& self) {
    bool juedu = self.getMark("渡江已觉醒") > 0;
    if (juedu) return !(usedDiscard && usedLose);
    return !(usedDiscard && usedLose); // 各限一次：两项都用过才不能发动
}

void MouKeJiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<std::string> options;
    std::vector<int> ids;
    if (!usedDiscard && self.getHandCardCount() > 0) { options.push_back("弃置一张手牌，获得1点“护甲”"); ids.push_back(1); }
    if (!usedLose) { options.push_back("流失1点体力，获得2点护甲"); ids.push_back(2); }
    if (options.empty()) return;
    int opt = engine.askChooseOption(me, options, "【克己】选择一项", 0);
    if (ids[opt] == 1) {
        CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【克己】弃置一张手牌", false,
                                         AIController::chooseLeastValuableCard(self.getHandCards()));
        if (!c) return;
        engine.discardCardOf(me, c, "克己");
        self.addMark("护甲", 1);
        usedDiscard = true;
        engine.logMessage("  【克己】" + who(self) + " 获得1点“护甲”（现 " + std::to_string(self.getMark("护甲")) + "）。");
    } else {
        engine.loseHp(me, 1, "克己");
        if (!self.isAlive()) return;
        self.addMark("护甲", 2);
        usedLose = true;
        engine.logMessage("  【克己】" + who(self) + " 获得2点护甲（现 " + std::to_string(self.getMark("护甲")) + "）。");
    }
    // 若已发动过“渡江”则出牌阶段限一次。
    if (self.getMark("渡江已觉醒") > 0) { usedDiscard = true; usedLose = true; }
}

bool MouKeJiSkill::aiShouldActivate(GameEngine&, Player& self) {
    return self.getHandCardCount() > 2 || self.isWounded();
}

void MouKeJiSkill::onCalculateHandLimit(GameEngine&, const Player& self, int& handLimit) {
    handLimit += std::max(0, self.getMark("护甲"));
}

bool MouKeJiSkill::onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr&) {
    // 若你不处于濒死状态，你无法使用【桃】。
    return false; // 桃限制在 GameEngine::useCard 中按 MouKeJiSkill 判定
}

MouDuJiangSkill::MouDuJiangSkill()
    : TriggerSkill("谋-渡江", mouText("谋·吕蒙", "谋-渡江"), SkillTag::AWAKEN) {}

void MouDuJiangSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION) return;
    if (self.getMark("渡江已觉醒") > 0) return;
    if (self.getMark("护甲") < 3) return;
    self.addMark("渡江已觉醒", 1);
    if (self.getHero())
        self.getHero()->addSkill(std::make_shared<MouDuoJingSkill>());
    engine.logMessage("  【渡江】" + who(self) + " 觉醒，获得技能【夺荆】！");
}

MouDuoJingSkill::MouDuoJingSkill()
    : TriggerSkill("谋-夺荆", mouText("谋·吕蒙", "谋-夺荆")) {}

void MouDuoJingSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    (void)engine;
    if (phase == TurnPhase::PLAY) setMark(self, "夺荆次数", 0);
}
void MouDuoJingSkill::onCalculateShaLimit(GameEngine&, const Player& self, int& shaLimit) {
    int add = self.getMark("夺荆次数");
    if (add > 0) shaLimit += add;
}
void MouDuoJingSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (self.getMark("护甲") <= 0 || !ctx.target) return;
    if (ctx.target->getId() == self.getId()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr tgt = engine.getPlayerById(ctx.target->getId());
    if (!engine.askConfirm(me, "【夺荆】失去1点护甲，令此【杀】无视防具并获得其一张牌、杀次数+1？",
                           true)) return;
    self.addMark("护甲", -1);
    self.addMark("无前目标:" + std::to_string(tgt->getId()), 1); // 无视防具（复用现有机制，结算后清除）
    self.addMark("夺荆次数", 1);
    // 获得目标一张牌
    if (tgt->getHandCardCount() > 0) {
        CardPtr c = AIController::chooseLeastValuableCard(tgt->getHandCards());
        engine.obtainCard(me, c, tgt);
    } else {
        auto eq = tgt->getAllEquipment();
        if (!eq.empty() && engine.canMoveFieldCard(tgt, me, eq.front()))
            engine.moveFieldCard(tgt, me, eq.front());
    }
    engine.logMessage("  【夺荆】" + who(self) + " 失去1点护甲，获得 " + who(*tgt) + " 一张牌。");
}

// ---------------- 谋·黄忠 ----------------

MouLieGongSkill::MouLieGongSkill()
    : TriggerSkill("谋-烈弓", mouText("谋·黄忠", "谋-烈弓")) {}

void MouLieGongSkill::onUseCard(GameEngine& engine, Player& self, CardPtr card) {
    if (!card) return;
    if (engine.effectiveSuit(self, card) != Suit::NONE) {
        std::string s = std::to_string((int)engine.effectiveSuit(self, card));
        if (self.getMark("烈弓花色" + s) == 0) {
            self.addMark("烈弓花色" + s, 1);
        }
    }
}

void MouLieGongSkill::onCardTargetConfirmed(GameEngine& engine, Player& self, Player* source, CardPtr card,
                                             const std::vector<PlayerPtr>& targets) {
    if (!card || !source) return;
    if (source->getId() == self.getId()) {
        if (card->getSubType() == CardSubType::SHA)
            setMark(self, "烈弓唯一杀", targets.size() == 1 ? 1 : 0);
        return;
    }
    if (std::find_if(targets.begin(), targets.end(), [&](const PlayerPtr& p) {
            return p && p->getId() == self.getId();
        }) == targets.end()) return;
    Suit suit = engine.effectiveSuit(self, card);
    if (suit != Suit::NONE) {
        std::string key = "烈弓花色" + std::to_string((int)suit);
        if (self.getMark(key) == 0) self.addMark(key, 1);
    }
}

void MouLieGongSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (self.getMark("烈弓唯一杀") == 0) return;
    int recorded = 0;
    for (int i = 0; i <= (int)Suit::DIAMOND; i++) if (self.getMark("烈弓花色" + std::to_string(i)) > 0) recorded++;
    int x = std::max(0, recorded - 1);
    if (x == 0) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【烈弓】展示牌堆顶 " + std::to_string(x) + " 张牌并按花色结算？", true)) return;
    auto shown = engine.getDeck().peekTopCards(x);
    int bonus = 0;
    for (auto& c : shown) {
        if (!c) continue;
        int s = (int)c->getSuit();
        if (self.getMark("烈弓花色" + std::to_string(s)) > 0) bonus++;
    }
    ctx.extraDamage += bonus;
    if (bonus > 0 && ctx.target) {
        // 目标不能使用“烈弓”记录花色的牌响应此杀：记录其禁色至结算结束。
        std::string colors;
        for (int i = 0; i <= (int)Suit::DIAMOND; i++) if (self.getMark("烈弓花色" + std::to_string(i)) > 0) colors += std::to_string(i) + ",";
        setMark(*ctx.target, "烈弓禁色" + std::to_string(self.getId()), 1);
        engine.logMessage("  【烈弓】展示的牌中 " + std::to_string(bonus) + " 张与记录花色相同，此【杀】伤害+" + std::to_string(bonus) + "。");
    }
    self.addMark("烈弓已展示", 1);
}

void MouLieGongSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext&) {
    setMark(self, "烈弓唯一杀", 0);
    if (self.getMark("烈弓已展示") == 0) return;
    self.addMark("烈弓已展示", -self.getMark("烈弓已展示"));
    for (int i = 0; i <= (int)Suit::DIAMOND; i++) setMark(self, "烈弓花色" + std::to_string(i), 0);
}

// ---------------- 谋·华雄 ----------------

MouYaoWuSkill::MouYaoWuSkill()
    : TriggerSkill("谋-耀武", mouText("谋·华雄", "谋-耀武"), SkillTag::LOCK) {}

void MouYaoWuSkill::onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage,
                                   ShaElement, CardPtr cause) {
    if (!cause || cause->getSubType() != CardSubType::SHA || damage <= 0) return;
    int s = (int)cause->getSuit();
    bool red = (s == (int)Suit::HEART || s == (int)Suit::DIAMOND);
    if (red && source && source->isAlive()) {
        PlayerPtr src = engine.getPlayerById(source->getId());
        int opt = engine.askChooseOption(src, {"回复1点体力", "摸一张牌"}, "【耀武】选择一项", src->isWounded() ? 0 : 1);
        if (opt == 0) engine.recoverHp(src, 1, "耀武");
        else engine.drawCards(src, 1, "耀武");
    } else if (!red) {
        PlayerPtr me = selfPtr(engine, self);
        engine.drawCards(me, 1, "耀武");
    }
}

MouYangWeiSkill::MouYangWeiSkill()
    : ActiveSkill("谋-扬威", mouText("谋·华雄", "谋-扬威")) {}

bool MouYangWeiSkill::canActivate(GameEngine& engine, Player& self) {
    return engine.getCurrentPhase() == TurnPhase::PLAY && engine.isPlayerTurn(self) &&
           !usedThisTurn && !disablePending;
}

void MouYangWeiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    engine.drawCards(me, 2, "扬威");
    setMark(self, "威", 1);
    usedThisTurn = true;
    disablePending = true;
    engine.logMessage("  【扬威】" + who(self) + " 摸两张牌并获得“威”标记（至本阶段结束）。");
}

// 【扬威】摸两张牌并获得“威”，代价是本技能失效到下回合结束 → 手牌不多时才换牌。
bool MouYangWeiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return self.getHandCardCount() <= 4 || self.isWounded();
}

void MouYangWeiSkill::onCalculateShaLimit(GameEngine&, const Player& self, int& shaLimit) {
    if (self.getMark("威") > 0) shaLimit += 1;
}

void MouYangWeiSkill::onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target, CardPtr, bool& canTarget) {
    if (self.getMark("威") > 0 && engine.getCurrentPhase() == TurnPhase::PLAY)
        canTarget = true; // 拥有“威”标记的角色使用【杀】无距离限制
    (void)target;
}

void MouYangWeiSkill::onPhaseEnd(GameEngine&, Player& self, TurnPhase phase) {
    if (phase == TurnPhase::PLAY) setMark(self, "威", 0);
    if (phase == TurnPhase::FINISH && disablePending && !usedThisTurn) disablePending = false;
}

void MouYangWeiSkill::onTurnBoundary(GameEngine&, Player& self, Player& turnOwner, bool starting) {
    if (starting && turnOwner.getId() == self.getId()) usedThisTurn = false;
}


// ---------------- 谋·吕蒙（夺荆清理） ----------------

void MouDuoJingSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext& ctx) {
    (void)engine;
    if (ctx.target) setMark(self, "无前目标:" + std::to_string(ctx.target->getId()), 0);
}

// ---------------- 谋·杨婉 ----------------

MouMingXuanSkill::MouMingXuanSkill()
    : TriggerSkill("谋-暝眩", mouText("谋·杨婉", "谋-暝眩")) {}

void MouMingXuanSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return;
    if (self.getHandCardCount() == 0) return;
    std::vector<PlayerPtr> others = engine.getOtherAlivePlayers(self);
    if (others.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    // 选择数张牌：由玩家自选（随机在角色上而非手牌）
    std::vector<CardPtr> given;
    if (me->isAI()) {
        int want = std::min(2, self.getHandCardCount());
        for (int i = 0; i < want && self.getHandCardCount() > 0; ++i) {
            CardPtr c = AIController::chooseLeastValuableCard(self.getHandCards());
            if (!c) break;
            engine.loseHandCard(me, c);
            given.push_back(c);
        }
    } else {
        std::vector<CardPtr> hand = self.getHandCards();
        while (!hand.empty()) {
            std::string prompt = "【暝眩】选择一张要随机交给其他角色的牌（0=结束选择，已选" + std::to_string(given.size()) + "张）";
            CardPtr c = engine.askChooseCard(me, hand, prompt, true, nullptr);
            if (!c) break;
            // 从展示的手牌池移除并从真实手牌失去
            hand.erase(std::remove(hand.begin(), hand.end(), c), hand.end());
            engine.loseHandCard(me, c);
            given.push_back(c);
            if (hand.empty()) break;
            if (!engine.askConfirm(me, "继续选择下一张牌吗？", false)) break;
        }
    }
    if (given.empty()) return;
    // 随机交给其他角色：每张牌独立随机一名其他角色（使用对局 RNG，保证可复现）
    std::map<int, std::vector<CardPtr>> giveMap; // targetId -> cards
    for (auto& c : given) {
        std::uniform_int_distribution<size_t> dist(0, others.size() - 1);
        PlayerPtr tgt = others[dist(engine.getRng())];
        giveMap[tgt->getId()].push_back(c);
    }
    std::vector<PlayerPtr> distinctTargets;
    for (auto& kv : giveMap) {
        PlayerPtr tgt = engine.getPlayerById(kv.first);
        if (!tgt || !tgt->isAlive()) continue;
        for (auto& c : kv.second) engine.obtainCard(tgt, c, me);
        distinctTargets.push_back(tgt);
        engine.logMessage("  【暝眩】" + who(self) + " 将 " + std::to_string(kv.second.size()) + " 张牌随机交给了 " + who(*tgt) + "。");
    }
    // 每名获得牌的角色依次选择一项
    for (auto tgt : distinctTargets) {
        if (!tgt->isAlive()) continue;
        const bool canUseSha = hasLegalShaOption(engine, tgt, me, false);
        std::vector<std::string> options;
        if (canUseSha) options.push_back("对 " + who(self) + " 使用一张【杀】");
        bool canGive = tgt->getHandCardCount() > 0;
        if (canGive) options.push_back("交给其一张牌，且你摸一张牌");
        else if (!canUseSha) continue; // 无可行选项
        int def = (!canUseSha || !canGive) ? 0 : (AIController::isFriend(engine, *tgt, self) ? 1 : 0);
        // 若仅一项可行，def 为 0 对应该项
        int optIdx = 0;
        if (options.size() == 2) {
            optIdx = engine.askChooseOption(tgt, options, "【暝眩】选择一项", def);
            // options 顺序与 canUseSha/canGive 相关，需映射回真实分支
            bool pickedSha = false;
            if (canUseSha && canGive) pickedSha = (optIdx == 0);
            else if (canUseSha) pickedSha = true;
            else pickedSha = false;
            if (pickedSha) {
                CardPtr sha = engine.askUseSha(tgt, "【暝眩】对 " + who(self) + " 使用一张【杀】",
                                               !AIController::isFriend(engine, *tgt, self), me, false, false);
                if (sha) engine.resolveSha(tgt, sha, {me});
                else {
                    // 未能出杀则 fallback 为交牌
                    if (canGive && tgt->getHandCardCount() > 0) {
                        CardPtr c = AIController::chooseLeastValuableCard(tgt->getHandCards());
                        if (c) engine.obtainCard(me, c, tgt);
                    }
                    engine.drawCards(tgt, 1, "暝眩");
                }
            } else {
                if (tgt->getHandCardCount() > 0) {
                    CardPtr c = engine.askChooseCard(tgt, tgt->getHandCards(), "【暝眩】交给 " + who(self) + " 一张牌", false,
                                                     AIController::chooseLeastValuableCard(tgt->getHandCards()));
                    if (c) engine.obtainCard(me, c, tgt);
                }
                engine.drawCards(tgt, 1, "暝眩");
            }
        } else {
            // 仅一项
            if (canUseSha) {
                CardPtr sha = engine.askUseSha(tgt, "【暝眩】对 " + who(self) + " 使用一张【杀】",
                                               !AIController::isFriend(engine, *tgt, self), me, false, false);
                if (sha) engine.resolveSha(tgt, sha, {me});
            } else {
                if (tgt->getHandCardCount() > 0) {
                    CardPtr c = engine.askChooseCard(tgt, tgt->getHandCards(), "【暝眩】交给 " + who(self) + " 一张牌", false,
                                                     AIController::chooseLeastValuableCard(tgt->getHandCards()));
                    if (c) engine.obtainCard(me, c, tgt);
                }
                engine.drawCards(tgt, 1, "暝眩");
            }
        }
    }
}

MouXianChouSkill::MouXianChouSkill()
    : TriggerSkill("谋-陷仇", mouText("谋·杨婉", "谋-陷仇")) {}

void MouXianChouSkill::onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage,
                                     ShaElement, CardPtr) {
    if (damage <= 0 || !source || source == &self || !source->isAlive()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr src = engine.getPlayerById(source->getId());
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "陷仇");
    std::vector<PlayerPtr> others;
    for (const auto& candidate : engine.getOtherAlivePlayers(self)) {
        if (!candidate || candidate->getId() == src->getId() ||
            candidate->getHandAndEquipmentCards().empty()) continue;
        if (!engine.canUseShaOn(*candidate, *src, sha) || !engine.canBeTargeted(src, sha, candidate)) continue;
        others.push_back(candidate);
    }
    if (others.empty()) return;
    PlayerPtr actor = engine.askChoosePlayer(me, others, "【陷仇】选择一名能对伤害来源使用【杀】的角色", true);
    if (!actor) return;
    auto discardable = actor->getHandAndEquipmentCards();
    CardPtr cost = engine.askChooseCard(actor, discardable, "【陷仇】弃一张牌视为对 " + who(*src) + " 使用【杀】", true,
                                        AIController::chooseLeastValuableCard(discardable));
    if (!cost) return;
    engine.discardCardOf(actor, cost, "陷仇");
    engine.logMessage("  【陷仇】" + who(*actor) + " 弃一张牌，视为对 " + who(*src) + " 使用一张【杀】。");
    int hpBefore = src->getHp();
    engine.useCard(actor, sha, {src});
    if (src->isAlive() && src->getHp() < hpBefore) engine.recoverHp(me, 1, "陷仇");
}

// ---------------- 谋·马超 ----------------

MouTieQiSkill::MouTieQiSkill()
    : TriggerSkill("谋-铁骑", mouText("谋·马超", "谋-铁骑")) {}

void MouTieQiSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId() || !ctx.target) return;
    if (ctx.target->getId() == self.getId()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr tgt = engine.getPlayerById(ctx.target->getId());
    if (!me || !tgt) return;
    if (!engine.askConfirm(me, "【铁骑】是否令 " + who(*tgt) + " 非锁定技失效、不能使用【闪】，并进行谋弈？", true))
        return;
    tgt->setNonLockSkillsDisabled(true);
    ctx.cannotDodge = true;
    engine.logMessage("  【铁骑】" + who(*tgt) + " 本回合内非锁定技失效，不能使用【闪】响应此【杀】。");
    mouYiDuel(engine, self, *tgt);
}

// ---------------- 谋·张飞 ----------------

MouPaoXiaoSkill::MouPaoXiaoSkill()
    : StateSkill("谋-咆哮", mouText("谋·张飞", "谋-咆哮"), SkillTag::LOCK) {}

void MouPaoXiaoSkill::onCalculateShaLimit(GameEngine&, const Player&, int& shaLimit) {
    shaLimit = 99; // 使用【杀】无次数限制
}

void MouPaoXiaoSkill::onCheckShaTarget(GameEngine&, const Player& self, const Player&, CardPtr, bool& canTarget) {
    if (self.getWeapon()) canTarget = true; // 装备武器时【杀】无距离限制
}

void MouPaoXiaoSkill::onShaTargeted(GameEngine&, Player& self, ShaContext& ctx) {
    // useCard 在合法目标确认后才递增“本回合用过杀”；因此 >=2 正好表示当前阶段不是第一张杀。
    if (!ctx.source || ctx.source->getId() != self.getId() ||
        ctx.source->getMark("本回合用过杀") < 2) return;
    if (ctx.target && !ctx.target->isNonLockSkillsDisabled()) {
        ctx.target->addMark("谋张飞封技", 1);
        ctx.target->setNonLockSkillsDisabled(true);
    }
    ctx.cannotDodge = true;
}

void MouPaoXiaoSkill::onCalculateShaDamage(GameEngine&, const Player& self, const Player& source,
                                            const Player& target, CardPtr, int& damage) {
    if (source.getId() != self.getId() || source.getMark("本回合用过杀") < 2) return;
    if (target.getMark("谋张飞封技") > 0) damage += 1;
}

void MouPaoXiaoSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.source || ctx.source->getId() != self.getId()) return;
    const bool strengthened = self.getMark("本回合用过杀") >= 2;
    // 非锁定技失效为“本回合内”效果，由回合结束时的 clearTurnEffects 统一恢复；
    // 此处仅清除用于本次【杀】伤害+1 的临时标记。
    if (ctx.target && ctx.target->getMark("谋张飞封技") > 0)
        ctx.target->addMark("谋张飞封技", -ctx.target->getMark("谋张飞封技"));
    if (!strengthened || !ctx.hit || !ctx.target || !ctx.target->isAlive()) return;
    engine.loseHp(selfPtr(engine, self), 1, "谋·咆哮惩罚", selfPtr(engine, self));
    if (self.isAlive() && self.getHandCardCount() > 0) {
        auto hand = self.getHandCards();
        std::uniform_int_distribution<size_t> pick(0, hand.size() - 1);
        engine.discardCardOf(selfPtr(engine, self), hand[pick(engine.getRng())], "谋·咆哮惩罚");
    }
}

MouXieJiSkill::MouXieJiSkill()
    : ActiveSkill("谋-协击", mouText("谋·张飞", "谋-协击")) {}

bool MouXieJiSkill::canActivate(GameEngine& engine, Player& self) {
    if (self.getMark("协击成功") == 0) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY) return false;
    return engine.getShaLimit(self) > 0;
}

void MouXieJiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<PlayerPtr> cands;
    for (auto& p : engine.getOtherAlivePlayers(self)) cands.push_back(p);
    std::vector<PlayerPtr> targets;
    // 逐次询问，且**每次把已选中的目标从候选中剔除**：
    // AI 的默认选择恒为候选首位，若候选不变会永远选到同一人而陷入死循环
    // （回归来源：2026-10-05 斗地主 seed=131，谋·张飞【协击】协力成功后卡死）。
    while (targets.size() < 3) {
        std::vector<PlayerPtr> rest;   // 尚未被选中的候选
        for (auto& c : cands)
            if (std::find(targets.begin(), targets.end(), c) == targets.end()) rest.push_back(c);
        if (rest.empty()) break;
        // AI 默认：按【杀】的目标评分挑敌方（可击杀优先），不打自己人；没有敌方目标就收手。
        PlayerPtr aiPick = AIController::chooseShaTarget(engine, self, rest, 1);
        if (me->isAI() && !aiPick) break;
        PlayerPtr t = engine.askChoosePlayer(me, rest,
                                            targets.empty() ? "【协击】选择一名目标（0=结束）"
                                                            : "【协击】再选择一名目标（0=结束）",
                                            true, aiPick);
        if (!t) break;
        if (std::find(targets.begin(), targets.end(), t) != targets.end()) break; // 保险：重复即止
        targets.push_back(t);
    }
    if (targets.empty()) return;
    engine.logMessage("  【协击】" + who(self) + " 依次对 " + std::to_string(targets.size()) + " 名角色视为使用普通【杀】。");
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "协击");
    for (auto& tgt : targets) {
        if (!tgt || !tgt->isAlive()) continue;   // 原实现误用循环外的 t（可能为空）→ 已修正
        int hpBefore = tgt->getHp();
        engine.useCard(me, sha, {tgt});
        int dealt = std::max(0, hpBefore - tgt->getHp());
        if (dealt > 0) engine.drawCards(me, dealt, "协击造成伤害");
    }
    self.addMark("协击成功", -self.getMark("协击成功"));
}

// 只有确实存在可视为使用【杀】的敌方目标时才发动，避免 AI 空转（原为恒 true）。
bool MouXieJiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    std::vector<PlayerPtr> rest;
    for (auto& p : engine.getOtherAlivePlayers(self)) rest.push_back(p);
    return AIController::chooseShaTarget(engine, self, rest, 1) != nullptr;
}

void MouXieJiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION) return;
    if (!engine.isPlayerTurn(self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【协击】准备阶段：选择一名角色进行“协力”", true);
    if (!t) return;
    engine.startXieLi(me, t, "协击");
}

void MouXieJiSkill::onTurnEnd(GameEngine& engine, Player& self, Player& turnOwner) {
    (void)engine; (void)self; (void)turnOwner;
    // 协力结算由 GameEngine::resolveXieLiAtTurnEnd 统一处理并设置“协击成功”标记
}

// ---------------- 谋·赵云 ----------------

MouLongDanSkill::MouLongDanSkill()
    : ActiveSkill("谋-龙胆", mouText("谋·赵云", "谋-龙胆")) {}

CardPtr MouLongDanSkill::convertCard(GameEngine&, Player& self, CardPtr card, CardSubType wanted) {
    if (!card || card->isVirtual()) return nullptr;
    int available = 1 + self.getMark("龙胆上限成长"); // 初始1
    if (self.getMark("龙胆已用") >= available) return nullptr;
    bool upgraded = self.getMark("龙胆强化") > 0;
    if (upgraded) {
        // 强化：一张基本牌可转化为另一种基本牌；枚举候选时不消耗次数或摸牌。
        if (card->getType() != CardType::BASIC || wanted == card->getSubType()) return nullptr;
        switch (wanted) {
            case CardSubType::SHA:
                return Card::makeVirtual("杀", CardType::BASIC, wanted, {card}, name);
            case CardSubType::SHAN:
                return Card::makeVirtual("闪", CardType::BASIC, wanted, {card}, name);
            case CardSubType::TAO:
                return Card::makeVirtual("桃", CardType::BASIC, wanted, {card}, name);
            case CardSubType::JIU:
                return Card::makeVirtual("酒", CardType::BASIC, wanted, {card}, name);
            default:
                return nullptr;
        }
    }
    if (card->getSubType() == CardSubType::SHA && wanted == CardSubType::SHAN)
        return Card::makeVirtual("闪", CardType::BASIC, wanted, {card}, name);
    if (card->getSubType() == CardSubType::SHAN && wanted == CardSubType::SHA)
        return Card::makeVirtual("杀", CardType::BASIC, wanted, {card}, name);
    return nullptr;
}

void MouLongDanSkill::onCardPlayed(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getSkillSource() != name) return;
    self.addMark("龙胆已用", 1);
    engine.drawCards(selfPtr(engine, self), 1, "龙胆");
}

void MouLongDanSkill::onCardResponded(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getSkillSource() != name) return;
    self.addMark("龙胆已用", 1);
    engine.drawCards(selfPtr(engine, self), 1, "龙胆");
}

void MouLongDanSkill::onTurnBoundary(GameEngine&, Player& self, Player&, bool starting) {
    if (starting) return;
    if (self.getMark("龙胆上限成长") < 2) self.addMark("龙胆上限成长", 1);
    setMark(self, "龙胆已用", 0);
    if (self.getMark("龙胆强化临时") > 0) {
        setMark(self, "龙胆强化", 0);
        setMark(self, "龙胆强化临时", 0);
    }
}

MouJiZhuSkill::MouJiZhuSkill()
    : ActiveSkill("谋-积著", mouText("谋·赵云", "谋-积著")) {}

bool MouJiZhuSkill::canActivate(GameEngine& engine, Player& self) {
    if (self.getMark("积著已发动") > 0) return false;
    return engine.isPlayerTurn(self) && engine.getCurrentPhase() == TurnPhase::PREPARATION;
}

void MouJiZhuSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【积著】选择一名角色进行“协力”", false);
    if (!t) return;
    setMark(self, "积著已发动", 1);
    setMark(self, "积著目标", t->getId() + 1);
    engine.startXieLi(me, t, "积著");
    engine.logMessage("  【积著】" + who(self) + " 与 " + who(*t) + " 进行“协力”（四类任一达标即成功：伤害≥4/摸牌≥8/弃4花色/用4花色）。");
}

// 【积著】与人“协力”成功可升级【龙胆】→ 场上还有别的角色就值得发动（无费用）。
bool MouJiZhuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return !engine.getOtherAlivePlayers(self).empty();
}

void MouJiZhuSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION || !canActivate(engine, self)) return;
    if (engine.askConfirm(selfPtr(engine, self), "【积著】是否选择一名其他角色进行协力？", true))
        activate(engine, self);
}

void MouJiZhuSkill::onTurnEnd(GameEngine& engine, Player& self, Player& turnOwner) {
    if (self.getMark("积著已发动") == 0) return;
    int tid = self.getMark("积著目标") - 1;
    if (turnOwner.getId() != tid) return;
    if (self.getMark("积著协力成功") > 0) {
        setMark(self, "龙胆强化", 1);
        setMark(self, "龙胆强化临时", 1);
        engine.logMessage("  【积著】协力成功！" + who(self) + " 的【龙胆】被强化至你的下个回合结束后。");
        self.addMark("积著协力成功", -self.getMark("积著协力成功"));
    } else {
        engine.logMessage("  【积著】协力失败。");
    }
    setMark(self, "积著已发动", 0);
}

// ---------------- 谋·孙尚香 ----------------

MouJieYinSkill::MouJieYinSkill()
    : TriggerSkill("谋-结姻", mouText("谋·孙尚香", "谋-结姻")) {}

void MouJieYinSkill::onGameStart(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【结姻】选择一名其他角色获得“助”标记", false);
    if (t) {
        setMark(*t, "助", 1);
        t->addMark("助次数", 1);
        engine.logMessage("  【结姻】" + who(*t) + " 获得“助”标记。");
    }
}

void MouJieYinSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return;
    // 找到有“助”标记的角色，由其选择
    for (auto& p : engine.getPlayers()) {
        if (!p->isAlive() || p->getMark("助") == 0) continue;
        int opt = engine.askChooseOption(p, {"交给你两张手牌并获得1点护甲", "令你移动或移除助标记"},
                                         "【结姻】选择一项", p->getHandCardCount() >= 2 ? 0 : 1);
        if (opt == 0) {
            int give = std::min(2, p->getHandCardCount());
            for (int i = 0; i < give; i++) {
                CardPtr c = AIController::chooseLeastValuableCard(p->getHandCards());
                if (!c) break;
                engine.obtainCard(selfPtr(engine, self), c, p);
            }
            p->addMark("护甲", 1);
        } else {
            bool moved = false;
            if (p->getMark("助次数") <= 1) {
                auto destinations = engine.getOtherAlivePlayers(*p);
                PlayerPtr destination = engine.askChoosePlayer(p, destinations,
                    "【结姻】选择一名角色移动“助”标记（取消则移除）", true);
                if (destination) {
                    setMark(*p, "助", 0);
                    setMark(*destination, "助", 1);
                    destination->addMark("助次数", 1);
                    moved = true;
                    engine.logMessage("  【结姻】“助”标记移动至" + who(*destination) + "。");
                }
            }
            if (!moved) {
                setMark(*p, "助", 0);
                engine.logMessage("  【结姻】“助”标记被移除。");
                // 使命失败：当“助”标记被移除时，获得全部“妆”牌。
                if (self.getMark("结姻失败处理") == 0 && p->getMark("助次数") > 0) {
                    setMark(self, "结姻失败处理", 1);
                    auto makeup = self.getPile("妆");
                    for (const auto& card : makeup) {
                        self.removeFromPile("妆", card);
                        self.addHandCard(card);
                    }
                    setMark(self, "妆", 0);
                    engine.recoverHp(selfPtr(engine, self), 1, "结姻");
                    engine.logMessage("  【结姻】使命失败：获得全部“妆”牌，回复1点体力，势力修改为“吴”，减1点体力上限。");
                    self.changeMaxHp(-1);
                    if (self.getHero()) self.getHero()->setAvatarIdentity(Country::WU, self.getHero()->getGender());
                }
            }
        }
        break;
    }
}

void MouJieYinSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool) {}

MouLiangZhuSkill::MouLiangZhuSkill()
    : ActiveSkill("谋-良助", mouText("谋·孙尚香", "谋-良助")) {}

bool MouLiangZhuSkill::canActivate(GameEngine& engine, Player& self) {
    if (self.getMark("良助已用") > 0) return false;
    if (self.getHero() && self.getHero()->getCountry() != Country::SHU) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    for (auto& p : engine.getPlayers())
        if (p->isAlive() && p->getId() != self.getId() && !p->getAllEquipment().empty()) return true;
    return false;
}

void MouLiangZhuSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> eqs;
    PlayerPtr owner;
    for (auto& p : engine.getPlayers()) {
        if (p == me || !p->isAlive()) continue;
        auto eq = p->getAllEquipment();
        if (!eq.empty()) { owner = p; eqs.insert(eqs.end(), eq.begin(), eq.end()); }
    }
    if (eqs.empty()) return;
    CardPtr c = engine.askChooseCard(me, eqs, "【良助】选择一张装备牌置于你的武将牌上（称为“妆”）", false,
                                     AIController::chooseMostValuableCard(eqs));
    if (!c) return;
    PlayerPtr cOwner = owner;
    for (auto& pp : engine.getPlayers()) {
        auto eq2 = pp->getAllEquipment();
        if (std::find(eq2.begin(), eq2.end(), c) != eq2.end()) { cOwner = pp; break; }
    }
    setMark(self, "良助已用", 1);
    // 置入“妆”堆
    if (cOwner && cOwner->getHandCardCount() > 0 && std::find(cOwner->getHandCards().begin(), cOwner->getHandCards().end(), c) != cOwner->getHandCards().end())
        engine.loseHandCard(cOwner, c);
    else if (cOwner) engine.moveFieldCard(cOwner, selfPtr(engine, self), c);
    self.addToPile("妆", c);
    self.addMark("妆", 1);
    engine.logMessage("  【良助】" + who(self) + " 获得“妆”牌。");
    PlayerPtr helper;
    for (auto& p : engine.getPlayers())
        if (p->isAlive() && p->getMark("助") > 0) { helper = p; break; }
    if (helper) {
        int opt = engine.askChooseOption(helper, {"回复1点体力", "摸两张牌"}, "【良助】选择一项", helper->isWounded() ? 0 : 1);
        if (opt == 0) engine.recoverHp(helper, 1, "良助");
        else engine.drawCards(helper, 2, "良助");
    }
}

// 【良助】把别人装备区的牌变成自己的“妆”，再让有“助”标记者回血/摸牌：
// 只在**敌方有装备**（拆装备）或**有人带“助”标记**（能吃到收益）时发动，不白拆队友装备。
bool MouLiangZhuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    for (const auto& p : engine.getOtherAlivePlayers(self)) {
        if (!p->getAllEquipment().empty() && !AIController::isFriend(engine, self, *p)) return true;
        if (p->getMark("助") > 0) return true;
    }
    return false;
}

MouXiaoJiSkill::MouXiaoJiSkill()
    : TriggerSkill("谋-枭姬", mouText("谋·孙尚香", "谋-枭姬")) {}

void MouXiaoJiSkill::onEquipmentLost(GameEngine& engine, Player& self, CardPtr) {
    if (self.getHero() && self.getHero()->getCountry() != Country::WU) return;
    engine.drawCards(selfPtr(engine, self), 2, "枭姬");
    PlayerPtr me = selfPtr(engine, self);
    // 可以弃置场上的一张牌
    std::vector<CardPtr> field;
    for (auto& p : engine.getPlayers()) {
        if (!p->isAlive()) continue;
        auto eq = p->getAllEquipment();
        field.insert(field.end(), eq.begin(), eq.end());
    }
    if (field.empty()) return;
    if (engine.askConfirm(me, "【枭姬】是否弃置场上一张牌？", true)) {
        CardPtr c = AIController::chooseLeastValuableCard(field);
        if (c) {
            PlayerPtr owner;
        for (auto& pp : engine.getPlayers()) {
            auto eq2 = pp->getAllEquipment();
            if (std::find(eq2.begin(), eq2.end(), c) != eq2.end()) { owner = pp; break; }
        }
            if (owner) engine.discardCardOf(owner, c, "枭姬");
        }
    }
}

// ---------------- 谋·夏侯氏 ----------------

MouYanYuSkill::MouYanYuSkill()
    : ActiveSkill("谋-燕语", mouText("谋·夏侯氏", "谋-燕语")) {}

bool MouYanYuSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (usesThisPhase >= 2) return false;
    for (auto& c : self.getHandCards()) if (c->getSubType() == CardSubType::SHA) return true;
    return false;
}

void MouYanYuSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> shas;
    for (auto& c : self.getHandCards()) if (c->getSubType() == CardSubType::SHA) shas.push_back(c);
    if (shas.empty()) return;
    CardPtr c = engine.askChooseCard(me, shas, "【燕语】弃置一张【杀】并摸一张牌", false, shas.front());
    if (!c) return;
    engine.discardCardOf(me, c, "燕语");
    usesThisPhase++;
    killedThisPhase++;
    engine.drawCards(me, 1, "燕语");
}

// 【燕语】弃一张【杀】摸一张（限两次），阶段结束再让别人摸 X 张：
// 【杀】有富余（≥2 张）或手牌溢出时才弃，避免把唯一的【杀】丢掉。
bool MouYanYuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    int sha = 0;
    for (const auto& c : self.getHandCards()) if (c && c->getSubType() == CardSubType::SHA) ++sha;
    return sha >= 2 || self.getHandCardCount() >= 5;
}

void MouYanYuSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) { usesThisPhase = 0; killedThisPhase = 0; }
}

void MouYanYuSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY || !engine.isPlayerTurn(self) || killedThisPhase == 0) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【燕语】令一名角色摸 " + std::to_string(killedThisPhase * 3) + " 张牌", false);
    if (t) engine.drawCards(t, killedThisPhase * 3, "燕语");
    killedThisPhase = 0;
}

MouQiaoShiSkill::MouQiaoShiSkill()
    : TriggerSkill("谋-樵拾", mouText("谋·夏侯氏", "谋-樵拾")) {}

void MouQiaoShiSkill::onTurnStart(GameEngine&, Player& self) {
    setMark(self, "樵拾本回合", 0);
}

void MouQiaoShiSkill::onAfterDamage(GameEngine& engine, Player& self, Player* source, int damage,
                                    ShaElement, CardPtr) {
    if (damage <= 0 || !source || source == &self || !source->isAlive()) return;
    if (self.getMark("樵拾本回合") > 0) return;
    PlayerPtr src = engine.getPlayerById(source->getId());
    if (engine.askConfirm(src, "【樵拾】令 " + who(self) + " 回复 " + std::to_string(damage) + " 点体力，你摸两张牌？", true)) {
        setMark(self, "樵拾本回合", 1);
        engine.recoverHp(selfPtr(engine, self), damage, "樵拾");
        engine.drawCards(src, 2, "樵拾");
    }
}


// ---------------- 谋弈（谋·马超【铁骑】） ----------------
// 官方机制：发动者与目标各从两项中选择一项（同时、互不可见）。
// 两人选择不同则发动者“攻城成功”，执行发动者所选项的效果；选择相同则谋弈失败、无事发生。
bool mouYiDuel(GameEngine& engine, Player& self, Player& target, int mineChoice, int theirsChoice) {
    PlayerPtr me = engine.getPlayerById(self.getId());
    PlayerPtr tgt = engine.getPlayerById(target.getId());
    if (!me || !tgt || !me->isAlive() || !tgt->isAlive()) return false;
    std::uniform_int_distribution<int> coin(0, 1);
    if (mineChoice < 0)
        mineChoice = engine.askChooseOption(me, {"直取敌营：你获得其一张牌", "扰阵疲敌：你摸两张牌"},
                                            "【铁骑】谋弈：选择你的选项", coin(engine.getRng()));
    if (theirsChoice < 0)
        theirsChoice = engine.askChooseOption(tgt, {"直取敌营", "扰阵疲敌"},
                                              "【铁骑】谋弈：选择一项（与发动者选择相同则其失败）", coin(engine.getRng()));
    mineChoice = (mineChoice == 0) ? 0 : 1;
    theirsChoice = (theirsChoice == 0) ? 0 : 1;
    if (mineChoice == theirsChoice) {
        engine.logMessage("  【铁骑】谋弈失败（双方选择相同），无事发生。");
        return false;
    }
    if (mineChoice == 0) {
        if (tgt->getAllCards().empty()) {
            engine.logMessage("  【铁骑】谋弈成功·直取敌营，但" + who(target) + "没有牌可获得。");
            return true;
        }
        CardPtr picked = engine.chooseCardFromPlayer(me, tgt, "【铁骑】直取敌营：选择获得其一张牌", false);
        if (picked) {
            engine.obtainCard(me, picked, tgt);
            engine.logMessage("  【铁骑】谋弈成功·直取敌营：" + who(self) + " 获得" + who(target) + "的一张牌。");
        }
        return true;
    }
    engine.drawCards(me, 2, "铁骑");
    engine.logMessage("  【铁骑】谋弈成功·扰阵疲敌：" + who(self) + " 摸两张牌。");
    return true;
}

// =====================================================================
//  续：吴
// =====================================================================

// ---------------- 谋·周瑜 ----------------

MouYingZiSkill::MouYingZiSkill()
    : StateSkill("谋-英姿", mouText("谋·周瑜", "谋-英姿"), SkillTag::LOCK) {}

void MouYingZiSkill::onDrawCards(GameEngine&, Player& self, int& drawCount) {
    int n = 0;
    if (self.getHandCardCount() >= 2) n++;
    if (self.getHp() >= 2) n++;
    int eq = (int)self.getAllEquipment().size();
    if (eq >= 1) n++;
    drawCount += n;
}

void MouYingZiSkill::onCalculateHandLimit(GameEngine& engine, const Player& self, int& handLimit) {
    if (!engine.isPlayerTurn(self)) return;
    int n = 0;
    if (self.getHandCardCount() >= 2) n++;
    if (self.getHp() >= 2) n++;
    int eq = (int)self.getAllEquipment().size();
    if (eq >= 1) n++;
    handLimit += n; // 本回合手牌上限+1×n（仅于技能持有者回合内生效）
}

MouFanJianSkill::MouFanJianSkill()
    : ActiveSkill("谋-反间", mouText("谋·周瑜", "谋-反间")) {}

bool MouFanJianSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("反间失效") > 0) return false;
    for (auto& c : self.getHandCards())
        if (self.getMark("反间花色" + std::to_string((int)c->getSuit())) == 0) return true;
    return false;
}

void MouFanJianSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> cands;
    for (auto& c : self.getHandCards())
        if (self.getMark("反间花色" + std::to_string((int)c->getSuit())) == 0) cands.push_back(c);
    if (cands.empty()) return;
    PlayerPtr tgt = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【反间】选择一名其他角色", false);
    if (!tgt) return;
    CardPtr card = engine.askChooseCard(me, cands, "【反间】扣置一张本回合未以此法扣置过花色的手牌", false, cands.front());
    if (!card) return;
    self.addMark("反间花色" + std::to_string((int)card->getSuit()), 1);
    int decl = engine.askChooseOption(me, {"声明红桃", "声明方块", "声明梅花", "声明黑桃"}, "【反间】声明一个花色", 0);
    engine.logMessage("  【反间】" + who(self) + " 扣置一张手牌并声明花色。");
    int choice = engine.askChooseOption(tgt, {"猜测与声明花色一致", "翻面且此技能失效直到回合结束"},
                                        "【反间】" + who(*tgt) + " 选择一项", 1);
    // 声明索引 0:红桃(HEART=1) 1:方块(DIAMOND=3) 2:梅花(CLUB=2) 3:黑桃(SPADE=0)
    int declSuitMap[4] = {(int)Suit::HEART, (int)Suit::DIAMOND, (int)Suit::CLUB, (int)Suit::SPADE};
    int declSuit = declSuitMap[std::max(0, std::min(3, decl))];
    (void)declSuit;
    engine.logMessage("  【反间】展示扣置牌为 " + card->getFormattedName() + "。");
    if (choice == 1) {
        tgt->setTurnedOver(!tgt->isTurnedOver());
        setMark(self, "反间失效", 1); // 失效至本回合结束；技能次回合开始时清除标记。
        engine.logMessage("  【反间】" + who(*tgt) + " 翻面，【反间】失效直到回合结束。");
    } else {
        int realSuit = (int)card->getSuit();
        int guessIsMatch = (declSuit == realSuit) ? 1 : 0;
        if (guessIsMatch) {
            setMark(self, "反间失效", 1);
            engine.logMessage("  【反间】猜对，此技能失效直到回合结束。");
        } else {
            engine.loseHp(tgt, 1, "反间");
        }
    }
    engine.obtainCard(tgt, card, me);
}

// 【反间】要有敌人可猜，且自己手里有能扣置的牌（canActivate 已保证花色未用过）。
bool MouFanJianSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return AIController::hasEnemy(engine, self) && self.getHandCardCount() >= 2;
}

void MouFanJianSkill::onTurnStart(GameEngine&, Player& self) {
    for (int suit = 0; suit < 4; ++suit)
        setMark(self, "反间花色" + std::to_string(suit), 0);
    setMark(self, "反间失效", 0);
}

// ---------------- 谋·貂蝉 ----------------

MouLiJianSkill::MouLiJianSkill()
    : ActiveSkill("谋-离间", mouText("谋·貂蝉", "谋-离间")) {}

bool MouLiJianSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("离间已用") > 0) return false;
    return engine.getOtherAlivePlayers(self).size() >= 2 &&
           !self.getHandAndEquipmentCards().empty();
}

void MouLiJianSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<PlayerPtr> others = engine.getOtherAlivePlayers(self);
    std::vector<CardPtr> discardable = self.getHandAndEquipmentCards();
    const size_t maxTargets = std::min(others.size(), discardable.size() + 1);
    std::vector<PlayerPtr> picked;
    while (picked.size() < maxTargets) {
        std::vector<PlayerPtr> candidates;
        for (const auto& p : others)
            if (std::find(picked.begin(), picked.end(), p) == picked.end()) candidates.push_back(p);
        if (candidates.empty()) break;
        PlayerPtr t = engine.askChoosePlayer(me, candidates,
            picked.empty() ? "【离间】选择至少两名角色（0=取消）" : "【离间】可再选择一名角色（0=结束）",
            true);
        if (!t) break;
        picked.push_back(t);
    }
    if (picked.size() < 2) return;
    const size_t cost = picked.size() - 1;
    std::vector<CardPtr> costs;
    while (costs.size() < cost && !discardable.empty()) {
        CardPtr c = engine.askChooseCard(me, discardable, "【离间】选择第 " +
            std::to_string(costs.size() + 1) + " 张弃置牌", false,
            AIController::chooseLeastValuableCard(discardable));
        if (!c) return;
        costs.push_back(c);
        discardable.erase(std::remove(discardable.begin(), discardable.end(), c), discardable.end());
    }
    if (costs.size() != cost) return;
    setMark(self, "离间已用", 1);
    for (const auto& card : costs) engine.discardCardOf(me, card, "离间");
    engine.logMessage("  【离间】" + who(self) + " 弃置 " + std::to_string(cost) + " 张牌。");

    // 每名角色依次对逆时针最近座次的另一名已选角色视为使用【决斗】。
    const auto& players = engine.getPlayers();
    for (const auto& p : picked) {
        if (!p || !p->isAlive()) continue;
        int seat = -1;
        for (size_t i = 0; i < players.size(); ++i)
            if (players[i] == p) { seat = static_cast<int>(i); break; }
        PlayerPtr duelTarget;
        for (int step = 1; seat >= 0 && step <= static_cast<int>(players.size()); ++step) {
            PlayerPtr candidate = players[(seat + step) % players.size()];
            if (candidate && candidate->isAlive() && candidate != p &&
                std::find(picked.begin(), picked.end(), candidate) != picked.end()) {
                duelTarget = candidate;
                break;
            }
        }
        if (!duelTarget) continue;
        auto duel = Card::makeVirtual("决斗", CardType::TRICK, CardSubType::JUE_DOU, {}, "离间");
        engine.useCard(p, duel, {duelTarget});
    }
}

// 【离间】弃 X 张牌（X＝所选人数-1）令他们互相【决斗】：
// 至少要能凑出两名**敌人**（让他们互砍），且弃完牌还剩得下防御牌。
bool MouLiJianSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return AIController::enemyCount(engine, self) >= 2 && self.getHandCardCount() >= 3;
}

void MouLiJianSkill::onTurnStart(GameEngine&, Player& self) {
    setMark(self, "离间已用", 0);
}

MouBiYueSkill::MouBiYueSkill()
    : TriggerSkill("谋-闭月", mouText("谋·貂蝉", "谋-闭月"), SkillTag::LOCK) {}

void MouBiYueSkill::onGlobalDamage(GameEngine&, Player& self, Player*, Player& target, int, CardPtr) {
    if (std::find(damagedThisRound.begin(), damagedThisRound.end(), target.getId()) == damagedThisRound.end())
        damagedThisRound.push_back(target.getId());
    (void)self;
}

void MouBiYueSkill::onTurnStart(GameEngine&, Player&) { damagedThisRound.clear(); }
void MouBiYueSkill::onRoundStart(GameEngine&, Player&) { damagedThisRound.clear(); }

void MouBiYueSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    // 仅在自己回合结束阶段（貂蝉的结束阶段）
    if (engine.getCurrentPlayer() && engine.getCurrentPlayer()->getId() != self.getId()) return;
    int x = std::min(4, (int)damagedThisRound.size() + 1);
    engine.drawCards(selfPtr(engine, self), x, "闭月");
}

// ---------------- 谋·袁绍 ----------------

MouLuanJiSkill::MouLuanJiSkill()
    : ActiveSkill("谋-乱击", mouText("谋·袁绍", "谋-乱击")) {}

bool MouLuanJiSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("乱击已用") > 0) return false;
    return self.getHandCardCount() >= 2;
}

void MouLuanJiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (self.getHandCardCount() < 2) return;
    CardPtr a = engine.askChooseCard(me, self.getHandCards(), "【乱击】选择第一张手牌当【万箭齐发】", false,
                                     AIController::chooseLeastValuableCard(self.getHandCards()));
    if (!a) return;
    engine.loseHandCard(me, a);
    std::vector<CardPtr> rest = self.getHandCards();
    CardPtr b = engine.askChooseCard(me, rest, "【乱击】选择第二张手牌当【万箭齐发】", false,
                                     rest.empty() ? nullptr : AIController::chooseLeastValuableCard(rest));
    if (!b) { engine.obtainCard(me, a); return; }
    engine.loseHandCard(me, b);
    setMark(self, "乱击已用", 1);
    setMark(self, "乱击摸牌计数", 0);
    auto wj = Card::makeVirtual("万箭齐发", CardType::TRICK, CardSubType::WAN_JIAN_QI_FA, {a, b}, "乱击");
    engine.logMessage("  【乱击】" + who(self) + " 将两张手牌当【万箭齐发】使用。");
    engine.useCard(me, wj, engine.getOtherAlivePlayers(self));
}

// 【乱击】两张手牌当【万箭齐发】（全体）：按 A3 群体锦囊时机——敌方人数要多于己方，
// 且手里要留得下牌（至少 3 张），否则等于自伤。
bool MouLuanJiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return AIController::enemyCount(engine, self) > AIController::friendCount(engine, self) &&
           self.getHandCardCount() >= 3;
}

void MouLuanJiSkill::onTurnStart(GameEngine&, Player& self) {
    setMark(self, "乱击已用", 0);
    setMark(self, "乱击摸牌计数", 0);
}

void MouLuanJiSkill::onCardResponded(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getSubType() != CardSubType::SHAN) return;
    if (self.getMark("乱击来源") == 0 || self.getMark("乱击摸牌计数") >= 3) return;
    self.addMark("乱击摸牌计数", 1);
    engine.drawCards(selfPtr(engine, self), 1, "乱击");
}

void MouLuanJiSkill::onCardTargetConfirmed(GameEngine&, Player& self, Player* source, CardPtr card,
                                           const std::vector<PlayerPtr>& targets) {
    if (!source || source->getId() != self.getId() || !card || card->getSkillSource() != "乱击") return;
    for (const auto& target : targets)
        if (target && target->getId() != self.getId()) setMark(*target, "乱击来源", 1);
}

void MouLuanJiSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getSkillSource() != "乱击") return;
    for (const auto& target : engine.getOtherAlivePlayers(self)) setMark(*target, "乱击来源", 0);
}

MouXueYiSkill::MouXueYiSkill()
    : StateSkill("谋-血裔", mouText("谋·袁绍", "谋-血裔"), SkillTag::LOCK | SkillTag::LORD) {}

void MouXueYiSkill::onCalculateHandLimit(GameEngine& engine, const Player& self, int& handLimit) {
    int others = 0;
    for (auto& p : engine.getPlayers())
        if (p->isAlive() && p->getId() != self.getId() && p->getHero() && p->getHero()->getCountry() == Country::QUN) others++;
    handLimit += others * 2;
}

void MouXueYiSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    // 【杀】由 onShaTargeted 记录；其他牌由 onCardTargetConfirmed 记录，避免重复。
    if (!ctx.source || ctx.source->getId() != self.getId() || !ctx.target || !ctx.target->getHero()) return;
    if (ctx.target->getHero()->getCountry() != Country::QUN || ctx.target->getId() == self.getId()) return;
    if (self.getMark("血裔摸牌计数") >= 2) return;
    self.addMark("血裔摸牌计数", 1);
    engine.drawCards(selfPtr(engine, self), 1, "血裔");
}

void MouXueYiSkill::onCardTargetConfirmed(GameEngine& engine, Player& self, Player* source,
                                           CardPtr card, const std::vector<PlayerPtr>& targets) {
    if (!source || source->getId() != self.getId() || !card ||
        card->getSubType() == CardSubType::SHA) return;
    for (const auto& target : targets) {
        if (!target || !target->isAlive() || target->getId() == self.getId() || !target->getHero() ||
            target->getHero()->getCountry() != Country::QUN) continue;
        if (self.getMark("血裔摸牌计数") >= 2) break;
        self.addMark("血裔摸牌计数", 1);
        engine.drawCards(selfPtr(engine, self), 1, "血裔");
    }
}

void MouXueYiSkill::onTurnStart(GameEngine&, Player& self) {
    setMark(self, "血裔摸牌计数", 0);
}

void MouXueYiSkill::onCardPlayed(GameEngine&, Player&, CardPtr) {
    // 【杀】目标在 onShaTargeted 计数，其余牌在 onCardTargetConfirmed 计数，此处无需处理。
}


// ---------------- 谋·庞统 ----------------

MouLianHuanSkill::MouLianHuanSkill()
    : ActiveSkill("谋-连环", mouText("谋·庞统", "谋-连环")) {}

bool MouLianHuanSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    for (const auto& card : self.getHandCards())
        if (card->getSuit() == Suit::CLUB) return true;
    return false;
}

CardPtr MouLianHuanSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (!engine.isPlayerTurn(self) || engine.getCurrentPhase() != TurnPhase::PLAY ||
        !card || card->isVirtual() || card->getSuit() != Suit::CLUB ||
        wanted != CardSubType::TIE_SUO_LIAN_HUAN || convertsThisPhase >= 1) return nullptr;
    return Card::makeVirtual("铁索连环", CardType::TRICK, wanted, {card}, name);
}

void MouLianHuanSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> clubs;
    for (const auto& card : self.getHandCards())
        if (card->getSuit() == Suit::CLUB) clubs.push_back(card);
    if (clubs.empty()) return;
    CardPtr card = engine.askChooseCard(me, clubs, "【连环】重铸一张梅花手牌", false,
                                        AIController::chooseLeastValuableCard(clubs));
    if (!card || !self.hasHandCard(card)) return;
    engine.loseHandCard(me, card);
    engine.getDeck().discardCard(card);
    engine.drawCards(me, 1, "连环重铸");
}

// 【连环】主动入口是“重铸一张梅花手牌”（弃一摸一）：只在手牌超过上限（反正要弃）时用；
// 铁索连环的连环+火杀联动走用牌路径（A7），不在这里空转。
bool MouLianHuanSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    PlayerPtr me = engine.getPlayerById(self.getId());
    if (!me) return false;
    return self.getHandCardCount() > engine.calculateHandLimit(me);
}

void MouLianHuanSkill::onCardPlayed(GameEngine&, Player&, CardPtr card) {
    if (card && card->getSkillSource() == name &&
        card->getSubType() == CardSubType::TIE_SUO_LIAN_HUAN)
        convertsThisPhase++;
}

void MouLianHuanSkill::onCardTargetConfirmed(GameEngine& engine, Player& self, Player* source,
                                              CardPtr card, const std::vector<PlayerPtr>& targets) {
    if (!source || source->getId() != self.getId() || !card ||
        card->getSubType() != CardSubType::TIE_SUO_LIAN_HUAN || targets.empty()) return;
    std::vector<PlayerPtr> eligible;
    for (const auto& target : targets)
        if (target && target->isAlive() && !target->isChained() && target->getHandCardCount() > 0)
            eligible.push_back(target);
    if (eligible.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    if (level == 1) {
        PlayerPtr chosen = engine.askChoosePlayer(me, eligible,
            "【连环】选择一名未横置且有手牌的目标", true);
        if (!chosen) return;
        if (!engine.askConfirm(me, "【连环】是否失去1点体力，令该目标随机弃置一张手牌？", false)) return;
        eligible = {chosen};
        engine.loseHp(me, 1, "连环");
        if (!self.isAlive()) return;
    }
    for (const auto& target : eligible) {
        if (!target->isAlive() || target->isChained() || target->getHandCardCount() == 0) continue;
        auto hand = target->getHandCards();
        std::uniform_int_distribution<size_t> pick(0, hand.size() - 1);
        CardPtr discard = hand[pick(engine.getRng())];
        engine.discardCardOf(target, discard, "连环", me);
        engine.logMessage("  【连环】" + who(*target) + " 未处于连环状态，随机弃置一张手牌。");
    }
}

void MouLianHuanSkill::onCalculateTieSuoTargetLimit(GameEngine&, const Player&, int& limit) {
    if (level >= 2) limit = std::numeric_limits<int>::max();
}

void MouLianHuanSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) convertsThisPhase = 0;
}

void MouLianHuanSkill::resetTurnState() { convertsThisPhase = 0; }

MouNiePanSkill::MouNiePanSkill()
    : TriggerSkill("谋-涅槃", mouText("谋·庞统", "谋-涅槃"), SkillTag::LIMITED) {}

void MouNiePanSkill::onDying(GameEngine& engine, Player& self, Player& dying) {
    if (dying.getId() != self.getId() || spent) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【涅槃】弃置区域里的所有牌，摸两张牌，回复至2点并升级“连环”？", true)) return;
    spent = true;
    for (auto& c : self.getAllEquipment()) engine.discardCardOf(me, c, "涅槃");
    for (auto& c : self.getJudgeZone()) engine.discardCardOf(me, c, "涅槃");
    while (self.getHandCardCount() > 0) engine.discardCardOf(me, self.getHandCards().front(), "涅槃");
    engine.drawCards(me, 2, "涅槃");
    self.setHp(2);
    self.setTurnedOver(false);
    self.setChained(false);
    self.setNonLockSkillsDisabled(false);
    if (auto s = self.getHero() ? self.getHero()->findSkill("谋-连环") : nullptr) {
        if (auto lh = std::dynamic_pointer_cast<MouLianHuanSkill>(s)) lh->upgrade();
    }
    engine.logMessage("  【涅槃】" + who(self) + " 复原武将牌并升级“连环”。");
}

// ---------------- 谋·刘备 ----------------

MouRenDeSkill::MouRenDeSkill()
    : ActiveSkill("谋-仁德", mouText("谋·刘备", "谋-仁德")) {}

bool MouRenDeSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    return self.getHandCardCount() > 0;
}

void MouRenDeSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self),
                                         "【仁德】选择一名本阶段未获得过“仁德”牌的角色", false);
    if (!t) return;
    const std::string phaseMark = "仁德本阶段:" + std::to_string(self.getId());
    if (t->getMark(phaseMark) > 0) return;
    while (self.getHandCardCount() > 0) {
        CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【仁德】交给 " + who(*t) + " 的手牌（0=结束）", true,
                                         AIController::chooseLeastValuableCard(self.getHandCards()));
        if (!c) break;
        engine.obtainCard(t, c, me);
        t->addMark(phaseMark, 1);
        t->addMark("仁德获得:" + std::to_string(self.getId()), 1);
        givenThisPhase++;
        if (self.getMark("仁望") < 8) self.addMark("仁望", 1);
    }
    if (givenThisPhase > 0) t->addMark("仁德本阶段", 1);
}

// 【仁德】给牌：给出第二张时自己回复 1 点体力 → 受伤且有牌可给时才给；
// 手牌溢出（≥6）且有队友时也值得给（帮队友存牌）。
bool MouRenDeSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (self.isWounded() && self.getHandCardCount() >= 3) return true;
    return self.getHandCardCount() >= 6 && AIController::friendCount(engine, self) > 0;
}

bool MouRenDeSkill::onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) {
    if (self.getMark("仁望") < 2) return false;
    if (self.getMark("仁德本回合已用") > 0) return false;
    if (wanted == CardSubType::SHA || wanted == CardSubType::SHAN || wanted == CardSubType::TAO ||
        wanted == CardSubType::JIU) {
        PlayerPtr me = selfPtr(engine, self);
        if (!engine.askConfirm(me, "【仁德】弃置2个“仁望”标记视为使用/打出一张基本牌？", true)) return false;
        setMark(self, "仁望", self.getMark("仁望") - 2);
        setMark(self, "仁德本回合已用", 1);
        out = Card::makeVirtual(wanted == CardSubType::SHA ? "杀" : (wanted == CardSubType::SHAN ? "闪" :
            (wanted == CardSubType::TAO ? "桃" : "酒")), CardType::BASIC, wanted, {}, "仁德");
        return true;
    }
    return false;
}

void MouRenDeSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) {
        const std::string phaseMark = "仁德本阶段:" + std::to_string(self.getId());
        for (const auto& p : engine.getPlayers()) setMark(*p, phaseMark, 0);
        self.addMark("仁望", std::max(0, std::min(2, 8 - self.getMark("仁望"))));
        setMark(self, "仁德本回合已用", 0);
        givenThisPhase = 0;
        engine.logMessage("  【仁德】" + who(self) + " 获得2个“仁望”标记。");
    }
}

void MouRenDeSkill::onPhaseEnd(GameEngine&, Player& self, TurnPhase phase) {
    if (phase == TurnPhase::PLAY) {
        for (auto& p : {self}) (void)p;
        self.addMark("仁德本阶段清", 0);
    }
}

MouZhangWuSkill::MouZhangWuSkill()
    : ActiveSkill("谋-章武", mouText("谋·刘备", "谋-章武"), 0, SkillTag::LIMITED) {}

bool MouZhangWuSkill::canActivate(GameEngine& engine, Player& self) {
    if (spent) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    for (auto& p : engine.getPlayers())
        if (p->isAlive() && p->getId() != self.getId() && p->getMark("仁德获得:" + std::to_string(self.getId())) > 0)
            return true;
    return false;
}

void MouZhangWuSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    int y = std::min(3, std::max(0, engine.getCurrentRound() - 1));
    for (auto& p : engine.getPlayers()) {
        if (!p->isAlive() || p->getId() == self.getId()) continue;
        if (p->getMark("仁德获得:" + std::to_string(self.getId())) == 0) continue;
        for (int i = 0; i < y && p->getHandCardCount() > 0; i++) {
            CardPtr c = AIController::chooseMostValuableCard(p->getHandCards());
            if (!c) break;
            engine.obtainCard(me, c, engine.getPlayerById(p->getId()));
        }
    }
    spent = true;
    engine.recoverHp(me, 3, "章武");
    engine.logMessage("  【章武】" + who(self) + " 回复3点体力并失去“仁德”。");
    if (self.getHero()) engine.removeHeroSkill(me, "仁德");
}

// 【章武】限定技：所有获得过“仁德”牌的角色各交给你 Y 张（Y＝轮数-1）。
// 一局一次，必须值：至少 2 人拿过仁德牌，且已到第 2 轮（Y≥1）。
bool MouZhangWuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (engine.getCurrentRound() < 2) return false;
    int givers = 0;
    for (const auto& p : engine.getAlivePlayers())
        if (p->getId() != self.getId() && p->getMark("仁德获得:" + std::to_string(self.getId())) > 0) ++givers;
    return givers >= 2;
}

MouJiJiangSkill::MouJiJiangSkill()
    : TriggerSkill("谋-激将", mouText("谋·刘备", "谋-激将"), SkillTag::LORD) {}

void MouJiJiangSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY || !self.isAlive()) return;
    if (engine.getCurrentPhase() == TurnPhase::PLAY && !engine.isPlayerTurn(self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!me) return;
    auto allies = engine.getOtherAlivePlayers(self);
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "激将");
    auto hasEligibleHelper = [&](const PlayerPtr& target) {
        if (!target || !target->isAlive()) return false;
        for (const auto& ally : allies) {
            if (!ally->isAlive() || !ally->getHero() || ally->getHero()->getCountry() != Country::SHU ||
                ally->getId() == target->getId() || ally->getHp() < self.getHp()) continue;
            if (engine.canUseShaOn(*ally,*target,sha) && engine.canBeTargeted(target,sha,ally)) return true;
        }
        return false;
    };
    // 官网写“一名角色”，没有“其他”限制；刘备自己也是合法的指定目标。
    std::vector<PlayerPtr> candidates;
    for (const auto& target : engine.getAlivePlayers())
        if (hasEligibleHelper(target)) candidates.push_back(target);
    if (candidates.empty()) return;
    PlayerPtr aiTarget;
    for (const auto& target : candidates)
        if (!AIController::isFriend(engine, self,*target)) { aiTarget=target; break; }
    if (!aiTarget) aiTarget=candidates.front();
    PlayerPtr target = engine.askChoosePlayer(me, candidates, "【激将】指定一名角色", true, aiTarget);
    if (!target) return;
    // 官网原文：“你可指定一名角色，并令**另一名**攻击范围内含有该角色且体力值不小于你的
    // 其他蜀势力角色选择一项……”——只令一名合格蜀势力角色（由刘备指定），
    // 原实现令**所有**合格角色依次选择，属于过度发动（2026-10-06 修）。
    std::vector<PlayerPtr> helpers;
    for (const auto& ally : allies) {
        if (!ally->isAlive() || !ally->getHero() || ally->getHero()->getCountry() != Country::SHU ||
            ally->getId() == target->getId() || ally->getHp() < self.getHp()) continue;
        if (engine.canUseShaOn(*ally,*target,sha) && engine.canBeTargeted(target,sha,ally)) helpers.push_back(ally);
    }
    if (helpers.empty()) return;
    PlayerPtr aiHelper;
    for (const auto& helper : helpers)
        if (AIController::isFriend(engine, self, *helper)) { aiHelper = helper; break; }
    if (!aiHelper) aiHelper = helpers.front();
    PlayerPtr helper = engine.askChoosePlayer(me, helpers, "【激将】令一名其他蜀势力角色选择", false, aiHelper);
    if (!helper) return;
    int opt = engine.askChooseOption(helper,
        {"视为对 " + who(*target) + " 使用一张普通【杀】", "跳过下一个出牌阶段"},
        "【激将】选择一项", 0);
    if (opt == 0) {
        engine.useCard(helper, Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "激将"), {target});
    } else {
        helper->addMark("跳过下个出牌阶段", 1);
    }
}

// ---------------- 谋·姜维 ----------------

MouTiaoXinSkill::MouTiaoXinSkill()
    : ActiveSkill("谋-挑衅", mouText("谋·姜维", "谋-挑衅")) {}

// 蓄力技（4/4）：官方描述未写初值与上限，取移动版武将牌上的“（4/4）”标注。
void MouTiaoXinSkill::onGameStart(GameEngine& engine, Player& self) {
    setMark(self, "蓄力上限", 4);
    if (self.getMark("蓄力") < 4) setMark(self, "蓄力", 4);
    engine.logMessage("  【挑衅】" + who(self) + " 的蓄力点 4/4。");
}

bool MouTiaoXinSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("挑衅已用") > 0 || self.getMark("蓄力") <= 0) return false;
    PlayerPtr me = selfPtr(engine, self);
    for (const auto& target : engine.getOtherAlivePlayers(self)) {
        if (target->getHandCardCount() > 0 || hasLegalShaOption(engine, target, me, true)) return true;
    }
    return false;
}

void MouTiaoXinSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    int x = self.getMark("蓄力");
    std::vector<PlayerPtr> picked;
    while ((int)picked.size() < x) {
        std::vector<PlayerPtr> cands;
        for (const auto& target : engine.getOtherAlivePlayers(self)) {
            if (std::find(picked.begin(), picked.end(), target) != picked.end()) continue;
            if (target->getHandCardCount() > 0 || hasLegalShaOption(engine, target, me, true))
                cands.push_back(target);
        }
        if (cands.empty()) break;
        PlayerPtr target = engine.askChoosePlayer(me, cands, "【挑衅】选择一名其他角色（可取消）", true);
        if (!target) break;

        const bool canUseSha = hasLegalShaOption(engine, target, me, true);
        const bool canGive = target->getHandCardCount() > 0;
        std::vector<std::string> options;
        if (canUseSha) options.push_back("对 " + who(self) + " 使用一张无距离限制的【杀】");
        if (canGive) options.push_back("交给其一张牌");
        // 兜底：理论上候选过滤已保证 options 非空；万一为空则结束选择，
        // 不能 continue（AI 的默认选择恒为候选首位，会原地打转 → 死循环）。
        if (options.empty()) break;
        int choice = engine.askChooseOption(target, options, "【挑衅】选择一项",
                                            canUseSha && canGive ? 1 : 0);
        if (canUseSha && choice == 0) {
            CardPtr sha = engine.askUseSha(target,
                "【挑衅】对 " + who(self) + " 使用一张无距离限制的【杀】",
                !canGive || !AIController::isFriend(engine, *target, self), me, true, false);
            if (sha) engine.resolveSha(target, sha, {me});
        } else if (canGive && target->getHandCardCount() > 0) {
            CardPtr card = AIController::chooseLeastValuableCard(target->getHandCards());
            if (card && target->hasHandCard(card)) engine.obtainCard(me, card, target);
        }
        picked.push_back(target);
    }
    if (picked.empty()) return;
    setMark(self, "挑衅已用", 1);
    int used = (int)picked.size();
    setMark(self, "蓄力", std::max(0, self.getMark("蓄力") - used));
    chosenTotal += used;
    self.addMark("挑衅累计", used);
    engine.logMessage("  【挑衅】" + who(self) + " 选择了 " + std::to_string(used) + " 名角色。");
}

// 官网原文：“弃牌阶段，你每弃置一张牌，获得1点蓄力点。”
// 2026-10-06 复核修正：官网与移动版（谋·姜维，hero-detail-510 / 手杀“蓄力技（4/4）”）都限定为
// **弃牌阶段**，此前“任何弃置都算”（2026-10-04 FAQ）会与弃牌阶段双重口径并存、并让蓄力点无上限膨胀；
// 现改为只在“本人在自己的弃牌阶段弃置手牌”时计数，且不超过上限 4。
void MouTiaoXinSkill::onDiscardedInDiscardPhase(GameEngine& engine, Player& self, CardPtr) {
    const int gained = engine.gainCharge(self, 1);
    if (gained > 0)
        engine.logMessage("  【挑衅】" + who(self) + " 弃置手牌，获得1点蓄力点（" +
                          std::to_string(self.getMark("蓄力")) + "/4）。");
}

void MouTiaoXinSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) setMark(self, "挑衅已用", 0);
}

bool MouTiaoXinSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    return canActivate(engine, self);
}

MouZhiJiSkill::MouZhiJiSkill()
    : TriggerSkill("谋-志继", mouText("谋·姜维", "谋-志继"), SkillTag::AWAKEN) {}

void MouZhiJiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION || awakened || !self.isAlive()) return;
    if (engine.getCurrentPhase() == TurnPhase::PREPARATION && !engine.isPlayerTurn(self)) return;
    if (self.getMark("挑衅累计") < 4) return;
    awakened = true;
    self.changeMaxHp(-1);
    PlayerPtr me = selfPtr(engine, self);
    auto candidates=engine.getAlivePlayers(); // “任意名角色”包括自己，且可选择多名不同角色。
    std::vector<PlayerPtr> chosen;
    if (me && me->isAI()) {
        // AI 只给敌方施加目标限制；不因可选自己就自动选择自己或队友。
        for (const auto& target : candidates)
            if (!AIController::isFriend(engine, self,*target)) chosen.push_back(target);
    } else {
        while (!candidates.empty()) {
            PlayerPtr target=engine.askChoosePlayer(
                me,candidates,"【志继】选择获得“北伐”标记的角色（可多选，0结束）",true);
            if (!target) break;
            chosen.push_back(target);
            candidates.erase(std::remove(candidates.begin(),candidates.end(),target),candidates.end());
        }
    }
    for (const auto& target : chosen) {
        setMark(*target,"北伐",1);
        setMark(*target,"北伐来源",self.getId()+1);
        engine.logMessage("  【志继】" + who(*target) + " 获得“北伐”标记（使用牌只能选择姜维或其为目标）。");
    }
    engine.logMessage("  【志继】" + who(self) + " 觉醒，减1点体力上限。");
}

void MouZhiJiSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (!starting || turnOwner.getId() != self.getId()) return;
    // “直到你的下个回合开始时”：清除所有由这名姜维授予的标记，而不只检查姜维自身。
    for (const auto& target : engine.getPlayers()) {
        if (!target || target->getMark("北伐来源") != self.getId()+1) continue;
        setMark(*target,"北伐",0);
        setMark(*target,"北伐来源",0);
    }
}

// ---------------- 谋·法正 ----------------

MouXuanHuoSkill::MouXuanHuoSkill()
    : ActiveSkill("谋-眩惑", mouText("谋·法正", "谋-眩惑")) {}

bool MouXuanHuoSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("眩惑已用") > 0) return false;
    if (self.getHandCardCount() == 0) return false;
    for (auto& p : engine.getOtherAlivePlayers(self))
        if (p->getMark("眩") == 0) return true;
    return false;
}

void MouXuanHuoSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<PlayerPtr> cands;
    for (auto& p : engine.getOtherAlivePlayers(self))
        if (p->getMark("眩") == 0) cands.push_back(p);
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【眩惑】交给一名没有“眩”标记的角色一张牌", false);
    if (!t) return;
    CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【眩惑】选择要交出的牌", false,
                                     AIController::chooseLeastValuableCard(self.getHandCards()));
    if (!c) return;
    engine.obtainCard(t, c, me);
    setMark(*t, "眩", 1);
    t->addMark("眩获得回合", engine.getCurrentRound());
    setMark(*t, "眩被拿", 0);
    setMark(self, "眩惑已用", 1);
    engine.logMessage("  【眩惑】" + who(*t) + " 获得“眩”标记。");
}

// 【眩惑】给一名没有“眩”的角色一张牌 → 其在摸牌阶段外获得牌时你随机拿其一张手牌：
// 挂在**敌人**身上才划算，且自己手里要留牌。
bool MouXuanHuoSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    for (const auto& p : engine.getOtherAlivePlayers(self))
        if (p->getMark("眩") == 0 && !AIController::isFriend(engine, self, *p)) return self.getHandCardCount() >= 2;
    return false;
}

void MouXuanHuoSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) setMark(self, "眩惑已用", 0);
}

void MouXuanHuoSkill::onAnyCardsObtained(GameEngine& engine, Player& self, Player& recipient, int count) {
    if (count <= 0 || engine.getCurrentPhase() == TurnPhase::DRAW ||
        recipient.getId() == self.getId() || recipient.getMark("眩") == 0) return;
    int remaining = std::max(0, 5 - recipient.getMark("眩被拿"));
    int steals = std::min(count, remaining);
    auto target = engine.getPlayerById(recipient.getId());
    auto me = engine.getPlayerById(self.getId());
    for (int i = 0; i < steals && target && me && target->isAlive() && target->getHandCardCount() > 0; ++i) {
        auto hand = target->getHandCards();
        std::uniform_int_distribution<size_t> pick(0, hand.size() - 1);
        engine.obtainCard(me, hand[pick(engine.getRng())], target);
        recipient.addMark("眩被拿", 1);
        engine.logMessage("  【眩惑】" + who(self) + " 随机获得了" + who(recipient) + "的一张手牌。");
    }
}

MouEnYuanSkill::MouEnYuanSkill()
    : TriggerSkill("谋-恩怨", mouText("谋·法正", "谋-恩怨"), SkillTag::LOCK) {}

void MouEnYuanSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION || !engine.isPlayerTurn(self)) return;
    for (auto& p : engine.getPlayers()) {
        if (!p->isAlive() || p->getMark("眩") == 0) continue;
        int stolen = p->getMark("眩被拿"); // 法正从其处获得的牌数（由眩惑计数）
        if (stolen >= 3) {
            setMark(*p, "眩", 0);
            for (int i = 0; i < 3 && self.getHandCardCount() < 99; i++) {
                CardPtr c = AIController::chooseLeastValuableCard(self.getHandCards());
                if (!c) break;
                engine.obtainCard(p, c, selfPtr(engine, self));
            }
            engine.logMessage("  【恩怨】" + who(*p) + " 已失去至少三张牌，" + who(self) + " 交给其三张牌。");
        } else {
            engine.loseHp(engine.getPlayerById(p->getId()), 1, "恩怨");
            if (self.isAlive()) {
                engine.recoverHp(selfPtr(engine, self), 1, "恩怨");
                setMark(*p, "眩", 0);
            }
        }
        setMark(*p, "眩被拿", 0);
    }
}


// =====================================================================
//  群
// =====================================================================

// ---------------- 谋·陈宫 ----------------

MouMingCeSkill::MouMingCeSkill()
    : ActiveSkill("谋-明策", mouText("谋·陈宫", "谋-明策")) {}

bool MouMingCeSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("明策已用") > 0) return false;
    return self.getHandCardCount() > 0;
}

void MouMingCeSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (self.getHandCardCount() > 0) {
        PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【明策】将一张牌交给一名其他角色", false);
        if (!t) return;
        CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【明策】选择要交出的牌", false,
                                         AIController::chooseLeastValuableCard(self.getHandCards()));
        if (!c) return;
        engine.obtainCard(t, c, me);
        setMark(self, "明策已用", 1);
        int opt = engine.askChooseOption(t, {"流失1点体力（你摸两张牌并获得“策”标记）", "摸一张牌"},
                                         "【明策】选择一项", 1);
        if (opt == 0) {
            engine.loseHp(engine.getPlayerById(t->getId()), 1, "明策");
            engine.drawCards(me, 2, "明策");
            self.addMark("策", 1);
        } else {
            engine.drawCards(t, 1, "明策");
        }
        return;
    }
    // 阶段开始时的“策”消耗在 onPhaseStart；此处若只有“策”无手牌不做（主动补发动）
}

bool MouMingCeSkill::aiShouldActivate(GameEngine&, Player& self) {
    return self.getHandCardCount() > 1 || self.getMark("策") > 0;
}

void MouMingCeSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return;
    setMark(self, "明策已用", 0);
    int x = self.getMark("策");
    if (x <= 0) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self),
                                         "【明策】对一名角色造成 " + std::to_string(x) + " 点伤害并移除所有“策”标记？", true);
    if (!t) return;
    setMark(self, "策", 0);
    engine.applyDamage(me, t, x, ShaElement::NORMAL);
    engine.logMessage("  【明策】" + who(self) + " 移除 " + std::to_string(x) + " 个“策”标记，对 " + who(*t) +
                      " 造成 " + std::to_string(x) + " 点伤害。");
}

MouZhiChiSkill::MouZhiChiSkill()
    : StateSkill("谋-智迟", mouText("谋·陈宫", "谋-智迟"), SkillTag::LOCK) {}

void MouZhiChiSkill::onTakeDamage(GameEngine& engine, Player& self, Player*, int& damage, ShaElement) {
    if (damage <= 0) return;
    if (damagedThisTurn) {
        engine.logMessage("  【智迟】" + who(self) + " 本回合已受到过伤害，防止之。");
        damage = 0;
    } else {
        damagedThisTurn = true; // 首次伤害正常结算，并标记“受到伤害后”
    }
}

void MouZhiChiSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool starting) {
    // 「本回合」指受到伤害时所处的回合：任何回合结束即重置
    if (!starting) damagedThisTurn = false;
}

// ---------------- 谋·甘宁 ----------------

MouQiXiSkill::MouQiXiSkill()
    : ActiveSkill("谋-奇袭", mouText("谋·甘宁", "谋-奇袭")) {}

bool MouQiXiSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("奇袭已用") > 0) return false;
    return self.getHandCardCount() > 0;
}

void MouQiXiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【奇袭】选择一名其他角色", false);
    if (!t) return;
    setMark(self, "奇袭已用", 1);
    // 统计手牌花色最多的种类（之一）—— 正确映射 SPADE/HEART/CLUB/DIAMOND 到 红桃/方块/梅花/黑桃
    std::vector<Suit> order = {Suit::HEART, Suit::DIAMOND, Suit::CLUB, Suit::SPADE};
    std::vector<std::string> names = {"红桃", "方块", "梅花", "黑桃"};
    int cnt[4] = {0, 0, 0, 0};
    for (auto& c : self.getHandCards()) {
        Suit s = c->getSuit();
        for (int i = 0; i < 4; ++i) if (order[i] == s) { cnt[i]++; break; }
    }
    int mx = 0;
    for (int i = 0; i < 4; ++i) mx = std::max(mx, cnt[i]);
    std::vector<int> options;
    for (int i = 0; i < 4; ++i) if (cnt[i] == mx && mx > 0) options.push_back(i);
    std::set<int> guessed;
    int wrong = 0;
    while (true) {
        std::vector<std::string> opts;
        std::vector<int> ids;
        for (int i = 0; i < 4; ++i) if (!guessed.count(i)) { opts.push_back(names[i]); ids.push_back(i); }
        if (opts.empty()) break;
        int opt = engine.askChooseOption(t, opts, "【奇袭】猜测 " + who(self) + " 手牌中最多的花色（或之一）", 0);
        int g = ids[opt];
        guessed.insert(g);
        bool right = std::find(options.begin(), options.end(), g) != options.end();
        if (right) {
            engine.logMessage("  【奇袭】" + who(*t) + " 猜对了，" + who(self) + " 展示所有手牌。");
            // 展示：日志打印所有手牌名
            std::string seen;
            for (auto &c : self.getHandCards()) seen += c->getFormattedName() + " ";
            if (!seen.empty()) engine.logMessage("    展示手牌：" + seen);
            break;
        }
        wrong++;
        // 按描述“你可令其再次猜测”—— 询问发动者（谋甘宁）而非被猜者
        if (!engine.askConfirm(me, "猜错了，是否令其再次猜测？", guessed.size() < 4)) break;
    }
    int X = wrong;
    // 弃置其区域内X张牌（若不足则全弃）
    std::vector<CardPtr> zone = t->getAllCards();
    int n = std::min<int>(X, zone.size());
    for (int i = 0; i < n && !zone.empty(); i++) {
        CardPtr c = AIController::chooseLeastValuableCard(zone);
        if (!c) c = zone.front();
        engine.discardCardOf(t, c, "奇袭");
        zone.erase(std::find(zone.begin(), zone.end(), c));
    }
    engine.logMessage("  【奇袭】弃置 " + who(*t) + " 区域内 " + std::to_string(n) + " 张牌。");
}

// 【奇袭】让对方猜你手牌里最多的花色，猜错则有收益 → 要有敌人，且手牌够多才好骗。
bool MouQiXiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return AIController::hasEnemy(engine, self) && self.getHandCardCount() >= 3;
}

void MouQiXiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) setMark(self, "奇袭已用", 0);
}

MouFenWeiSkill::MouFenWeiSkill()
    : ActiveSkill("谋-奋威", mouText("谋·甘宁", "谋-奋威"), 0, SkillTag::LIMITED) {}

bool MouFenWeiSkill::canActivate(GameEngine& engine, Player& self) {
    if (spent) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    return self.getHandCardCount() > 0;
}

void MouFenWeiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!me) return;
    int placed = 0;
    while (placed < 3) {
        std::vector<CardPtr> hand = self.getHandCards();
        if (hand.empty()) break;
        CardPtr c = engine.askChooseCard(me, hand, "【奋威】选择一张手牌作为“威”（0=结束）", true, nullptr);
        if (!c) break;
        std::vector<PlayerPtr> cands;
        for (auto& p : engine.getPlayers())
            if (p->isAlive() && p->getPileCount("威") == 0) cands.push_back(p);
        if (cands.empty()) break;
        PlayerPtr t = engine.askChoosePlayer(me, cands, "【奋威】将“威”置于哪名角色的武将牌上", false);
        if (!t) break;
        engine.loseHandCard(me, c);
        t->addToPile("威", c);
        placed++;
        engine.logMessage("  【奋威】" + who(self) + " 将一张“威”置于 " + who(*t) + " 的武将牌上。");
    }
    if (placed > 0) {
        engine.drawCards(me, placed, "奋威");
        spent = true;
        engine.logMessage("  【奋威】" + who(self) + " 置“威”" + std::to_string(placed) + " 张并摸等量牌。");
    }
}

void MouFenWeiSkill::onCheckCardTarget(GameEngine& engine, const Player& self, const Player& target,
                                       CardPtr card, bool& canTarget) {
    if (!card || card->getType() != CardType::TRICK) return;
    // 有“威”的角色成为锦囊牌的目标时，甘宁（保护者）选择一项
    // “威”牌在 target 的 pile "威" 中；保护者=奋威持有者 self（此处钩子在被指定者身上——self 即被指定者）
    PlayerPtr holder;
    if (target.getPileCount("威") > 0) {
        // 找到放置“威”的甘宁（任何有奋威技能的角色）
        for (auto& p : engine.getPlayers()) {
            if (p->isAlive() && p->getHero() && p->getHero()->findSkill("谋-奋威")) { holder = engine.getPlayerById(p->getId()); break; }
        }
    }
    if (!holder) return;
    int opt = engine.askChooseOption(holder, {"令其获得“威”牌", "弃置其“威”牌，取消其作为此锦囊牌的目标"},
                                     "【奋威】" + who(target) + " 成为锦囊目标，选择一项", 0);
    PlayerPtr tgt = engine.getPlayerById(target.getId());
    std::vector<CardPtr> w = target.getPile("威"); // 拷贝（处理过程会改动武将牌上的牌）
    if (opt == 0) {
        for (auto& c : w) {
            const_cast<Player&>(target).removeFromPile("威", c);
            engine.obtainCard(tgt, c, holder);
        }
    } else {
        for (auto& c : w) {
            const_cast<Player&>(target).removeFromPile("威", c);
            engine.getDeck().discardCard(c);
        }
        canTarget = false;
    }
}

// ---------------- 谋·黄盖 ----------------

MouKuRouSkill::MouKuRouSkill()
    : TriggerSkill("谋-苦肉", mouText("谋·黄盖", "谋-苦肉")) {}

void MouKuRouSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY) return;
    if (self.getHandCardCount() == 0 && self.getAllEquipment().empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> mine = self.getHandCards();
    auto eq = self.getAllEquipment();
    mine.insert(mine.end(), eq.begin(), eq.end());
    if (!engine.askConfirm(me, "【苦肉】交给其他角色一张牌，然后失去1点体力（桃/酒则-2）？", self.isWounded())) return;
    CardPtr c = engine.askChooseCard(me, mine, "【苦肉】选择要交出的一张牌", false,
                                     AIController::chooseLeastValuableCard(mine));
    if (!c) return;
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【苦肉】交给哪名角色", false);
    if (!t) return;
    bool big = (c->getSubType() == CardSubType::TAO || c->getSubType() == CardSubType::JIU);
    if (std::find(self.getHandCards().begin(), self.getHandCards().end(), c) != self.getHandCards().end()) engine.obtainCard(t, c, me);
    else engine.moveFieldCard(selfPtr(engine, self), t, c);
    engine.loseHp(me, big ? 2 : 1, "苦肉");
}

void MouKuRouSkill::onLoseHp(GameEngine& engine, Player& self, int amount) {
    if (amount <= 0) return;
    // 每失去1点体力获得2点护甲
    self.addMark("护甲", amount * 2);
    engine.logMessage("  【苦肉】" + who(self) + " 失去 " + std::to_string(amount) + " 点体力，获得 " +
                      std::to_string(amount * 2) + " 点护甲（现 " + std::to_string(self.getMark("护甲")) + "）。");
}

MouZhaXiangSkill::MouZhaXiangSkill()
    : StateSkill("谋-诈降", mouText("谋·黄盖", "谋-诈降"), SkillTag::LOCK) {}

void MouZhaXiangSkill::onDrawCards(GameEngine&, Player& self, int& drawCount) {
    drawCount += std::max(0, self.getMaxHp() - self.getHp());
}

void MouZhaXiangSkill::onCheckShaTarget(GameEngine&, const Player& self, const Player&, CardPtr, bool& canTarget) {
    int x = std::max(0, self.getMaxHp() - self.getHp());
    if (cardsUsedThisTurn < x) canTarget = true; // 前X张牌无距离限制
}

void MouZhaXiangSkill::onCheckCardTargetAsSource(GameEngine&, const Player& self, const Player&, CardPtr card,
                                                   bool& canTarget) {
    if (card && cardsUsedThisTurn < std::max(0, self.getMaxHp() - self.getHp())) canTarget = true;
}

void MouZhaXiangSkill::onCalculateShaLimit(GameEngine&, const Player& self, int& shaLimit) {
    int x = std::max(0, self.getMaxHp() - self.getHp());
    if (cardsUsedThisTurn < x) shaLimit += 10; // 前X张牌无次数限制
}

void MouZhaXiangSkill::onCardUsed(GameEngine&, Player&, CardPtr, bool) {
    // 【诈降】统计每回合真正使用的所有牌，而不是只统计普通锦囊。
    cardsUsedThisTurn++;
}

void MouZhaXiangSkill::onTurnBoundary(GameEngine&, Player& self, Player& turnOwner, bool starting) {
    if (starting && turnOwner.getId() == self.getId()) cardsUsedThisTurn = 0;
}

// =====================================================================
//  吴（续）
// =====================================================================

// ---------------- 谋·孙权 ----------------

MouZhiHengSkill::MouZhiHengSkill()
    : ActiveSkill("谋-制衡", mouText("谋·孙权", "谋-制衡")) {}

bool MouZhiHengSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("制衡已用") > 0) return false;
    return !self.getHandAndEquipmentCards().empty();
}

void MouZhiHengSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> disc;
    std::vector<CardPtr> pool = self.getHandAndEquipmentCards();
    const int handBefore = self.getHandCardCount();
    while (!pool.empty()) {
        CardPtr c = engine.askChooseCard(me, pool, "【制衡】选择弃置的牌（0=结束）", true,
                                         AIController::chooseLeastValuableCard(pool));
        if (!c) break;
        disc.push_back(c);
        pool.erase(std::remove(pool.begin(), pool.end(), c), pool.end());
    }
    if (disc.empty()) return;
    const int discardedHandCount = static_cast<int>(std::count_if(
        disc.begin(), disc.end(), [&](const CardPtr& card) { return self.hasHandCard(card); }));
    const bool discardedAllHand = handBefore > 0 && discardedHandCount == handBefore;
    setMark(self, "制衡已用", 1);
    for (auto& c : disc) engine.discardCardOf(me, c, "制衡");
    engine.drawCards(me, static_cast<int>(disc.size()), "制衡");
    if (discardedAllHand) {
        int x = self.getMark("业");
        engine.drawCards(me, x + 1, "制衡（业）");
        if (x > 0) setMark(self, "业", x - 1);
    }
}

void MouZhiHengSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) setMark(self, "制衡已用", 0);
}

bool MouZhiHengSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    const auto cards = self.getHandAndEquipmentCards();
    return std::any_of(cards.begin(), cards.end(),
                       [&](const CardPtr& card) { return AIController::cardValue(engine, self, card) <= 3; });
}

MouTongYeSkill::MouTongYeSkill()
    : TriggerSkill("谋-统业", mouText("谋·孙权", "谋-统业"), SkillTag::LOCK) {}

void MouTongYeSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    if (engine.getCurrentPlayer() && engine.getCurrentPlayer()->getId() != self.getId()) return;
    int now = 0;
    for (auto& p : engine.getPlayers()) if (p->isAlive()) now += (int)p->getAllEquipment().size();
    bool changed = (equipCountAtEnd >= 0 && now != equipCountAtEnd);
    if (equipCountAtEnd < 0) changed = false;
    equipCountAtEnd = now;
    int opt = engine.askChooseOption(selfPtr(engine, self),
                                     {"装备数变化时+1业（否则-1）", "装备数不变时+1业（否则-1）"},
                                     "【统业】选择一项直到下回合准备阶段", 0);
    bool gain = (opt == 0) ? changed : !changed;
    int cur = self.getMark("业");
    if (gain) setMark(self, "业", std::min(2, cur + 1));
    else setMark(self, "业", std::max(0, cur - 1));
    engine.logMessage("  【统业】" + who(self) + " 的“业”标记为 " + std::to_string(self.getMark("业")) + "。");
}

MouJiuYuanSkill::MouJiuYuanSkill()
    : StateSkill("谋-救援", mouText("谋·孙权", "谋-救援"), SkillTag::LOCK | SkillTag::LORD) {}

void MouJiuYuanSkill::onAnyPeachUsed(GameEngine& engine, Player& self, Player& user, Player& target) {
    // 其他吴势力角色使用【桃】时，你摸一张牌
    if (user.getId() == self.getId()) return;
    if (!user.getHero() || user.getHero()->getCountry() != Country::WU) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!me) return;
    engine.drawCards(me, 1, "救援");
    if (target.getId() == self.getId())
        engine.recoverHp(me, 1, "救援", engine.getPlayerById(user.getId()));
    engine.logMessage("  【救援】" + who(self) + " 摸了一张牌。");
}

void MouJiuYuanSkill::onCalculateRecover(GameEngine&, Player&, Player*, int&,
                                          const std::string&) {
    // 【救援】仅在明确的【桃】使用广播中追加回复，避免放大其他回复效果。
}


// ---------------- 谋·大乔 ----------------

MouGuoSeSkill::MouGuoSeSkill()
    : ActiveSkill("谋-国色", mouText("谋·大乔", "谋-国色")) {}

bool MouGuoSeSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (uses >= 4) return false;
    for (auto& c : self.getHandCards()) if (c->getSuit() == Suit::DIAMOND) return true;
    for (auto& p : engine.getPlayers()) {
        for (auto& c : p->getJudgeZone()) if (c->getSubType() == CardSubType::LE_BU_SI_SHU) return true;
    }
    return false;
}

CardPtr MouGuoSeSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (!card || card->getSuit() != Suit::DIAMOND) return nullptr;
    if (wanted != CardSubType::LE_BU_SI_SHU) return nullptr;
    if (uses >= 4 || !engine.isPlayerTurn(self) || engine.getCurrentPhase() != TurnPhase::PLAY) return nullptr;
    return Card::makeVirtual("乐不思蜀", CardType::TRICK, CardSubType::LE_BU_SI_SHU, {card}, "国色");
}

void MouGuoSeSkill::onCardPlayed(GameEngine& engine, Player& self, CardPtr card) {
    if (!card || card->getSkillSource() != "国色" || card->getSubType() != CardSubType::LE_BU_SI_SHU) return;
    ++uses;
    engine.drawCards(selfPtr(engine, self), 1, "国色");
}

void MouGuoSeSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    // 弃置场上一张【乐不思蜀】
    PlayerPtr me = selfPtr(engine, self);
    for (auto& p : engine.getPlayers()) {
        auto& jz = p->getJudgeZone();
        for (auto& c : jz) {
            if (c->getSubType() == CardSubType::LE_BU_SI_SHU) {
                if (engine.askConfirm(me, "【国色】是否弃置 " + who(*p) + " 判定区的【乐不思蜀】？", true)) {
                    uses++;
                    engine.discardCardOf(engine.getPlayerById(p->getId()), c, "国色");
                    engine.drawCards(me, 1, "国色");
                    return;
                }
            }
        }
    }
}

bool MouGuoSeSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    // 判定区有【乐不思蜀】时才用主动弃乐（转化使用由出牌 AI 自动完成）
    for (auto& p : engine.getPlayers())
        for (auto& c : p->getJudgeZone()) if (c->getSubType() == CardSubType::LE_BU_SI_SHU) return true;
    return false;
}

MouLiuLiSkill::MouLiuLiSkill()
    : TriggerSkill("谋-流离", mouText("谋·大乔", "谋-流离")) {}

void MouLiuLiSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.target || ctx.target->getId() != self.getId()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr user = ctx.source ? engine.getPlayerById(ctx.source->getId()) : nullptr;
    std::vector<CardPtr> costs = self.getHandAndEquipmentCards();
    std::vector<PlayerPtr> cands;
    for (auto& p : engine.getPlayersInRange(self, self.getAttackRange()))
        if (p->isAlive() && p->getId() != self.getId() && (!user || p->getId() != user->getId())) cands.push_back(p);
    if (costs.empty() || cands.empty()) return;
    CardPtr cost = engine.askChooseCard(me, costs, "【流离】弃置一张牌并转移此【杀】", true,
                                        AIController::chooseLeastValuableCard(costs));
    if (!cost) return;
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【流离】转移给谁", false);
    if (!t) return;
    engine.discardCardOf(me, cost, "流离");
    ctx.redirect = t;

    if (cost->getSuit() == Suit::HEART && self.getMark("流离红桃已用") == 0) {
        setMark(self, "流离红桃已用", 1);
        std::vector<PlayerPtr> markCands;
        for (auto& p : engine.getPlayers())
            if (p->isAlive() && p->getId() != self.getId() && (!user || p->getId() != user->getId())) markCands.push_back(p);
        if (!markCands.empty() && engine.askConfirm(me, "【流离】是否令一名其他角色获得“流离”标记？", true)) {
            PlayerPtr marked = engine.askChoosePlayer(me, markCands, "【流离】选择获得标记的角色", false);
            if (marked) {
                for (auto& p : engine.getPlayers()) if (p->getMark("流离") > 0) setMark(*p, "流离", 0);
                setMark(*marked, "流离", 1);
            }
        }
    }
    engine.logMessage("  【流离】" + who(self) + " 将【杀】转移给 " + who(*t) + "。");
}

void MouLiuLiSkill::onTurnStart(GameEngine& engine, Player&) {
    for (auto& p : engine.getPlayers()) setMark(*p, "流离红桃已用", 0);
}

void MouLiuLiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION) return;
    if (self.getMark("流离") == 0) return;
    setMark(self, "流离", 0);
    setMark(self, "流离额外出牌阶段", 1);
    engine.logMessage("  【流离】" + who(self) + " 执行一个额外的出牌阶段。");
}

// ---------------- 谋·孟获 ----------------

MouHuoShouSkill::MouHuoShouSkill()
    : StateSkill("谋-祸首", mouText("谋·孟获", "谋-祸首"), SkillTag::LOCK) {}

void MouHuoShouSkill::onCheckCardEffect(GameEngine&, const Player& self, CardPtr card, bool& effective) {
    if (card && card->getSubType() == CardSubType::NAN_MAN_RU_QIN && self.getHero() &&
        self.getHero()->findSkill("谋-祸首")) {
        // 南蛮入侵对祸首持有者无效
        effective = false;
    }
}

void MouHuoShouSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return;
    usedThisPhase = false;
    setMark(self, "谋祸首本阶段已用", 0);
    // 随机获得弃牌堆中一张【南蛮入侵】
    std::vector<CardPtr> nanmans;
    for (auto& c : engine.getDeck().getDiscardPile())
        if (c && c->getSubType() == CardSubType::NAN_MAN_RU_QIN) nanmans.push_back(c);
    if (!nanmans.empty()) {
        std::uniform_int_distribution<size_t> pick(0, nanmans.size() - 1);
        CardPtr c = nanmans[pick(engine.getRng())];
        if (engine.getDeck().removeDiscardCard(c)) {
            engine.obtainCard(selfPtr(engine, self), c);
            engine.logMessage("  【祸首】" + who(self) + " 从弃牌堆随机获得一张【南蛮入侵】。");
        }
    }
}

void MouHuoShouSkill::onCardPlayed(GameEngine& engine, Player& self, CardPtr card) {
    if (card && card->getSubType() == CardSubType::NAN_MAN_RU_QIN && engine.isPlayerTurn(self)) {
        usedThisPhase = true;
        setMark(self, "谋祸首本阶段已用", 1);
    }
    (void)engine; (void)self;
}

void MouHuoShouSkill::onPhaseEnd(GameEngine&, Player&, TurnPhase) {}

MouZaiQiSkill::MouZaiQiSkill()
    : ActiveSkill("谋-再起", mouText("谋·孟获", "谋-再起")) {}

void MouZaiQiSkill::onGameStart(GameEngine&, Player& self) {
    setMark(self, "蓄力上限", 7); // 蓄力技（0/7）：初始 0、上限 7（供局势显示与跨技能引用）
}

bool MouZaiQiSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::DISCARD || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("蓄力") <= 0) return false;
    if (self.getMark("再起已用") > 0) return false;
    return true;
}

void MouZaiQiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine,self)) return;
    PlayerPtr me = selfPtr(engine, self);
    // “任意名角色”包括自己；可选目标数也据全体存活角色计算。
    int x = std::min(self.getMark("蓄力"), (int)engine.getAlivePlayers().size());
    if (x <= 0) return;
    std::vector<PlayerPtr> picked;
    while ((int)picked.size() < x) {
        std::vector<PlayerPtr> cands;
        for (auto& p : engine.getAlivePlayers())
            if (std::find(picked.begin(), picked.end(), p) == picked.end()) cands.push_back(p);
        if (cands.empty()) break;
        PlayerPtr t = engine.askChoosePlayer(me, cands, "【再起】选择一名角色（0=结束）", true);
        if (!t) break;
        picked.push_back(t);
    }
    if (picked.empty()) return;
    setMark(self, "再起已用", 1);
    setMark(self, "蓄力", self.getMark("蓄力") - (int)picked.size());
    for (auto& t : picked) {
        auto discardable = t->getHandAndEquipmentCards();
        std::vector<std::string> options{"令 " + who(self) + " 摸一张牌"};
        if (!discardable.empty()) options.push_back("弃置一张牌，然后令其回复1点体力");
        int opt = engine.askChooseOption(t, options, "【再起】选择一项", 0);
        if (opt == 0) engine.drawCards(me, 1, "再起");
        else if (opt == 1 && !discardable.empty()) {
            CardPtr c = engine.askChooseCard(t, discardable, "【再起】选择弃置的一张牌", false,
                                             AIController::chooseLeastValuableCard(discardable));
            if (!c) continue;
            engine.discardCardOf(t, c, "再起");
            engine.recoverHp(me, 1, "再起");
        }
    }
}

// 【再起】弃牌阶段结束消耗蓄力点，令所选角色“摸一张”或“弃一张并令你回复 1 点体力”：
// 自己缺血或缺牌时才花蓄力点（蓄力点还要留给挑衅/距离等其它用途）。
bool MouZaiQiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return self.isWounded() || self.getHandCardCount() <= 1;
}

void MouZaiQiSkill::onAfterDealDamage(GameEngine& engine, Player& self, Player* source, int, ShaElement, CardPtr) {
    if (source != &self || gainedThisTurn) return;
    gainedThisTurn = true;
    int cap = self.getMark("蓄力上限"); // 蓄力技（0/7）
    if (cap <= 0) cap = 7;
    if (self.getMark("蓄力") < cap) self.addMark("蓄力", 1);
    (void)engine;
}


// ---------------- 谋·孙策 ----------------

MouJiAngSkill::MouJiAngSkill()
    : ActiveSkill("谋-激昂", mouText("谋·孙策", "谋-激昂")) {}

CardPtr MouJiAngSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (wanted != CardSubType::JUE_DOU) return nullptr;
    if (!card) return nullptr;
    // 将所有手牌当【决斗】：由 activate 组织；单牌转化返回空（走 activate 路径）
    (void)engine; (void)self; (void)card;
    return nullptr;
}

bool MouJiAngSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getHandCardCount() == 0) return false;
    int limit = level == 1 ? 1 : [&engine] {
        int n = 1;
        for (auto& p : engine.getPlayers())
            if (p->isAlive() && p->getHero() && p->getHero()->getCountry() == Country::WU) n++;
        return n;
    }();
    return usedDuels < limit;
}

void MouJiAngSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> all = self.getHandCards();
    if (all.empty()) return;
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【激昂】将所有手牌当【决斗】，选择目标", false);
    if (!t) return;
    usedDuels++;
    std::vector<CardPtr> mats;
    for (auto& c : all) { engine.loseHandCard(me, c); mats.push_back(c); }
    auto duel = Card::makeVirtual("决斗", CardType::TRICK, CardSubType::JUE_DOU, mats, "激昂");
    engine.logMessage("  【激昂】" + who(self) + " 将所有手牌当【决斗】对 " + who(*t) + " 使用。");
    std::vector<PlayerPtr> targets{t};
    engine.useCard(me, duel, targets);
    setMark(self, "谋激昂追加目标", 0);
}

// 【激昂】出牌阶段可把**所有手牌**当【决斗】使用（还会为额外目标流失 1 点体力）：
// 只在能压死对面、或手牌已经很少（不心疼）时用，且必须有敌人。
bool MouJiAngSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (!AIController::hasEnemy(engine, self)) return false;
    if (self.getHandCardCount() <= 2) return true;
    for (const auto& t : AIController::enemiesOf(engine, self))
        if (t->getHp() <= 2) return true;   // 决斗能压到低血敌人
    return false;
}

void MouJiAngSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (ctx.card && (ctx.card->getSuit() == Suit::HEART || ctx.card->getSuit() == Suit::DIAMOND ||
                     engine.effectiveSuit(self, ctx.card) == Suit::HEART ||
                     engine.effectiveSuit(self, ctx.card) == Suit::DIAMOND))
        engine.drawCards(selfPtr(engine, self), 1, "激昂");
}

void MouJiAngSkill::onDuelTargeted(GameEngine& engine, Player& self, Player& source, Player& target) {
    // 使用者和成为目标者分别在“指定目标后”各摸一张；多目标决斗逐目标触发。
    if (source.getId() == self.getId() || target.getId() == self.getId())
        engine.drawCards(selfPtr(engine, self), 1, "激昂");
}



MouHunZiSkill::MouHunZiSkill()
    : TriggerSkill("谋-魂姿", mouText("谋·孙策", "谋-魂姿"), SkillTag::AWAKEN) {}

void MouHunZiSkill::onLeaveDying(GameEngine& engine, Player& self) {
    if (self.getMark("谋魂姿已觉醒") > 0) return;
    setMark(self, "谋魂姿已觉醒", 1);
    self.changeMaxHp(-1);
    self.addMark("护甲", 1);
    engine.drawCards(selfPtr(engine, self), 3, "魂姿");
    if (self.getHero()) {
        self.getHero()->addSkill(std::make_shared<MouXingYingZiSkill>());
        self.getHero()->addSkill(std::make_shared<MouXingYingHunSkill>());
    }
    engine.logMessage("  【魂姿】" + who(self) + " 觉醒，获得“英姿※”与“英魂※”！");
}

MouZhiBaSkill::MouZhiBaSkill()
    : TriggerSkill("谋-制霸", mouText("谋·孙策", "谋-制霸"), SkillTag::LORD | SkillTag::LIMITED) {}

void MouZhiBaSkill::onDying(GameEngine& engine, Player& self, Player& dying) {
    if (dying.getId() != self.getId() || self.getMark("制霸已用") > 0) return;
    setMark(self, "制霸已用", 1);
    int wu = 0;
    for (auto& p : engine.getPlayers())
        if (p->isAlive() && p->getHero() && p->getHero()->getCountry() == Country::WU) wu++;
    int x = std::max(0, wu - 1);
    if (x > 0) engine.recoverHp(selfPtr(engine, self), x, "制霸");
    // 升级激昂
    if (auto s = self.getHero() ? self.getHero()->findSkill("谋-激昂") : nullptr)
        if (auto ja = std::dynamic_pointer_cast<MouJiAngSkill>(s)) ja->setLevel(2);
    engine.logMessage("  【制霸】" + who(self) + " 回复 " + std::to_string(x) + " 点体力并升级“激昂”。");
    // 其他吴势力角色依次受到1点无来源伤害
    for (auto& p : engine.getPlayers()) {
        if (!p->isAlive() || p->getId() == self.getId()) continue;
        if (!p->getHero() || p->getHero()->getCountry() != Country::WU) continue;
        if (!p->isAlive()) continue;
        PlayerPtr pp = engine.getPlayerById(p->getId());
        engine.applyDamage(nullptr, pp, 1, ShaElement::NORMAL);
        if (!p->isAlive() && p->getMark("制霸因你死亡") == 0) {
            setMark(*p, "制霸因你死亡", 1);
            engine.drawCards(selfPtr(engine, self), 3, "制霸");
        }
    }
}

MouXingYingZiSkill::MouXingYingZiSkill()
    : StateSkill("谋-英姿", mouText("谋·孙策", "谋-英姿※"), SkillTag::LOCK) {}

void MouXingYingZiSkill::onDrawCards(GameEngine&, Player& self, int& drawCount) {
    int n = 0;
    if (self.getHandCardCount() >= 2) n++;
    if (self.getHp() >= 2) n++;
    if ((int)self.getAllEquipment().size() >= 1) n++;
    drawCount += n;
}

void MouXingYingZiSkill::onCalculateHandLimit(GameEngine&, const Player& self, int& handLimit) {
    int n = 0;
    if (self.getHandCardCount() >= 2) n++;
    if (self.getHp() >= 2) n++;
    if ((int)self.getAllEquipment().size() >= 1) n++;
    handLimit += n;
}

MouXingYingHunSkill::MouXingYingHunSkill()
    : ActiveSkill("谋-英魂", mouText("谋·孙策", "谋-英魂※")) {}

bool MouXingYingHunSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PREPARATION) return false;
    if (self.getMark("英魂已用") > 0) return false;
    return self.isWounded();
}

void MouXingYingHunSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【英魂※】选择一名其他角色", false);
    if (!t) return;
    setMark(self, "英魂已用", 1);
    int x = std::max(0, self.getMaxHp() - self.getHp());
    int opt = engine.askChooseOption(t, {"令其摸" + std::to_string(x) + "张牌，然后弃置一张牌", "令其摸一张牌，然后弃置" + std::to_string(x) + "张牌"},
                                     "【英魂※】选择一项", 0);
    if (opt == 0) {
        engine.drawCards(t, x, "英魂※");
        auto discardable = t->getHandAndEquipmentCards();
        if (!discardable.empty()) {
            CardPtr c = engine.askChooseCard(t, discardable, "【英魂※】选择弃置的一张牌", false,
                                             AIController::chooseLeastValuableCard(discardable));
            if (c) engine.discardCardOf(t, c, "英魂※");
        }
    } else {
        engine.drawCards(t, 1, "英魂※");
        for (int i = 0; i < x; ++i) {
            auto discardable = t->getHandAndEquipmentCards();
            if (discardable.empty()) break;
            CardPtr c = engine.askChooseCard(t, discardable, "【英魂※】选择第 " + std::to_string(i + 1) +
                                             " 张弃置牌", false,
                                             AIController::chooseLeastValuableCard(discardable));
            if (!c) break;
            engine.discardCardOf(t, c, "英魂※");
        }
    }
}

// 【英魂】受伤时准备阶段令一名角色摸 X 弃 Y（canActivate 已要求受伤）：有别人在场就值得发动。
bool MouXingYingHunSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return !engine.getOtherAlivePlayers(self).empty();
}

// ---------------- 谋·祝融 ----------------

MouLieRenSkill::MouLieRenSkill()
    : TriggerSkill("谋-烈刃", mouText("谋·祝融", "谋-烈刃")) {}

void MouLieRenSkill::onShaFinished(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.target || ctx.target->getId() == self.getId()) return;
    if (ctx.target && ctx.target->getHandCardCount() == 0 && false) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr tgt = engine.getPlayerById(ctx.target->getId());
    if (!engine.askConfirm(me, "【烈刃】与 " + who(*tgt) + " 拼点？", tgt->getHandCardCount() > 0)) return;
    if (tgt->getHandCardCount() == 0 || self.getHandCardCount() == 0) return;
    bool won = engine.pindian(me, tgt, "烈刃");
    if (won) {
        std::vector<PlayerPtr> others;
        for (auto& p : engine.getAlivePlayers()) if (p->getId() != tgt->getId()) others.push_back(p);
        if (others.empty()) return;
        PlayerPtr t2 = engine.askChoosePlayer(me, others, "【烈刃】对另一名角色造成1点伤害", true);
        if (t2) engine.applyDamage(me, t2, 1, ShaElement::NORMAL);
    }
}

MouJuXiangSkill::MouJuXiangSkill()
    : StateSkill("谋-巨象", mouText("谋·祝融", "谋-巨象"), SkillTag::LOCK) {}

void MouJuXiangSkill::onCardPlayed(GameEngine& engine, Player& self, CardPtr card) {
    if (card && card->getSubType() == CardSubType::NAN_MAN_RU_QIN && engine.isPlayerTurn(self))
        usedNanmanThisTurn = true;
}

void MouJuXiangSkill::onTurnBoundary(GameEngine&, Player&, Player&, bool starting) {
    if (starting) usedNanmanThisTurn = false;
}

void MouJuXiangSkill::onCheckCardEffect(GameEngine&, const Player& self, CardPtr card, bool& effective) {
    if (card && card->getSubType() == CardSubType::NAN_MAN_RU_QIN && self.getHero() &&
        self.getHero()->findSkill("谋-巨象"))
        effective = false;
}

void MouJuXiangSkill::onCardResolvedByAny(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    if (!card || card->getSubType() != CardSubType::NAN_MAN_RU_QIN ||
        user.getId() == self.getId() || !self.isAlive()) return;
    if (engine.getDeck().removeDiscardCard(card)) {
        engine.obtainCard(selfPtr(engine, self), card);
        engine.logMessage("  【巨象】" + who(self) + " 获得其他角色使用的【南蛮入侵】。");
    }
}

void MouJuXiangSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    if (engine.getCurrentPlayer() && engine.getCurrentPlayer()->getId() != self.getId()) return;
    if (usedNanmanThisTurn) return; // 本回合未使用过【南蛮入侵】
    // 随机从除外区（exilePile 储备区，原“游戏外”）取一张【南蛮入侵】交给一名角色
    static std::mt19937 xrng(std::random_device{}());
    auto nm = engine.takeFromExile(
        [](const CardPtr& c) { return c && c->getSubType() == CardSubType::NAN_MAN_RU_QIN; }, xrng);
    if (!nm) {
        // 除外区储备耗尽：按备牌规则补充一张实体南蛮入除外区
        nm = std::make_shared<Card>(-9001, "南蛮入侵", Suit::SPADE, 14, CardType::TRICK, CardSubType::NAN_MAN_RU_QIN);
    }
    PlayerPtr t = engine.askChoosePlayer(selfPtr(engine, self), engine.getAlivePlayers(),
                                         "【巨象】从游戏外交给一名角色一张【南蛮入侵】", true);
    if (t) {
        engine.obtainCard(t, nm);
        engine.logMessage("  【巨象】" + who(*t) + " 从游戏外获得一张【南蛮入侵】。");
    }
}


// ---------------- 谋·卢植 ----------------

MouMingRenSkill::MouMingRenSkill()
    : ActiveSkill("谋-明任", mouText("谋·卢植", "谋-明任")) {}

bool MouMingRenSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::FINISH || !engine.isPlayerTurn(self)) return false;
    if (self.getHandCardCount() == 0) return false;
    return self.getPileCount("任") > 0;
}

void MouMingRenSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【明任】用一张手牌替换“任”", false,
                                     AIController::chooseMostValuableCard(self.getHandCards()));
    if (!c) return;
    for (auto& old : self.getPile("任")) engine.getDeck().discardCard(old); // 旧任置入弃牌堆？（官文字面未说去向——保留：置入弃牌堆）
    setMark(self, "任清", 0);
    while (self.getPileCount("任") > 0) {
        auto pile = self.getPile("任");
        self.removeFromPile("任", pile.front());
    }
    engine.loseHandCard(me, c);
    self.addToPile("任", c);
    engine.logMessage("  【明任】" + who(self) + " 替换了“任”。");
}

// 【明任】结束阶段用手牌替换武将牌上的“任”：手里至少留 2 张，否则替换会把自己掏空。
bool MouMingRenSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return self.getHandCardCount() >= 2;
}

void MouMingRenSkill::onGameStart(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self);
    engine.drawCards(me, 2, "明任");
    if (self.getHandCardCount() == 0) return;
    CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【明任】将一张手牌扣置于武将牌上，称为“任”", false,
                                     AIController::chooseLeastValuableCard(self.getHandCards()));
    if (!c) c = AIController::chooseLeastValuableCard(self.getHandCards());
    if (c) {
        engine.loseHandCard(me, c);
        self.addToPile("任", c);
        engine.logMessage("  【明任】" + who(self) + " 扣置一张“任”。");
    }
}

MouZhenLiangSkill::MouZhenLiangSkill()
    : ActiveSkill("谋-贞良", mouText("谋·卢植", "谋-贞良"), 0, SkillTag::SWITCH) {}

bool MouZhenLiangSkill::canActivate(GameEngine& engine, Player& self) {
    if (!yang) return false; // 阴：回合外触发（在 onCardResolved）
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("贞良阳已用") > 0 || self.getPileCount("任") == 0) return false;
    CardPtr ren = self.getPile("任").front();
    if (!ren) return false;
    const bool red = ren->isRed();
    for (const auto& target : engine.getPlayersInRange(self, self.getAttackRange())) {
        if (!target || !target->isAlive() || target->getId() == self.getId()) continue;
        const int need = std::max(1, self.getHp() - target->getHp());
        int available = 0;
        for (const auto& card : self.getHandCards())
            if (card->isRed() == red) ++available;
        if (available >= need) return true;
    }
    return false;
}

void MouZhenLiangSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    CardPtr ren = self.getPile("任").front();
    const bool red = ren && ren->isRed();
    std::vector<PlayerPtr> cands;
    for (auto& p : engine.getPlayersInRange(self, self.getAttackRange())) {
        if (!p || !p->isAlive() || p->getId() == self.getId()) continue;
        const int need = std::max(1, self.getHp() - p->getHp());
        int available = 0;
        for (const auto& card : self.getHandCards()) if (card->isRed() == red) ++available;
        if (available >= need) cands.push_back(p);
    }
    if (cands.empty()) return;
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【贞良·阳】选择一名攻击范围内的其他角色", false);
    if (!t) return;
    setMark(self, "贞良阳已用", 1);
    int need = std::max(1, self.getHp() - t->getHp());
    std::vector<CardPtr> pay;
    for (auto& c : self.getHandCards()) if (c->isRed() == red) pay.push_back(c);
    if ((int)pay.size() < need) return; // 最终合法性复核，禁止不足时降级发动
    for (int i = 0; i < need; i++) {
        CardPtr c = pay[i];
        engine.discardCardOf(me, c, "贞良");
    }
    engine.applyDamage(me, t, 1, ShaElement::NORMAL);
    yang = false;
}

bool MouZhenLiangSkill::aiShouldActivate(GameEngine&, Player& self) { return self.isWounded(); }

void MouZhenLiangSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (yang) return; // 阴态
    if (engine.isPlayerTurn(self)) return;
    if (!card || self.getPileCount("任") == 0) return;
    CardPtr ren = self.getPile("任").front();
    if (card->getType() != ren->getType()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getAlivePlayers(), "【贞良·阴】令一名角色摸两张牌", false);
    if (t) engine.drawCards(t, 2, "贞良");
    yang = true;
}

// ---------------- 谋·诸葛亮 ----------------

MouHuoJiSkill::MouHuoJiSkill()
    : ActiveSkill("谋-火计", mouText("谋·诸葛亮", "谋-火计")) {}

bool MouHuoJiSkill::canActivate(GameEngine& engine, Player& self) {
    if (completed || failed) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("火计已用") > 0) return false;
    return !engine.getOtherAlivePlayers(self).empty();
}

void MouHuoJiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【火计】选择一名其他角色", false);
    if (!t) return;
    setMark(self, "火计已用", 1);
    engine.logMessage("  【火计】对 " + who(*t) + " 及其同势力角色各造成1点火焰伤害。");
    std::vector<PlayerPtr> victims{t};
    if (t->getHero())
        for (auto& p : engine.getPlayers())
            if (p->isAlive() && p->getId() != t->getId() && p->getHero() &&
                p->getHero()->getCountry() == t->getHero()->getCountry())
                victims.push_back(engine.getPlayerById(p->getId()));
    for (auto& v : victims)
        if (v->isAlive()) engine.applyDamage(me, v, 1, ShaElement::FIRE);
}

// 【火计】对一名角色及其**同势力**角色各造成 1 点火焰伤害：
// 只有当某个敌人的势力圈里“敌人多于队友”时才放，否则会烧到自己人。
bool MouHuoJiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    for (const auto& e : AIController::enemiesOf(engine, self)) {
        if (!e->getHero()) continue;
        Country c = e->getHero()->getCountry();
        int foes = 0, mates = 0;
        for (const auto& p : engine.getAlivePlayers()) {
            if (!p->getHero() || p->getHero()->getCountry() != c) continue;
            if (p->getId() == self.getId()) continue;
            if (AIController::isFriend(engine, self, *p)) ++mates; else ++foes;
        }
        if (foes > mates) return true;
    }
    return false;
}

void MouHuoJiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) {
        setMark(self, "火计已用", 0);
        return;
    }
    if (phase != TurnPhase::PREPARATION || completed || failed) return;
    int need = (int)engine.getPlayers().size();
    if (self.getMark("火计累计") >= need) {
        completed = true;
        engine.removeHeroSkill(selfPtr(engine, self), "火计");
        engine.removeHeroSkill(selfPtr(engine, self), "看破");
        if (self.getHero()) {
            self.getHero()->addSkill(std::make_shared<MouGuanXingXinSkill>());
            self.getHero()->addSkill(std::make_shared<MouKongChengXinSkill>());
        }
        engine.logMessage("  【火计】使命成功！" + who(self) + " 失去“火计”“看破”，获得“观星※”“空城※”。");
    }
}

void MouHuoJiSkill::onAfterDealDamage(GameEngine&, Player& self, Player* target, int damage,
                                       ShaElement element, CardPtr) {
    if (!failed && target && target->getId() != self.getId() && element == ShaElement::FIRE && damage > 0)
        self.addMark("火计累计", damage);
}

void MouHuoJiSkill::onDying(GameEngine& engine, Player& self, Player& dying) {
    if (dying.getId() != self.getId() || completed || failed) return;
    failed = true;
    engine.logMessage("  【火计】" + who(self) + " 进入濒死，使命失败。");
}

MouKanPoSkill::MouKanPoSkill()
    : StateSkill("谋-看破", mouText("谋·诸葛亮", "谋-看破"), SkillTag::NONE) {}

void MouKanPoSkill::onRoundStart(GameEngine& engine, Player& self) {
    lastRoundRecorded = recorded;
    recorded.clear();
    PlayerPtr me = selfPtr(engine, self);
    // 2026-10-06 实现级复核：官网原文是“**每局游戏**最多记录4个牌名（斗地主/排位模式修改为2）”，
    // 原先把上限当成“每轮最多记录数”（每轮清零后又能记满 4 个）。改为**本局累计预算**。
    const int gameCap = (engine.getGameMode() == GameEngine::GameMode::JUNZHENG) ? 4 : 2;
    int cap = gameCap - recordedThisGame;
    if (cap <= 0) {
        engine.logMessage("  【看破】本局已记录满上限（" + std::to_string(gameCap) + " 个牌名），本轮不再记录。");
        return;
    }
    // 死循环修复（2026-10-05，实机 seed=143 挂起）：AI 的默认项恒为 0，
    // 若该牌名与上一轮记录相同就会 sameAsLast → continue → 再次选到同一项 → 永不退出。
    // 现在：① AI 默认项改为“第一个与上一轮不同的牌名”，没有可用牌名就直接结束记录；
    //       ② 加尝试次数上限兜底（人类反复选重复牌名也不会卡死）。
    int guard = 0;
    while ((int)recorded.size() < cap && guard++ < cap * 4) {
        std::vector<std::string> opts = {"杀", "闪", "桃", "过河拆桥", "顺手牵羊", "无中生有", "决斗", "南蛮入侵",
                                         "万箭齐发", "无懈可击", "结束记录"};
        std::vector<std::string> shown;
        for (auto& o : opts) if (std::find(recorded.begin(), recorded.end(), o) == recorded.end()) shown.push_back(o);
        int aiDefault = -1;
        for (size_t i = 0; i < shown.size(); ++i) {
            if (shown[i] == "结束记录") continue;
            if (std::find(lastRoundRecorded.begin(), lastRoundRecorded.end(), shown[i]) == lastRoundRecorded.end()) {
                aiDefault = static_cast<int>(i);
                break;
            }
        }
        if (me->isAI() && aiDefault < 0) break; // 所有可选牌名都与上一轮相同 → 结束记录
        int opt = engine.askChooseOption(me, shown, "【看破】选择并记录牌名（不可与上次相同）",
                                        aiDefault < 0 ? 0 : aiDefault);
        if (opt < 0 || opt >= (int)shown.size() || shown[opt] == "结束记录") break;
        bool sameAsLast = std::find(lastRoundRecorded.begin(), lastRoundRecorded.end(), shown[opt]) != lastRoundRecorded.end();
        if (sameAsLast) continue;
        recorded.push_back(shown[opt]);
        ++recordedThisGame; // 每局累计预算
        if (!engine.askConfirm(me, "继续记录？", false)) break;
    }
    engine.logMessage("  【看破】" + who(self) + " 记录了 " + std::to_string(recorded.size()) + " 个牌名。");
}

void MouKanPoSkill::onOtherCardUsedBefore(GameEngine& engine, Player& self, Player& user, CardPtr card, bool& cancelled) {
    // 其他角色“使用”与记录牌名相同的牌时：移除该记录、令此牌无效并摸一张牌
    if (!card) return;
    std::string n = card->getName();
    auto it = std::find(recorded.begin(), recorded.end(), n);
    if (it == recorded.end()) return;
    (void)user; // 钩子仅广播给非使用者
    PlayerPtr me = engine.getPlayerById(self.getId());
    if (!engine.askConfirm(me, "【看破】移除“" + n + "”记录令此牌无效并摸一张牌？", true)) return;
    recorded.erase(it);
    cancelled = true;
    engine.drawCards(me, 1, "看破");
    engine.logMessage("  【看破】" + who(self) + " 令【" + n + "】无效并摸一张牌。");
}

MouGuanXingXinSkill::MouGuanXingXinSkill()
    : ActiveSkill("谋-观星", mouText("谋·诸葛亮", "谋-观星※")) {}

CardPtr MouGuanXingXinSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    // 需要使用或打出手牌时，将“星”视为你的牌：由 onNeedResponseCard 处理（此处留空转化入口）
    (void)engine; (void)self; (void)card; (void)wanted;
    return nullptr;
}

MouKongChengXinSkill::MouKongChengXinSkill()
    : StateSkill("谋-空城", mouText("谋·诸葛亮", "谋-空城※"), SkillTag::LOCK) {}

void MouKongChengXinSkill::onTakeDamage(GameEngine& engine, Player& self, Player*, int& damage, ShaElement) {
    if (!self.getHero() || !self.getHero()->findSkill("谋-观星")) return;
    int stars = self.getPileCount("星");
    if (stars > 0) {
        CardPtr j = engine.getDeck().drawCard();
        if (!j) return;
        engine.logMessage("  【空城※】判定牌为 " + j->getFormattedName() + "（点数 " + std::to_string(j->getRank()) + "）。");
        if (j->getRank() <= stars) damage = std::max(0, damage - 1);
        engine.getDeck().discardCard(j);
    } else {
        damage += 1;
        engine.logMessage("  【空城※】没有“星”，受到的伤害+1。");
    }
}

// ---------------- 谋·关羽（重写） ----------------

MouWuShengSkill::MouWuShengSkill()
    : TriggerSkill("谋-武圣", mouText("谋·关羽", "谋-武圣")) {}

CardPtr MouWuShengSkill::convertCard(GameEngine& engine, Player& self, CardPtr card, CardSubType wanted) {
    if (!card || wanted != CardSubType::SHA) return nullptr;
    if (!self.hasHandCard(card)) return nullptr;
    (void)engine;
    return Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {card}, "武圣");
}

void MouWuShengSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY || !engine.isPlayerTurn(self) ||
        self.getMark("武圣指定未用") != 0) return;
    // “主公以外”限制的是身份，不是与技能持有者的关系；非主公持有者可选自己。
    std::vector<PlayerPtr> cands;
    for (auto& p : engine.getAlivePlayers())
        if (p->getIdentity() != Identity::ZHU_GONG) cands.push_back(p);
    if (cands.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr ai;
    for (const auto& p : cands)
        if (!AIController::isFriend(engine, self, *p)) { ai = p; break; }
    if (!ai) ai = cands.front();
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【武圣】指定一名主公以外的角色", true, ai);
    if (!t) return;
    setMark(self, "武圣指定", t->getId() + 1);
    setMark(self, "武圣指定未用", 1);
    setMark(self, "武圣对其出杀", 0);
    engine.logMessage("  【武圣】" + who(self) + " 指定了 " + who(*t) + "（至本阶段结束）。");
}

void MouWuShengSkill::onCheckShaTarget(GameEngine& engine, const Player& self, const Player& target,
                                       CardPtr, bool& canTarget) {
    if (self.getMark("武圣指定") - 1 != target.getId() || engine.getCurrentPhase() != TurnPhase::PLAY)
        return;
    if (self.getMark("武圣对其出杀") >= 3) {
        canTarget = false; // 达到三张后应从合法候选中移除，而不是选中后才令杀无效。
        return;
    }
    canTarget = true; // 无距离限制
}

void MouWuShengSkill::onCalculateDistance(GameEngine& engine, const Player& self, const Player& target, int& distance) {
    if (self.getMark("武圣指定") - 1 == target.getId() && engine.getCurrentPhase() == TurnPhase::PLAY &&
        self.getMark("武圣对其出杀") < 3) {
        distance = 1; // 对指定角色无距离限制
    }
    (void)engine;
}

void MouWuShengSkill::onPhaseEnd(GameEngine&, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::PLAY) return;
    setMark(self, "武圣指定", 0);
    setMark(self, "武圣指定未用", 0);
    setMark(self, "武圣对其出杀", 0);
}

bool MouWuShengSkill::canUseShaBeyondLimitOn(GameEngine& engine, const Player& self,
                                                   const Player& target, CardPtr) {
    return engine.getCurrentPhase() == TurnPhase::PLAY &&
           self.getMark("武圣指定") == target.getId() + 1 &&
           self.getMark("武圣对其出杀") < 3;
}

void MouWuShengSkill::onShaTargeted(GameEngine& engine, Player& self, ShaContext& ctx) {
    if (!ctx.target) return;
    if (self.getMark("武圣指定") - 1 == ctx.target->getId() &&
        engine.getCurrentPhase() == TurnPhase::PLAY) {
        int n = self.getMark("武圣对其出杀");
        if (n >= 3) {
            ctx.invalidTarget = true; // 使用三张杀后不可再以其为目标
            engine.logMessage("  【武圣】已对其使用三张【杀】，不可再指定其为目标。");
            return;
        }
        setMark(self, "武圣对其出杀", n + 1);
        int draw = (engine.getGameMode() == GameEngine::GameMode::JUNZHENG) ? 2 : 1;
        engine.drawCards(selfPtr(engine, self), draw, "武圣");
        engine.logMessage("  【武圣】对指定角色使用【杀】，摸" + std::to_string(draw) + "张牌。");
    }
}

void MouWuShengSkill::resetTurnState() {}

MouYiJueSkill::MouYiJueSkill()
    : StateSkill("谋-义绝", mouText("谋·关羽", "谋-义绝"), SkillTag::LOCK) {}

void MouYiJueSkill::onDealDamage(GameEngine& engine, Player& self, Player& target, int& damage, ShaElement, CardPtr) {
    if (damage <= 0 || !self.isAlive() || !target.isAlive()) return;
    if (!engine.isPlayerTurn(self)) return;
    if (target.getId() == self.getId()) return;
    if (self.getMark("义绝已用:" + std::to_string(target.getId())) > 0) return; // 本局每名角色限一次
    if (target.getHp() - damage > 0) return; // 非濒死不触发
    damage = 0;
    setMark(self, "义绝已用:" + std::to_string(target.getId()), 1);
    setMark(self, "义绝取消:" + std::to_string(target.getId()), 1);
    engine.logMessage("  【义绝】" + who(self) + " 对 " + who(target) + " 的将死伤害被防止（本局首次），本回合对其用牌均取消。");
}

void MouYiJueSkill::onCheckCardTargetAsSource(GameEngine& engine, const Player& self, const Player& target,
                                      CardPtr, bool& canTarget) {
    if (self.getMark("义绝取消:" + std::to_string(target.getId())) > 0) {
        canTarget = false;
    }
    (void)engine;
}

void MouYiJueSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (!starting && turnOwner.getId() == self.getId()) {
        // 清除本回合的取消标记，保留本局一次标记
        for (auto& p : engine.getPlayers()) {
            if (p && self.getMark("义绝取消:" + std::to_string(p->getId())) > 0)
                setMark(self, "义绝取消:" + std::to_string(p->getId()), 0);
        }
    }
}

// ---------------- 谋·黄月英 ----------------

MouJiZhiSkill::MouJiZhiSkill()
    : TriggerSkill("谋-集智", mouText("谋·黄月英", "谋-集智"), SkillTag::LOCK) {}

void MouJiZhiSkill::onUseCard(GameEngine& engine, Player& self, CardPtr card) {
    if (!isNormalTrick(card)) return;
    self.addMark("集智不计上限", 1);
    engine.drawCards(selfPtr(engine, self), 1, "集智");
}

MouQiCaiSkill::MouQiCaiSkill()
    : ActiveSkill("谋-奇才", mouText("谋·黄月英", "谋-奇才")) {}

bool MouQiCaiSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("奇才已用") > 0) return false;
    if (engine.getOtherAlivePlayers(self).empty()) return false;
    for (auto& p : engine.getPlayers())
        if (!p->getAllEquipment().empty()) return true;
    // 弃牌堆装备
    for (auto& c : engine.getDeck().getDiscardPile())
        if (c && c->getType() == CardType::EQUIPMENT) return true;
    return false;
}

void MouQiCaiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【奇才】选择一名其他角色", false);
    if (!t) return;
    std::vector<CardPtr> eqs;
    for (auto& p : engine.getPlayers()) {
        auto eq = p->getAllEquipment();
        eqs.insert(eqs.end(), eq.begin(), eq.end());
    }
    for (auto& c : engine.getDeck().getDiscardPile())
        if (c && c->getType() == CardType::EQUIPMENT) eqs.push_back(c);
    if (eqs.empty()) return;
    CardPtr c = engine.askChooseCard(me, eqs, "【奇才】选择一张装备牌置入其装备区", false,
                                     AIController::chooseLeastValuableCard(eqs));
    if (!c) return;
    // 若来自弃牌堆则取出；若来自角色装备区则移动
    bool fromDiscard = false;
    for (auto& d : engine.getDeck().getDiscardPile())
        if (d == c) { fromDiscard = true; break; }
    if (fromDiscard) {
        engine.getDeck().removeDiscardCard(c);
        engine.obtainCard(t, c, me);
    } else {
        PlayerPtr owner;
        for (auto& pp : engine.getPlayers()) {
            auto eq2 = pp->getAllEquipment();
            if (std::find(eq2.begin(), eq2.end(), c) != eq2.end()) { owner = pp; break; }
        }
        if (owner && owner->getId() != t->getId()) engine.moveFieldCard(owner, t, c);
    }
    setMark(self, "奇才已用", 1);
    setMark(*t, "奇", 1);
    t->addMark("奇剩余", 3);
    engine.logMessage("  【奇才】" + who(*t) + " 获得“奇”标记。");
}

// 【奇才】把一张装备牌置入一名其他角色的装备区，令其获得“奇”（其之后获得的 3 张普通锦囊交给你）：
// 挂在**敌人**身上才划算，且手里或弃牌堆里得有装备牌（canActivate 已校验来源存在）。
bool MouQiCaiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return AIController::hasEnemy(engine, self);
}

void MouQiCaiSkill::onCalculateDistance(GameEngine&, const Player&, const Player&, int&) {
    // 普通锦囊的无距离限制按牌种处理。
}

void MouQiCaiSkill::onCheckCardTargetAsSource(GameEngine&, const Player& self, const Player&, CardPtr card,
                                      bool& canTarget) {
    if (card && isNormalTrick(card) && true)
        canTarget = true;
}

void MouQiCaiSkill::onAnyCardObtained(GameEngine& engine, Player& self, Player& recipient, CardPtr card) {
    if (!card || !isNormalTrick(card) || recipient.getId() == self.getId() ||
        recipient.getMark("奇") == 0 || recipient.getMark("奇剩余") <= 0) return;
    recipient.addMark("奇剩余", -1);
    engine.obtainCard(selfPtr(engine, self), card, engine.getPlayerById(recipient.getId()));
    engine.logMessage("  【奇才】" + who(recipient) + " 获得的普通锦囊交给了 " + who(self) + "。");
}


// ---------------- 谋·小乔 ----------------

MouTianXiangSkill::MouTianXiangSkill()
    : ActiveSkill("谋-天香", mouText("谋·小乔", "谋-天香")) {}

bool MouTianXiangSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("天香已用") >= 3) return false;
    for (auto& c : self.getHandCards()) {
        int s = (int)engine.effectiveSuit(self, c);
        if (s == (int)Suit::HEART || s == (int)Suit::DIAMOND) return true;
    }
    return false;
}

void MouTianXiangSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    std::vector<CardPtr> reds;
    for (auto& c : self.getHandCards()) {
        int s = (int)engine.effectiveSuit(self, c);
        if (s == (int)Suit::HEART || s == (int)Suit::DIAMOND) reds.push_back(c);
    }
    if (reds.empty()) return;
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self),
                                         "【天香】交给一名没有“天香”标记的红色手牌", false);
    if (!t) return;
    if (t->getMark("天香") > 0) return;
    CardPtr c = engine.askChooseCard(me, reds, "【天香】选择一张红色手牌", false, reds.front());
    if (!c) return;
    engine.obtainCard(t, c, me);
    self.addMark("天香已用", 1);
    int suit = (int)engine.effectiveSuit(self, c);
    setMark(*t, "天香", suit);
    t->addMark("天香花色数", 1);
    engine.logMessage("  【天香】" + who(*t) + " 获得“天香”标记。");
}

// 【天香】把一张红色手牌交给一名没有“天香”标记的角色 → 自己受伤时可移除其标记：
// 红桃＝防止伤害并令其受到 1 点伤害；方块＝其交给你两张牌。标记要挂在**敌人**身上。
bool MouTianXiangSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (self.getHandCardCount() < 2) return false;
    for (const auto& p : engine.getOtherAlivePlayers(self)) {
        if (p->getMark("天香") > 0 || p->getMark("天香红桃") > 0 || p->getMark("天香方块") > 0) continue;
        if (!AIController::isFriend(engine, self, *p)) return true;
    }
    return false;
}

void MouTianXiangSkill::onTakeDamage(GameEngine& engine, Player& self, Player* source, int& damage, ShaElement) {
    // 选择一名拥有“天香”标记的角色，移除并按花色发动
    std::vector<PlayerPtr> owners;
    for (auto& p : engine.getPlayers())
        if (p->isAlive() && p->getMark("天香") > 0) owners.push_back(engine.getPlayerById(p->getId()));
    if (owners.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr owner = engine.askChoosePlayer(me, owners, "【天香】选择一名拥有“天香”标记的角色", false);
    if (!owner) return;
    if (!engine.askConfirm(me, "【天香】移除 " + who(*owner) + " 的“天香”标记并按花色结算？", true)) return;
    int suit = owner->getMark("天香");
    setMark(*owner, "天香", 0);
    if (suit == (int)Suit::HEART) {
        damage = 0;
        if (source && source->isAlive()) {
            PlayerPtr src = engine.getPlayerById(source->getId());
            engine.applyDamage(src, owner, 1, ShaElement::NORMAL); // 来源角色对带有“天香”标记的角色造成1点伤害
        }
        engine.logMessage("  【天香】红桃：防止伤害并令来源受到1点伤害。");
    } else if (suit == (int)Suit::DIAMOND) {
        for (int i = 0; i < 2 && owner->getHandCardCount() > 0; i++) {
            CardPtr c = engine.askChooseCard(owner, owner->getHandCards(), "【天香】交给小乔一张牌", false,
                                             AIController::chooseMostValuableCard(owner->getHandCards()));
            if (!c) break;
            engine.obtainCard(me, c, owner);
        }
        engine.logMessage("  【天香】方块：" + who(*owner) + " 交给你两张牌。");
    }
}

void MouTianXiangSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PREPARATION || !engine.isPlayerTurn(self)) return;
    int total = 0;
    for (auto& p : engine.getPlayers()) {
        if (p->getMark("天香") > 0) {
            total += p->getMark("天香花色数");
            setMark(*p, "天香", 0);
            setMark(*p, "天香花色数", 0);
        }
    }
    if (total > 0) {
        if (engine.getGameMode() == GameEngine::GameMode::PAIWEI) total += 2;
        engine.drawCards(selfPtr(engine, self), total, "天香");
        engine.logMessage("  【天香】清除场上 " + std::to_string(total) + " 个“天香”标记并摸等量的牌。");
    }
    setMark(self, "天香已用", 0);
}

MouHongYanSkill::MouHongYanSkill()
    : StateSkill("谋-红颜", mouText("谋·小乔", "谋-红颜"), SkillTag::LOCK) {}

void MouHongYanSkill::onBeforeJudge(GameEngine& engine, Player& self, Player&, CardPtr& judgeCard) {
    if (!judgeCard) return;
    if (judgeCard->getSuit() == Suit::SPADE) {
        judgeCard = judgeCard->copyWithSuit(Suit::HEART);
        engine.logMessage("  【红颜】黑桃判定牌视为红桃。");
    }
    if (judgeCard->getSuit() == Suit::HEART) {
        int opt = engine.askChooseOption(selfPtr(engine, self), {"红桃", "方块", "梅花", "黑桃"},
                                         "【红颜】将判定结果改为由你指定的一种花色", 0);
        int suits[4] = {(int)Suit::HEART, (int)Suit::DIAMOND, (int)Suit::CLUB, (int)Suit::SPADE};
        judgeCard = judgeCard->copyWithSuit((Suit)suits[opt]);
        engine.logMessage("  【红颜】判定结果改为指定花色。");
    }
}

// ---------------- 谋·公孙瓒 ----------------

MouYiCongSkill::MouYiCongSkill()
    : ActiveSkill("谋-义从", mouText("谋·公孙瓒", "谋-义从")) {}

bool MouYiCongSkill::canActivate(GameEngine&, Player&) { return false; } // 在每轮开始时经 onRoundStart 发动

void MouYiCongSkill::onGameStart(GameEngine&, Player& self) {
    // 蓄力技（2/4）：初始2点蓄力，上限4
    if (self.getMark("蓄力") < 2) setMark(self, "蓄力", 2);
    setMark(self, "蓄力上限", 4);
}

void MouYiCongSkill::onActivate(GameEngine&, Player&) {}

// 【义从】不是主动技：每轮开始时经 onRoundStart 发动（canActivate 恒为 false），AI 不应在出牌阶段空转。
bool MouYiCongSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return false;
}

void MouYiCongSkill::onRoundStart(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self); // FAQ（2026-10-04）：“消耗至多x点”允许 x=0 发动
    // 三选项：方向-1（扈·杀）、方向+1（扈·闪）、不发动；AI 按局势自动选择，无“方向=0”的非法状态
    std::vector<std::string> dirOpts = {
        "消耗x点蓄力点：你与其他角色距离-1并获得x张“扈·杀”",
        "消耗x点蓄力点：其他角色与你距离+1并获得x张“扈·闪”",
        "不发动"
    };
    int aiDir = 0;
    if (me->isAI()) {
        // C4（转换技档位选择）：按局势在两个档位间取舍，而不是固定选一个——
        //   进攻档（距离-1 + 扈·杀）：有敌人超出攻击范围够不着时必须选它；
        //   防御档（距离+1 + 扈·闪）：自己低血/少牌，或敌方人数占优（要被集火）时选它。
        const int foes = AIController::enemyCount(engine, self);
        const bool pressed = (self.getHp() <= 2 || self.getHandCardCount() <= 2 || foes >= 3);
        bool needRange = false;
        for (const auto& t : AIController::enemiesOf(engine, self)) {
            if (!t) continue;
            if (engine.calculateDistance(self, *t) > self.getAttackRange()) { needRange = true; break; }
        }
        if (needRange && self.getHp() >= 2) aiDir = 0;   // 够不着就先解决距离（但快死了还是先保命）
        else aiDir = pressed ? 1 : 0;
    }
    int opt = engine.askChooseOption(me, dirOpts, "【义从】每轮开始时选择一项", aiDir);
    if (opt == 2 || opt < 0) return; // 不发动：方向保持 0（无效果），本轮标记不更新
    int total = self.getMark("蓄力");
    // 至多x点：交互选择消耗点数（0..total；AI 默认保留最后 1 点，x=0 时只拿距离效果）
    std::vector<std::string> opts;
    opts.push_back("消耗0点蓄力点");
    for (int i = 1; i <= total; i++) opts.push_back("消耗" + std::to_string(i) + "点蓄力点");
    int aiSpendIdx = total; // AI 默认全耗（既有行为）
    int sp = engine.askChooseOption(me, opts, "【义从】消耗至多x点蓄力点（选择消耗数）", aiSpendIdx);
    int spend = std::max(0, std::min(sp, total));
    int x = spend;
    setMark(self, "蓄力", total - spend);
    setMark(self, "义从方向", opt == 0 ? 1 : 2); // 1=距离-1方向，2=距离+1方向（0表示未发动）
    setMark(self, "义从本轮", engine.getCurrentRound());
    std::string pileName = "扈";
    int have = self.getPileCount(pileName);
    int add = std::max(0, 4 - have);
    add = std::min(add, x);
    // 使用引擎的随机源（受 THKS_SEED 控制）：此前用 std::rand() 建局部引擎，
    // 导致同一 seed 的对局不可复现（2026-10-05 排查挂起时发现）。
    for (int i = 0; i < add; i++) {
        auto c = engine.getDeck().drawRandomMatching([opt](const CardPtr& c2) {
            return c2 && c2->getSubType() == (opt == 0 ? CardSubType::SHA : CardSubType::SHAN);
        }, engine.getRng());
        if (!c) break;
        self.addToPile(pileName, c);
    }
    engine.logMessage("  【义从】" + who(self) + " 消耗" + std::to_string(spend) + "点蓄力，方向=" + std::to_string(opt == 0 ? -1 : 1) +
                      "，获得“扈”牌 " + std::to_string(self.getPileCount(pileName)) + " 张。");
}

void MouYiCongSkill::onCalculateDistance(GameEngine& engine, const Player& self, const Player& target, int& distance) {
    // 方向一：你与其他角色距离-1（自己为距离来源时）
    if (self.getMark("义从本轮") != engine.getCurrentRound()) return;
    if (self.getMark("义从方向") == 1 && self.getId() != target.getId()) distance -= 1;
}

void MouYiCongSkill::onCalculateDistanceToYou(GameEngine& engine, const Player& self, const Player& otherFrom, int& distance) {
    // 方向二：其他角色与你距离+1（目标侧广播）
    if (self.getMark("义从本轮") != engine.getCurrentRound()) return;
    if (self.getMark("义从方向") == 2 && self.getId() != otherFrom.getId()) distance += 1;
}

bool MouYiCongSkill::onNeedResponseCard(GameEngine& engine, Player& self, CardSubType wanted, CardPtr& out) {
    if (self.getPileCount("扈") == 0) return false;
    for (auto& c : self.getPile("扈")) {
        if (c->getSubType() == wanted) {
            PlayerPtr me = selfPtr(engine, self);
            if (engine.askConfirm(me, "【义从】将“扈”视为打出 " + c->getFormattedName() + "？", true)) {
                self.removeFromPile("扈", c);
                engine.getDeck().discardCard(c);
                out = Card::makeVirtual(c->getName(), c->getType(), c->getSubType(), {c}, "义从");
                return true;
            }
        }
    }
    return false;
}

MouQiaoMengSkill::MouQiaoMengSkill()
    : TriggerSkill("谋-趫猛", mouText("谋·公孙瓒", "谋-趫猛")) {}

void MouQiaoMengSkill::onAfterDealDamage(GameEngine& engine, Player& self, Player* target, int damage,
                                         ShaElement, CardPtr cause) {
    if (damage <= 0 || !target || !cause || cause->getSubType() != CardSubType::SHA) return;
    if (!self.getHero() || !self.getHero()->findSkill("谋-义从")) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.getPlayerById(target->getId());
    int opt = engine.askChooseOption(me, {"弃置其区域内一张牌并摸一张牌", "获得3蓄力点"}, "【趫猛】选择一项", 0);
    if (opt == 0) {
        std::vector<CardPtr> zone = t->getAllCards();
        if (!zone.empty()) {
            CardPtr c = AIController::chooseLeastValuableCard(zone);
            if (c) engine.discardCardOf(t, c, "趫猛");
        }
        engine.drawCards(me, 1, "趫猛");
    } else {
        // +3 蓄力点，但不超过蓄力上限 Y（FAQ 2026-10-04）。
        int cap = self.getMark("蓄力上限") > 0 ? self.getMark("蓄力上限") : 4;
        setMark(self, "蓄力", std::min(cap, self.getMark("蓄力") + 3));
    }
}

// ---------------- 谋·韩当 ----------------

MouGongQiSkill::MouGongQiSkill()
    : ActiveSkill("谋-弓骑", mouText("谋·韩当", "谋-弓骑")) {}

bool MouGongQiSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self) || !phaseStartAvailable) return false;
    return !self.getHandAndEquipmentCards().empty();
}

void MouGongQiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    auto cards = self.getHandAndEquipmentCards();
    CardPtr c = engine.askChooseCard(me, cards, "【弓骑】弃置一张牌令本回合攻击范围无限", false,
                                     AIController::chooseLeastValuableCard(cards));
    if (!c) return;
    const bool equipment = self.hasEquipment(c);
    engine.discardCardOf(me, c, "弓骑");
    phaseStartAvailable = false;
    setMark(self, "弓骑无限", 1);
    if (equipment) {
        std::vector<PlayerPtr> others = engine.getOtherAlivePlayers(self);
        if (!others.empty()) {
            PlayerPtr target = engine.askChoosePlayer(me, others, "【弓骑】选择一名其他角色弃置一张牌", true);
            if (target) {
                auto zone = target->getHandAndEquipmentCards();
                if (!zone.empty()) {
                    CardPtr stolen = engine.askChooseCard(me, zone, "【弓骑】选择要弃置的牌", true,
                                                           AIController::chooseLeastValuableCard(zone));
                    if (stolen) engine.discardCardOf(target, stolen, "弓骑");
                }
            }
        }
    }
}

bool MouGongQiSkill::aiShouldActivate(GameEngine&, Player& self) { return self.getHandCardCount() > 0; }

void MouGongQiSkill::onCheckShaTarget(GameEngine&, const Player& self, const Player&, CardPtr, bool& canTarget) {
    if (self.getMark("弓骑无限") > 0) canTarget = true;
}

void MouGongQiSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase == TurnPhase::PLAY && engine.isPlayerTurn(self)) {
        phaseStartAvailable = true;
        setMark(self, "弓骑无限", 0);
    }
}

void MouGongQiSkill::onPhaseEnd(GameEngine&, Player& self, TurnPhase phase) {
    if (phase == TurnPhase::PLAY) {
        phaseStartAvailable = false;
        setMark(self, "弓骑无限", 0);
    }
}

// ---------------- 谋·陆逊 ----------------

MouQianXunSkill::MouQianXunSkill()
    : TriggerSkill("谋-谦逊", mouText("谋·陆逊", "谋-谦逊")) {}

void MouQianXunSkill::onCheckCardEffect(GameEngine& engine, const Player& self, CardPtr card, bool& effective) {
    if (!isAnyTrick(card)) return;   // 延时锦囊同样记录（用户 2026-10-05）
    (void)engine;
    (void)effective;
    std::string n = card->getName();
    // 目标侧已确认牌对陆逊生效；使用者排除由全局结算广播完成。
    const_cast<Player&>(self).addMark("谦逊记录:" + n, 1);
    setMark(const_cast<Player&>(self), "谦逊待记录:" + n, 1);
}

void MouQianXunSkill::onCardTargetConfirmed(GameEngine&, Player& self, Player* source, CardPtr card,
                                             const std::vector<PlayerPtr>& targets) {
    if (!source || source->getId() == self.getId() || !isAnyTrick(card)) return;
    for (const auto& target : targets) {
        if (target && target->getId() == self.getId()) {
            setMark(self, "谦逊记录:" + card->getName(), 1);
            setMark(self, "谦逊待记录:" + card->getName(), 1);
            break;
        }
    }
}

void MouQianXunSkill::onAnyCardUsed(GameEngine&, Player&, Player&, CardPtr) {
    // 【谦逊】只由“锦囊对陆逊生效”的目标确认/结算钩子记录，
    // 不能因其他角色使用了未指定陆逊为目标的锦囊而记录牌名。
}

void MouQianXunSkill::onCardResolved(GameEngine& engine, Player& self, CardPtr card) {
    recordResolved(engine, self, card);
}

void MouQianXunSkill::onCardResolvedByAny(GameEngine& engine, Player& self, Player& user, CardPtr card) {
    if (user.getId() == self.getId()) return;
    recordResolved(engine, self, card);
}

void MouQianXunSkill::recordResolved(GameEngine& engine, Player& self, CardPtr card) {
    if (!isAnyTrick(card)) return;   // 含延时锦囊
    std::string n = card->getName();
    // 必须已经由目标确认钩子确认该牌指定陆逊且实际生效。
    if (self.getMark("谦逊待记录:" + n) == 0 && engine.getCurrentPhase() != TurnPhase::FINISH) return;
    setMark(self, "谦逊待记录:" + n, 0);
    // 记录之（牌名列表）并可将至多X张牌置于武将牌上
    if (self.getMark("谦逊记录:" + n) == 0) setMark(self, "谦逊记录:" + n, 1);
    if (std::find(recordedNames.begin(), recordedNames.end(), n) == recordedNames.end())
        recordedNames.push_back(n);
    recordedCount = (int)recordedNames.size();
    // 「此回合结束时获得」：记录放牌时的回合所有者
    pendingReturnOwner = engine.getCurrentPlayer() ? engine.getCurrentPlayer()->getId() : self.getId();
    int x = std::min(5, recordedCount); // X=“谦逊”记录的牌名数且至多为5
    if (x <= 0) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【谦逊】将至多 " + std::to_string(x) + " 张牌置于武将牌上？", true)) return;
    int n2 = std::min(x, self.getHandCardCount());
    for (int i = 0; i < n2; i++) {
        CardPtr c = AIController::chooseLeastValuableCard(self.getHandCards());
        if (!c) break;
        engine.loseHandCard(me, c);
        self.addToPile("谦屯", c);
    }
}

void MouQianXunSkill::onPhaseStart(GameEngine& engine, Player& self, TurnPhase phase, bool&) {
    if (phase != TurnPhase::PLAY) return;
    if (recordedNames.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    // 移去一个记录的牌名，若为普通锦囊牌的牌名，则你可视为使用此牌
    std::vector<std::string> opts = recordedNames;
    opts.push_back("不移去");
    // D9：AI 不再一律“不移去”——优先移去**能视为使用**的普通锦囊牌名（延时锦囊移去了也用不了），
    // 且优先收益高的（无中生有 > 顺手牵羊/过河拆桥 > 决斗/南蛮/万箭 > 其余）；只有延时锦囊时不移去。
    static const std::vector<std::string> kPriority = {
        "无中生有", "顺手牵羊", "过河拆桥", "决斗", "南蛮入侵", "万箭齐发", "借刀杀人", "火攻", "铁索连环",
        "无懈可击", "桃园结义", "五谷丰登",
    };
    int aiOpt = static_cast<int>(recordedNames.size()); // 默认“不移去”
    for (const auto& want : kPriority) {
        auto it = std::find(recordedNames.begin(), recordedNames.end(), want);
        if (it == recordedNames.end()) continue;
        aiOpt = static_cast<int>(it - recordedNames.begin());
        break;
    }
    int opt = engine.askChooseOption(me, opts, "【谦逊】移去一个记录的牌名", aiOpt);
    if (opt < 0 || opt >= (int)recordedNames.size()) { (void)engine; return; }
    std::string name = recordedNames[opt];
    recordedNames.erase(recordedNames.begin() + opt);
    recordedCount = (int)recordedNames.size();
    setMark(self, "谦逊记录:" + name, 0);
    // 「若为**普通锦囊牌**的牌名，则你可视为使用此牌」——延时锦囊的牌名可以移去，但不能视为使用。
    if (isDelayedTrickName(name)) {
        engine.logMessage("  【谦逊】移去牌名【" + name + "】（延时锦囊牌的牌名，不能视为使用）。");
        return;
    }
    // 随机源统一用引擎 rng（受 THKS_SEED 控制）：此前用 std::random_device 会破坏可复现性
    auto probe = engine.getDeck().drawRandomMatching(
        [name](const CardPtr& c) { return c && c->getName() == name; },
        engine.getRng());
    if (probe) {
        // 从牌堆找到对应实体：放回牌堆顶再视为使用会扰动；直接构造同名虚拟牌视为使用
        engine.getDeck().putOnTop({probe});
        auto v = Card::makeVirtual(name, CardType::TRICK,
            name == "过河拆桥" ? CardSubType::GUO_HE_CHAI_QIAO :
            name == "顺手牵羊" ? CardSubType::SHUN_SHOU_QIAN_YANG :
            name == "无中生有" ? CardSubType::WU_ZHONG_SHENG_YOU :
            name == "决斗" ? CardSubType::JUE_DOU :
            name == "南蛮入侵" ? CardSubType::NAN_MAN_RU_QIN :
            name == "万箭齐发" ? CardSubType::WAN_JIAN_QI_FA :
            name == "无懈可击" ? CardSubType::WU_XIE_KE_JI :
            name == "铁索连环" ? CardSubType::TIE_SUO_LIAN_HUAN :
            name == "火攻" ? CardSubType::HUO_GONG :
            name == "桃园结义" ? CardSubType::TAO_YUAN_JIE_YI :
            name == "五谷丰登" ? CardSubType::WU_GU_FENG_DENG :
            name == "借刀杀人" ? CardSubType::JIE_DAO_SHA_REN :
            name == "乐不思蜀" ? CardSubType::LE_BU_SI_SHU :
            name == "兵粮寸断" ? CardSubType::BING_LIANG_CUN_DUAN : CardSubType::WU_ZHONG_SHENG_YOU,
            {}, "谦逊");
        std::vector<PlayerPtr> tgs;
        if (name == "无中生有") tgs = {me};
        else if (name == "过河拆桥" || name == "顺手牵羊" || name == "决斗") {
            auto others = engine.getOtherAlivePlayers(self);
            if (!others.empty()) tgs = {others.front()};
        }
        engine.logMessage("  【谦逊】移去牌名【" + name + "】，视为使用之。");
        engine.useCard(me, v, tgs);
    } else {
        engine.logMessage("  【谦逊】移去牌名【" + name + "】（牌堆无此牌，无法视为使用）。");
    }
}

void MouQianXunSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    // 兜底：放牌发生在自己回合时于自己的结束阶段取回
    returnPile(engine, self);
}

void MouQianXunSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    // 「此回合结束时」= 放牌所在的那回合结束（含他人回合）
    if (starting) return;
    if (pendingReturnOwner >= 0 && turnOwner.getId() == pendingReturnOwner) returnPile(engine, self);
}

void MouQianXunSkill::returnPile(GameEngine& engine, Player& self) {
    while (self.getPileCount("谦屯") > 0) {
        auto cards = self.getPile("谦屯");
        for (auto& c : cards) self.removeFromPile("谦屯", c);
        for (auto& c : cards) engine.obtainCard(selfPtr(engine, self), c);
    }
    pendingReturnOwner = -1;
}

MouLianYingSkill::MouLianYingSkill()
    : TriggerSkill("谋-连营", mouText("谋·陆逊", "谋-连营")) {}

void MouLianYingSkill::onCardLostOutsideTurn(GameEngine&, Player&, CardPtr) {
    // 引擎仅在“非自己回合”广播：即本回合（他人回合）内你失去的牌数
    lostThisTurn++;
}

void MouLianYingSkill::onTurnBoundary(GameEngine&, Player& self, Player&, bool starting) {
    if (starting) lostThisTurn = 0;
}

void MouLianYingSkill::onTurnEnd(GameEngine& engine, Player& self, Player& turnOwner) {
    if (turnOwner.getId() == self.getId()) { lostThisTurn = 0; return; } // 自己回合不触发并清零
    int bonus = (engine.getGameMode() == GameEngine::GameMode::DOUDIZHU) ? 1 : 0;
    int x = std::min(5, lostThisTurn + bonus);
    lostThisTurn = 0;
    if (x <= 0) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【连营】观看牌堆顶 " + std::to_string(x) + " 张牌并分配？", true)) return;
    std::vector<CardPtr> cards = engine.getDeck().drawCards(x); // 真正取出
    if (cards.empty()) return;
    engine.viewCards(me, cards, "【连营】观看牌堆顶的牌");
    // 官网“将这些牌交给任意角色”：逐张选择交给任意一名角色（含自己）。
    for (auto& c : cards) {
        PlayerPtr t = engine.askChoosePlayer(me, engine.getAlivePlayers(),
                                             "【连营】将 " + c->getFormattedName() + " 交给一名角色", false, me);
        if (!t) t = me;
        engine.obtainCard(t, c);
    }
    engine.logMessage("  【连营】" + who(self) + " 将牌堆顶 " + std::to_string(cards.size()) + " 张牌交给任意角色。");
}


// ---------------- 谋·贾诩 ----------------

MouWanShaSkill::MouWanShaSkill()
    : StateSkill("谋-完杀", mouText("谋·贾诩", "谋-完杀"), SkillTag::NONE) {}

bool MouWanShaSkill::onNeedResponseCard(GameEngine&, Player&, CardSubType, CardPtr&) { return false; }

void MouWanShaSkill::onRoundStart(GameEngine& engine, Player& self) {
    setMark(self, "完杀本轮已用", 0); // 每轮限一次
    (void)engine;
}

void MouWanShaSkill::onDying(GameEngine& engine, Player& self, Player& dying) {
    // 每轮限一次：观看其手牌并选择零至两张牌，其二择（self==dying 时由濒死者自身技能广播）
    process(engine, self, dying);
}

void MouWanShaSkill::onOtherDying(GameEngine& engine, Player& self, Player& dying) {
    // 其他角色进入濒死状态（引擎新钩子）
    process(engine, self, dying);
}

void MouWanShaSkill::process(GameEngine& engine, Player& self, Player& dying) {
    if (self.getMark("完杀本轮已用") > 0) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr d = engine.getPlayerById(dying.getId());
    // 一级：观看其手牌；二级：观看其手牌并可选其区域内的牌（装备区）
    auto buildPool = [&]() {
        std::vector<CardPtr> pool = dying.getHandCards();
        if (level >= 2) {
            auto eq = dying.getAllEquipment();
            pool.insert(pool.end(), eq.begin(), eq.end());
            // 二级「其区域内」：判定区牌一并入池
            auto js = dying.getJudgeZone();
            pool.insert(pool.end(), js.begin(), js.end());
        }
        return pool;
    };
    if (buildPool().empty()) return;
    setMark(self, "完杀本轮已用", 1);
    std::vector<CardPtr> pick;
    int maxPick = 2;
    for (int i = 0; i < maxPick; i++) {
        auto pool = buildPool();
        pool.erase(std::remove_if(pool.begin(), pool.end(), [&](const CardPtr& c) {
            return std::find(pick.begin(), pick.end(), c) != pick.end();
        }), pool.end());
        if (pool.empty()) break;
        CardPtr c = engine.askChooseCard(me, pool, "【完杀】观看并选择至多两张牌（0=结束）", true, nullptr);
        if (!c) break;
        if (std::find(pick.begin(), pick.end(), c) == pick.end()) pick.push_back(c);
        if ((int)buildPool().size() <= (int)pick.size()) break;
    }
    int opt = engine.askChooseOption(d, {"由贾诩将被选择的牌分配给其以外的角色", "弃置所有未被选择的牌"},
                                     "【完杀】选择一项", 0);
    if (opt == 0) {
        // “由你将被选择的牌分配给其以外的角色”：逐张由贾诩指定目标，牌直接从濒死者移给目标
        // （不先经过贾诩，避免触发“一次获得牌”类技能）。
        auto others = engine.getOtherAlivePlayers(dying);
        for (auto& c : pick) {
            PlayerPtr t = nullptr;
            if (!others.empty()) {
                PlayerPtr prefer = (std::find(others.begin(), others.end(), me) != others.end()) ? me : others.front();
                t = engine.askChoosePlayer(me, others, "【完杀】将 " + c->getFormattedName() + " 分配给谁", false, prefer);
            }
            if (!t) t = me;
            if (t->getId() == dying.getId()) t = me; // 兜底：不得分配给其本人
            engine.obtainCard(t, c, d); // 手牌/装备/判定区牌均可直接移动
        }
    } else {
        std::vector<CardPtr> rest = buildPool();
        for (auto& c : pick) {
            auto it = std::find(rest.begin(), rest.end(), c);
            if (it != rest.end()) rest.erase(it);
        }
        for (auto& c : rest) engine.discardCardOf(d, c, "完杀");
    }
}

MouLuanWuSkill::MouLuanWuSkill()
    : ActiveSkill("谋-乱武", mouText("谋·贾诩", "谋-乱武"), 0, SkillTag::LIMITED) {}

bool MouLuanWuSkill::canActivate(GameEngine& engine, Player& self) {
    if (spent) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    return !engine.getOtherAlivePlayers(self).empty();
}

void MouLuanWuSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!engine.askConfirm(me, "【乱武】发动限定技：全场其他角色除非对最近角色用杀，否则失去1点体力？", true))
        return;
    spent = true;
    int lostCount = 0;
    for (auto& p : engine.getPlayers()) {
        if (!p->isAlive() || p->getId() == self.getId()) continue;
        // 找距离最小的另一名角色
        PlayerPtr nearest;
        int best = 99;
        for (auto& q : engine.getOtherAlivePlayers(*p)) {
            int d = engine.calculateDistance(*p, *q);
            if (d < best) { best = d; nearest = q; }
        }
        PlayerPtr pp = p;
        if (!nearest) { engine.loseHp(pp, 1, "乱武"); lostCount++; continue; }
        // 是否对 nearest 使用合法的【杀】（含技能转化牌）
        bool used = false;
        std::string prompt = "【乱武】对 " + who(*nearest) + " 使用一张【杀】，否则失去1点体力";
        if (engine.askConfirm(pp, prompt, true)) {
            CardPtr sha = engine.askUseSha(pp, prompt, true, nearest, false, true);
            if (sha) used = engine.useCard(pp, sha, {nearest});
        }
        if (!used) {
            engine.loseHp(pp, 1, "乱武");
            lostCount++;
            // 升级选择
            int opt = engine.askChooseOption(me, {"升级“完杀”", "升级“帷幕”"}, "【乱武】选择一个技能升级", 1);
            if (opt == 0) {
                if (auto s = self.getHero() ? self.getHero()->findSkill("谋-完杀") : nullptr)
                    if (auto ws = std::dynamic_pointer_cast<MouWanShaSkill>(s)) ws->upgrade();
            } else {
                if (auto s = self.getHero() ? self.getHero()->findSkill("谋-帷幕") : nullptr)
                    if (auto wm = std::dynamic_pointer_cast<MouWeiMuSkill>(s)) wm->upgrade();
            }
        }
    }
    engine.logMessage("  【乱武】" + std::to_string(lostCount) + " 名角色因此失去体力。");
}

// 【乱武】限定技：所有其他角色须对各自距离最小者用【杀】，否则失去 1 点体力。
// 一局一次，要在“敌人不少于队友”或“有敌人濒死边缘（体力≤1）”时开，否则等于帮对手清场。
bool MouLuanWuSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    if (engine.getOtherAlivePlayers(self).size() < 2) return false;
    if (AIController::enemyCount(engine, self) >= AIController::friendCount(engine, self)) return true;
    for (const auto& t : AIController::enemiesOf(engine, self))
        if (t->getHp() <= 1) return true;
    return false;
}

MouWeiMuSkill::MouWeiMuSkill()
    : StateSkill("谋-帷幕", mouText("谋·贾诩", "谋-帷幕"), SkillTag::LOCK) {}

void MouWeiMuSkill::onCheckCardTarget(GameEngine& engine, const Player& self, const Player& target,
                                      CardPtr card, bool& canTarget) {
    // 黑色锦囊牌以自己为目标时取消（被取消即未成为目标，不计数）
    if (&target == &self && card && card->getType() == CardType::TRICK &&
        (card->getSuit() == Suit::SPADE || card->getSuit() == Suit::CLUB)) {
        canTarget = false;
    }
    (void)engine; (void)self; (void)target;
}

// 「上一轮成为其他角色使用牌的目标的次数」在目标确定后计数（非候选阶段）
void MouWeiMuSkill::onCardTargetConfirmed(GameEngine& engine, Player& self, Player* source, CardPtr,
                                          const std::vector<PlayerPtr>& targets) {
    if (!source || source->getId() == self.getId()) return;
    for (auto& t : targets)
        if (t && t->getId() == self.getId()) { self.addMark("帷幕被打", 1); break; }
    (void)engine;
}

void MouWeiMuSkill::onRoundStart(GameEngine& engine, Player& self) {
    if (level < 2) return;
    // 上一轮成为其他角色使用牌目标的次数不大于一次：超过一次则不获得牌。
    if (self.getMark("帷幕被打") > 1) {
        setMark(self, "帷幕被打", 0);
        return;
    }
    setMark(self, "帷幕被打", 0);
    // 从弃牌堆随机（均匀）获得一张黑色锦囊牌或防具牌
    std::vector<CardPtr> cands;
    for (auto& c : engine.getDeck().getDiscardPile()) {
        if (!c) continue;
        bool ok = (c->getType() == CardType::TRICK && (c->getSuit() == Suit::SPADE || c->getSuit() == Suit::CLUB)) ||
                  (c->getType() == CardType::EQUIPMENT && c->getSubType() == CardSubType::ARMOR);
        if (ok) cands.push_back(c);
    }
    if (!cands.empty()) {
        static std::mt19937 wrng(std::random_device{}());
        std::uniform_int_distribution<size_t> d(0, cands.size() - 1);
        CardPtr pick = cands[d(wrng)];
        if (engine.getDeck().removeDiscardCard(pick)) {
            engine.obtainCard(selfPtr(engine, self), pick);
            engine.logMessage("  【帷幕】" + who(self) + " 从弃牌堆随机获得一张" + pick->getFormattedName() + "。");
        }
    }
}

// ---------------- 谋·诸葛瑾 ----------------

MouHuanShiSkill::MouHuanShiSkill()
    : TriggerSkill("谋-缓释", mouText("谋·诸葛瑾", "谋-缓释")) {}

void MouHuanShiSkill::onBeforeJudge(GameEngine& engine, Player& self, Player&, CardPtr& judgeCard) {
    PlayerPtr me = selfPtr(engine, self);
    CardPtr top = engine.getDeck().peekTop();
    if (!top) return;
    std::vector<std::string> opts = {"用牌堆顶的 " + top->getFormattedName() + " 代替"};
    if (self.getHandCardCount() > 0) opts.push_back("用手牌中的一张替换");
    opts.push_back("不替换");
    int opt = engine.askChooseOption(me, opts, "【缓释】观看牌堆顶一张牌并选择", (int)opts.size() - 1);
    if (opt == 0) {
        engine.getDeck().drawCard();
        judgeCard = top;
        engine.logMessage("  【缓释】判定牌替换为牌堆顶的 " + top->getFormattedName() + "。");
    } else if (opt == 1 && self.getHandCardCount() > 0) {
        CardPtr c = engine.askChooseCard(me, self.getHandCards(), "【缓释】用一张手牌替换判定牌", true,
                                         AIController::chooseLeastValuableCard(self.getHandCards()));
        if (c) {
            engine.loseHandCard(me, c);
            engine.getDeck().discardCard(judgeCard);
            judgeCard = c;
            engine.logMessage("  【缓释】判定牌被手牌替换。");
        }
    }
}

MouHongYuanSkill::MouHongYuanSkill()
    : TriggerSkill("谋-弘援", mouText("谋·诸葛瑾", "谋-弘援")) {}

void MouHongYuanSkill::onGameStart(GameEngine&, Player& self) {
    setMark(self, "蓄力上限", 3); // 蓄力技（1/3）
    // 蓄力技（1/3）：初始1点蓄力
    if (self.getMark("蓄力") < 1) setMark(self, "蓄力", 1);
}

void MouHongYuanSkill::onCardsObtained(GameEngine& engine, Player& self, int count) {
    // 当你一次获得不少于两张牌时，可消耗1点蓄力点令至多两名不同角色各摸一张牌；“角色”包括自己。
    if (count < 2 || self.getMark("蓄力") <= 0) return;
    PlayerPtr me = selfPtr(engine, self);
    if (!me || !engine.askConfirm(me, "【弘援】消耗1点蓄力点令至多两名角色各摸一张牌？", true)) return;
    auto candidates=engine.getAlivePlayers();
    std::vector<PlayerPtr> chosen;
    for (int i=0;i<2 && !candidates.empty();++i) {
        PlayerPtr target=engine.askChoosePlayer(me,candidates,"【弘援】令其摸一张牌（可取消）",true,
                                                candidates.front());
        if (!target) break;
        chosen.push_back(target);
        candidates.erase(std::remove(candidates.begin(),candidates.end(),target),candidates.end());
        if (i==0 && !engine.askConfirm(me,"再选择一名不同的角色？",false)) break;
    }
    // 先完成合法目标选择；取消第一目标时不消耗蓄力点、不留下空发动。
    if(chosen.empty())return;
    setMark(self, "蓄力", self.getMark("蓄力") - 1);
    for(const auto& target:chosen)
        if(target && target->isAlive())engine.drawCards(target,1,"弘援");
}

void MouHongYuanSkill::onCardsLostBatch(GameEngine& engine, Player& self, Player& victim, int count) {
    // 当一名其他角色一次失去不少于两张牌时，可消耗1点蓄力点令其摸一张牌（军争）
    if (count < 2 || self.getMark("蓄力") <= 0) return;
    if (victim.getId() == self.getId()) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr v = engine.getPlayerById(victim.getId());
    // 模式分流：军争摸一张；排位、斗地主摸两张
    int n = (engine.getGameMode() == GameEngine::GameMode::JUNZHENG) ? 1 : 2;
    if (!engine.askConfirm(me, "【弘援】消耗1点蓄力点令 " + who(victim) + " 摸" +
                          std::to_string(n) + "张牌？", true)) return;
    setMark(self, "蓄力", self.getMark("蓄力") - 1);
    engine.drawCards(v, n, "弘援");
}

MouMingZheSkill::MouMingZheSkill()
    : TriggerSkill("谋-明哲", mouText("谋·诸葛瑾", "谋-明哲"), SkillTag::LOCK) {}

void MouMingZheSkill::onCardLostOutsideTurn(GameEngine& engine, Player& self, CardPtr card) {
    if (usedThisRound >= 2) return;
    if (!card) return;
    usedThisRound++;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getAlivePlayers(), "【明哲】选择一名角色", false);
    if (!t) return;
    bool nonBasic = card->getType() != CardType::BASIC;
    // 若其有蓄力技：获得1点蓄力点（FAQ 2026-10-04：可获得，但不超过其蓄力上限 Y）。
    int cap = t->getMark("蓄力上限");
    if (cap > 0 && t->getMark("蓄力") < cap) t->addMark("蓄力", 1);
    if (nonBasic) engine.drawCards(t, 1, "明哲");
}

void MouMingZheSkill::onRoundStart(GameEngine&, Player&) { usedThisRound = 0; }

// ---------------- 谋·吕布 ----------------

MouWuShuangSkill::MouWuShuangSkill()
    : StateSkill("谋-无双", mouText("谋·吕布", "谋-无双"), SkillTag::LOCK) {}

void MouWuShuangSkill::onCalculateResponseCount(GameEngine&, const Player& self, const Player& responder,
                                                 CardSubType wanted, int& count) {
    if (&responder == &self) return;
    if (wanted == CardSubType::SHAN || wanted == CardSubType::SHA) {
        count = 2;
    }
}

void MouWuShuangSkill::onTurnBoundary(GameEngine& engine, Player& self, Player& turnOwner, bool starting) {
    if (starting) bonusUsedThisTurn = false;
    self.addMark("杀响应已打出", -self.getMark("杀响应已打出"));
    self.addMark("决斗已打杀", -self.getMark("决斗已打杀"));
    if (self.getMark("利驭无双") > 0 && turnOwner.getId() == self.getId()) {
        if (starting) setMark(self, "利驭无双已跨回合", 1);
        else if (self.getMark("利驭无双已跨回合") > 0) {
            engine.removeHeroSkill(selfPtr(engine, self), "谋-无双");
            setMark(self, "利驭无双", 0);
            setMark(self, "利驭无双已跨回合", 0);
        }
    }
}

void MouWuShuangSkill::onShaTargeted(GameEngine&, Player&, ShaContext&) {
    // 伤害+1 在 onDealDamage 判定（对方确实未打出杀/闪时），此处不再近似。
}

// 每回合限一次：对方没有使用或打出【杀】或【闪】→ 此【杀】或【决斗】对其伤害+1
void MouWuShuangSkill::onDealDamage(GameEngine& engine, Player& self, Player& target, int& damage,
                                    ShaElement, CardPtr cause) {
    if (bonusUsedThisTurn || !cause) return;
    bool isSha = cause->getSubType() == CardSubType::SHA;
    bool isDuel = cause->getSubType() == CardSubType::JUE_DOU;
    if (!isSha && !isDuel) return;
    // 对方（目标）没有使用或打出杀/闪
    if (isSha && target.getMark("杀响应已打出") > 0) return;
    if (isDuel && target.getMark("决斗已打杀") > 0) return;
    damage += 1;
    bonusUsedThisTurn = true;
    engine.logMessage("  【无双】" + who(self) + " 的伤害+1（每回合限一次）。");
}

void MouWuShuangSkill::onDuelTargeted(GameEngine& engine, Player& self, Player& source, Player& target) {
    (void)engine; (void)self; (void)source; (void)target;
}

MouLiYuSkill::MouLiYuSkill()
    : TriggerSkill("谋-利驭", mouText("谋·吕布", "谋-利驭")) {}

void MouLiYuSkill::onAfterDealDamage(GameEngine& engine, Player& self, Player* target, int damage,
                                      ShaElement, CardPtr cause) {
    if (!target || damage <= 0 || !cause || cause->getSubType() != CardSubType::SHA) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.getPlayerById(target->getId());
    int n = damage;
    int gained = 0;
    std::vector<CardPtr> gotMine;   // 你因此获得的牌
    for (int i = 0; i < n; i++) {
        std::vector<CardPtr> zone = t->getHandCards();
        auto eq = t->getAllEquipment();
        auto judge = t->getJudgeZone();
        zone.insert(zone.end(), eq.begin(), eq.end());
        zone.insert(zone.end(), judge.begin(), judge.end());
        if (zone.empty()) break;
        CardPtr c = engine.askChooseCard(me, zone, "【利驭】选择获得其区域内的一张牌（可取消）", true,
                                         AIController::chooseMostValuableCard(zone));
        if (!c) break;
        engine.obtainCard(me, c, t);
        gotMine.push_back(c);
        gained++;
    }
    // 其摸等量张牌（记录其因此摸到的牌）
    int tBefore = t->getHandCardCount();
    engine.drawCards(t, gained, "利驭");
    std::vector<CardPtr> drewTheirs;
    {
        auto after = t->getHandCards();
        for (size_t k = tBefore; k < after.size(); k++) drewTheirs.push_back(after[k]);
    }
    engine.logMessage("  【利驭】" + who(self) + " 获得 " + who(*t) + " " + std::to_string(gained) + " 张牌，其摸等量。");
    // 若你与其因此获得了全部类别的牌（基本/锦囊/装备）：由对方选择二择
    bool hasBasic = false, hasTrick = false, hasEquip = false;
    for (auto& c : gotMine) {
        if (c->getType() == CardType::BASIC) hasBasic = true;
        if (c->getType() == CardType::TRICK) hasTrick = true;
        if (c->getType() == CardType::EQUIPMENT) hasEquip = true;
    }
    for (auto& c : drewTheirs) {
        if (c->getType() == CardType::BASIC) hasBasic = true;
        if (c->getType() == CardType::TRICK) hasTrick = true;
        if (c->getType() == CardType::EQUIPMENT) hasEquip = true;
    }
    if (hasBasic && hasTrick && hasEquip) {
        int opt = engine.askChooseOption(t, {"令吕布视为对指定角色使用一张【决斗】", "其获得技能“无双”直至其下个回合结束"},
                                         "【利驭】已获得全部类别的牌，选择一项", 1);
        if (opt == 0) {
            PlayerPtr t2 = engine.askChoosePlayer(t, engine.getOtherAlivePlayers(*t), "【利驭】由其指定决斗目标", false);
            if (t2) {
                auto duel = Card::makeVirtual("决斗", CardType::TRICK, CardSubType::JUE_DOU, {}, "利驭");
                engine.useCard(me, duel, {t2});
            }
        } else {
            if (t->getHero()) t->getHero()->addSkill(std::make_shared<MouWuShuangSkill>());
            setMark(*t, "利驭无双", 1);
            engine.logMessage("  【利驭】" + who(*t) + " 获得“无双”直至其下个回合结束。");
        }
    }
}

// ---------------- 谋·朱然 ----------------

MouZhenWeiSkill::MouZhenWeiSkill()
    : ActiveSkill("谋-镇围", mouText("谋·朱然", "谋-镇围")) {}

bool MouZhenWeiSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    if (self.getMark("镇围已用") > 0) return false;
    return self.getHandCardCount() > 0;
}

void MouZhenWeiSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    PlayerPtr t = engine.askChoosePlayer(me, engine.getOtherAlivePlayers(self), "【镇围】选择一名其他角色同时弃牌", false);
    if (!t) return;
    // 我方弃牌数
    auto myDiscardable = self.getHandAndEquipmentCards();
    auto discardCountOptions = [](size_t available) {
        std::vector<std::string> result;
        for (size_t n = 0; n <= available; ++n)
            result.push_back("弃" + std::to_string(n) + "张");
        return result;
    };
    auto myCountOptions = discardCountOptions(myDiscardable.size());
    int mine = engine.askChooseOption(me, myCountOptions, "【镇围】选择弃置的牌数",
                                      static_cast<int>(myCountOptions.size()) - 1);
    std::vector<CardPtr> mineCards;
    for (int i = 0; i < mine && !myDiscardable.empty(); i++) {
        CardPtr c = engine.askChooseCard(me, myDiscardable, "【镇围】选择第 " + std::to_string(i + 1) + " 张弃置牌",
                                         false, AIController::chooseLeastValuableCard(myDiscardable));
        if (!c) break;
        mineCards.push_back(c);
        myDiscardable.erase(std::remove(myDiscardable.begin(), myDiscardable.end(), c), myDiscardable.end());
    }
    std::vector<CardPtr> contributed;
    // 已成为“合援角色”的角色在本次对其他角色发动镇围时，也可弃牌，计入朱然弃牌数。
    for (const auto& helper : engine.getAlivePlayers()) {
        if (!helper || helper->getId() == t->getId() ||
            helper->getMark("合援角色:" + std::to_string(self.getId())) == 0) continue;
        auto helperCards = helper->getHandAndEquipmentCards();
        if (!helperCards.empty() && engine.askConfirm(helper, "【合援】是否代朱然弃置牌？", true)) {
            auto helperOptions = discardCountOptions(helperCards.size());
            int helperCount = engine.askChooseOption(helper, helperOptions, "【合援】选择弃置数量", 0);
            for (int i = 0; i < helperCount && !helperCards.empty(); ++i) {
                CardPtr c = engine.askChooseCard(helper, helperCards, "【合援】选择弃置牌", false,
                                                 AIController::chooseLeastValuableCard(helperCards));
                if (!c) break;
                engine.discardCardOf(helper, c, "合援");
                contributed.push_back(c);
                helperCards.erase(std::remove(helperCards.begin(), helperCards.end(), c), helperCards.end());
            }
        }
    }
    auto theirDiscardable = t->getHandAndEquipmentCards();
    auto theirCountOptions = discardCountOptions(theirDiscardable.size());
    int theirs = engine.askChooseOption(t, theirCountOptions, "【镇围】对方选择弃置的牌数",
                                        static_cast<int>(theirCountOptions.size()) - 1);
    std::vector<CardPtr> theirCards;
    for (int i = 0; i < theirs && !theirDiscardable.empty(); i++) {
        CardPtr c = engine.askChooseCard(t, theirDiscardable, "【镇围】选择第 " + std::to_string(i + 1) + " 张弃置牌",
                                         false, AIController::chooseLeastValuableCard(theirDiscardable));
        if (!c) break;
        theirCards.push_back(c);
        theirDiscardable.erase(std::remove(theirDiscardable.begin(), theirDiscardable.end(), c), theirDiscardable.end());
    }
    for (auto& c : mineCards) engine.discardCardOf(me, c, "镇围");
    for (auto& c : theirCards) engine.discardCardOf(t, c, "镇围", me);
    setMark(self, "镇围已用", 1);
    setMark(self, "镇围弃牌数", (int)mineCards.size());
    // 条件数：1.牌数 2.花色数
    auto suitsOf = [](const std::vector<CardPtr>& v) {
        std::set<int> s;
        for (auto& c : v) s.insert((int)c->getSuit());
        return s.size();
    };
    std::vector<CardPtr> allMine = mineCards;
    allMine.insert(allMine.end(), contributed.begin(), contributed.end());
    int X = 0;
    if ((int)allMine.size() >= (int)theirCards.size()) X++;
    if (suitsOf(allMine) >= suitsOf(theirCards)) X++;
    int last = 0;
    for (int i = 1; i <= X; i++) {
        int opt = engine.askChooseOption(me, {"对 " + who(*t) + " 造成1点伤害", "摸三张牌"},
                                         "【镇围】执行第" + std::to_string(i) + "项（至多" + std::to_string(X) + "项）", 1);
        if (opt == 0) { engine.applyDamage(me, t, 1, ShaElement::NORMAL); last = 1; }
        else { engine.drawCards(me, 3, "镇围"); last = 2; }
    }
    setMark(self, "镇围最后一项", last);
}

// 【镇围】与一名角色同时选择是否弃牌，你弃的牌数/花色数不小于对方时可执行至多 X 项
// （对其造成 1 点伤害 / 摸三张牌）→ 要有敌人，且手里有足够牌去“压过”对方。
bool MouZhenWeiSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return false;
    return AIController::hasEnemy(engine, self) && self.getHandCardCount() >= 3;
}

MouHeYuanSkill::MouHeYuanSkill()
    : TriggerSkill("谋-合援", mouText("谋·朱然", "谋-合援")) {}

void MouHeYuanSkill::onPhaseEnd(GameEngine& engine, Player& self, TurnPhase phase) {
    if (phase != TurnPhase::FINISH) return;
    if (engine.getCurrentPlayer() && engine.getCurrentPlayer()->getId() != self.getId()) return;
    int X = self.getMark("镇围弃牌数");
    if (X <= 0) return;
    std::vector<PlayerPtr> wounded;
    // 官网只写“已受伤角色”，未限定其他角色，技能持有者也可成为目标。
    for (auto& p : engine.getAlivePlayers()) if (p->isWounded()) wounded.push_back(p);
    if (wounded.empty()) return;
    PlayerPtr me = selfPtr(engine, self);
    wounded.erase(std::remove_if(wounded.begin(), wounded.end(), [&](const PlayerPtr& p) {
        return p->getMark("合援已用:" + std::to_string(p->getId())) > 0;
    }), wounded.end());
    if (wounded.empty()) return;
    PlayerPtr t = engine.askChoosePlayer(me, wounded, "【合援】选择一名已受伤角色", true);
    if (!t) return;
    setMark(self, "合援已用:" + std::to_string(t->getId()), 1);
    setMark(self, "合援已用", 1); // 兼容旧状态显示；实际限次按目标角色分别记录
    setMark(*t, "合援角色:" + std::to_string(self.getId()), 1);
    for (int i = 0; i < X; i++) {
        auto discardable = self.getHandAndEquipmentCards();
        if (discardable.empty()) break;
        CardPtr c = AIController::chooseLeastValuableCard(discardable);
        if (!c) break;
        engine.discardCardOf(me, c, "合援");
    }
    int last = self.getMark("镇围最后一项");
    if (last == 1) engine.applyDamage(me, t, 1, ShaElement::NORMAL);
    else if (last == 2) engine.drawCards(t, 3, "合援");
    engine.logMessage("  【合援】" + who(*t) + " 执行上次“镇围”的最后一项。");
}


// ---------------- 谋·马超 · 马术 ----------------

MouMaShuSkill::MouMaShuSkill()
    : StateSkill("谋-马术", mouText("谋·马超", "谋-马术"), SkillTag::LOCK) {}

void MouMaShuSkill::onCalculateDistance(GameEngine&, const Player&, const Player&, int& distance) {
    // self 为计算发起者（标准马术同实现：来源侧距离-1——引擎调用语义见 GameEngine::calculateDistance）
    distance = std::max(1, distance - 1);
}


// ---------------- 谋·韩当 · 解烦 ----------------

MouJieFanSkill::MouJieFanSkill()
    : ActiveSkill("谋-解烦", mouText("谋·韩当", "谋-解烦")) {}

bool MouJieFanSkill::canActivate(GameEngine& engine, Player& self) {
    if (disabled) return false;
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !engine.isPlayerTurn(self)) return false;
    return !hasUsesLeft() ? false : getUsesThisTurn() == 0;
}

void MouJieFanSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine,self)) return;
    PlayerPtr me = selfPtr(engine, self);
    // 官网只写“一名角色”，没有“其他”限制。
    std::vector<PlayerPtr> cands = engine.getAlivePlayers();
    if (cands.empty()) return;
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【解烦】指定一名角色", false, cands.front());
    if (!t) return;
    markUsed();
    // 攻击范围内含有其的角色
    std::vector<PlayerPtr> inRange;
    for (auto& p : engine.getAlivePlayers()) {
        auto reachable = engine.getPlayersInRange(*p,p->getAttackRange());
        if (std::find(reachable.begin(),reachable.end(),t)!=reachable.end()) inRange.push_back(p);
    }
    int opt = engine.askChooseOption(me, {"令攻击范围内含有其的角色依次选择弃置武器或令其摸牌",
                                          "令其摸" + std::to_string(inRange.size()) + "张牌",
                                          "背水：失效直至你杀死一名角色"}, "【解烦】选择一项");
    if (opt == 0 || opt == 2) {
        for (auto& r : inRange) {
            auto weapons = std::vector<CardPtr>{};
            for (auto& c : r->getAllEquipment())
                if (c && c->getSubType() == CardSubType::WEAPON) weapons.push_back(c);
            int choice = weapons.empty() ? 1 : engine.askChooseOption(r, {"弃置一张武器牌", "令目标摸一张牌"}, "【解烦】选择一项");
            if (choice == 0 && !weapons.empty()) {
                CardPtr c = engine.askChooseCard(r, weapons, "【解烦】选择弃置的武器牌", false, weapons.front());
                if (c) engine.discardCardOf(r, c, "解烦");
            } else {
                engine.drawCards(t, 1, "解烦");
            }
        }
    } else if (opt == 1) {
        engine.drawCards(t, (int)inRange.size(), "解烦");
    }
    if (opt == 2) {
        setMark(self, "解烦背水", 1);
        disabled = true;
        engine.logMessage("  【解烦】背水发动，失效直至" + who(self) + "杀死一名角色。");
    } else {
        setMark(self, "解烦已用", 1);
    }
}

bool MouJieFanSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    if (disabled || !hasUsesLeft()) return false;
    for (auto& p : engine.getAlivePlayers())
        if (p->getId() != self.getId()) return true;
    return false;
}

void MouJieFanSkill::onPlayerDeath(GameEngine&, Player& self, Player&, Player* killer) {
    // 背水恢复条件：你杀死一名角色（此处为“你造成伤害致其死亡”时的击杀者判定）。
    if (disabled && killer && killer->getId() == self.getId()) {
        disabled = false;
        setMark(self, "解烦背水", 0);
    }
}

// ---------------- 势·小乔 · 合韵 ----------------

ShiHeYunSkill::ShiHeYunSkill()
    : ActiveSkill("势-合韵", mouText("势·小乔", "势-合韵"), 2) {}

namespace {
// 与 self 拥有相同技能（按技能名交集）的存活角色。
// 【合韵】没有“其他”限定：按官网字面，自己也可成为目标；因此只要自己仍有技能，就把自己列为合法目标。
// 其他角色仍排在前面，保留原有 AI 默认选择及常规目标顺序。
std::vector<PlayerPtr> sameSkillUsers(GameEngine& engine, const Player& self) {
    std::vector<PlayerPtr> res;
    if (!self.getHero() || !self.isAlive()) return res;
    std::set<std::string> mine;
    // 模式技能（择途/立储/飞扬/跋扈/共苦）由玩法赋予，不算“与你有相同技能”
    for (auto& sk : self.getHero()->getSkills())
        if (sk && !isModeSkillName(sk->getName())) mine.insert(sk->getName());
    if (mine.empty()) return res;
    for (auto& p : engine.getOtherAlivePlayers(self)) {
        if (!p->getHero()) continue;
        for (auto& sk : p->getHero()->getSkills()) {
            if (sk && mine.count(sk->getName())) { res.push_back(p); break; }
        }
    }
    // 角色与自己当然拥有相同技能；不能仅因场上没有另一名技能相同者而禁止发动。
    auto me = engine.getPlayerById(self.getId());
    if (me) res.push_back(me);
    return res;
}
} // namespace

bool ShiHeYunSkill::canActivate(GameEngine& engine, Player& self) {
    if (engine.getCurrentPhase() != TurnPhase::PLAY || !hasUsesLeft()) return false;
    return !sameSkillUsers(engine, self).empty();
}

void ShiHeYunSkill::onActivate(GameEngine& engine, Player& self) {
    if (!canActivate(engine, self)) return;
    PlayerPtr me = selfPtr(engine, self);
    auto cands = sameSkillUsers(engine, self);
    if (!me || cands.empty() || !hasUsesLeft()) return;
    PlayerPtr t = engine.askChoosePlayer(me, cands, "【合韵】选择一名与你有相同技能的角色（可选择自己）", false, cands.front());
    if (!t) return;
    markUsed();
    // 失去技能是费用尝试；若所选为持恒技则不会被移除，但“令其摸两张牌”仍正常结算。
    std::vector<std::string> names;
    // 只能失去武将自身的技能；玩法赋予的模式技能不在其中
    if (self.getHero())
        for (auto& sk : self.getHero()->getSkills())
            if (sk && !isModeSkillName(sk->getName())) names.push_back(sk->getName());
    if (!names.empty()) {
        int idx = engine.askChooseOption(me, names, "【合韵】选择你失去的一个技能", (int)names.size() - 1);
        if (idx >= 0 && idx < (int)names.size()) {
            const std::string chosen = names[idx];
            const SkillPtr chosenSkill = self.getHero()->findSkill(chosen);
            if (engine.removeHeroSkill(me, chosen)) {
                engine.logMessage("  【合韵】" + who(self) + " 失去技能 " + chosen + "。");
            } else if (chosenSkill && chosenSkill->isSustained()) {
                engine.logMessage("  【合韵】" + who(self) + " 选择的【" + chosen + "】为持恒技，未失去；摸牌效果仍结算。");
            }
        }
    }
    engine.drawCards(t, 2, "合韵");
    engine.logMessage("  【合韵】" + who(*t) + " 摸两张牌。");
}

bool ShiHeYunSkill::aiShouldActivate(GameEngine& engine, Player& self) {
    auto cands = sameSkillUsers(engine, self);
    if (cands.empty()) return false;
    // AI 仅在牌少时为自身换牌；避免仅因“自己可选自己”便自动拆掉核心技能。
    if (cands.size() == 1 && cands.front()->getId() == self.getId())
        return self.getHandCardCount() < 2;
    return true;
}

// ---------------- 势·小乔 · 音洄 ----------------

namespace {
// 【音洄】获得的技能必须是独立实例：按技能名用登记表工厂重建，
// 避免与原持有者共享同一技能对象导致内部计数/状态串用。
// 登记表中没有该技能（觉醒/使命动态获得等）时退回共享对象。
SkillPtr freshSkillInstance(const SkillPtr& source) {
    if (!source) return nullptr;
    for (const auto& info : HeroRegistry::all()) {
        if (!info.create) continue;
        HeroPtr h = info.create();
        if (!h) continue;
        if (SkillPtr s = h->findSkill(source->getName())) return s;
    }
    return source;
}
} // namespace

ShiYinHuiSkill::ShiYinHuiSkill()
    : TriggerSkill("势-音洄", mouText("势·小乔", "势-音洄")) {}

void ShiYinHuiSkill::onRoundStart(GameEngine& engine, Player& self) {
    PlayerPtr me = selfPtr(engine, self);
    if (!me || !self.getHero()) return;
    // 可获得的目标技能池
    struct Src { PlayerPtr p; SkillPtr sk; };
    std::vector<Src> pool;
    for (auto& other : engine.getOtherAlivePlayers(self)) {
        if (!other->getHero()) continue;
        for (auto& sk : other->getHero()->getSkills()) pool.push_back({other, sk});
    }
    if (pool.empty()) return;
    std::vector<std::string> names;
    for (auto& x : pool) names.push_back(who(*x.p) + ":" + x.sk->getName());
    if (!engine.askConfirm(me, "【音洄】发动：清除此前获得的技能并获得一名其他角色的一个技能？", true))
        return;
    // 清除因此获得的技能
    for (auto& n : granted) engine.removeHeroSkill(me, n);
    if (!granted.empty())
        engine.logMessage("  【音洄】" + who(self) + " 清除此前获得的技能。");
    granted.clear();
    int idx = engine.askChooseOption(me, names, "【音洄】选择获得一名其他角色的一个技能", 0);
    if (idx < 0 || idx >= (int)pool.size()) return;
    SkillPtr got = pool[idx].sk;
    // 与已有技能重名则不重复获得
    for (auto& sk : self.getHero()->getSkills())
        if (sk->getName() == got->getName()) {
            engine.logMessage("  【音洄】已有同名技能，不再获得。");
            return;
        }
    self.getHero()->addSkill(freshSkillInstance(got)); // 独立实例，状态不与原持有者串用
    granted.push_back(got->getName());
    engine.logMessage("  【音洄】" + who(self) + " 获得技能 " + got->getName() + "。");
}

} // namespace Thks
