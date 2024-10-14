#pragma once

#include <span>
#include <string>
#include <variant>
#include <vector>

class Reply {
public:
    enum class Type : unsigned char { nil, integer, error, status, string, array };

    Reply(Type type, std::variant<long, std::string, std::vector<Reply>> &&value, unsigned long databaseIndex = {},
          bool isTransaction = {}) noexcept;

    explicit Reply(std::span<const std::byte> data);

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

    [[nodiscard]] auto getDatabaseIndex() const noexcept -> unsigned long;

    auto setDatabaseIndex(unsigned long databaseIndex) noexcept -> void;

    [[nodiscard]] auto getIsTransaction() const noexcept -> bool;

    auto setIsTransaction(bool isTransaction) noexcept -> void;

    [[nodiscard]] auto getType() const noexcept -> Type;

    [[nodiscard]] auto getInteger() const -> long;

    [[nodiscard]] auto getString() const -> std::string_view;

    [[nodiscard]] auto getArray() const -> std::span<const Reply>;

private:
    [[nodiscard]] auto serializeInteger() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeString() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeArray() const -> std::vector<std::byte>;

    auto deserializeInteger(std::span<const std::byte> data) -> void;

    auto deserializeString(std::span<const std::byte> data) -> void;

    auto deserializeArray(std::span<const std::byte> data) -> void;

    unsigned long databaseIndex;
    bool isTransaction;
    Type type;
    std::variant<long, std::string, std::vector<Reply>> value;
};
