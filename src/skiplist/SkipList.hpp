#pragma once

#include "Entry.hpp"

class SkipList {
    struct Node {
        std::variant<std::string_view, Entry> data;
        Node *next{}, *down{};
    };

public:
    constexpr SkipList() noexcept = default;

    SkipList(const SkipList &);

    SkipList(SkipList &&) noexcept;

    auto operator=(const SkipList &) -> SkipList &;

    auto operator=(SkipList &&) noexcept -> SkipList &;

    ~SkipList();

    [[nodiscard]] auto find(std::string_view key) const -> Entry &;

private:
    [[nodiscard]] static auto random() -> double;

    [[nodiscard]] static auto randomLevel() -> unsigned char;

    [[nodiscard]] auto copy() const -> Node *;

    auto destroy() noexcept -> void;

    Node *start{};
};
