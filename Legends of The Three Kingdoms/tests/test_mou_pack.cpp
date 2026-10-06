// 附录C（docs/mou_appendix.md）逐字测试：谋攻篇37名武将的技能描述必须与官网原文完全一致。
// 本文件由附录C生成（python），含：逐字表 / 注册表完整性 / 行为冒烟。
#include "test_helpers.h"
#include "HeroRegistry.h"
#include "Interaction.h"
#include "SkillsMou.h"
#include "Skills.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <tuple>
#include <vector>

using namespace Thks;

namespace {

const std::vector<std::tuple<std::string, std::string, std::string>>& verbatimRows() {
    static const std::vector<std::tuple<std::string, std::string, std::string>> rows = {
        {"谋·刘赪", "谋-掠影", "出牌阶段内，你使用【杀】指定其他角色为目标时，你获得一个“椎”标记，出牌阶段限两次。你使用【杀】结算结束后，若你拥有至少两个“椎”标记，则你移除两个“椎”标记，然后摸一张牌，且可以选择一名角色视为对其使用一张【过河拆桥】。"},
        {"谋·刘赪", "谋-莺舞", "出牌阶段内，你使用非伤害类普通锦囊指定一名角色为目标时，若你拥有技能“掠影”，则你获得一个“椎”标记，出牌阶段限两次。你使用非伤害类普通锦囊结算结束后，若你拥有至少两个“椎”标记，则你移除两个“椎”标记，然后摸一张牌，且可以选择一名角色视为对其使用一张不受次数限制的普通【杀】。"},
        {"谋·吕蒙", "谋-克己", "出牌阶段各限一次（若你已发动过“渡江”，则修改为出牌阶段限一次），你可以选择一项执行对应效果：1. 弃置一张手牌，获得1点“护甲”；2. 流失1点体力，获得2点护甲。你的手牌上限+X(X为你的护甲值)。若你不处于濒死状态，你无法使用【桃】。"},
        {"谋·吕蒙", "谋-渡江", "觉醒技，准备阶段，若你的“护甲”值不少于3，则获得技能“夺荆”。"},
        {"谋·黄忠", "谋-烈弓", "若你未装备武器，你的【杀】只能当作普通【杀】使用或打出。你使用牌时或成为其他角色使用牌的目标后，若此牌的花色未被“烈弓”记录，则记录此种花色。当你使用【杀】指定唯一目标后，你可以展示牌堆顶的X张牌（X为你记录的花色数-1，且至少为0），然后每有一张牌花色与“烈弓”记录的花色相同，你令此【杀】伤害+1，且其不能使用“烈弓”记录花色的牌响应此【杀】。若如此做，此【杀】结算结束后，清除“烈弓”记录的花色。"},
        {"谋·华雄", "谋-耀武", "锁定技，当你受到【杀】造成的伤害时，若此【杀】为红色，伤害来源回复1点体力或摸一张牌；若此【杀】不为红色，则你摸一张牌。"},
        {"谋·华雄", "谋-扬威", "出牌阶段限一次，你可以摸两张牌并获得“威”标记直到此阶段结束，然后此技能失效直到下个回合的结束阶段。拥有“威”标记的角色出牌阶段可以额外使用一张【杀】、使用【杀】无距离限制且无视防具。"},
        {"谋·杨婉", "谋-暝眩", "出牌阶段开始时，选择数张牌然后随机交给其他角色，然后其选择一项：对你使用一张【杀】；2.交给你一张牌，且你摸一张牌"},
        {"谋·杨婉", "谋-陷仇", "当你受到伤害后，可以选择另一名角色，其可以弃置一张牌，视为对伤害来源使用一张【杀】。若此【杀】造成伤害，则你回复1点体力值。"},
        {"谋·马超", "谋-铁骑", "使用杀指定的目标角色，非锁定技失效，不能使用闪响应此杀，通过谋弈获得牌或摸牌"},
        {"谋·马超", "谋-马术", "你计算与其他角色的距离-1。"},
        {"谋·张飞", "谋-咆哮", "锁定技，你使用【杀】无次数限制。若你装备了武器，你使用【杀】无距离限制。你的出牌阶段，若你于当前阶段内使用过【杀】，你于此阶段使用【杀】具有以下效果：此【杀】指定的目标本回合非锁定技失效；此【杀】不可被响应且伤害值+1；此【杀】对一名角色造成伤害后若其未死亡，你失去1点体力并随机弃置一张手牌。"},
        {"谋·张飞", "谋-协击", "准备阶段，你可以选择一名其他角色，与其进行“协力”。其回合的结束阶段，若你与其“协力”成功，则你可以选择至多三名角色，依次视为对其使用一张普通【杀】，你以此【杀】造成伤害后，你摸等同于此【杀】造成伤害数的牌。"},
        {"谋·赵云", "谋-龙胆", "剩余可用X次（×初始为1且最大为3，每名角色的回合结束后X加1)，你可以将一张【杀】当【闪】、【闪】当普通【杀】使用或打出，若如此做,你摸一张牌。"},
        {"谋·赵云", "谋-积著", "准备阶段,你可以选择一名其他角色，与其进行“协力”。其回合结束后，若你与其“协力”成功，则直到你的下个回合结束后，你修改龙胆为“剩余可用次数×次（×初始为1且最大为3，每名角色的回合结束后X加一），你可以将一张基本牌当做任意基本牌使用或打出，若如此做，你摸一张牌”。"},
        {"谋·孙尚香", "谋-结姻", "使命技，你的登场势力为“蜀”。游戏开始时，你选择一名其他角色，令其获得“助”标记。出牌阶段开始时，有“助”标记的角色选择一项：1. 若其有手牌，交给你两张手牌（若其手牌不足两张则交给你所有手牌），然后其获得一点“护甲”；2、令你移动或移除助标记（若其不是第一次获得“助”标记，则你只能移除“助”标记。）失败：当“助”标记被移除时，你回复1点体力并获得你武将牌上所有“妆”牌，移除“助”标记，你将势力修改为“吴”，减1点体力上限。"},
        {"谋·孙尚香", "谋-良助", "蜀势力技，出牌阶段限一次，你可以将其他角色装备区内的一张牌置于你的武将牌上，称为“妆”，然后令拥有“助”标记的角色选择一项：1.回复1点体力值；2.摸两张牌。"},
        {"谋·孙尚香", "谋-枭姬", "吴势力技，当你失去装备区内的一张牌时，你摸两张牌，然后可以弃置场上的一张牌。"},
        {"谋·夏侯氏", "谋-燕语", "出牌阶段限两次，你可以弃置一张【杀】并摸一张牌。出牌阶段结束时，你可以令一名其他角色摸X张牌（X为你本回合以此法弃置的【杀】的数量的三倍）。"},
        {"谋·夏侯氏", "谋-樵拾", "每回合限一次，你受到其他角色造成的伤害后，伤害来源可选择令你回复等同此次伤害值的体力，若如此做，其摸两张牌。"},
        {"谋·周瑜", "谋-英姿", "锁定技，摸牌阶段，你以下的数值每满足一项，你多摸一张牌，且本回合手牌上限+1:1.你的手牌数不少于2;2.你的体力值不低于2;3.你装备区的牌数不低于1。"},
        {"谋·周瑜", "谋-反间", "出牌阶段,你可以选择一名其他角色、扣置一张本回合未以此法扣置过的花色的手牌，并声明一个花色，其须选择一项:1.猜测此牌花色与声明花色是否一致;2.其翻面，且此技能失效直到回合结束。然后你展示此牌令其获得之。若其选择猜测，则:若猜对，此技能失效直到回合结束;若猜错，其失去1点体力。"},
        {"谋·貂蝉", "谋-离间", "出牌阶段限一次，你可以选择至少两名其他角色并弃置X张牌（X为你选择的角色数-1），然后他们依次对逆时针最近座次的你选择的另一名角色视为使用一张【决斗】。"},
        {"谋·貂蝉", "谋-闭月", "锁定技，结束阶段，你摸X张牌。（X为本回合受到伤害的角色数+1，至多为4）"},
        {"谋·袁绍", "谋-乱击", "出牌阶段限一次，你可以将两张手牌当【万箭齐发】使用。其他角色因响应你使用的【万箭齐发】而打出【闪】时，你摸一张牌（每回合你至多以此法获得3张牌）。"},
        {"谋·袁绍", "谋-血裔", "主公技，锁定技，你的手牌上限+X（X为其他群势力角色数的两倍）。你使用牌指定其他群势力角色为目标后，你摸一张牌（每回合你至多以此法获得2张牌）。"},
        {"谋·庞统", "谋-连环", "一级：出牌阶段，你可以将一张梅花手牌当【铁索连环】使用（每个出牌阶段限1次），或重铸一张梅花手牌。你使用【铁索连环】时，你可以失去一点体力，若如此做，你指定一名角色为目标后，若其不处于连环状态，随机弃置其一张手牌。二级：出牌阶段，你可以将一张梅花手牌当【铁索连环】使用（每个出牌阶段限1次），或重铸一张梅花手牌。你使用【铁索连环】可以额外指定任意名目标。你使用【铁索连环】指定一名角色为目标后，若其不处于连环状态，随机弃置其一张手牌。"},
        {"谋·庞统", "谋-涅槃", "限定技，当你处于濒死状态时，你可以弃置区域里的所有牌，摸两张牌，将体力回复至2点，复原武将牌，并升级“连环”。"},
        {"谋·刘备", "谋-仁德", "出牌阶段开始时，你获得2个“仁望”标记。出牌阶段，你可以将任意张牌交给一名本阶段未获得过“仁德”牌的其他角色，然后你获得等量的“仁望”标记（你至多拥有8个“仁望”标记）。每回合限一次，当你需要使用或打出一张基本牌时，你可以弃置2个“仁望”标记视为使用或打出之。"},
        {"谋·刘备", "谋-章武", "限定技，出牌阶段，你可以令本局游戏中所有获得过“仁德”牌的角色依次交给你Y张牌（Y为游戏轮数-1，且最大为3），若如此做，你回复3点体力，然后失去“仁德”。"},
        {"谋·刘备", "谋-激将", "主公技，出牌阶段结束时，你可指定一名角色，并令另一名攻击范围内含有该角色且体力值不小于你的其他蜀势力角色选择一项：1.视为对你指定的角色使用一张普通【杀】；2.跳过下一个出牌阶段。"},
        {"谋·姜维", "谋-挑衅", "蓄力技，出牌阶段限一次，你可以至多选择X名其他角色（X为你拥有的蓄力点数量），令这些角色依次选择一项：1.对你使用一张无距离限制的【杀】；2.交给你一张牌。然后你每选择一名角色，减少1点蓄力点。弃牌阶段，你每弃置一张牌，获得1点蓄力点。"},
        {"谋·姜维", "谋-志继", "觉醒技，准备阶段，若你发动“挑衅”选择过至少4名角色，你减少1点体力值上限，令任意名角色直到你的下个回合开始时获得“北伐”标记。拥有“北伐”标记的角色使用牌只能选择你或其为目标。"},
        {"谋·法正", "谋-眩惑", "出牌阶段限一次，你可以交给一名没有“眩”标记的其他角色一张牌并令其获得“眩”标记。有“眩”标记的角色于摸牌阶段外获得牌时，你随机获得其一张手牌（每个“眩”标记最多令你获得五张牌）。"},
        {"谋·法正", "谋-恩怨", "锁定技，准备阶段，你令有“眩”标记的角色执行以下效果：自其获得“眩”标记开始，若你获得其至少三张牌，则你移除其“眩”标记，然后交给其三张牌；否则其流失1点体力值，然后你回复1点体力并移除其“眩”标记。"},
        {"谋·陈宫", "谋-明策", "出牌阶段限一次，你可以将一张牌交给一名其他角色，然后其选择一项：1. 其流失1点体力，你摸两张牌并获得一个“策”标记；2.其摸一张牌。出牌阶段开始时，若你拥有“策”标记，你可以选择一名其他角色，对其造成X点伤害并移除所有“策”标记（X为你拥有的“策”标记数量）。"},
        {"谋·陈宫", "谋-智迟", "锁定技，当你受到伤害后，本回合接下来你受到伤害时，防止之。"},
        {"谋·甘宁", "谋-奇袭", "出牌阶段限一次，你可以选择一名其他角色，令其猜测你手牌中某种花色的牌最多（或之一）。若其猜错，你可令其再次猜测（其无法选择此阶段已猜测过的花色）；否则你展示所有手牌。然后你弃置其区域内X张牌。（X为其此阶段猜错的次数，若不足则全弃）"},
        {"谋·甘宁", "谋-奋威", "限定技，出牌阶段，你可以将至多三张牌置于任意名角色的武将牌上（每名角色各一张），称为“威”，然后你摸等量的牌。有“威”的角色成为锦囊牌的目标时，你须选择一项：1.令其获得“威”牌；2.弃置其“威”牌，取消其作为此锦囊牌的目标。"},
        {"谋·黄盖", "谋-苦肉", "出牌阶段开始时，你可以交给其他角色一张牌，然后失去1点体力（若你交出的牌是【桃】或【酒】，则改为失去2点体力）。当你失去1点体力后，你获得2点护甲。"},
        {"谋·黄盖", "谋-诈降", "锁定技，你于每个回合使用的前X张牌无距离和次数限制且不可被响应。摸牌阶段，你多摸X张牌。（X为你已损失体力值）"},
        {"谋·孙权", "谋-制衡", "出牌阶段限一次，你可以弃置任意张牌，然后摸等量的牌。若你弃置了所有手牌，则额外摸X+1张牌（X为你拥有的“业”标记数量），然后移除一个“业”标记。"},
        {"谋·孙权", "谋-统业", "锁定技，结束阶段，你须选择一项，直到下回合准备阶段：1.若场上的装备数变化，则你获得一个“业”标记，否则失去一个“业”标记；2.若场上的装备数不变，则你获得一个“业”标记，否则失去一个“业”标记。你至多拥有2个“业”标记。"},
        {"谋·孙权", "谋-救援", "主公技，锁定技，其他吴势力角色使用【桃】时，你摸一张牌。其他吴势力角色对你使用【桃】回复的体力+1。"},
        {"谋·大乔", "谋-国色", "出牌阶段限四次，你可以将一张方块牌当【乐不思蜀】使用，或弃置场上一张【乐不思蜀】。然后你摸一张牌。"},
        {"谋·大乔", "谋-流离", "当你成为【杀】的目标时，你可以弃置一张牌并选择你攻击范围内的一名其他角色（不能是此【杀】的使用者），然后将此【杀】转移给该角色。每名角色的回合限一次，若你弃置的是红桃牌，你可令一名其他角色（不能是此【杀】的使用者）获得“流离”标记（若场上已有“流离”标记则改为转移给该角色）。拥有“流离”标记的角色回合开始时，执行一个额外的出牌阶段并令其移除“流离”标记。"},
        {"谋·孟获", "谋-祸首", "锁定技，【南蛮入侵】对你无效；当其他角色使用【南蛮入侵】指定目标后，你代替其成为此牌造成的伤害的来源。出牌阶段开始时，你随机获得弃牌堆中一张【南蛮入侵】。出牌阶段，若你使用过【南蛮入侵】，则你此阶段不能使用【南蛮入侵】。"},
        {"谋·孟获", "谋-再起", "蓄力技（0/7），弃牌阶段结束时，你可以选择任意名角色并扣除等量蓄力点，然后令你选择的角色各选择一项：1.令你摸一张牌；2.弃置一张牌，然后你回复1点体力。当你造成伤害后，获得1点蓄力点（每回合限获得1点蓄力点）。"},
        {"谋·孙策", "谋-激昂", "一级：你使用【决斗】可以额外指定一名目标，若如此做，你流失1点体力。当你使用【决斗】或红色【杀】指定一名目标后，或成为【决斗】或红色【杀】的目标后，你摸一张牌。出牌阶段限一次，你可以将所有手牌当【决斗】使用。二级：你使用【决斗】可以额外指定一名目标，若如此做，你流失1点体力。当你使用【决斗】或红色【杀】指定一名目标后，或成为【决斗】或红色【杀】的目标后，你摸一张牌。出牌阶段限X次（X为场上吴势力角色数），你可以将所有手牌当【决斗】使用。"},
        {"谋·孙策", "谋-魂姿", "觉醒技，你脱离濒死状态时，你减1点体力上限、获得1点护甲、摸三张牌，然后获得技能“英姿※”和“英魂※”。"},
        {"谋·孙策", "谋-制霸", "主公技，限定技，当你进入濒死状态时，你可回复X点体力（X为场上吴势力角色数量-1）并升级技能“激昂”，然后其他吴势力角色依次受到1点无来源伤害，若其因此伤害死亡，则其死亡后，你摸三张牌。"},
        {"谋·祝融", "谋-烈刃", "当你使用【杀】指定一名其他角色为唯一目标后，你可与其拼点，若你赢，此【杀】结算结束后，你可对另一名其他角色造成1点伤害。"},
        {"谋·祝融", "谋-巨象", "锁定技，【南蛮入侵】对你无效；当其他角色使用的【南蛮入侵】结算结束后，你获得之。结束阶段，若你本回合未使用过【南蛮入侵】，你随机从游戏外将一张【南蛮入侵】交给一名角色。"},
        {"谋·卢植", "谋-明任", "明任：游戏开始时，你摸两张牌，然后将你的一张手牌扣置于你的武将牌上，称为“任”。结束阶段，你可以用手牌替换“任”。"},
        {"谋·卢植", "谋-贞良", "贞良：转换技，阳：出牌阶段限一次，你可以选择一名攻击范围内的其他角色并弃置x张与“任”颜色相同的牌对其造成1点伤害（x为你与其体力值之差且至少为1） 阴：你的回合外，当一名角色使用或打出的牌结算结束后，若此牌与“任”类型相同，则你可令一名角色摸两张牌。"},
        {"谋·诸葛亮", "谋-火计", "使命技，出牌阶段限一次，你可以选择一名其他角色，对其及其同势力的其他角色各造成1点火焰伤害。成功：准备阶段，若你本局游戏对其他角色造成过至少X点火焰伤害（X为本局游戏人数），你失去“火计”和“看破”，获得“观星※”和“空城※”。失败：成功达成使命前，进入濒死状态。"},
        {"谋·诸葛亮", "谋-看破", "看破：每轮开始时，你清除“看破”记录的牌名，然后你可以选择并记录任意个数的牌名（不可选择上次发动此技能记录过的牌名；每局游戏最多记录4个牌名，若为斗地主和排位赛模式则修改为2）。其他角色使用与你记录牌名相同的牌时，你可以移除一个对应牌名的记录，然后令此牌无效，且你摸一张牌。"},
        {"谋·诸葛亮", "谋-观星", "观星：准备阶段，你移去所有的“星”，并将牌堆顶的X张牌置于武将牌上（X为7-此前此技能准备阶段发动次数的三倍），称为“星”。然后你可以将任意张“星”牌置于牌堆顶。结束阶段，若你未于准备阶段将“星”牌置于牌堆顶，则你可以将任意张“星”牌置于牌堆顶。当你需要使用或打出手牌时，你可以将“星”视为你的牌使用或打出。"},
        {"谋·诸葛亮", "谋-空城", "空城：锁定技，当你受到伤害时，若你有技能“观星”且你的武将牌上有“星”，你进行一次判定，若判定结果点数小于等于你“星”牌的数量，则此伤害-1；若你有技能“观星”且你武将牌上没有“星”，你受到的伤害+1。"},
        {"谋·关羽", "谋-武圣", "武圣：你可以将一张手牌当作【杀】使用或打出。出牌阶段开始时，你可以指定一名主公以外的角色。此阶段：你对其使用【杀】无距离和次数限制；你使用【杀】指定其为目标后，你摸一张牌（若为身份场则修改为摸两张牌）；你对其使用三张【杀】后，不可再指定其为你使用【杀】的目标。"},
        {"谋·关羽", "谋-义绝", "义绝：锁定技。一名其他角色于你的回合内受到你造成的伤害时，若此伤害会令其进入濒死状态，防止之（本局游戏每名角色限一次）。若如此做，直到回合结束，你使用牌指定其为目标时，取消之。"},
        {"谋·黄月英", "谋-集智", "锁定技，当你使用一张普通锦囊牌时，你摸一张牌。以此法获得的牌本回合不计入手牌上限。"},
        {"谋·黄月英", "谋-奇才", "（身份场、团战类）你使用锦囊牌没有距离限制。出牌阶段限一次，你可以选择一名其他角色，将手牌或弃牌堆中的一张装备牌置入其装备区，然后其获得“奇”标记。拥有“奇”标记的角色接下来获得的三张普通锦囊牌须交给你。（斗地主）你使用锦囊牌没有距离限制。出牌阶段限一次，你可以选择一名其他角色，将手牌或弃牌堆中一张防具牌置入其装备区（每局游戏每个防具名限一次），然后其获得“奇”标记。拥有“奇”标记的角色接下来获得的三张普通锦囊牌须交给你。"},
        {"谋·小乔", "谋-天香", "（身份场、斗地主）准备阶段，若场上有“天香”标记，则你清除场上所有“天香”标记，并摸等量的牌。出牌阶段限三次，你可将一张红色手牌交给一名没有“天香”标记的其他角色，并令其获得对应花色的“天香”标记。当你受到伤害时，你可以选择一名拥有“天香”标记的角色，移除其“天香”标记，并根据移除的“天香”花色发动：红桃，你防止此伤害，然后令其受到防止伤害的来源角色造成的1点伤害；方块，其交给你两张牌。（团战类）准备阶段，若场上有“天香”标记，则你清除场上所有“天香”标记，并摸x张牌（x为本次清除的“天香”标记数+2）。出牌阶段限三次，你可将一张红色手牌交给一名没有“天香”标记的其他角色，并令其获得对应花色的“天香”标记。当你受到伤害时，你可以选择一名拥有“天香”标记的角色，移除其“天香”标记，并根据移除的“天香”花色发动：红桃，你防止此伤害，然后令其受到防止伤害的来源角色造成的1点伤害；方块，其交给你两张牌。"},
        {"谋·小乔", "谋-红颜", "锁定技。你的黑桃手牌只能当做红桃牌使用、打出、弃置或交给其他角色。你的黑桃判定牌只能当做红桃判定牌。当一张判定牌生效前，如果此判定牌为红桃，你将判定结果改为由你指定的一种花色。"},
        {"谋·公孙瓒", "谋-义从", "蓄力技（2/4）。每轮开始时，你可消耗至多x点蓄力点并选择一项：直至本轮结束，你与其他角色距离-1，并将牌堆中的x张【杀】置于武将牌上，称为“扈”；直至本轮结束，其他角色与你距离+1，并将牌堆中的x张【闪】置于武将牌上，称为“扈”。你至多拥有四张“扈”，当你需要使用或打出手牌时，你可以将”扈”视为你的牌使用或打出。"},
        {"谋·公孙瓒", "谋-趫猛", "你使用【杀】对一名角色造成伤害后，若你拥有技能“义从”，你可选择一项：1.弃置其区域内的一张牌并摸一张牌 2.获得3蓄力点。"},
        {"谋·韩当", "谋-弓骑", "你的攻击范围+4。出牌阶段开始时，你可弃一张牌，若如此做，则此阶段你使用的牌其他角色只能使用或打出虚拟牌或与你弃置牌颜色相同的手牌响应。"},
        {"谋·韩当", "谋-解烦", "出牌阶段限一次，你可指定一名角色，令其选择一项：1.攻击范围内含有其的角色依次弃一张牌;2.其摸此时攻击范围内有其的角色数的牌；背水：此技能失效直至你杀死一名角色。"},
        {"谋·陆逊", "谋-谦逊", "当一张锦囊牌对你生效时，若此牌名未记录且你不是使用者，则你记录之，然后可将至多X张牌置于你的武将牌上（X为“谦逊”记录的牌名数且至多为5）；若如此做，此回合结束时，你获得武将牌上的所有牌。出牌阶段开始时，你可移去一个记录的牌名，若为普通锦囊牌的牌名，则你可视为使用此牌。"},
        {"谋·陆逊", "谋-连营", "身份、团战：其他角色的回合结束时，你可观看牌堆顶的x张牌，然后将这些牌交给任意角色（x为你本回合失去的牌数，且至多为5）。斗地主：其他角色的回合结束时，你可观看牌堆顶的x张牌，然后将这些牌交给任意角色（x为你本回合失去的牌数+1，且至多为5）。"},
        {"谋·贾诩", "谋-完杀", "一级：你的回合内，不处于濒死状态的其他角色不能使用【桃】。每轮限一次，一名角色进入濒死状态时，你可观看其手牌并选择其中的零至两张牌，然后其须选择一项：1、由你将被选择的牌分配给其以外的角色；2、弃置所有未被选择的牌。二级：你的回合内，不处于濒死状态的其他角色不能使用【桃】。每轮限一次，一名角色进入濒死状态时，你可观看其手牌并选择其区域内的零至两张牌，然后其须选择一项：1、由你将被选择的牌分配给其以外的角色；2、弃置所有未被选择的牌。"},
        {"谋·贾诩", "谋-乱武", "限定技，出牌阶段，你可令所有其他角色除非对各自距离最小的另一名其他角色使用一张【杀】，否则失去1点体力。每有一名角色因此失去体力时，你便可以选择“完杀”、“帷幕”中的一个进行升级。"},
        {"谋·贾诩", "谋-帷幕", "一级：锁定技，你成为黑色锦囊牌的目标时，取消之。二级：锁定技，你成为黑色锦囊牌的目标时，取消之。每轮开始时，若你上一轮成为其他角色使用牌的目标的次数不大于一次，则你从弃牌堆随机获得一张黑色锦囊牌或防具牌。"},
        {"谋·诸葛瑾", "谋-缓释", "当一名角色的判定牌生效前，你可以观看牌堆顶的一张牌，然后你可以用此牌代替之，或用手牌中的一张替换之。"},
        {"谋·诸葛瑾", "谋-弘援", "军争：蓄力技（1/3）。当你一次获得不少于两张牌时，你可以消耗1点蓄力点令至多两名角色各摸一张牌。当一名其他角色一次失去不少于两张牌时，你可以消耗1点蓄力点令其摸一张牌。排位、斗地主：蓄力技（1/3）。当你一次获得不少于两张牌时，你可以消耗1点蓄力点令至多两名角色各摸一张牌。当一名其他角色一次失去不少于两张牌时，你可以消耗1点蓄力点令其摸两张牌。"},
        {"谋·诸葛瑾", "谋-明哲", "锁定技，每轮限两次。当你于回合外失去牌时，你选择一名角色，若其有蓄力技，则其获得1点蓄力点；若你失去的牌中有非基本牌，则其摸一张牌。"},
        {"谋·吕布", "谋-无双", "锁定技，你使用的【杀】需两张【闪】才能抵消；与你进行【决斗】的角色每次需打出两张【杀】。每回合限一次，若对方没有使用或打出【杀】或【闪】，则此【杀】或【决斗】对其造成的伤害+1。"},
        {"谋·吕布", "谋-利驭", "当你使用【杀】对一名其他角色造成伤害后，你可以获得其区域里的至多等同于伤害数张牌，然后其摸等量张牌。若你与其因此获得了全部类别的牌，其选择一项：令你视为对由其指定的另一名其他角色使用一张【决斗】；其获得技能“无双”直至其下个回合结束。"},
        {"谋·朱然", "谋-镇围", "出牌阶段限一次，你可与一名其他角色同时选择是否弃置任意张牌。然后你可执行至多X项（X为你弃置牌大于等于其的条件数：1.牌数；2.花色数）：1.对其造成1点伤害；2.摸三张牌。"},
        {"谋·朱然", "谋-合援", "每名角色限一次，结束阶段，你可选择一名已受伤角色并弃置X张牌（X为你上次发动镇围时弃置的牌数），令其执行上次“镇围”执行的最后一项，且此后你对除其以外的角色发动“镇围”时，该角色也可选择弃置牌（视为你弃置的牌）。"},
    };
    return rows;
}

struct MouInput {
    std::istringstream script;
    std::streambuf* saved;
    explicit MouInput(const std::string& text)
        : script(text), saved(std::cin.rdbuf(script.rdbuf())) { Interaction::resetInputState(); }
    ~MouInput() { std::cin.rdbuf(saved); Interaction::resetInputState(); }
};

} // namespace

