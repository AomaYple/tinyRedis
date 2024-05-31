#include "Entry.hpp"

template<>
struct std::hash<Entry::SortedSetElement> {
    auto operator()(const Entry::SortedSetElement &other) const noexcept -> size_t { return hash<string>{}(other.key); }
};

auto Entry::SortedSetElement::operator<(const SortedSetElement &other) const noexcept -> bool {
    return this->score < other.score;
}

Entry::Entry(std::string &&key, std::string &&value) noexcept : key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::deque<std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::unordered_set<std::string> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::set<SortedSetElement> &&value) noexcept :
    key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::span<const std::byte> serializedEntry) {
    const auto valueType{static_cast<unsigned char>(serializedEntry.front())};
    serializedEntry = serializedEntry.subspan(sizeof(valueType));

    const auto keySize{*reinterpret_cast<const unsigned long *>(serializedEntry.data())};
    serializedEntry = serializedEntry.subspan(sizeof(keySize));

    this->key = {reinterpret_cast<const char *>(serializedEntry.data()), keySize};
    serializedEntry = serializedEntry.subspan(keySize);

    switch (valueType) {
        case 0:
            this->deserializeString(serializedEntry);
            break;
        case 1:
            this->deserializeHash(serializedEntry);
            break;
        case 2:
            this->deserializeList(serializedEntry);
            break;
        case 3:
            this->deserializeSet(serializedEntry);
            break;
        case 4:
            this->deserializeSortedSet(serializedEntry);
            break;
    }
}

auto Entry::getKey() noexcept -> std::string & { return this->key; }

auto Entry::getValueType() const noexcept -> unsigned char { return this->value.index(); }

auto Entry::getStringValue() -> std::string & { return std::get<std::string>(this->value); }

auto Entry::getHashValue() -> std::unordered_map<std::string, std::string> & {
    return std::get<std::unordered_map<std::string, std::string>>(this->value);
}

auto Entry::getListValue() -> std::deque<std::string> & { return std::get<std::deque<std::string>>(this->value); }

auto Entry::getSetValue() -> std::unordered_set<std::string> & {
    return std::get<std::unordered_set<std::string>>(this->value);
}

auto Entry::getSortedSetValue() -> std::set<SortedSetElement> & {
    return std::get<std::set<SortedSetElement>>(this->value);
}

auto Entry::serialize() const -> std::vector<std::byte> {
    std::vector<std::byte> serializedValue;
    const unsigned char valueType{this->getValueType()};
    switch (valueType) {
        case 0:
            serializedValue = this->serializeString();
            break;
        case 1:
            serializedValue = this->serializeHash();
            break;
        case 2:
            serializedValue = this->serializeList();
            break;
        case 3:
            serializedValue = this->serializeSet();
            break;
        case 4:
            serializedValue = this->serializeSortedSet();
            break;
    }

    const std::vector serializedKey{this->serializeKey()};
    const unsigned long size{sizeof(valueType) + serializedKey.size() + serializedValue.size()};
    std::vector<std::byte> serialization{sizeof(size)};
    *reinterpret_cast<unsigned long *>(serialization.data()) = size;

    serialization.emplace_back(std::byte{valueType});
    serialization.insert(serialization.cend(), serializedKey.cbegin(), serializedKey.cend());
    serialization.insert(serialization.cend(), serializedValue.cbegin(), serializedValue.cend());

    return serialization;
}

auto Entry::serializeKey() const -> std::vector<std::byte> {
    const unsigned long size{this->key.size()};
    std::vector<std::byte> serialization{sizeof(size)};
    *reinterpret_cast<unsigned long *>(serialization.data()) = size;

    const auto spanKey{std::as_bytes(std::span{this->key})};
    serialization.insert(serialization.cend(), spanKey.cbegin(), spanKey.cend());

    return serialization;
}

auto Entry::serializeString() const -> std::vector<std::byte> {
    const auto spanValue{std::as_bytes(std::span{std::get<std::string>(this->value)})};

    return {spanValue.cbegin(), spanValue.cend()};
}

