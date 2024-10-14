#pragma once

#include "../../../common/Answer.hpp"

class Context {
public:
    constexpr Context() noexcept = default;

    [[nodiscard]] auto getDatabaseIndex() const noexcept -> unsigned long;

    auto setDatabaseIndex(unsigned long databaseIndex) noexcept -> void;

    [[nodiscard]] auto getIsTransaction() const noexcept -> bool;

    auto setIsTransaction(bool isTransaction) noexcept -> void;

    auto addAnswer(Answer &&answer) -> void;

    [[nodiscard]] auto getAnswers() noexcept -> std::span<Answer>;

    auto clearAnswers() noexcept -> void;

private:
    unsigned long databaseIndex{};
    bool isTransaction{};
    std::vector<Answer> answers;
};
