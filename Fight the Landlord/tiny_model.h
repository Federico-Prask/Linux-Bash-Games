#ifndef DDZ_TINY_MODEL_H
#define DDZ_TINY_MODEL_H

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// CPU专用“共享局面编码器 + 小动作头”。大网络每回合只运行一次，
// 动作头只评价规则AI筛出的Top-64，避免给每个候选重复跑64 MB网络。
class TinyPolicyModel {
public:
    static constexpr int INPUT = 72;
    static constexpr int STATE = 40;
    static constexpr int ACTION = 40;
    static constexpr int S1 = 512;
    static constexpr int S2 = 4096;
    static constexpr int S3 = 3072;
    static constexpr int EMBED = 512;
    static constexpr int HEAD = 256;

private:
    // state: 40 -> 512 -> 4096 -> 3072 -> 512
    std::vector<float> sw1,sb1,sw2,sb2,sw3,sb3,sw4,sb4;
    // action head: (512 + 40) -> 256 -> 1
    std::vector<float> hw1,hb1,hw2;
    float hb2=0.0f;
    bool trained=false;
    bool announced=false;

    TinyPolicyModel(){
        if(const char* path=std::getenv("DDZ_MODEL"))load(path);
    }

    void allocateWeights(){
        sw1.resize(S1*STATE);sb1.resize(S1);
        sw2.resize(S2*S1);sb2.resize(S2);
        sw3.resize(S3*S2);sb3.resize(S3);
        sw4.resize(EMBED*S3);sb4.resize(EMBED);
        hw1.resize(HEAD*(EMBED+ACTION));hb1.resize(HEAD);
        hw2.resize(HEAD);
    }

    void releaseWeights(){
        sw1.clear();sb1.clear();sw2.clear();sb2.clear();sw3.clear();sb3.clear();
        sw4.clear();sb4.clear();hw1.clear();hb1.clear();hw2.clear();
        sw1.shrink_to_fit();sw2.shrink_to_fit();sw3.shrink_to_fit();sw4.shrink_to_fit();
        hw1.shrink_to_fit();
    }

    static float dot(const float* a,const float* b,int n){
        float sum=0.0f;
        // -O3 -march=native 可将此连续循环自动向量化。
        for(int i=0;i<n;++i)sum+=a[i]*b[i];
        return sum;
    }

    static void denseRelu(const std::vector<float>& input,const std::vector<float>& w,
                          const std::vector<float>& bias,std::vector<float>& output,
                          int inSize,int outSize){
        output.resize(outSize);
#ifdef DDZ_USE_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for(int o=0;o<outSize;++o){
            float z=bias[o]+dot(input.data(),w.data()+static_cast<size_t>(o)*inSize,inSize);
            output[o]=z>0.0f?z:0.0f;
        }
    }

    // 从原72维特征拆出共享局面特征。动作相关维度不会进入大干线。
    static std::vector<float> extractState(const float* x){
        std::vector<float> s(STATE,0.0f);int k=0;
        for(int i=0;i<30;++i)s[k++]=x[i];             // 手牌、上手牌
        for(int i=45;i<=49;++i)s[k++]=x[i];           // 剩余张数、身份、先后手
        s[k++]=x[52];                                  // 上家是否队友
        for(int i=69;i<=71;++i)s[k++]=x[i];           // 阶段、危险度、常量
        return s;
    }

    static std::vector<float> extractAction(const float* x){
        std::vector<float> a(ACTION,0.0f);int k=0;
        for(int i=30;i<45;++i)a[k++]=x[i];             // 候选牌点数组成
        a[k++]=x[50];a[k++]=x[51];                    // 主值、张数
        for(int i=53;i<=68;++i)a[k++]=x[i];           // 牌型one-hot
        return a;
    }

public:
    TinyPolicyModel(const TinyPolicyModel&)=delete;
    TinyPolicyModel& operator=(const TinyPolicyModel&)=delete;
    static TinyPolicyModel& instance(){static TinyPolicyModel model;return model;}

    bool load(const std::string& path){
        std::ifstream f(path,std::ios::binary);
        if(!f)return false;
        char magic[8]{};f.read(magic,8);
        if(std::string(magic,magic+7)!="DDZMLP4")return false;
        allocateWeights();
        auto read=[&](std::vector<float>& v){
            f.read(reinterpret_cast<char*>(v.data()),v.size()*sizeof(float));
        };
        read(sw1);read(sb1);read(sw2);read(sb2);read(sw3);read(sb3);
        read(sw4);read(sb4);read(hw1);read(hb1);read(hw2);
        f.read(reinterpret_cast<char*>(&hb2),sizeof(float));
        trained=static_cast<bool>(f);
        if(!trained)releaseWeights();
        return trained;
    }

    bool isTrained()const{return trained;}
    std::string backend()const{
#ifdef DDZ_USE_OPENMP
        return "CPU / OpenMP";
#else
        return "CPU / 单线程";
#endif
    }

    std::vector<float> forward(const std::vector<float>& x){
        if(x.empty()||x.size()%INPUT)return {};
        const int rows=static_cast<int>(x.size()/INPUT);
        if(!announced){
            std::cout<<"AI模型：共享干线40-512-4096-3072-512，动作头552-256-1；"
                     <<backend()<<(trained?"，已加载约64 MB权重":"，无权重（仅规则AI）")<<"\n";
            announced=true;
        }
        if(!trained)return std::vector<float>(rows,0.0f);

        // 所有候选来自同一个局面，因此64 MB干线只计算一次。
        auto state=extractState(x.data());
        std::vector<float> h1,h2,h3,embedding;
        denseRelu(state,sw1,sb1,h1,STATE,S1);
        denseRelu(h1,sw2,sb2,h2,S1,S2);
        denseRelu(h2,sw3,sb3,h3,S2,S3);
        denseRelu(h3,sw4,sb4,embedding,S3,EMBED);

        std::vector<float> result(rows);
#ifdef DDZ_USE_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for(int row=0;row<rows;++row){
            auto action=extractAction(x.data()+static_cast<size_t>(row)*INPUT);
            std::vector<float> joined(EMBED+ACTION),hidden(HEAD);
            std::copy(embedding.begin(),embedding.end(),joined.begin());
            std::copy(action.begin(),action.end(),joined.begin()+EMBED);
            for(int o=0;o<HEAD;++o){
                float z=hb1[o]+dot(joined.data(),hw1.data()+static_cast<size_t>(o)*(EMBED+ACTION),EMBED+ACTION);
                hidden[o]=z>0.0f?z:0.0f;
            }
            result[row]=hb2+dot(hidden.data(),hw2.data(),HEAD);
        }
        return result;
    }
};

#endif
