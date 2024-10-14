#include "SkipList.hpp"

#include "Entry.hpp"

#include <random>
#include <ranges>
#include <utility>

SkipList::SkipList(std::span<const std::byte> serialization) {
    while (!serialization.empty()) {
        const auto size{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(size));

        this->insert(std::make_shared<Entry>(serialization.subspan(0, size)));
        serialization = serialization.subspan(size);
    }
}

SkipList::SkipList(const SkipList &other) : levels{other.copy()} {}

SkipList::SkipList(SkipList &&other) noexcept : levels{std::exchange(other.levels, {})} {}

auto SkipList::operator=(const SkipList &other) -> SkipList & {
    if (this == &other) return *this;

    this->destroy();

    this->levels = other.copy();

    return *this;
}

auto SkipList::operator=(SkipList &&other) noexcept -> SkipList & {
    if (this == &other) return *this;

    this->destroy();

    this->levels = std::exchange(other.levels, {});

    return *this;
}

SkipList::~SkipList() { this->destroy(); }

auto SkipList::find(const std::string_view key) const noexcept -> std::shared_ptr<Entry> {
    Node *node{this->levels.back()};
    while (node != nullptr) {
        Node *const next{node->next}, *const down{node->down};

        if (key == node->entry->getKey()) return node->entry;

        if (next != nullptr && key >= next->entry->getKey()) node = next;
        else node = down;
    }

    return nullptr;
}

auto SkipList::insert(const std::shared_ptr<Entry> &entry) const -> void {
    const std::string_view key{entry->getKey()};
    Node *previous{};
    for (Node *node{this->levels[this->randomLevel()]}; node != nullptr; node = node->down) {
        while (node->next != nullptr && key >= node->next->entry->getKey()) node = node->next;

        if (key != node->entry->getKey()) {
            node->next = new Node{entry, node->next};

            if (previous != nullptr) previous->down = node->next;
            previous = node->next;
        } else node->entry = entry;
    }
}

auto SkipList::erase(const std::string_view key) const noexcept -> bool {
    bool isSuccess{};

    Node *node{this->levels.back()};
    while (node != nullptr) {
        while (node->next != nullptr && key > node->next->entry->getKey()) node = node->next;

        Node *const down{node->down};
        if (const Node *const next{node->next}; next != nullptr && key == next->entry->getKey()) {
            const Node *const deleteNode{next};
            node->next = next->next;
            delete deleteNode;

            isSuccess = true;
        }

        node = down;
    }

    return isSuccess;
}

auto SkipList::serialize() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;
    for (const Node *node{this->levels.front()->next}; node != nullptr; node = node->next) {
        const std::vector serializedEntry{node->entry->serialize()};

        const unsigned long size{serializedEntry.size()};
        const auto sizeBytes{std::as_bytes(std::span{&size, 1})};
        serialization.insert(serialization.cend(), sizeBytes.cbegin(), sizeBytes.cend());

        serialization.insert(serialization.cend(), serializedEntry.cbegin(), serializedEntry.cend());
    }

    return serialization;
}

auto SkipList::initialize() -> std::array<Node *, 32> {
    std::array<Node *, 32> levels;

    for (Node *previous{}; auto &level : levels | std::views::reverse) {
        level = new Node{std::make_shared<Entry>(std::string{}, std::string{})};

        if (previous != nullptr) previous->down = level;
        previous = level;
    }

    return levels;
}

auto SkipList::copy() const -> std::array<Node *, 32> {
    std::array<Node *, 32> copies;

    for (unsigned char i{}; i != this->levels.size(); ++i) {
        Node *previous{};
        for (const Node *node{this->levels[i]}; node != nullptr; node = node->next) {
            const auto newNode{new Node{std::make_shared<Entry>(*node->entry)}};

            if (previous != nullptr) previous->next = newNode;
            else copies[i] = newNode;
            previous = newNode;
        }
    }
    for (auto up{copies.crbegin()}, down{up + 1}; down != copies.crend(); ++up, ++down) {
        for (Node *upNode{*up}, *downNode{*down}; upNode != nullptr; downNode = downNode->next) {
            if (upNode->entry->getKey() == downNode->entry->getKey()) {
                upNode->down = downNode;
                upNode = upNode->next;
            }
        }
    }

    return copies;
}

auto SkipList::destroy() const noexcept -> void {
    for (const Node *level : this->levels) {
        while (level != nullptr) {
            const Node *const next{level->next};
            delete level;
            level = next;
        }
    }
}

[[nodiscard]] constexpr auto randomZeroOne() -> int {
    static std::mt19937 generator{std::random_device{}()};
    static std::uniform_int_distribution distribution{0, 1};

    return distribution(generator);
}

auto SkipList::randomLevel() const -> unsigned char {
    unsigned char level{};
    while (randomZeroOne() == 0 && level != this->levels.size()) ++level;

    return level;
}
