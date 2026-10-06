#ifndef THKS_PLATFORM_H
#define THKS_PLATFORM_H

#include <string>

namespace Thks {

class Platform {
public:
    // Windows 控制台 UTF-8 / VT 初始化（Linux 下为空操作）
    static void init();
    static void clearScreen();
    static void pause();
    static void sleepMs(int milliseconds);

    // ---- 文本显示宽度（终端对齐用） ----
    // 按 UTF-8 码点计算终端列宽：CJK/全角/常用 emoji 记 2 列，其余记 1 列。
    // 替代旧实现中 size()/3*2 的粗略估算，对任意混合文本均正确。
    static int displayWidth(const std::string& text);
    // 右侧补空格，把文本补齐到指定显示宽度（超宽则原样返回）
    static std::string padRight(const std::string& text, int width);

    // 输出风格统一为黑白（无 ANSI 彩色转义序列），原 colorize() 已移除。
    // 需要强调时使用【】、!、▲ 等纯文本标记。
};

} // namespace Thks

#endif // THKS_PLATFORM_H
