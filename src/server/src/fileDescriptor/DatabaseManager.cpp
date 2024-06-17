#include "DatabaseManager.hpp"

#include "../../../common/command/Command.hpp"
#include "../../../common/log/Exception.hpp"

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <linux/io_uring.h>
#include <ranges>

static constexpr std::string filepath{"dump.aof"};

auto DatabaseManager::create(const std::source_location sourceLocation) -> int {
    const int fileDescriptor{open(filepath.data(), O_CREAT | O_WRONLY | O_APPEND | O_SYNC, S_IRUSR | S_IWUSR)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }
    if (std::filesystem::file_size(filepath) == 0) {
        constexpr unsigned long count{};
        if (::write(fileDescriptor, &count, sizeof(count)) == -1) {
            throw Exception{
                Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
            };
        }
    }

    return fileDescriptor;
}

DatabaseManager::DatabaseManager(const int fileDescriptor) : FileDescriptor{fileDescriptor} {
    for (unsigned char i{}; i < 16; ++i) this->databases.emplace(i, Database{i, std::span<const std::byte>{}});

    if (std::ifstream file{filepath}; file.is_open()) {
        std::vector<std::byte> buffer{std::filesystem::file_size(filepath)};
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<long>(buffer.size()));
        if (buffer.empty()) return;

        std::span<const std::byte> data{buffer};
        auto count{*reinterpret_cast<const unsigned long *>(data.data())};
        data = data.subspan(sizeof(count));

        while (count > 0) {
            const auto index{*reinterpret_cast<const unsigned long *>(data.data())};
            data = data.subspan(sizeof(index));

            const auto size{*reinterpret_cast<const unsigned long *>(data.data())};
            data = data.subspan(sizeof(size));

            if (const auto result{this->databases.find(index)}; result != this->databases.cend())
                result->second = Database{index, data.subspan(0, size)};
            else this->databases.emplace(index, Database{index, data.subspan(0, size)});
            data = data.subspan(size);

            --count;
        }

        while (!data.empty()) {
            const auto size{*reinterpret_cast<const unsigned long *>(data.data())};
            data = data.subspan(sizeof(size));

            this->query(data.subspan(0, size));
            data = data.subspan(size);
        }
    }
}

auto DatabaseManager::query(std::span<const std::byte> request) -> std::vector<std::byte> {
    const std::span requestCopy{request};

    const auto command{static_cast<Command>(request.front())};
    request = request.subspan(sizeof(command));

    const auto index{*reinterpret_cast<const unsigned long *>(request.data())};
    request = request.subspan(sizeof(index));

    const std::string_view statement{reinterpret_cast<const char *>(request.data()), request.size()};

    std::string response;
    bool isRecord{};
    switch (command) {
        case Command::select:
            {
                const std::lock_guard lockGuard{this->lock};

                this->databases.try_emplace(index, Database{index, std::span<const std::byte>{}});
                response = "OK";
                isRecord = true;

                break;
            }
        case Command::del:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).del(statement);
                isRecord = true;

                break;
            }
        case Command::exists:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).exists(statement);

                break;
            }
        case Command::move:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).move(this->databases, statement);
                isRecord = true;

                break;
            }
        case Command::rename:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).rename(statement);
                isRecord = true;

                break;
            }
        case Command::renamenx:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).renamenx(statement);
                isRecord = true;

                break;
            }
        case Command::type:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).type(statement);

                break;
            }
        case Command::set:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).set(statement);
                isRecord = true;

                break;
            }
        case Command::get:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).get(statement);

                break;
            }
        case Command::getRange:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).getRange(statement);

                break;
            }
        case Command::getBit:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).getBit(statement);

                break;
            }
        case Command::setBit:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).setBit(statement);
                isRecord = true;

                break;
            }
        case Command::mget:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).mget(statement);

                break;
            }
        case Command::setnx:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).setnx(statement);
                isRecord = true;

                break;
            }
        case Command::setRange:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).setRange(statement);
                isRecord = true;

                break;
            }
        case Command::strlen:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).strlen(statement);

                break;
            }
        case Command::mset:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).mset(statement);
                isRecord = true;

                break;
            }
        case Command::msetnx:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).msetnx(statement);
                isRecord = true;

                break;
            }
        case Command::incr:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).incr(statement);
                isRecord = true;

                break;
            }
        case Command::incrBy:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).incrBy(statement);
                isRecord = true;

                break;
            }
        case Command::decr:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).decr(statement);
                isRecord = true;

                break;
            }
        case Command::decrBy:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).decrBy(statement);
                isRecord = true;

                break;
            }
        case Command::append:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).append(statement);
                isRecord = true;

                break;
            }
        case Command::hdel:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hdel(statement);
                isRecord = true;

                break;
            }
        case Command::hexists:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hexists(statement);

                break;
            }
        case Command::hget:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hget(statement);

                break;
            }
        case Command::hgetAll:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hgetAll(statement);

                break;
            }
        case Command::hincrBy:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hincrBy(statement);
                isRecord = true;

                break;
            }
        case Command::hkeys:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hkeys(statement);

                break;
            }
        case Command::hlen:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hlen(statement);

                break;
            }
        case Command::hset:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hset(statement);
                isRecord = true;

                break;
            }
        case Command::hvals:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).hvals(statement);

                break;
            }
        case Command::lindex:
            {
                const std::shared_lock sharedLock{this->lock};

                response = this->databases.at(index).lindex(statement);

                break;
            }
    }
    if (isRecord) this->record(requestCopy);
    const auto bytes{std::as_bytes(std::span{response})};

    return {bytes.cbegin(), bytes.cend()};
}

