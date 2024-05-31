#include "log/Exception.hpp"
#include "network/Connection.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <print>

auto shieldSignal(std::source_location sourceLocation = std::source_location::current()) -> void;

auto main() -> int {
    shieldSignal();

    const Connection connection;
    const std::pair peerName{connection.getPeerName()};

    unsigned long id{};
    while (true) {
        const std::string serverInfo{std::format("tinyRedis {}:{}{}{}{}> ", peerName.first, peerName.second,
                                                 id == 0 ? "" : "[", id == 0 ? "" : std::to_string(id),
                                                 id == 0 ? "" : "]")};

        std::string buffer;
        while (buffer.empty()) {
            std::print("{}", serverInfo);
            std::getline(std::cin, buffer);
        }

        if (buffer == "QUIT") break;

        buffer.insert(buffer.cbegin(), sizeof(id), 0);
        *reinterpret_cast<decltype(id) *>(buffer.data()) = id;

        const auto spanBuffer{std::as_bytes(std::span{buffer})};
        connection.send(spanBuffer);

        const std::vector receivedData{connection.receive()};
        id = *reinterpret_cast<const decltype(id) *>(receivedData.data());

        const std::string_view response{reinterpret_cast<const char *>(receivedData.data() + sizeof(id)),
                                        receivedData.size() - sizeof(id)};
        std::println("{}", response);
    }

    return 0;
}

auto shieldSignal(std::source_location sourceLocation) -> void {
    struct sigaction signalAction {};

    signalAction.sa_handler = SIG_IGN;

    if (sigaction(SIGTERM, &signalAction, nullptr) != 0) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }

    if (sigaction(SIGINT, &signalAction, nullptr) != 0) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }
}
