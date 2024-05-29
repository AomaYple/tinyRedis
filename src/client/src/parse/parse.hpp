#pragma once

#include <span>
#include <string>

namespace parse {
    auto parse(std::span<const std::byte> data) -> std::pair<unsigned char, std::string>;
}    // namespace parse
