#pragma once

#include "FileDescriptor.hpp"

#include <chrono>
#include <source_location>
#include <unordered_map>

class Timer : public FileDescriptor {
public:
    [[nodiscard]] static auto create() -> int;

    explicit Timer(int fileDescriptor) noexcept;

    [[nodiscard]] auto timing() noexcept -> Awaiter;

    auto add(int fileDescriptor, std::chrono::seconds seconds) -> void;

    auto update(int fileDescriptor, std::chrono::seconds seconds) -> void;

    auto remove(int fileDescriptor) -> void;

    [[nodiscard]] auto clearTimeout() -> std::vector<int>;

private:
    [[nodiscard]] static auto
        createTimerFileDescriptor(std::source_location sourceLocation = std::source_location::current()) -> int;

    static auto setTime(int fileDescriptor, std::source_location sourceLocation = std::source_location::current())
        -> void;

    unsigned long timeout{};
    std::chrono::seconds now{};
    std::array<std::unordered_map<int, unsigned long>, 61> wheel;
    std::unordered_map<int, std::chrono::seconds> location;
};
