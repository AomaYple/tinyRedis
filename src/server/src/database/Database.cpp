#include "Database.hpp"

#include <mutex>
#include <ranges>

static constexpr std::string ok{"OK"}, integer{"(integer) "}, nil{"(nil)"}, emptyArray{"(empty array)"};
static const std::string wrongType{"(error) WRONGTYPE Operation against a key holding the wrong kind of value"},
    wrongInteger{"(error) ERR value is not an integer or out of range"};

constexpr auto isInteger(const std::string &integer) {
    try {
        std::stol(integer);
    } catch (const std::invalid_argument &) { return false; }

    return true;
}

Database::Database(const unsigned long index, const std::span<const std::byte> data) : index{index}, skiplist{data} {}

Database::Database(Database &&other) noexcept {
    const std::lock_guard lockGuard{other.lock};

    this->index = other.index;
    this->skiplist = std::move(other.skiplist);
}

auto Database::operator=(Database &&other) noexcept -> Database & {
    const std::scoped_lock scopedLock{this->lock, other.lock};

    if (this == &other) return *this;

    this->index = other.index;
    this->skiplist = std::move(other.skiplist);

    return *this;
}

auto Database::serialize() -> std::vector<std::byte> {
    std::vector<std::byte> data{sizeof(this->index)};
    *reinterpret_cast<decltype(this->index) *>(data.data()) = this->index;

    const std::shared_lock sharedLock{this->lock};

    const std::vector serialization{this->skiplist.serialize()};
    data.insert(data.cend(), serialization.cbegin(), serialization.cend());

    return data;
}

auto Database::del(const std::string_view statement) -> std::string {
    unsigned long count{};

    {
        std::vector<std::string_view> keys;
        for (const auto &view : statement | std::views::split(' ')) keys.emplace_back(view);

        const std::lock_guard lockGuard{this->lock};

        for (const auto key : keys) count += this->skiplist.erase(key);
    }

    return integer + std::to_string(count);
}

auto Database::exists(const std::string_view statement) -> std::string {
    unsigned long count{};

    {
        std::vector<std::string_view> keys;
        for (const auto &view : statement | std::views::split(' ')) keys.emplace_back(view);

        const std::shared_lock sharedLock{this->lock};

        for (const auto key : keys)
            if (this->skiplist.find(key) != nullptr) ++count;
    }

    return integer + std::to_string(count);
}

auto Database::move(std::unordered_map<unsigned long, Database> &databases, const std::string_view statement)
    -> std::string {
    bool isSuccess{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};

        if (const auto targetResult{databases.find(std::stoul(std::string{statement.substr(space + 1)}))};
            targetResult != databases.cend()) {
            Database &target{targetResult->second};

            const std::scoped_lock scopedLock{this->lock, target.lock};

            if (std::shared_ptr entry{this->skiplist.find(key)};
                entry != nullptr && target.skiplist.find(key) == nullptr) {
                this->skiplist.erase(key);
                target.skiplist.insert(std::move(entry));

                isSuccess = true;
            }
        }
    }

    return integer + std::to_string(isSuccess);
}

auto Database::rename(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)}, newKey{statement.substr(space + 1)};

    const std::lock_guard lockGuard{this->lock};

    if (std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        this->skiplist.erase(key);

        entry->getKey() = newKey;
        this->skiplist.insert(std::move(entry));

        return ok;
    }

    return "(error) ERR no such key";
}

auto Database::renamenx(const std::string_view statement) -> std::string {
    bool isSuccess{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)}, newKey{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (std::shared_ptr entry{this->skiplist.find(key)};
            entry != nullptr && this->skiplist.find(newKey) == nullptr) {
            this->skiplist.erase(key);

            entry->getKey() = newKey;
            this->skiplist.insert(std::move(entry));

            isSuccess = true;
        }
    }

    return integer + std::to_string(isSuccess);
}

auto Database::type(const std::string_view statement) -> std::string {
    const std::shared_lock sharedLock{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
        switch (entry->getType()) {
            case Entry::Type::string:
                return "string";
            case Entry::Type::hash:
                return "hash";
            case Entry::Type::list:
                return "list";
            case Entry::Type::set:
                return "set";
            case Entry::Type::sortedSet:
                return "zset";
        }
    }

    return "none";
}

auto Database::set(const std::string_view statement) -> std::string {
    {
        const unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)}, value{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(value)));
    }

    return ok;
}

auto Database::get(const std::string_view statement) -> std::string {
    std::string value;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) value = entry->getString();
            else return wrongType;
        } else return nil;
    }

    return '"' + value + '"';
}

