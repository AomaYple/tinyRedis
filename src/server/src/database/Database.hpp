#pragma once

#include "SkipList.hpp"

#include <shared_mutex>

class Reply;

class Database {
public:
    Database(unsigned long index, std::span<const std::byte> data);

    Database(const Database &) = delete;

    Database(Database &&) noexcept;

    auto operator=(const Database &) -> Database & = delete;

    auto operator=(Database &&) noexcept -> Database &;

    ~Database() = default;

    [[nodiscard]] auto serialize() -> std::vector<std::byte>;

    [[nodiscard]] auto del(std::string_view statement) -> Reply;

    [[nodiscard]] auto exists(std::string_view statement) -> Reply;

    [[nodiscard]] auto move(std::span<Database> databases, std::string_view statement) -> Reply;

    [[nodiscard]] auto rename(std::string_view statement) -> Reply;

    [[nodiscard]] auto renameNx(std::string_view statement) -> Reply;

    [[nodiscard]] auto type(std::string_view statement) -> Reply;

    [[nodiscard]] auto set(std::string_view statement) -> Reply;

    [[nodiscard]] auto get(std::string_view statement) -> Reply;

    [[nodiscard]] auto getRange(std::string_view statement) -> Reply;

    [[nodiscard]] auto getBit(std::string_view statement) -> Reply;

    [[nodiscard]] auto mGet(std::string_view statement) -> Reply;

    [[nodiscard]] auto setBit(std::string_view statement) -> Reply;

    [[nodiscard]] auto setNx(std::string_view statement) -> Reply;

    [[nodiscard]] auto setRange(std::string_view statement) -> Reply;

    [[nodiscard]] auto strlen(std::string_view statement) -> Reply;

    [[nodiscard]] auto mSet(std::string_view statement) -> Reply;

    [[nodiscard]] auto mSetNx(std::string_view statement) -> Reply;

    [[nodiscard]] auto incr(std::string_view statement) -> Reply;

    [[nodiscard]] auto incrBy(std::string_view statement) -> Reply;

    [[nodiscard]] auto decr(std::string_view statement) -> Reply;

    [[nodiscard]] auto decrBy(std::string_view statement) -> Reply;

    [[nodiscard]] auto append(std::string_view statement) -> Reply;

    [[nodiscard]] auto hDel(std::string_view statement) -> Reply;

    [[nodiscard]] auto hExists(std::string_view statement) -> Reply;

    [[nodiscard]] auto hGet(std::string_view statement) -> Reply;

    [[nodiscard]] auto hGetAll(std::string_view statement) -> Reply;

    [[nodiscard]] auto hIncrBy(std::string_view statement) -> Reply;

    [[nodiscard]] auto hKeys(std::string_view statement) -> Reply;

    [[nodiscard]] auto hLen(std::string_view statement) -> Reply;

    [[nodiscard]] auto hSet(std::string_view statement) -> Reply;

    [[nodiscard]] auto hVals(std::string_view statement) -> Reply;

    [[nodiscard]] auto lIndex(std::string_view statement) -> Reply;

    [[nodiscard]] auto lLen(std::string_view statement) -> Reply;

    [[nodiscard]] auto lPop(std::string_view statement) -> Reply;

    [[nodiscard]] auto lPush(std::string_view statement) -> Reply;

    [[nodiscard]] auto lPushX(std::string_view statement) -> Reply;

private:
    [[nodiscard]] auto crement(std::string_view key, long digital, bool isPlus) -> Reply;

    static constexpr std::string ok{"OK"};
    static const std::string wrongType, wrongInteger;

    unsigned long index;
    SkipList skipList;
    std::shared_mutex lock;
};
