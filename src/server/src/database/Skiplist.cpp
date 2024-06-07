#include "Skiplist.hpp"

#include <random>
#include <utility>

Skiplist::Skiplist(std::span<const std::byte> serialization) {
    while (!serialization.empty()) {
        const auto size{*reinterpret_cast<const unsigned long *>(serialization.data())};
        serialization = serialization.subspan(sizeof(size));

        this->insert(std::make_shared<Entry>(serialization.subspan(0, size)));
        serialization = serialization.subspan(size);
    }
}

Skiplist::Skiplist(const Skiplist &other) : start{other.copy()} {}

Skiplist::Skiplist(Skiplist &&other) noexcept : start{std::exchange(other.start, nullptr)} {}

auto Skiplist::operator=(const Skiplist &other) -> Skiplist & {
    if (this == &other) return *this;

    this->destroy();

    this->start = other.copy();

    return *this;
}

auto Skiplist::operator=(Skiplist &&other) noexcept -> Skiplist & {
    if (this == &other) return *this;

    this->destroy();

    this->start = std::exchange(other.start, nullptr);

    return *this;
}

Skiplist::~Skiplist() { this->destroy(); }

auto Skiplist::find(const std::string_view key) const noexcept -> std::shared_ptr<Entry> {
    Node *node{this->start};
    while (node != nullptr) {
        Node *const next{node->next}, *const down{node->down};

        if (key == node->entry->getKey()) return node->entry;

        if (next != nullptr && key >= next->entry->getKey()) node = next;
        else node = down;
    }

    return nullptr;
}

auto Skiplist::insert(std::shared_ptr<Entry> &&entry) const -> void {
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

auto Skiplist::erase(const std::string_view key) const noexcept -> bool {
    bool success{};

    Node *node{this->start};
    while (node != nullptr) {
        while (node->next != nullptr && key > node->next->entry->getKey()) node = node->next;

        Node *const down{node->down};
        if (const Node *const next{node->next}; next != nullptr && key == next->entry->getKey()) {
            const Node *const deleteNode{next};
            node->next = next->next;
            delete deleteNode;

            success = true;
        }

        node = down;
    }

    return success;
}

auto Skiplist::serialize() const -> std::vector<std::byte> {
    const Node *node{this->start};
    while (node != nullptr && node->down != nullptr) node = node->down;

    std::vector<std::byte> serialization{sizeof(unsigned long)};
    while (node != nullptr) {
        const std::vector serializedEntry{node->entry->serialize()};
        serialization.insert(serialization.cend(), serializedEntry.cbegin(), serializedEntry.cend());

        node = node->next;
    }
    *reinterpret_cast<unsigned long *>(serialization.data()) = serialization.size() - sizeof(unsigned long);

    return serialization;
}

auto Skiplist::initlialize() -> Node * {
    Node *start{}, *previous{};
    const auto entry{std::make_shared<Entry>(std::string{}, std::string{})};
    for (unsigned char i{maxLevel}; i > 0; --i) {
        const auto node{
            new Node{static_cast<decltype(i)>(i - 1), entry}
        };
        if (previous != nullptr) {
            previous->down = node;
            previous = previous->down;
        } else start = previous = node;
    }

    return start;
}

auto Skiplist::random() -> double {
    static std::mt19937 generator{std::random_device{}()};
    static std::uniform_real_distribution<> distribution{0, 1};

    return distribution(generator);
}

auto Skiplist::randomLevel() -> unsigned char {
    unsigned char level{};
    while (random() < 0.5 && level < maxLevel) ++level;

    return level;
}

auto Skiplist::copy() const -> Node * {
    const Node *node{this->start};

    Node *newStart{}, *newPrevious{};
    while (node != nullptr) {
        const Node *const down{node->down};

        Node *subNewNode{}, *subNewPrevious{};
        while (node != nullptr) {
            const auto newNode{
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

auto Skiplist::destroy() noexcept -> void {
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
