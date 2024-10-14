#pragma once

#include <span>
#include <string>
#include <vector>

class Answer {
public:
    explicit Answer(std::string &&statement) noexcept;

    explicit Answer(std::span<const std::byte> data);

    [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

    [[nodiscard]] auto getStatement() const noexcept -> std::string_view;

private:
    std::string statement;
};
