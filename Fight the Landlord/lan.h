#ifndef DDZ_LAN_H
#define DDZ_LAN_H

#include <arpa/inet.h>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// 行文本TCP协议；房主保存全部隐藏状态，客户端只收到自己的手牌。
namespace lan {

inline int cardId(const Card& c){
    if(c.rank==Card::LITTLE_JOKER)return 52;
    if(c.rank==Card::BIG_JOKER)return 53;
    return (c.getValue()-3)*4+static_cast<int>(c.suit);
}
inline Card cardFromId(int id){
    if(id==52)return {Card::JOKER,Card::LITTLE_JOKER};
    if(id==53)return {Card::JOKER,Card::BIG_JOKER};
    return {static_cast<Card::Suit>(id%4),static_cast<Card::Rank>(id/4+3)};
}
inline std::vector<std::string> words(const std::string& line){
    std::stringstream ss(line);std::vector<std::string> out;std::string s;
    while(ss>>s) out.push_back(s);
    return out;
}

class LineSocket {
    int fd_=-1;std::string pending;
public:
    LineSocket()=default;explicit LineSocket(int fd):fd_(fd){}
    LineSocket(const LineSocket&)=delete;LineSocket& operator=(const LineSocket&)=delete;
    LineSocket(LineSocket&& o)noexcept:fd_(o.fd_),pending(std::move(o.pending)){o.fd_=-1;}
    LineSocket& operator=(LineSocket&& o)noexcept{
        if(this!=&o){closeNow();fd_=o.fd_;pending=std::move(o.pending);o.fd_=-1;}return *this;
    }
    ~LineSocket(){closeNow();}
    bool valid()const{return fd_>=0;}
    void closeNow(){if(fd_>=0){::close(fd_);fd_=-1;}pending.clear();}
    bool sendLine(const std::string& text){
        if(fd_<0) return false;
        std::string data=text+'\n';size_t sent=0;
        while(sent<data.size()){
            ssize_t n=::send(fd_,data.data()+sent,data.size()-sent,MSG_NOSIGNAL);
            if(n<=0){if(errno==EINTR)continue;return false;}sent+=static_cast<size_t>(n);
        }return true;
    }
    bool recvLine(std::string& line){
        while(true){
            auto p=pending.find('\n');
            if(p!=std::string::npos){line=pending.substr(0,p);pending.erase(0,p+1);return true;}
            char b[2048];ssize_t n=::recv(fd_,b,sizeof(b),0);
            if(n==0) return false;
            if(n<0){if(errno==EINTR)continue;return false;}
            pending.append(b,static_cast<size_t>(n));if(pending.size()>65536)return false;
        }
    }
};

inline int createListener(int port){
    int fd=::socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;int yes=1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=INADDR_ANY;a.sin_port=htons(port);
    if(::bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0||::listen(fd,8)<0){::close(fd);return -1;}return fd;
}
inline int connectTo(const std::string& ip,int port){
    int fd=::socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&a.sin_addr)!=1||::connect(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0){::close(fd);return -1;}return fd;
}
inline std::string localIpHint(){
    int fd=::socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return "0.0.0.0";sockaddr_in a{};
    a.sin_family=AF_INET;a.sin_port=htons(53);inet_pton(AF_INET,"8.8.8.8",&a.sin_addr);
    ::connect(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a));socklen_t n=sizeof(a);getsockname(fd,reinterpret_cast<sockaddr*>(&a),&n);
    char out[INET_ADDRSTRLEN]="0.0.0.0";inet_ntop(AF_INET,&a.sin_addr,out,sizeof(out));::close(fd);return out;
}

class HostGame {
    enum Phase{LOBBY,BIDDING,PLAYING,ROUND_OVER};
    std::vector<Player> players{Player("房主"),Player("玩家2"),Player("玩家3")};
    std::array<bool,3> remote{{false,false,false}};
    std::array<LineSocket,3> peer;
    std::array<std::string,3> token;
    std::vector<Card> deck,bottom,last,played;
    int listener=-1,current=0,lastPlayer=-1,passes=0,landlord=-1;
    int bidSeat=-1,currentBid=0,highestBidder=-1;
    bool over=false;Phase phase=LOBBY;
    std::mt19937 rng{std::random_device{}()};