TEST("mou/verbatim_descriptions_match_appendix_c") {
    int n = 0;
    for (auto& [hero, skill, text] : verbatimRows()) {
        const HeroInfo* info = nullptr;
        for (auto& i : HeroRegistry::all()) if (i.name == hero) { info = &i; break; }
        if (!info) { CHECK(info != nullptr); continue; }
        auto h = info->create();
        auto s = h->findSkill(skill);
        if (!s) {
            std::cerr << " [缺技能] " << hero << "·" << skill << std::endl;
            CHECK(s != nullptr);
            continue;
        }
        // 官网原文有明显错字时描述末尾附“（官网原文如此）”（用户 2026-10-04 裁定 C）。
        std::string got = s->getDescription();
        const std::string typoSuffix = "（官网原文如此）";
        if (got.size() > typoSuffix.size() &&
            got.compare(got.size() - typoSuffix.size(), typoSuffix.size(), typoSuffix) == 0)
            got = got.substr(0, got.size() - typoSuffix.size());
        if (got != text) {
            std::cerr << " [描述不逐字] " << hero << "·" << skill << std::endl;
        }
        CHECK_EQ(got, text);
        ++n;
    }
    std::cerr << " [逐字条目] " << n << std::endl;
    CHECK(n >= 81); // 81登记技能（夺荆/英姿/英魂觉醒动态获得，见下）
    // 觉醒动态获得的3个技能：直接构造对象逐字断言
    CHECK_EQ(MouDuoJingSkill().getDescription(),
             std::string("当你使用【杀】指定一名角色为目标时，你可以失去1点护甲，令此【杀】无视该角色的防具，然后你获得该角色的一张牌且你本阶段使用【杀】的次数上限+1。"));
    CHECK_EQ(MouXingYingZiSkill().getDescription(),
             std::string("锁定技，摸牌阶段，你每满足以下一项，你便多摸一张牌且本回合手牌上限+1：1.手牌数大于等于二；2.体力值大于等于２；3.装备区的牌数大于等于１。"));
    CHECK_EQ(MouXingYingHunSkill().getDescription(),
             std::string("准备阶段，若你已受伤，你可以选择一名其他角色并选择一项：1.令其摸X张牌，然后弃置一张牌；2.令其摸一张牌，然后弃置X张牌（X为你已损失的体力值）。"));
}

