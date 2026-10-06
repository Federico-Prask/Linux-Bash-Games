// 一次性审计工具：对比技能描述中的类型关键词与 SkillTag 实际登记（实现级复核辅助）。
// 用法：g++ -std=c++17 -Iinclude tools/audit_tags.cpp src/*.o -o /tmp/audit_tags && /tmp/audit_tags
#include "HeroRegistry.h"
#include <iostream>
#include <string>
#include <vector>

using namespace Thks;

// 描述里是否“声明”了某关键词（排除“非锁定技/非限定技”等否定用法）
static bool declares(const std::string& s, const std::string& kw) {
    size_t pos = 0;
    while ((pos = s.find(kw, pos)) != std::string::npos) {
        if (pos > 0 && s.compare(pos - 3, 3, "非") == 0) { pos += kw.size(); continue; }
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    bool verbose = argc > 1 && std::string(argv[1]) == "all";
    int flagged = 0;
    for (auto& info : HeroRegistry::all()) {
        auto hero = info.create();
        if (!hero) continue;
        for (auto& sk : hero->getSkills()) {
            const std::string d = sk->getDescription();
            struct { const char* kw; unsigned tag; } rules[] = {
                {"锁定技", SkillTag::LOCK}, {"限定技", SkillTag::LIMITED},
                {"觉醒技", SkillTag::AWAKEN}, {"主公技", SkillTag::LORD},
                {"转换技", SkillTag::SWITCH}, {"持恒技", SkillTag::SUSTAINED},
            };
            std::string tags;
            for (auto& r : rules) {
                bool inText = declares(d, r.kw);
                bool inTag = sk->hasTag(r.tag);
                if (inTag) tags += std::string(r.kw) + " ";
                if (inText != inTag) {
                    ++flagged;
                    std::cout << "[" << info.name << "·" << sk->getName() << "] " << r.kw << " "
                              << (inText ? "文本有/标签无" : "文本无/标签有") << "\n";
                }
            }
            if (declares(d, "使命技"))
                std::cout << "[" << info.name << "·" << sk->getName() << "] 使命技 无对应标签（检查实现）\n";
            if (verbose) std::cout << "    " << info.name << "·" << sk->getName() << "  标签:[" << tags << "]\n";
        }
    }
    std::cout << "flagged=" << flagged << "\n";
    return 0;
}
