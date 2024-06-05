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
    const auto [host, port]{connection.getPeerName()};

    unsigned long id{};
    while (true) {
        std::print("tinyRedis {}:{}{}{}{}> ", host, port, id == 0 ? "" : "[", id == 0 ? "" : std::to_string(id),
                   id == 0 ? "" : "]");

        std::string buffer;
        std::getline(std::cin, buffer);

        if (buffer.empty()) continue;

        if (buffer == "QUIT") {
            std::println("OK");
            return 0;
        }

        connection.send(formatRequest(buffer, id));

        const std::vector data{connection.receive()};
        std::println("{}", std::string_view{reinterpret_cast<const char *>(data.data()), data.size()});
    }

    return 0;
}

auto shieldSignal(const std::source_location sourceLocation) -> void {
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
    } else if (command == "DEL") commandType = Command::del;
    else if (command == "DUMP") commandType = Command::dump;
    else if (command == "EXISTS") commandType = Command::exists;
    else if (command == "MOVE") commandType = Command::move;
    else if (command == "RENAME") commandType = Command::rename;
    else if (command == "RENAMENX") commandType = Command::renamenx;
    else if (command == "TYPE") commandType = Command::type;
    else if (command == "SET") commandType = Command::set;
    else if (command == "GET") commandType = Command::get;
    else if (command == "GETRANGE") commandType = Command::getRange;
    else if (command == "GETSET") commandType = Command::getSet;
    else if (command == "MGET") commandType = Command::mget;
    else if (command == "SETNX") commandType = Command::setnx;
    else if (command == "SETRANGE") commandType = Command::setRange;
    else if (command == "STRLEN") commandType = Command::strlen;

    std::vector buffer{std::byte{std::to_underlying(commandType)}};

    buffer.resize(buffer.size() + sizeof(unsigned long));
    *reinterpret_cast<unsigned long *>(buffer.data() + buffer.size() - sizeof(unsigned long)) = id;

    const auto spanStatement{std::as_bytes(std::span{statement})};
    buffer.insert(buffer.cend(), spanStatement.cbegin(), spanStatement.cend());

    return buffer;
}