auto Database::getRange(std::string_view statement) -> std::string {
    unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    statement.remove_prefix(space + 1);

    space = statement.find(' ');
    auto start{std::stol(std::string{statement.substr(0, space)})},
        end{std::stol(std::string{statement.substr(space + 1)})};

    std::string result;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            const auto entryValueSize{static_cast<decltype(start)>(entry->getString().size())};

            start = start < 0 ? entryValueSize + start : start;
            if (start < 0) start = 0;

            end = end < 0 ? entryValueSize + end : end;
            ++end;
            if (end > entryValueSize) end = entryValueSize;

            if (start < entryValueSize && end > 0 && start < end)
                result = entry->getString().substr(start, end - start);
        }
    }

    return '"' + result + '"';
}

auto Database::getBit(const std::string_view statement) -> std::string {
    bool bit{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const auto offset{std::stoul(std::string{statement.substr(space + 1)})};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                if (const unsigned long index{offset / 8}; index < entry->getString().size())
                    bit = entry->getString()[index] >> offset % 8 & 1;
            } else return wrongType;
        }
    }

    return integer + std::to_string(bit);
}

auto Database::mget(const std::string_view statement) -> std::string {
    std::vector<std::string> values;

    {
        std::vector<std::string_view> keys;
        for (const auto &view : statement | std::views::split(' ')) keys.emplace_back(view);

        const std::shared_lock sharedLock{this->lock};

        for (const auto key : keys) {
            if (const std::shared_ptr entry{this->skiplist.find(key)};
                entry != nullptr && entry->getType() == Entry::Type::string)
                values.emplace_back(entry->getString());
            else values.emplace_back(nil);
        }
    }

    std::string result;
    unsigned long index{};
    for (const auto &value : values) {
        result += std::to_string(++index) + ") ";
        result += '"' + value + '"' + '\n';
    }
    result.pop_back();

    return result;
}

auto Database::setBit(std::string_view statement) -> std::string {
    bool oldBit{};

    {
        unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        space = statement.find(' ');
        const auto offset{std::stoul(std::string{statement.substr(0, space)})}, index{offset / 8};
        const auto position{static_cast<unsigned char>(offset % 8)};
        const auto value{statement.substr(space + 1) == "1"};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};

                if (index >= entryValue.size()) entryValue.resize(index + 1);

                char &element{entryValue[index]};
                oldBit = element >> position & 1;

                if (value) element = static_cast<char>(element | 1 << position);
                else element = static_cast<char>(element & ~(1 << position));
            } else return wrongType;
        } else {
            std::string newValue(index + 1, 0);
            if (char &element{newValue[index]}; value) element = static_cast<char>(element | 1 << position);

            this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(newValue)));
        }
    }

    return integer + std::to_string(oldBit);
}

auto Database::setnx(const std::string_view statement) -> std::string {
    bool isSuccess{};

    {
        const unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)}, value{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (this->skiplist.find(key) == nullptr) {
            this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(value)));

            isSuccess = true;
        }
    }

    return integer + std::to_string(isSuccess);
}

auto Database::setRange(std::string_view statement) -> std::string {
    unsigned long size;

    {
        unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        space = statement.find(' ');
        const auto offset{std::stoul(std::string{statement.substr(0, space)})};
        const auto value{statement.substr(space + 1)};

        const unsigned long end{offset + value.size()};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};
                const unsigned long oldEnd{entryValue.size()};

                if (end > oldEnd) entryValue.resize(end);
                if (offset > oldEnd) entryValue.replace(oldEnd, offset - oldEnd, offset - oldEnd, '\0');

                entryValue.replace(offset, value.size(), value);
                size = entryValue.size();
            } else return wrongType;
        } else {
            std::string newValue{std::string(offset, '\0') + std::string{value}};
            size = newValue.size();

            this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(newValue)));
        }
    }

    return integer + std::to_string(size);
}

auto Database::strlen(const std::string_view statement) -> std::string {
    unsigned long size{};

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) size = entry->getString().size();
            else return wrongType;
        }
    }

    return integer + std::to_string(size);
}

auto Database::mset(std::string_view statement) -> std::string {
    {
        std::vector<std::pair<std::string, std::string>> keyValues;
        while (!statement.empty()) {
            unsigned long space{statement.find(' ')};
            const auto key{statement.substr(0, space)};
            statement.remove_prefix(space + 1);

            space = statement.find(' ');
            std::string_view value;
            if (space != std::string_view::npos) {
                value = statement.substr(0, space);
                statement.remove_prefix(space + 1);
            } else {
                value = statement;
                statement = {};
            }

            keyValues.emplace_back(key, value);
        }

        const std::lock_guard lockGuard{this->lock};

        for (auto &[key, value] : keyValues)
            this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(value)));
    }

    return ok;
}

