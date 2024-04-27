#include "Entry.hpp"

Entry::Entry(std::string &&key, std::string &&value) noexcept : key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::queue<std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::unordered_set<std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::set<std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

auto Entry::getKey() noexcept -> std::string & { return this->key; }

auto Entry::getValueType() const noexcept -> unsigned char { return this->value.index(); }

auto Entry::getStringValue() -> std::string & { return std::get<std::string>(this->value); }

auto Entry::getHashValue() -> std::unordered_map<std::string, std::string> & {
    return std::get<std::unordered_map<std::string, std::string>>(this->value);
}

auto Entry::getListValue() -> std::queue<std::string> & { return std::get<std::queue<std::string>>(this->value); }

auto Entry::getSetValue() -> std::unordered_set<std::string> & {
    return std::get<std::unordered_set<std::string>>(this->value);
}

auto Entry::getSortedSetValue() -> std::set<std::string> & { return std::get<std::set<std::string>>(this->value); }
