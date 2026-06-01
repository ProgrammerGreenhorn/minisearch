#include "minisearch/util/Logger.hpp"

#include <iostream>

namespace minisearch::util {

namespace {

const char* label(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:
        return "info";
    case LogLevel::Warning:
        return "warning";
    case LogLevel::Error:
        return "error";
    }
    return "log";
}

const char* color(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:
        return "\033[32m";
    case LogLevel::Warning:
        return "\033[33m";
    case LogLevel::Error:
        return "\033[31m";
    }
    return "\033[0m";
}

} // namespace

void Logger::log(LogLevel level, const std::string& message)
{
    auto& stream = level == LogLevel::Error ? std::cerr : std::cout;
    stream << color(level) << '[' << label(level) << "] " << message << "\033[0m\n";
}

void Logger::info(const std::string& message)
{
    log(LogLevel::Info, message);
}

void Logger::warning(const std::string& message)
{
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message)
{
    log(LogLevel::Error, message);
}

} // namespace minisearch::util
