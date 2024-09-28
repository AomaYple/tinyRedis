#pragma once

#include <liburing.h>
#include <memory>

class Ring;

class RingBuffer {
public:
    RingBuffer(const std::shared_ptr<Ring> &ring, unsigned int entries, int id);

    RingBuffer(const RingBuffer &) = delete;

    RingBuffer(RingBuffer &&) noexcept;

    auto operator=(const RingBuffer &) -> RingBuffer & = delete;

    auto operator=(RingBuffer &&) noexcept -> RingBuffer &;

    ~RingBuffer();

    [[nodiscard]] auto getHandle() const noexcept -> io_uring_buf_ring *;

    [[nodiscard]] auto getId() const noexcept -> int;

    auto addBuffer(std::span<std::byte> buffer, unsigned short index) noexcept -> void;

    auto getAddedBufferCount() noexcept -> int;

private:
    auto destroy() const -> void;

    std::shared_ptr<Ring> ring;
    io_uring_buf_ring *handle;
    unsigned int entries;
    int id, offset{};
};
