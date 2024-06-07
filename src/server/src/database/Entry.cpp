#include "Entry.hpp"

#include <utility>

template<>
struct std::hash<Entry::SortedSetElement> {
    auto operator()(const Entry::SortedSetElement &other) const noexcept {
        return hash<decltype(other.key)>{}(other.key);
    }
};

auto Entry::SortedSetElement::operator<(const SortedSetElement &other) const noexcept -> bool {
    return this->score < other.score;
}

Entry::Entry(std::string &&key, std::string &&value) noexcept :
    type{Type::string}, key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::unordered_map<std::string, std::string> &&value) noexcept :
    type{Type::hash}, key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::deque<std::string> &&value) noexcept :
    type{Type::list}, key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::unordered_set<std::string> &&value) noexcept :
    type{Type::set}, key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::string &&key, std::set<SortedSetElement> &&value) noexcept :
    type{Type::sortedSet}, key{std::move(key)}, value{std::move(value)} {}

Entry::Entry(std::span<const std::byte> serialization) : type{static_cast<Type>(serialization.front())} {
    serialization = serialization.subspan(sizeof(this->type));

    const auto keySize{*reinterpret_cast<const unsigned long *>(serialization.data())};
    serialization = serialization.subspan(sizeof(keySize));

    this->key = {reinterpret_cast<const char *>(serialization.data()), keySize};
    serialization = serialization.subspan(keySize);

    switch (this->type) {
        case Type::string:
            this->deserializeString(serialization);
            break;
        case Type::hash:
            this->deserializeHash(serialization);
            break;
        case Type::list:
            this->deserializeList(serialization);
            break;
        case Type::set:
            this->deserializeSet(serialization);
            break;
        case Type::sortedSet:
            this->deserializeSortedSet(serialization);
            break;
    }
}

auto Entry::getType() const noexcept -> Type { return this->type; }

auto Entry::getKey() noexcept -> std::string & { return this->key; }

auto Entry::getString() -> std::string & { return std::get<std::string>(this->value); }

auto Entry::getHash() -> std::unordered_map<std::string, std::string> & {
    return std::get<std::unordered_map<std::string, std::string>>(this->value);
}

auto Entry::getList() -> std::deque<std::string> & { return std::get<std::deque<std::string>>(this->value); }

auto Entry::getSet() -> std::unordered_set<std::string> & {
    return std::get<std::unordered_set<std::string>>(this->value);
}

auto Entry::getSortedSet() -> std::set<SortedSetElement> & { return std::get<std::set<SortedSetElement>>(this->value); }

auto Entry::setValue(std::string &&value) noexcept -> void {
    this->type = Type::string;
    this->value = std::move(value);
}

auto Entry::setValue(std::unordered_map<std::string, std::string> &&value) noexcept -> void {
    this->type = Type::hash;
    this->value = std::move(value);
}

auto Entry::setValue(std::deque<std::string> &&value) noexcept -> void {
    this->type = Type::list;
    this->value = std::move(value);
}

auto Entry::setValue(std::unordered_set<std::string> &&value) noexcept -> void {
    this->type = Type::set;
    this->value = std::move(value);
}

auto Entry::setValue(std::set<SortedSetElement> &&value) noexcept -> void {
    this->type = Type::sortedSet;
    this->value = std::move(value);
}

auto Entry::serialize() const -> std::vector<std::byte> {
    std::vector<std::byte> serializedValue;
    switch (this->type) {
        case Type::string:
            serializedValue = this->serializeString();
            break;
        case Type::hash:
            serializedValue = this->serializeHash();
            break;
        case Type::list:
            serializedValue = this->serializeList();
            break;
        case Type::set:
            serializedValue = this->serializeSet();
            break;
        case Type::sortedSet:
            serializedValue = this->serializeSortedSet();
            break;
    }

    const std::vector serializedKey{this->serializeKey()};
    const unsigned long size{sizeof(this->type) + serializedKey.size() + serializedValue.size()};
    std::vector<std::byte> serialization{sizeof(size)};
    *reinterpret_cast<std::remove_reference_t<std::remove_const_t<decltype(size)>> *>(serialization.data()) = size;

    serialization.emplace_back(std::byte{std::to_underlying(this->type)});
    serialization.insert(serialization.cend(), serializedKey.cbegin(), serializedKey.cend());
    serialization.insert(serialization.cend(), serializedValue.cbegin(), serializedValue.cend());

    return serialization;
}

