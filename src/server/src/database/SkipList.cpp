#include "SkipList.hpp"

#include <random>
#include <utility>

SkipList::SkipList(std::span<const std::byte> serializedSkipList) {
    while (!serializedSkipList.empty()) {
        const auto entrySize{*reinterpret_cast<const unsigned long *>(serializedSkipList.data())};
        serializedSkipList = serializedSkipList.subspan(sizeof(entrySize));

        auto entry{std::make_shared<Entry>(serializedSkipList.subspan(0, entrySize))};
        serializedSkipList = serializedSkipList.subspan(entrySize);

        this->insert(std::move(entry));
    }
}

SkipList::SkipList(const SkipList &other) : start{other.copy()} {}

SkipList::SkipList(SkipList &&other) noexcept : start{std::exchange(other.start, nullptr)} {}

auto SkipList::operator=(const SkipList &other) -> SkipList & {
    if (this == &other) return *this;

    this->destroy();

    this->start = other.copy();

    return *this;
}

auto SkipList::operator=(SkipList &&other) noexcept -> SkipList & {
    if (this == &other) return *this;

    this->destroy();

    this->start = std::exchange(other.start, nullptr);

    return *this;
}

SkipList::~SkipList() { this->destroy(); }

auto SkipList::find(std::string_view key) const noexcept -> std::shared_ptr<Entry> {
    Node *node{this->start};
    while (node != nullptr) {
        Node *const next{node->next}, *const down{node->down};

        if (key == node->entry->getKey()) return node->entry;

        if (next != nullptr && key >= next->entry->getKey()) node = next;
        else node = down;
    }

    return nullptr;
}

auto SkipList::insert(std::shared_ptr<Entry> &&entry) const -> void {
    Node *node{this->start};
    for (unsigned char level{randomLevel()}; node != nullptr && level < node->level; --level) node = node->down;

    const std::string_view key{entry->getKey()};
    Node *previous{};
    while (node != nullptr) {
        while (node->next != nullptr && key >= node->next->entry->getKey()) node = node->next;

        if (key != node->entry->getKey()) node->next = new Node{node->level, entry, node->next};
        else node->entry = entry;

        if (previous != nullptr) {
            previous->down = node->next;
            previous = previous->down;
        } else previous = node->next;

        node = node->down;
    }
}

auto SkipList::erase(std::string_view key) const noexcept -> void {
    Node *node{this->start};
    while (node != nullptr) {
        while (node->next != nullptr && key > node->next->entry->getKey()) node = node->next;

        Node *const next{node->next}, *const down{node->down};
        if (next != nullptr && key == next->entry->getKey()) {
            const Node *const deleteNode{next};
            node->next = next->next;
            delete deleteNode;
        } else [[likely]] {
            node = down;
        }
    }
}

auto SkipList::serialize() const -> std::vector<std::byte> {
    const Node *node{this->start};
    while (node != nullptr && node->down != nullptr) node = node->down;

    std::vector<std::byte> serialization;
    while (node != nullptr) {
        const std::vector serializedEntry{node->entry->serialize()};
        serialization.insert(serialization.cend(), serializedEntry.cbegin(), serializedEntry.cend());

        node = node->next;
    }

    return serialization;
}

auto SkipList::initlialize() -> Node * {
    Node *start{}, *previous{};
    const auto entry{std::make_shared<Entry>(std::string{}, std::string{})};
    for (unsigned char i{maxLevel}; i > 0; --i) {
        Node *const node{
            new Node{static_cast<unsigned char>(i - 1), entry}
        };
        if (previous != nullptr) {
            previous->down = node;
            previous = previous->down;
        } else start = previous = node;
    }

    return start;
}

auto SkipList::random() -> double {
    static std::mt19937 generator{std::random_device{}()};
    static std::uniform_real_distribution<> distribution{0, 1};

    return distribution(generator);
}

auto SkipList::randomLevel() -> unsigned char {
    unsigned char level{};
    while (random() < 0.5 && level < maxLevel) ++level;

    return level;
}

auto SkipList::copy() const -> Node * {
    const Node *node{this->start};

    Node *newStart{}, *newPrevious{};
    while (node != nullptr) {
        const Node *const down{node->down};

        Node *subNewNode{}, *subNewPrevious{};
        while (node != nullptr) {
            Node *const newNode{
                new Node{node->level, std::make_shared<Entry>(*node->entry)}
            };

            if (subNewPrevious != nullptr) {
                subNewPrevious->next = newNode;
                subNewPrevious = subNewPrevious->next;
            } else subNewNode = subNewPrevious = newNode;

            node = node->next;
        }

        if (newPrevious != nullptr) {
            newPrevious->down = subNewNode;
            newPrevious = newPrevious->down;
        } else newStart = newPrevious = subNewNode;

        node = down;
    }

    return newStart;
}

auto SkipList::destroy() noexcept -> void {
    const Node *node{this->start};
    while (node != nullptr) {
        const Node *const down{node->down};
        while (node != nullptr) {
            const Node *const next{node->next};
            delete node;
            node = next;
        }
        node = down;
    }

    this->start = nullptr;
}