    std::string newToken(){
        std::ostringstream o;for(int i=0;i<4;++i)o<<std::hex<<std::setw(8)<<std::setfill('0')<<rng();return o.str();
    }
    std::string cardsMsg(const std::string& tag,const std::vector<Card>& c)const{
        std::ostringstream o;o<<tag<<' '<<c.size();for(const auto& x:c)o<<' '<<cardId(x);return o.str();
    }
    std::string namesMsg()const{return "NAMES "+players[0].name+' '+players[1].name+' '+players[2].name;}
    std::string stateMsg()const{
        std::ostringstream o;o<<"STATE "<<current<<' '<<landlord<<' '<<lastPlayer<<' '<<passes;
        for(const auto& p:players) o<<' '<<p.hand.size();
        o<<' '<<last.size();
        for(const auto& c:last) o<<' '<<cardId(c);
        return o.str();
    }
    std::string landlordMsg()const{
        std::ostringstream o;o<<"LANDLORD "<<landlord<<' '<<bottom.size();for(const auto& c:bottom)o<<' '<<cardId(c);return o.str();
    }
    void rawSnapshot(int seat){
        peer[seat].sendLine("WELCOME "+std::to_string(seat)+' '+token[seat]);
        peer[seat].sendLine(namesMsg());peer[seat].sendLine(cardsMsg("HAND",players[seat].hand));
        if(phase==BIDDING)peer[seat].sendLine("BIDSTATE "+std::to_string(bidSeat)+' '+std::to_string(currentBid)+' '+std::to_string(highestBidder));
        if(phase==PLAYING||phase==ROUND_OVER){peer[seat].sendLine(landlordMsg());peer[seat].sendLine(stateMsg());}
    }
    void reconnectSeat(int seat){
        peer[seat].closeNow();std::cout<<players[seat].name<<"掉线，等待使用重连令牌恢复...\n";
        while(true){
            sockaddr_in a{};socklen_t n=sizeof(a);int fd=::accept(listener,reinterpret_cast<sockaddr*>(&a),&n);
            if(fd<0){if(errno==EINTR)continue;throw std::runtime_error("重连接受失败");}
            LineSocket candidate(fd);std::string hello;
            if(!candidate.recvLine(hello)) continue;
            auto w=words(hello);
            if(w.size()==2&&w[0]=="REJOIN"&&w[1]==token[seat]){
                peer[seat]=std::move(candidate);rawSnapshot(seat);std::cout<<players[seat].name<<"重连成功。\n";return;
            }
            candidate.sendLine("ERROR 重连令牌无效");
        }
    }
    void sendTo(int seat,const std::string& s){
        if(!remote[seat]) return;
        while(!peer[seat].sendLine(s)) reconnectSeat(seat);
    }
    void broadcast(const std::string& s){for(int i=1;i<3;++i)if(remote[i])sendTo(i,s);}
    bool recvFrom(int seat,std::string& s){
        while(!peer[seat].recvLine(s)){reconnectSeat(seat);if(phase==BIDDING)sendTo(seat,"REQUEST_BID "+std::to_string(currentBid));else if(phase==PLAYING)sendTo(seat,"REQUEST");}
        return true;
    }
    void sendHands(){for(int i=1;i<3;++i)if(remote[i])sendTo(i,cardsMsg("HAND",players[i].hand));}
    void syncState(){sendHands();broadcast(stateMsg());}
    void showState(bool withHand=true)const{
        std::cout<<"\n--- 局域网牌局 ---\n";for(int i=0;i<3;++i)std::cout<<players[i].name<<(i==landlord?"[地主]":"[农民]")<<":"<<players[i].hand.size()<<"张\n";
        if(lastPlayer>=0){std::cout<<"需压 "<<players[lastPlayer].name<<"：";for(const auto& c:last)std::cout<<c.getName()<<' ';std::cout<<"\n";}
        if(withHand)players[0].displayHand();
    }
    void makeDeckAndDeal17(){
        deck.clear();bottom.clear();played.clear();last.clear();
        for(int r=Card::THREE;r<=Card::TWO;++r)for(int s=0;s<4;++s)deck.emplace_back(static_cast<Card::Suit>(s),static_cast<Card::Rank>(r));
        deck.emplace_back(Card::JOKER,Card::LITTLE_JOKER);deck.emplace_back(Card::JOKER,Card::BIG_JOKER);std::shuffle(deck.begin(),deck.end(),rng);
        for(auto& p:players){p.hand.clear();p.isLandlord=false;}
        for(int i=0;i<51;++i) players[i%3].hand.push_back(deck[i]);
        bottom.assign(deck.begin()+51,deck.end());
        for(auto& p:players) p.sortHand();
        landlord=-1;lastPlayer=-1;passes=0;over=false;broadcast("DEAL");sendHands();
    }
    int aiBid(int seat)const{
        std::map<int,int> cnt;int score=0;for(const auto& c:players[seat].hand)++cnt[c.getValue()];
        for(auto [v,n]:cnt){if(n==4)score+=7;if(v==15)score+=n*2;if(v>=16)score+=5;}
        if(cnt.count(16)&&cnt.count(17)) score+=8;
        int target=score>=18?3:score>=11?2:score>=6?1:0;
        return target>currentBid?target:0;
    }
    int askBid(int seat){
        if(seat==0){
            players[0].displayHand();while(true){std::cout<<"当前最高叫分"<<currentBid<<"，请输入0(不叫)或"<<currentBid+1<<"～3：";
                std::string s;std::getline(std::cin,s);try{int b=std::stoi(s);if(b==0||(b>currentBid&&b<=3))return b;}catch(...){}std::cout<<"叫分无效。\n";}
        }
        if(remote[seat]){
            sendTo(seat,"REQUEST_BID "+std::to_string(currentBid));while(true){std::string line;recvFrom(seat,line);auto w=words(line);
                if(w.size()==2&&w[0]=="BID"){try{int b=std::stoi(w[1]);if(b==0||(b>currentBid&&b<=3))return b;}catch(...){} }
                sendTo(seat,"ERROR 叫分无效");sendTo(seat,"REQUEST_BID "+std::to_string(currentBid));}
        }
        int b=aiBid(seat);std::cout<<players[seat].name<<(b?" 叫 "+std::to_string(b):" 不叫")<<"\n";return b;
    }
    void bidding(){
        phase=BIDDING;
        while(true){
            makeDeckAndDeal17();int start=std::uniform_int_distribution<int>(0,2)(rng);currentBid=0;highestBidder=-1;
            for(int turn=0;turn<3;++turn){
                bidSeat=(start+turn)%3;broadcast("BIDSTATE "+std::to_string(bidSeat)+' '+std::to_string(currentBid)+' '+std::to_string(highestBidder));
                int b=askBid(bidSeat);broadcast("BIDRESULT "+std::to_string(bidSeat)+' '+std::to_string(b));
                if(b>currentBid){currentBid=b;highestBidder=bidSeat;}if(b==3)break;
            }
            if(highestBidder<0){std::cout<<"三家都不叫，重新发牌。\n";broadcast("REDEAL");continue;}
            landlord=highestBidder;players[landlord].isLandlord=true;players[landlord].hand.insert(players[landlord].hand.end(),bottom.begin(),bottom.end());players[landlord].sortHand();
            current=landlord;lastPlayer=-1;passes=0;phase=PLAYING;broadcast(landlordMsg());sendHands();
            std::cout<<players[landlord].name<<"成为地主（"<<currentBid<<"分），底牌：";for(const auto& c:bottom)std::cout<<c.getName()<<' ';std::cout<<"\n";return;
        }
    }
    bool legalPlay(int seat,const std::vector<Card>& p,std::string& error)const{
        if(p.empty()){error="没有选择牌";return false;}auto copy=players[seat].hand;
        for(const auto& c:p){auto it=std::find(copy.begin(),copy.end(),c);if(it==copy.end()){error="选择了不在手中的牌";return false;}copy.erase(it);}
        auto pat=CardPattern::checkPattern(p);if(pat.type==CardPattern::INVALID){error="无效牌型";return false;}
        if(lastPlayer>=0&&!CardPattern::canBeat(pat,CardPattern::checkPattern(last))){error="压不过当前牌";return false;}return true;
    }
    std::vector<Card> parsePlay(const std::vector<std::string>& w)const{
        std::vector<Card> p;if(w.size()<2)return p;int n=std::stoi(w[1]);if(n<0||w.size()!=static_cast<size_t>(n+2))return {};
        for(int i=0;i<n;++i){int id=std::stoi(w[i+2]);if(id<0||id>53)return {};p.push_back(cardFromId(id));}return p;
    }
    void applyPlay(int seat,const std::vector<Card>& p){
        players[seat].removeCards(p);played.insert(played.end(),p.begin(),p.end());last=p;lastPlayer=seat;passes=0;
        std::ostringstream e;e<<"EVENT "<<seat<<" PLAY "<<p.size();for(const auto& c:p)e<<' '<<cardId(c);broadcast(e.str());
        std::cout<<players[seat].name<<" 出牌：";for(const auto& c:p)std::cout<<c.getName()<<' ';std::cout<<"\n";
        if(players[seat].isEmpty()){
            over=true;phase=ROUND_OVER;std::string side=players[seat].isLandlord?"LANDLORD":"FARMER";
            broadcast("GAMEOVER "+std::to_string(seat)+' '+side);std::cout<<players[seat].name<<"获胜，"<<(side=="LANDLORD"?"地主":"农民")<<"阵营胜利！\n";
        }else current=(seat+1)%3;
    }
    void applyPass(int seat){
        broadcast("EVENT "+std::to_string(seat)+" PASS");std::cout<<players[seat].name<<" 不出\n";
        if(++passes==2){current=lastPlayer;lastPlayer=-1;last.clear();passes=0;}else current=(seat+1)%3;
    }
    void hostTurn(){
        showState();while(true){std::cout<<"输入牌号"<<(lastPlayer>=0?"，或0不出":"")<<"：";std::string line;std::getline(std::cin,line);
            if(line=="0"){if(lastPlayer<0){std::cout<<"先手不能不出。\n";continue;}applyPass(0);return;}
            std::stringstream ss(line);int n;std::set<int> used;std::vector<Card> p;bool bad=false;
            while(ss>>n){--n;if(n<0||n>=(int)players[0].hand.size()||!used.insert(n).second)bad=true;else p.push_back(players[0].hand[n]);}
            std::string err;if(bad||!legalPlay(0,p,err)){std::cout<<(bad?"牌号无效":err)<<"\n";continue;}applyPlay(0,p);return;}
    }
    void remoteTurn(int seat){
        sendTo(seat,"REQUEST");while(true){std::string line;recvFrom(seat,line);auto w=words(line);if(w.empty())continue;
            if(w[0]=="PASS"){if(lastPlayer<0){sendTo(seat,"ERROR 先手不能不出");sendTo(seat,"REQUEST");continue;}applyPass(seat);return;}
            if(w[0]=="PLAY")try{auto p=parsePlay(w);std::string err;if(!legalPlay(seat,p,err)){sendTo(seat,"ERROR "+err);sendTo(seat,"REQUEST");continue;}applyPlay(seat,p);return;}
                catch(...){sendTo(seat,"ERROR 出牌数据错误");sendTo(seat,"REQUEST");}
        }
    }
    void aiTurn(int seat){
        ComputerAI ai(seat,players);auto p=ai.choosePlay(last,lastPlayer);
        if(p.empty()){if(lastPlayer<0)p={players[seat].hand[0]};else{applyPass(seat);return;}}applyPlay(seat,p);
    }
    void playRound(){
        bidding();while(!over){syncState();if(current==0)hostTurn();else if(remote[current])remoteTurn(current);else aiTurn(current);}
    }

public:
    ~HostGame(){if(listener>=0)::close(listener);}
    void run(int port,int remoteCount){
        if(remoteCount<0||remoteCount>2)throw std::runtime_error("远程玩家数必须是0到2");
        listener=createListener(port);if(listener<0)throw std::runtime_error("无法监听端口，可能已被占用");
        if(remoteCount)std::cout<<"房间地址："<<localIpHint()<<":"<<port<<"，等待"<<remoteCount<<"名玩家...\n";
        for(int seat=1;seat<=remoteCount;++seat){
            sockaddr_in a{};socklen_t n=sizeof(a);int fd=::accept(listener,reinterpret_cast<sockaddr*>(&a),&n);if(fd<0)throw std::runtime_error("接受连接失败");
            peer[seat]=LineSocket(fd);remote[seat]=true;std::string hello;if(!peer[seat].recvLine(hello))throw std::runtime_error("客户端握手失败");auto w=words(hello);
            if(w.size()>=2&&w[0]=="HELLO") players[seat].name=w[1];
            token[seat]=newToken();sendTo(seat,"WELCOME "+std::to_string(seat)+' '+token[seat]);
            std::cout<<players[seat].name<<"已加入，重连令牌："<<token[seat]<<"\n";
        }
        for(int i=remoteCount+1;i<3;++i) players[i].name="电脑"+std::to_string(i);
        broadcast(namesMsg());
        while(true){playRound();std::cout<<"保持当前房间再来一局？(y/n)：";std::string s;std::getline(std::cin,s);
            if(s!="y"&&s!="Y"){broadcast("ROOM_CLOSED");break;}broadcast("REMATCH");}
    }
};

