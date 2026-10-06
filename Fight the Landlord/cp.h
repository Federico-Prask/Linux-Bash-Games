#ifndef DOU_DIZHU_CP_H
#define DOU_DIZHU_CP_H

#include "tiny_model.h"

// 本文件在 Card、CardPattern、Player 定义之后包含。
// AI 不直接修改游戏状态，只返回建议出的实体牌，便于测试和复用。
class ComputerAI {
    int me;
    const std::vector<Player>& players;

    using Play = std::vector<Card>;

    std::map<int,std::vector<Card>> groups() const {
        std::map<int,std::vector<Card>> g;
        for(const auto& c:players[me].hand)g[c.getValue()].push_back(c);
        return g;
    }

    // 边枚举边验牌，不保存全部组合。20张选10张共有18万余种，
    // 若先全部存下会浪费大量内存；这里把合法候选硬限制为4096个。
    static bool legalCombinationsDfs(const std::vector<Card>& hand,int need,int pos,
                                     Play& cur,const CardPattern& lastPat,
                                     std::vector<Play>& out) {
        constexpr size_t MAX_CANDIDATES=4096;
        if(out.size()>=MAX_CANDIDATES)return true;
        if((int)cur.size()==need){
            auto pat=CardPattern::checkPattern(cur);
            if(CardPattern::canBeat(pat,lastPat))out.push_back(cur);
            return out.size()>=MAX_CANDIDATES;
        }
        int left=need-(int)cur.size();
        for(int i=pos;i<=(int)hand.size()-left;++i){
            cur.push_back(hand[i]);
            bool full=legalCombinationsDfs(hand,need,i+1,cur,lastPat,out);
            cur.pop_back();
            if(full)return true;
        }
        return false;
    }

    static void addUnique(std::vector<Play>& out,Play p) {
        std::sort(p.begin(),p.end());
        for(auto q:out){std::sort(q.begin(),q.end());if(q==p)return;}
        out.push_back(std::move(p));
    }

    std::vector<Play> responseCandidates(const Play& last) const {
        std::vector<Play> result;
        auto lastPat=CardPattern::checkPattern(last);
        const auto& hand=players[me].hand;

        // 同长度组合可覆盖三带、顺子、连对、飞机及四带等全部牌型。
        // 使用流式枚举，避免在4 GB内存机器上保存十几万个临时vector。
        Play current;
        legalCombinationsDfs(hand,(int)last.size(),0,current,lastPat,result);

        // 炸弹和王炸长度不同，需要额外生成。
        auto g=groups();
        for(const auto& [v,cards]:g)
            if(cards.size()==4 && CardPattern::canBeat(CardPattern::checkPattern(cards),lastPat))
                addUnique(result,cards);
        if(g.count(16)&&g.count(17)){
            Play rocket{g.at(16)[0],g.at(17)[0]};
            if(CardPattern::canBeat(CardPattern::checkPattern(rocket),lastPat))addUnique(result,rocket);
        }
        return result;
    }

    std::vector<Play> leadCandidates() const {
        std::vector<Play> result;
        const auto& hand=players[me].hand;
        auto g=groups();

        // 若整手一次可出完，这是绝对优先候选。
        if(CardPattern::checkPattern(hand).type!=CardPattern::INVALID)addUnique(result,hand);

        // 单、对、三、炸弹。
        for(const auto& [v,c]:g){
            addUnique(result,Play{c[0]});
            if(c.size()>=2)addUnique(result,Play(c.begin(),c.begin()+2));
            if(c.size()>=3)addUnique(result,Play(c.begin(),c.begin()+3));
            if(c.size()==4)addUnique(result,c);
        }

        // 三带一、三带二。
        for(const auto& [v,c]:g)if(c.size()>=3){
            Play core(c.begin(),c.begin()+3);
            for(const auto& [w,d]:g)if(w!=v){
                Play p=core;p.push_back(d[0]);addUnique(result,p);
                if(d.size()>=2){p=core;p.insert(p.end(),d.begin(),d.begin()+2);addUnique(result,p);}
            }
        }

        // 所有顺子区间和连对区间，2与王不参与。
        for(int lo=3;lo<=14;++lo){
            Play straight,pairs;
            for(int hi=lo;hi<=14 && g.count(hi);++hi){
                straight.push_back(g[hi][0]);
                if((int)straight.size()>=5)addUnique(result,straight);
                if(g[hi].size()>=2){
                    pairs.push_back(g[hi][0]);pairs.push_back(g[hi][1]);
                    if((int)pairs.size()>=6)addUnique(result,pairs);
                } else pairs.clear();
            }
        }

        // 纯飞机。带翅膀牌型通常会在“整手可出”或跟牌组合搜索中发现。
        for(int lo=3;lo<=14;++lo){
            Play plane;
            for(int hi=lo;hi<=14 && g.count(hi)&&g[hi].size()>=3;++hi){
                plane.insert(plane.end(),g[hi].begin(),g[hi].begin()+3);
                if((int)plane.size()>=6)addUnique(result,plane);
            }
        }

        if(g.count(16)&&g.count(17))addUnique(result,Play{g[16][0],g[17][0]});
        return result;
    }

