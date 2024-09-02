#pragma once

#include "FileDescriptor.hpp"

#include <source_location>

class Timer : public FileDescriptor {
public:
    [[nodiscard]] static auto create() -> int;

    explicit Timer(int fileDescriptor) noexcept;

    Timer(const Timer &) = delete;

    Timer(Timer &&) noexcept = default;

    auto operator=(const Timer &) -> Timer & = delete;

    auto operator=(Timer &&) -> Timer & = delete;

    ~Timer() = default;

    [[nodiscard]] auto timing() noexcept -> Awaiter;

private:
    [[nodiscard]] static auto
        createTimerFileDescriptor(std::source_location sourceLocation = std::source_location::current()) -> int;

    static auto setTime(int fileDescriptor, std::source_location sourceLocation = std::source_location::current())
        -> void;

    unsigned long timeout{};
};
