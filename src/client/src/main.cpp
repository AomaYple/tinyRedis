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
    bool isTransaction{};
    std::vector<std::string> transactions;

    while (true) {
        std::print("tinyRedis {}:{}{}{}{}> ", host, port, id == 0 ? "" : "[", id == 0 ? "" : std::to_string(id),
                   id == 0 ? "" : "]");

        std::string inputBuffer;
        std::getline(std::cin, inputBuffer);

        if (inputBuffer.empty()) continue;
        if (inputBuffer == "QUIT") {
            std::println("OK");

            break;
        }
        if (inputBuffer == "MULTI") {
            isTransaction = true;
            std::println("OK");

            continue;
        }
        if (inputBuffer == "DISCARD") {
            isTransaction = false;
            transactions.clear();
            std::println("OK");

            continue;
        }
        if (inputBuffer == "EXEC") {
            for (unsigned long i{}; i < transactions.size(); ++i) {
                connection.send(formatRequest(transactions[i], id));
                const std::vector data{connection.receive()};

                std::println("{}{}", std::to_string(i + 1) + ") ",
                             std::string_view{reinterpret_cast<const char *>(data.data()), data.size()});
            }

            transactions.clear();
            isTransaction = false;
        } else if (isTransaction) {
            transactions.emplace_back(inputBuffer);
            std::println("QUEUED");
        } else {
            connection.send(formatRequest(inputBuffer, id));
            const std::vector data{connection.receive()};

            std::println("{}", std::string_view{reinterpret_cast<const char *>(data.data()), data.size()});
        }
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
    else if (command == "MGET") commandType = Command::mget;
    else if (command == "SETNX") commandType = Command::setnx;
    else if (command == "SETRANGE") commandType = Command::setRange;
    else if (command == "STRLEN") commandType = Command::strlen;
    else if (command == "MSET") commandType = Command::mset;
    else if (command == "MSETNX") commandType = Command::msetnx;
    else if (command == "INCR") commandType = Command::incr;

    std::vector buffer{std::byte{std::to_underlying(commandType)}};

    buffer.resize(buffer.size() + sizeof(id));
    *reinterpret_cast<std::remove_reference_t<decltype(id)> *>(buffer.data() + buffer.size() - sizeof(id)) = id;

    const auto spanStatement{std::as_bytes(std::span{statement})};
    buffer.insert(buffer.cend(), spanStatement.cbegin(), spanStatement.cend());

    return buffer;
}

