#include "BufferGroup.hpp"

BufferGroup::BufferGroup(const unsigned int count) : size{static_cast<long>(this->group.size()) / count} {}

auto BufferGroup::getBuffer(const unsigned short index) -> std::span<std::byte> {
    const auto offset{this->group.begin() + index * this->size};

    return {offset, offset + this->size};
}
