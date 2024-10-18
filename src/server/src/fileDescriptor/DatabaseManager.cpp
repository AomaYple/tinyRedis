#include "DatabaseManager.hpp"

#include "../../../common/Exception.hpp"
#include "../../../common/Reply.hpp"
#include "../database/Context.hpp"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <linux/io_uring.h>

auto DatabaseManager::create(const std::source_location sourceLocation) -> int {
    const int fileDescriptor{open(filepath.data(), O_CREAT | O_WRONLY | O_APPEND | O_SYNC, S_IRUSR | S_IWUSR)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    if (std::filesystem::file_size(filepath) == 0) {
        const std::vector emptyRdb{serializeEmptyRdb()};
        if (::write(fileDescriptor, emptyRdb.data(), emptyRdb.size()) == -1) {
            throw Exception{
                Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
            };
        }
    }

    return fileDescriptor;
}

DatabaseManager::DatabaseManager(const int fileDescriptor) : FileDescriptor{fileDescriptor} {
    for (unsigned long i{}; i != databaseCount; ++i) this->databases.emplace_back(i, std::span<const std::byte>{});

    if (std::ifstream file{filepath}; file.is_open()) {
        std::vector<std::byte> buffer{std::filesystem::file_size(filepath)};
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<long>(buffer.size()));
        std::span<const std::byte> bufferSpan{buffer};

        for (unsigned long i{}; i != this->databases.size(); ++i) {
            const auto size{*reinterpret_cast<const unsigned long *>(bufferSpan.data())};
            bufferSpan = bufferSpan.subspan(sizeof(size));

            this->databases[i] = Database{i, bufferSpan.first(size)};
            bufferSpan = bufferSpan.subspan(size);
        }

        Context context;
        while (!bufferSpan.empty()) {
            const auto size{*reinterpret_cast<const unsigned long *>(bufferSpan.data())};
            bufferSpan = bufferSpan.subspan(sizeof(size));

            this->query(context, Answer{bufferSpan.first(size)});
            bufferSpan = bufferSpan.subspan(size);
        }
    }
}