auto DatabaseManager::isWritable() -> bool {
    ++this->seconds;

    if (const std::lock_guard lockGuard{this->lock}; this->writeBuffer.empty()) {
        if ((this->seconds >= std::chrono::seconds{900} && this->writeCount > 1) ||
            (this->seconds >= std::chrono::seconds{300} && this->writeCount > 10) ||
            (this->seconds >= std::chrono::seconds{60} && this->writeCount > 10000)) {
            this->seconds = std::chrono::seconds::zero();
            this->aofBuffer.clear();
            this->writeCount = 0;
            this->writeBuffer = this->serialize();

            return true;
        }

        if (!this->aofBuffer.empty()) {
            this->writeBuffer = std::move(this->aofBuffer);

            return true;
        }
    }

    return false;
}

auto DatabaseManager::isTruncatable() const noexcept -> bool {
    return this->seconds == std::chrono::seconds::zero() && !this->writeBuffer.empty();
}

auto DatabaseManager::truncate() const noexcept -> Awaiter {
    Awaiter awaiter;
    awaiter.setSubmission(Submission{this->getFileDescriptor(), IOSQE_FIXED_FILE, 0, Submission::Truncate{}});

    return awaiter;
}

auto DatabaseManager::write() const noexcept -> Awaiter {
    Awaiter awaiter;
    awaiter.setSubmission(Submission{
        this->getFileDescriptor(), IOSQE_FIXED_FILE, 0, Submission::Write{this->writeBuffer, 0}
    });

    return awaiter;
}

auto DatabaseManager::wrote() noexcept -> void { this->writeBuffer.clear(); }

auto DatabaseManager::record(const std::span<const std::byte> request) -> void {
    const std::lock_guard lockGuard{this->lock};

    const unsigned long requestSize{request.size()};
    this->aofBuffer.insert(this->aofBuffer.cend(), sizeof(requestSize), std::byte{});
    *reinterpret_cast<std::remove_const_t<decltype(requestSize)> *>(this->aofBuffer.data() + this->aofBuffer.size() -
                                                                    sizeof(requestSize)) = requestSize;

    this->aofBuffer.insert(this->aofBuffer.cend(), request.cbegin(), request.cend());

    ++this->writeCount;
}

auto DatabaseManager::serialize() -> std::vector<std::byte> {
    const unsigned long size{this->databases.size()};
    std::vector<std::byte> serialization{sizeof(size)};
    *reinterpret_cast<std::remove_const_t<decltype(size)> *>(serialization.data()) = size;

    for (auto &value : this->databases | std::views::values) {
        const std::vector subSerialization{value.serialize()};
        serialization.insert(serialization.cend(), subSerialization.cbegin(), subSerialization.cend());
    }

    return serialization;
}
