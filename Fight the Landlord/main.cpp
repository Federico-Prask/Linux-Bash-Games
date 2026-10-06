#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

class Card {
public:
    enum Suit { SPADE, HEART, CLUB, DIAMOND, JOKER };
    enum Rank { THREE = 3, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN,
                JACK, QUEEN, KING, ACE, TWO, LITTLE_JOKER, BIG_JOKER };

    Suit suit;
    Rank rank;

    Card(Suit s, Rank r) : suit(s), rank(r) {}

    int getValue() const { return static_cast<int>(rank); }

    string getName() const {
        if (rank == LITTLE_JOKER) return "小王";
        if (rank == BIG_JOKER) return "大王";
        static const array<string, 5> suits{"♠", "♥", "♣", "♦", ""};
        static const map<int, string> names{{3,"3"},{4,"4"},{5,"5"},{6,"6"},{7,"7"},
            {8,"8"},{9,"9"},{10,"10"},{11,"J"},{12,"Q"},{13,"K"},{14,"A"},{15,"2"}};
        return suits[suit] + names.at(getValue());
    }

    bool operator<(const Card& rhs) const {
        if (getValue() != rhs.getValue()) return getValue() < rhs.getValue();
        return suit < rhs.suit;
    }
    bool operator==(const Card& rhs) const { return suit == rhs.suit && rank == rhs.rank; }
};

class CardPattern {
public:
    enum PatternType {
        SINGLE, PAIR, TRIO, TRIO_WITH_SINGLE, TRIO_WITH_PAIR,
        STRAIGHT, CONSECUTIVE_PAIRS, AIRPLANE,
        AIRPLANE_WITH_SINGLES, AIRPLANE_WITH_PAIRS,
        FOUR_WITH_TWO, FOUR_WITH_TWO_PAIRS,
        BOMB, ROCKET, INVALID
    };

    PatternType type = INVALID;
    vector<Card> cards;
    int value = 0;       // 主牌最大点数
    int sequenceLen = 0; // 顺子、连对、飞机的连续组数

    CardPattern() = default;
    CardPattern(PatternType t, vector<Card> c, int v, int len = 0)
        : type(t), cards(std::move(c)), value(v), sequenceLen(len) {}

