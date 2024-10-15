#include "../../common/Answer.hpp"
#include "../../common/Reply.hpp"
#include "network/Connection.hpp"

#include <iostream>
#include <print>

constexpr auto printReply(const Reply &reply, unsigned long &databaseIndex, bool &isTransaction,
                          const std::string &leadSpace) -> void {
    databaseIndex = reply.getDatabaseIndex();
    isTransaction = reply.getIsTransaction();

    switch (reply.getType()) {
        case Reply::Type::nil:
            std::println("(nil)");
            break;
        case Reply::Type::integer:
            std::println("(integer) {}", reply.getInteger());
            break;
        case Reply::Type::error:
            std::println("(error) {}", reply.getString());
            break;
        case Reply::Type::status:
            std::println("{}", reply.getString());
            break;
        case Reply::Type::string:
            std::println(R"("{}")", reply.getString());
            break;
        case Reply::Type::array:
            const std::span replies{reply.getArray()};
            if (!replies.empty()) {
                for (unsigned long i{}; i != replies.size(); ++i) {
                    const std::string index{std::to_string(i + 1) + ") "};
                    std::print("{}{}", i != 0 ? leadSpace : std::string_view{}, index);

                    printReply(replies[i], databaseIndex, isTransaction, leadSpace + std::string(index.size(), ' '));
                }
            } else std::println("(empty array)");

            break;
    }
}

auto main() -> int {
    constexpr std::string_view host{"127.0.0.1"};
    constexpr unsigned short port{9090};
    const Connection connection{host, port};

    unsigned long databaseIndex{};
    bool isTransaction{};
    while (true) {
        std::string stringDatabaseIndex;
        if (databaseIndex != 0) stringDatabaseIndex = '[' + std::to_string(databaseIndex) + ']';

        std::print("{}:{}{}{}> ", host, port, stringDatabaseIndex, isTransaction ? "(TX)" : "");

        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) continue;
        if (input == "QUIT") break;

        connection.send(Answer{std::move(input)}.serialize());

        printReply(Reply{connection.receive()}, databaseIndex, isTransaction, std::string{});
    }

    return 0;
}