auto Entry::serializeKey() const -> std::vector<std::byte> {
    const unsigned long size{this->key.size()};
    std::vector<std::byte> serialization{sizeof(size)};
    *reinterpret_cast<std::remove_const_t<decltype(size)> *>(serialization.data()) = size;

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

    for (const auto &[elementKey, elementValue] : std::get<std::unordered_map<std::string, std::string>>(this->value)) {
        const unsigned long keySize{elementKey.size()};
        std::vector<std::byte> serializedElement{sizeof(keySize)};
        *reinterpret_cast<std::remove_const_t<decltype(keySize)> *>(serializedElement.data()) = keySize;

        const auto spanKey{std::as_bytes(std::span{elementKey})};
        serializedElement.insert(serializedElement.cend(), spanKey.cbegin(), spanKey.cend());

        const unsigned long valueSize{elementValue.size()};
        serializedElement.resize(serializedElement.size() + sizeof(valueSize));
        *reinterpret_cast<std::remove_const_t<decltype(valueSize)> *>(
            serializedElement.data() + serializedElement.size() - sizeof(valueSize)) = valueSize;

        const auto spanValue{std::as_bytes(std::span{elementValue})};
        serializedElement.insert(serializedElement.cend(), spanValue.cbegin(), spanValue.cend());

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::serializeList() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const std::string_view element : std::get<std::deque<std::string>>(this->value)) {
        const unsigned long size{element.size()};
        std::vector<std::byte> serializedElement{sizeof(size)};
        *reinterpret_cast<std::remove_const_t<decltype(size)> *>(serializedElement.data()) = size;

        const auto spanElement{std::as_bytes(std::span{element})};
        serializedElement.insert(serializedElement.cend(), spanElement.cbegin(), spanElement.cend());

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::serializeSet() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const std::string_view element : std::get<std::unordered_set<std::string>>(this->value)) {
        const unsigned long size{element.size()};
        std::vector<std::byte> serializedElement{sizeof(size)};
        *reinterpret_cast<std::remove_const_t<decltype(size)> *>(serializedElement.data()) = size;

        const auto spanElement{std::as_bytes(std::span{element})};
        serializedElement.insert(serializedElement.cend(), spanElement.cbegin(), spanElement.cend());

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::serializeSortedSet() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const auto &[key, score] : std::get<std::set<SortedSetElement>>(this->value)) {
        const unsigned long size{key.size() + sizeof(score)};
        std::vector<std::byte> serializedElement{sizeof(size)};
        *reinterpret_cast<std::remove_const_t<decltype(size)> *>(serializedElement.data()) = size;

        const auto spanKey{std::as_bytes(std::span{key})};
        serializedElement.insert(serializedElement.cend(), spanKey.cbegin(), spanKey.cend());

        serializedElement.resize(serializedElement.size() + sizeof(score));
        *reinterpret_cast<std::remove_const_t<decltype(score)> *>(serializedElement.data() + serializedElement.size() -
                                                                  sizeof(score)) = score;

        serialization.insert(serialization.cend(), serializedElement.cbegin(), serializedElement.cend());
    }

    return serialization;
}

auto Entry::deserializeString(const std::span<const std::byte> serialization) -> void {
    this->value = std::string{reinterpret_cast<const char *>(serialization.data()), serialization.size()};
}

auto Entry::deserializeHash(std::span<const std::byte> serialization) -> void {
    std::unordered_map<std::string, std::string> value;

    while (!serialization.empty()) {
        const auto elementKeySize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(elementKeySize));

        std::string elementKey{reinterpret_cast<const char *>(serialization.data()), elementKeySize};
        serialization = serialization.subspan(elementKeySize);

        const auto elementValueSize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(elementValueSize));

        value.emplace(std::move(elementKey),
                      std::string{reinterpret_cast<const char *>(serialization.data()), elementValueSize});
        serialization = serialization.subspan(elementValueSize);
    }

    this->value = std::move(value);
}

auto Entry::deserializeList(std::span<const std::byte> serialization) -> void {
    std::deque<std::string> value;

    while (!serialization.empty()) {
        const auto elementSize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(elementSize));

        value.emplace_back(reinterpret_cast<const char *>(serialization.data()), elementSize);
        serialization = serialization.subspan(elementSize);
    }

    this->value = std::move(value);
}

auto Entry::deserializeSet(std::span<const std::byte> serialization) -> void {
    std::unordered_set<std::string> value;

    while (!serialization.empty()) {
        const auto elementSize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(elementSize));

        value.emplace(reinterpret_cast<const char *>(serialization.data()), elementSize);
        serialization = serialization.subspan(elementSize);
    }

    this->value = std::move(value);
}

auto Entry::deserializeSortedSet(std::span<const std::byte> serialization) -> void {
    std::set<SortedSetElement> value;

    while (!serialization.empty()) {
        const auto elementSize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(elementSize));

        const unsigned long elementKeySize{elementSize - sizeof(double)};
        std::string elementKey{reinterpret_cast<const char *>(serialization.data()), elementKeySize};
        serialization = serialization.subspan(elementKeySize);

        const auto elementScore{*reinterpret_cast<const double *>(serialization.data())};
        serialization = serialization.subspan(sizeof(elementScore));

        value.emplace(std::move(elementKey), elementScore);
    }

    this->value = std::move(value);
}
