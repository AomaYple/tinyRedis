#pragma once

#include "FileDescriptor.hpp"

class Client : public FileDescriptor {
public:
    explicit Client(int fileDescriptor) noexcept;

    [[nodiscard]] auto receive(int ringBufferId) const noexcept -> Awaiter;

    [[nodiscard]] auto send(std::span<const std::byte> data) const noexcept -> Awaiter;
};
