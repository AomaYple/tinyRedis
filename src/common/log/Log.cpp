#include "Log.hpp"

#include <chrono>
#include <utility>

Log::Log(const Level level, std::string &&text, const std::source_location sourceLocation,
         const std::chrono::system_clock::time_point timestamp, const std::jthread::id joinThreadId) noexcept :
    level{level}, text{std::move(text)}, sourceLocation{sourceLocation}, timestamp{timestamp},
    joinThreadId{joinThreadId} {}

auto Log::toString() const -> std::string {
    static constexpr std::array<const std::string_view, 4> levels{"info", "warn", "error", "fatal"};

    return std::format("{} {} {} {}:{}:{}:{} {}\n", levels[std::to_underlying(this->level)], this->timestamp,
                       this->joinThreadId, this->sourceLocation.file_name(), this->sourceLocation.line(),
                       this->sourceLocation.column(), this->sourceLocation.function_name(), this->text);
}

auto Log::toByte() const -> std::vector<std::byte> {
    const auto stringLog{this->toString()};
    const auto spanLog{std::as_bytes(std::span{stringLog})};

    return {spanLog.cbegin(), spanLog.cend()};
}
