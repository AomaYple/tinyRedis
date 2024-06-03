#pragma once

#include "SkipList.hpp"

#include <shared_mutex>
#include <source_location>

class Database {
public:
    [[nodiscard]] static auto query(std::span<const std::byte> data) -> std::vector<std::byte>;

    [[nodiscard]] static auto select(unsigned long id) -> std::vector<std::byte>;

    Database(const Database &) = delete;

    Database(Database &&) noexcept;

    auto operator=(const Database &) -> Database = delete;

    auto operator=(Database &&) noexcept -> Database &;

    ~Database();

    [[nodiscard]] auto del(std::string_view keys) -> std::vector<std::byte>;

    [[nodiscard]] auto dump(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto exists(std::string_view keys) -> std::vector<std::byte>;

    [[nodiscard]] auto move(std::string_view statment) -> std::vector<std::byte>;

    [[nodiscard]] auto rename(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto renamenx(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto type(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto set(std::string_view statement) -> std::vector<std::byte>;

    [[nodiscard]] auto get(std::string_view key) -> std::vector<std::byte>;

    [[nodiscard]] auto getRange(std::string_view statement) -> std::vector<std::byte>;

private:
    static auto initialize() -> std::unordered_map<unsigned long, Database>;

    explicit Database(unsigned long id, std::source_location sourceLocation = std::source_location::current());

    static constexpr std::string filepathPrefix{"data/"};
    static std::unordered_map<unsigned long, Database> databases;

    unsigned long id;
    SkipList skipList;
    std::shared_mutex lock;
};
