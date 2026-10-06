#include "SkillsMyth.h"
#include "GameEngine.h"
#include "Player.h"
#include "AI.h"
#include "SkillsStd.h"
#include "HeroRegistry.h"
#include <algorithm>
#include <sstream>
#include <map>
#include <set>

namespace Thks {
namespace {
PlayerPtr selfOf(GameEngine& e, Player& p) { return e.getPlayerById(p.getId()); }
bool friendOf(GameEngine& e, const Player& a, const Player& b) { return AIController::isFriend(e, a, b); }
std::vector<CardPtr> possessions(const Player& p) {
    return p.getHandAndEquipmentCards();
}
CardPtr least(const std::vector<CardPtr>& v) { return v.empty() ? nullptr : AIController::chooseLeastValuableCard(v); }
bool confirm(GameEngine& e, Player& p, const std::string& name, bool decision = true) {
    return e.askConfirm(selfOf(e,p), "是否发动【" + name + "】？", decision);
}
CardPtr cost(GameEngine& e, Player& p, const std::string& skill, bool handOnly = false) {
    auto cards = handOnly ? p.getHandCards() : possessions(p);
    auto c = e.askChooseCard(selfOf(e,p), cards, "【"+skill+"】选择一张牌", true, least(cards));
    if (c) e.discardCardOf(selfOf(e,p),c,skill);
    return c;
}
PlayerPtr choose(GameEngine& e, Player& p, bool enemy, const std::string& skill, bool optional = true) {
    auto pool = e.getOtherAlivePlayers(p);
    PlayerPtr pick;
    for (auto& t:pool) if (friendOf(e, p,*t) != enemy) {pick=t;break;}
    return e.askChoosePlayer(selfOf(e,p),pool,"【"+skill+"】选择目标",optional,pick);
}
// 仅已实现的技能进入操作菜单；未支持的技能显式标注，不伪称可用。
// 以下技能有可玩结算但仍与官网规则有差异；图鉴明确标出，避免把近似实现当完整规则。
// 【无谋】已按最终裁定完整实现（此【杀】对应原锦囊的目标，形态任意：全体/双目标/单目标/无目标；
// 无懈可击转化的杀无角色目标、不能用于使用，可作为打出响应），移出 partial，不再标“简化版”。
const std::set<std::string> partial = {};
const std::map<std::string,std::string> texts = {
    // 以下均为三国杀移动版官网（https://www.sanguosha.cn/hero-detail-N.html “技能介绍”）原文。
    {"狂骨","锁定技，当你对距离1以内的一名角色造成1点伤害后，若你与其的距离于其因受到此伤害而扣减体力前不大于1，你回复1点体力。"},
    {"神速","你可以做出如下选择：1.跳过判定阶段和摸牌阶段。2.跳过出牌阶段并弃置一张装备牌。你每选择一项，便视为你使用一张无距离限制的【杀】。"},
    {"据守","结束阶段开始时，你可以摸三张牌。若如此做，你翻面。"},
    {"天香","当你受到伤害时，你可以弃置一张红桃手牌并选择一名其他角色。若如此做，你将此伤害转移给该角色，然后其摸X张牌（X为该角色已损失的体力值）。"},
    {"红颜","锁定技，你的黑桃手牌只能当做红桃牌使用、打出、弃置或交给其他角色。你的黑桃判定牌只能当做红桃判定牌。"},
    {"不屈","任何时候，当你的体力被扣减到或更低时，每扣减1点体力：从牌堆亮出一张牌放在你的角色牌上，若该牌的点数与你角色牌上已有的任何一张牌都不同，你可以不死去。此牌亮出的时刻为你的濒死状态。（官网原文如此）"},
    {"雷击","每当你使用或打出一张【闪】时，可令任意一名角色判定，若为黑桃，你对该角色造成2点雷电伤害。"},
    {"鬼道","当一名角色的判定牌生效前，你可以打出一张黑色牌替换之。"},
    {"黄天","主公技，其他群势力角色的出牌阶段限一次，该角色可以将一张【闪】或【闪电】交给你。"},
    {"蛊惑","当你需要使用或打出一张基本牌或普通锦囊牌时，你可以声明并将一张手牌扣于桌上。若无人质疑，则该牌按你所述之牌来用。若有人质疑，则亮出验明：若为真，质疑者各失去1点体力；若为假，质疑者各摸一张牌，除非被质疑的牌是红桃且为真（仍可用），否则无论真假，该牌都作废，弃置之。"},
    {"武神","锁定技，你的红桃手牌只能当做【杀】；你使用红桃【杀】无距离限制。"},
    {"武魂","锁定技，当你受到1点伤害后，你令伤害来源获得1枚梦魇标记；当你死亡时，你令拥有最多该标记的一名其他角色进行判定，若结果不为【桃】或【桃园结义】，则该角色死亡。"},
    {"涉猎","摸牌阶段，你可以改为亮出牌堆顶的五张牌，然后获得其中每种花色的牌各一张。"},
    {"攻心","出牌阶段限一次，你可以观看一名其他角色的手牌，然后你可以展示其中一张红桃牌，选择一项：1.弃置此牌；2.将此牌置于牌堆顶。"},
    {"强袭","出牌阶段限一次，你可以失去1点体力或弃置一张武器牌，并对你攻击范围内的一名其他角色造成1点伤害。"},
    {"驱虎","出牌阶段限一次，你可以与一名体力值大于你的角色拼点。若你赢，该角色对其攻击范围内你选择的一名角色造成1点伤害；若你没赢，该角色对你造成1点伤害。"},
    {"节命","当你受到1点伤害后，你可以令一名角色将手牌摸至X张（X为其体力上限且最多为5）。"},
    {"八阵","锁定技，当你没装备防具时，始终视为你装备着【八卦阵】。"},
    {"火计","你可以将一张红色手牌当【火攻】使用。"},
    {"看破","你可以将一张黑色手牌当【无懈可击】使用。"},
    {"连环","你可以将一张梅花手牌当【铁索连环】使用，或重铸一张梅花手牌。"},
    {"涅槃","涅槃[niè pán]，限定技，当你处于濒死状态时，你可以弃置你的区域里的所有牌，然后复原你的武将牌，摸三张牌，将体力回复至3点。"},
    {"天义","出牌阶段限一次，你可以与一名角色拼点。若你赢，直到回合结束，你可以多使用一张【杀】、使用【杀】无距离限制且可以多选择一个目标；若你没赢，本回合你不能使用【杀】。"},
    {"马术","锁定技，你计算与其他角色的距离-1。"},
    {"猛进","当你使用的【杀】被目标角色使用的【闪】抵消时，你可以弃置其一张牌。"},
    {"双雄","摸牌阶段，你可以改为进行一次判定，你获得生效后的判定牌，然后本回合你可以将与判定结果颜色不同的一张手牌当【决斗】使用。"},
    {"乱击","你可以将两张花色相同的手牌当【万箭齐发】使用。"},
    {"血裔","主公技，锁定技，你的手牌上限+X（X为其他群势力角色数的两倍）。"},
    {"琴音","弃牌阶段结束时，若你于此阶段内弃置过你的至少两张手牌，则你可以选择一项：1.令所有角色各回复1点体力；2.令所有角色各失去1点体力。"},
    {"业炎","限定技，出牌阶段，你可以选择至多三名角色，对这些角色造成共计至多3点火焰伤害（若你将对一名角色分配2点或更多火焰伤害，你须先弃置四张花色各不相同的手牌并失去3点体力）。"},
    {"七星","游戏开始时，你将牌堆顶的七张牌扣置于你的武将牌上，称为“星”，然后你可以用任意张手牌替换等量的“星”；摸牌阶段结束时，你可以用任意张手牌替换等量的“星”。"},
    {"狂风","结束阶段，你可以移去一张\"星\"并选择一名角色，然后直到你的下回合开始之前，当该角色受到火焰伤害时，此伤害+1。"},
    {"大雾","结束阶段，你可以移去任意张\"星\"并选择等量的角色，然后直到你的下回合开始之前，当这些角色受到非雷电伤害时，防止此伤害。"},
    {"行殇","行殇[shāng]，当其他角色死亡时，你可以获得其所有的牌。"},
    {"放逐","当你受到伤害后，你可以令一名其他角色翻面，然后该角色摸X张牌（X为你已损失的体力值）。"},
    {"颂威","主公技，当其他魏势力角色的黑色判定牌生效后，其可以令你摸一张牌。"},
    {"断粮","你可以将一张黑色基本牌或黑色装备牌当【兵粮寸断】使用；你可以对距离为2的角色使用【兵粮寸断】。"},
    {"祸首","锁定技，【南蛮入侵】对你无效；当其他角色使用【南蛮入侵】指定目标后，你代替其成为此牌造成的伤害的来源。"},
    {"再起","摸牌阶段，若你已受伤，你可以改为亮出牌堆顶的X张牌（X为你已损失的体力值），然后回复等同于其中红桃牌数量的体力，并获得其余的牌。"},
    {"巨象","锁定技，【南蛮入侵】对你无效；当其他角色使用的【南蛮入侵】结算结束后，你获得之。"},
    {"烈刃","当你使用【杀】对目标角色造成伤害后，你可以与其拼点，若你赢，你获得其一张牌。"},
    {"英魂","准备阶段，若你已受伤，你可以选择一名其他角色并选择一项：1.令其摸X张牌，然后弃置一张牌；2.令其摸一张牌，然后弃置X张牌。（X为你已损失的体力值）"},
    {"好施","摸牌阶段，你可以多摸两张牌，然后若你的手牌数大于5，则你将一半的手牌（向下取整）交给手牌最少的一名其他角色。"},
    {"缔盟","出牌阶段限一次，你可以选择两名其他角色并弃置X张牌（X为这两名角色手牌数的差），然后令这两名角色交换手牌。"},
    {"酒池","你可以将一张黑桃手牌当【酒】使用。"},
    {"肉林","锁定技，你对女性角色使用的【杀】、女性角色对你使用的【杀】均需使用两张【闪】才能抵消。"},
    {"崩坏","锁定技，结束阶段，若你不是体力值最小的角色，你失去1点体力或减1点体力上限。"},
    {"暴虐","主公技，当其他群势力角色造成伤害后，其可以进行判定，若结果为黑桃，你回复1点体力。"},
    {"完杀","锁定技，你的回合内，不处于濒死状态的其他角色不能使用【桃】。"},
    {"乱武","限定技，出牌阶段，你可以令所有其他角色除非对各自距离最小的另一名角色使用一张【杀】，否则失去1点体力。"},
    {"帷幕","锁定技，你不能被选择为黑色锦囊牌的目标。"},
    {"归心","当你受到1点伤害后，你可以获得每名其他角色区域里的一张牌，然后你翻面。"},
    {"飞影","锁定技，其他角色计算与你的距离+1。"},
    {"狂暴","锁定技，游戏开始时，你获得2个“暴怒”标记；你造成或受到1点伤害后，获得1个“暴怒”。"},
    {"无谋","锁定技，你的普通锦囊牌只能当普通【杀】使用或打出，此【杀】的目标改为原锦囊的目标。"},
    {"无前","出牌阶段，你可移除2个“暴怒”，令一名角色防具牌失效且你获得“无双”，直至你下次使用伤害牌但未造成伤害；你的出【杀】次数+X（X为当前被“无前”的角色数）；结束阶段，若你的手牌中没有伤害牌，你随机获得一张伤害牌。"},
    {"神愤","出牌阶段限一次，你可移除6个“暴怒”，对所有其他角色各造成1点伤害，然后这些角色先各弃置装备区里的所有牌，再各弃置四张手牌，最后你翻面。"},
    {"巧变","你可以弃置一张手牌并跳过一个阶段。若你以此法跳过摸牌阶段，你可以获得至多两名角色的各一张手牌；若你以此法跳过出牌阶段，你可以将一名角色场上的一张牌置入另一名角色区域里的相应位置。"},
    {"屯田","当你于回合外失去牌后，你可以进行判定，然后将生效后的非红桃判定牌置于你的武将牌上，称为田；你计算与其他角色的距离-X（X为田的数量）。"},
    {"凿险","觉醒技，准备阶段，若田的数量不小于3，你减1点体力上限，然后获得技能急袭（你可以将一张田当【顺手牵羊】使用）。"},
    {"急袭","你可以将一张田当【顺手牵羊】使用"},
    {"挑衅","出牌阶段限一次，你可以选择一名攻击范围内含有你的角色，然后除非该角色对你使用一张【杀】，否则你弃置其一张牌。"},
    {"志继","觉醒技，准备阶段，若你没有手牌，你回复1点体力或摸两张牌，然后减1点体力上限，获得观星（准备阶段，你可以观看牌堆顶的X张牌（X为全场角色数且最多为5），然后将其中任意数量的牌置于牌堆顶，将其余的牌置于牌堆底。）。"},
    {"享乐","锁定技，当你成为一名角色使用【杀】的目标后，除非该角色弃置一张基本牌，否则此【杀】对你无效。"},
    {"放权","你可以跳过出牌阶段，然后此回合结束时，你可以弃置一张手牌并令一名其他获得一个额外的回合。（官网原文如此）"},
    {"若愚","主公技，觉醒技，准备阶段，若你是体力值最小的角色，你加1点体力上限，回复1点体力，然后获得技能激将（主公技，当你需要使用或打出【杀】时，你可以令其他蜀势力角色选择是否打出一张【杀】（视为由你使用或打出）。）。"},
    {"激昂","当你使用【决斗】或红色【杀】指定目标后，或成为【决斗】或红色【杀】的目标后，你可以摸一张牌。"},
    {"魂姿","觉醒技，准备阶段，若你的体力值为1，你减1点体力上限，然后获得技能英姿（摸牌阶段，你可以额外摸一张牌）和英魂（准备阶段开始时，若你已受伤，你可以选择一项：1.令一名其他角色摸X张牌，然后其弃置一张牌；2.令一名其他角色摸一张牌，然后其弃置X张牌。（X为你已损失的体力值））。"},
    {"制霸","主公技，其他吴势力角色的出牌阶段限一次，该角色可以与你拼点（若你已觉醒，你可以拒绝此拼点），若其没赢，你可以获得拼点的两张牌。"},
    {"直谏","出牌阶段，你可以将手牌中的一张装备牌置于一名其他角色的装备区里，然后摸一张牌。"},
    {"固政","其他角色的弃牌阶段结束时，你可以将该角色此阶段弃置的一张手牌交给该角色，然后你可以获得其余此阶段弃置的牌。"},
    {"化身","游戏开始时，你随机获得两张武将牌作为化身牌，然后亮出其中一张。你获得亮出化身牌的一个技能，且性别和势力视为与化身牌相同。回合开始时或结束后，你可以更改亮出的化身牌。"},
    {"新生","当你受到1点伤害后，你获得一张新的化身牌。"},
    {"悲歌","当一名角色受到【杀】造成的伤害后，你可以弃置一张牌，然后令其进行判定，若结果为：红桃，其回复1点体力；方块，其摸两张牌；梅花，伤害来源弃置两张牌；黑桃，伤害来源翻面。"},
    {"断肠","锁定技，当你死亡时，杀死你的角色失去所有武将技能。"},
    {"绝境","锁定技，你的手牌上限+2；当你进入或脱离濒死状态时，你摸一张牌。"},
    {"龙魂","你可以将至多两张同花色的牌按以下规则使用或打出：红桃当【桃】；方块当火【杀】；梅花当【闪】；黑桃当【无懈可击】。若你以此法使用了两张红色牌，则此牌回复值或伤害值+1。若你以此法使用了两张黑色牌，则你弃置当前回合角色一张牌。"},
    {"忍戒","锁定技，当你受到伤害后，或于弃牌阶段内弃置手牌后，你获得X枚“忍”标记（X为伤害值或弃置的手牌数）。"},
    {"拜印","觉醒技，准备阶段开始时，若\"忍\"标记的数量不小于4，你减1点体力上限，然后获得\"极略\"(你可以弃置1枚“忍”标记，发动下列一项技能：“鬼才”、“放逐”、“集智”、“制衡”或“完杀”。)。"},
    {"极略","你可以弃置1枚“忍”标记，发动下列一项技能：“鬼才”、“放逐”、“集智”、“制衡”或“完杀”。"},
    {"连破","当你杀死任意角色后，你可于此回合结束后获得一个额外回合。"},
    {"激将","主公技，当你需要使用或打出【杀】时，你可以令其他蜀势力角色选择是否打出一张【杀】（视为由你使用或打出）。"},
};
const std::set<std::string> active = {"奇谋","强袭","火计","连环","乱击","直谏","挑衅","业炎","驱虎","天义","缔盟","乱武","急袭","无前","神愤","攻心","极略"};
const std::set<std::string> conversion = {"火计","看破","连环","酒池","断粮","龙魂","武神","双雄","蛊惑","无谋"};
const std::set<std::string> locked = {"狂骨","红颜","马术","八阵","祸首","巨象","肉林","崩坏","帷幕","享乐","断肠","绝境","飞影","忍戒","狂暴","武神","无谋","武魂","完杀","血裔"};
}
MythSkill::MythSkill(const std::string& n, const std::string& desc, SkillKind k, unsigned t,
                     bool modeFirst):Skill(n,desc,k,t),yinghunModeFirst(modeFirst) {}
void MythSkill::gainAvatar(GameEngine& e) {
    std::set<std::string> occupied;
    for (const auto& id : avatars) occupied.insert(HeroRegistry::personKey(id));
    for (const auto& player : e.getPlayers()) if (player->getHero())
        occupied.insert(HeroRegistry::personKey(player->getHero()->getId()));
    std::vector<std::string> pool;
    // 官网只称随机获得武将牌，未排除神将；DIY 设计不是官方化身牌。
    for (const auto& h : HeroRegistry::all())
        if (h.pack != "DIY包" && h.id != "zuoci" &&
            !occupied.count(HeroRegistry::personKey(h.id))) pool.push_back(h.id);
    if (!pool.empty()) avatars.push_back(pool[e.getRng()() % pool.size()]);
}
void MythSkill::changeAvatar(GameEngine& e, Player& p) {
    if (avatars.empty() || !p.getHero()) return;
    std::vector<std::string> labels;
    for (const auto& id : avatars) {
        auto h = HeroRegistry::create(id);
        labels.push_back(h ? h->getName() + "（" + id + "）" : id);
    }
    int selected = e.askChooseOption(selfOf(e,p),labels,"【化身】亮出一张化身牌",0);
    if (selected < 0 || selected >= static_cast<int>(avatars.size())) return;
    auto h = HeroRegistry::create(avatars[selected]);
    if (!h) return;
    // 移动版官网描述为“一个技能”，并未排除限定/觉醒技；但**主公技当且仅当你是主公时才拥有**
    // （用户 2026-10-05），且“只要有主公技就能用”——所以非主公的化身候选里必须剔除主公技，
    // 否则左慈会借到一个可用的主公技。
    std::vector<SkillPtr> choices;
    for (const auto& sk : h->getSkills()) {
        if (!sk) continue;
        if (sk->hasTag(SkillTag::LORD) && p.getIdentity() != Identity::ZHU_GONG) continue;
        choices.push_back(sk);
    }
    // 即使亮出的武将没有可借用技能，性别/势力仍跟随其化身。
    if (!borrowedSkill.empty()) { e.removeHeroSkill(selfOf(e,p),borrowedSkill); borrowedSkill.clear(); }
    p.getHero()->setAvatarIdentity(h->getCountry(),h->getGender());
    shownAvatar = avatars[selected];
    if (choices.empty()) return;
    std::vector<std::string> skills;
    for (const auto& skill : choices) skills.push_back(skill->getName());
    int idx = e.askChooseOption(selfOf(e,p),skills,"【化身】选择亮出的化身的一项技能",0);
    if (idx >= 0 && idx < static_cast<int>(choices.size())) {
        borrowedSkill=choices[idx]->getName();p.getHero()->addSkill(choices[idx]);
    }
}
bool MythSkill::hasEffectiveLockedComponent() const {
    // 官网【屯田】【七星】【不屈】均未标注“锁定技”，非锁定技失效时整体失效。
    return Skill::hasEffectiveLockedComponent();
}
bool MythSkill::isUsableActively() const { return active.count(name) && !conversion.count(name); }
bool MythSkill::isConversionSkill() const { return conversion.count(name); }
bool MythSkill::canActivate(GameEngine& e, Player& p) {
    // uses 只为官网“出牌阶段限一次”的技能限次；急袭/无前/极略按官网无每回合次数上限，
    // 乱击（两张同花色当万箭）、直谏（每张装备均可发动）官网同样未限次数，不得限为一次。
    if (spent || (uses && name!="急袭" && name!="无前" && name!="极略" &&
                  name!="乱击" && name!="直谏") || p.getHp() <= 0) return false;
    if (name=="奇谋") return p.getHp()>0;
    if(name=="极略")return p.getMark("忍")>0 &&
        ((uses==0 && !possessions(p).empty()) ||
         (!grantedWanSha && p.getHero() && !p.getHero()->findSkill("完杀") && !p.getHero()->findSkill("谋-完杀")));
    if(name=="攻心") {
        for(auto t:e.getOtherAlivePlayers(p))if(t->getHandCardCount()>0)return true;
        return false;
    }
    if(name=="无前")return p.getMark("暴怒")>=2;
    if(name=="神愤")return p.getMark("暴怒")>=6;
    if(name=="急袭") {
        if(p.getPileCount("田")==0)return false;
        for(auto t:e.getOtherAlivePlayers(p))
            if(e.calculateDistance(p,*t)<=1 && !t->getAllCards().empty())
                for(auto c:p.getPile("田")) {
                    auto trick=Card::makeVirtual("顺手牵羊",CardType::TRICK,CardSubType::SHUN_SHOU_QIAN_YANG,{c},name);
                    if(e.canBeTargeted(t,trick,e.getPlayerById(p.getId())))return true;
                }
        return false;
    }
    if (name=="乱武") return !e.getOtherAlivePlayers(p).empty();
    if (name=="天义") {
        if(p.getHandCards().empty())return false;
        for(auto t:e.getOtherAlivePlayers(p))if(t->getHandCardCount())return true;
        return false;
    }
    if (name=="乱击") {
        auto hand=p.getHandCards();
        for(size_t i=0;i<hand.size();i++)for(size_t j=i+1;j<hand.size();j++)
            if(e.effectiveSuit(p,hand[i])==e.effectiveSuit(p,hand[j]))return true;
        return false;
    }
    if (name=="缔盟") {
        auto others=e.getOtherAlivePlayers(p);
        for(auto a:others)for(auto b:others)
            if(a!=b && std::abs(a->getHandCardCount()-b->getHandCardCount())<=
                        static_cast<int>(possessions(p).size()))return true;
        return false;
    }
    if (name=="驱虎") {
        if (p.getHandCards().empty()) return false;
        for(auto t:e.getOtherAlivePlayers(p)) if(t->getHp()>p.getHp() && !t->getHandCards().empty()) return true;
        return false;
    }
    if (name=="业炎") return p.isAlive();
    if (name=="强袭") return !e.getPlayersInRange(p,p.getAttackRange()).empty();
    if (name=="挑衅") {
        for(auto t:e.getOtherAlivePlayers(p))
            if(e.calculateDistance(*t,p)<=t->getAttackRange())return true;
        return false;
    }
    if (name=="直谏") {
        auto hand=p.getHandCards();
        return std::any_of(hand.begin(),hand.end(),[](CardPtr c){return c->getType()==CardType::EQUIPMENT;});
    }
    return false;
}
void MythSkill::onActivate(GameEngine& e, Player& p) {
    if(!canActivate(e,p))return;
    auto me=selfOf(e,p);
    if(name=="极略") {
        auto cards=possessions(p);
        std::vector<std::string> modes;
        bool canBalance=uses==0 && !cards.empty();
        bool canSeal=!grantedWanSha && p.getHero() && !p.getHero()->findSkill("完杀") && !p.getHero()->findSkill("谋-完杀");
        if(canBalance)modes.push_back("制衡（弃忍，弃任意张牌并摸等量牌）");
        if(canSeal)modes.push_back("完杀（弃忍，至回合结束获得完杀）");
        if(modes.empty())return;
        int mode=e.askChooseOption(me,modes,"【极略】选择发动的技能",canBalance?0:static_cast<int>(modes.size())-1);
        if(mode<0 || mode>=static_cast<int>(modes.size()))return;
        if(canBalance ? mode==1 : mode==0) {
            if(!canSeal)return;
            p.addMark("忍",-1);
            p.getHero()->addSkill(std::make_shared<MythSkill>("完杀",texts.at("完杀"),SkillKind::STATE,SkillTag::LOCK));
            grantedWanSha=true;
            return;
        }
        std::vector<std::string> amounts;
        for(size_t n=1;n<=cards.size();n++)amounts.push_back(std::to_string(n)+"张牌");
        int pick=e.askChooseOption(me,amounts,"【极略·制衡】弃置多少张牌？",0);
        int count=std::max(1,std::min(static_cast<int>(cards.size()),pick+1));
        auto selected=e.chooseCards(me,cards,count,"【极略·制衡】选择弃置的牌");
        if(static_cast<int>(selected.size())!=count)return;
        p.addMark("忍",-1);
        for(auto c:selected)e.discardCardOf(me,c,name);
        e.drawCards(me,count,name);uses++;return;
    }
    if(name=="攻心") {
        std::vector<PlayerPtr> pool;
        for(auto t:e.getOtherAlivePlayers(p))if(t->getHandCardCount()>0)pool.push_back(t);
        auto t=e.askChoosePlayer(me,pool,"【攻心】选择有手牌的角色",true,pool.empty()?nullptr:pool.front());
        if(!t)return;
        uses++;
        e.viewCards(me,t->getHandCards(),"【攻心】观看"+t->getName()+"的所有手牌（仅你可见）");
        std::vector<CardPtr> hearts;
        for(auto card:t->getHandCards())if(e.effectiveSuit(*t,card)==Suit::HEART)hearts.push_back(card);
        auto shown=e.askChooseCard(me,hearts,"【攻心】可展示一张红桃手牌",true,
                                   hearts.empty()?nullptr:hearts.front());
        if(shown) {
            e.logMessage("  【攻心】展示 " + shown->getFormattedName());
            int choice=e.askChooseOption(me,{"弃置此牌","置于牌堆顶"},name,0);
            if(choice==0)e.discardCardOf(t,shown,name,me);
            else if(e.loseHandCard(t,shown))e.getDeck().putOnTop({shown});
        }
        return;
    }
    if(name=="无前") {
        auto pool=e.getAlivePlayers();
        PlayerPtr ai;
        for(auto t:pool)if(!friendOf(e, p,*t)) {ai=t;break;}
        auto t=e.askChoosePlayer(me,pool,"【无前】选择一名角色（可选自己）",true,ai);
        if(!t)return;
        p.addMark("暴怒",-2);uses=1;
        if(!p.getHero()->findSkill("无双") && !p.getHero()->findSkill("谋-无双")){
            p.getHero()->addSkill(std::make_shared<WuShuangSkill>());
            grantedWuShuang=true;
        }
        p.addMark("无前目标:"+std::to_string(t->getId()),1);
        t->addMark("无前防具失效",1);
        markedTargets.push_back(t->getId());
        return;
    }
    if(name=="神愤") {
        p.addMark("暴怒",-6);uses=1;
        auto targets=e.getOtherAlivePlayers(p);
        // 先逐个造成伤害，再统一依次弃装备、手牌，不能让首名角色先弃牌改变后续伤害响应。
        for(auto t:targets)if(t->isAlive() && !e.isGameOver())e.applyDamage(me,t,1);
        for(auto t:targets)if(t->isAlive())for(auto c:t->getAllEquipment())e.discardCardOf(t,c,name);
        for(auto t:targets)if(t->isAlive()) {
            int n=std::min(4,t->getHandCardCount());
            auto cards=e.chooseCards(t,t->getHandCards(),n,"【神愤】弃置四张手牌");
            for(auto c:cards)e.discardCardOf(t,c,name,me);
        }
        if(p.isAlive())p.setTurnedOver(!p.isTurnedOver());
        return;
    }
    if(name=="急袭") {
        auto pile=p.getPile("田");if(pile.empty())return;
        auto c=e.askChooseCard(me,pile,"【急袭】选择一张田",true,pile.front());
        if(!c)return;
        auto trick=Card::makeVirtual("顺手牵羊",CardType::TRICK,CardSubType::SHUN_SHOU_QIAN_YANG,{c},name);
        std::vector<PlayerPtr> pool;
        for(auto t:e.getOtherAlivePlayers(p))
            if(e.calculateDistance(p,*t)<=1 && !t->getAllCards().empty() &&
               e.canBeTargeted(t,trick,me))pool.push_back(t);
        auto t=e.askChoosePlayer(me,pool,"【急袭】选择合法的顺手牵羊目标",true,pool.empty()?nullptr:pool.front());
        if(!t)return;
        p.removeFromPile("田",c);
        // 只有成功使用时才将田作为锦囊消耗；合法目标消失时恢复原牌区。
        if(e.useCard(me,trick,{t}))++uses;
        else p.addToPile("田",c);
        return;
    }
    if (name=="奇谋") {
        std::vector<std::string> options;
        for(int n=1;n<=p.getHp();n++)options.push_back("失去"+std::to_string(n)+"点体力");
        options.push_back("取消");
        int pick=e.askChooseOption(me,options,"【奇谋】失去多少体力？",0);
        if(pick<0 || pick>=p.getHp())return;
        bonus=pick+1;spent=true;e.loseHp(me,bonus,"奇谋");return;
    }
    if (name=="业炎") {
        std::set<Suit> suits;
        for (auto card : p.getHandCards()) suits.insert(e.effectiveSuit(p,card));
        const bool heavy = suits.size() == 4;
        std::vector<std::pair<PlayerPtr,int>> assigned;
        std::vector<PlayerPtr> available=e.getAlivePlayers();
        int remaining=3;
        while (remaining>0 && !available.empty()) {
            PlayerPtr aiTarget;
            for(auto candidate:available)if(!friendOf(e, p,*candidate)) {aiTarget=candidate;break;}
            if(p.isAI() && !aiTarget)break;
            auto target=e.askChoosePlayer(me,available,"【业炎】分配火焰伤害（至多三点）",!assigned.empty(),aiTarget);
            if (!target) break;
            int count=1;
            if (heavy && remaining>1) {
                std::vector<std::string> options;
                for (int i=1;i<=remaining;i++) options.push_back(std::to_string(i)+"点火焰伤害");
                int selected=e.askChooseOption(me,options,"【业炎】为此目标分配伤害",0);
                if (selected>=0 && selected<static_cast<int>(options.size())) count=selected+1;
            }
            assigned.emplace_back(target,count);
            remaining-=count;
            available.erase(std::remove(available.begin(),available.end(),target),available.end());
        }
        if (assigned.empty()) return;
        bool paid=false;
        for (const auto& [target,count] : assigned) if (count>=2) paid=true;
        if (paid) {
            std::set<Suit> discarded;
            for (int i=0;i<4;i++) {
                std::vector<CardPtr> cards;
                for (auto c:p.getHandCards())if(!discarded.count(e.effectiveSuit(p,c)))cards.push_back(c);
                auto c=e.askChooseCard(me,cards,"【业炎】弃置四张花色各异的手牌",false,cards.front());
                if (!c) return;
                discarded.insert(e.effectiveSuit(p,c));e.discardCardOf(me,c,name);
            }
            e.loseHp(me,3,name);
        }
        spent=true;
        // 支付代价后若神周瑜已阵亡，则不再由其造成伤害。
        if (!me->isAlive()) return;
        for (auto [target,count]:assigned) if (target->isAlive() && !e.isGameOver())
            e.applyDamage(me,target,count,ShaElement::FIRE);
        return;
    }
    if(name=="乱击") {
        auto hand=p.getHandCards();
        std::vector<CardPtr> first;
        for(auto a:hand)for(auto b:hand)if(a!=b && e.effectiveSuit(p,a)==e.effectiveSuit(p,b)) {
            first.push_back(a);break;
        }
        auto a=e.askChooseCard(me,first,"【乱击】选择第一张手牌",true,least(first));
        if(!a)return;
        std::vector<CardPtr> second;
        for(auto b:hand)if(b!=a && e.effectiveSuit(p,a)==e.effectiveSuit(p,b))second.push_back(b);
        auto b=e.askChooseCard(me,second,"【乱击】选择同花色的第二张手牌",false,least(second));
        if(b) {
            auto aoe=Card::makeVirtual("万箭齐发",CardType::TRICK,CardSubType::WAN_JIAN_QI_FA,{a,b},name);
            if(e.useCard(me,aoe,{}))uses++;
        }
        return;
    }
    if(name=="天义") {
        std::vector<PlayerPtr> pool;
        for(auto t:e.getOtherAlivePlayers(p))if(t->getHandCardCount()>0)pool.push_back(t);
        auto t=e.askChoosePlayer(me,pool,"【天义】选择有手牌的拼点目标",true,pool.empty()?nullptr:pool.front());
        if(!t)return;
        uses++;
        if(e.pindian(me,t,name))bonus=1;
        else bonus=-999; // 拼点未赢不能再用杀
        return;
    }
    if(name=="乱武") {
        spent=true;
        for(auto t:e.getOtherAlivePlayers(p)) {
            // 官网“令所有其他角色除非……否则失去1点体力”：逐个独立结算，
            // 中途阵亡的目标跳过即可，其后的角色仍须继续结算。
            if(e.isGameOver())return;
            if(!t->isAlive())continue;
            int nearest=999;std::vector<PlayerPtr> pool;
            for(auto target:e.getOtherAlivePlayers(*t)){
                int d=e.calculateDistance(*t,*target);
                if(d<nearest){nearest=d;pool.clear();}if(d==nearest)pool.push_back(target);
            }
            auto v=e.askChoosePlayer(t,pool,"【乱武】选择最近角色对其使用杀",true,pool.empty()?nullptr:pool.front());
            auto sha=v?e.askUseSha(t,"【乱武】使用杀",!friendOf(e, *t,*v),v):nullptr;
            if(sha)e.resolveSha(t,sha,{v});else e.loseHp(t,1,name);
        }
        return;
    }
    if (name=="驱虎") {
        std::vector<PlayerPtr> pool;
        for(auto t:e.getOtherAlivePlayers(p)) if(t->getHp()>p.getHp() && !t->getHandCards().empty()) pool.push_back(t);
        auto t=e.askChoosePlayer(me,pool,"【驱虎】选择体力值较大的拼点对象",true,pool.empty()?nullptr:pool.front());
        if (!t) return;
        uses++;
        if (e.pindian(me,t,name)) {
            auto victim=e.getOtherAlivePlayers(*t);
            victim.erase(std::remove_if(victim.begin(),victim.end(),[&](PlayerPtr v){return e.calculateDistance(*t,*v)>t->getAttackRange();}),victim.end());
            auto v=e.askChoosePlayer(me,victim,"【驱虎】选择其攻击范围内的角色",false,victim.empty()?nullptr:victim.front());
            if(v)e.applyDamage(t,v,1);
        } else e.applyDamage(t,me,1);
        return;
    }
    if (name=="强袭") {
        auto pool=e.getPlayersInRange(p,p.getAttackRange());
        auto t=e.askChoosePlayer(me,pool,"【强袭】选择攻击范围内的目标",true,pool.empty()?nullptr:pool.front());
        if(!t)return;
        std::vector<CardPtr> weapons;
        for(auto c:p.getHandCards())if(c->getType()==CardType::EQUIPMENT &&
            c->getSubType()==CardSubType::WEAPON)weapons.push_back(c);
        if(p.getWeapon())weapons.push_back(p.getWeapon());
        if(!weapons.empty() && e.askConfirm(me,"【强袭】弃置一张武器牌（否则失去1点体力）？",true)) {
            auto weapon=e.askChooseCard(me,weapons,"【强袭】选择手牌或装备区里的武器",false,least(weapons));
            if(weapon)e.discardCardOf(me,weapon,name);
        }else e.loseHp(me,1,name);
        uses++; if (me->isAlive()) e.applyDamage(me,t,1); return;
    }
    if (name=="挑衅") {
        std::vector<PlayerPtr> pool;
        for(auto t:e.getOtherAlivePlayers(p))if(e.calculateDistance(*t,p)<=t->getAttackRange())pool.push_back(t);
        auto t=e.askChoosePlayer(me,pool,"【挑衅】选择攻击范围内包含你的角色",true,pool.empty()?nullptr:pool.front());
        if(!t)return;
        uses++;
        auto placeholder=Card::makeVirtual("杀",CardType::BASIC,CardSubType::SHA,{},name);
        auto sha=e.canBeTargeted(me,placeholder,t)
            ? e.askUseSha(t,"【挑衅】对"+p.getName()+"使用杀",!friendOf(e, *t,p),me) : nullptr;
        if (sha) e.resolveSha(t,sha,{me});
        else if (!t->getAllCards().empty()) {
            auto picked=e.chooseCardFromPlayer(me,t,"【挑衅】弃置其一张牌");
            if(picked)e.discardCardOf(t,picked,name,me);
        }
        return;
    }
    if(name=="缔盟") {
        auto others=e.getOtherAlivePlayers(p);
        std::vector<PlayerPtr> first;
        for(auto a:others)for(auto b:others)if(a!=b &&
           std::abs(a->getHandCardCount()-b->getHandCardCount())<=static_cast<int>(possessions(p).size())){
            first.push_back(a);break;
        }
        auto a=e.askChoosePlayer(me,first,"【缔盟】选择第一名其他角色",true,first.empty()?nullptr:first.front());
        if(!a)return;
        std::vector<PlayerPtr> second;
        for(auto b:others)if(b!=a && std::abs(a->getHandCardCount()-b->getHandCardCount())<=
           static_cast<int>(possessions(p).size()))second.push_back(b);
        auto b=e.askChoosePlayer(me,second,"【缔盟】选择第二名其他角色",true,second.empty()?nullptr:second.front());
        if(!b)return;
        int diff=std::abs(a->getHandCardCount()-b->getHandCardCount());
        auto selected=e.chooseCards(me,possessions(p),diff,"【缔盟】选择弃置的牌");
        if(static_cast<int>(selected.size())!=diff)return;
        for(auto c:selected)e.discardCardOf(me,c,name);
        e.swapHands(a,b);
        uses++;return;
    }
    if (name=="直谏") {
        std::vector<CardPtr> cards;
        for (auto c:p.getHandCards()) if (c->getType()==CardType::EQUIPMENT) cards.push_back(c);
        auto c=e.askChooseCard(me,cards,"【直谏】选择装备",true,cards.empty()?nullptr:cards.front());
        if (!c) return;
        auto t=choose(e,p,false,name); if (!t) return;
        if(!e.equipHandCard(me,t,c))return;
        e.drawCards(me,1,name); uses++; return;
    }
}
bool MythSkill::aiShouldActivate(GameEngine& e,Player& p) {
    if(name=="业炎") {
        for(auto t:e.getOtherAlivePlayers(p))if(!friendOf(e, p,*t))return true;
        return false;
    }
    return name!="奇谋" || p.getHp()>2;
}
CardPtr MythSkill::convertCard(GameEngine& e,Player& p,CardPtr c,CardSubType want) {
    if (!c || c->isVirtual()) return nullptr;
    bool hand=p.hasHandCard(c);
    auto v=[&](const char* title,CardType t,CardSubType sub,ShaElement el=ShaElement::NORMAL){return Card::makeVirtual(title,t,sub,{c},name,el);};
    if(name=="无谋" && hand && c->getType()==CardType::TRICK &&
       c->getSubType()!=CardSubType::LE_BU_SI_SHU &&
       c->getSubType()!=CardSubType::SHAN_DIAN &&
       c->getSubType()!=CardSubType::BING_LIANG_CUN_DUAN &&
       want==CardSubType::SHA) return v("杀",CardType::BASIC,want);
    if(name=="蛊惑" && hand) {
        switch(want) {
            case CardSubType::SHA:return v("杀",CardType::BASIC,want);
            case CardSubType::SHAN:return v("闪",CardType::BASIC,want);
            case CardSubType::TAO:return v("桃",CardType::BASIC,want);
            case CardSubType::JIU:return v("酒",CardType::BASIC,want);
            case CardSubType::JUE_DOU:return v("决斗",CardType::TRICK,want);
            case CardSubType::HUO_GONG:return v("火攻",CardType::TRICK,want);
            case CardSubType::GUO_HE_CHAI_QIAO:return v("过河拆桥",CardType::TRICK,want);
            case CardSubType::SHUN_SHOU_QIAN_YANG:return v("顺手牵羊",CardType::TRICK,want);
            case CardSubType::NAN_MAN_RU_QIN:return v("南蛮入侵",CardType::TRICK,want);
            case CardSubType::WAN_JIAN_QI_FA:return v("万箭齐发",CardType::TRICK,want);
            case CardSubType::WU_ZHONG_SHENG_YOU:return v("无中生有",CardType::TRICK,want);
            case CardSubType::TAO_YUAN_JIE_YI:return v("桃园结义",CardType::TRICK,want);
            case CardSubType::WU_XIE_KE_JI:return v("无懈可击",CardType::TRICK,want);
            case CardSubType::WU_GU_FENG_DENG:return v("五谷丰登",CardType::TRICK,want);
            case CardSubType::JIE_DAO_SHA_REN:return v("借刀杀人",CardType::TRICK,want);
            case CardSubType::TIE_SUO_LIAN_HUAN:return v("铁索连环",CardType::TRICK,want);
            default:break;
        }
    }
    if (name=="火计" && hand && (e.effectiveSuit(p,c)==Suit::HEART || e.effectiveSuit(p,c)==Suit::DIAMOND) && want==CardSubType::HUO_GONG) return v("火攻",CardType::TRICK,want);
    if (name=="连环" && hand && e.effectiveSuit(p,c)==Suit::CLUB && want==CardSubType::TIE_SUO_LIAN_HUAN) return v("铁索连环",CardType::TRICK,want);
    if (name=="双雄" && bonus && hand && c->getSubType()!=CardSubType::JUE_DOU && want==CardSubType::JUE_DOU &&
        (bonus==1 ? (e.effectiveSuit(p,c)==Suit::SPADE || e.effectiveSuit(p,c)==Suit::CLUB) : (e.effectiveSuit(p,c)==Suit::HEART || e.effectiveSuit(p,c)==Suit::DIAMOND))) return v("决斗",CardType::TRICK,want);
    if (name=="酒池" && hand && e.effectiveSuit(p,c)==Suit::SPADE && want==CardSubType::JIU) return v("酒",CardType::BASIC,want);
    if (name=="看破" && hand && (e.effectiveSuit(p,c)==Suit::SPADE || e.effectiveSuit(p,c)==Suit::CLUB) && want==CardSubType::WU_XIE_KE_JI) return v("无懈可击",CardType::TRICK,want);
    if (name=="断粮" && (e.effectiveSuit(p,c)==Suit::SPADE || e.effectiveSuit(p,c)==Suit::CLUB) && (c->getType()==CardType::BASIC || c->getType()==CardType::EQUIPMENT) && want==CardSubType::BING_LIANG_CUN_DUAN) return v("兵粮寸断",CardType::TRICK,want);
    if (name=="武神" && hand && e.effectiveSuit(p,c)==Suit::HEART && want==CardSubType::SHA) return v("杀",CardType::BASIC,want);
    if (name=="龙魂" && (hand || p.hasEquipment(c))) {
        auto make=[&](const char* title,CardType t,CardSubType type,ShaElement el=ShaElement::NORMAL){
            return Card::makeVirtual(title,t,type,{c},name,el);
        };
        if(e.effectiveSuit(p,c)==Suit::HEART && want==CardSubType::TAO)return make("桃",CardType::BASIC,want);
        if(e.effectiveSuit(p,c)==Suit::DIAMOND && want==CardSubType::SHA)return make("火杀",CardType::BASIC,want,ShaElement::FIRE);
        if(e.effectiveSuit(p,c)==Suit::CLUB && want==CardSubType::SHAN)return make("闪",CardType::BASIC,want);
        if(e.effectiveSuit(p,c)==Suit::SPADE && want==CardSubType::WU_XIE_KE_JI)return make("无懈可击",CardType::TRICK,want);
    }
    return nullptr;
}
std::vector<CardPtr> MythSkill::convertCards(GameEngine& e,Player& p,CardPtr c,CardSubType wanted) {
    auto primary=convertCard(e,p,c,wanted);
    if(!primary)return {};
    std::vector<CardPtr> results{primary};
    if(name=="蛊惑" && wanted==CardSubType::SHA) {
        results.push_back(Card::makeVirtual("火杀",CardType::BASIC,wanted,{c},name,ShaElement::FIRE));
        results.push_back(Card::makeVirtual("雷杀",CardType::BASIC,wanted,{c},name,ShaElement::THUNDER));
    }
    return results;
}
bool MythSkill::canDelegate(GameEngine& e,Player& owner,Player& actor) {
    if(&owner==&actor || !owner.isAlive() || !actor.isAlive() || !owner.getHero() ||
       !actor.getHero() || owner.getIdentity()!=Identity::ZHU_GONG)return false;
    if(name=="黄天") {
        if(actor.getHero()->getCountry()!=Country::QUN || actor.getMark("黄天已献")>0)return false;
        for(auto c:actor.getHandCards())if(c->getSubType()==CardSubType::SHAN ||
           c->getSubType()==CardSubType::SHAN_DIAN)return true;
    }
    if(name=="制霸")return actor.getHero()->getCountry()==Country::WU &&
        actor.getMark("制霸已挑战")==0 && actor.getHandCardCount()>0 && owner.getHandCardCount()>0;
    (void)e;return false;
}
void MythSkill::invokeDelegated(GameEngine& e,Player& owner,Player& actor) {
    if(!canDelegate(e,owner,actor))return;
    auto giver=selfOf(e,actor),lord=selfOf(e,owner);
    if(name=="黄天") {
        std::vector<CardPtr> available;
        for(auto c:actor.getHandCards())if(c->getSubType()==CardSubType::SHAN ||
            c->getSubType()==CardSubType::SHAN_DIAN)available.push_back(c);
        auto chosen=e.askChooseCard(giver,available,"【黄天】交给张角一张闪或闪电",true,least(available));
        if(chosen){actor.addMark("黄天已献",1);e.obtainCard(lord,chosen,giver);}
    }else if(name=="制霸") {
        actor.addMark("制霸已挑战",1);
        if(owner.getMark("魂姿已觉醒")>0 &&
           !e.askConfirm(lord,"【制霸】已觉醒：是否接受拼点？",true))return;
        std::vector<CardPtr> shown;
        bool won=e.pindian(giver,lord,name,&shown);
        if(!won && e.askConfirm(lord,"【制霸】获得双方拼点牌？",true))
            for(auto card:shown)if(e.getDeck().removeDiscardCard(card))e.obtainCard(lord,card);
    }
}
void MythSkill::onTurnStart(GameEngine& e,Player& p) {
    if(name=="化身" && e.isPlayerTurn(p) && confirm(e,p,name,false)) changeAvatar(e,p);
    if(name=="连破")bonus=0;
}
void MythSkill::onTurnEnd(GameEngine& e,Player& p,Player& turnOwner) {
    if(name=="化身" && &p==&turnOwner && confirm(e,p,name,false)) changeAvatar(e,p);
    if(name=="放权" && &p==&turnOwner && uses==1 && p.isAlive() && !p.getHandCards().empty()) {
        // 普通刘禅官网：回合结束时（不是结束阶段开始时）支付一张手牌。
        if(!e.getOtherAlivePlayers(p).empty() && cost(e,p,name,true)) {
            auto target=choose(e,p,false,name,false);
            if(target)e.scheduleExtraTurn(target);
        }
    }
    if(name=="连破" && bonus>0) {
        // 官网“你可于此回合结束后获得一个额外回合”：报价窗口仅限击杀发生的该回合
        // 结束时；无论接受还是放弃都在此刻消耗计数，不顺延到之后的每个回合结束。
        int grants=bonus; bonus=0;
        for(int i=0;i<grants && p.isAlive();++i)
            if(confirm(e,p,name))e.scheduleExtraTurn(selfOf(e,p));
    }
}
void MythSkill::onTurnBoundary(GameEngine& e,Player& p,Player& turnOwner,bool starting) {
    if(&p!=&turnOwner)return;
    if(starting && (name=="狂风" || name=="大雾"))onRemoved(e,p);
    if(!starting && name=="极略" && grantedWanSha) {
        e.removeHeroSkill(selfOf(e,p),"完杀");
        grantedWanSha=false;
    }
}
void MythSkill::onPhaseStart(GameEngine& e, Player& p, TurnPhase phase,bool& skipPhase) {
    auto me=selfOf(e,p);
    if(name=="据守" && phase==TurnPhase::FINISH && confirm(e,p,name)) {
        e.drawCards(me,3,name);
        if(p.isAlive())p.setTurnedOver(!p.isTurnedOver());
    }
    if(name=="巧变" && phase!=TurnPhase::NONE && !skipPhase && p.getHandCardCount()>0 &&
       confirm(e,p,name,false)) {
        if(!cost(e,p,name,true))return;
        skipPhase=true;
        if(phase==TurnPhase::DRAW) {
            std::vector<PlayerPtr> pool;
            // “至多两名角色”未限定其他角色；自己也可以被选择（此时获得自己的手牌）。
            for(auto t:e.getAlivePlayers())if(t->getHandCardCount()>0)pool.push_back(t);
            for(int i=0;i<2 && !pool.empty();i++) {
                PlayerPtr aiTarget;
                for(auto candidate:pool)if(candidate->getId()!=p.getId() && friendOf(e, p,*candidate)) {
                    aiTarget=candidate;break;
                }
                if(!aiTarget)for(auto candidate:pool)if(candidate->getId()!=p.getId()) {
                    aiTarget=candidate;break;
                }
                if(!aiTarget)aiTarget=pool.front();
                auto t=e.askChoosePlayer(me,pool,"【巧变】选择要获取一张手牌的角色",true,aiTarget);
                if(!t)break;
                auto picked=e.chooseHiddenHandCard(me,t,"【巧变】选择其一张隐藏手牌");
                if(picked)e.obtainCard(me,picked,t);
                pool.erase(std::remove(pool.begin(),pool.end(),t),pool.end());
            }
        }else if(phase==TurnPhase::PLAY) {
            std::vector<PlayerPtr> owners;
            for(auto from:e.getAlivePlayers()) {
                bool legal=false;
                auto field=from->getAllEquipment();
                for(auto c:from->getJudgeZone())field.push_back(c);
                for(auto c:field)for(auto to:e.getAlivePlayers())
                    if(e.canMoveFieldCard(from,to,c))legal=true;
                if(legal)owners.push_back(from);
            }
            auto from=e.askChoosePlayer(me,owners,"【巧变】选择场上牌的来源",true,
                owners.empty()?nullptr:owners.front());
            if(from) {
                std::vector<CardPtr> cards,all=from->getAllEquipment();
                for(auto c:from->getJudgeZone())all.push_back(c);
                for(auto c:all)for(auto to:e.getAlivePlayers())if(e.canMoveFieldCard(from,to,c)){
                    cards.push_back(c);break;
                }
                auto c=e.askChooseCard(me,cards,"【巧变】选择要移动的牌",true,least(cards));
                if(c){
                    std::vector<PlayerPtr> dests;
                    for(auto t:e.getAlivePlayers())if(e.canMoveFieldCard(from,t,c))dests.push_back(t);
                    auto to=e.askChoosePlayer(me,dests,"【巧变】选择迁移目的地",false,
                                              dests.empty()?nullptr:dests.front());
                    if(to)e.moveFieldCard(from,to,c);
                }
            }
        }
    }
    if(phase==TurnPhase::PLAY && name=="放权" && !skipPhase &&
       e.askConfirm(me,"【放权】跳过出牌阶段，回合结束时可弃置一张手牌令一名其他角色获得额外回合？",false)) {
        uses=1; // 标记本回合已跳过出牌阶段
        // 回合结束时再支付费用并调度额外回合，避免出牌阶段内递归。
        skipPhase = true;
    }
    if (phase==TurnPhase::PREPARATION) {
        if(name=="魂姿" && !spent && p.getHp()==1) {
            spent=true;p.addMark("魂姿已觉醒",1);p.changeMaxHp(-1);p.getHero()->addSkill(std::make_shared<YingZiSkill>());
            // 孙策【魂姿】授予的英魂官网写“准备阶段开始时”；勿将孙坚的
            // “准备阶段”说明直接共享给这一版本，更不能回溯本次阶段开始。
            p.getHero()->addSkill(std::make_shared<MythSkill>("英魂",
                "准备阶段开始时，若你已受伤，你可以选择一项：1.令一名其他角色摸X张牌，然后其弃置一张牌；2.令一名其他角色摸一张牌，然后其弃置X张牌。（X为你已损失的体力值）",
                SkillKind::TRIGGER,SkillTag::NONE,true));
            e.logMessage("【魂姿】觉醒，获得【英姿】【英魂】");
        }
        if(name=="若愚" && !spent) {
            bool lowest=true;for(auto t:e.getOtherAlivePlayers(p))if(t->getHp()<p.getHp())lowest=false;
            if(lowest){spent=true;p.changeMaxHp(1);e.recoverHp(me,1,name);
                p.getHero()->addSkill(std::make_shared<JiJiangSkill>());
                e.logMessage("【若愚】觉醒，获得【激将】");}
        }
        if(name=="凿险" && !spent && p.getPileCount("田")>=3) {
            spent=true;p.changeMaxHp(-1);p.getHero()->addSkill(std::make_shared<MythSkill>("急袭",texts.at("急袭"),SkillKind::ACTIVE));
            e.logMessage("【凿险】觉醒，获得【急袭】");
        }
        if(name=="拜印" && !spent && p.getMark("忍")>=4) {
            spent=true;p.changeMaxHp(-1);
            p.getHero()->addSkill(std::make_shared<MythSkill>("极略",texts.at("极略"),SkillKind::ACTIVE));
            e.logMessage("【拜印】觉醒：体力上限-1，获得【极略】");
        }
        if(name=="志继" && !spent && p.getHandCardCount()==0) {
            spent=true;
            int option=e.askChooseOption(me,{"回复1点体力","摸两张牌"},"【志继】选择觉醒效果",p.isWounded()?0:1);
            if(option==0)e.recoverHp(me,1,name);else e.drawCards(me,2,name);
            p.changeMaxHp(-1);
            p.getHero()->addSkill(std::make_shared<GuanXingSkill>());
        }
        if(name=="英魂" && p.isWounded()) {
            int x=p.getMaxHp()-p.getHp();
            const std::vector<std::string> options{"摸X张弃1张","摸1张弃X张"};
            // 孙策【魂姿】所得版本：官网先选一项，再指定执行该项的角色；
            // 孙坚版本则先选择目标，后选择结算方式。
            bool sunce=yinghunModeFirst;
            bool friendly=false;
            for(auto other:e.getOtherAlivePlayers(p))if(friendOf(e, p,*other)){friendly=true;break;}
            int opt=sunce ? e.askChooseOption(me,options,"【英魂】选择结算方式",friendly?0:1) : -1;
            auto t=choose(e,p,false,name);
            if(t) {
                if(!sunce)opt=e.askChooseOption(me,options,"【英魂】选择结算方式",friendOf(e, p,*t)?0:1);
                e.drawCards(t,opt==0?x:1,name);
                int count=std::min(static_cast<int>(possessions(*t).size()),opt==0?1:x);
                auto cards=e.chooseCards(t,possessions(*t),count,"【英魂】选择弃置手牌");
                for(auto c:cards)e.discardCardOf(t,c,name,me);
            }
        }
    }
}
void MythSkill::onPhaseEnd(GameEngine& e,Player& p,TurnPhase ph) {
    if(name=="无前" && ph==TurnPhase::FINISH) {
        auto isDamage=[](CardPtr card){
            if(!card)return false;
            auto sub=card->getSubType();
            return sub==CardSubType::SHA || sub==CardSubType::JUE_DOU ||
                   sub==CardSubType::HUO_GONG || sub==CardSubType::NAN_MAN_RU_QIN ||
                   sub==CardSubType::WAN_JIAN_QI_FA || sub==CardSubType::SHAN_DIAN;
        };
        auto hand=p.getHandCards();
        if(std::none_of(hand.begin(),hand.end(),isDamage)) {
            auto card=e.getDeck().drawRandomMatching(isDamage,e.getRng());
            if(card)p.addHandCard(card);
        }
    }
    auto me=selfOf(e,p);
    if(name=="好施" && ph==TurnPhase::DRAW && bonus==1 && p.getHandCardCount()>5) {
        auto others=e.getOtherAlivePlayers(p);
        if(others.empty())return;
        int few=others.front()->getHandCardCount();for(auto& t:others)few=std::min(few,t->getHandCardCount());
        std::vector<PlayerPtr> pool;for(auto& t:others)if(t->getHandCardCount()==few)pool.push_back(t);
        auto t=e.askChoosePlayer(me,pool,"【好施】选择手牌最少的角色",false,pool.front());
        if(t)for(int i=0,n=p.getHandCardCount()/2;i<n;i++) {
            auto cards=p.getHandCards();auto c=e.askChooseCard(me,cards,"【好施】交给目标一张手牌",false,least(cards));
            if(c)e.obtainCard(t,c,me);
        }
    }
    if (name=="七星" && ph==TurnPhase::DRAW && !p.isNonLockSkillsDisabled() &&
        p.getPileCount("星")>0 && p.getHandCardCount()>0) {
        auto handOrder=p.getHandCards(),starOrder=p.getPile("星");
        std::sort(handOrder.begin(),handOrder.end(),[&](CardPtr a,CardPtr b){return AIController::cardValue(e,p,a)<AIController::cardValue(e,p,b);});
        std::sort(starOrder.begin(),starOrder.end(),[&](CardPtr a,CardPtr b){return AIController::cardValue(e,p,a)>AIController::cardValue(e,p,b);});
        int maxCount=static_cast<int>(std::min(handOrder.size(),starOrder.size())),beneficial=0;
        while(beneficial<maxCount && AIController::cardValue(e,p,starOrder[beneficial])>
            AIController::cardValue(e,p,handOrder[beneficial]))++beneficial;
        if (!confirm(e,p,name,beneficial>0))return;
        std::vector<std::string> options;
        for(int i=1;i<=maxCount;i++)options.push_back(std::to_string(i)+"张");
        int pick=e.askChooseOption(me,options,"【七星】选择交换数量",std::max(0,beneficial-1));
        if(pick>=0 && pick<maxCount) {
            std::vector<CardPtr> hand,stars;
            for(int i=0;i<=pick;i++) {
                auto cards=p.getHandCards();
                for(auto c:hand)cards.erase(std::remove(cards.begin(),cards.end(),c),cards.end());
                hand.push_back(e.askChooseCard(me,cards,"【七星】选择要换出的手牌",false,AIController::chooseLeastValuableCard(cards)));
            }
            for(int i=0;i<=pick;i++) {
                auto cards=p.getPile("星");
                for(auto c:stars)cards.erase(std::remove(cards.begin(),cards.end(),c),cards.end());
                stars.push_back(e.askChooseCard(me,cards,"【七星】选择要换入的星",false,AIController::chooseMostValuableCard(e,p,cards)));
            }
            for(auto c:hand){e.loseHandCard(me,c);p.addToPile("星",c);}
            for(auto c:stars){p.removeFromPile("星",c);p.addHandCard(c);}
        }
    }
    auto otherPlayers=e.getOtherAlivePlayers(p);
    bool enemyAvailable=std::any_of(otherPlayers.begin(),otherPlayers.end(),
        [&](const PlayerPtr& t){return !friendOf(e, p,*t);});
    if((name=="狂风" || name=="大雾") && ph==TurnPhase::FINISH && p.getPileCount("星")>0 &&
       confirm(e,p,name,name=="大雾" || enemyAvailable)) {
        auto pool=e.getAlivePlayers();
        int maxCount=name=="狂风"?1:std::min(p.getPileCount("星"),static_cast<int>(pool.size()));
        if(maxCount<=0)return;
        int count=1;
        if(maxCount>1) {
            std::vector<std::string> options;
            for(int i=1;i<=maxCount;i++)options.push_back(std::to_string(i)+"名角色");
            int selected=e.askChooseOption(me,options,"【大雾】弃置多少张星？",0);
            if(selected>=0 && selected<maxCount)count=selected+1;
        }
        std::vector<PlayerPtr> targets;
        for(int i=0;i<count;i++) {
            PlayerPtr ai=pool.front();
            for(auto candidate:pool)if(friendOf(e, p,*candidate)==(name=="大雾")) {ai=candidate;break;}
            auto t=e.askChoosePlayer(me,pool,"【"+name+"】指定角色",false,ai);
            if(!t)break;
            targets.push_back(t);
            pool.erase(std::remove(pool.begin(),pool.end(),t),pool.end());
        }
        for(auto t:targets) {
            auto star=p.getPile("星").front();p.removeFromPile("星",star);e.getDeck().discardCard(star);
            t->addMark(name=="狂风"?"风":"雾",1);markedTargets.push_back(t->getId());
        }
    }
    if(name=="琴音" && ph==TurnPhase::DISCARD && uses>=2 && confirm(e,p,name)) {
        int option=e.askChooseOption(me,{"所有角色各回复1点体力","所有角色各失去1点体力"},name,0);
        for(auto t:e.getAlivePlayers()) {
            if(option==0)e.recoverHp(t,1,name);
            else e.loseHp(t,1,name);
        }
    }
    if(name=="崩坏" && ph==TurnPhase::FINISH) {
        bool someoneLower=false;
        for(auto t:e.getOtherAlivePlayers(p))if(t->getHp()<p.getHp()){someoneLower=true;break;}
        if(!someoneLower)return;
        if(e.askChooseOption(me,{"失去1点体力","减1点体力上限"},name,0)==0) e.loseHp(me,1,name);
        else {p.changeMaxHp(-1);if(p.getHp()<=0)e.processDying(me,nullptr);}
    }
}
void MythSkill::onPhaseSkipped(GameEngine& e,Player& p,Player& owner,TurnPhase phase) {
    if(name=="截辎" && &p!=&owner && phase==TurnPhase::DRAW)e.drawCards(selfOf(e,p),1,name);
}
void MythSkill::onDrawCards(GameEngine& e,Player& p,int& n) {
    if(name=="好施") {
        // 每次摸牌阶段重新选择；上回合发动过不能使本回合拒绝发动后仍被迫分牌。
        bonus=0;
        if(confirm(e,p,name)){n+=2;bonus=1;}
    }
    if(name=="涉猎" && confirm(e,p,name)) {
        n=0;
        auto shown=e.getDeck().drawCards(5);
        std::set<Suit> suits;
        for(auto c:shown)suits.insert(e.effectiveSuit(p,c));
        for(auto suit:suits) {
            std::vector<CardPtr> candidates;
            for(auto c:shown)if(e.effectiveSuit(p,c)==suit)candidates.push_back(c);
            auto choice=e.askChooseCard(selfOf(e,p),candidates,"【涉猎】选择一张此花色的牌",false,
                                        AIController::chooseMostValuableCard(e,p,candidates));
            if(choice){p.addHandCard(choice);shown.erase(std::remove(shown.begin(),shown.end(),choice),shown.end());}
        }
        e.getDeck().discardCards(shown);
    }
    if(name=="双雄" && confirm(e,p,name)) {
        n=0;bool claimed=false;
        auto c=e.doJudgement(selfOf(e,p),name,false,&claimed);
        if(c){bonus=(e.effectiveSuit(p,c)==Suit::HEART || e.effectiveSuit(p,c)==Suit::DIAMOND)?1:2;
              if(!claimed)e.obtainCard(selfOf(e,p),c);}
    }
    if(name=="再起" && p.isWounded() && confirm(e,p,name)) {
        n=0;
        int lost=p.getMaxHp()-p.getHp(),hearts=0;
        auto cards=e.getDeck().drawCards(lost);
        std::vector<CardPtr> nonHearts;
        std::string shown;
        for(auto c:cards) {
            shown += c->getFormattedName()+" ";
            if(e.effectiveSuit(p,c)==Suit::HEART) { ++hearts; e.getDeck().discardCard(c); }
            else nonHearts.push_back(c);
        }
        e.logMessage("  【再起】亮出："+shown);
        e.recoverHp(selfOf(e,p),hearts,name);
        for(auto c:nonHearts)e.obtainCard(selfOf(e,p),c);
    }
}
void MythSkill::onCalculateDistance(GameEngine&,const Player& p,const Player& t,int& d) {
    if(name=="马术" || name=="奇谋" || name=="屯田")
        d=std::max(1,d-(name=="马术"?1:name=="屯田"?p.getPileCount("田"):bonus));
    (void)p; (void)t; // 飞影由目标侧引擎距离结算
}
void MythSkill::onCalculateShaLimit(GameEngine& e,const Player&,int& n) {
    if(name=="奇谋") n+=bonus;
    if(name=="无前") {
        // X 是“当前被无前的角色数”；阵亡角色不再计算，重复指定也只计一次。
        std::set<int> current;
        for(int id:markedTargets)if(auto target=e.getPlayerById(id))
            if(target->isAlive() && target->getMark("无前防具失效")>0)current.insert(id);
        n+=static_cast<int>(current.size());
    }
    if(name=="天义") n=std::max(0,n+(bonus>0?1:bonus));
}
void MythSkill::onCalculateShaTargets(GameEngine&,const Player&,CardPtr,int& count) {
    if(name=="天义" && bonus>0)count++;
}
void MythSkill::onCheckShaTarget(GameEngine&,const Player&,const Player&,CardPtr sha,bool& allowed) {
    if(name=="天义" && bonus>0)allowed=true;
    if(name=="武神" && sha && sha->getSkillSource()=="武神" && sha->getSuit()==Suit::HEART)allowed=true;
}
void MythSkill::onDuelTargeted(GameEngine& e,Player& p,Player&,Player&) {
    if(name=="激昂" && confirm(e,p,name))e.drawCards(selfOf(e,p),1,name);
}
void MythSkill::onCalculateHandLimit(GameEngine& e,const Player& p,int& n) {
    if(name=="绝境") n+=2;
    if(name=="血裔") for(auto t:e.getOtherAlivePlayers(p)) if(t->getHero() && t->getHero()->getCountry()==Country::QUN)n+=2;
}
void MythSkill::onCheckCardTarget(GameEngine&,const Player&,const Player&,CardPtr c,bool& ok) {
    if(name=="帷幕" && c && c->getType()==CardType::TRICK && c->isBlack()) ok=false;
}
void MythSkill::onCheckCardEffect(GameEngine&,const Player&,CardPtr c,bool& effective) {
    if((name=="祸首"||name=="巨象") && c && c->getSubType()==CardSubType::NAN_MAN_RU_QIN)
        effective=false;
}
void MythSkill::onCalculateResponseCount(GameEngine&,const Player& p,const Player& responder,CardSubType wanted,int& count) {
    if(name=="肉林" && wanted==CardSubType::SHAN && p.getHero() && responder.getHero() &&
       (p.getHero()->getGender()==Gender::FEMALE || responder.getHero()->getGender()==Gender::FEMALE)) count=std::max(count,2);
}
void MythSkill::onShaTargeted(GameEngine& e,Player& p,ShaContext& ctx) {
    if(name=="激昂" && ctx.card && ctx.source &&
       (ctx.source.get()==&p || ctx.target.get()==&p) &&
       (e.effectiveSuit(*ctx.source,ctx.card)==Suit::HEART ||
        e.effectiveSuit(*ctx.source,ctx.card)==Suit::DIAMOND) && confirm(e,p,name))
        e.drawCards(selfOf(e,p),1,name);
    if(name=="享乐" && ctx.target.get()==&p && ctx.source && ctx.source!=ctx.target) {
        std::vector<CardPtr> basics;
        for(auto c:ctx.source->getHandCards()) if(c->getType()==CardType::BASIC) basics.push_back(c);
        CardPtr c=e.askChooseCard(ctx.source,basics,"【享乐】弃置基本牌，否则杀无效",true,least(basics));
        if(c)e.discardCardOf(ctx.source,c,name,selfOf(e,p));else ctx.invalidTarget=true;
    }
}
void MythSkill::onShaFinished(GameEngine& e,Player& p,ShaContext& ctx) {
    if(name!="猛进" || ctx.source.get()!=&p || !ctx.dodgedByShan ||
       !ctx.target || !ctx.target->isAlive() || ctx.target->getAllCards().empty() ||
       !confirm(e,p,name))return;
    auto selected=e.chooseCardFromPlayer(selfOf(e,p),ctx.target,"【猛进】弃置目标一张牌");
    if(selected)e.discardCardOf(ctx.target,selected,name,selfOf(e,p));
}
void MythSkill::onTakeDamage(GameEngine& e,Player& p,Player* src,int& dmg,ShaElement element) {
    onTakeDamageFromCard(e,p,src,dmg,element,nullptr,nullptr);
}
void MythSkill::onTakeDamageFromCard(GameEngine& e,Player& p,Player* src,int& dmg,
                                     ShaElement element,CardPtr cause,bool* claimed) {
    if(name!="天香" || dmg<=0)return;
    std::vector<CardPtr> hearts;
    for(auto c:p.getHandCards())if(e.effectiveSuit(p,c)==Suit::HEART)hearts.push_back(c);
    if(hearts.empty())return;
    auto t=choose(e,p,true,name);if(!t)return;
    auto c=e.askChooseCard(selfOf(e,p),hearts,"【天香】弃置一张红桃手牌",true,least(hearts));
    if(!c)return;
    e.discardCardOf(selfOf(e,p),c,name);
    int transferred=dmg;
    dmg=0;
    e.applyDamage(src?e.getPlayerById(src->getId()):nullptr,t,transferred,element,false,cause,claimed,true);
    if(t->isAlive())e.drawCards(t,std::max(0,t->getMaxHp()-t->getHp()),name);
}
void MythSkill::onAfterDamage(GameEngine& e,Player& p,Player*,int dmg,ShaElement,CardPtr cause) {
    auto me=selfOf(e,p);
    if(name=="极略" && p.getMark("忍")>0 && confirm(e,p,"极略·放逐")) {
        auto t=choose(e,p,false,name);
        if(t){p.addMark("忍",-1);t->setTurnedOver(!t->isTurnedOver());e.drawCards(t,p.getMaxHp()-p.getHp(),name);}
    }
    if(name=="放逐") {
        auto t=choose(e,p,false,name); if(t){t->setTurnedOver(!t->isTurnedOver());e.drawCards(t,p.getMaxHp()-p.getHp(),name);}
    }
    if(name=="节命") for(int i=0;i<dmg;i++) if(confirm(e,p,name)) {
        auto pool=e.getAlivePlayers();
        // B3：节命＝令一名角色将手牌补至 X 张 → 选“最缺牌”的那个（自己或队友），不再固定给自己
        PlayerPtr aiPick = selfOf(e,p);
        {
            int bestNeed = -1;
            for (const auto& cand : pool) {
                if (!cand) continue;
                if (cand->getId()!=p.getId() && !friendOf(e, p, *cand)) continue; // 不资敌
                PlayerPtr cp = selfOf(e,*cand);
                int limit = cp ? e.calculateHandLimit(cp) : cand->getHandCardCount();
                int need = std::max(0, limit - cand->getHandCardCount());
                if (need > bestNeed) { bestNeed = need; aiPick = cand; }
            }
        }
        auto t=e.askChoosePlayer(selfOf(e,p),pool,"【节命】选择一名角色（可以是自己）",false,aiPick);
        if(t)e.drawCards(t,std::max(0,std::min(5,t->getMaxHp())-t->getHandCardCount()),name);
    }
    if(name=="归心") for(int i=0;i<dmg;i++) if(confirm(e,p,name)) {
        for(auto t:e.getOtherAlivePlayers(p)) {
            auto picked=e.chooseCardFromPlayer(me,t,"【归心】选择要获得的一张牌",true);
            if(picked)e.obtainCard(me,picked,t);
        }
        p.setTurnedOver(!p.isTurnedOver());
    }
    if(name=="新生") {
        auto avatar=std::dynamic_pointer_cast<MythSkill>(p.getHero()->findSkill("化身"));
        if(avatar)for(int i=0;i<dmg;i++)avatar->gainAvatar(e);
    }
    if(name=="忍戒") p.addMark("忍",dmg);
    if(name=="狂暴")p.addMark("暴怒",dmg);
    if(name=="悲歌" && cause && cause->getSubType()==CardSubType::SHA) { /* 被伤害者本人触发在此，非本人悲歌在广播钩子中 */ }
}
void MythSkill::onDamageApplied(GameEngine& e,Player& p,Player* source,Player& target,int damage,CardPtr) {
    if(name=="武魂" && &p==&target && source && source!=&p)
        source->addMark("魇",damage);
    if(name=="狂骨" && source==&p) {
        recentDamageTargetId=target.getId();
        recentDamageInRange=e.calculateDistance(p,target)<=1;
    }
}
void MythSkill::onAfterDealDamage(GameEngine& e,Player& p,Player* t,int dmg,ShaElement,CardPtr cause) {
    if(name=="无前" && dmg>0)
        for(auto& [card,hit]:damageCardsInFlight)if(card==cause)hit=true;
    if(name=="狂骨" && t && t->getId()==recentDamageTargetId && recentDamageInRange)
        for(int i=0;i<dmg && p.isAlive();i++)e.recoverHp(selfOf(e,p),1,name);
    if(name=="烈刃" && t && cause && cause->getSubType()==CardSubType::SHA &&
        p.getHandCardCount()>0 && t->getHandCardCount()>0 && confirm(e,p,name)) {
        auto target=e.getPlayerById(t->getId());
        if(e.pindian(selfOf(e,p),target,name) && !target->getAllCards().empty()) {
            auto picked=e.chooseCardFromPlayer(selfOf(e,p),target,"【烈刃】获得目标一张牌",true);
            if(picked)e.obtainCard(selfOf(e,p),picked,target);
        }
    }
    if(name=="狂暴") p.addMark("暴怒",dmg);
}
void MythSkill::onGlobalDamage(GameEngine& e,Player& p,Player* src,Player& target,int,CardPtr cause) {
    if(name=="暴虐" && src && src!=&p && src->isAlive() && src->getHero() &&
       src->getHero()->getCountry()==Country::QUN && confirm(e,*src,name)) {
        auto judge=e.getPlayerById(src->getId());
        auto c=e.doJudgement(judge,name);
        if(c && e.effectiveSuit(*judge,c)==Suit::SPADE)e.recoverHp(selfOf(e,p),1,name);
    }
    if(name=="悲歌" && target.isAlive() && cause && cause->getSubType()==CardSubType::SHA && !p.getAllCards().empty() && confirm(e,p,name)) {
        if(!cost(e,p,name))return;
        auto c=e.doJudgement(e.getPlayerById(target.getId()),name);if(!c)return;
        if(e.effectiveSuit(target,c)==Suit::HEART)e.recoverHp(e.getPlayerById(target.getId()),1,name);
        else if(e.effectiveSuit(target,c)==Suit::DIAMOND)e.drawCards(e.getPlayerById(target.getId()),2,name);
        else if(src && src->isAlive() && e.effectiveSuit(target,c)==Suit::SPADE)src->setTurnedOver(!src->isTurnedOver());
        else if(src && src->isAlive() && e.effectiveSuit(target,c)==Suit::CLUB) {
            // 梅花：伤害来源自己选择弃置两张牌；判定区的牌不能作为普通弃牌。
            auto owner=e.getPlayerById(src->getId());
            auto cards=possessions(*src);
            auto discarded=e.chooseCards(owner,cards,std::min<size_t>(2,cards.size()),"【悲歌】伤害来源弃置两张牌");
            for(auto card:discarded)e.discardCardOf(owner,card,name);
        }
    }
}
void MythSkill::onDying(GameEngine& e,Player& p,Player&) {
    if(name=="绝境")e.drawCards(selfOf(e,p),1,name);
    if(name=="不屈") {
        // 创记录体力扣减至 0 以下的每一点；不额外回复体力或改写伤害量。
        int needed=1-p.getHp()-p.getPileCount("创");
        for(int i=0;i<needed;i++) {
            auto c=e.getDeck().drawCard();if(!c)return;
            // 即使点数重复也必须先置于武将牌上；能否免死在求桃后统一检验。
            p.addToPile("创",c);
        }
        return;
    }
    if(name=="涅槃" && !spent && confirm(e,p,name)) {
        spent=true;auto me=selfOf(e,p);
        for(auto c:p.getAllCards())e.discardCardOf(me,c,name);
        p.setTurnedOver(false);p.setChained(false);
        // 官网先复原武将牌、摸三张，再回复至三点；回复使用通用入口通知回复技能。
        e.drawCards(me,3,name);
        if(p.isAlive() && p.getHp()<3)e.recoverHp(me,3-p.getHp(),name);
    }
}
void MythSkill::onAfterRecover(GameEngine& e,Player& p,int) {
    if(name!="不屈")return;
    int needed=std::max(0,1-p.getHp());
    auto wounds=p.getPile("创");
    while(static_cast<int>(wounds.size())>needed) {
        auto c=e.askChooseCard(selfOf(e,p),wounds,"【不屈】回复体力，选择移去一张创",false,wounds.back());
        if(!c)c=wounds.back();
        wounds.erase(std::remove(wounds.begin(),wounds.end(),c),wounds.end());
        p.removeFromPile("创",c);e.getDeck().discardCard(c);
    }
}
void MythSkill::onLeaveDying(GameEngine& e,Player& p) {
    if(name=="绝境")e.drawCards(selfOf(e,p),1,name);
    if(name=="不屈" && p.getHp()>0)
        for(auto c:p.getPile("创")){p.removeFromPile("创",c);e.getDeck().discardCard(c);}
}
void MythSkill::onPlayerDeath(GameEngine& e,Player& p,Player& dead,Player* killer) {
    if(name=="行殇" && &p!=&dead && confirm(e,p,name)) {
        auto from=e.getPlayerById(dead.getId());
        for(auto c:dead.getAllCards())e.obtainCard(selfOf(e,p),c,from);
    }
    if(name=="断肠" && &p==&dead && killer && killer->getHero())
        e.removeHeroSkills(e.getPlayerById(killer->getId()));
    if(name=="连破" && killer==&p)++bonus;
    if(name=="武魂" && &dead==&p) {
        auto others=e.getOtherAlivePlayers(p);
        int highest=0;
        for(auto t:others)highest=std::max(highest,t->getMark("魇"));
        if(highest<=0)return;
        std::vector<PlayerPtr> tied;
        for(auto t:others)if(t->getMark("魇")==highest)tied.push_back(t);
        auto chosen=e.askChoosePlayer(e.getPlayerById(p.getId()),tied,"【武魂】指定梦魇标记最多的角色",false,tied.front());
        if(chosen) {
            auto judge=e.doJudgement(chosen,name);
            if(judge && judge->getSubType()!=CardSubType::TAO &&
               judge->getSubType()!=CardSubType::TAO_YUAN_JIE_YI)
                e.killPlayer(chosen,e.getPlayerById(p.getId()));
        }
    }
}
void MythSkill::onRemoved(GameEngine& e,Player& p) {
    if(name=="狂风" || name=="大雾") {
        for(int id:markedTargets)if(auto target=e.getPlayerById(id))
            target->addMark(name=="狂风"?"风":"雾",-1);
        markedTargets.clear();
    }else if(name=="无前") {
        for(int id:markedTargets)if(auto target=e.getPlayerById(id)) {
            target->addMark("无前防具失效",-1);
            p.addMark("无前目标:"+std::to_string(id),-1);
        }
        markedTargets.clear();
        if(grantedWuShuang && p.getHero())e.removeHeroSkill(selfOf(e,p),"无双");
        grantedWuShuang=false;
        damageCardsInFlight.clear();
    }
}
void MythSkill::onBeforeJudge(GameEngine& e,Player& p,Player& judgeTarget,CardPtr& card) {
    if(name!="鬼道" && name!="极略")return;
    if(name=="极略" && p.getMark("忍")==0)return;
    std::vector<CardPtr> choices;
    for(auto c:p.getHandCards())if(name=="极略" || e.effectiveSuit(p,c)==Suit::SPADE || e.effectiveSuit(p,c)==Suit::CLUB)choices.push_back(c);
    if(name=="鬼道")for(auto c:p.getAllEquipment()) {
        if(e.effectiveSuit(p,c)!=Suit::SPADE && e.effectiveSuit(p,c)!=Suit::CLUB)continue;
        // 官网 FAQ：判定【八卦阵】自身时，不能用正在判定的这张装备牌去替换它。
        if(e.getJudgeReason()=="八卦阵" && c==p.getArmor() && p.getId()==judgeTarget.getId())continue;
        choices.push_back(c);
    }
    // B6：改判要有收益才改；鬼道/极略只能打黑色牌，鬼道还可用装备区的黑色牌（一并作为候选池）
    CardPtr aiPick = AIController::chooseJudgementReplacement(e, p, judgeTarget, card,
                                                             /*blackOnly=*/name == "鬼道", choices);
    CardPtr replacement;
    if (selfOf(e,p)->isAI()) {
        if (!aiPick) return;
        replacement = aiPick;
    } else {
        replacement = e.askChooseCard(selfOf(e,p), choices,
            name=="极略"?"【极略·鬼才】选择打出的手牌替换判定牌":"【鬼道】选择替换判定牌", true,
            aiPick ? aiPick : least(choices));
    }
    if(!replacement)return;
    e.consumeCard(selfOf(e,p),replacement,false);
    if(name=="极略")p.addMark("忍",-1);
    card=replacement;
}
void MythSkill::onAfterJudge(GameEngine& e,Player& p,Player& target,CardPtr c,bool&) {
    if(name=="颂威" && &p!=&target && target.getHero() && target.getHero()->getCountry()==Country::WEI &&
        c && (e.effectiveSuit(target,c)==Suit::SPADE || e.effectiveSuit(target,c)==Suit::CLUB) &&
        e.askConfirm(selfOf(e,target),"【颂威】是否令"+p.getName()+"摸一张牌？",friendOf(e, target,p)))
        e.drawCards(selfOf(e,p),1,name);
}
void MythSkill::onCardLostOutsideTurn(GameEngine& e,Player& p,CardPtr) {
    if(name=="屯田" && !p.isNonLockSkillsDisabled() &&
       confirm(e,p,name)) {
        bool claimed=false;
        auto c=e.doJudgement(selfOf(e,p),name,false,&claimed);
        if(c && !claimed){if(e.effectiveSuit(p,c)!=Suit::HEART)p.addToPile("田",c);
                          else e.getDeck().discardCard(c);}
    }
}
void MythSkill::onGameStart(GameEngine& e,Player& p) {
    if(name=="狂暴")p.addMark("暴怒",2);
    if(name=="七星") {
        for(auto c:e.getDeck().drawCards(7))p.addToPile("星",c);
        // 移动版：起始四张手牌保持不变，起始七星来自牌堆；首次换星为可选。
        onPhaseEnd(e,p,TurnPhase::DRAW);
    }
    if(name=="化身"){gainAvatar(e);gainAvatar(e);changeAvatar(e,p);}
}
void MythSkill::onUseCard(GameEngine& e,Player& p,CardPtr c) {
    if(name=="极略" && c && c->getType()==CardType::TRICK && p.getMark("忍")>0 && confirm(e,p,"极略·集智")) {
        p.addMark("忍",-1);e.drawCards(selfOf(e,p),1,"极略·集智");
    }
}
void MythSkill::onCardResolved(GameEngine& e,Player& p,CardPtr c) {
    if(name!="无前")return;
    for(auto it=damageCardsInFlight.begin();it!=damageCardsInFlight.end();++it)
        if(it->first==c){
            bool dealtDamage=it->second;
            damageCardsInFlight.erase(it);
            if(!dealtDamage)onRemoved(e,p);
            return;
        }
}
void MythSkill::onCardResponded(GameEngine& e,Player& p,CardPtr c) {
    // “打出”【杀】应对【决斗】/【南蛮入侵】不是“使用伤害牌”，不触发无前的失效计时。
    if(name!="无前")onCardPlayed(e,p,c);
}
void MythSkill::onCardPlayed(GameEngine& e,Player& p,CardPtr c) {
    if(name=="无前" && c && !markedTargets.empty() &&
       (c->getSubType()==CardSubType::SHA || c->getSubType()==CardSubType::JUE_DOU ||
        c->getSubType()==CardSubType::HUO_GONG || c->getSubType()==CardSubType::NAN_MAN_RU_QIN ||
        c->getSubType()==CardSubType::WAN_JIAN_QI_FA || c->getSubType()==CardSubType::SHAN_DIAN))
        damageCardsInFlight.emplace_back(c,false);
    if(name=="雷击" && c && c->getSubType()==CardSubType::SHAN) {
        auto pool=e.getAlivePlayers();
        PlayerPtr desired;
        for(auto candidate:pool)if(!friendOf(e, p,*candidate)){desired=candidate;break;}
        auto t=e.askChoosePlayer(selfOf(e,p),pool,"【雷击】选择判定角色",true,desired);
        if(!t)return;
        auto result=e.doJudgement(t,name);
        if(result && e.effectiveSuit(*t,result)==Suit::SPADE)e.applyDamage(selfOf(e,p),t,2,ShaElement::THUNDER);
    }
    if(name=="龙魂" && c && c->getSkillSource()=="龙魂" && c->getSubCards().size()==2 &&
       (c->getSubType()==CardSubType::SHAN || c->getSubType()==CardSubType::WU_XIE_KE_JI)) {
        auto current=e.getCurrentPlayer();
        if(current && e.isPlayerTurn(*current) && current->isAlive() && !current->getAllCards().empty()) {
            auto selected=e.chooseCardFromPlayer(selfOf(e,p),current,"【龙魂】弃置当前回合角色一张牌");
            if(selected)e.discardCardOf(current,selected,name);
        }
    }

}
void MythSkill::onDiscardedInDiscardPhase(GameEngine&,Player& p,CardPtr) {
    if(name=="琴音")uses++;
    if(name=="忍戒")p.addMark("忍",1);
}
void MythSkill::onOtherDiscardPhaseEnd(GameEngine& e,Player& p,Player& owner,const std::vector<CardPtr>& dropped) {
    if(name!="固政" || dropped.empty() || !confirm(e,p,name))return;
    auto me=selfOf(e,p), target=e.getPlayerById(owner.getId());
    auto returned=e.askChooseCard(me,dropped,"【固政】选择一张弃牌归还",false,dropped.front());
    if(!returned)return;
    if(e.getDeck().removeDiscardCard(returned))e.obtainCard(target,returned);
    if(confirm(e,p,"固政·获得其余弃牌"))
        for(auto c:dropped)if(c!=returned && e.getDeck().removeDiscardCard(c))e.obtainCard(me,c);
}
void MythSkill::onHandCardLostToOther(GameEngine& e,Player& p,Player& victim,Player&) {
    if(name=="奋激" && p.isAlive() && victim.isAlive() && confirm(e,p,name)) {
        e.loseHp(selfOf(e,p),1,name);
        if(victim.isAlive())e.drawCards(e.getPlayerById(victim.getId()),2,name);
    }
}

const std::vector<MythHeroSpec>& mythHeroes() {
    static const std::vector<MythHeroSpec> data = {
        {"xiahouyuan","夏侯渊","疾行的猎豹","风包",Country::WEI,Gender::MALE,4,"神速"},
        {"caoren","曹仁","大将军","风包",Country::WEI,Gender::MALE,4,"据守"},
        {"weiyan","魏延","嗜血的独狼","风包",Country::SHU,Gender::MALE,4,"狂骨"},
        {"xiaoqiao","小乔","矫情之花","风包",Country::WU,Gender::FEMALE,3,"天香,红颜"},
        {"zhoutai","周泰","历战之躯","风包",Country::WU,Gender::MALE,4,"不屈"},
        {"zhangjiao","张角","天公将军","风包",Country::QUN,Gender::MALE,3,"雷击,鬼道,黄天"},
        {"yuji","于吉","太平道人","风包",Country::QUN,Gender::MALE,3,"蛊惑"},
        {"shen_guanyu","神关羽","鬼神再临","风包",Country::GOD,Gender::MALE,5,"武神,武魂"},
        {"shen_lvmeng","神吕蒙","圣光之国士","风包",Country::GOD,Gender::MALE,3,"涉猎,攻心"},
        {"dianwei","典韦","古之恶来","火包",Country::WEI,Gender::MALE,4,"强袭"},
        {"xunyu","荀彧","王佐之才","火包",Country::WEI,Gender::MALE,3,"驱虎,节命"},
        {"wolong","卧龙诸葛亮","卧龙","火包",Country::SHU,Gender::MALE,3,"火计,看破,八阵"},
        {"pangtong","庞统","凤雏","火包",Country::SHU,Gender::MALE,3,"连环,涅槃"},
        {"taishici","太史慈","笃烈之士","火包",Country::WU,Gender::MALE,4,"天义"},
        {"pangde","庞德","人马一体","火包",Country::QUN,Gender::MALE,4,"马术,猛进"},
        {"yanliang_wenchou","颜良文丑","虎狼兄弟","火包",Country::QUN,Gender::MALE,4,"双雄"},
        {"yuanshao","袁绍","高贵的名门","火包",Country::QUN,Gender::MALE,4,"乱击,血裔"},
        {"shen_zhouyu","神周瑜","赤壁的火神","火包",Country::GOD,Gender::MALE,4,"琴音,业炎"},
        {"shen_zhugeliang","神诸葛亮","赤壁的妖术师","火包",Country::GOD,Gender::MALE,3,"七星,狂风,大雾"},
        {"caopi","曹丕","霸业的继承者","林包",Country::WEI,Gender::MALE,3,"行殇,放逐,颂威"},
        {"xuhuang","徐晃","周亚夫之风","林包",Country::WEI,Gender::MALE,4,"断粮"},
        {"menghuo","孟获","南蛮王","林包",Country::SHU,Gender::MALE,4,"祸首,再起"},
        {"zhurong","祝融","野性的女王","林包",Country::SHU,Gender::FEMALE,4,"巨象,烈刃"},
        {"sunjian","孙坚","武烈帝","林包",Country::WU,Gender::MALE,4,"英魂"},
        {"lusu","鲁肃","独断的外交家","林包",Country::WU,Gender::MALE,3,"好施,缔盟"},
        {"dongzhuo","董卓","魔王","林包",Country::QUN,Gender::MALE,8,"酒池,肉林,崩坏,暴虐"},
        {"jiaxu","贾诩","冷酷的毒士","林包",Country::QUN,Gender::MALE,3,"完杀,乱武,帷幕"},
        {"shen_caocao","神曹操","超世之英杰","林包",Country::GOD,Gender::MALE,3,"归心,飞影"},
        {"shen_lvbu","神吕布","修罗之道","林包",Country::GOD,Gender::MALE,5,"狂暴,无谋,无前,神愤"},
        {"zhanghe","张郃","料敌机先","山包",Country::WEI,Gender::MALE,4,"巧变"},
        {"dengai","邓艾","矫然的壮士","山包",Country::WEI,Gender::MALE,4,"屯田,凿险"},
        {"jiangwei","姜维","龙的衣钵","山包",Country::SHU,Gender::MALE,4,"挑衅,志继"},
        {"liuchan","刘禅","无为的真命主","山包",Country::SHU,Gender::MALE,3,"享乐,放权,若愚"},
        {"sunce","孙策","江东的小霸王","山包",Country::WU,Gender::MALE,4,"激昂,魂姿,制霸"},
        {"zhangzhao_zhanghong","张昭张纮","经天纬地","山包",Country::WU,Gender::MALE,3,"直谏,固政"},
        {"zuoci","左慈","迷之仙人","山包",Country::QUN,Gender::MALE,3,"化身,新生"},
        {"caiwenji","蔡文姬","异乡的孤女","山包",Country::QUN,Gender::FEMALE,3,"悲歌,断肠"},
        {"shen_zhaoyun","神赵云","神威如龙","山包",Country::GOD,Gender::MALE,2,"绝境,龙魂"},
        {"shen_simayi","神司马懿","晋国之祖","山包",Country::GOD,Gender::MALE,4,"忍戒,拜印,连破"},
    };
    return data;
}
std::vector<SkillPtr> createMythSkills(const char* names) {
    std::vector<SkillPtr> result;std::stringstream ss(names);std::string n;
    while(std::getline(ss,n,',')) {
        std::string desc = texts.count(n) ? texts.at(n) : "技能资料待核对。";
        if (partial.count(n)) desc += "（当前结算为简化版，尚未覆盖官方全部细则）";
        SkillKind k=active.count(n)||conversion.count(n)?SkillKind::ACTIVE:locked.count(n)?SkillKind::STATE:SkillKind::TRIGGER;
        unsigned tags=locked.count(n)?static_cast<unsigned>(SkillTag::LOCK):0u;
        if(n=="奇谋"||n=="涅槃"||n=="业炎"||n=="乱武")tags|=SkillTag::LIMITED;
        if(n=="拜印"||n=="志继"||n=="凿险"||n=="若愚"||n=="魂姿")tags|=SkillTag::AWAKEN;
        // 主公技标签以官网原文“主公技，……”为准：若愚（刘禅）原文即“主公技，觉醒技，……”
        if(n=="黄天"||n=="颂威"||n=="血裔"||n=="暴虐"||n=="激将"||n=="制霸"||n=="若愚")tags|=SkillTag::LORD;
        result.push_back(std::make_shared<MythSkill>(n,desc,k,tags));
    }
    return result;
}
} // namespace Thks
