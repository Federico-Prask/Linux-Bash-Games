#include "Platform.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#ifndef NOMINMAX // windows.h 的 min/max 宏会干扰 <algorithm>，防御性屏蔽
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Thks {

namespace {

// 解码一个 UTF-8 码点；失败时前进一个字节并返回 false
bool decodeUtf8(const std::string& s, size_t& i, unsigned int& cp) {
    if (i >= s.size()) return false;
    unsigned char c = static_cast<unsigned char>(s[i]);
    int len;
    unsigned int v;
    if (c < 0x80) {
        cp = c;
        i += 1;
        return true;
    } else if ((c & 0xE0) == 0xC0) {
        len = 2; v = c & 0x1Fu;
    } else if ((c & 0xF0) == 0xE0) {
        len = 3; v = c & 0x0Fu;
    } else if ((c & 0xF8) == 0xF0) {
        len = 4; v = c & 0x07u;
    } else {
        i += 1;
        return false;
    }
    if (i + static_cast<size_t>(len) > s.size()) {
        i += 1;
        return false;
    }
    for (int k = 1; k < len; ++k) {
        unsigned char cc = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
        if ((cc & 0xC0) != 0x80) {
            i += 1;
            return false;
        }
        v = (v << 6) | (cc & 0x3Fu);
    }
    i += static_cast<size_t>(len);
    cp = v;
    return true;
}

// 近似 wcwidth：东亚宽字符 / 全角 / 常用 emoji 记 2 列，控制字符记 0 列
bool isWideCodepoint(unsigned int cp) {
    if (cp < 0x20) return false;                                   // 控制字符
    if (cp >= 0x1100 && cp <= 0x115F) return true;                 // Hangul Jamo
    if (cp >= 0x2E80 && cp <= 0xA4CF) return true;                 // CJK 部首/符号/标点/汉字/假名/谚文
    if (cp >= 0xAC00 && cp <= 0xD7A3) return true;                 // 谚文音节
    if (cp >= 0xF900 && cp <= 0xFAFF) return true;                 // CJK 兼容表意
    if (cp >= 0xFE10 && cp <= 0xFE6F) return true;                 // 竖排形式 / CJK 兼容形式
    if (cp >= 0xFF00 && cp <= 0xFF60) return true;                 // 全角 ASCII
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return true;                 // 全角符号
    if (cp >= 0x1F300 && cp <= 0x1FAFF) return true;               // emoji（如 💥🆘💀）
    if (cp >= 0x20000 && cp <= 0x3FFFD) return true;               // CJK 扩展 B-F
    return false;
}

} // namespace

void Platform::init() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

void Platform::clearScreen() {
    std::cout << "\033[2J\033[1;1H" << std::flush;
}

void Platform::pause() {
    std::cout << "\n按回车键继续...";
    std::cin.ignore(1024, '\n');
    std::cin.get();
}

void Platform::sleepMs(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

int Platform::displayWidth(const std::string& text) {
    int width = 0;
    size_t i = 0;
    unsigned int cp = 0;
    while (decodeUtf8(text, i, cp)) {
        width += isWideCodepoint(cp) ? 2 : 1;
    }
    return width;
}

std::string Platform::padRight(const std::string& text, int width) {
    std::string out = text;
    for (int k = displayWidth(text); k < width; ++k) out += ' ';
    return out;
}

} // namespace Thks