    static CardPattern checkPattern(const vector<Card>& input) {
        if (input.empty()) return {};
        vector<Card> c = input;
        sort(c.begin(), c.end());
        map<int,int> cnt;
        for (const auto& x : c) ++cnt[x.getValue()];
        const int n = static_cast<int>(c.size());

        if (n == 2 && cnt.count(16) && cnt.count(17)) return {ROCKET,c,17};
        if (n == 4 && cnt.size() == 1) return {BOMB,c,c.front().getValue()};
        if (n == 1) return {SINGLE,c,c.front().getValue()};
        if (n == 2 && cnt.size() == 1) return {PAIR,c,c.front().getValue()};
        if (n == 3 && cnt.size() == 1) return {TRIO,c,c.front().getValue()};

        auto rankWithCount = [&](int wanted) {
            for (auto [v,k] : cnt) if (k == wanted) return v;
            return 0;
        };
        if (n == 4 && cnt.size() == 2 && rankWithCount(3))
            return {TRIO_WITH_SINGLE,c,rankWithCount(3)};
        if (n == 5 && cnt.size() == 2 && rankWithCount(3) && rankWithCount(2))
            return {TRIO_WITH_PAIR,c,rankWithCount(3)};

        auto consecutiveRanks = [](const vector<int>& ranks) {
            if (ranks.empty() || ranks.back() >= 15) return false;
            for (size_t i=1;i<ranks.size();++i) if (ranks[i] != ranks[i-1]+1) return false;
            return true;
        };

        vector<int> ranks;
        for (auto [v,k] : cnt) ranks.push_back(v);
        if (n >= 5 && static_cast<int>(cnt.size()) == n && consecutiveRanks(ranks))
            return {STRAIGHT,c,ranks.back(),n};
        bool allPairs = all_of(cnt.begin(),cnt.end(),[](const auto& p){return p.second==2;});
        if (n >= 6 && n%2==0 && allPairs && consecutiveRanks(ranks))
            return {CONSECUTIVE_PAIRS,c,ranks.back(),n/2};

        // 飞机：连续的三张不能包含2和王；支持不带、带单、带对。
        vector<int> tripleRanks;
        for (auto [v,k] : cnt) if (k == 3) tripleRanks.push_back(v);
        auto findPlane = [&](int groups, PatternType t, bool pairWings) -> CardPattern {
            if (groups < 2) return {};
            for (size_t start=0; start+groups<=tripleRanks.size(); ++start) {
                vector<int> core(tripleRanks.begin()+start,tripleRanks.begin()+start+groups);
                if (!consecutiveRanks(core)) continue;
                map<int,int> rest=cnt;
                for (int v:core) rest.erase(v);
                int remaining=0; bool ok=true;
                for (auto [v,k]:rest) {
                    remaining += k;
                    if (pairWings && k!=2) ok=false;
                }
                int need = (t==AIRPLANE ? 0 : (pairWings ? groups*2 : groups));
                if (ok && remaining==need && (t!=AIRPLANE_WITH_PAIRS || (int)rest.size()==groups))
                    return {t,c,core.back(),groups};
            }
            return {};
        };
        if (n%3==0) { auto p=findPlane(n/3,AIRPLANE,false); if(p.type!=INVALID)return p; }
        if (n%4==0) { auto p=findPlane(n/4,AIRPLANE_WITH_SINGLES,false); if(p.type!=INVALID)return p; }
        if (n%5==0) { auto p=findPlane(n/5,AIRPLANE_WITH_PAIRS,true); if(p.type!=INVALID)return p; }

        if (n == 6 && rankWithCount(4)) return {FOUR_WITH_TWO,c,rankWithCount(4)};
        if (n == 8 && rankWithCount(4)) {
            int pairs=0; bool ok=true;
            for (auto [v,k]:cnt) if (k!=4) { if(k==2) ++pairs; else ok=false; }
            if (ok && pairs==2) return {FOUR_WITH_TWO_PAIRS,c,rankWithCount(4)};
        }
        return {INVALID,c,0};
    }

    static bool canBeat(const CardPattern& a, const CardPattern& b) {
        if (a.type == INVALID || b.type == INVALID) return false;
        if (a.type == ROCKET) return b.type != ROCKET;
        if (b.type == ROCKET) return false;
        if (a.type == BOMB) return b.type != BOMB || a.value > b.value;
        if (b.type == BOMB) return false;
        return a.type == b.type && a.cards.size() == b.cards.size() &&
               a.sequenceLen == b.sequenceLen && a.value > b.value;
    }

    string getPatternName() const {
        static const map<PatternType,string> names{{SINGLE,"单张"},{PAIR,"对子"},{TRIO,"三张"},
            {TRIO_WITH_SINGLE,"三带一"},{TRIO_WITH_PAIR,"三带二"},{STRAIGHT,"顺子"},
            {CONSECUTIVE_PAIRS,"连对"},{AIRPLANE,"飞机"},{AIRPLANE_WITH_SINGLES,"飞机带单"},
            {AIRPLANE_WITH_PAIRS,"飞机带对"},{FOUR_WITH_TWO,"四带二"},
            {FOUR_WITH_TWO_PAIRS,"四带两对"},{BOMB,"炸弹"},{ROCKET,"王炸"},{INVALID,"无效牌型"}};
        return names.at(type);
    }
};

class Player {
public:
    string name;
    vector<Card> hand;
    bool isLandlord=false;
    explicit Player(string n):name(std::move(n)){}
    void sortHand(){sort(hand.begin(),hand.end());}
    bool isEmpty() const{return hand.empty();}

    bool removeCards(const vector<Card>& cards) {
        vector<Card> copy=hand;
        for(const auto& c:cards){
            auto it=find(copy.begin(),copy.end(),c);
            if(it==copy.end()) return false;
            copy.erase(it);
        }
        hand=std::move(copy); sortHand(); return true;
    }
    void displayHand() const {
        cout << "你的手牌：\n";
        for(size_t i=0;i<hand.size();++i) cout << '(' << i+1 << ')' << hand[i].getName() << ' ';
        cout << '\n';
    }
};

