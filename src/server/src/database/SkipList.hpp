#pragma once

#include <memory>
#include <vector>

class Entry;

class SkipList {
    struct Node {
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

    auto insert(const std::shared_ptr<Entry> &entry) const -> void;

    auto erase(std::string_view key) const noexcept -> bool;

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

private:
    [[nodiscard]] static auto initialize() -> std::array<Node *, 32>;

    [[nodiscard]] auto copy() const -> std::array<Node *, 32>;

    auto destroy() const noexcept -> void;

    [[nodiscard]] static auto random() -> int;

    [[nodiscard]] auto randomLevel() const -> unsigned char;

    std::array<Node *, 32> levels{initialize()};
};
