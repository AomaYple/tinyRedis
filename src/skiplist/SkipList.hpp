#pragma once

#include "Entry.hpp"

#include <memory>

class SkipList {
    struct Node {
        unsigned char level;
        std::shared_ptr<Entry> entry;
        Node *next{}, *down{};
    };

public:
    SkipList();

    SkipList(const SkipList &);

    SkipList(SkipList &&) noexcept;

    auto operator=(const SkipList &) -> SkipList &;

    auto operator=(SkipList &&) noexcept -> SkipList &;

    ~SkipList();

    [[nodiscard]] auto find(std::string_view key) const noexcept -> std::shared_ptr<Entry>;

    auto insert(std::shared_ptr<Entry> &&entry) const -> void;

    auto erase(std::string_view key) const noexcept -> void;

private:
    [[nodiscard]] static auto random() -> double;

    [[nodiscard]] static auto randomLevel() -> unsigned char;

    [[nodiscard]] auto copy() const -> Node *;

    auto destroy() noexcept -> void;

    static constexpr unsigned char maxLevel{32};

    Node *start;
};
