#include "Logger.hpp"

#include "../log/Exception.hpp"

#include <cstring>
#include <fcntl.h>
#include <linux/io_uring.h>

auto Logger::create(std::source_location sourceLocation) -> int {
    const int result{openat(AT_FDCWD, "log.log", O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR)};
    if (result == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }

    return result;
}

Logger::Logger(int fileDescriptor) : FileDescriptor{fileDescriptor} {}

auto Logger::push(Log &&log) -> void { this->logs.push(std::move(log)); }

auto Logger::writable() const noexcept -> bool { return !this->logs.empty() && this->data.empty(); }

auto Logger::write() -> Awaiter {
    while (!this->logs.empty()) {
        const std::vector<std::byte> byteLog{this->logs.front().toByte()};
        this->data.insert(this->data.cend(), byteLog.cbegin(), byteLog.cend());
        this->logs.pop();
    }

    Awaiter awaiter;
    awaiter.setSubmission(Submission{
        this->getFileDescriptor(), IOSQE_FIXED_FILE, 0, Submission::Write{this->data, 0}
    });

    return awaiter;
}

auto Logger::wrote() noexcept -> void { this->data.clear(); }
