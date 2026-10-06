#include "test_framework.h"
#include <cstdio>
#include <cstdlib>

// 测试可复现性（2026-10-05）：GameEngine 未显式 setSeed 时会读 THKS_SEED，否则用当前时间。
// 大量既有测试没有调用 setSeed（只固定武将、不固定身份与牌堆），种子随运行时刻变化会让
// 身份洗牌与牌堆顺序改变，从而偶发失败（例如 mou/liucheng_lueying_* 曾两次随机失败，
// 排查后确认与代码无关，纯粹是时间种子）。这里统一给测试进程一个默认种子；
// 需要复现其它种子时，外部显式设置 THKS_SEED 即可覆盖（本函数不会覆盖已有值）。
static void ensureDeterministicSeed() {
    const char* existing = std::getenv("THKS_SEED");
    if (existing && *existing) return;
#if defined(_WIN32)
    _putenv_s("THKS_SEED", "20261005");
#else
    setenv("THKS_SEED", "20261005", 1);
#endif
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0); // 崩溃时不丢失已输出内容
    ensureDeterministicSeed();
    auto& tests = thkstest::registry();
    std::printf("运行 %zu 个测试...\n\n", tests.size());
    for (auto& t : tests) {
        thkstest::currentFailed() = false;
        thkstest::currentName() = t.name;
        t.fn();
        if (!thkstest::currentFailed()) {
            std::printf("[通过] %s\n", t.name.c_str());
        } else {
            thkstest::failedTests()++;
        }
    }
    std::printf("\n==================== 结果 ====================\n");
    std::printf("测试: %zu 个 | 失败: %d 个 | 断言: %d 次\n",
                tests.size(), thkstest::failedTests(), thkstest::totalChecks());
    std::printf("==============================================\n");
    return thkstest::failedTests() == 0 ? 0 : 1;
}
