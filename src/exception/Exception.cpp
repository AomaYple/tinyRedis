#include "Exception.hpp"

Exception::Exception(std::string &&text) : text{std::move(text)} {}

auto Exception::what() const noexcept -> const char * { return this->text.c_str(); }
