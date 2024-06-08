#pragma once

#include "Skiplist.hpp"

#include <shared_mutex>
#include <source_location>

class Database {
public:
    Database(unsigned long id, std::span<const std::byte> data);

    Database(const Database &) = delete;

    Database(Database &&) noexcept;

    auto operator=(const Database &) -> Database = delete;

    auto operator=(Database &&) noexcept -> Database &;

    ~Database() = default;

    [[nodiscard]] auto serialize() -> std::vector<std::byte>;

    [[nodiscard]] auto del(std::string_view keys) -> std::vector<std::byte>;

    [[nodiscard]] auto dump(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto exists(std::string_view keys) -> std::vector<std::byte>;

    [[nodiscard]] auto move(std::unordered_map<unsigned long, Database> &databases, std::string_view statement)
        -> std::vector<std::byte>;

    [[nodiscard]] auto rename(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto renamenx(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto type(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto set(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto get(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto getRange(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto mget(std::string_view keys) -> std::vector<std::byte>;

    [[nodiscard]] auto setnx(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto setRange(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto strlen(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto mset(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto msetnx(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto crement(std::string_view key, long digital) -> std::vector<std::byte>;

    [[nodiscard]] auto incr(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto incrBy(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto decr(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto decrBy(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto append(std::string_view statement) -> std::vector<std::byte>;

private:
    unsigned long id;
    Skiplist skiplist;
    std::shared_mutex lock;
};
