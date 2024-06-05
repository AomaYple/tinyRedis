#include "Database.hpp"

#include "../../../common/command/Command.hpp"
#include "../../../common/log/Exception.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
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

auto Database::query(std::span<const std::byte> data) -> std::vector<std::byte> {
    const auto command{static_cast<Command>(data.front())};
    data = data.subspan(sizeof(command));

    const auto id{*reinterpret_cast<const unsigned long *>(data.data())};
    data = data.subspan(sizeof(id));

    const std::string_view statement{reinterpret_cast<const char *>(data.data()), data.size()};
    switch (command) {
        case Command::select:
            return select(id);
        case Command::del:
            return databases.at(id).del(statement);
        case Command::dump:
            return databases.at(id).dump(statement);
        case Command::exists:
            return databases.at(id).exists(statement);
        case Command::move:
            return databases.at(id).move(statement);
        case Command::rename:
            return databases.at(id).rename(statement);
        case Command::renamenx:
            return databases.at(id).renamenx(statement);
        case Command::type:
            return databases.at(id).type(statement);
        case Command::set:
            return databases.at(id).set(statement);
        case Command::get:
            return databases.at(id).get(statement);
        case Command::getRange:
            return databases.at(id).getRange(statement);
        case Command::mget:
            return databases.at(id).mget(statement);
        case Command::setnx:
            return databases.at(id).setnx(statement);
        case Command::setRange:
            return databases.at(id).setRange(statement);
        case Command::strlen:
            return databases.at(id).strlen(statement);
        case Command::mset:
            return databases.at(id).mset(statement);
    }

    return {data.cbegin(), data.cend()};
}

auto Database::select(const unsigned long id) -> std::vector<std::byte> {
    if (!databases.contains(id)) databases.emplace(id, Database{id});

    return {ok.cbegin() + 1, ok.end() - 1};
}

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

Database::~Database() {
    const std::vector serialization{this->skiplist.serialize()};

    std::ofstream file{filepathPrefix + std::to_string(this->id) + ".db", std::ios::binary};
    file.write(reinterpret_cast<const char *>(serialization.data()), static_cast<long>(serialization.size()));
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

auto Database::move(const std::string_view statment) -> std::vector<std::byte> {
    const unsigned long result{statment.find(' ')};
    const std::string_view key{statment.substr(0, result)};

    bool success{};
    {
        if (const auto targetResult{databases.find(std::stoul(std::string{statment.substr(result + 1)}))};
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
    const std::string_view key{statement.substr(0, result)}, newKey{statement.substr(result + 1)};

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
    const std::string_view key{statement.substr(0, result)}, newKey{statement.substr(result + 1)};

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
    std::string_view value{statement.substr(result + 2)};
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
    const std::string_view key{statement.substr(0, result)};
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
                const std::string value{entry->getString().substr(start, end - start)};
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
    std::string_view value{statement.substr(result + 2)};
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
    const std::string_view key{statement.substr(0, result)};
    statement.remove_prefix(result + 1);

    result = statement.find(' ');
    const auto offset{std::stoul(std::string{statement.substr(0, result)})};
    std::string_view value{statement.substr(result + 2)};
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
        const std::string_view key{statement.substr(0, result)};
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

auto Database::initialize() -> std::unordered_map<unsigned long, Database> {
    std::filesystem::create_directories(filepathPrefix);

    std::unordered_map<unsigned long, Database> databases;
    for (const auto &entry : std::filesystem::directory_iterator{filepathPrefix}) {
        const auto filename{entry.path().filename().string()};
        const auto id{std::stoul(filename.substr(0, filename.size() - 3))};
        databases.emplace(id, Database{id});
    }

    for (unsigned char i{}; i < 16; ++i)
        if (!databases.contains(i)) databases.emplace(i, Database{i});

    return databases;
}

Database::Database(const unsigned long id, std::source_location sourceLocation) :
    id{id}, skiplist{[id, sourceLocation] {
        const auto filepath{filepathPrefix + std::to_string(id) + ".db"};

        if (!std::filesystem::exists(filepath)) return Skiplist{};

        std::ifstream file{filepath, std::ios::binary};
        if (!file) {
            throw Exception{
                Log{Log::Level::fatal, "failed to open database file", sourceLocation}
            };
        }

        std::vector<std::byte> buffer{std::filesystem::file_size(filepath)};
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<long>(buffer.size()));

        return Skiplist{buffer};
    }()} {}

std::unordered_map<unsigned long, Database> Database::databases{initialize()};