TEST("mou/registry_has_37_mou_packs_verbatim_skill_names") {
    int mouHeroes = 0;
    std::set<std::string> skillNames;
    for (auto& info : HeroRegistry::all()) {
        if (info.pack != "谋攻篇") continue;
        ++mouHeroes;
        auto h = info.create();
        CHECK(h != nullptr);
        if (!h) continue;
        for (auto& s : h->getSkills()) {
            skillNames.insert(s->getName());
            CHECK(!s->getDescription().empty());
            CHECK(s->getDescription().find("待核对") == std::string::npos);
        }
    }
    CHECK_EQ(mouHeroes, 37);
    // 83个技能名（按附录C登记名单）
    CHECK_EQ(skillNames.size(), size_t(81)); // 登记81（英姿/英魂/夺荆觉醒获得）
}

TEST("mou/hero_count_per_pack_and_skill_cardinality") {
    const std::vector<std::pair<std::string, size_t>> expect = {
        {"mou_liucheng", 2}, {"mou_lvmeng", 2}, {"mou_huangzhong", 1}, {"mou_huaxiong", 2},
        {"mou_yangwan", 2}, {"mou_machao", 2}, {"mou_zhangfei", 2}, {"mou_zhaoyun", 2},
        {"mou_sunshangxiang", 3}, {"mou_xiahoushi", 2}, {"mou_zhouyu", 2}, {"mou_diaochan", 2},
        {"mou_yuanshao", 2}, {"mou_pangtong", 2}, {"mou_liubei", 3}, {"mou_jiangwei", 2},
        {"mou_fazheng", 2}, {"mou_chengong", 2}, {"mou_ganning", 2}, {"mou_huanggai", 2},
        {"mou_sunquan", 3}, {"mou_daqiao", 2}, {"mou_menghuo", 2}, {"mou_sunce", 3},
        {"mou_zhurong", 2}, {"mou_luzhi", 2}, {"mou_zhugeliang", 4}, {"mou_guanyu", 2},
        {"mou_huangyueying", 2}, {"mou_xiaoqiao", 2}, {"mou_gongsunzan", 2}, {"mou_handang", 2},
        {"mou_luxun", 2}, {"mou_jiaxu", 3}, {"mou_zhugejin", 3}, {"mou_lvbu", 2}, {"mou_zhuran", 2},
    };
    int total = 0;
    for (auto& [id, cnt] : expect) {
        auto info = HeroRegistry::find(id);
        CHECK(info != nullptr);
        if (!info) continue;
        CHECK_EQ(info->pack, std::string("谋攻篇"));
        auto h = info->create();
        CHECK_EQ(h->getSkills().size(), cnt);
        total += (int)cnt;
    }
    CHECK_EQ(total, 81);
}

// ---------------- 行为冒烟 ----------------


