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
        if (key < std::get<std::string_view>(node->data)) break;

        if (node->down != nullptr) {
            Node *next{node->next};
            if (next == nullptr || key < std::get<std::string_view>(next->data)) node = node->down;
            else node = next;
        } else {
            if (key == std::get<std::string_view>(node->data)) return std::get<Entry>(node->data);
            else node = node->next;
        }
    }

    throw Exception{"key not found"};
}

auto SkipList::random() -> double {
    static std::mt19937 generator{std::random_device{}()};
    static std::uniform_real_distribution<> distribution{0, 1};

    return distribution(generator);
}

auto SkipList::randomLevel() -> unsigned char {
    unsigned char level{1};
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
