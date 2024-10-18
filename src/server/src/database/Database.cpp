#include "Database.hpp"

#include "../../../common/Reply.hpp"
#include "Entry.hpp"

#include <mutex>
#include <ranges>

[[nodiscard]] constexpr auto isInteger(const std::string &integer) {
    try {
        std::stol(integer);
    } catch (const std::invalid_argument &) { return false; }

    return true;
}

Database::Database(const unsigned long index, const std::span<const std::byte> data) : index{index}, skipList{data} {}

Database::Database(Database &&other) noexcept {
    const std::lock_guard lockGuard{other.lock};

    this->index = other.index;
    this->skipList = std::move(other.skipList);
}

auto Database::operator=(Database &&other) noexcept -> Database & {
    const std::scoped_lock scopedLock{this->lock, other.lock};

    if (this == &other) return *this;

    this->index = other.index;
    this->skipList = std::move(other.skipList);

    return *this;
}

auto Database::serialize() -> std::vector<std::byte> {
    std::vector<std::byte> serializedSkipList;
    {
        const std::shared_lock sharedLock{this->lock};

        serializedSkipList = this->skipList.serialize();
    }

    std::vector<std::byte> serialization;

    const unsigned long size{serializedSkipList.size()};
    const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
    serialization.insert(serialization.cend(), sizeBytes.cbegin(), sizeBytes.cend());

    serialization.insert(serialization.cend(), serializedSkipList.cbegin(), serializedSkipList.cend());

    return serialization;
}

auto Database::flushDb() -> Reply {
    {
        const std::lock_guard lockGuard{this->lock};

        this->skipList.clear();
    }

    return {Reply::Type::status, ok};
}

auto Database::del(const std::string_view statement) -> Reply {
    long count{};
    std::vector<std::string_view> keys;
    for (const auto &view : statement | std::views::split(' ')) keys.emplace_back(view);

    for (const std::lock_guard lockGuard{this->lock}; const auto key : keys) count += this->skipList.erase(key) ? 1 : 0;

    return {Reply::Type::integer, count};
}

auto Database::exists(const std::string_view statement) -> Reply {
    long count{};

    std::vector<std::string_view> keys;
    for (const auto &view : statement | std::views::split(' ')) keys.emplace_back(view);

    for (const std::shared_lock sharedLock{this->lock}; const auto key : keys)
        if (this->skipList.find(key) != nullptr) ++count;

    return {Reply::Type::integer, count};
}

auto Database::move(const std::span<Database> databases, const std::string_view statement) -> Reply {
    bool isSuccess{};
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};

        Database &target{databases[std::stoul(std::string{statement.substr(space + 1)})]};

        const std::scoped_lock scopedLock{this->lock, target.lock};

        if (const std::shared_ptr entry{this->skipList.find(key)};
            entry != nullptr && target.skipList.find(key) == nullptr) {
            this->skipList.erase(key);
            target.skipList.insert(entry);

            isSuccess = true;
        }
    }

    return {Reply::Type::integer, isSuccess ? 1 : 0};
}

auto Database::rename(const std::string_view statement) -> Reply {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)}, newKey{statement.substr(space + 1)};

    const std::lock_guard lockGuard{this->lock};

    if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
        this->skipList.erase(key);

        entry->setKey(std::string{newKey});
        this->skipList.insert(entry);

        return {Reply::Type::status, ok};
    }

    return {Reply::Type::error, "ERR no such key"};
}

auto Database::renameNx(const std::string_view statement) -> Reply {
    bool isSuccess{};
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)}, newKey{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)};
            entry != nullptr && this->skipList.find(newKey) == nullptr) {
            this->skipList.erase(key);

            entry->setKey(std::string{newKey});
            this->skipList.insert(entry);

            isSuccess = true;
        }
    }

    return {Reply::Type::integer, isSuccess ? 1 : 0};
}

auto Database::type(const std::string_view statement) -> Reply {
    std::string value;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            switch (entry->getType()) {
                case Entry::Type::string:
                    value = "string";
                    break;
                case Entry::Type::hash:
                    value = "hash";
                    break;
                case Entry::Type::list:
                    value = "list";
                    break;
                case Entry::Type::set:
                    value = "set";
                    break;
                case Entry::Type::sortedSet:
                    value = "zset";
                    break;
            }
        } else value = "none";
    }

    return {Reply::Type::status, std::move(value)};
}

