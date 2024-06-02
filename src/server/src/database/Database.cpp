#include "Database.hpp"

#include "../../../common/command/Command.hpp"
#include "../../../common/log/Exception.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <ranges>

auto Database::query(std::span<const std::byte> data) -> std::vector<std::byte> {
    const auto command{static_cast<Command>(data.front())};
    data = data.subspan(sizeof(command));

    const auto id{*reinterpret_cast<const unsigned long *>(data.data())};
    data = data.subspan(sizeof(id));

    const std::string_view statement{reinterpret_cast<const char *>(data.data()), data.size()};
    switch (command) {
        case Command::select:
            return select(id);
        case Command::exists:
            return databases.at(id).exists(statement);
        case Command::get:
            return databases.at(id).get(statement);
    }

    return {data.cbegin(), data.cend()};
}

auto Database::select(unsigned long id) -> std::vector<std::byte> {
    if (!databases.contains(id)) databases.emplace(id, Database{id});

    return {std::byte{'O'}, std::byte{'K'}};
}

auto Database::exists(std::string_view keys) -> std::vector<std::byte> {
    unsigned long count{};

    {
        const std::shared_lock sharedLock{this->lock};
        for (const auto &view : keys | std::views::split(' ')) {
            const std::string_view key{view};
            if (this->skipList.find(key) != nullptr) ++count;
        }
    }

    std::vector<std::byte> buffer;

    static constexpr std::string_view integer{"(integer) "};
    const auto spanInteger{std::as_bytes(std::span{integer})};
    buffer.insert(buffer.cend(), spanInteger.cbegin(), spanInteger.cend());

    const std::string stringCount{std::to_string(count)};
    const auto spanCount{std::as_bytes(std::span{stringCount})};
    buffer.insert(buffer.cend(), spanCount.cbegin(), spanCount.cend());

    return buffer;
}

auto Database::get(std::string_view key) -> std::vector<std::byte> {
    std::vector<std::byte> buffer;

    std::string response;
    {
        const std::shared_lock sharedLock{this->lock};

        const std::shared_ptr entry{this->skipList.find(key)};
        if (entry != nullptr) {
            const Entry::Type type{entry->getType()};
            if (type == Entry::Type::string) response = '"' + entry->getString() + '"';
            else response = "(error) ERR Operation against a key holding the wrong kind of value";
        } else response = "(nil)";
    }

    const auto spanResponse{std::as_bytes(std::span{response})};
    buffer.insert(buffer.cend(), spanResponse.cbegin(), spanResponse.cend());

    return buffer;
}

Database::Database(Database &&other) noexcept {
    const std::lock_guard lockGuard{other.lock};

    this->id = other.id;
    this->skipList = std::move(other.skipList);
}

auto Database::operator=(Database &&other) noexcept -> Database & {
    const std::scoped_lock scopedLock{this->lock, other.lock};

    if (this == &other) return *this;

    this->id = other.id;
    this->skipList = std::move(other.skipList);

    return *this;
}

Database::~Database() {
    const std::vector serialization{this->skipList.serialize()};

    std::ofstream file{filepathPrefix + std::to_string(this->id) + ".db", std::ios::binary};
    file.write(reinterpret_cast<const char *>(serialization.data()), static_cast<long>(serialization.size()));
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

Database::Database(unsigned long id, std::source_location sourceLocation) :
    id{id}, skipList{[id, sourceLocation] {
        const auto filepath{filepathPrefix + std::to_string(id) + ".db"};

        if (!std::filesystem::exists(filepath)) return SkipList{};

        std::ifstream file{filepath, std::ios::binary};
        if (!file) {
            throw Exception{
                Log{Log::Level::fatal, "failed to open database file", sourceLocation}
            };
        }

        std::vector<std::byte> buffer{std::filesystem::file_size(filepath)};
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<long>(buffer.size()));

        return SkipList{buffer};
    }()} {}

std::unordered_map<unsigned long, Database> Database::databases{initialize()};
