#include "Database.hpp"

#include "../log/Exception.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>

auto Database::query(std::span<const std::byte> statement) -> std::vector<std::byte> {
    return std::vector<std::byte>{statement.cbegin(), statement.cend()};
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
