#include "Connection.hpp"

#include "../log/Exception.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <utility>

Connection::Connection() :
    fileDescriptor{[] {
        const int fileDescriptor{socket()};

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(8080);
        translateIpAddress(address.sin_addr);

        connect(fileDescriptor, address);

        return fileDescriptor;
    }()},
    id{} {}

Connection::Connection(Connection &&other) noexcept :
    fileDescriptor{std::exchange(other.fileDescriptor, -1)}, id{other.id} {}

auto Connection::operator=(Connection &&other) noexcept -> Connection & {
    if (this == &other) return *this;

    this->close();

    this->fileDescriptor = std::exchange(other.fileDescriptor, -1);
    this->id = other.id;

    return *this;
}

Connection::~Connection() { this->close(); }

auto Connection::getId() const noexcept -> unsigned char { return this->id; }

auto Connection::setId(unsigned char id) noexcept -> void { this->id = id; }

auto Connection::socket(std::source_location sourceLocation) -> int {
    const int fileDescriptor{::socket(AF_INET, SOCK_STREAM, 0)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, "socket failed: " + std::string{std::strerror(errno)}, sourceLocation}
        };
    }

    return fileDescriptor;
}

auto Connection::translateIpAddress(in_addr &address, std::source_location sourceLocation) -> void {
    if (inet_pton(AF_INET, "127.0.0.1", &address) != 1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }
}

auto Connection::connect(int fileDescriptor, const sockaddr_in &address, std::source_location sourceLocation) -> void {
    if (::connect(fileDescriptor, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        throw Exception{
            Log{Log::Level::fatal, std::to_string(fileDescriptor) + "connect failed: " + std::strerror(errno),
                sourceLocation}
        };
    }
}

auto Connection::close(std::source_location sourceLocation) const -> void {
    if (::close(this->fileDescriptor) != 0) {
        throw Exception{
            Log{Log::Level::fatal, std::to_string(this->fileDescriptor) + "close failed: " + std::strerror(errno),
                sourceLocation}
        };
    }
}