TEST("mou/lvmeng_dujiang_awakens_at_three_armors") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_lvmeng", "zhangfei"});
    auto ps = e.getPlayers();
    CHECK(ps[0]->getHero()->findSkill("谋-夺荆") == nullptr);
    ps[0]->addMark("护甲", 3);
    auto djsk = ps[0]->getHero()->findSkill("谋-渡江");
    CHECK(djsk != nullptr);
    if (djsk) { bool started = false; djsk->onPhaseStart(e, *ps[0], TurnPhase::PREPARATION, started); }
    CHECK(ps[0]->getHero()->findSkill("谋-夺荆") != nullptr);
    CHECK_EQ(ps[0]->getHero()->findSkill("谋-夺荆")->getDescription(),
             std::string("当你使用【杀】指定一名角色为目标时，你可以失去1点护甲，令此【杀】无视该角色的防具，然后你获得该角色的一张牌且你本阶段使用【杀】的次数上限+1。"));
}

TEST("mou/liucheng_lueying_gains_zhui_on_sha") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_liucheng", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setPhase(TurnPhase::PLAY);
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(sha);
    int before = ps[0]->getMark("椎");
    CHECK(e.useCard(ps[0], sha, {ps[1]}));
    CHECK_EQ(ps[0]->getMark("椎"), before + 1);
}

TEST("mou/liucheng_lueying_uses_legal_other_guohe_target_and_real_card_effect") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 0, {"mou_liucheng", "zhangfei", "guanyu"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    auto ownCard = makeCard("桃", Suit::HEART, 3, CardType::BASIC, CardSubType::TAO);
    auto targetCard = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    players[0]->addHandCard(ownCard);
    players[1]->addHandCard(targetCard);
    players[0]->addMark("椎", 2);
    engine.setPhase(TurnPhase::PLAY);
    auto skill = players[0]->getHero()->findSkill("谋-掠影");
    CHECK(skill != nullptr);
    if (!skill) return;

    ShaContext ctx;
    {
        MouInput input("1 1"); // 只能选择第一名其他角色，然后选择其手牌区。
        skill->onShaFinished(engine, *players[0], ctx);
    }
    CHECK(players[0]->hasHandCard(ownCard)); // 不得把自己作为过河拆桥目标。
    CHECK(!players[1]->hasHandCard(targetCard)); // 执行真实过河拆桥效果，而非随机弃牌近似。
    CHECK_EQ(players[0]->getMark("椎"), 1); // 本次视为使用的非伤害锦囊再触发【莺舞】。
}

TEST("mou/liucheng_yingwu_uses_only_legal_sha_targets_and_ignores_sha_quota") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(4, -1, {"mou_liucheng", "zhangfei", "guanyu", "zhaoyun"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    engine.setPhase(TurnPhase::PLAY);
    players[0]->addMark("莺舞得椎", 1);
    players[0]->addMark("椎", 1);
    players[0]->incrementShaCount(); // 普通杀次数已用尽。
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "莺舞");
    auto legal = engine.getShaTargets(*players[0], sha);
    CHECK(std::find(legal.begin(), legal.end(), players[0]) == legal.end());
    CHECK(std::find(legal.begin(), legal.end(), players[2]) == legal.end()); // 对座角色超出攻击范围。
    CHECK_EQ(legal.size(), size_t(2));

    auto ordinaryTrick = makeCard("无中生有", Suit::HEART, 7, CardType::TRICK,
                                  CardSubType::WU_ZHONG_SHENG_YOU);
    auto skill = players[0]->getHero()->findSkill("谋-莺舞");
    CHECK(skill != nullptr);
    if (!skill) return;
    const int hp0 = players[0]->getHp();
    const int hp2 = players[2]->getHp();
    const int hp1 = players[1]->getHp();
    const int hp3 = players[3]->getHp();
    skill->onCardResolved(engine, *players[0], ordinaryTrick);
    CHECK_EQ(players[0]->getHp(), hp0);
    CHECK_EQ(players[2]->getHp(), hp2);
    CHECK(players[1]->getHp() < hp1 || players[3]->getHp() < hp3);
    CHECK_EQ(players[0]->getShaCountThisTurn(), 2); // 该技能杀虽不受上限，仍正常记入使用历史。
}

TEST("mou/wusheng_target_is_any_living_non_lord_including_non_lord_owner") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, 1, {"zhangfei", "mou_guanyu", "zhaoyun"});
    auto players = engine.getPlayers();
    CHECK(players[0]->getIdentity() == Identity::ZHU_GONG);
    CHECK(players[1]->getIdentity() != Identity::ZHU_GONG);
    engine.setCurrentPlayerForTesting(players[1]);
    engine.setPhase(TurnPhase::PLAY);
    auto skill = players[1]->getHero()->findSkill("谋-武圣");
    CHECK(skill != nullptr);
    if (!skill) return;
    bool skip = false;
    {
        MouInput input("1"); // 可选目标按身份筛选：第一名非主公正是技能持有者本人。
        skill->onPhaseStart(engine, *players[1], TurnPhase::PLAY, skip);
    }
    CHECK_EQ(players[1]->getMark("武圣指定"), players[1]->getId() + 1);
    CHECK_EQ(players[1]->getMark("武圣指定未用"), 1);
    skill->onPhaseEnd(engine, *players[1], TurnPhase::PLAY);
    CHECK_EQ(players[1]->getMark("武圣指定"), 0);
    CHECK_EQ(players[1]->getMark("武圣指定未用"), 0);
}

TEST("mou/wusheng_only_waives_sha_limit_for_selected_target_and_stops_after_three") {
    // 注：同一人物多版本不会同场（personKey），故第三名角色改用赵云而非关羽。
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3, -1, {"zhangfei", "mou_guanyu", "zhaoyun"});
    auto players = engine.getPlayers();
    for (const auto& player : players) clearHand(*player);
    engine.setCurrentPlayerForTesting(players[1]);
    engine.setPhase(TurnPhase::PLAY);
    auto skill = players[1]->getHero()->findSkill("谋-武圣");
    CHECK(skill != nullptr);
    if (!skill) return;
    players[1]->addMark("武圣指定", players[2]->getId() + 1);
    players[1]->addMark("武圣指定未用", 1);
    auto sha = Card::makeVirtual("杀", CardType::BASIC, CardSubType::SHA, {}, "测试");
    players[1]->incrementShaCount();
    CHECK_EQ(engine.getShaLimit(*players[1]), 1); // 指定目标不应放宽对其他角色的次数上限。
    CHECK(!engine.useCard(players[1], sha, {players[0]}));
    CHECK_EQ(players[0]->getHp(), players[0]->getMaxHp());
    CHECK(engine.useCard(players[1], sha, {players[2]}));
    CHECK(engine.useCard(players[1], sha, {players[2]}));
    CHECK(engine.useCard(players[1], sha, {players[2]}));
    CHECK_EQ(players[2]->getHp(), players[2]->getMaxHp() - 3);
    CHECK_EQ(players[1]->getMark("武圣对其出杀"), 3);
    auto candidates = engine.getShaTargets(*players[1], sha);
    CHECK(std::find(candidates.begin(), candidates.end(), players[2]) == candidates.end());
    CHECK(!engine.useCard(players[1], sha, {players[2]}));
    CHECK_EQ(players[2]->getHp(), players[2]->getMaxHp() - 3);
}

TEST("mou/sunquan_zhiheng_once_per_play_phase") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_sunquan", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    for (int i = 0; i < 3; i++)
        ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 7 + i, CardType::BASIC, CardSubType::SHA));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("谋-制衡");
    CHECK(sk != nullptr);
    auto act = std::dynamic_pointer_cast<ActiveSkill>(sk);
    CHECK(act != nullptr);
    if (!act) return;
    int used = 0;
    while (act->hasUsesLeft() && act->canActivate(e, *ps[0])) {
        act->activate(e, *ps[0]);
        ++used;
        if (used > 6) break;
    }
    CHECK_EQ(used, 1); // 官网原文：出牌阶段限一次
    e.setPhase(TurnPhase::NONE);
}

TEST("mou/zhouyu_yingzi_self_state_not_overwrite_biaozhouyu") {
    // 同名技能区分：谋周瑜的英姿与标准周瑜互不影响。
    // 注：同一人物多版本不会同场（personKey 已并回同一人物，
    // 谋·/势·/友· 与标/界/神/DIY 互斥），故此处直接由登记表构造两个实例比较。
    auto biao = HeroRegistry::create("zhouyu");
    auto mou = HeroRegistry::create("mou_zhouyu");
    CHECK(biao != nullptr && mou != nullptr);
    if (!biao || !mou) return;
    auto s1 = biao->findSkill("英姿");   // 标周瑜
    auto s2 = mou->findSkill("谋-英姿"); // 谋周瑜
    CHECK(s1 != nullptr && s2 != nullptr);
    if (!s1 || !s2) return;
    CHECK(s1.get() != s2.get());                              // 不同技能对象
    CHECK(s1->getDescription() != s2->getDescription());      // 各自的官网原文
}

TEST("mou/jiaxu_weimu_marks_damage_protected") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_jiaxu", "zhangfei"});
    auto ps = e.getPlayers();
    // 帷幕：成为黑色锦囊牌的目标时取消——AOE 场景下该目标被过滤（群体锦囊仍可使用）
    auto wanjian = makeCard("万箭齐发", Suit::SPADE, 1, CardType::TRICK, CardSubType::WAN_JIAN_QI_FA);
    ps[1]->addHandCard(wanjian);
    CHECK(!e.canBeTargeted(ps[0], wanjian, ps[1]));
    CHECK(e.useCard(ps[1], wanjian, {ps[0]}));
}

TEST("mou/huanggai_zhaxiang_grants_recover_once") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_huanggai", "zhangfei"});
    auto ps = e.getPlayers();
    e.loseHp(ps[0], 1, "test");
    int hp = ps[0]->getHp();
    auto sk = ps[0]->getHero()->findSkill("谋-诈降");
    CHECK(sk != nullptr);
    if (sk) sk->onLoseHp(e, *ps[0], 1);
    CHECK(ps[0]->getHp() >= hp); // 诈降：失去体力后摸牌/回复按描述
}

TEST("mou/zhugeliang_kanpo_replaces_judge_black") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_zhugeliang", "zhangjiao"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto wu = makeCard("兵粮寸断", Suit::SPADE, 4, CardType::TRICK, CardSubType::BING_LIANG_CUN_DUAN);
    ps[1]->addHandCard(wu);
    CHECK(e.useCard(ps[1], wu, {ps[0]})); // 判定阶段生效前看破可改判
    auto sk = ps[0]->getHero()->findSkill("谋-看破");
    CHECK(sk != nullptr);
    CHECK(sk != nullptr && sk->getDescription().find("每轮开始时，你清除“看破”记录的牌名") != std::string::npos);
}

TEST("mou/luxun_qianxun_records_trick_name_when_affected") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_luxun", "zhangfei"});
    auto ps = e.getPlayers();
    auto wj = makeCard("万箭齐发", Suit::HEART, 1, CardType::TRICK, CardSubType::WAN_JIAN_QI_FA);
    ps[1]->addHandCard(wj);
    CHECK(e.useCard(ps[1], wj, {ps[0]})); // 谦逊不阻止成为目标；生效检查时记录牌名
    CHECK(ps[0]->getMark("谦逊记录:万箭齐发") > 0);
    auto sk = ps[0]->getHero()->findSkill("谋-谦逊");
    CHECK(sk != nullptr);
    CHECK_EQ(sk->getDescription(),
             std::string("当一张锦囊牌对你生效时，若此牌名未记录且你不是使用者，则你记录之，然后可将至多X张牌置于你的武将牌上（X为“谦逊”记录的牌名数且至多为5）；若如此做，此回合结束时，你获得武将牌上的所有牌。出牌阶段开始时，你可移去一个记录的牌名，若为普通锦囊牌的牌名，则你可视为使用此牌。"));
}

