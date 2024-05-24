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

        auto operator<(const SortedSetElement &) const noexcept -> bool;
    };

    explicit Entry(std::string &&key, std::string &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::deque<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_set<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::set<SortedSetElement> &&value = {}) noexcept;

    auto getKey() noexcept -> std::string &;

    auto getValueType() const noexcept -> unsigned char;

    auto getStringValue() -> std::string &;

    auto getHashValue() -> std::unordered_map<std::string, std::string> &;

    auto getListValue() -> std::deque<std::string> &;

    auto getSetValue() -> std::unordered_set<std::string> &;

    auto getSortedSetValue() -> std::set<SortedSetElement> &;

    auto serialize() const -> std::vector<std::byte>;

private:
    auto serializeKey() const -> std::vector<std::byte>;

    auto serializeString() const -> std::vector<std::byte>;

    auto serializeHash() const -> std::vector<std::byte>;

    auto serializeList() const -> std::vector<std::byte>;

    auto serializeSet() const -> std::vector<std::byte>;

    auto serializeSortedSet() const -> std::vector<std::byte>;

    std::string key;
    std::variant<std::string, std::unordered_map<std::string, std::string>, std::deque<std::string>,
                 std::unordered_set<std::string>, std::set<SortedSetElement>>
        value;
};
