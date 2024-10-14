#include "Context.hpp"

auto Context::getDatabaseIndex() const noexcept -> unsigned long { return this->databaseIndex; }

auto Context::setDatabaseIndex(const unsigned long databaseIndex) noexcept -> void {
    this->databaseIndex = databaseIndex;
}

auto Context::getIsTransaction() const noexcept -> bool { return this->isTransaction; }

auto Context::setIsTransaction(const bool isTransaction) noexcept -> void { this->isTransaction = isTransaction; }

auto Context::addAnswer(Answer &&answer) -> void { this->answers.emplace_back(std::move(answer)); }

auto Context::getAnswers() noexcept -> std::span<Answer> { return this->answers; }

auto Context::clearAnswers() noexcept -> void { this->answers.clear(); }