#include "cp.h"

class DouDizhuGame {
    vector<Player> players{Player("你"),Player("电脑1"),Player("电脑2")};
    vector<Card> deck, bottomCards, lastPlayedCards, allPlayedCards;
    int currentPlayer=0;
    int lastPlayer=-1;
    int passCount=0;
    bool gameOver=false;
    bool farmerMode=false;
    mt19937 rng{random_device{}()};

public:
    DouDizhuGame(){initializeDeck();}

    void initializeDeck(){
        deck.clear();
        for(int r=Card::THREE;r<=Card::TWO;++r)
            for(int s=Card::SPADE;s<=Card::DIAMOND;++s)
                deck.emplace_back(static_cast<Card::Suit>(s),static_cast<Card::Rank>(r));
        deck.emplace_back(Card::JOKER,Card::LITTLE_JOKER);
        deck.emplace_back(Card::JOKER,Card::BIG_JOKER);
    }

    bool validateAllCards() const {
        map<pair<int,int>,int> count;
        for(const auto& p:players) for(const auto& c:p.hand) ++count[{c.suit,c.rank}];
        for(const auto& c:allPlayedCards) ++count[{c.suit,c.rank}];
        if(players[0].hand.size()+players[1].hand.size()+players[2].hand.size()+allPlayedCards.size()!=54) return false;
        for(int r=Card::THREE;r<=Card::TWO;++r)
            for(int s=Card::SPADE;s<=Card::DIAMOND;++s)
                if(count[{s,r}]!=1) return false;
        return count[{Card::JOKER,Card::LITTLE_JOKER}]==1 && count[{Card::JOKER,Card::BIG_JOKER}]==1;
    }

    void selectMode(){
        cout << "请选择模式：1.随机地主  2.固定当农民\n> ";
        string line; getline(cin,line); farmerMode=(line=="2");
    }

    void dealCards(){
        initializeDeck(); shuffle(deck.begin(),deck.end(),rng);
        for(auto& p:players){p.hand.clear();p.isLandlord=false;}
        allPlayedCards.clear(); lastPlayedCards.clear(); bottomCards.clear();
        for(int i=0;i<51;++i) players[i%3].hand.push_back(deck[i]);
        bottomCards.assign(deck.begin()+51,deck.end());
        int landlord=farmerMode?1:uniform_int_distribution<int>(0,2)(rng);
        players[landlord].isLandlord=true;
        players[landlord].hand.insert(players[landlord].hand.end(),bottomCards.begin(),bottomCards.end());
        for(auto& p:players)p.sortHand();
        currentPlayer=landlord; lastPlayer=-1; passCount=0; gameOver=false;
        cout << "地主是：" << players[landlord].name << "\n底牌：";
        for(const auto& c:bottomCards) cout << c.getName() << ' ';
        cout << "\n";
        if(!validateAllCards()) throw runtime_error("发牌后牌数校验失败");
    }

    void showState() const {
        cout << "\n========== 当前牌局 ==========\n";
        for(size_t i=0;i<players.size();++i)
            cout<<players[i].name<<(players[i].isLandlord?"[地主]":"[农民]")<<"："<<players[i].hand.size()<<"张\n";
        if(lastPlayer>=0){
            auto p=CardPattern::checkPattern(lastPlayedCards);
            cout<<"当前需压："<<players[lastPlayer].name<<" 的 ";
            for(const auto& c:lastPlayedCards)cout<<c.getName()<<' ';
            cout<<'['<<p.getPatternName()<<"]\n";
        } else cout<<"新一轮，必须出牌。\n";
        cout<<"==============================\n";
    }

    void recordPlay(int who,const vector<Card>& play){
        if(!players[who].removeCards(play)) throw runtime_error("出牌不在手牌中");
        allPlayedCards.insert(allPlayedCards.end(),play.begin(),play.end());
        lastPlayedCards=play; lastPlayer=who; passCount=0;
        auto pat=CardPattern::checkPattern(play);
        cout<<players[who].name<<" 出了：";
        for(const auto& c:play)cout<<c.getName()<<' ';
        cout<<'['<<pat.getPatternName()<<"]\n";
        if(players[who].isEmpty()){
            gameOver=true;
            cout<<players[who].name<<" 获胜！"<<(players[who].isLandlord?"地主":"农民")<<"阵营胜利！\n";
        }
        if(!validateAllCards()) throw runtime_error("出牌后牌数校验失败");
    }

