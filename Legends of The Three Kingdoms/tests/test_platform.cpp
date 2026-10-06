#include "test_framework.h"
#include "Platform.h"
using namespace Thks;

TEST("platform/ascii_width") {
    CHECK_EQ(Platform::displayWidth(""), 0);
    CHECK_EQ(Platform::displayWidth("abc"), 3);
    CHECK_EQ(Platform::displayWidth("(10)"), 4);
}

TEST("platform/cjk_width") {
    CHECK_EQ(Platform::displayWidth("关羽"), 4);      // 2 个汉字 = 4 列
    CHECK_EQ(Platform::displayWidth("界关羽"), 6);    // 3 个汉字 = 6 列
    CHECK_EQ(Platform::displayWidth("桀骜的野心家"), 12);
    CHECK_EQ(Platform::displayWidth("武将：钟会"), 10); // 全角冒号 2 列
}

TEST("platform/mixed_and_emoji_width") {
    CHECK_EQ(Platform::displayWidth("HP:4/5"), 6);
    CHECK_EQ(Platform::displayWidth("💥"), 2);   // emoji 按宽字符计
    CHECK_EQ(Platform::displayWidth("[♠ 7 杀]"), 8); // ♠(U+2660) 按窄字符计
}

TEST("platform/pad_right") {
    CHECK_EQ(Platform::displayWidth(Platform::padRight("关羽", 10)), 10);
    CHECK_EQ(Platform::displayWidth(Platform::padRight("界关羽", 6)), 6); // 恰好无填充
    CHECK_EQ(Platform::padRight("abc", 6), std::string("abc   "));
    // 超宽原样返回
    CHECK_EQ(Platform::padRight("桀骜的野心家", 6), std::string("桀骜的野心家"));
}
