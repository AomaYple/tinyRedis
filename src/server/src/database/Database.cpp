#include "Database.hpp"

#include <mutex>
#include <queue>
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

auto Database::del(const std::string_view keys) -> std::string {
    unsigned long count{};

    {
        const std::lock_guard lockGuard{this->lock};

        for (const auto &view : keys | std::views::split(' '))
            if (this->skiplist.erase(std::string_view{view})) ++count;
    }

    return integer + std::to_string(count);
}

auto Database::exists(const std::string_view keys) -> std::string {
    unsigned long count{};

    {
        const std::shared_lock sharedLock{this->lock};

        for (const auto &view : keys | std::views::split(' '))
            if (this->skiplist.find(std::string_view{view}) != nullptr) ++count;
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

auto Database::type(const std::string_view key) -> std::string {
    const std::shared_lock sharedLock{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
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
    const unsigned long space{statement.find(' ')};

    const std::lock_guard lockGuard{this->lock};

    this->skiplist.insert(
        std::make_shared<Entry>(std::string{statement.substr(0, space)}, std::string{statement.substr(space + 1)}));

    return ok;
}

auto Database::get(const std::string_view key) -> std::string {
    const std::shared_lock sharedLock{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        if (entry->getType() == Entry::Type::string) return '"' + entry->getString() + '"';

        return wrongType;
    }

    return nil;
}

auto Database::getRange(std::string_view statement) -> std::string {
    unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    statement.remove_prefix(space + 1);

    space = statement.find(' ');
    auto start{std::stol(std::string{statement.substr(0, space)})},
        end{std::stol(std::string{statement.substr(space + 1)})};

    std::string result{'"'};

    const std::shared_lock sharedLock{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        const auto entryValueSize{static_cast<decltype(start)>(entry->getString().size())};

        start = start < 0 ? entryValueSize + start : start;
        if (start < 0) start = 0;

        end = end < 0 ? entryValueSize + end : end;
        ++end;
        if (end > entryValueSize) end = entryValueSize;

        if (start < entryValueSize && end > 0 && start < end) result += entry->getString().substr(start, end - start);
    }
    result += '"';

    return result;
}

auto Database::getBit(const std::string_view statement) -> std::string {
    auto bit{'0'};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const auto offset{std::stoul(std::string{statement.substr(space + 1)})};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                if (const unsigned long index{offset / 8}; index < entry->getString().size())
                    bit = entry->getString()[index] >> offset % 8 & 1 ? '1' : '0';
            } else return wrongType;
        }
    }

    return integer + bit;
}

auto Database::setBit(std::string_view statement) -> std::string {
    auto bit{'0'};

    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
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
                bit = element >> position & 1 ? '1' : '0';

                if (value) element = static_cast<char>(element | 1 << position);
                else element = static_cast<char>(element & ~(1 << position));
            } else return wrongType;
        } else {
            std::string newValue(index + 1, 0);

            if (char &element{newValue[index]}; value) element = static_cast<char>(element | 1 << position);

            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::move(newValue)));
        }
    }

    return integer + bit;
}

auto Database::mget(const std::string_view keys) -> std::string {
    std::string result;

    {
        unsigned long count{1};

        const std::shared_lock sharedLock{this->lock};

        for (const auto &view : keys | std::views::split(' ')) {
            result += std::to_string(count++) + ") ";

            if (const std::shared_ptr entry{this->skiplist.find(std::string_view{view})};
                entry != nullptr && entry->getType() == Entry::Type::string)
                result += '"' + entry->getString() + '"';
            else result += nil;

            result += '\n';
        }
    }

    result.pop_back();

    return result;
}

auto Database::setnx(const std::string_view statement) -> std::string {
    bool isSuccess{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};

        const std::lock_guard lockGuard{this->lock};

        if (this->skiplist.find(key) == nullptr) {
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{statement.substr(space + 1)}));

            isSuccess = true;
        }
    }

    return integer + std::to_string(isSuccess);
}