auto Database::msetnx(std::string_view statement) -> std::string {
    std::vector<std::pair<std::string, std::string>> keyValues;

    {
        while (!statement.empty()) {
            unsigned long space{statement.find(' ')};
            const auto key{statement.substr(0, space)};
            statement.remove_prefix(space + 1);

            space = statement.find(' ');
            std::string_view value;
            if (space != std::string_view::npos) {
                value = statement.substr(0, space);
                statement.remove_prefix(space + 1);
            } else {
                value = statement;
                statement = {};
            }

            keyValues.emplace_back(key, value);
        }

        const std::lock_guard lockGuard{this->lock};

        for (const std::string_view key : keyValues | std::views::keys) {
            if (this->skiplist.find(key) != nullptr) {
                keyValues.clear();

                break;
            }
        }

        for (auto &[key, value] : keyValues)
            this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(value)));
    }

    return integer + std::to_string(keyValues.size());
}

auto Database::incr(const std::string_view statement) -> std::string { return this->crement(statement, 1, true); }

auto Database::incrBy(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    const auto increment{std::stol(std::string{statement.substr(space + 1)})};

    return this->crement(key, increment, true);
}

auto Database::decr(const std::string_view statement) -> std::string { return this->crement(statement, 1, false); }

auto Database::decrBy(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    const auto increment{std::stol(std::string{statement.substr(space + 1)})};

    return this->crement(key, increment, false);
}

auto Database::append(const std::string_view statement) -> std::string {
    unsigned long size;

    {
        const unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)}, value{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};

                entryValue += value;
                size = entryValue.size();
            } else return wrongType;
        } else {
            size = value.size();
            this->skiplist.insert(std::make_shared<Entry>(std::move(key), std::move(value)));
        }
    }

    return integer + std::to_string(size);
}

auto Database::hdel(std::string_view statement) -> std::string {
    unsigned long count{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::string> fileds;
        for (const auto &view : statement | std::views::split(' ')) fileds.emplace_back(std::string_view{view});

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                for (const auto &filed : fileds) count += hash.erase(filed);
            } else return wrongType;
        }
    }

    return integer + std::to_string(count);
}

auto Database::hexists(const std::string_view statement) -> std::string {
    bool isExist{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const std::string field{statement.substr(space + 1)};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                if (entry->getHash().contains(field)) isExist = true;
            } else return wrongType;
        }
    }

    return integer + std::to_string(isExist);
}

auto Database::hget(const std::string_view statement) -> std::string {
    std::string value;

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const std::string field{statement.substr(space + 1)};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                const std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (const auto result{hash.find(field)}; result != hash.cend()) value = result->second;
            } else return wrongType;
        } else return nil;
    }

    return '"' + value + '"';
}

auto Database::hgetAll(const std::string_view statement) -> std::string {
    std::vector<std::pair<std::string, std::string>> filedValues;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr)
            for (const auto &[field, value] : entry->getHash()) filedValues.emplace_back(field, value);
    }

    std::string result;
    unsigned long index{};
    for (const auto &[field, value] : filedValues) {
        result += std::to_string(++index) + ") ";
        result += '"' + field + '"' + '\n';

        result += std::to_string(++index) + ") ";
        result += '"' + value + '"' + '\n';
    }
    if (!result.empty()) {
        result.pop_back();

        return result;
    }

    return emptyArray;
}

auto Database::hincrBy(std::string_view statement) -> std::string {
    std::string value;

    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        space = statement.find(' ');
        std::string field{statement.substr(0, space)};
        const auto crement{std::stol(std::string{statement.substr(space + 1)})};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (const auto result{hash.find(field)}; result != hash.cend()) {
                    if (isInteger(result->second)) {
                        result->second = std::to_string(std::stol(result->second) + crement);

                        value = result->second;
                    } else return wrongInteger;
                } else {
                    value = std::to_string(crement);
                    hash.emplace(std::move(field), value);
                }
            } else return wrongType;
        } else {
            value = std::to_string(crement);

            this->skiplist.insert(std::make_shared<Entry>(
                std::string{
                    key
            },
                std::unordered_map{std::pair{std::move(field), std::string{value}}}));
        }
    }

    return integer + value;
}

auto Database::hkeys(const std::string_view statement) -> std::string {
    std::vector<std::string> fileds;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash)
                for (const std::string_view filed : entry->getHash() | std::views::keys) fileds.emplace_back(filed);
            else return wrongType;
        }
    }

    std::string result;
    unsigned long index{};
    for (const auto &filed : fileds) {
        result += std::to_string(++index) + ") ";
        result += '"' + filed + '"' + '\n';
    }
    if (!result.empty()) {
        result.pop_back();

        return result;
    }

    return emptyArray;
}

