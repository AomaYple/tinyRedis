#include "../../common/command/Command.hpp"
#include "../../common/log/Exception.hpp"
#include "network/Connection.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <print>
#include <utility>

auto shieldSignal(std::source_location sourceLocation = std::source_location::current()) -> void;
auto formatRequest(std::string_view data, unsigned long &id) -> std::vector<std::byte>;

auto main() -> int {
    shieldSignal();

    const Connection connection;
    const std::pair peerName{connection.getPeerName()};

    unsigned long id{};
    while (true) {
        const std::string serverInformation{std::format("tinyRedis {}:{}{}{}{}> ", peerName.first, peerName.second,
                                                        id == 0 ? "" : "[", id == 0 ? "" : std::to_string(id),
                                                        id == 0 ? "" : "]")};

        std::string buffer;
        while (buffer.empty()) {
            std::print("{}", serverInformation);
            std::getline(std::cin, buffer);
        }
        if (buffer == "QUIT") break;

        connection.send(formatRequest(buffer, id));

        const std::vector data{connection.receive()};
        const std::string_view response{reinterpret_cast<const char *>(data.data()), data.size()};
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

auto formatRequest(std::string_view data, unsigned long &id) -> std::vector<std::byte> {
    const unsigned long result{data.find(' ')};
    const auto command{data.substr(0, result)};
    auto statement{data.substr(result + 1)};

    Command commandType{};
    if (command == "SELECT") {
        commandType = Command::select;
        id = std::stoul(std::string{statement});
        statement = {};
    } else if (command == "EXISTS") commandType = Command::exists;
    else if (command == "GET") commandType = Command::get;
    else if (command == "DEL") commandType = Command::del;

    std::vector buffer{std::byte{std::to_underlying(commandType)}};

    buffer.resize(buffer.size() + sizeof(unsigned long));
    *reinterpret_cast<unsigned long *>(buffer.data() + buffer.size() - sizeof(unsigned long)) = id;

    const auto spanStatement{std::as_bytes(std::span{statement})};
    buffer.insert(buffer.cend(), spanStatement.cbegin(), spanStatement.cend());

    return buffer;
}

