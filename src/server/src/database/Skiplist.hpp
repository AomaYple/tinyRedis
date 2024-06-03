#pragma once

#include "Entry.hpp"

#include <memory>

class Skiplist {
    struct Node {
        unsigned char level;
        std::shared_ptr<Entry> entry;
        Node *next{}, *down{};
    };

public:
    Skiplist() = default;

    explicit Skiplist(std::span<const std::byte> serialization);

    Skiplist(const Skiplist &);

    Skiplist(Skiplist &&) noexcept;

    auto operator=(const Skiplist &) -> Skiplist &;

    auto operator=(Skiplist &&) noexcept -> Skiplist &;

    ~Skiplist();

    [[nodiscard]] auto find(std::string_view key) const noexcept -> std::shared_ptr<Entry>;

    auto insert(std::shared_ptr<Entry> &&entry) const -> void;

    auto erase(std::string_view key) const noexcept -> bool;

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

private:
    [[nodiscard]] static auto initlialize() -> Node *;

    [[nodiscard]] static auto random() -> double;

    [[nodiscard]] static auto randomLevel() -> unsigned char;

    [[nodiscard]] auto copy() const -> Node *;

    auto destroy() noexcept -> void;

    static constexpr unsigned char maxLevel{32};

    Node *start{initlialize()};
};
