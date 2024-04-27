#pragma once

#include <string>

class Exception : public std::exception {
public:
    explicit Exception(std::string &&text);

    [[nodiscard]] auto what() const noexcept -> const char * override;

private:
    std::string text;
};
