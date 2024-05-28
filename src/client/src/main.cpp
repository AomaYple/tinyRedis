#include "log/Exception.hpp"
#include "network/Connection.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <print>

auto shieldSignal(std::source_location sourceLocation = std::source_location::current()) -> void;

auto main() -> int {
    const Connection connection;
    const std::pair<std::string, std::string> peerName{connection.getPeerName()};
    const std::string serverInfo{std::format("tinyRedis {}:{}> ", peerName.first, peerName.second)};

    shieldSignal();

    while (true) {
        std::string buffer;
        while (buffer.empty()) {
            std::print("{}", serverInfo);
            std::getline(std::cin, buffer);
        }

        if (buffer == "exit") {
            std::println("bye!");
            break;
        }

        const auto spanBuffer{std::as_bytes(std::span{buffer})};
        connection.send(spanBuffer);

        const std::vector<std::byte> receivedData{connection.receive()};
        const std::string_view response{reinterpret_cast<const char *>(receivedData.data()), receivedData.size()};
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
