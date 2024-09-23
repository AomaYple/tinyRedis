#pragma once

#include <source_location>
#include <span>
#include <string_view>
#include <vector>

class Connection {
public:
    Connection(std::string_view host, unsigned short port);

    Connection(const Connection &) = delete;

    Connection(Connection &&) noexcept;

    auto operator=(const Connection &) -> Connection & = delete;

    auto operator=(Connection &&) noexcept -> Connection &;

    ~Connection();

    auto send(std::span<const std::byte> data,
              std::source_location sourceLocation = std::source_location::current()) const -> void;

    [[nodiscard]] auto receive(std::source_location sourceLocation = std::source_location::current()) const
        -> std::vector<std::byte>;

private:
    auto close(std::source_location sourceLocation = std::source_location::current()) const -> void;

    int fileDescriptor;
};
