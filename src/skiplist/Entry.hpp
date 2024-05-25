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
    struct SortedSetElement {
        std::string key;
        double score;

        [[nodiscard]] auto operator<(const SortedSetElement &) const noexcept -> bool;
    };

    explicit Entry(std::string &&key, std::string &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::deque<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_set<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::set<SortedSetElement> &&value = {}) noexcept;

    explicit Entry(std::span<const std::byte> serializedEntry);

    [[nodiscard]] auto getKey() noexcept -> std::string &;

    [[nodiscard]] auto getValueType() const noexcept -> unsigned char;

    [[nodiscard]] auto getStringValue() -> std::string &;

    [[nodiscard]] auto getHashValue() -> std::unordered_map<std::string, std::string> &;

    [[nodiscard]] auto getListValue() -> std::deque<std::string> &;

    [[nodiscard]] auto getSetValue() -> std::unordered_set<std::string> &;

    [[nodiscard]] auto getSortedSetValue() -> std::set<SortedSetElement> &;

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

private:
    [[nodiscard]] auto serializeKey() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeString() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeHash() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeList() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeSet() const -> std::vector<std::byte>;

    [[nodiscard]] auto serializeSortedSet() const -> std::vector<std::byte>;

    auto deserializeString(std::span<const std::byte> serializedValue) -> void;

    auto deserializeHash(std::span<const std::byte> serializedValue) -> void;

    auto deserializeList(std::span<const std::byte> serializedValue) -> void;

    auto deserializeSet(std::span<const std::byte> serializedValue) -> void;

    auto deserializeSortedSet(std::span<const std::byte> serializedValue) -> void;

    std::string key;
    std::variant<std::string, std::unordered_map<std::string, std::string>, std::deque<std::string>,
                 std::unordered_set<std::string>, std::set<SortedSetElement>>
        value;
};