auto Database::setRange(std::string_view statement) -> std::string {
    std::string size;

    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        space = statement.find(' ');
        const auto offset{std::stoul(std::string{statement.substr(0, space)})};
        const auto value{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};
                const unsigned long oldEnd{entryValue.size()};

                if (const unsigned long end{offset + value.size()}; end > oldEnd) entryValue.resize(end);
                if (offset > oldEnd) entryValue.replace(oldEnd, offset - oldEnd, offset - oldEnd, '\0');

                entryValue.replace(offset, value.size(), value);
                size = std::to_string(entryValue.size());
            } else return wrongType;
        } else {
            std::string newValue{std::string(offset, '\0') + std::string{value}};
            size = std::to_string(newValue.size());

            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::move(newValue)));
        }
    }

    return integer + size;
}

auto Database::strlen(const std::string_view key) -> std::string {
    std::string size;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) size = std::to_string(entry->getString().size());
            else return wrongType;
        } else size = '0';
    }

    return integer + size;
}

auto Database::mset(std::string_view statement) -> std::string {
    const std::lock_guard lockGuard{this->lock};

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

        this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{value}));
    }

    return ok;
}

auto Database::msetnx(std::string_view statement) -> std::string {
    std::vector<std::pair<std::string_view, std::string_view>> entries;

    {
        const std::lock_guard lockGuard{this->lock};

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

            entries.emplace_back(key, value);

            if (this->skiplist.find(key) != nullptr) {
                entries.clear();

                break;
            }
        }

        for (const auto &[key, value] : entries)
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{value}));
    }

    return integer + std::to_string(entries.size());
}

auto Database::incr(const std::string_view key) -> std::string { return this->crement(key, 1, true); }

auto Database::incrBy(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    const auto increment{std::stol(std::string{statement.substr(space + 1)})};

    return this->crement(key, increment, true);
}

auto Database::decr(const std::string_view key) -> std::string { return this->crement(key, 1, false); }

auto Database::decrBy(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    const auto increment{std::stol(std::string{statement.substr(space + 1)})};

    return this->crement(key, increment, false);
}

auto Database::append(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)}, value{statement.substr(space + 1)};

    const std::lock_guard lockGuard{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        if (entry->getType() == Entry::Type::string) {
            std::string &entryValue{entry->getString()};
            entryValue += value;

            return std::to_string(entryValue.size());
        }

        return wrongType;
    }

    this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{value}));

    return std::to_string(value.size());
}

auto Database::hdel(std::string_view statement) -> std::string {
    unsigned long count{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                for (std::unordered_map<std::string, std::string> &hash{entry->getHash()};
                     const auto &view : statement | std::views::split(' '))
                    if (hash.erase(std::string{std::string_view{view}}) == 1) ++count;
            } else return wrongType;
        }
    }

    return integer + std::to_string(count);
}

auto Database::hexists(const std::string_view statement) -> std::string {
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)}, field{statement.substr(space + 1)};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                if (entry->getHash().contains(std::string{field})) return integer + '1';
            } else return wrongType;
        }
    }

    return integer + '0';
}

auto Database::hget(const std::string_view statement) -> std::string {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)}, field{statement.substr(space + 1)};

    const std::shared_lock sharedLock{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        if (entry->getType() == Entry::Type::hash) {
            const std::unordered_map<std::string, std::string> &hash{entry->getHash()};

            if (const auto result{hash.find(std::string{field})}; result != hash.cend())
                return '"' + result->second + '"';
        } else return wrongType;
    }

    return nil;
}

auto Database::hgetAll(const std::string_view key) -> std::string {
    std::string result;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            unsigned long index{1};
            for (const auto &[field, value] : entry->getHash()) {
                result += std::to_string(index++) + ") ";
                result += '"' + field + '"' + '\n';

                result += std::to_string(index++) + ") ";
                result += '"' + value + '"' + '\n';
            }
        }
    }

    if (!result.empty()) {
        result.pop_back();

        return result;
    }

    return emptyArray;
}

