#include "RingBuffer.hpp"

#include "Ring.hpp"

#include <utility>

RingBuffer::RingBuffer(const std::shared_ptr<Ring> &ring, const unsigned int entries, const unsigned int bufferSize,
                       const int id) :
    ring{ring}, handle{this->ring->setupRingBuffer(entries, id)}, bufferGroup{entries * bufferSize},
    bufferSize{bufferSize}, id{id}, mask{io_uring_buf_ring_mask(entries)} {
    for (unsigned short i{}; i != entries; ++i) this->add(i);

    this->ring->advance(this->getHandle(), 0, this->getAddedBufferCount());
}

RingBuffer::RingBuffer(RingBuffer &&other) noexcept :
    ring{std::move(other.ring)}, handle{std::exchange(other.handle, nullptr)},
    bufferGroup{std::move(other.bufferGroup)}, bufferSize{other.bufferSize}, id{other.id}, mask{other.mask},
    offset{other.offset} {}

auto RingBuffer::operator=(RingBuffer &&other) noexcept -> RingBuffer & {
    if (this == &other) return *this;

    this->destroy();

    this->ring = std::move(other.ring);
    this->handle = std::exchange(other.handle, nullptr);
    this->bufferGroup = std::move(other.bufferGroup);
    this->bufferSize = other.bufferSize;
    this->id = other.id;
    this->mask = other.mask;
    this->offset = other.offset;

    return *this;
}

RingBuffer::~RingBuffer() { this->destroy(); }

auto RingBuffer::getHandle() const noexcept -> io_uring_buf_ring * { return this->handle; }

auto RingBuffer::getId() const noexcept -> int { return this->id; }

auto RingBuffer::readFromBuffer(const unsigned short index, const unsigned int dataSize) -> std::span<const std::byte> {
    this->add(index);
    const auto bufferOffset{this->bufferGroup.cbegin() + index * this->bufferSize};

    return {bufferOffset, bufferOffset + dataSize};
}

auto RingBuffer::getAddedBufferCount() noexcept -> int { return std::exchange(this->offset, 0); }

auto RingBuffer::destroy() const -> void {
    if (this->handle != nullptr)
        this->ring->freeRingBuffer(this->handle, this->bufferGroup.size() / this->bufferSize, this->id);
}

auto RingBuffer::add(const unsigned short index) noexcept -> void {
    io_uring_buf_ring_add(this->handle, this->bufferGroup.data() + index * this->bufferSize, this->bufferSize, index,
                          this->mask, this->offset++);
}
