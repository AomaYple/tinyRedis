#pragma once

#include <netinet/in.h>
#include <source_location>

class Connection {
public:
    Connection();

    Connection(const Connection &) = delete;

    Connection(Connection &&) noexcept;

    auto operator=(const Connection &) -> Connection & = delete;

    auto operator=(Connection &&) noexcept -> Connection &;

    ~Connection();

    [[nodiscard]] auto getId() const noexcept -> unsigned char;

    auto setId(unsigned char id) noexcept -> void;

private:
    static auto socket(std::source_location sourceLocation = std::source_location::current()) -> int;

    static auto translateIpAddress(in_addr &address,
                                   std::source_location sourceLocation = std::source_location::current()) -> void;

    static auto connect(int fileDescriptor, const sockaddr_in &address,
                        std::source_location sourceLocation = std::source_location::current()) -> void;

    auto close(std::source_location sourceLocation = std::source_location::current()) const -> void;

    int fileDescriptor;
    unsigned char id;
};
