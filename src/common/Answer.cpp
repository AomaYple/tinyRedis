#include "Answer.hpp"

Answer::Answer(std::string &&statement) noexcept : statement{std::move(statement)} {}

Answer::Answer(const std::span<const std::byte> data) {
    this->statement = std::string{reinterpret_cast<const char *>(data.data()), data.size()};
}

auto Answer::serialize() const -> std::vector<std::byte> {
    std::vector<std::byte> serialization;

    const auto bytes{std::as_bytes(std::span{this->statement})};
    serialization.insert(serialization.cend(), bytes.cbegin(), bytes.cend());

    return serialization;
}

auto Answer::getStatement() const noexcept -> std::string_view { return this->statement; }
