#pragma once

#include "Skiplist.hpp"

#include <shared_mutex>

class Database {
public:
    Database(unsigned long index, std::span<const std::byte> data);

    Database(const Database &) = delete;

    Database(Database &&) noexcept;

    auto operator=(const Database &) -> Database = delete;

    auto operator=(Database &&) noexcept -> Database &;

    ~Database() = default;

    [[nodiscard]] auto serialize() -> std::vector<std::byte>;

    [[nodiscard]] auto del(std::string_view statement) -> std::string;

    [[nodiscard]] auto exists(std::string_view statement) -> std::string;

    [[nodiscard]] auto move(std::unordered_map<unsigned long, Database> &databases, std::string_view statement)
        -> std::string;

    [[nodiscard]] auto rename(std::string_view statement) -> std::string;

    [[nodiscard]] auto renamenx(std::string_view statement) -> std::string;

    [[nodiscard]] auto type(std::string_view statement) -> std::string;

    [[nodiscard]] auto set(std::string_view statement) -> std::string;

    [[nodiscard]] auto get(std::string_view statement) -> std::string;

    [[nodiscard]] auto getRange(std::string_view statement) -> std::string;

    [[nodiscard]] auto getBit(std::string_view statement) -> std::string;

    [[nodiscard]] auto setBit(std::string_view statement) -> std::string;

    [[nodiscard]] auto mget(std::string_view statement) -> std::string;

    [[nodiscard]] auto setnx(std::string_view statement) -> std::string;

    [[nodiscard]] auto setRange(std::string_view statement) -> std::string;

    [[nodiscard]] auto strlen(std::string_view statement) -> std::string;

    [[nodiscard]] auto mset(std::string_view statement) -> std::string;

    [[nodiscard]] auto msetnx(std::string_view statement) -> std::string;

    [[nodiscard]] auto incr(std::string_view statement) -> std::string;

    [[nodiscard]] auto incrBy(std::string_view statement) -> std::string;

    [[nodiscard]] auto decr(std::string_view statement) -> std::string;

    [[nodiscard]] auto decrBy(std::string_view statement) -> std::string;

    [[nodiscard]] auto append(std::string_view statement) -> std::string;

    [[nodiscard]] auto hdel(std::string_view statement) -> std::string;

    [[nodiscard]] auto hexists(std::string_view statement) -> std::string;

    [[nodiscard]] auto hget(std::string_view statement) -> std::string;

    [[nodiscard]] auto hgetAll(std::string_view statement) -> std::string;

    [[nodiscard]] auto hincrBy(std::string_view statement) -> std::string;

    [[nodiscard]] auto hkeys(std::string_view statement) -> std::string;

    [[nodiscard]] auto hlen(std::string_view statement) -> std::string;

    [[nodiscard]] auto hset(std::string_view statement) -> std::string;

    [[nodiscard]] auto hvals(std::string_view statement) -> std::string;

    [[nodiscard]] auto lindex(std::string_view statement) -> std::string;

    [[nodiscard]] auto llen(std::string_view statement) -> std::string;

    [[nodiscard]] auto lpop(std::string_view statement) -> std::string;

    [[nodiscard]] auto lpush(std::string_view statement) -> std::string;

    [[nodiscard]] auto lpushx(std::string_view statement) -> std::string;

private:
    [[nodiscard]] auto crement(std::string_view key, long digital, bool isPlus) -> std::string;

    unsigned long index;
    Skiplist skiplist;
    std::shared_mutex lock;
};
