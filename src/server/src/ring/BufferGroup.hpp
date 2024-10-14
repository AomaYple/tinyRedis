#pragma once

#include <thread>
#include <vector>

class BufferGroup {
public:
    explicit BufferGroup(unsigned int count);

    [[nodiscard]] auto getBuffer(unsigned short index) noexcept -> std::span<std::byte>;

private:
    std::vector<std::byte> group{std::bit_ceil(2 * 1024 * 1024 / std::thread::hardware_concurrency())};
    long size;
};