auto Database::hincrBy(std::string_view statement) -> std::string {
    std::string digital;

    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        space = statement.find(' ');
        const auto field{statement.substr(0, space)};
        const auto crement{std::stol(std::string{statement.substr(space + 1)})};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            std::unordered_map<std::string, std::string> &hash{entry->getHash()};

            if (const auto result{hash.find(std::string{field})}; result != hash.cend()) {
                if (isInteger(result->second)) {
                    result->second = std::to_string(std::stol(result->second) + crement);

                    digital = result->second;
                } else return wrongInteger;
            } else {
                digital = std::to_string(crement);
                hash.emplace(std::string{field}, digital);
            }
        } else {
            digital = std::to_string(crement);

            this->skiplist.insert(std::make_shared<Entry>(
                std::string{
                    key
            },
                std::unordered_map{std::pair{std::string{field}, digital}}));
        }
    }

    return integer + digital;
}

auto Database::hkeys(const std::string_view key) -> std::string {
    std::string result;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                unsigned long count{1};
                for (const auto &filed : entry->getHash() | std::views::keys) {
                    result += std::to_string(count++) + ") ";
                    result += '"' + filed + '"' + '\n';
                }
            } else return wrongType;
        }
    }

    if (!result.empty()) {
        result.pop_back();

        return result;
    }

    return emptyArray;
}

auto Database::hlen(const std::string_view key) -> std::string {
    std::string length{'0'};

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) length = std::to_string(entry->getHash().size());
            else return wrongType;
        }
    }

    return integer + length;
}

auto Database::hset(std::string_view statement) -> std::string {
    unsigned long count{};

    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::unordered_map<std::string, std::string> newHash;
        auto isNew{true};

        const std::lock_guard lockGuard{this->lock};

        const std::shared_ptr entry{this->skiplist.find(key)};
        if (entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) isNew = false;
            else return wrongType;
        }

        while (!statement.empty()) {
            space = statement.find(' ');
            std::string field{statement.substr(0, space)};
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

            if (!isNew) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (!hash.contains(field)) ++count;
                hash[std::move(field)] = std::string{value};
            } else newHash.emplace(std::move(field), std::string{value});
        }

        if (isNew) {
            count = newHash.size();
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::move(newHash)));
        }
    }

    return integer + std::to_string(count);
}

auto Database::hvals(const std::string_view key) -> std::string {
    std::string result;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                unsigned long index{1};
                for (const auto &value : entry->getHash() | std::views::values) {
                    result += std::to_string(index++) + ") ";
                    result += '"' + value + '"' + '\n';
                }
            } else return wrongType;
        }
    }

    if (!result.empty()) {
        result.pop_back();

        return result;
    }

    return emptyArray;
}

auto Database::lindex(const std::string_view statement) -> std::string {
    std::string value;

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

                value = list[index];
            } else return wrongType;
        } else return nil;
    }

    return '"' + value + '"';
}

auto Database::llen(const std::string_view key) -> std::string {
    std::string length{'0'};

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) length = std::to_string(entry->getList().size());
            else return wrongType;
        }
    }

    return integer + length;
}

auto Database::lpop(const std::string_view key) -> std::string {
    std::string element;

    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
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

        std::queue<std::string_view> elements;
        for (const auto &view : statement | std::views::split(' ')) elements.emplace(view);

        bool isNew{};
        std::deque<std::string> newList;

        const std::lock_guard lockGuard{this->lock};

        const std::shared_ptr entry{this->skiplist.find(key)};
        if (entry != nullptr) {
            if (entry->getType() != Entry::Type::list) return wrongType;
        } else isNew = true;

        while (!elements.empty()) {
            if (!isNew) entry->getList().emplace_front(elements.front());
            else newList.emplace_front(elements.front());

            elements.pop();
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

        std::queue<std::string_view> elements;
        for (const auto &view : statement | std::views::split(' ')) elements.emplace(view);

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                std::deque<std::string> &list{entry->getList()};
                while (!elements.empty()) {
                    list.emplace_front(elements.front());
                    elements.pop();
                }

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
