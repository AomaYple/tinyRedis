#include "Connection.hpp"

#include "../../../common/log/Exception.hpp"

#include <arpa/inet.h>
#include <utility>

[[nodiscard]] constexpr auto socket(const std::source_location sourceLocation = std::source_location::current())
    -> int {
    const int fileDescriptor{::socket(AF_INET, SOCK_STREAM, 0)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    return fileDescriptor;
}

constexpr auto translateIpAddress(const std::string_view host, in_addr &address,
                                  const std::source_location sourceLocation = std::source_location::current()) -> void {
    if (inet_pton(AF_INET, host.data(), &address) != 1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

constexpr auto connect(const int fileDescriptor, const sockaddr_in &address,
                       const std::source_location sourceLocation = std::source_location::current()) -> void {
    if (connect(fileDescriptor, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

auto Connection::close(const std::source_location sourceLocation) const -> void {
    if (this->fileDescriptor != -1 && ::close(this->fileDescriptor) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

Connection::Connection(const std::string_view host, const unsigned short port) :
    fileDescriptor{[host, port] {
        const int fileDescriptor{socket()};

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        translateIpAddress(host, address.sin_addr);

        connect(fileDescriptor, address);

        return fileDescriptor;
    }()} {}

Connection::Connection(Connection &&other) noexcept : fileDescriptor{std::exchange(other.fileDescriptor, -1)} {}

auto Connection::operator=(Connection &&other) noexcept -> Connection & {
    if (this == &other) return *this;

    this->close();

    this->fileDescriptor = std::exchange(other.fileDescriptor, -1);

    return *this;
}

Connection::~Connection() { this->close(); }

auto Connection::send(const std::span<const std::byte> data, const std::source_location sourceLocation) const -> void {
    if (const long result{::send(this->fileDescriptor, data.data(), data.size(), 0)}; result <= 0) {
        throw Exception{
            Log{Log::Level::fatal,
                result == 0 ? "connection closed" : std::error_code{errno, std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Connection::receive(const std::source_location sourceLocation) const -> std::vector<std::byte> {
    std::vector<std::byte> buffer;

    while (true) {
        std::vector<std::byte> subBuffer{1024};

        if (const long result{
                recv(this->fileDescriptor, subBuffer.data(), subBuffer.size(), !buffer.empty() ? MSG_DONTWAIT : 0)};
            result > 0) {
            subBuffer.resize(result);
            buffer.insert(buffer.cend(), subBuffer.cbegin(), subBuffer.cend());
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;

            throw Exception{
                Log{Log::Level::fatal,
                    result == 0 ? "connection closed" : std::error_code{errno, std::generic_category()}.message(),
                    sourceLocation}
            };
        }
    }

    return buffer;
}