auto Database::hlen(const std::string_view statement) -> std::string {
    unsigned long size{};

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) size = entry->getHash().size();
            else return wrongType;
        }
    }

    return integer + std::to_string(size);
}

auto Database::hset(std::string_view statement) -> std::string {
    unsigned long count{};

    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::pair<std::string_view, std::string_view>> filedValues;
        while (!statement.empty()) {
            space = statement.find(' ');
            const auto field{statement.substr(0, space)};
            statement.remove_prefix(space + 1);

            space = statement.find(' ');
            std::string_view value;
            if (space != std::string_view::npos) {
                value = statement.substr(0, space);
                statement.remove_prefix(space + 1);
            } else {
                value = statement;
                statement = {};
            }

            filedValues.emplace_back(field, value);
        }

        bool isNew{};
        std::unordered_map<std::string, std::string> newHash;

        const std::lock_guard lockGuard{this->lock};

        const std::shared_ptr entry{this->skiplist.find(key)};
        if (entry != nullptr) {
            if (entry->getType() != Entry::Type::hash) return wrongType;
        } else isNew = true;

        for (const auto &[first, second] : filedValues) {
            std::string filed{first}, value{second};

            if (!isNew) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (!hash.contains(filed)) ++count;
                hash[std::move(filed)] = std::move(value);
            } else newHash.emplace(std::move(filed), std::move(value));
        }

        if (isNew) {
            count = newHash.size();
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::move(newHash)));
        }
    }

    return integer + std::to_string(count);
}

auto Database::hvals(const std::string_view statement) -> std::string {
    std::vector<std::string> values;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash)
                for (const std::string_view value : entry->getHash() | std::views::values) values.emplace_back(value);
            else return wrongType;
        }
    }

    std::string result;
    unsigned long index{};
    for (const auto &value : values) {
        result += std::to_string(++index) + ") ";
        result += '"' + value + '"' + '\n';
    }
    if (!result.empty()) {
        result.pop_back();

        return result;
    }

    return emptyArray;
}

auto Database::lindex(const std::string_view statement) -> std::string {
    std::string element;

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        auto index{std::stol(std::string{statement.substr(space + 1)})};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                const std::deque<std::string> &list{entry->getList()};
                const auto listSize{static_cast<decltype(index)>(list.size())};

                index = index < 0 ? listSize + index : index;
                if (index >= listSize || index < 0) return nil;

                element = list[index];
            } else return wrongType;
        } else return nil;
    }

    return '"' + element + '"';
}

auto Database::llen(const std::string_view statement) -> std::string {
    unsigned long size{};

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) size = entry->getList().size();
            else return wrongType;
        }
    }

    return integer + std::to_string(size);
}

auto Database::lpop(const std::string_view statement) -> std::string {
    std::string element;

    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                if (std::deque<std::string> & list{entry->getList()}; !list.empty()) {
                    element = std::move(list.front());
                    list.pop_front();
                }
            } else return wrongType;
        }
    }

    if (!element.empty()) return '"' + element + '"';

    return nil;
}

auto Database::lpush(std::string_view statement) -> std::string {
    unsigned long size;

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::string> elements;
        for (const auto &view : statement | std::views::split(' ')) elements.emplace_back(std::string_view{view});

        bool isNew{};
        std::deque<std::string> newList;

        const std::lock_guard lockGuard{this->lock};

        const std::shared_ptr entry{this->skiplist.find(key)};
        if (entry != nullptr) {
            if (entry->getType() != Entry::Type::list) return wrongType;
        } else isNew = true;

        for (auto &element : elements) {
            if (!isNew) entry->getList().emplace_front(std::move(element));
            else newList.emplace_front(std::move(element));
        }

        if (!isNew) size = entry->getList().size();
        else {
            size = newList.size();
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::move(newList)));
        }
    }

    return integer + std::to_string(size);
}

auto Database::lpushx(std::string_view statement) -> std::string {
    unsigned long size{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::string> elements;
        for (const auto &view : statement | std::views::split(' ')) elements.emplace_back(std::string_view{view});

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                std::deque<std::string> &list{entry->getList()};

                for (auto &element : elements) list.emplace_front(std::move(element));
                size = list.size();
            } else return wrongType;
        }
    }

    return integer + std::to_string(size);
}

auto Database::crement(const std::string_view key, const long digital, const bool isPlus) -> std::string {
    long number;

    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                if (std::string & value{entry->getString()}; isInteger(value)) {
                    number = isPlus ? std::stol(value) + digital : std::stol(value) - digital;

                    value = std::to_string(number);
                } else return wrongInteger;
            } else return wrongType;
        } else {
            number = digital;

            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{std::to_string(number)}));
        }
    }

    return integer + std::to_string(number);
}
