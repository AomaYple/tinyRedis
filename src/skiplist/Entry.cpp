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

auto Entry::getKey() const noexcept -> std::string_view { return this->key; }

auto Entry::getValueType() const noexcept -> std::size_t { return this->value.index(); }

auto Entry::getStringValue() const -> std::string_view { return std::get<0>(this->value); }

auto Entry::getHashValue() -> std::unordered_map<std::string, std::string> & { return std::get<1>(this->value); }

auto Entry::getListValue() -> std::queue<std::string> & { return std::get<2>(this->value); }

auto Entry::getSetValue() -> std::unordered_set<std::string> & { return std::get<3>(this->value); }

auto Entry::getSortedSetValue() -> std::set<std::string> & { return std::get<4>(this->value); }
