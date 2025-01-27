#include "Server.hpp"

#include "../../../common/Exception.hpp"

#include <arpa/inet.h>
#include <linux/io_uring.h>

[[nodiscard]] constexpr auto socket(const std::source_location sourceLocation = std::source_location::current())
    -> int {
    const int fileDescriptor{socket(AF_INET, SOCK_STREAM, 0)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    return fileDescriptor;
}

constexpr auto setSocketOption(const int fileDescriptor,
                               const std::source_location sourceLocation = std::source_location::current()) -> void {
    constexpr auto option{1};
    if (setsockopt(fileDescriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(option)) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

constexpr auto translateIpAddress(const std::string_view host, in_addr &address,
                                  const std::source_location sourceLocation = std::source_location::current()) -> void {
    if (inet_pton(AF_INET, host.data(), &address) != 1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

constexpr auto bind(const int fileDescriptor, const sockaddr_in &address,
                    const std::source_location sourceLocation = std::source_location::current()) -> void {
    if (bind(fileDescriptor, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

constexpr auto listen(const int fileDescriptor,
                      const std::source_location sourceLocation = std::source_location::current()) -> void {
    if (listen(fileDescriptor, SOMAXCONN) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

auto Server::create(const std::string_view host, const unsigned short port) -> int {
    const int fileDescriptor{socket()};

    setSocketOption(fileDescriptor);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    translateIpAddress(host, address.sin_addr);

    bind(fileDescriptor, address);
    listen(fileDescriptor);

    return fileDescriptor;
}

Server::Server(const int fileDescriptor) noexcept : FileDescriptor{fileDescriptor} {}

auto Server::accept() const noexcept -> Awaiter {
    return Awaiter{
        Submission{this->getFileDescriptor(), IOSQE_FIXED_FILE, IORING_ACCEPT_POLL_FIRST, 0, Submission::Accept{}}
    };
}
