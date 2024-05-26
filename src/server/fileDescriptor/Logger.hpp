#pragma once

#include "../log/Log.hpp"
#include "FileDescriptor.hpp"

#include <queue>

class Logger : public FileDescriptor {
public:
    [[nodiscard]] static auto create(std::source_location sourceLocation = std::source_location::current()) -> int;

    explicit Logger(int fileDescriptor);

    auto push(Log &&log) -> void;

    [[nodiscard]] auto writable() const noexcept -> bool;

    [[nodiscard]] auto write() -> Awaiter;

    auto wrote() noexcept -> void;

private:
    std::queue<Log> logs;
    std::vector<std::byte> data;
};
