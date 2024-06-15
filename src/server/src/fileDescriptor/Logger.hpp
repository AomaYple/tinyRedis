#pragma once

#include "../../../common/log/Log.hpp"
#include "FileDescriptor.hpp"

#include <queue>

class Logger : public FileDescriptor {
public:
    [[nodiscard]] static auto create(std::string_view filename,
                                     std::source_location sourceLocation = std::source_location::current()) -> int;

    explicit Logger(int fileDescriptor);

    Logger(const Logger &) = delete;

    Logger(Logger &&) = default;

    auto operator=(const Logger &) -> Logger & = delete;

    auto operator=(Logger &&) -> Logger & = delete;

    ~Logger() = default;

    auto push(Log &&log) -> void;

    [[nodiscard]] auto writable() const noexcept -> bool;

    [[nodiscard]] auto write() -> Awaiter;

    auto wrote() noexcept -> void;

private:
    std::queue<Log> logs;
    std::vector<std::byte> data;
};