auto Database::set(const std::string_view statement) -> Reply {
    {
        const unsigned long space{statement.find(' ')};
        const auto entry{
            std::make_shared<Entry>(std::string{statement.substr(0, space)}, std::string{statement.substr(space + 1)})};

        const std::lock_guard lockGuard{this->lock};

        this->skipList.insert(entry);
    }

    return {Reply::Type::status, ok};
}

auto Database::get(const std::string_view statement) -> Reply {
    std::string value;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) value = entry->getString();
            else return {Reply::Type::error, wrongType};
        } else return {Reply::Type::nil, 0};
    }

    return {Reply::Type::string, std::move(value)};
}

auto Database::getRange(std::string_view statement) -> Reply {
    unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    statement.remove_prefix(space + 1);

    space = statement.find(' ');
    auto start{std::stol(std::string{statement.substr(0, space)})},
        end{std::stol(std::string{statement.substr(space + 1)})};

    std::string value;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            const auto entryValueSize{static_cast<decltype(start)>(entry->getString().size())};

            start = start < 0 ? entryValueSize + start : start;
            if (start < 0) start = 0;

            end = end < 0 ? entryValueSize + end : end;
            ++end;
            if (end > entryValueSize) end = entryValueSize;

            if (start < entryValueSize && end > 0 && start < end) value = entry->getString().substr(start, end - start);
        }
    }

    return {Reply::Type::string, std::move(value)};
}

auto Database::getBit(const std::string_view statement) -> Reply {
    bool bit{};
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const auto offset{std::stoul(std::string{statement.substr(space + 1)})};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                if (const unsigned long index{offset / 8}; index < entry->getString().size())
                    bit = entry->getString()[index] >> offset % 8 & 1;
            } else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, bit ? 1 : 0};
}

auto Database::mGet(const std::string_view statement) -> Reply {
    std::vector<Reply> replies;

    std::vector<std::string_view> keys;
    for (const auto &view : statement | std::views::split(' ')) keys.emplace_back(view);

    for (const std::shared_lock sharedLock{this->lock}; const auto key : keys) {
        if (const std::shared_ptr entry{this->skipList.find(key)};
            entry != nullptr && entry->getType() == Entry::Type::string)
            replies.emplace_back(Reply::Type::string, entry->getString());
        else replies.emplace_back(Reply::Type::nil, 0);
    }

    return {Reply::Type::array, std::move(replies)};
}

auto Database::setBit(std::string_view statement) -> Reply {
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

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};

                if (index >= entryValue.size()) entryValue.resize(index + 1);

                char &element{entryValue[index]};
                oldBit = element >> position & 1;

                if (value) element = static_cast<char>(element | 1 << position);
                else element = static_cast<char>(element & ~(1 << position));
            } else return {Reply::Type::error, wrongType};
        } else {
            std::string newValue(index + 1, 0);
            if (char &element{newValue[index]}; value) element = static_cast<char>(element | 1 << position);

            this->skipList.insert(std::make_shared<Entry>(std::move(key), std::move(newValue)));
        }
    }

    return {Reply::Type::integer, oldBit ? 1 : 0};
}

auto Database::setNx(const std::string_view statement) -> Reply {
    bool isSuccess{};
    {
        const unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)}, value{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (this->skipList.find(key) == nullptr) {
            this->skipList.insert(std::make_shared<Entry>(std::move(key), std::move(value)));

            isSuccess = true;
        }
    }

    return {Reply::Type::integer, isSuccess ? 1 : 0};
}

auto Database::setRange(std::string_view statement) -> Reply {
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

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};
                const unsigned long oldEnd{entryValue.size()};

                if (end > oldEnd) entryValue.resize(end);
                if (offset > oldEnd) entryValue.replace(oldEnd, offset - oldEnd, offset - oldEnd, '\0');

                entryValue.replace(offset, value.size(), value);
                size = entryValue.size();
            } else return {Reply::Type::error, wrongType};
        } else {
            std::string newValue{std::string(offset, 0) + std::string{value}};
            size = newValue.size();

            this->skipList.insert(std::make_shared<Entry>(std::move(key), std::move(newValue)));
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::strlen(const std::string_view statement) -> Reply {
    unsigned long size{};
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) size = entry->getString().size();
            else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::mSet(std::string_view statement) -> Reply {
    std::vector<std::shared_ptr<Entry>> entries;
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
            statement = std::string_view{};
        }

        entries.emplace_back(std::make_shared<Entry>(std::string{key}, std::string{value}));
    }

    for (const std::lock_guard lockGuard{this->lock}; const auto &entry : entries) this->skipList.insert(entry);

    return {Reply::Type::status, ok};
}

