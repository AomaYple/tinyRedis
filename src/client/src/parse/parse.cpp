#include "parse.hpp"

auto parse::parse(std::span<const std::byte> data) -> std::pair<unsigned char, std::string> {
    const unsigned char id{static_cast<unsigned char>(data.front())};

    data = data.subspan(2);
    std::string response{reinterpret_cast<const char *>(data.data()), data.size()};

    return std::pair<unsigned char, std::string>{id, std::move(response)};
}
