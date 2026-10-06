#include "Logger.h"
#include <iostream>

namespace Thks {

Logger::Logger() : stream(&std::cout) {}

Logger::Logger(std::ostream& streamRef) : stream(&streamRef) {}

Logger::Logger(std::shared_ptr<std::ostream> ownedSink) : stream(ownedSink.get()), sink(std::move(ownedSink)) {}

void Logger::setStream(std::ostream& streamRef) {
    stream = &streamRef;
    sink.reset();
}

void Logger::setSink(std::shared_ptr<std::ostream> ownedSink) {
    sink = std::move(ownedSink);
    stream = sink.get();
}

void Logger::log(const std::string& message) {
    (*stream) << message << std::endl;
}

std::ostream& Logger::raw() {
    return *stream;
}

} // namespace Thks
