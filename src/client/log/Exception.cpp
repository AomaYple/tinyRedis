#include "Exception.hpp"

Exception::Exception(Log &&log) : text{log.toString()}, log{std::move(log)} {}

auto Exception::what() const noexcept -> const char * { return this->text.c_str(); }

auto Exception::getLog() noexcept -> Log & { return this->log; }
