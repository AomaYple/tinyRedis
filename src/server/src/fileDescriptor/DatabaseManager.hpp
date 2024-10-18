#pragma once

#include "../database/Database.hpp"
#include "FileDescriptor.hpp"

#include <source_location>

class Answer;
class Context;

class DatabaseManager final : public FileDescriptor {
public:
    [[nodiscard]] static auto create(std::source_location sourceLocation = std::source_location::current()) -> int;

    explicit DatabaseManager(int fileDescriptor);

    DatabaseManager(const DatabaseManager &) = delete;

    DatabaseManager(DatabaseManager &&) noexcept = delete;

    auto operator=(const DatabaseManager &) -> DatabaseManager & = delete;

    auto operator=(DatabaseManager &&) noexcept -> DatabaseManager & = delete;

    ~DatabaseManager() override = default;

    auto query(Context &context, Answer &&answer) -> Reply;

    [[nodiscard]] auto isWritable() -> bool;

    [[nodiscard]] auto isCanTruncate() const -> bool;

    [[nodiscard]] auto truncate() const noexcept -> Awaiter;

    [[nodiscard]] auto write() const noexcept -> Awaiter;

    auto wrote() noexcept -> void;

private:
    [[nodiscard]] static auto serializeEmptyRdb() -> std::vector<std::byte>;

    [[nodiscard]] static auto multi(Context &context) -> Reply;

    [[nodiscard]] static auto discard(Context &context) -> Reply;

    [[nodiscard]] static auto transaction(Context &context, Answer &&answer) -> Reply;

    [[nodiscard]] static auto select(Context &context, std::string_view statement) -> Reply;

    auto record(std::span<const std::byte> answer) -> void;

    [[nodiscard]] auto serialize() -> std::vector<std::byte>;

    [[nodiscard]] auto exec(Context &context) -> Reply;

    [[nodiscard]] auto flushAll() -> Reply;

    static constexpr unsigned long databaseCount{16};
    static constexpr std::string filepath{"dump.aof"};

    std::vector<Database> databases;
    std::shared_mutex lock;
    std::vector<std::byte> aofBuffer, writeBuffer;
    std::chrono::seconds seconds{};
    unsigned long writeCount{};
};
