#pragma once

#include <liburing.h>
#include <memory>
#include <vector>

class Ring;

class RingBuffer {
public:
    RingBuffer(const std::shared_ptr<Ring> &ring, unsigned int entries, unsigned int subBufferSize, int id);

    RingBuffer(const RingBuffer &) = delete;

    RingBuffer(RingBuffer &&) noexcept;

    auto operator=(const RingBuffer &) -> RingBuffer & = delete;

    auto operator=(RingBuffer &&) noexcept -> RingBuffer &;

    ~RingBuffer();

    [[nodiscard]] auto getHandle() const noexcept -> io_uring_buf_ring *;

    [[nodiscard]] auto getId() const noexcept -> int;

    [[nodiscard]] auto readFromBuffer(unsigned short index, unsigned int dataSize) -> std::span<const std::byte>;

    auto getAddedBufferCount() noexcept -> int;

private:
    auto destroy() const -> void;

    auto add(unsigned short index) noexcept -> void;

    std::shared_ptr<Ring> ring;
    io_uring_buf_ring *handle;
    std::vector<std::byte> buffer;
    unsigned int subBufferSize;
    int id, mask, offset{};
};
