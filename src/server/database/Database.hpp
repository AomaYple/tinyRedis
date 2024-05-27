#pragma once

#include "SkipList.hpp"

#include <shared_mutex>
#include <source_location>

class Database {
public:
    [[nodiscard]] static auto query(std::span<const std::byte> statement) -> std::vector<std::byte>;

    Database(const Database &) = delete;

    Database(Database &&) noexcept;

    auto operator=(const Database &) -> Database = delete;

    auto operator=(Database &&) noexcept -> Database &;

    ~Database();

private:
    static auto initialize() -> std::vector<Database>;

    explicit Database(unsigned char id, std::source_location sourceLocation = std::source_location::current());

    static constexpr std::string filepathPrefix{"data/"};
    static std::vector<Database> databases;

    unsigned char id;
    SkipList skipList;
    std::shared_mutex lock;
};
