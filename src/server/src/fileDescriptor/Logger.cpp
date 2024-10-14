#include "Logger.hpp"

#include "../../../common/Exception.hpp"

#include <fcntl.h>
#include <linux/io_uring.h>

auto Logger::create(const std::string_view filename, const std::source_location sourceLocation) -> int {
    const int fileDescriptor{open(filename.data(), O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    return fileDescriptor;
}

Logger::Logger(const int fileDescriptor) noexcept : FileDescriptor{fileDescriptor} {}

auto Logger::push(Log &&log) -> void { this->logs.emplace_back(std::move(log)); }

auto Logger::isWritable() const noexcept -> bool { return !this->logs.empty() && this->buffer.empty(); }

auto Logger::write() -> Awaiter {
    for (const auto &log : this->logs) {
        const std::vector bytes{log.toByte()};
        this->buffer.insert(this->buffer.cend(), bytes.cbegin(), bytes.cend());
    }
    this->logs.clear();

    return Awaiter{
        Submission{this->getFileDescriptor(), IOSQE_FIXED_FILE, 0, 0, Submission::Write{this->buffer, 0}}
    };
}

auto Logger::wrote() noexcept -> void { this->buffer.clear(); }
