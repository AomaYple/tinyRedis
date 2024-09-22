#pragma once

#include "Log.hpp"

class Exception final : public std::exception {
public:
    explicit Exception(Log &&log);

    [[nodiscard]] auto what() const noexcept -> const char * override;

    [[nodiscard]] auto getLog() noexcept -> Log &;

private:
    std::string text;
    Log log;
};
