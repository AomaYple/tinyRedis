#include "log/Exception.hpp"
#include "network/Connection.hpp"
#include "parse/parse.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <print>

auto shieldSignal(std::source_location sourceLocation = std::source_location::current()) -> void;

auto main() -> int {
    shieldSignal();

    const Connection connection;
    const std::pair<std::string, std::string> peerName{connection.getPeerName()};

    unsigned char id{};
    while (true) {
        const std::string serverInfo{std::format("tinyRedis {}:{}[{}]> ", peerName.first, peerName.second, id)};

        std::string buffer;
        while (buffer.empty()) {
            std::print("{}", serverInfo);
            std::getline(std::cin, buffer);
        }

        if (buffer == "QUIT") break;

        buffer.insert(buffer.cbegin(), ' ');
        buffer.insert(buffer.cbegin(), static_cast<char>(id));

        const auto spanBuffer{std::as_bytes(std::span{buffer})};
        connection.send(spanBuffer);

        const std::pair<unsigned char, std::string> response{parse::parse(connection.receive())};

        id = response.first;
        std::println("{}", response.second);
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