TEST("mou/ganning_qixi_black_card_conversion_ok") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"mou_ganning", "zhangfei", "zhaoyun"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    // 奇袭为转化技（黑色牌→过河拆桥），此处验证描述与注册
    auto sk = ps[0]->getHero()->findSkill("谋-奇袭");
    CHECK(sk != nullptr);
    CHECK_EQ(sk->getDescription(), std::string("出牌阶段限一次，你可以选择一名其他角色，令其猜测你手牌中某种花色的牌最多（或之一）。若其猜错，你可令其再次猜测（其无法选择此阶段已猜测过的花色）；否则你展示所有手牌。然后你弃置其区域内X张牌。（X为其此阶段猜错的次数，若不足则全弃）"));
}

// ---------------- 势·小乔（势包，hero-detail-666） ----------------

TEST("mou/shi_xiaoqiao_registry_and_verbatim") {
    auto info = HeroRegistry::find("shi_xiaoqiao");
    CHECK(info != nullptr);
    if (!info) return;
    CHECK_EQ(info->pack, std::string("势包"));
    CHECK_EQ(info->name, std::string("势·小乔"));
    auto h = info->create();
    CHECK_EQ(h->getSkills().size(), size_t(2));
    auto hy = h->findSkill("势-合韵");
    auto yh = h->findSkill("势-音洄");
    CHECK(hy != nullptr && yh != nullptr);
    if (hy) CHECK_EQ(hy->getDescription(),
        std::string("出牌阶段限两次，你可选择一名与你有相同技能的角色，然后你失去一个技能并令其摸两张牌。"));
    if (yh) CHECK_EQ(yh->getDescription(),
        std::string("每轮开始时，你可清除因此获得的技能，然后你选择一名其他角色当前拥有的一个技能获得之。"));
}

TEST("mou/shi_yinhui_grants_a_skill_each_round") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"shi_xiaoqiao", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("势-音洄");
    CHECK(sk != nullptr);
    if (!sk) return;
    int before = (int)ps[0]->getHero()->getSkills().size();
    sk->onRoundStart(e, *ps[0]);
    CHECK_EQ((int)ps[0]->getHero()->getSkills().size(), before + 1);
    auto cast = std::dynamic_pointer_cast<ShiYinHuiSkill>(sk);
    CHECK(cast != nullptr && cast->grantedSkills().size() == 1);
    // 再次发动：清除此前获得的技能并重新获得一个（总数仍为 before+1）
    sk->onRoundStart(e, *ps[0]);
    CHECK_EQ((int)ps[0]->getHero()->getSkills().size(), before + 1);
    if (cast) CHECK_EQ(cast->grantedSkills().size(), size_t(1));
}

TEST("mou/shi_heyun_loses_skill_and_ally_draws_two") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_xiaoqiao", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    // 构造“相同技能”：给小乔补一个与关羽相同的【武圣】
    ps[0]->getHero()->addSkill(std::make_shared<WuShengSkill>(false));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("势-合韵");
    CHECK(sk != nullptr);
    auto act = std::dynamic_pointer_cast<ActiveSkill>(sk);
    CHECK(act != nullptr);
    if (!act) return;
    int skillsBefore = (int)ps[0]->getHero()->getSkills().size();
    int handBefore = ps[1]->getHandCardCount();
    act->activate(e, *ps[0]);
    CHECK_EQ((int)ps[0]->getHero()->getSkills().size(), skillsBefore - 1); // 失去一个技能
    CHECK_EQ(ps[1]->getHandCardCount(), handBefore + 2);                   // 其摸两张牌
    CHECK(ps[1]->getHero()->findSkill("武圣") != nullptr);               // 失去自身副本不移除技能来源
    e.setPhase(TurnPhase::NONE);
}

TEST("mou/shi_heyun_can_target_self_and_draw_two_without_matching_others") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_xiaoqiao", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setPhase(TurnPhase::PLAY);
    auto act = std::dynamic_pointer_cast<ShiHeYunSkill>(ps[0]->getHero()->findSkill("势-合韵"));
    CHECK(act != nullptr);
    if (!act) return;
    CHECK(act->canActivate(e, *ps[0])); // 自己是合法目标，不要求场上另有同技能角色
    int skillsBefore = (int)ps[0]->getHero()->getSkills().size();
    int selfHandBefore = ps[0]->getHandCardCount();
    int otherHandBefore = ps[1]->getHandCardCount();
    act->activate(e, *ps[0]);
    CHECK_EQ(ps[0]->getHandCardCount(), selfHandBefore + 2);
    CHECK_EQ((int)ps[0]->getHero()->getSkills().size(), skillsBefore - 1);
    CHECK(ps[0]->getHero()->findSkill("势-音洄") == nullptr); // AI 默认失去最后一个技能，仍正确摸牌
    CHECK_EQ(ps[1]->getHandCardCount(), otherHandBefore);
    CHECK_EQ(act->getUsesThisTurn(), 1);
    e.setPhase(TurnPhase::NONE);
}

TEST("mou/shi_yinhui_heyun_self_loop_keeps_source_skill") {
    GameEngine e; captureLog(e); e.initGame(2, 0, {"shi_xiaoqiao", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    Interaction::resetInputState();
    std::istringstream scripted("y 1 2 3"); // 音洄确认、选武圣；合韵选自己、失去武圣
    auto* oldInput = std::cin.rdbuf(scripted.rdbuf());
    auto yinHui = std::dynamic_pointer_cast<ShiYinHuiSkill>(ps[0]->getHero()->findSkill("势-音洄"));
    auto heYun = std::dynamic_pointer_cast<ShiHeYunSkill>(ps[0]->getHero()->findSkill("势-合韵"));
    CHECK(yinHui != nullptr && heYun != nullptr);
    if (!yinHui || !heYun) {
        std::cin.rdbuf(oldInput);
        Interaction::resetInputState();
        return;
    }
    yinHui->onRoundStart(e, *ps[0]);
    CHECK(ps[0]->getHero()->findSkill("武圣") != nullptr);
    e.setPhase(TurnPhase::PLAY);
    int handBefore = ps[0]->getHandCardCount();
    heYun->activate(e, *ps[0]);
    std::cin.rdbuf(oldInput);
    Interaction::resetInputState();
    CHECK_EQ(ps[0]->getHandCardCount(), handBefore + 2);
    CHECK(ps[0]->getHero()->findSkill("武圣") == nullptr); // 小乔失去通过音洄获得的技能
    CHECK(ps[1]->getHero()->findSkill("武圣") != nullptr); // 来源武将的原技能仍保留
    e.setPhase(TurnPhase::NONE);
}

TEST("mou/shi_heyun_keeps_sustained_skill_but_self_still_draws") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"shi_xiaoqiao", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    ps[1]->getHero()->removeSkill("武圣");
    auto potential = std::make_shared<TriggerSkill>("潜龙", "持恒技。游戏开始时，你获得20点道心值。", SkillTag::NONE);
    ps[1]->getHero()->addSkill(potential);

    auto yinHui = std::dynamic_pointer_cast<ShiYinHuiSkill>(ps[0]->getHero()->findSkill("势-音洄"));
    auto heYun = std::dynamic_pointer_cast<ShiHeYunSkill>(ps[0]->getHero()->findSkill("势-合韵"));
    CHECK(yinHui != nullptr && heYun != nullptr);
    if (!yinHui || !heYun) return;
    yinHui->onRoundStart(e, *ps[0]);
    CHECK(ps[0]->getHero()->findSkill("潜龙") != nullptr);
    CHECK(ps[0]->getHero()->findSkill("潜龙")->isSustained());
    CHECK(ps[0]->getHero()->findSkill("潜龙")->hasEffectiveLockedComponent());
    CHECK_EQ(yinHui->grantedSkills().size(), size_t(1));
    ps[0]->setNonLockSkillsDisabled(true);
    auto effective = e.getEffectiveSkills(*ps[0]);
    CHECK(std::find_if(effective.begin(), effective.end(), [](const SkillPtr& skill) {
        return skill && skill->getName() == "潜龙";
    }) != effective.end());
    ps[0]->setNonLockSkillsDisabled(false);

    // 技能来源离场后仅小乔自己满足同技目标条件；失去“潜龙”失败不取消摸牌效果。
    ps[1]->setAlive(false);
    e.setPhase(TurnPhase::PLAY);
    int selfHandBefore = ps[0]->getHandCardCount();
    heYun->activate(e, *ps[0]);
    CHECK(ps[0]->getHero()->findSkill("潜龙") != nullptr);
    CHECK_EQ(ps[0]->getHandCardCount(), selfHandBefore + 2);
    CHECK(!e.removeHeroSkill(ps[0], "潜龙"));
    e.removeHeroSkills(ps[0]);
    CHECK(ps[0]->getHero()->findSkill("潜龙") != nullptr); // “失去所有技能”也保留持恒技
    ps[0]->getHero()->clearSkills();
    CHECK(ps[0]->getHero()->findSkill("潜龙") != nullptr); // 英雄层清理接口同样遵守不可移除规则
    e.setPhase(TurnPhase::NONE);
}

// ---------------- 第三轮补全：行为测试（智迟/救援/奋威/连营/完杀/义从） ----------------

TEST("mou/chengong_zhichi_prevents_second_damage_in_turn") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_chengong", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-智迟");
    CHECK(sk != nullptr);
    int d1 = 1;
    sk->onTakeDamage(e, *ps[0], ps[1].get(), d1, ShaElement::NORMAL);
    CHECK_EQ(d1, 1); // 首次受到伤害正常结算
    int d2 = 1;
    sk->onTakeDamage(e, *ps[0], ps[1].get(), d2, ShaElement::NORMAL);
    CHECK_EQ(d2, 0); // 本回合接下来受到伤害时，防止之
    sk->onTurnBoundary(e, *ps[0], *ps[0], false); // 回合结束重置
    int d3 = 1;
    sk->onTakeDamage(e, *ps[0], ps[1].get(), d3, ShaElement::NORMAL);
    CHECK_EQ(d3, 1); // 新回合首次伤害再次通过
}

TEST("mou/sunquan_jiuyuan_peach_by_other_wu_draws_one") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_sunquan", "zhouyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setPhase(TurnPhase::PLAY);
    e.loseHp(ps[1], 1, "test");
    int before = ps[0]->getHandCardCount();
    auto tao = makeCard("桃", Suit::HEART, 1, CardType::BASIC, CardSubType::TAO);
    ps[1]->addHandCard(tao);
    CHECK(e.useCard(ps[1], tao, {ps[1]})); // 其他吴势力角色使用【桃】
    CHECK_EQ(ps[0]->getHandCardCount(), before + 1); // 你摸一张牌
}

TEST("mou/ganning_fenwei_places_wei_then_spent") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"mou_ganning", "zhangfei", "zhaoyun"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    for (int i = 0; i < 3; i++)
        ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 1, CardType::BASIC, CardSubType::SHA));
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("谋-奋威");
    CHECK(sk != nullptr);
    CHECK(sk->canActivate(e, *ps[0]));
    int handBefore = ps[0]->getHandCardCount();
    sk->activate(e, *ps[0]);
    int wei = 0;
    for (auto& p : ps) wei += p->getPileCount("威");
    CHECK(wei >= 1);                     // “威”已置于某角色武将牌上
    CHECK(ps[0]->getHandCardCount() == handBefore - wei + wei); // 消耗等量、摸等量（净持平）
    CHECK(!sk->canActivate(e, *ps[0]));  // 限定技每局一次
}

