#pragma once

#include "Entry.hpp"

class SkipList {
    struct Node {
        std::variant<std::string_view, Entry> data;
        Node *next{}, *down{};
    };

public:
    constexpr SkipList() noexcept = default;

    SkipList(const SkipList &) = delete;

private:
    static auto destroy(const Node *node) -> void;

    Node *start{};
};
