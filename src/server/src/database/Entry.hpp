#pragma once

#include <deque>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

class Entry {
public:
    enum class Type : unsigned char { string, hash, list, set, sortedSet };

    struct SortedSetElement {
        std::string key;
        double score;

        [[nodiscard]] auto operator<=>(const SortedSetElement &) const noexcept -> std::strong_ordering;
    };

    explicit Entry(std::string &&key, std::string &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::deque<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_set<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::set<SortedSetElement> &&value = {}) noexcept;

    explicit Entry(std::span<const std::byte> serialization);

    [[nodiscard]] auto getType() const noexcept -> Type;

    [[nodiscard]] auto getKey() const noexcept -> std::string_view;

    auto setKey(std::string &&key) noexcept -> void;

    [[nodiscard]] auto getString() -> std::string &;

    [[nodiscard]] auto getHash() -> std::unordered_map<std::string, std::string> &;

    [[nodiscard]] auto getList() -> std::deque<std::string> &;

    [[nodiscard]] auto getSet() -> std::unordered_set<std::string> &;

    [[nodiscard]] auto getSortedSet() -> std::set<SortedSetElement> &;

    auto setValue(std::string &&value) noexcept -> void;

    auto setValue(std::unordered_map<std::string, std::string> &&value) noexcept -> void;

    auto setValue(std::deque<std::string> &&value) noexcept -> void;

    auto setValue(std::unordered_set<std::string> &&value) noexcept -> void;

    auto setValue(std::set<SortedSetElement> &&value) noexcept -> void;

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

private:
    [[nodiscard]] auto serializeKey() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeString() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeHash() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeList() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeSet() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeSortedSet() const -> std::vector<std::byte>;

    auto deserializeString(std::span<const std::byte> serialization) -> void;

    auto deserializeHash(std::span<const std::byte> serialization) -> void;

    auto deserializeList(std::span<const std::byte> serialization) -> void;

    auto deserializeSet(std::span<const std::byte> serialization) -> void;

    auto deserializeSortedSet(std::span<const std::byte> serialization) -> void;

    Type type;
    std::string key;
    std::variant<std::string, std::unordered_map<std::string, std::string>, std::deque<std::string>,
                 std::unordered_set<std::string>, std::set<SortedSetElement>>
        value;
};
