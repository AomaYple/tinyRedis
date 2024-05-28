#pragma once

#include "FileDescriptor.hpp"

#include <source_location>
#include <unordered_map>
#include <vector>

class Timer : public FileDescriptor {
public:
    [[nodiscard]] static auto create() -> int;

    explicit Timer(int fileDescriptor) noexcept;

    [[nodiscard]] auto timing() noexcept -> Awaiter;

    auto add(int fileDescriptor, unsigned long seconds) -> void;

    auto update(int fileDescriptor, unsigned long seconds) -> void;

    auto remove(int fileDescriptor) -> void;

    [[nodiscard]] auto clearTimeout() -> std::vector<int>;

private:
    [[nodiscard]] static auto
        createTimerFileDescriptor(std::source_location sourceLocation = std::source_location::current()) -> int;

    static auto setTime(int fileDescriptor, std::source_location sourceLocation = std::source_location::current())
        -> void;

    unsigned long timeout{};
    unsigned char now{};
    std::array<std::unordered_map<int, unsigned long>, 61> wheel;
    std::unordered_map<int, unsigned char> location;
};
