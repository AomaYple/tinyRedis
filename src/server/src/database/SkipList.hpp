#pragma once

#include <memory>
#include <vector>

class Entry;

class SkipList {
    struct Node {
        unsigned char level;
        std::shared_ptr<Entry> entry;
        Node *next{}, *down{};
    };

public:
    SkipList() = default;

    explicit SkipList(std::span<const std::byte> serialization);

    SkipList(const SkipList &);

    SkipList(SkipList &&) noexcept;

    auto operator=(const SkipList &) -> SkipList &;

    auto operator=(SkipList &&) noexcept -> SkipList &;

    ~SkipList();

    [[nodiscard]] auto find(std::string_view key) const noexcept -> std::shared_ptr<Entry>;

    auto insert(std::shared_ptr<Entry> &&entry) const -> void;

    auto erase(std::string_view key) const noexcept -> bool;

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

private:
    [[nodiscard]] static auto initialize() -> Node *;

    [[nodiscard]] static auto random() -> int;

    [[nodiscard]] static auto randomLevel() -> unsigned char;

    [[nodiscard]] auto copy() const -> Node *;

    auto destroy() noexcept -> void;

    static constexpr unsigned char maxLevel{32};

    Node *start{initialize()};
};