class ClientGame {
    LineSocket socket;std::string ip,name,token;int port=0,seat=-1,landlord=-1,current=-1,lastPlayer=-1;
    std::array<std::string,3> names{{"房主","玩家2","玩家3"}};std::array<int,3> remaining{{17,17,17}};
    std::vector<Card> hand,last;bool closing=false;

    void reconnect(){
        std::cout<<"连接中断，正在自动重连...\n";
        while(true){int fd=connectTo(ip,port);if(fd>=0){socket=LineSocket(fd);if(socket.sendLine("REJOIN "+token)){std::cout<<"已连接，等待房主恢复会话。\n";return;}}
            std::this_thread::sleep_for(std::chrono::seconds(2));}
    }
    void show()const{
        std::cout<<"\n--- 局域网牌局 ---\n";for(int i=0;i<3;++i)std::cout<<names[i]<<(i==landlord?"[地主]":"[农民]")<<":"<<remaining[i]<<"张\n";
        if(lastPlayer>=0){std::cout<<"需压 "<<names[lastPlayer]<<"：";for(const auto& c:last)std::cout<<c.getName()<<' ';std::cout<<"\n";}
        std::cout<<"你的手牌：\n";for(size_t i=0;i<hand.size();++i)std::cout<<'('<<i+1<<')'<<hand[i].getName()<<' ';std::cout<<"\n";
    }
    void readCards(const std::vector<std::string>& w,std::vector<Card>& out,int pos){
        out.clear();int n=std::stoi(w[pos]);for(int i=0;i<n;++i)out.push_back(cardFromId(std::stoi(w[pos+1+i])));std::sort(out.begin(),out.end());
    }
    void requestBid(int high){
        while(true){std::cout<<"当前最高叫分"<<high<<"，输入0(不叫)或"<<high+1<<"～3：";std::string s;std::getline(std::cin,s);
            try{int b=std::stoi(s);if(b==0||(b>high&&b<=3)){socket.sendLine("BID "+std::to_string(b));return;}}catch(...){}std::cout<<"叫分无效。\n";}
    }
    void requestPlay(){
        show();while(true){std::cout<<"轮到你，输入牌号"<<(lastPlayer>=0?"，或0不出":"")<<"：";std::string line;std::getline(std::cin,line);
            if(line=="0"){socket.sendLine("PASS");return;}std::stringstream ss(line);int n;std::set<int> used;std::vector<Card> p;bool bad=false;
            while(ss>>n){--n;if(n<0||n>=(int)hand.size()||!used.insert(n).second)bad=true;else p.push_back(hand[n]);}
            if(bad||p.empty()){std::cout<<"牌号无效。\n";continue;}std::ostringstream o;o<<"PLAY "<<p.size();for(const auto& c:p)o<<' '<<cardId(c);socket.sendLine(o.str());return;}
    }
public:
    void run(const std::string& host,int p,const std::string& playerName,const std::string& resumeToken=""){
        ip=host;port=p;name=playerName;token=resumeToken;
        int fd=connectTo(ip,port);if(fd<0)throw std::runtime_error("无法连接房主");
        socket=LineSocket(fd);
        if(token.empty()) socket.sendLine("HELLO "+name);
        else socket.sendLine("REJOIN "+token);
        while(!closing){std::string line;if(!socket.recvLine(line)){if(token.empty())throw std::runtime_error("握手期间连接中断");reconnect();continue;}auto w=words(line);if(w.empty())continue;
            if(w[0]=="WELCOME"){seat=std::stoi(w[1]);if(w.size()>=3)token=w[2];std::cout<<"你的重连令牌："<<token<<"（请勿泄露）\n";}
            else if(w[0]=="NAMES"&&w.size()>=4)for(int i=0;i<3;++i)names[i]=w[i+1];
            else if(w[0]=="DEAL"){landlord=-1;lastPlayer=-1;last.clear();std::cout<<"新一轮发牌，开始叫地主。\n";}
            else if(w[0]=="HAND")readCards(w,hand,1);
            else if(w[0]=="BIDSTATE")std::cout<<"轮到"<<names[std::stoi(w[1])]<<"叫地主，当前最高"<<w[2]<<"分。\n";
            else if(w[0]=="REQUEST_BID")requestBid(std::stoi(w[1]));
            else if(w[0]=="BIDRESULT")std::cout<<names[std::stoi(w[1])]<<(w[2]=="0"?"不叫":"叫"+w[2]+"分")<<"\n";
            else if(w[0]=="REDEAL")std::cout<<"三家不叫，重新发牌。\n";
            else if(w[0]=="LANDLORD"){
                landlord=std::stoi(w[1]);std::cout<<names[landlord]<<"成为地主，底牌：";int n=std::stoi(w[2]);for(int i=0;i<n;++i)std::cout<<cardFromId(std::stoi(w[3+i])).getName()<<' ';std::cout<<"\n";
            }else if(w[0]=="STATE"){
                current=std::stoi(w[1]);landlord=std::stoi(w[2]);lastPlayer=std::stoi(w[3]);for(int i=0;i<3;++i)remaining[i]=std::stoi(w[5+i]);
                int n=std::stoi(w[8]);last.clear();for(int i=0;i<n;++i)last.push_back(cardFromId(std::stoi(w[9+i])));if(current!=seat)show();
            }else if(w[0]=="REQUEST")requestPlay();
            else if(w[0]=="ERROR"){std::cout<<"房主拒绝：";for(size_t i=1;i<w.size();++i)std::cout<<w[i];std::cout<<"\n";}
            else if(w[0]=="EVENT"){int who=std::stoi(w[1]);std::cout<<names[who]<<(w[2]=="PASS"?" 不出":" 已出牌")<<"\n";}
            else if(w[0]=="GAMEOVER"){int winner=std::stoi(w[1]);std::cout<<"本局结束："<<names[winner]<<"获胜，"<<(w[2]=="LANDLORD"?"地主":"农民")<<"阵营胜利！等待房主决定是否再来一局。\n";}
            else if(w[0]=="REMATCH")std::cout<<"房主开始下一局，保持当前连接。\n";
            else if(w[0]=="ROOM_CLOSED"){std::cout<<"房主已关闭房间。\n";closing=true;}
        }
    }
};

} // namespace lan
#endif
