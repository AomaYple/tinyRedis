#include "RingBuffer.hpp"

#include "Ring.hpp"

#include <utility>

RingBuffer::RingBuffer(const std::shared_ptr<Ring> &ring, const unsigned int entries, const int id) :
    ring{ring}, handle{this->ring->setupRingBuffer(entries, id)}, entries{entries}, id{id} {}

RingBuffer::RingBuffer(RingBuffer &&other) noexcept :
    ring{std::move(other.ring)}, handle{std::exchange(other.handle, nullptr)}, entries{other.entries}, id{other.id},
    offset{other.offset} {}

auto RingBuffer::operator=(RingBuffer &&other) noexcept -> RingBuffer & {
    if (this == &other) return *this;

    this->destroy();

    this->ring = std::move(other.ring);
    this->handle = std::exchange(other.handle, nullptr);
    this->entries = other.entries;
    this->id = other.id;
    this->offset = other.offset;

    return *this;
}

RingBuffer::~RingBuffer() { this->destroy(); }

auto RingBuffer::getHandle() const noexcept -> io_uring_buf_ring * { return this->handle; }

auto RingBuffer::getId() const noexcept -> int { return this->id; }

auto RingBuffer::addBuffer(const std::span<std::byte> buffer, const unsigned short index) noexcept -> void {
    io_uring_buf_ring_add(this->handle, buffer.data(), buffer.size(), index, io_uring_buf_ring_mask(this->entries),
                          this->offset++);
}

auto RingBuffer::getAddedBufferCount() noexcept -> int { return std::exchange(this->offset, 0); }

auto RingBuffer::destroy() const -> void {
    if (this->handle != nullptr) this->ring->freeRingBuffer(this->handle, this->entries, this->id);
}