auto Database::mSetNx(std::string_view statement) -> Reply {
    std::vector<std::shared_ptr<Entry>> entries;
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
                statement = std::string_view{};
            }

            entries.emplace_back(std::make_shared<Entry>(std::string{key}, std::string{value}));
        }

        const std::lock_guard lockGuard{this->lock};

        for (const auto &entry : entries) {
            if (this->skipList.find(entry->getKey()) != nullptr) {
                entries.clear();

                break;
            }
        }

        for (const auto &entry : entries) this->skipList.insert(entry);
    }

    return {Reply::Type::integer, static_cast<long>(entries.size())};
}

auto Database::incr(const std::string_view statement) -> Reply { return this->crement(statement, 1, true); }

auto Database::incrBy(const std::string_view statement) -> Reply {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    const auto increment{std::stol(std::string{statement.substr(space + 1)})};

    return this->crement(key, increment, true);
}

auto Database::decr(const std::string_view statement) -> Reply { return this->crement(statement, 1, false); }

auto Database::decrBy(const std::string_view statement) -> Reply {
    const unsigned long space{statement.find(' ')};
    const auto key{statement.substr(0, space)};
    const auto increment{std::stol(std::string{statement.substr(space + 1)})};

    return this->crement(key, increment, false);
}

auto Database::append(const std::string_view statement) -> Reply {
    unsigned long size;
    {
        const unsigned long space{statement.find(' ')};
        std::string key{statement.substr(0, space)}, value{statement.substr(space + 1)};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};

                entryValue += value;
                size = entryValue.size();
            } else return {Reply::Type::error, wrongType};
        } else {
            size = value.size();
            this->skipList.insert(std::make_shared<Entry>(std::move(key), std::move(value)));
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::hDel(std::string_view statement) -> Reply {
    unsigned long count{};
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::string> fields;
        for (const auto &view : statement | std::views::split(' ')) fields.emplace_back(std::string_view{view});

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                for (const auto &field : fields) count += hash.erase(field);
            } else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, static_cast<long>(count)};
}

auto Database::hExists(const std::string_view statement) -> Reply {
    bool isExist{};
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const std::string field{statement.substr(space + 1)};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                if (entry->getHash().contains(field)) isExist = true;
            } else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, isExist ? 1 : 0};
}

auto Database::hGet(const std::string_view statement) -> Reply {
    std::string value;
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        const std::string field{statement.substr(space + 1)};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                const std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (const auto result{hash.find(field)}; result != hash.cend()) value = result->second;
                else return {Reply::Type::nil, 0};
            } else return {Reply::Type::error, wrongType};
        } else return {Reply::Type::nil, 0};
    }

    return {Reply::Type::string, std::move(value)};
}

auto Database::hGetAll(const std::string_view statement) -> Reply {
    std::vector<Reply> replies;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            for (const auto &[field, value] : entry->getHash()) {
                replies.emplace_back(Reply::Type::string, field);
                replies.emplace_back(Reply::Type::string, value);
            }
        }
    }

    return {Reply::Type::array, std::move(replies)};
}

auto Database::hIncrBy(std::string_view statement) -> Reply {
    std::string value;
    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        space = statement.find(' ');
        std::string field{statement.substr(0, space)};
        const auto crement{std::stol(std::string{statement.substr(space + 1)})};

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (const auto result{hash.find(field)}; result != hash.cend()) {
                    if (isInteger(result->second)) {
                        result->second = std::to_string(std::stol(result->second) + crement);

                        value = result->second;
                    } else return {Reply::Type::error, wrongInteger};
                } else {
                    value = std::to_string(crement);
                    hash.emplace(std::move(field), value);
                }
            } else return {Reply::Type::error, wrongType};
        } else {
            value = std::to_string(crement);

            this->skipList.insert(std::make_shared<Entry>(
                std::string{
                    key
            },
                std::unordered_map{std::pair{std::move(field), std::string{value}}}));
        }
    }

    return {Reply::Type::integer, std::stol(value)};
}

auto Database::hKeys(const std::string_view statement) -> Reply {
    std::vector<Reply> replies;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                for (const std::string_view field : entry->getHash() | std::views::keys)
                    replies.emplace_back(Reply::Type::string, std::string{field});
            } else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::array, std::move(replies)};
}

