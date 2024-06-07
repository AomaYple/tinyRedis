#include "Database.hpp"

#include <algorithm>
#include <mutex>
#include <print>
#include <ranges>

static constexpr std::array ok{std::byte{'"'}, std::byte{'O'}, std::byte{'K'}, std::byte{'"'}};
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

    std::vector<std::byte> response;

    {
        const std::lock_guard lockGuard{this->lock};

        if (std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            this->skiplist.erase(key);

            entry->getKey() = newKey;
            this->skiplist.insert(std::move(entry));

            response.insert(response.cend(), ok.cbegin(), ok.cend());
        } else {
            static constexpr std::string_view noSuchKey{"(error) no such key"};
            const auto spanNoSuchKey{std::as_bytes(std::span{noSuchKey})};

            response.insert(response.cend(), spanNoSuchKey.cbegin(), spanNoSuchKey.cend());
        }
    }

    return response;
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

    std::vector response{std::byte{'"'}};

    const auto spanType{std::as_bytes(std::span{type})};
    response.insert(response.cend(), spanType.cbegin(), spanType.cend());

    response.emplace_back(std::byte{'"'});

    return response;
}

auto Database::set(const std::string_view statement) -> std::vector<std::byte> {
    const unsigned long result{statement.find(' ')};
    auto value{statement.substr(result + 2)};
    value.remove_suffix(1);

    {
        const std::lock_guard lockGuard{this->lock};

        this->skiplist.insert(std::make_shared<Entry>(std::string{statement.substr(0, result)}, std::string{value}));
    }

    return {ok.cbegin(), ok.cend()};
}

auto Database::get(const std::string_view key) -> std::vector<std::byte> {
    std::vector<std::byte> response;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                response.emplace_back(std::byte{'"'});

                const auto spanValue{std::as_bytes(std::span{entry->getString()})};
                response.insert(response.cend(), spanValue.cbegin(), spanValue.cend());

                response.emplace_back(std::byte{'"'});
            } else response.insert(response.cend(), wrongType.cbegin(), wrongType.cend());
        } else response.insert(response.cend(), nil.cbegin(), nil.cend());
    }

    return response;
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
    const unsigned long result{statement.find(' ')};
    auto value{statement.substr(result + 2)};
    value.remove_suffix(1);

    bool success{};
    {
        const std::lock_guard lockGuard{this->lock};

        if (this->skiplist.find(statement.substr(0, result)) == nullptr) {
            this->skiplist.insert(
                std::make_shared<Entry>(std::string{statement.substr(0, result)}, std::string{value}));

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
    auto value{statement.substr(result + 2)};
    value.remove_suffix(1);

    std::vector<std::byte> response;

    {
        const std::lock_guard lockGuard{this->lock};

        if (std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                std::string &entryValue{entry->getString()};

                const unsigned long oldEnd{entryValue.size()};

                if (const unsigned long end{offset + value.size()}; end > entryValue.size()) entryValue.resize(end);
                if (offset > oldEnd) entryValue.replace(oldEnd, offset - oldEnd, offset - oldEnd, ' ');

                entryValue.replace(offset, value.size(), value);

                response.insert(response.cend(), integer.cbegin(), integer.cend());

                const auto size{std::to_string(entryValue.size())};
                const auto spanSize{std::as_bytes(std::span{size})};
                response.insert(response.cend(), spanSize.cbegin(), spanSize.cend());
            } else response.insert(response.cend(), wrongType.cbegin(), wrongType.cend());
        } else {
            entry = std::make_shared<Entry>(std::string{key}, std::string(offset, ' '));
            entry->getString() += value;

            response.insert(response.cend(), integer.cbegin(), integer.cend());

            const auto size{std::to_string(entry->getString().size())};
            const auto spanSize{std::as_bytes(std::span{size})};
            response.insert(response.cend(), spanSize.cbegin(), spanSize.cend());

            this->skiplist.insert(std::move(entry));
        }
    }

    return response;
}

auto Database::strlen(const std::string_view key) -> std::vector<std::byte> {
    std::vector<std::byte> response;

    {
        const std::shared_lock sharedLock{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string) {
                response.insert(response.cend(), integer.cbegin(), integer.cend());

                const auto size{std::to_string(entry->getString().size())};
                const auto spanSize{std::as_bytes(std::span{size})};
                response.insert(response.cend(), spanSize.cbegin(), spanSize.cend());
            } else response.insert(response.cend(), wrongType.cbegin(), wrongType.cend());
        } else {
            response.insert(response.cend(), integer.cbegin(), integer.cend());
            response.emplace_back(std::byte{'0'});
        }
    }

    return response;
}

auto Database::mset(std::string_view statement) -> std::vector<std::byte> {
    while (!statement.empty()) {
        unsigned long result{statement.find(' ')};
        const auto key{statement.substr(0, result)};
        statement.remove_prefix(result + 2);

        result = statement.find(' ');
        std::string_view value;
        if (result != std::string_view::npos) {
            value = statement.substr(0, result - 1);
            statement.remove_prefix(result + 1);
        } else {
            statement.remove_suffix(1);
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
        statement.remove_prefix(result + 2);

        result = statement.find(' ');
        std::string_view value;
        if (result != std::string_view::npos) {
            value = statement.substr(0, result - 1);
            statement.remove_prefix(result + 1);
        } else {
            statement.remove_suffix(1);
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
    std::vector<std::byte> response;

    {
        const std::lock_guard lockGuard{this->lock};

        if (const std::shared_ptr entry{this->skiplist.find(key)}; entry != nullptr) {
            if (entry->getType() == Entry::Type::string && isInteger(entry->getString())) {
                entry->getString() = std::to_string(std::stol(entry->getString()) + digital);

                response.insert(response.cend(), integer.cbegin(), integer.cend());
                const auto spanNumber{std::as_bytes(std::span{entry->getString()})};
                response.insert(response.cend(), spanNumber.cbegin(), spanNumber.cend());
            } else response = wrongType;
        } else {
            const auto stringDigital{std::to_string(digital)};
            this->skiplist.insert(std::make_shared<Entry>(std::string{key}, std::string{stringDigital}));

            response.insert(response.cend(), integer.cbegin(), integer.cend());
            const auto spanDigital{std::as_bytes(std::span{stringDigital})};
            response.insert(response.cend(), spanDigital.cbegin(), spanDigital.cend());
        }
    }

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
