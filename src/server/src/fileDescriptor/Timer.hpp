#pragma once

#include "FileDescriptor.hpp"

class Timer final : public FileDescriptor {
public:
    [[nodiscard]] static auto create() -> int;

    explicit Timer(int fileDescriptor) noexcept;

    Timer(const Timer &) = delete;

    constexpr Timer(Timer &&) noexcept = default;

    auto operator=(const Timer &) -> Timer & = delete;

    auto operator=(Timer &&) noexcept -> Timer & = delete;

    constexpr ~Timer() override = default;

    [[nodiscard]] auto timing() noexcept -> Awaiter;

private:
    unsigned long timeout{};
};
