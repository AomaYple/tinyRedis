#include "Database.hpp"

#include <mutex>
#include <ranges>

static constexpr std::string ok{"OK"}, integer{"(integer) "}, nil{"(nil)"};
static const std::string wrongType{"(error) WRONGTYPE Operation against a key holding the wrong kind of value"};

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
    bool success{};

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

                success = true;
            }
        }
    }

    return integer + std::to_string(success);
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
    bool success{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)}, newKey{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (std::shared_ptr entry{this->skiplist.find(key)};
            entry != nullptr && this->skiplist.find(newKey) == nullptr) {
            this->skiplist.erase(key);

            entry->getKey() = newKey;
            this->skiplist.insert(std::move(entry));

            success = true;
        }
    }

    return integer + std::to_string(success);
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
    bool success{};

    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};

        const std::lock_guard lockGuard{this->lock};

        if (this->skiplist.find(key) == nullptr) {
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{statement.substr(space + 1)}));

            success = true;
        }
    }

    return integer + std::to_string(success);
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

constexpr auto isInteger(const std::string &integer) {
    try {
        std::stol(integer);
    } catch (const std::invalid_argument &) { return false; }

    return true;
}

auto Database::crement(const std::string_view key, const long digital, const bool plus) -> std::string {
    std::string size;

    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (std::string & entryValue{entry->getString()};
                entry->getType() == Entry::Type::string && isInteger(entryValue)) {
                entryValue = size =
                    std::to_string(plus ? std::stol(entryValue) + digital : std::stol(entryValue) - digital);
            } else return wrongType;
        } else {
            size = std::to_string(digital);

            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{size}));
        }
    }

    return integer + size;
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
