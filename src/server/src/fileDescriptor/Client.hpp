#pragma once

#include "../database/Context.hpp"
#include "FileDescriptor.hpp"

class Client final : public FileDescriptor {
public:
    explicit Client(int fileDescriptor) noexcept;

    Client(const Client &) = delete;

    constexpr Client(Client &&) noexcept = default;

    auto operator=(const Client &) -> Client & = delete;

    auto operator=(Client &&) noexcept -> Client & = delete;

    constexpr ~Client() override = default;

    [[nodiscard]] auto receive(int ringBufferId) const noexcept -> Awaiter;

    [[nodiscard]] auto send(std::span<const std::byte> data) const noexcept -> Awaiter;

    [[nodiscard]] auto getContext() noexcept -> Context &;

private:
    Context context;
};