    void doPass(int who){
        cout<<players[who].name<<" 不出\n";
        if(++passCount==2){
            cout<<"其余两家均不出，开始新一轮。\n";
            currentPlayer=lastPlayer; lastPlayer=-1; lastPlayedCards.clear(); passCount=0;
        } else currentPlayer=(who+1)%3;
    }

    void playerPlay(){
        showState();
        while(true){
            players[0].displayHand();
            cout<<"输入牌号（空格分隔）"<<(lastPlayer>=0?"，输入0不出":"")<<"：";
            string line; if(!getline(cin,line)){gameOver=true;return;}
            if(line=="0"){
                if(lastPlayer<0){cout<<"本轮你先出，不能不出。\n";continue;}
                doPass(0); return;
            }
            stringstream ss(line); int x; vector<int> ids; bool bad=false; set<int> seen;
            while(ss>>x){--x;if(x<0||x>=(int)players[0].hand.size()||!seen.insert(x).second)bad=true;else ids.push_back(x);}
            if(bad||ids.empty()){cout<<"牌号无效或重复，请重试。\n";continue;}
            vector<Card> play; for(int id:ids)play.push_back(players[0].hand[id]);
            auto pat=CardPattern::checkPattern(play);
            if(pat.type==CardPattern::INVALID){cout<<"无效牌型。\n";continue;}
            if(lastPlayer>=0 && !CardPattern::canBeat(pat,CardPattern::checkPattern(lastPlayedCards))){
                cout<<"压不过当前牌。\n";continue;
            }
            recordPlay(0,play);
            if(!gameOver) currentPlayer=1;
            return;
        }
    }

    void computerPlay(){
        ComputerAI ai(currentPlayer,players);
        vector<Card> play=ai.choosePlay(lastPlayedCards,lastPlayer);
        if(play.empty()){doPass(currentPlayer);return;}
        int who=currentPlayer; recordPlay(who,play);
        if(!gameOver)currentPlayer=(who+1)%3;
    }

    void playOneGame(){
        dealCards();
        while(!gameOver){if(currentPlayer==0)playerPlay();else computerPlay();}
    }

    void run(){
        cout<<"欢迎来到斗地主（修正版）！\n";
        while(true){
            selectMode(); playOneGame();
            cout<<"再来一局？(y/n)："; string s; getline(cin,s); if(s!="y"&&s!="Y")break;
        }
    }
};

#include "lan.h"

int main(){
    try{
        cout << "斗地主模式：\n1. 本机人机\n2. 局域网开房\n3. 加入局域网房间\n4. 使用令牌重连房间\n> ";
        string mode; getline(cin,mode);
        if(mode=="2"){
            cout << "端口（直接回车使用34567）："; string s; getline(cin,s);
            int port=s.empty()?34567:stoi(s);
            cout << "远程真人数量（0～2，其余座位由AI补齐）："; getline(cin,s);
            int remoteCount=stoi(s);
            lan::HostGame host; host.run(port,remoteCount);
        }else if(mode=="3"){
            cout << "房主IP："; string ip; getline(cin,ip);
            cout << "端口（直接回车使用34567）："; string s; getline(cin,s);
            int port=s.empty()?34567:stoi(s);
            cout << "你的名字（不能含空格）："; string name; getline(cin,name);
            if(name.empty())name="玩家";
            lan::ClientGame client; client.run(ip,port,name);
        }else if(mode=="4"){
            cout << "房主IP："; string ip; getline(cin,ip);
            cout << "端口（直接回车使用34567）："; string s; getline(cin,s);
            int port=s.empty()?34567:stoi(s);
            cout << "重连令牌："; string token; getline(cin,token);
            lan::ClientGame client; client.run(ip,port,"",token);
        }else{
            DouDizhuGame game;game.run();
        }
    }catch(const exception& e){cerr<<"程序错误："<<e.what()<<'\n';return 1;}
    return 0;
}
