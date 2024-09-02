#pragma once

#include "FileDescriptor.hpp"

class Client : public FileDescriptor {
public:
    explicit Client(int fileDescriptor) noexcept;

    Client(const Client &) = delete;

    Client(Client &&) noexcept = default;

    auto operator=(const Client &) -> Client & = delete;

    auto operator=(Client &&) -> Client & = delete;

    ~Client() = default;

    [[nodiscard]] auto receive(int ringBufferId) const noexcept -> Awaiter;

    [[nodiscard]] auto send(std::span<const std::byte> data) const noexcept -> Awaiter;
};
