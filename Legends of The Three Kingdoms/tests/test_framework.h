#ifndef THKS_TEST_FRAMEWORK_H
#define THKS_TEST_FRAMEWORK_H

// =====================================================================
//  极简单元测试框架（零依赖，tests 专用）
//
//  用法：
//      TEST(test_name) { CHECK(cond); CHECK_EQ(a, b); }
//  所有 TEST 自动注册，test_main.cpp 统一运行并汇总。
// =====================================================================
#include <cstdio>
#include <string>
#include <sstream>
#include <functional>
#include <vector>

namespace thkstest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}
inline int& failedTests() { static int f = 0; return f; }
inline int& totalChecks() { static int c = 0; return c; }
inline bool& currentFailed() { static bool b = false; return b; }
inline std::string& currentName() { static std::string n; return n; }

inline int registerTest(const char* name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
    return 0;
}

inline void reportFailure(const char* file, int line, const std::string& what) {
    if (!currentFailed()) {
        std::printf("  FAIL [%s] %s:%d\n    %s\n", currentName().c_str(), file, line, what.c_str());
        currentFailed() = true;
    }
}

template <typename T>
std::string repr(const T& v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

} // namespace thkstest

#define THKS_CONCAT2(a, b) a##b
#define THKS_CONCAT(a, b) THKS_CONCAT2(a, b)

#define TEST(name)                                                                   \
    static void THKS_CONCAT(thks_test_fn_, __LINE__)();                              \
    static int THKS_CONCAT(thks_test_reg_, __LINE__) =                               \
        ::thkstest::registerTest(name, THKS_CONCAT(thks_test_fn_, __LINE__));        \
    static void THKS_CONCAT(thks_test_fn_, __LINE__)()

// 检查条件为真
#define CHECK(cond)                                                                  \
    do {                                                                             \
        ::thkstest::totalChecks()++;                                                 \
        if (!(cond))                                                                 \
            ::thkstest::reportFailure(__FILE__, __LINE__,                            \
                                      std::string("CHECK(") + #cond + ")");          \
    } while (0)

// 检查两值相等（需支持 operator== 与 operator<<）
#define CHECK_EQ(a, b)                                                               \
    do {                                                                             \
        ::thkstest::totalChecks()++;                                                 \
        auto va_ = (a);                                                              \
        auto vb_ = (b);                                                              \
        if (!(va_ == vb_))                                                           \
            ::thkstest::reportFailure(__FILE__, __LINE__,                            \
                                      std::string("CHECK_EQ(" #a ", " #b ") -> ") +  \
                                          ::thkstest::repr(va_) + " != " +           \
                                          ::thkstest::repr(vb_));                    \
    } while (0)

#endif // THKS_TEST_FRAMEWORK_H