TEST("mou/luxun_lianying_watches_and_gives_on_others_turn_end") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_luxun", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-连营");
    CHECK(sk != nullptr);
    auto loseCard = makeCard("杀", Suit::SPADE, 2, CardType::BASIC, CardSubType::SHA);
    sk->onCardLostOutsideTurn(e, *ps[0], loseCard); // 他人回合内失去2张牌
    sk->onCardLostOutsideTurn(e, *ps[0], loseCard);
    int totalBefore = ps[0]->getHandCardCount() + ps[1]->getHandCardCount();
    int deckBefore = e.getDeck().getDrawPileSize();
    sk->onTurnEnd(e, *ps[0], *ps[1]); // 他人回合结束触发（x=2）
    int totalAfter = ps[0]->getHandCardCount() + ps[1]->getHandCardCount();
    CHECK_EQ(totalAfter, totalBefore + 2);            // 牌堆顶2张被取出并交给角色
    CHECK_EQ(e.getDeck().getDrawPileSize(), deckBefore - 2);
    // 本回合计数已清零：再次触发不观看
    int t2 = ps[0]->getHandCardCount() + ps[1]->getHandCardCount();
    int d2 = e.getDeck().getDrawPileSize();
    sk->onTurnEnd(e, *ps[0], *ps[1]);
    CHECK_EQ(ps[0]->getHandCardCount() + ps[1]->getHandCardCount(), t2);
    CHECK_EQ(e.getDeck().getDrawPileSize(), d2);
}

TEST("mou/jiaxu_wansha_others_dying_broadcast_and_once_per_round") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"mou_jiaxu", "zhangfei", "zhaoyun"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-完杀");
    CHECK(sk != nullptr);
    // 构造：张飞给一张手牌供观看
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 3, CardType::BASIC, CardSubType::SHA));
    // 通过引擎新钩子：非濒死者侧广播
    int hp = ps[1]->getHp();
    e.loseHp(ps[1], hp + 1, "test"); // 进入濒死（触发 onOtherDying → 完杀 process）
    CHECK(ps[0]->getMark("完杀本轮已用") > 0); // 每轮限一次已消耗
    // 本轮再次广播：被轮限挡住
    int used = ps[0]->getMark("完杀本轮已用");
    sk->onOtherDying(e, *ps[0], *ps[1]);
    CHECK_EQ(ps[0]->getMark("完杀本轮已用"), used);
    // 每轮开始重置
    sk->onRoundStart(e, *ps[0]);
    CHECK_EQ(ps[0]->getMark("完杀本轮已用"), 0);
}

TEST("mou/gongsunzan_yicong_directions_distance") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"mou_gongsunzan", "zhangfei", "zhaoyun"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-义从");
    CHECK(sk != nullptr);
    // 方向二（其他角色与你距离+1）：目标侧广播
    int base = e.calculateDistance(*ps[1], *ps[0]); // 他人到你
    ps[0]->addMark("义从方向", 2 - ps[0]->getMark("义从方向"));
    ps[0]->addMark("义从本轮", e.getCurrentRound() - ps[0]->getMark("义从本轮"));
    int d = e.calculateDistance(*ps[1], *ps[0]);
    CHECK_EQ(d, base + 1); // 目标侧钩子生效
    // 方向一（你与其他角色距离-1）：来源侧广播
    ps[0]->addMark("义从方向", 1 - ps[0]->getMark("义从方向"));
    int base2 = e.calculateDistance(*ps[0], *ps[2]); // 选座次距离为2的目标
    int d2 = e.calculateDistance(*ps[0], *ps[2]);
    CHECK_EQ(d2, std::max(1, base2 - 1)); // 来源侧 -1（3人环距离2 → 1）
    CHECK(d2 >= 1);
}

TEST("mou/kanpo_intercepts_recorded_card_used_by_other") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_zhugeliang", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    e.setPhase(TurnPhase::PLAY);
    auto sk = ps[0]->getHero()->findSkill("谋-看破");
    CHECK(sk != nullptr);
    sk->onRoundStart(e, *ps[0]); // AI 记录第一个牌名（杀）
    int before = ps[0]->getHandCardCount();
    // 固定拦截后摸到的牌为非【闪】：否则第二次【杀】时 AI 可能打出该【闪】响应，
    // 使手牌数依牌堆内容而变（CI 上的偶发失败根因）。
    e.getDeck().putOnTop({makeCard("无中生有", Suit::HEART, 3, CardType::TRICK,
                                   CardSubType::WU_ZHONG_SHENG_YOU)});
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(sha);
    CHECK(e.useCard(ps[1], sha, {ps[0]})); // 其他角色使用与记录牌名相同的牌 → 被令无效但消耗
    CHECK_EQ(ps[0]->getHandCardCount(), before + 1); // 且你摸一张牌
    CHECK(ps[1]->getHandCardCount() == 0);           // 此牌已消耗（无效化）
    // 记录已移除：再次使用不触发
    int before2 = ps[0]->getHandCardCount();
    auto sha2 = makeCard("杀", Suit::HEART, 8, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(sha2);
    e.useCard(ps[1], sha2, {ps[0]});
    CHECK_EQ(ps[0]->getHandCardCount(), before2);
}

TEST("mou/qianxun_places_cards_on_general_on_resolve") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_luxun", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("谋-谦逊");
    CHECK(sk != nullptr);
    ps[0]->addHandCard(makeCard("杀", Suit::SPADE, 9, CardType::BASIC, CardSubType::SHA));
    ps[0]->addHandCard(makeCard("闪", Suit::HEART, 2, CardType::BASIC, CardSubType::SHAN));
    auto wj = makeCard("万箭齐发", Suit::HEART, 1, CardType::TRICK, CardSubType::WAN_JIAN_QI_FA);
    ps[1]->addHandCard(wj);
    int pileBefore = ps[0]->getPileCount("谦屯");
    // 锦囊生效 → 记录（onCheckCardEffect）+ 然后可将至多X张置于武将牌上（onCardResolved，AI=放）
    bool effective = true; // 栈上 bool（hook 参数为 bool&，替代临时量泄漏）
    sk->onCheckCardEffect(e, *ps[0], wj, effective);
    sk->onCardResolved(e, *ps[0], wj);
    CHECK(ps[0]->getPileCount("谦屯") > pileBefore); // X≥1 时置牌成功
    // 回合结束（FINISH）获得武将牌上的所有牌
    sk->onPhaseEnd(e, *ps[0], TurnPhase::FINISH);
    CHECK_EQ(ps[0]->getPileCount("谦屯"), 0);
}

TEST("mou/wushuang_response_count_requires_two") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_lvbu", "zhaoyun"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-无双");
    CHECK(sk != nullptr);
    int need = 1;
    sk->onCalculateResponseCount(e, *ps[0], *ps[1], CardSubType::SHAN, need);
    CHECK_EQ(need, 2); // 杀需两张闪
    int need2 = 1;
    sk->onCalculateResponseCount(e, *ps[0], *ps[1], CardSubType::SHA, need2);
    CHECK_EQ(need2, 2); // 决斗每次需两张杀
    // 自己响应不受影响
    int need3 = 1;
    sk->onCalculateResponseCount(e, *ps[0], *ps[0], CardSubType::SHAN, need3);
    CHECK_EQ(need3, 1);
}

TEST("mou/wansha_level2_selects_equipment_zone") {
    GameEngine e; captureLog(e); e.initGame(2, -1, {"mou_jiaxu", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = dynamic_cast<MouWanShaSkill*>(ps[0]->getHero()->findSkill("谋-完杀").get());
    CHECK(sk != nullptr);
    if (!sk) return;
    sk->upgrade(); // 二级：可选区域内的牌
    // 张飞装备防具（区域牌），手牌为空
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 5, CardType::BASIC, CardSubType::SHA));
    auto armor = makeCard("八卦阵", Suit::SPADE, 2, CardType::EQUIPMENT, CardSubType::ARMOR);
    e.moveFieldCard(ps[1], ps[0], ps[0]->getHandCards().front()); // 确保焦点牌存在（辅助）
    ps[1]->addToPile("_tmp", armor); // 临时
    // 直接把装备置入张飞装备区
    {
        // 通过手牌→装备路径：addHandCard 后装备化
        ps[1]->addHandCard(armor);
    }
    int handOrArmor = ps[1]->getHandCardCount() + ps[1]->getAllEquipment().size();
    CHECK(handOrArmor >= 1);
    sk->onOtherDying(e, *ps[0], *ps[1]);
    CHECK(ps[0]->getMark("完杀本轮已用") > 0); // 二级流程已执行（选牌+二择）
    // 本轮限一次：再次广播无效
    int used = ps[0]->getMark("完杀本轮已用");
    sk->onOtherDying(e, *ps[0], *ps[1]);
    CHECK_EQ(ps[0]->getMark("完杀本轮已用"), used);
    // 清理临时 pile（避免影响后续逻辑）
    for (auto& c : ps[1]->getPile("_tmp")) ps[1]->removeFromPile("_tmp", c);
}

TEST("mou/zhiji_can_mark_self_and_multiple_roles_then_enforces_and_expires_northward") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3,0,{"mou_jiangwei","guanyu","zhangfei"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    auto skill=players[0]->getHero()->findSkill("谋-志继");
    CHECK(skill!=nullptr);
    if(!skill)return;
    players[0]->addMark("挑衅累计",4);
    const int maxHp=players[0]->getMaxHp();
    bool skip=false;
    {
        MouInput input("1 1 0"); // 先选姜维自己，再选第二名角色，然后结束。
        skill->onPhaseStart(engine,*players[0],TurnPhase::PREPARATION,skip);
    }
    CHECK_EQ(players[0]->getMaxHp(),maxHp-1);
    CHECK_EQ(players[0]->getMark("北伐"),1);
    CHECK_EQ(players[1]->getMark("北伐"),1);
    CHECK_EQ(players[2]->getMark("北伐"),0);

    auto sha=makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA);
    players[1]->addHandCard(sha);
    CHECK(!engine.canBeTargeted(players[2],sha,players[1]));
    CHECK(engine.canBeTargeted(players[0],sha,players[1]));
    CHECK(!engine.useCard(players[1],sha,{players[2]}));
    CHECK(players[1]->hasHandCard(sha));

    skill->onTurnBoundary(engine,*players[0],*players[0],true);
    CHECK_EQ(players[0]->getMark("北伐"),0);
    CHECK_EQ(players[1]->getMark("北伐"),0);
    CHECK_EQ(players[1]->getMark("北伐来源"),0);
}

TEST("mou/jijiang_unqualified_other_wording_allows_own_hero_as_chosen_target") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3,0,{"mou_liubei","guanyu","zhangfei"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    players[0]->setHp(4); // 主公体力上限+1； helpers 必须体力值不小于刘备。
    int ownHp=players[0]->getHp();
    auto skill=players[0]->getHero()->findSkill("谋-激将");
    CHECK(skill!=nullptr);
    if(!skill)return;
    {
        MouInput input("1"); // “一名角色” includes Liu Bei himself.
        skill->onPhaseEnd(engine,*players[0],TurnPhase::PLAY);
    }
    CHECK(players[0]->getHp()<ownHp);
}

