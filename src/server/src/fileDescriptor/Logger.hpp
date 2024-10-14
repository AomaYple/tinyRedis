#pragma once

#include "../../../common/Log.hpp"
#include "FileDescriptor.hpp"

class Logger final : public FileDescriptor {
public:
    [[nodiscard]] static auto create(std::string_view filename,
                                     std::source_location sourceLocation = std::source_location::current()) -> int;

    explicit Logger(int fileDescriptor) noexcept;

    Logger(const Logger &) = delete;

    Logger(Logger &&) noexcept = default;

    auto operator=(const Logger &) -> Logger & = delete;

    auto operator=(Logger &&) noexcept -> Logger & = delete;

    ~Logger() override = default;

    auto push(Log &&log) -> void;

    [[nodiscard]] auto isWritable() const noexcept -> bool;

    [[nodiscard]] auto write() -> Awaiter;

    auto wrote() noexcept -> void;

private:
    std::vector<Log> logs;
    std::vector<std::byte> buffer;
};