auto DatabaseManager::query(Context &context, Answer &&answer) -> Reply {
    const unsigned long databaseIndex{context.getDatabaseIndex()};

    std::string_view statement{answer.getStatement()}, command;
    if (const unsigned long position{statement.find(' ')}; position != std::string_view::npos) {
        command = statement.substr(0, position);
        statement.remove_prefix(position + 1);
    } else {
        command = statement;
        statement = std::string_view{};
    }

    Reply reply{Reply::Type::nil, 0};
    bool isRecord{};
    if (command == "MULTI") reply = multi(context);
    else if (command == "EXEC") reply = this->exec(context);
    else if (command == "DISCARD") reply = discard(context);
    else if (context.getIsTransaction()) reply = transaction(context, std::move(answer));
    else if (command == "SELECT") {
        reply = select(context, statement);
        isRecord = true;
    } else if (command == "DEL") {
        reply = this->databases[databaseIndex].del(statement);
        isRecord = true;
    } else if (command == "EXISTS") reply = this->databases[databaseIndex].exists(statement);
    else if (command == "MOVE") {
        reply = this->databases[databaseIndex].move(this->databases, statement);
        isRecord = true;
    } else if (command == "RENAME") {
        reply = this->databases[databaseIndex].rename(statement);
        isRecord = true;
    } else if (command == "RENAMENX") {
        reply = this->databases[databaseIndex].renameNx(statement);
        isRecord = true;
    } else if (command == "TYPE") reply = this->databases[databaseIndex].type(statement);
    else if (command == "SET") {
        reply = this->databases[databaseIndex].set(statement);
        isRecord = true;
    } else if (command == "GET") reply = this->databases[databaseIndex].get(statement);
    else if (command == "GETRANGE") reply = this->databases[databaseIndex].getRange(statement);
    else if (command == "GETBIT") reply = this->databases[databaseIndex].getBit(statement);
    else if (command == "MGET") reply = this->databases[databaseIndex].mGet(statement);
    else if (command == "SETBIT") {
        reply = this->databases[databaseIndex].setBit(statement);
        isRecord = true;
    } else if (command == "SETNX") {
        reply = this->databases[databaseIndex].setNx(statement);
        isRecord = true;
    } else if (command == "SETRANGE") {
        reply = this->databases[databaseIndex].setRange(statement);
        isRecord = true;
    } else if (command == "STRLEN") reply = this->databases[databaseIndex].strlen(statement);
    else if (command == "MSET") {
        reply = this->databases[databaseIndex].mSet(statement);
        isRecord = true;
    } else if (command == "MSETNX") {
        reply = this->databases[databaseIndex].mSetNx(statement);
        isRecord = true;
    } else if (command == "INCR") {
        reply = this->databases[databaseIndex].incr(statement);
        isRecord = true;
    } else if (command == "INCRBY") {
        reply = this->databases[databaseIndex].incrBy(statement);
        isRecord = true;
    } else if (command == "DECR") {
        reply = this->databases[databaseIndex].decr(statement);
        isRecord = true;
    } else if (command == "DECRBY") {
        reply = this->databases[databaseIndex].decrBy(statement);
        isRecord = true;
    } else if (command == "APPEND") {
        reply = this->databases[databaseIndex].append(statement);
        isRecord = true;
    } else if (command == "HDEL") {
        reply = this->databases[databaseIndex].hDel(statement);
        isRecord = true;
    } else if (command == "HEXISTS") reply = this->databases[databaseIndex].hExists(statement);
    else if (command == "HGET") reply = this->databases[databaseIndex].hGet(statement);
    else if (command == "HGETALL") reply = this->databases[databaseIndex].hGetAll(statement);
    else if (command == "HINCRBY") {
        reply = this->databases[databaseIndex].hIncrBy(statement);
        isRecord = true;
    } else if (command == "HKEYS") reply = this->databases[databaseIndex].hKeys(statement);
    else if (command == "HLEN") reply = this->databases[databaseIndex].hLen(statement);
    else if (command == "HSET") {
        reply = this->databases[databaseIndex].hSet(statement);
        isRecord = true;
    } else if (command == "HVALS") reply = this->databases[databaseIndex].hVals(statement);
    else if (command == "LINDEX") reply = this->databases[databaseIndex].lIndex(statement);
    else if (command == "LLEN") reply = this->databases[databaseIndex].lLen(statement);
    else if (command == "LPOP") {
        reply = this->databases[databaseIndex].lPop(statement);
        isRecord = true;
    } else if (command == "LPUSH") {
        reply = this->databases[databaseIndex].lPush(statement);
        isRecord = true;
    } else if (command == "LPUSHX") {
        reply = this->databases[databaseIndex].lPushX(statement);
        isRecord = true;
    }
    reply.setDatabaseIndex(context.getDatabaseIndex());
    reply.setIsTransaction(context.getIsTransaction());

    if (isRecord) this->record(answer.serialize());

    return reply;
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

auto DatabaseManager::isCanTruncate() const -> bool {
    return this->seconds == std::chrono::seconds::zero() && !this->writeBuffer.empty();
}

auto DatabaseManager::truncate() const noexcept -> Awaiter {
    return Awaiter{
        Submission{this->getFileDescriptor(), IOSQE_FIXED_FILE, 0, 0, Submission::Truncate{}}
    };
}

auto DatabaseManager::write() const noexcept -> Awaiter {
    return Awaiter{
        Submission{this->getFileDescriptor(), IOSQE_FIXED_FILE, 0, 0, Submission::Write{this->writeBuffer, 0}}
    };
}

auto DatabaseManager::wrote() noexcept -> void { this->writeBuffer.clear(); }

auto DatabaseManager::serializeEmptyRdb() -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (unsigned long i{}; i != databaseCount; ++i) {
        constexpr unsigned long size{};
        const auto bytes{std::as_bytes(std::span{&size, 1})};
        serialization.insert(serialization.cend(), bytes.cbegin(), bytes.cend());
    }

    return serialization;
}

auto DatabaseManager::multi(Context &context) -> Reply {
    context.setIsTransaction(true);

    return {Reply::Type::status, "OK"};
}

auto DatabaseManager::discard(Context &context) -> Reply {
    context.setIsTransaction(false);
    context.clearAnswers();

    return {Reply::Type::status, "OK"};
}

auto DatabaseManager::transaction(Context &context, Answer &&answer) -> Reply {
    context.addAnswer(std::move(answer));

    return {Reply::Type::status, "QUEUED"};
}

auto DatabaseManager::select(Context &context, const std::string_view statement) -> Reply {
    context.setDatabaseIndex(std::stoul(std::string{statement}));

    return {Reply::Type::status, "OK"};
}

auto DatabaseManager::record(const std::span<const std::byte> answer) -> void {
    const std::lock_guard lockGuard{this->lock};

    const unsigned long size{answer.size()};
    const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
    this->aofBuffer.insert(this->aofBuffer.cend(), sizeBytes.cbegin(), sizeBytes.cend());

    this->aofBuffer.insert(this->aofBuffer.cend(), answer.cbegin(), answer.cend());

    ++this->writeCount;
}

auto DatabaseManager::serialize() -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (auto &database : this->databases) {
        const std::vector serializedDatabase{database.serialize()};
        serialization.insert(serialization.cend(), serializedDatabase.cbegin(), serializedDatabase.cend());
    }

    return serialization;
}

auto DatabaseManager::exec(Context &context) -> Reply {
    std::vector<Reply> replies;

    context.setIsTransaction(false);
    for (Answer &answer : context.getAnswers()) replies.emplace_back(Reply{this->query(context, std::move(answer))});
    context.clearAnswers();

    return {Reply::Type::array, std::move(replies)};
}
