#pragma once

#include "FileDescriptor.hpp"

class Client final : public FileDescriptor {
public:
    explicit Client(int fileDescriptor) noexcept;

    Client(const Client &) = delete;

    Client(Client &&) noexcept = default;

    auto operator=(const Client &) -> Client & = delete;

    auto operator=(Client &&) noexcept -> Client & = delete;

    ~Client() override = default;

    [[nodiscard]] auto receive(int ringBufferId) const noexcept -> Awaiter;

    [[nodiscard]] auto send(std::span<const std::byte> data) const noexcept -> Awaiter;
};
