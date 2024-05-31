#include "Database.hpp"

#include "../log/Exception.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <ranges>

auto Database::query(std::span<const std::byte> data) -> std::vector<std::byte> {
    const auto id{*reinterpret_cast<const unsigned long *>(data.data())};
    data = data.subspan(sizeof(id));

    const std::string_view response{reinterpret_cast<const char *>(data.data()), data.size()};
    const unsigned long result{response.find(' ')};

    const std::string_view command{response.substr(0, result)}, statement{response.substr(result + 1)};
    Database &database{databases.at(id)};
    if (command == "SELECT") return select(statement);
    else if (command == "EXISTS") return database.exists(statement);

    return {data.cbegin(), data.cend()};
}

auto Database::select(std::string_view statement) -> std::vector<std::byte> {
    const std::string stringNewId{statement};
    const auto newId{std::stoul(stringNewId)};

    if (!databases.contains(newId)) databases.emplace(newId, Database{newId});

    std::vector<std::byte> response{sizeof(newId)};
    *reinterpret_cast<unsigned long *>(response.data()) = newId;

    response.emplace_back(std::byte{'O'});
    response.emplace_back(std::byte{'K'});

    return response;
}

auto Database::exists(std::string_view statement) -> std::vector<std::byte> {
    unsigned long count{};

    {
        const std::shared_lock sharedLock{this->lock};
        for (const auto &view : statement | std::views::split(' ')) {
            const std::string_view key{view};
            if (this->skipList.find(key) != nullptr) ++count;
        }
    }

    std::vector<std::byte> response{sizeof(this->id)};
    *reinterpret_cast<decltype(this->id) *>(response.data()) = this->id;

    response.emplace_back(std::byte{'('});
    response.emplace_back(std::byte{'i'});
    response.emplace_back(std::byte{'n'});
    response.emplace_back(std::byte{'t'});
    response.emplace_back(std::byte{'e'});
    response.emplace_back(std::byte{'g'});
    response.emplace_back(std::byte{'e'});
    response.emplace_back(std::byte{'r'});
    response.emplace_back(std::byte{')'});
    response.emplace_back(std::byte{' '});

    for (const auto element : std::to_string(count)) response.emplace_back(static_cast<std::byte>(element));

    return response;
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