    int landlordIndex() const {
        for(int i=0;i<(int)players.size();++i)if(players[i].isLandlord)return i;
        return -1;
    }

    int score(const Play& play,bool leading) const {
        auto pat=CardPattern::checkPattern(play);
        int s=(int)play.size()*35-pat.value;
        if(play.size()==players[me].hand.size())s+=100000; // 能走完绝不保留

        int landlord=landlordIndex();
        int enemyMin=99;
        for(int i=0;i<(int)players.size();++i)
            if(players[i].isLandlord!=players[me].isLandlord)
                enemyMin=std::min(enemyMin,(int)players[i].hand.size());

        // 炸弹一般保留；对手只剩少量牌时降低保留惩罚。
        if(pat.type==CardPattern::BOMB)s-=(enemyMin<=2?40:450);
        if(pat.type==CardPattern::ROCKET)s-=(enemyMin<=2?60:600);

        // 惩罚拆炸弹、拆三张和拆对子。
        std::map<int,int> total,used;
        for(const auto& c:players[me].hand)++total[c.getValue()];
        for(const auto& c:play)++used[c.getValue()];
        for(auto [v,n]:used){
            if(total[v]==4 && n<4)s-=180;
            else if(total[v]==3 && n<3)s-=70;
            else if(total[v]==2 && n==1)s-=25;
        }

        // 先手优先甩多张组合；跟牌优先用刚好能压住的较小主牌。
        if(leading)s+=(int)play.size()*20;
        else s-=pat.value*3;
        (void)landlord;
        return s;
    }

    std::vector<float> encode(const Play& action,const Play& last,int lastPlayer) const {
        std::vector<float> x(TinyPolicyModel::INPUT,0.0f);
        for(const auto& c:players[me].hand)x[c.getValue()-3]+=0.25f;
        for(const auto& c:last)x[15+c.getValue()-3]+=0.25f;
        for(const auto& c:action)x[30+c.getValue()-3]+=0.25f;
        for(int i=0;i<3;++i)x[45+i]=players[i].hand.size()/20.0f;
        x[48]=players[me].isLandlord?1.0f:0.0f;
        x[49]=lastPlayer<0?1.0f:0.0f;
        auto p=CardPattern::checkPattern(action);
        x[50]=p.value/17.0f;x[51]=action.size()/20.0f;
        x[52]=(lastPlayer>=0 && players[lastPlayer].isLandlord==players[me].isLandlord)?1.0f:0.0f;
        if((int)p.type>=0 && (int)p.type<16)x[53+(int)p.type]=1.0f;
        int enemyMin=20;
        for(int i=0;i<3;++i)if(players[i].isLandlord!=players[me].isLandlord)
            enemyMin=std::min(enemyMin,(int)players[i].hand.size());
        x[69]=players[me].hand.size()/20.0f;x[70]=enemyMin/20.0f;x[71]=1.0f;
        return x;
    }

public:
    ComputerAI(int playerIndex,const std::vector<Player>& allPlayers)
        :me(playerIndex),players(allPlayers){}

    Play choosePlay(const Play& last,int lastPlayer) const {
        bool leading=(lastPlayer<0);
        std::vector<Play> candidates=leading?leadCandidates():responseCandidates(last);
        if(candidates.empty())return {};

        // 农民通常不压队友；但能直接出完，或地主仅剩1张时仍争夺牌权。
        if(!leading && !players[me].isLandlord && !players[lastPlayer].isLandlord){
            bool canFinish=false;
            for(const auto& p:candidates)if(p.size()==players[me].hand.size())canFinish=true;
            int landlord=landlordIndex();
            if(!canFinish && landlord>=0 && players[landlord].hand.size()>1)return {};
        }

        // 约1 MiB的CPU模型不能浪费在数千个明显较差的动作上。
        // 先用廉价规则分筛选Top-64，再交给网络精排。
        constexpr size_t MODEL_TOP_K=64;
        if(candidates.size()>MODEL_TOP_K){
            std::partial_sort(candidates.begin(),candidates.begin()+MODEL_TOP_K,candidates.end(),
                [&](const Play& a,const Play& b){return score(a,leading)>score(b,leading);});
            candidates.resize(MODEL_TOP_K);
        }

        std::vector<float> batch;
        batch.reserve(candidates.size()*TinyPolicyModel::INPUT);
        for(const auto& p:candidates){
            auto f=encode(p,last,lastPlayer);
            batch.insert(batch.end(),f.begin(),f.end());
        }
        auto& model=TinyPolicyModel::instance();
        std::vector<float> neural=model.forward(batch);

        size_t best=0;float bestScore=-1e30f;
        for(size_t i=0;i<candidates.size();++i){
            // 未加载模型时 neural 全为0，行为与原规则 AI 完全一致。
            float combined=(float)score(candidates[i],leading);
            if(model.isTrained() && i<neural.size())combined+=100.0f*neural[i];
            if(combined>bestScore){bestScore=combined;best=i;}
        }
        return candidates[best];
    }
};

#endif
