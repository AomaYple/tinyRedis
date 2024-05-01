#include "SkipList.hpp"

#include "../exception/Exception.hpp"

#include <random>
#include <utility>

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

auto SkipList::find(std::string_view key) const -> Entry & {
    Node *node{this->start};
    while (node != nullptr) {
        Node *const next{node->next}, *const down{node->down};

        if (down == nullptr && key == std::get<std::string_view>(node->data)) return std::get<Entry>(node->data);
        if (next != nullptr && key >= std::get<std::string_view>(next->data)) node = next;
        else node = down;
    }

    throw Exception{"key not found"};
}

auto SkipList::insert(Entry &&entry) -> void {
    const std::string_view key{entry.getKey()};
    unsigned char level{SkipList::randomLevel()};
    Node *node{this->start}, *previous{};

    while (node != nullptr && level > node->level) {
        if (previous != nullptr) previous->down = new Node{key, level, nullptr, node};
        else previous = new Node{key, level, nullptr, node};

        --level;
    }
    while (node != nullptr && node->level > level) node = node->down;

    while (node != nullptr && node->down != nullptr) {
        Node *const next{node->next};

        while (next != nullptr && key >= std::get<std::string_view>(next->data)) node = next;
        node->next = new Node{key, level, next};

        if (previous != nullptr) previous->down = node->next;
        else previous = node->next;

        node = node->down;
    }

    while (node != nullptr) node = node->next;
}

auto SkipList::random() -> double {
    static std::mt19937 generator{std::random_device{}()};
    static std::uniform_real_distribution<> distribution{0, 1};

    return distribution(generator);
}

auto SkipList::randomLevel() -> unsigned char {
    unsigned char level{};
    while (SkipList::random() < 0.5 && level < 32) ++level;

    return level;
}

auto SkipList::copy() const -> Node * {
    const Node *node{this->start};
    Node *newStart{}, *newDown{};

    while (node != nullptr) {
        const Node *const down{node->down};
        Node *newNode{};

        while (node != nullptr) {
            if (newNode == nullptr) {
                newNode = new Node{node->data};

                if (newDown != nullptr) newDown->down = newNode;
                else newDown = newNode;
            } else {
                newNode->next = new Node{node->data};
                newNode = newNode->next;
            }
            if (newStart == nullptr) newStart = newNode;

            node = node->next;
        }

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
