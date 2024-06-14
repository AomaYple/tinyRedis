#include "Database.hpp"

#include <algorithm>
#include <mutex>
#include <ranges>

static constexpr std::array ok{std::byte{'O'}, std::byte{'K'}};
static const std::vector integer{[] {
    constexpr std::string_view integer{"(integer) "};
    const auto spanInteger{std::as_bytes(std::span{integer})};

    return std::vector<std::byte>{spanInteger.cbegin(), spanInteger.cend()};
}()};
static constexpr std::array nil{std::byte{'n'}, std::byte{'i'}, std::byte{'l'}};
static const std::vector wrongType{[] {
    constexpr std::string_view wrongType{"(error) WRONGTYPE Operation against a key holding the wrong kind of value"};
    const auto spanWrongType{std::as_bytes(std::span{wrongType})};

    return std::vector<std::byte>{spanWrongType.cbegin(), spanWrongType.cend()};
}()};

Database::Database(const unsigned long id, const std::span<const std::byte> data) : id{id}, skiplist{data} {}

Database::Database(Database &&other) noexcept {
    const std::lock_guard lockGuard{other.lock};

    this->id = other.id;
    this->skiplist = std::move(other.skiplist);
}

auto Database::operator=(Database &&other) noexcept -> Database & {
    const std::scoped_lock scopedLock{this->lock, other.lock};

    if (this == &other) return *this;

    this->id = other.id;
    this->skiplist = std::move(other.skiplist);

    return *this;
}

auto Database::serialize() -> std::vector<std::byte> {
    std::vector<std::byte> data{sizeof(this->id)};
    *reinterpret_cast<decltype(this->id) *>(data.data()) = this->id;

    const std::shared_lock sharedLock{this->lock};

    const std::vector serialization{this->skiplist.serialize()};
    data.insert(data.cend(), serialization.cbegin(), serialization.cend());

    return data;
}

auto Database::del(const std::string_view keys) -> std::vector<std::byte> {
    unsigned long count{};

    {
        const std::lock_guard lockGuard{this->lock};

        for (const auto &view : keys | std::views::split(' '))
            if (this->skiplist.erase(std::string_view{view})) ++count;
    }

    std::vector response{integer};

    const auto stringCount{std::to_string(count)};
    const auto spanCount{std::as_bytes(std::span{stringCount})};
    response.insert(response.cend(), spanCount.cbegin(), spanCount.cend());

    return response;
}

auto Database::dump(const std::string_view key) -> std::vector<std::byte> {
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            std::vector response{std::byte{'"'}};

            const std::vector serialization{entry->serialize()};
            response.insert(response.cend(), serialization.cbegin(), serialization.cend());

            response.emplace_back(std::byte{'"'});

            return response;
        }
    }

    return {nil.cbegin(), nil.cend()};
}

auto Database::exists(const std::string_view keys) -> std::vector<std::byte> {
    unsigned long count{};

    {
        const std::shared_lock sharedLock{this->lock};

        for (const auto &view : keys | std::views::split(' '))
            if (this->skiplist.find(std::string_view{view}) != nullptr) ++count;
    }

    std::vector response{integer};

    const auto stringCount{std::to_string(count)};
    const auto spanCount{std::as_bytes(std::span{stringCount})};
    response.insert(response.cend(), spanCount.cbegin(), spanCount.cend());

    return response;
}

auto Database::move(std::unordered_map<unsigned long, Database> &databases, const std::string_view statement)
    -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)};

    bool success{};
    {
        if (const auto targetResult{databases.find(std::stoul(std::string{statement.substr(result + 1)}))};
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

    std::vector response{integer};
    response.emplace_back(success ? std::byte{'1'} : std::byte{'0'});

    return response;
}

auto Database::rename(const std::string_view statement) -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)}, newKey{statement.substr(result + 1)};

    const std::lock_guard lockGuard{this->lock};

    if (std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        this->skiplist.erase(key);

        entry->getKey() = newKey;
        this->skiplist.insert(std::move(entry));

        return {ok.cbegin(), ok.cend()};
    }

    static constexpr std::string_view noSuchKey{"(error) no such key"};
    const auto spanNoSuchKey{std::as_bytes(std::span{noSuchKey})};

    return {spanNoSuchKey.cbegin(), spanNoSuchKey.cend()};
}

auto Database::renamenx(const std::string_view statement) -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)}, newKey{statement.substr(result + 1)};

    bool success{};
    {
        const std::lock_guard lockGuard{this->lock};

        if (std::shared_ptr entry{this->skiplist.find(key)};
            entry != nullptr && this->skiplist.find(newKey) == nullptr) {
            this->skiplist.erase(key);

            entry->getKey() = newKey;
            this->skiplist.insert(std::move(entry));

            success = true;
        }
    }

    std::vector response{integer};
    response.emplace_back(success ? std::byte{'1'} : std::byte{'0'});

    return response;
}

