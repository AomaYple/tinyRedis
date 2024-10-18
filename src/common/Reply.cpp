#include "Reply.hpp"

Reply::Reply(const Type type, std::variant<long, std::string, std::vector<Reply>> &&value,
             const unsigned long databaseIndex, const bool isTransaction) noexcept :
    databaseIndex{databaseIndex}, isTransaction{isTransaction}, type{type}, value{std::move(value)} {}

Reply::Reply(std::span<const std::byte> data) {
    this->databaseIndex = *reinterpret_cast<const decltype(this->databaseIndex) *>(data.data());
    data = data.subspan(sizeof(this->databaseIndex));

    this->isTransaction = *reinterpret_cast<const decltype(this->isTransaction) *>(data.data());
    data = data.subspan(sizeof(this->isTransaction));

    this->type = *reinterpret_cast<const decltype(this->type) *>(data.data());
    data = data.subspan(sizeof(this->type));

    switch (this->type) {
        case Type::nil:
            break;
        case Type::integer:
            this->deserializeInteger(data);
            break;
        case Type::error:
        case Type::status:
        case Type::string:
            this->deserializeString(data);
            break;
        case Type::array:
            this->deserializeArray(data);
            break;
    }
}

auto Reply::serialize() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    const auto databaseIndexBytes{std::as_bytes(std::span{&this->databaseIndex, 1})};
    serialization.insert(serialization.cend(), databaseIndexBytes.cbegin(), databaseIndexBytes.cend());

    const auto isTransactionBytes{std::as_bytes(std::span{&this->isTransaction, 1})};
    serialization.insert(serialization.cend(), isTransactionBytes.cbegin(), isTransactionBytes.cend());

    const auto typeBytes{std::as_bytes(std::span{&this->type, 1})};
    serialization.insert(serialization.cend(), typeBytes.cbegin(), typeBytes.cend());

    std::vector<std::byte> serializedValue;
    switch (this->type) {
        case Type::nil:
            break;
        case Type::integer:
            serializedValue = this->serializeInteger();
            break;
        case Type::error:
        case Type::status:
        case Type::string:
            serializedValue = this->serializeString();
            break;
        case Type::array:
            serializedValue = this->serializeArray();
            break;
    }
    serialization.insert(serialization.cend(), serializedValue.cbegin(), serializedValue.cend());

    return serialization;
}

auto Reply::getDatabaseIndex() const noexcept -> unsigned long { return this->databaseIndex; }

auto Reply::setDatabaseIndex(const unsigned long databaseIndex) noexcept -> void {
    this->databaseIndex = databaseIndex;
}

auto Reply::getIsTransaction() const noexcept -> bool { return this->isTransaction; }

auto Reply::setIsTransaction(const bool isTransaction) noexcept -> void { this->isTransaction = isTransaction; }

auto Reply::getType() const noexcept -> Type { return this->type; }

auto Reply::getInteger() const -> long { return std::get<long>(this->value); }

auto Reply::getString() const -> std::string_view { return std::get<std::string>(this->value); }

auto Reply::getArray() const -> std::span<const Reply> { return std::get<std::vector<Reply>>(this->value); }

auto Reply::serializeInteger() const -> std::vector<std::byte> {
    const auto bytes{std::as_bytes(std::span{&std::get<long>(this->value), 1})};

    return {bytes.cbegin(), bytes.cend()};
}

auto Reply::serializeString() const -> std::vector<std::byte> {
    const auto bytes{std::as_bytes(std::span{std::get<std::string>(this->value)})};

    return {bytes.cbegin(), bytes.cend()};
}

auto Reply::serializeArray() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;
    for (const auto &reply : std::get<std::vector<Reply>>(this->value)) {
        const std::vector serializedReply{reply.serialize()};

        const unsigned long size{serializedReply.size()};
        const auto bytes{std::as_bytes(std::span{&size, 1})};
        serialization.insert(serialization.cend(), bytes.cbegin(), bytes.cend());

        serialization.insert(serialization.cend(), serializedReply.cbegin(), serializedReply.cend());
    }

    return serialization;
}

auto Reply::deserializeInteger(const std::span<const std::byte> data) -> void {
    this->value = *reinterpret_cast<const long *>(data.data());
}

auto Reply::deserializeString(const std::span<const std::byte> data) -> void {
    this->value = std::string{reinterpret_cast<const char *>(data.data()), data.size()};
}

auto Reply::deserializeArray(std::span<const std::byte> data) -> void {
    std::vector<Reply> replies;
    while (!data.empty()) {
        const auto size{*reinterpret_cast<const unsigned long *>(data.data())};
        data = data.subspan(sizeof(size));

        replies.emplace_back(data.first(size));
        data = data.subspan(size);
    }

    this->value = std::move(replies);
}
