#pragma once

#include "SkipList.hpp"

#include <shared_mutex>
#include <unordered_map>

class Database {
public:
    Database(unsigned long index, std::span<const std::byte> data);

    Database(const Database &) = delete;

    Database(Database &&) noexcept;

    auto operator=(const Database &) -> Database & = delete;

    auto operator=(Database &&) noexcept -> Database &;

    ~Database() = default;

    [[nodiscard]] auto serialize() -> std::vector<std::byte>;

    [[nodiscard]] auto del(std::string_view statement) -> std::string;

    [[nodiscard]] auto exists(std::string_view statement) -> std::string;

    [[nodiscard]] auto move(std::unordered_map<unsigned long, Database> &databases, std::string_view statement)
        -> std::string;

    [[nodiscard]] auto rename(std::string_view statement) -> std::string;

    [[nodiscard]] auto renameNx(std::string_view statement) -> std::string;

    [[nodiscard]] auto type(std::string_view statement) -> std::string;

    [[nodiscard]] auto set(std::string_view statement) -> std::string;

    [[nodiscard]] auto get(std::string_view statement) -> std::string;

    [[nodiscard]] auto getRange(std::string_view statement) -> std::string;

    [[nodiscard]] auto getBit(std::string_view statement) -> std::string;

    [[nodiscard]] auto mGet(std::string_view statement) -> std::string;

    [[nodiscard]] auto setBit(std::string_view statement) -> std::string;

    [[nodiscard]] auto setNx(std::string_view statement) -> std::string;

    [[nodiscard]] auto setRange(std::string_view statement) -> std::string;

    [[nodiscard]] auto strlen(std::string_view statement) -> std::string;

    [[nodiscard]] auto mSet(std::string_view statement) -> std::string;

    [[nodiscard]] auto mSetNx(std::string_view statement) -> std::string;

    [[nodiscard]] auto incr(std::string_view statement) -> std::string;

    [[nodiscard]] auto incrBy(std::string_view statement) -> std::string;

    [[nodiscard]] auto decr(std::string_view statement) -> std::string;

    [[nodiscard]] auto decrBy(std::string_view statement) -> std::string;

    [[nodiscard]] auto append(std::string_view statement) -> std::string;

    [[nodiscard]] auto hDel(std::string_view statement) -> std::string;

    [[nodiscard]] auto hExists(std::string_view statement) -> std::string;

    [[nodiscard]] auto hGet(std::string_view statement) -> std::string;

    [[nodiscard]] auto hGetAll(std::string_view statement) -> std::string;

    [[nodiscard]] auto hIncrBy(std::string_view statement) -> std::string;

    [[nodiscard]] auto hKeys(std::string_view statement) -> std::string;

    [[nodiscard]] auto hLen(std::string_view statement) -> std::string;

    [[nodiscard]] auto hSet(std::string_view statement) -> std::string;

    [[nodiscard]] auto hVals(std::string_view statement) -> std::string;

    [[nodiscard]] auto lIndex(std::string_view statement) -> std::string;

    [[nodiscard]] auto lLen(std::string_view statement) -> std::string;

    [[nodiscard]] auto lPop(std::string_view statement) -> std::string;

    [[nodiscard]] auto lPush(std::string_view statement) -> std::string;

    [[nodiscard]] auto lPushX(std::string_view statement) -> std::string;

private:
    [[nodiscard]] auto crement(std::string_view key, long digital, bool isPlus) -> std::string;

    unsigned long index;
    SkipList skipList;
    std::shared_mutex lock;
};