auto Database::type(const std::string_view key) -> std::vector<std::byte> {
    std::string_view type;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            switch (entry->getType()) {
                case Entry::Type::string:
                    type = "string";
                    break;
                case Entry::Type::hash:
                    type = "hash";
                    break;
                case Entry::Type::list:
                    type = "list";
                    break;
                case Entry::Type::set:
                    type = "set";
                    break;
                case Entry::Type::sortedSet:
                    type = "zset";
                    break;
            }
        } else type = "none";
    }

    const auto spanType{std::as_bytes(std::span{type})};

    return {spanType.cbegin(), spanType.cend()};
}

auto Database::set(const std::string_view statement) -> std::vector<std::byte> {
    {
        const unsigned long result{statement.find(' ')};

        const std::lock_guard lockGuard{this->lock};

        this->skiplist.insert(std::make_shared<Entry>(std::string{statement.substr(0, result)},
                                                      std::string{statement.substr(result + 1)}));
    }

    return {ok.cbegin(), ok.cend()};
}

auto Database::get(const std::string_view key) -> std::vector<std::byte> {
    const std::shared_lock sharedLock{this->lock};

    if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
        if (entry->getType() == Entry::Type::string) {
            std::vector response{std::byte{'"'}};

            const auto spanValue{std::as_bytes(std::span{entry->getString()})};
            response.insert(response.cend(), spanValue.cbegin(), spanValue.cend());

            response.emplace_back(std::byte{'"'});

            return response;
        }

        return wrongType;
    }

    return {nil.cbegin(), nil.cend()};
}

auto Database::getRange(std::string_view statement) -> std::vector<std::byte> {
    unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)};
    statement.remove_prefix(result + 1);

    result = statement.find(' ');
    const auto start{std::stol(std::string{statement.substr(0, result)})};
    auto end{std::stol(std::string{statement.substr(result + 1)})};

    std::vector response{std::byte{'"'}};
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            end = end < 0 ? static_cast<decltype(end)>(entry->getString().size()) + end : end;
            ++end;

            if (start < end) {
                const auto value{entry->getString().substr(start, end - start)};
                const auto spanValue{std::as_bytes(std::span{value})};
                response.insert(response.cend(), spanValue.cbegin(), spanValue.cend());
            }
        }
    }
    response.emplace_back(std::byte{'"'});

    return response;
}

auto Database::mget(const std::string_view keys) -> std::vector<std::byte> {
    std::vector<std::byte> response;

    {
        unsigned long count{1};

        const std::shared_lock sharedLock{this->lock};

        for (const auto &view : keys | std::views::split(' ')) {
            const auto stringCount{std::to_string(count++) + ") "};
            const auto spanCount{std::as_bytes(std::span{stringCount})};
            response.insert(response.cend(), spanCount.cbegin(), spanCount.cend());

            if (const std::shared_ptr entry{this->skiplist.find(std::string_view{view})};
                entry != nullptr && entry->getType() == Entry::Type::string) {
                const std::string value{'"' + entry->getString() + '"'};
                const auto spanValue{std::as_bytes(std::span{value})};
                response.insert(response.cend(), spanValue.cbegin(), spanValue.cend());
            } else response.insert(response.cend(), nil.cbegin(), nil.cend());

            response.emplace_back(std::byte{'\n'});
        }
    }
    response.pop_back();

    return response;
}

auto Database::setnx(const std::string_view statement) -> std::vector<std::byte> {
    bool success{};
    {
        const unsigned long result{statement.find(' ')};

        const std::lock_guard lockGuard{this->lock};

        if (this->skiplist.find(statement.substr(0, result)) == nullptr) {
            this->skiplist.insert(std::make_shared<Entry>(std::string{statement.substr(0, result)},
                                                          std::string{statement.substr(result + 1)}));

            success = true;
        }
    }

    std::vector response{integer};
    response.emplace_back(success ? std::byte{'1'} : std::byte{'0'});

    return response;
}