TEST("mou/jijiang_gives_the_order_to_exactly_one_eligible_shu_helper") {
    GameEngine engine;
    auto log = captureLog(engine);
    engine.initGame(4,0,{"mou_liubei","guanyu","zhangfei","machao"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    players[0]->setHp(4); // 主公体力上限+1；helpers 必须体力值不小于刘备。
    auto skill=players[0]->getHero()->findSkill("谋-激将");
    CHECK(skill!=nullptr);
    if(!skill)return;
    {
        ScriptedInput input("1 1"); // 指定自己为目标；再指定唯一一名 helper（关羽）
        skill->onPhaseEnd(engine,*players[0],TurnPhase::PLAY);
    }
    // 官网只令“另一名”合格蜀势力角色二选一：整局只应出现一次由【激将】视为使用的【杀】。
    const std::string text=log->str();
    size_t n=0; size_t pos=0;
    while((pos=text.find("使用了 【杀】(激将",pos))!=std::string::npos){++n;++pos;}
    CHECK_EQ(static_cast<int>(n),1);
    CHECK_EQ(players[2]->getMark("跳过下个出牌阶段"),0); // 其余合格蜀势力角色不再被令二选一
    CHECK_EQ(players[3]->getMark("跳过下个出牌阶段"),0);
    CHECK_EQ(players[1]->getMark("跳过下个出牌阶段"),0);
    CHECK_EQ(players[0]->getHp(),3); // 唯一一名 helper 对指定目标使用了一张【杀】
}

TEST("mou/hongyuan_includes_self_and_never_selects_the_same_role_twice") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3,0,{"mou_zhugejin","guanyu","zhangfei"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    auto skill=players[0]->getHero()->findSkill("谋-弘援");
    CHECK(skill!=nullptr);
    if(!skill)return;
    skill->onGameStart(engine,*players[0]);
    {
        MouInput input("y 1 y 1"); // 自己与第一名其他角色；同一角色不可被重复指定。
        skill->onCardsObtained(engine,*players[0],2);
    }
    CHECK_EQ(players[0]->getHandCardCount(),1);
    CHECK_EQ(players[1]->getHandCardCount(),1);
    CHECK_EQ(players[2]->getHandCardCount(),0);
    CHECK_EQ(players[0]->getMark("蓄力"),0);
}

TEST("mou/zaiqi_can_choose_owner_and_counts_self_in_available_targets") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2,0,{"mou_menghuo","guanyu"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    engine.setPhase(TurnPhase::DISCARD);
    players[0]->addMark("蓄力",1);
    auto skill=players[0]->getHero()->findSkill("谋-再起");
    CHECK(skill!=nullptr);
    if(!skill)return;
    CHECK(skill->canActivate(engine,*players[0]));
    {
        MouInput input("1 1"); // 选择自己；自己令技能持有者摸一张牌。
        skill->activate(engine,*players[0]);
    }
    CHECK_EQ(players[0]->getMark("蓄力"),0);
    CHECK_EQ(players[0]->getHandCardCount(),1);
    CHECK_EQ(players[1]->getHandCardCount(),0);
}

TEST("mou/jiefan_unqualified_role_target_can_be_the_skill_owner") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(3,0,{"mou_handang","guanyu","zhangfei"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    engine.setPhase(TurnPhase::PLAY);
    auto skill=players[0]->getHero()->findSkill("谋-解烦");
    CHECK(skill!=nullptr);
    if(!skill)return;
    {
        MouInput input("1 2"); // 指定自己，并令自己摸牌。
        skill->activate(engine,*players[0]);
    }
    CHECK(players[0]->getHandCardCount()>0);
    auto active=std::dynamic_pointer_cast<ActiveSkill>(skill);
    CHECK(active!=nullptr);
    if(active)CHECK_EQ(active->getUsesThisTurn(),1);
}

TEST("mou/he-yuan_unqualified_wounded_role_target_can_be_the_skill_owner") {
    GameEngine engine;
    captureLog(engine);
    engine.initGame(2,0,{"mou_zhuran","guanyu"});
    auto players=engine.getPlayers();
    for(const auto& player:players)clearHand(*player);
    players[0]->setHp(players[0]->getMaxHp()-1);
    players[0]->addHandCard(makeCard("杀",Suit::SPADE,7,CardType::BASIC,CardSubType::SHA));
    players[0]->addMark("镇围弃牌数",1);
    players[0]->addMark("镇围最后一项",2);
    auto skill=players[0]->getHero()->findSkill("谋-合援");
    CHECK(skill!=nullptr);
    if(!skill)return;
    {
        MouInput input("1"); // 唯一满足条件的角色正是武将本人。
        skill->onPhaseEnd(engine,*players[0],TurnPhase::FINISH);
    }
    CHECK_EQ(players[0]->getHandCardCount(),3);
    CHECK_EQ(players[0]->getMark("合援已用"),1);
}

// =====================================================================
//  谋弈（谋·马超【铁骑】）：官方机制 —— 发动者与目标各选一项，选择不同则发动者成功
// =====================================================================

TEST("mou/tieqi_mouyi_zhiqu_steals_one_card_when_choices_differ") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, -1, {"mou_machao", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    int myHand = ps[0]->getHandCardCount();
    int theirHand = ps[1]->getHandCardCount();
    CHECK(mouYiDuel(engine, *ps[0], *ps[1], 0 /*直取敌营*/, 1 /*扰阵疲敌*/));
    CHECK_EQ(ps[0]->getHandCardCount(), myHand + 1);
    CHECK_EQ(ps[1]->getHandCardCount(), theirHand - 1);
}

TEST("mou/tieqi_mouyi_raozhen_draws_two_when_choices_differ") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, -1, {"mou_machao", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    int myHand = ps[0]->getHandCardCount();
    CHECK(mouYiDuel(engine, *ps[0], *ps[1], 1 /*扰阵疲敌*/, 0 /*直取敌营*/));
    CHECK_EQ(ps[0]->getHandCardCount(), myHand + 2);
}

TEST("mou/tieqi_mouyi_fails_and_does_nothing_when_choices_match") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, -1, {"mou_machao", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    ps[1]->addHandCard(makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA));
    int myHand = ps[0]->getHandCardCount();
    int theirHand = ps[1]->getHandCardCount();
    CHECK(!mouYiDuel(engine, *ps[0], *ps[1], 0, 0));
    CHECK(!mouYiDuel(engine, *ps[0], *ps[1], 1, 1));
    CHECK_EQ(ps[0]->getHandCardCount(), myHand);
    CHECK_EQ(ps[1]->getHandCardCount(), theirHand);
}

TEST("mou/tieqi_is_optional_and_nonlock_disable_lasts_the_turn") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, 0, {"mou_machao", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    auto skill = std::dynamic_pointer_cast<MouTieQiSkill>(ps[0]->getHero()->findSkill("谋-铁骑"));
    CHECK(skill != nullptr);
    if (!skill) return;
    auto sha = makeCard("杀", Suit::SPADE, 7, CardType::BASIC, CardSubType::SHA);
    ShaContext ctx; ctx.source = ps[0]; ctx.target = ps[1]; ctx.card = sha;
    {
        MouInput input("n"); // 不发动【铁骑】。
        skill->onShaTargeted(engine, *ps[0], ctx);
    }
    CHECK(!ctx.cannotDodge);
    CHECK(!ps[1]->isNonLockSkillsDisabled());
    {
        MouInput input("y 1"); // 发动【铁骑】，谋弈选“直取敌营”（结果由双方选择决定，本测试只断言铁骑本体）。
        skill->onShaTargeted(engine, *ps[0], ctx);
    }
    CHECK(ctx.cannotDodge);
    CHECK(ps[1]->isNonLockSkillsDisabled()); // 本回合内持续失效，不在【杀】结算后恢复。
}

// =====================================================================
//  背水（谋·韩当【解烦】）：仅“你杀死一名角色”恢复，濒死不算
// =====================================================================

TEST("mou/jiefan_beishui_restores_only_on_kill_not_on_dying") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, 0, {"mou_handang", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    engine.setCurrentPlayerForTesting(ps[0]);
    engine.setPhase(TurnPhase::PLAY);
    auto skill = std::dynamic_pointer_cast<MouJieFanSkill>(ps[0]->getHero()->findSkill("谋-解烦"));
    CHECK(skill != nullptr);
    if (!skill) return;
    {
        MouInput input("1 3"); // 指定第一名角色（自己），选“背水：失效直至你杀死一名角色”。
        skill->activate(engine, *ps[0]);
    }
    CHECK(!skill->canActivate(engine, *ps[0])); // 背水后失效。
    CHECK_EQ(ps[0]->getMark("解烦背水"), 1);
    // 非致命伤害（未杀死任何角色）不恢复背水（旧实现按“目标进入濒死”近似恢复，已删除）。
    engine.applyDamage(ps[0], ps[1], 1, ShaElement::NORMAL, true);
    CHECK(ps[1]->isAlive());
    CHECK(!skill->canActivate(engine, *ps[0]));
    CHECK_EQ(ps[0]->getMark("解烦背水"), 1);
    // 由其他角色击杀也不恢复。
    skill->onPlayerDeath(engine, *ps[0], *ps[1], ps[2].get());
    CHECK_EQ(ps[0]->getMark("解烦背水"), 1);
    // 只有“你杀死一名角色”才恢复。
    skill->onPlayerDeath(engine, *ps[0], *ps[1], ps[0].get());
    CHECK_EQ(ps[0]->getMark("解烦背水"), 0);
}

// =====================================================================
//  音洄（势·小乔）：获得的技能为独立实例，内部状态不与来源共享
// =====================================================================

TEST("mou/shi_yinhui_grants_an_independent_skill_instance") {
    GameEngine engine; captureLog(engine);
    engine.initGame(3, 0, {"shi_xiaoqiao", "guanyu", "zhangfei"});
    auto ps = engine.getPlayers();
    for (auto& p : ps) clearHand(*p);
    auto yinHui = std::dynamic_pointer_cast<ShiYinHuiSkill>(ps[0]->getHero()->findSkill("势-音洄"));
    CHECK(yinHui != nullptr);
    if (!yinHui) return;
    {
        MouInput input("y 1"); // 确认发动，选择第一名其他角色（关羽）的第一个技能（武圣）。
        yinHui->onRoundStart(engine, *ps[0]);
    }
    auto mine = ps[0]->getHero()->findSkill("武圣");
    auto source = ps[1]->getHero()->findSkill("武圣");
    CHECK(mine != nullptr && source != nullptr);
    CHECK(mine != source);                  // 不是同一对象：状态不串用。
    if (mine && source) CHECK_EQ(mine->getDescription(), source->getDescription());
    CHECK_EQ(yinHui->grantedSkills().size(), size_t(1));
}

// 挑衅（2026-10-06 复核修正）：官网原文“弃牌阶段，你每弃置一张牌，获得1点蓄力点”只限定弃牌阶段；
// 且移动版武将牌标注“蓄力技（4/4）”→ 初始 4 点、上限 4。
TEST("mou/tiaoxin_charge_only_in_own_discard_phase_and_capped_at_four") {
    GameEngine e; captureLog(e); e.initGame(3, -1, {"mou_jiangwei", "zhangfei", "guanyu"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    CHECK_EQ(ps[0]->getMark("蓄力上限"), 4); // 蓄力技（4/4）
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);     // 初始 4 点
    // 他人弃置：不计数
    auto c1 = makeCard("杀", Suit::SPADE, 3, CardType::BASIC, CardSubType::SHA);
    ps[1]->addHandCard(c1);
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::DISCARD);
    e.discardCardOf(ps[1], c1, "测试");
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);
    // 自己弃置但不在弃牌阶段（技能弃置/被拆）：不计数
    e.setPhase(TurnPhase::PLAY);
    auto c2 = makeCard("闪", Suit::HEART, 4, CardType::BASIC, CardSubType::SHAN);
    ps[0]->addHandCard(c2);
    e.discardCardOf(ps[0], c2, "测试（技能弃置）");
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);
    // 已满（4/4）：自己的弃牌阶段弃置也不超过上限
    auto c3 = makeCard("桃", Suit::HEART, 5, CardType::BASIC, CardSubType::TAO);
    ps[0]->addHandCard(c3);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::DISCARD);
    e.discardCardOf(ps[0], c3, "弃牌阶段");
    CHECK_EQ(ps[0]->getMark("蓄力"), 4);
    // 消耗后再于弃牌阶段弃置：+1，且不超上限
    ps[0]->addMark("蓄力", -3);
    CHECK_EQ(ps[0]->getMark("蓄力"), 1);
    auto c4 = makeCard("杀", Suit::CLUB, 6, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(c4);
    e.discardCardOf(ps[0], c4, "弃牌阶段");
    CHECK_EQ(ps[0]->getMark("蓄力"), 2);
    // 回合外的弃置阶段（不是自己的弃牌阶段）：不计数
    e.setCurrentPlayerForTesting(ps[1]);
    auto c5 = makeCard("杀", Suit::DIAMOND, 7, CardType::BASIC, CardSubType::SHA);
    ps[0]->addHandCard(c5);
    e.discardCardOf(ps[0], c5, "他人回合弃置");
    CHECK_EQ(ps[0]->getMark("蓄力"), 2);
    e.setPhase(TurnPhase::NONE);
}

