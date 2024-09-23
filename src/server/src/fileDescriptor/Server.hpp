#pragma once

#include "FileDescriptor.hpp"

#include <string_view>

class Server final : public FileDescriptor {
public:
    [[nodiscard]] static auto create(std::string_view host, unsigned short port) -> int;

    explicit Server(int fileDescriptor) noexcept;

    Server(const Server &) = delete;

    constexpr Server(Server &&) noexcept = default;

    auto operator=(const Server &) -> Server & = delete;

    auto operator=(Server &&) noexcept -> Server & = delete;

    constexpr ~Server() override = default;

    [[nodiscard]] auto accept() const noexcept -> Awaiter;
};
