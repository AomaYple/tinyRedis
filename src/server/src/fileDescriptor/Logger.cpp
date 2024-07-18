#include "Logger.hpp"

#include "../../../common/log/Exception.hpp"

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

Logger::Logger(const int fileDescriptor) : FileDescriptor{fileDescriptor} {}

auto Logger::push(Log &&log) -> void { this->logs.push(std::move(log)); }

auto Logger::isWritable() const noexcept -> bool { return !this->logs.empty() && this->data.empty(); }

auto Logger::write() -> Awaiter {
    while (!this->logs.empty()) {
        const std::vector byteLog{this->logs.front().toByte()};
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
