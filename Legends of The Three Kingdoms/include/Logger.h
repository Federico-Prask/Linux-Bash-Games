#ifndef THKS_LOGGER_H
#define THKS_LOGGER_H

#include <string>
#include <ostream>
#include <memory>

namespace Thks {

// =====================================================================
//  游戏日志输出层
//
//  引擎（GameEngine / Interaction 等）的所有文本都经由 Logger 输出，
//  引擎逻辑本身不再直接依赖 std::cout，从而：
//    - 可重定向到字符串流做单元测试（断言日志内容）
//    - 可替换为 GUI / 联机 / 回放等其他前端（观察者式解耦）
//  输出风格统一为黑白纯文本，无 ANSI 颜色转义序列。
// =====================================================================
class Logger {
public:
    // 默认绑定标准输出
    Logger();
    // 绑定外部流（引用由调用方保证存活）
    explicit Logger(std::ostream& stream);
    // 绑定并持有 sink（如 std::ostringstream），适合测试捕获
    explicit Logger(std::shared_ptr<std::ostream> ownedSink);

    void setStream(std::ostream& stream);
    void setSink(std::shared_ptr<std::ostream> ownedSink);

    // 输出一行日志（自动换行刷新）
    void log(const std::string& message);

    // 底层流访问：支持 log() << "a" << b << std::endl 的多段风格
    std::ostream& raw();

private:
    std::ostream* stream;
    std::shared_ptr<std::ostream> sink; // 可选：持有的流所有权
};

} // namespace Thks

#endif // THKS_LOGGER_H
