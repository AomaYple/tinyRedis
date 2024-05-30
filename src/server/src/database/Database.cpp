#include "Database.hpp"

#include "../log/Exception.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>

auto Database::query(std::span<const std::byte> data) -> std::vector<std::byte> {
    const auto id{static_cast<unsigned char>(data.front())};
    data = data.subspan(2);

    const std::string_view response{reinterpret_cast<const char *>(data.data()), data.size()};
    const unsigned long result{response.find(' ')};

    const std::string_view command{response.substr(0, result)}, statement{response.substr(result + 1)};
    if (command == "SELECT") return select(id, statement);

    return std::vector<std::byte>{data.cbegin(), data.cend()};
}

auto Database::select(unsigned char id, std::string_view statement) -> std::vector<std::byte> {
    const std::string stringNewId{statement.front()};
    const auto newId{static_cast<unsigned char>(std::stoi(stringNewId))};

    std::string result;
    if (newId < databases.size()) {
        result += static_cast<char>(newId);
        result += " OK";
    } else {
        result += static_cast<char>(id);
        result += " DATABASE NOT FOUND";
    }

    const auto spanResult{std::as_bytes(std::span{result})};

    return std::vector<std::byte>{spanResult.cbegin(), spanResult.cend()};
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
    const std::vector<std::byte> serialization{this->skipList.serialize()};
    if (serialization.empty()) return;

    std::ofstream file{filepathPrefix + std::to_string(this->id) + ".db", std::ios::binary};
    file.write(reinterpret_cast<const char *>(serialization.data()), static_cast<long>(serialization.size()));
}

auto Database::initialize() -> std::vector<Database> {
    std::filesystem::create_directories(filepathPrefix);

    std::vector<Database> databases;
    for (unsigned char i{}; i < 16; ++i) databases.emplace_back(Database{i});

    return databases;
}

Database::Database(unsigned char id, std::source_location sourceLocation) :
    id{id}, skipList{[id, sourceLocation] {
        const std::string filepath{filepathPrefix + std::to_string(id) + ".db"};
        if (!std::filesystem::exists(filepath)) {
            const std::ofstream file{filepath};
            if (!file) {
                throw Exception{
                    Log{Log::Level::fatal, "failed to create database file", sourceLocation}
                };
            }
        }

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

std::vector<Database> Database::databases{initialize()};