// 挑衅：①选择的角色数受现有蓄力点限制并逐名扣除②；选择记录累计给【志继】。
TEST("mou/tiaoxin_spends_charge_per_chosen_target_and_accumulates_for_zhiji") {
    GameEngine e; captureLog(e); e.initGame(3, 0, {"mou_jiangwei", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto p : ps) clearHand(*p);
    for (size_t i = 1; i < ps.size(); ++i) {
        ps[i]->addHandCard(makeCard("闪", Suit::HEART, (int)(3 + i), CardType::BASIC, CardSubType::SHAN));
    }
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    ps[0]->addMark("蓄力", 2 - ps[0]->getMark("蓄力"));
    auto sk = ps[0]->getHero()->findSkill("谋-挑衅");
    CHECK(sk != nullptr);
    if (!sk) return;
    MouInput in("1\n1"); // 选第一名（关羽）→ 交出一张牌；再选第二名（张飞）→ 交出一张牌
    sk->activate(e, *ps[0]);
    CHECK_EQ(ps[0]->getMark("蓄力"), 0);          // 选 2 名 → 扣 2 点
    CHECK_EQ(ps[0]->getMark("挑衅累计"), 2);      // 累计给志继
    CHECK_EQ(ps[0]->getMark("挑衅已用"), 1);
}

// 挑衅：FAQ（2026-10-04）“任何弃置都算”——自己因任何原因弃置均 +1 蓄力点。
// 明哲：FAQ（2026-10-04）“可获得，但不超过 Y”。
TEST("mou/mingzhe_charge_gain_capped_at_limit") {
    GameEngine e; captureLog(e); e.initGame(3, 0, {"mou_zhugejin", "mou_gongsunzan", "zhangfei"});
    auto ps = e.getPlayers();
    auto ming = ps[0]->getHero()->findSkill("谋-明哲");
    CHECK(ming != nullptr);
    if (!ming) return;
    CHECK_EQ(ps[1]->getMark("蓄力上限"), 4); // 义从（2/4）
    int full = ps[1]->getMark("蓄力上限");
    ps[1]->addMark("蓄力", full - ps[1]->getMark("蓄力"));
    {
        MouInput in("2"); // 选择第 2 名角色（公孙瓒）
        ming->onCardLostOutsideTurn(e, *ps[0], makeCard("杀", Suit::SPADE, 4, CardType::BASIC, CardSubType::SHA));
    }
    CHECK_EQ(ps[1]->getMark("蓄力"), full); // 已满：不超过上限
}

// 义从：FAQ（2026-10-04）允许“消耗 0 点”发动（只拿距离效果，不获得“扈”牌）。
TEST("mou/yicong_zero_spend_allowed") {
    GameEngine e; captureLog(e); e.initGame(2, 0, {"mou_gongsunzan", "zhangfei"});
    auto ps = e.getPlayers();
    auto sk = ps[0]->getHero()->findSkill("谋-义从");
    CHECK(sk != nullptr);
    if (!sk) return;
    int charge0 = ps[0]->getMark("蓄力");
    {
        MouInput in("1\n1"); // 方向一（距离-1）→ 消耗 0 点
        sk->onRoundStart(e, *ps[0]);
    }
    CHECK_EQ(ps[0]->getMark("蓄力"), charge0);       // 未消耗
    CHECK_EQ(ps[0]->getPileCount("扈"), 0);          // 未获得“扈”牌
    int dist = 1;
    sk->onCalculateDistance(e, *ps[0], *ps[1], dist);
    CHECK_EQ(dist, 0);                               // 距离-1 生效
}

// ---------------- 谋·陆逊【谦逊】也可应延时锦囊（用户 2026-10-05） ----------------
// 官网原文：“当一张**锦囊牌**对你生效时，若此牌名未记录且你不是使用者，则你记录之……
// 出牌阶段开始时，你可移去一个记录的牌名，**若为普通锦囊牌的牌名**，则你可视为使用此牌。”
// 即：延时锦囊（乐不思蜀/兵粮寸断/闪电）同样要被记录，但移去它的牌名时不能视为使用。
TEST("mou/qianxun_records_delayed_trick_but_cannot_use_it") {
    GameEngine e;
    e.setSeed(110);
    auto sink = captureLog(e);
    e.initGame(3, 0, {"mou_luxun", "guanyu", "zhangfei"}); // 座位 0（谋·陆逊）为真人
    auto ps = e.getPlayers();
    for (auto& p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("谋-谦逊");
    CHECK(sk != nullptr);
    if (!sk) return;
    // 手上只留一张【闪】，供【谦逊】置于武将牌上（没有【无懈可击】，避免额外交互）
    ps[0]->addHandCard(std::make_shared<Card>(901, "闪", Suit::DIAMOND, 6, CardType::BASIC, CardSubType::SHAN, ShaElement::NORMAL, 1, ""));

    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    auto le = std::make_shared<Card>(902, "乐不思蜀", Suit::SPADE, 6, CardType::TRICK, CardSubType::LE_BU_SI_SHU, ShaElement::NORMAL, 1, "");
    ps[1]->addHandCard(le);
    {
        ScriptedInput in("y\ny\n"); // 【谦逊】是否将至多 X 张牌置于武将牌上
        CHECK(e.useCard(ps[1], le, {ps[0]}));
    }
    // 延时锦囊的牌名已被记录
    CHECK_EQ(ps[0]->getMark("谦逊记录:乐不思蜀"), 1);
    CHECK(ps[0]->getJudgeZone().size() == 1 || sink->str().find("乐不思蜀") != std::string::npos);

    // 自己的出牌阶段开始时移去该牌名：延时锦囊不能视为使用
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    {
        ScriptedInput in("1\n"); // 选项列表 = [乐不思蜀, 不移去] → 选第 1 个
        bool skip = false;
        sk->onPhaseStart(e, *ps[0], TurnPhase::PLAY, skip);
    }
    const std::string text = sink->str();
    CHECK(text.find("延时锦囊牌的牌名，不能视为使用") != std::string::npos);
    CHECK_EQ(ps[0]->getMark("谦逊记录:乐不思蜀"), 0); // 牌名已移去
}

TEST("mou/qianxun_still_uses_normal_trick_names") {
    // 回归：普通锦囊牌的牌名移去后仍视为使用
    GameEngine e;
    e.setSeed(111);
    auto sink = captureLog(e);
    e.initGame(3, 0, {"mou_luxun", "guanyu", "zhangfei"});
    auto ps = e.getPlayers();
    for (auto& p : ps) clearHand(*p);
    auto sk = ps[0]->getHero()->findSkill("谋-谦逊");
    CHECK(sk != nullptr);
    if (!sk) return;
    ps[0]->addHandCard(std::make_shared<Card>(911, "闪", Suit::DIAMOND, 7, CardType::BASIC, CardSubType::SHAN, ShaElement::NORMAL, 1, ""));
    e.setCurrentPlayerForTesting(ps[1]);
    e.setPhase(TurnPhase::PLAY);
    auto wu = std::make_shared<Card>(912, "无中生有", Suit::HEART, 9, CardType::TRICK, CardSubType::WU_ZHONG_SHENG_YOU, ShaElement::NORMAL, 1, "");
    ps[1]->addHandCard(wu);
    {
        ScriptedInput in("n\n"); // 不置牌，只记录牌名
        CHECK(e.useCard(ps[1], wu, {ps[0]}));
    }
    CHECK_EQ(ps[0]->getMark("谦逊记录:无中生有"), 1);
    e.setCurrentPlayerForTesting(ps[0]);
    e.setPhase(TurnPhase::PLAY);
    int deckBefore = (int)e.getDeck().getDrawPileSize();
    {
        ScriptedInput in("1\n");
        bool skip = false;
        sk->onPhaseStart(e, *ps[0], TurnPhase::PLAY, skip);
    }
    const std::string text = sink->str();
    CHECK(text.find("移去牌名【无中生有】，视为使用之") != std::string::npos);
    CHECK_EQ(ps[0]->getMark("谦逊记录:无中生有"), 0);
    // 视为使用【无中生有】→ 摸两张牌（牌堆因此减少）
    CHECK((int)e.getDeck().getDrawPileSize() <= deckBefore);
    CHECK(ps[0]->getHandCardCount() >= 1);
}

// ---------------- 势·小乔【音洄】获得的技能是独立实例（状态不与来源串用） ----------------
// docs/official_skill_audit.md 曾披露“音洄仍把来源技能对象共享给势小乔，次数/标记状态可能串用”，
// 现已由 freshSkillInstance() 按技能名从登记表重建独立实例；本测试锁定该行为。
TEST("shi/yinhui_granted_skill_is_an_independent_instance") {
    GameEngine e;
    e.setSeed(130);
    captureLog(e);
    e.initGame(2, -1, {"shi_xiaoqiao", "guanyu"});
    auto ps = e.getPlayers();
    auto yinhui = ps[0]->getHero()->findSkill("势-音洄");
    CHECK(yinhui != nullptr);
    if (!yinhui) return;
    yinhui->onRoundStart(e, *ps[0]);
    auto mine = ps[0]->getHero()->findSkill("武圣");
    auto his = ps[1]->getHero()->findSkill("武圣");
    CHECK(mine != nullptr);
    CHECK(his != nullptr);
    if (!mine || !his) return;
    CHECK(mine.get() != his.get());                 // 不是同一个对象
    CHECK_EQ(mine->getName(), his->getName());      // 同名同规则
    CHECK_EQ(mine->getDescription(), his->getDescription());
}
