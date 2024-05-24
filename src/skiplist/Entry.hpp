#pragma once

#include <deque>
#include <set>
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

    std::string key;
    std::variant<std::string, std::unordered_map<std::string, std::string>, std::deque<std::string>,
                 std::unordered_set<std::string>, std::set<SortedSetElement>>
        value;
};
