#include "Entry.hpp"

template<>
struct std::hash<Entry::SortedSetElement> {
    [[nodiscard]] constexpr auto operator()(const Entry::SortedSetElement &other) const noexcept {
        return hash<decltype(other.key)>{}(other.key);
    }
};

auto Entry::SortedSetElement::operator<=>(const SortedSetElement &other) const noexcept -> std::strong_ordering {
    if (this->score < other.score) return std::strong_ordering::less;
    if (this->score > other.score) return std::strong_ordering::greater;

    return std::strong_ordering::equal;
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

Entry::Entry(std::span<const std::byte> serialization) {
    this->type = *reinterpret_cast<const decltype(this->type) *>(serialization.data());
    serialization = serialization.subspan(sizeof(this->type));

    const auto size{*reinterpret_cast<const unsigned long *>(serialization.data())};
    serialization = serialization.subspan(sizeof(size));

    this->key = std::string{reinterpret_cast<const char *>(serialization.data()), size};
    serialization = serialization.subspan(size);

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

auto Entry::getKey() const noexcept -> std::string_view { return this->key; }

auto Entry::setKey(std::string &&key) noexcept -> void { this->key = std::move(key); }

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
    std::vector<std::byte> serialization;

    const auto typeBytes{std::as_bytes(std::span{&this->type, 1})};
    serialization.insert(serialization.cend(), typeBytes.cbegin(), typeBytes.cend());

    const std::vector serializedKey{this->serializeKey()};

    const unsigned long size{serializedKey.size()};
    const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
    serialization.insert(serialization.cend(), sizeBytes.cbegin(), sizeBytes.cend());

    serialization.insert(serialization.cend(), serializedKey.cbegin(), serializedKey.cend());

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
    serialization.insert(serialization.cend(), serializedValue.cbegin(), serializedValue.cend());

    return serialization;
}

auto Entry::serializeKey() const -> std::vector<std::byte> {
    const auto bytes{std::as_bytes(std::span{this->key})};

    return {bytes.cbegin(), bytes.cend()};
}

auto Entry::serializeString() const -> std::vector<std::byte> {
    const auto bytes{std::as_bytes(std::span{std::get<std::string>(this->value)})};

    return {bytes.cbegin(), bytes.cend()};
}

auto Entry::serializeHash() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const auto &[key, value] : std::get<std::unordered_map<std::string, std::string>>(this->value)) {
        const unsigned long keySize{key.size()};
        const auto keySizeBytes{std::as_bytes(std::span{&keySize, 1})};
        serialization.insert(serialization.cend(), keySizeBytes.cbegin(), keySizeBytes.cend());

        const auto keyBytes{std::as_bytes(std::span{key})};
        serialization.insert(serialization.cend(), keyBytes.cbegin(), keyBytes.cend());

        const unsigned long valueSize{value.size()};
        const auto valueSizeBytes{std::as_bytes(std::span{&valueSize, 1})};
        serialization.insert(serialization.cend(), valueSizeBytes.cbegin(), valueSizeBytes.cend());

        const auto valueBytes{std::as_bytes(std::span{value})};
        serialization.insert(serialization.cend(), valueBytes.cbegin(), valueBytes.cend());
    }

    return serialization;
}

auto Entry::serializeList() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const std::string_view value : std::get<std::deque<std::string>>(this->value)) {
        const unsigned long size{value.size()};
        const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
        serialization.insert(serialization.cend(), sizeBytes.cbegin(), sizeBytes.cend());

        const auto valueBytes{std::as_bytes(std::span{value})};
        serialization.insert(serialization.cend(), valueBytes.cbegin(), valueBytes.cend());
    }

    return serialization;
}

auto Entry::serializeSet() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const std::string_view value : std::get<std::unordered_set<std::string>>(this->value)) {
        const unsigned long size{value.size()};
        const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
        serialization.insert(serialization.cend(), sizeBytes.cbegin(), sizeBytes.cend());

        const auto valueBytes{std::as_bytes(std::span{value})};
        serialization.insert(serialization.cend(), valueBytes.cbegin(), valueBytes.cend());
    }

    return serialization;
}

auto Entry::serializeSortedSet() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    for (const auto &[value, score] : std::get<std::set<SortedSetElement>>(this->value)) {
        const unsigned long size{value.size()};
        const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
        serialization.insert(serialization.cend(), sizeBytes.cbegin(), sizeBytes.cend());

        const auto valueBytes{std::as_bytes(std::span{value})};
        serialization.insert(serialization.cend(), valueBytes.cbegin(), valueBytes.cend());

        const auto scoreBytes{std::as_bytes(std::span{&score, 1})};
        serialization.insert(serialization.cend(), scoreBytes.cbegin(), scoreBytes.cend());
    }

    return serialization;
}

auto Entry::deserializeString(const std::span<const std::byte> serialization) -> void {
    this->value = std::string{reinterpret_cast<const char *>(serialization.data()), serialization.size()};
}

auto Entry::deserializeHash(std::span<const std::byte> serialization) -> void {
    std::unordered_map<std::string, std::string> value;

    while (!serialization.empty()) {
        const auto keySize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(keySize));

        std::string key{reinterpret_cast<const char *>(serialization.data()), keySize};
        serialization = serialization.subspan(keySize);

        const auto valueSize{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(valueSize));

        value.emplace(std::move(key), std::string{reinterpret_cast<const char *>(serialization.data()), valueSize});
        serialization = serialization.subspan(valueSize);
    }

    this->value = std::move(value);
}

auto Entry::deserializeList(std::span<const std::byte> serialization) -> void {
    std::deque<std::string> value;

    while (!serialization.empty()) {
        const auto size{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(size));

        value.emplace_back(reinterpret_cast<const char *>(serialization.data()), size);
        serialization = serialization.subspan(size);
    }

    this->value = std::move(value);
}

auto Entry::deserializeSet(std::span<const std::byte> serialization) -> void {
    std::unordered_set<std::string> value;

    while (!serialization.empty()) {
        const auto size{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(size));

        value.emplace(reinterpret_cast<const char *>(serialization.data()), size);
        serialization = serialization.subspan(size);
    }

    this->value = std::move(value);
}

auto Entry::deserializeSortedSet(std::span<const std::byte> serialization) -> void {
    std::set<SortedSetElement> value;

    while (!serialization.empty()) {
        const auto size{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(size));

        std::string key{reinterpret_cast<const char *>(serialization.data()), size};
        serialization = serialization.subspan(size);

        const auto score{*reinterpret_cast<const double *>(serialization.data())};
        serialization = serialization.subspan(sizeof(score));

        value.emplace(std::move(key), score);
    }

    this->value = std::move(value);
}