auto Database::setRange(std::string_view statement) -> std::vector<std::byte> {
    unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)};
    statement.remove_prefix(result + 1);

    result = statement.find(' ');
    const auto offset{std::stoul(std::string{statement.substr(0, result)})};
    const auto value{statement.substr(result + 1)};

    std::string size;
    {
        const std::lock_guard lockGuard{this->lock};

        if (std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};

                const unsigned long oldEnd{entryValue.size()};

                if (const unsigned long end{offset + value.size()}; end > entryValue.size()) entryValue.resize(end);
                if (offset > oldEnd) entryValue.replace(oldEnd, offset - oldEnd, offset - oldEnd, ' ');

                entryValue.replace(offset, value.size(), value);

                size = std::to_string(entryValue.size());
            } else return wrongType;
        } else {
            entry = std::make_shared<Entry>(std::string{key}, std::string(offset, ' '));
            entry->getString() += value;

            size = std::to_string(entry->getString().size());

            this->skiplist.insert(std::move(entry));
        }
    }
    std::vector response{integer};
    const auto spanSize{std::as_bytes(std::span{size})};
    response.insert(response.cend(), spanSize.cbegin(), spanSize.cend());

    return response;
}

auto Database::strlen(const std::string_view key) -> std::vector<std::byte> {
    std::vector response{integer};

    std::string size;
    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                response.insert(response.cend(), integer.cbegin(), integer.cend());

                size = std::to_string(entry->getString().size());
            } else return wrongType;
        } else size = '0';
    }
    const auto spanSize{std::as_bytes(std::span{size})};
    response.insert(response.cend(), spanSize.cbegin(), spanSize.cend());

    return response;
}

auto Database::mset(std::string_view statement) -> std::vector<std::byte> {
    while (!statement.empty()) {
        unsigned long result{statement.find(' ')};
        const auto key{statement.substr(0, result)};
        statement.remove_prefix(result + 1);

        result = statement.find(' ');
        std::string_view value;
        if (result != std::string_view::npos) {
            value = statement.substr(0, result);
            statement.remove_prefix(result + 1);
        } else {
            value = statement;
            statement = {};
        }

        {
            const std::lock_guard lockGuard{this->lock};

            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{value}));
        }
    }

    return {ok.cbegin(), ok.cend()};
}

auto Database::msetnx(std::string_view statement) -> std::vector<std::byte> {
    std::vector<std::pair<std::string_view, std::string_view>> entries;
    while (!statement.empty()) {
        unsigned long result{statement.find(' ')};
        const auto key{statement.substr(0, result)};
        statement.remove_prefix(result + 1);

        result = statement.find(' ');
        std::string_view value;
        if (result != std::string_view::npos) {
            value = statement.substr(0, result);
            statement.remove_prefix(result + 1);
        } else {
            value = statement;
            statement = {};
        }

        entries.emplace_back(key, value);

        const std::shared_lock sharedLock{this->lock};

        if (this->skiplist.find(key) != nullptr) {
            entries.clear();
            break;
        }
    }

    {
        const std::lock_guard lockGuard{this->lock};

        for (const auto &[key, value] : entries)
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{value}));
    }

    const auto stringCount{std::to_string(entries.size())};
    const auto spanCount{std::as_bytes(std::span{stringCount})};

    std::vector response{integer};
    response.insert(response.cend(), spanCount.cbegin(), spanCount.cend());

    return response;
}

auto isInteger(const std::string &integer) {
    try {
        std::stol(integer);
    } catch (const std::invalid_argument &) { return false; }

    return true;
}

auto Database::crement(const std::string_view key, const long digital) -> std::vector<std::byte> {
    std::string stringDigital;
    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string && isInteger(entry->getString())) {
                entry->getString() = std::to_string(std::stol(entry->getString()) + digital);

                stringDigital = entry->getString();
            } else return wrongType;
        } else {
            stringDigital = std::to_string(digital);

            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{stringDigital}));
        }
    }
    std::vector response{integer};
    const auto spanDigital{std::as_bytes(std::span{stringDigital})};
    response.insert(response.cend(), spanDigital.cbegin(), spanDigital.cend());

    return response;
}

auto Database::incr(const std::string_view key) -> std::vector<std::byte> { return this->crement(key, 1); }

auto Database::incrBy(std::string_view statement) -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)};
    const auto increment{std::stol(std::string{statement.substr(result + 1)})};

    return this->crement(key, increment);
}

auto Database::decr(const std::string_view key) -> std::vector<std::byte> { return this->crement(key, -1); }

auto Database::decrBy(const std::string_view statement) -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)};
    const auto increment{std::stol(std::string{statement.substr(result + 1)})};

    return this->crement(key, -increment);
}

auto Database::append(const std::string_view statement) -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    const auto key{statement.substr(0, result)}, value{statement.substr(result + 1)};

    std::string size;
    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                entry->getString() += value;

                size = std::to_string(entry->getString().size());
            } else return wrongType;
        } else {
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{value}));

            size = std::to_string(value.size());
        }
    }
    std::vector response{integer};
    const auto spanSize{std::as_bytes(std::span{size})};
    response.insert(response.cend(), spanSize.cbegin(), spanSize.cend());

    return response;
}
