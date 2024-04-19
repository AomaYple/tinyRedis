#pragma once

#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

class Entry {
public:
    explicit Entry(std::string &&key, std::string &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::queue<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::unordered_set<std::string> &&value = {}) noexcept;

    explicit Entry(std::string &&key, std::set<std::string> &&value = {}) noexcept;

    auto getKey() const noexcept -> std::string_view;

    auto getValueType() const noexcept -> std::size_t;

    auto getStringValue() const -> std::string_view;

    auto getHashValue() -> std::unordered_map<std::string, std::string> &;

    auto getListValue() -> std::queue<std::string> &;

    auto getSetValue() -> std::unordered_set<std::string> &;

    auto getSortedSetValue() -> std::set<std::string> &;

private:
    std::string key;
    std::variant<std::string, std::unordered_map<std::string, std::string>, std::queue<std::string>,
                 std::unordered_set<std::string>, std::set<std::string>>
        value;
};
