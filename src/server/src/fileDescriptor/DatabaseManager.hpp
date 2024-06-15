#pragma once

#include "../database/Database.hpp"
#include "FileDescriptor.hpp"

#include <source_location>

class DatabaseManager : public FileDescriptor {
public:
    [[nodiscard]] static auto create(std::source_location sourceLocation = std::source_location::current()) -> int;

    explicit DatabaseManager(int fileDescriptor);

    auto query(std::span<const std::byte> request) -> std::vector<std::byte>;

    [[nodiscard]] auto writable() -> bool;

    [[nodiscard]] auto truncatable() const noexcept -> bool;

    [[nodiscard]] auto truncate() const noexcept -> Awaiter;

    [[nodiscard]] auto write() const noexcept -> Awaiter;

    auto wrote() noexcept -> void;

private:
    auto record(std::span<const std::byte> request) -> void;

    [[nodiscard]] auto select(unsigned long id) -> std::vector<std::byte>;

    [[nodiscard]] auto serialize() -> std::vector<std::byte>;

    std::unordered_map<unsigned long, Database> databases;
    std::shared_mutex lock;
    std::vector<std::byte> aofBuffer, writeBuffer;
    std::chrono::seconds seconds{};
    unsigned long writeCount{};
};