auto Entry::serializeHash() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;
    const auto &value{std::get<std::unordered_map<std::string, std::string>>(this->value)};

    for (const auto &element : value) {
        const unsigned long keySize{element.first.size()};
        std::vector<std::byte> serializedElement{sizeof(keySize)};
        *reinterpret_cast<unsigned long *>(serializedElement.data()) = keySize;

        const auto spanKey{std::as_bytes(std::span{element.first})};
        serializedElement.insert(serializedElement.cend(), spanKey.cbegin(), spanKey.cend());

        const unsigned long valueSize{element.second.size()};
        serializedElement.resize(serializedElement.size() + sizeof(valueSize));
        *reinterpret_cast<unsigned long *>(serializedElement.data() + serializedElement.size() - sizeof(valueSize)) =
            valueSize;

        const auto spanValue{std::as_bytes(std::span{element.second})};
        serializedElement.insert(serializedElement.cend(), spanValue.cbegin(), spanValue.cend());

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::serializeList() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;
    const auto &value{std::get<std::deque<std::string>>(this->value)};

    for (const std::string_view element : value) {
        const unsigned long size{element.size()};
        std::vector<std::byte> serializedElement{sizeof(size)};
        *reinterpret_cast<unsigned long *>(serializedElement.data()) = size;

        const auto spanElement{std::as_bytes(std::span{element})};
        serializedElement.insert(serializedElement.cend(), spanElement.cbegin(), spanElement.cend());

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::serializeSet() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;
    const auto &value{std::get<std::unordered_set<std::string>>(this->value)};

    for (const std::string_view element : value) {
        const unsigned long size{element.size()};
        std::vector<std::byte> serializedElement{sizeof(size)};
        *reinterpret_cast<unsigned long *>(serializedElement.data()) = size;

        const auto spanElement{std::as_bytes(std::span{element})};
        serializedElement.insert(serializedElement.cend(), spanElement.cbegin(), spanElement.cend());

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::serializeSortedSet() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;
    const auto &value{std::get<std::set<SortedSetElement>>(this->value)};

    for (const auto &element : value) {
        const unsigned long size{element.key.size() + sizeof(element.score)};
        std::vector<std::byte> serializedElement{sizeof(size)};
        *reinterpret_cast<unsigned long *>(serializedElement.data()) = size;

        const auto spanKey{std::as_bytes(std::span{element.key})};
        serializedElement.insert(serializedElement.cend(), spanKey.cbegin(), spanKey.cend());

        serializedElement.resize(serializedElement.size() + sizeof(element.score));
        *reinterpret_cast<decltype(element.score) *>(serializedElement.data() + serializedElement.size() -
                                                     sizeof(element.score)) = element.score;

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::deserializeString(std::span<const std::byte> serializedValue) -> void {
    this->value = std::string{reinterpret_cast<const char *>(serializedValue.data()), serializedValue.size()};
}

auto Entry::deserializeHash(std::span<const std::byte> serializedValue) -> void {
    std::unordered_map<std::string, std::string> value;

    while (!serializedValue.empty()) {
        const auto elementKeySize{*reinterpret_cast<const unsigned long *>(serializedValue.data())};
        serializedValue = serializedValue.subspan(sizeof(elementKeySize));

        std::string elementKey{reinterpret_cast<const char *>(serializedValue.data()), elementKeySize};
        serializedValue = serializedValue.subspan(elementKeySize);

        const auto elementValueSize{*reinterpret_cast<const unsigned long *>(serializedValue.data())};
        serializedValue = serializedValue.subspan(sizeof(elementValueSize));

        std::string elementValue{reinterpret_cast<const char *>(serializedValue.data()), elementValueSize};
        serializedValue = serializedValue.subspan(elementValueSize);

        value.emplace(std::move(elementKey), std::move(elementValue));
    }

    this->value = std::move(value);
}

auto Entry::deserializeList(std::span<const std::byte> serializedValue) -> void {
    std::deque<std::string> value;

    while (!serializedValue.empty()) {
        const auto elementSize{*reinterpret_cast<const unsigned long *>(serializedValue.data())};
        serializedValue = serializedValue.subspan(sizeof(elementSize));

        std::string element{reinterpret_cast<const char *>(serializedValue.data()), elementSize};
        serializedValue = serializedValue.subspan(elementSize);

        value.emplace_back(std::move(element));
    }

    this->value = std::move(value);
}

auto Entry::deserializeSet(std::span<const std::byte> serializedValue) -> void {
    std::unordered_set<std::string> value;

    while (!serializedValue.empty()) {
        const auto elementSize{*reinterpret_cast<const unsigned long *>(serializedValue.data())};
        serializedValue = serializedValue.subspan(sizeof(elementSize));

        std::string element{reinterpret_cast<const char *>(serializedValue.data()), elementSize};
        serializedValue = serializedValue.subspan(elementSize);

        value.emplace(std::move(element));
    }

    this->value = std::move(value);
}

auto Entry::deserializeSortedSet(std::span<const std::byte> serializedValue) -> void {
    std::set<SortedSetElement> value;

    while (!serializedValue.empty()) {
        const auto elementSize{*reinterpret_cast<const unsigned long *>(serializedValue.data())};
        serializedValue = serializedValue.subspan(sizeof(elementSize));

        const unsigned long elementKeySize{elementSize - sizeof(double)};
        std::string elementKey{reinterpret_cast<const char *>(serializedValue.data()), elementKeySize};
        serializedValue = serializedValue.subspan(elementKeySize);

        const auto elementScore{*reinterpret_cast<const double *>(serializedValue.data())};
        serializedValue = serializedValue.subspan(sizeof(elementScore));

        value.emplace(SortedSetElement{std::move(elementKey), elementScore});
    }

    this->value = std::move(value);
}