auto Database::hLen(const std::string_view statement) -> Reply {
    unsigned long size{};
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) size = entry->getHash().size();
            else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::hSet(std::string_view statement) -> Reply {
    unsigned long count{};
    {
        unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::pair<std::string_view, std::string_view>> fieldValues;
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
                statement = std::string_view{};
            }

            fieldValues.emplace_back(field, value);
        }

        bool isNew{};
        std::unordered_map<std::string, std::string> newHash;

        const std::lock_guard lockGuard{this->lock};

        const std::shared_ptr entry{this->skipList.find(key)};
        if (entry != nullptr) {
            if (entry->getType() != Entry::Type::hash) return {Reply::Type::error, wrongType};
        } else isNew = true;

        for (const auto &[first, second] : fieldValues) {
            std::string field{first}, value{second};

            if (!isNew) {
                std::unordered_map<std::string, std::string> &hash{entry->getHash()};

                if (!hash.contains(field)) ++count;
                hash[std::move(field)] = std::move(value);
            } else newHash.emplace(std::move(field), std::move(value));
        }

        if (isNew) {
            count = newHash.size();
            this->skipList.insert(std::make_shared<Entry>(std::string{key}, std::move(newHash)));
        }
    }

    return {Reply::Type::integer, static_cast<long>(count)};
}

auto Database::hVals(const std::string_view statement) -> Reply {
    std::vector<Reply> replies;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::hash) {
                for (const std::string_view value : entry->getHash() | std::views::values)
                    replies.emplace_back(Reply::Type::string, std::string{value});
            } else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::array, std::move(replies)};
}

auto Database::lIndex(const std::string_view statement) -> Reply {
    std::string value;
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        auto index{std::stol(std::string{statement.substr(space + 1)})};

        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                const std::deque<std::string> &list{entry->getList()};
                const auto listSize{static_cast<decltype(index)>(list.size())};

                index = index < 0 ? listSize + index : index;
                if (index >= listSize || index < 0) return {Reply::Type::nil, 0};

                value = list[index];
            } else return {Reply::Type::error, wrongType};
        } else return {Reply::Type::nil, 0};
    }

    return {Reply::Type::string, std::move(value)};
}

auto Database::lLen(const std::string_view statement) -> Reply {
    unsigned long size{};
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) size = entry->getList().size();
            else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::lPop(const std::string_view statement) -> Reply {
    std::string value;
    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(statement)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                if (std::deque<std::string> & list{entry->getList()}; !list.empty()) {
                    value = std::move(list.front());
                    list.pop_front();
                }
            } else return {Reply::Type::error, wrongType};
        }
    }

    if (!value.empty()) return {Reply::Type::string, std::move(value)};

    return {Reply::Type::nil, 0};
}

auto Database::lPush(std::string_view statement) -> Reply {
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

        const std::shared_ptr entry{this->skipList.find(key)};
        if (entry != nullptr) {
            if (entry->getType() != Entry::Type::list) return {Reply::Type::error, wrongType};
        } else isNew = true;

        for (auto &element : elements) {
            if (!isNew) entry->getList().emplace_front(std::move(element));
            else newList.emplace_front(std::move(element));
        }

        if (!isNew) size = entry->getList().size();
        else {
            size = newList.size();
            this->skipList.insert(std::make_shared<Entry>(std::string{key}, std::move(newList)));
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::lPushX(std::string_view statement) -> Reply {
    unsigned long size{};
    {
        const unsigned long space{statement.find(' ')};
        const auto key{statement.substr(0, space)};
        statement.remove_prefix(space + 1);

        std::vector<std::string> elements;
        for (const auto &view : statement | std::views::split(' ')) elements.emplace_back(std::string_view{view});

        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::list) {
                std::deque<std::string> &list{entry->getList()};

                for (auto &element : elements) list.emplace_front(std::move(element));
                size = list.size();
            } else return {Reply::Type::error, wrongType};
        }
    }

    return {Reply::Type::integer, static_cast<long>(size)};
}

auto Database::crement(const std::string_view key, const long digital, const bool isPlus) -> Reply {
    long number;
    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skipList.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                if (std::string & value{entry->getString()}; isInteger(value)) {
                    number = isPlus ? std::stol(value) + digital : std::stol(value) - digital;

                    value = std::to_string(number);
                } else return {Reply::Type::error, wrongInteger};
            } else return {Reply::Type::error, wrongType};
        } else {
            number = digital;

            this->skipList.insert(std::make_shared<Entry>(std::string{key}, std::string{std::to_string(number)}));
        }
    }

    return {Reply::Type::integer, number};
}

const std::string Database::wrongType{"WRONGTYPE Operation against a key holding the wrong kind of value"},
    Database::wrongInteger{"ERR value is not an integer or out of range"};
