#include "Timer.hpp"

#include "../../../common/log/Exception.hpp"

#include <linux/io_uring.h>
#include <sys/timerfd.h>

auto Timer::create() -> int {
    const int fileDescriptor{createTimerFileDescriptor()};
    setTime(fileDescriptor);

    return fileDescriptor;
}

Timer::Timer(const int fileDescriptor) noexcept : FileDescriptor{fileDescriptor} {}

auto Timer::timing() noexcept -> Awaiter {
    Awaiter awaiter;
    awaiter.setSubmission(Submission{
        this->getFileDescriptor(),
        IOSQE_FIXED_FILE,
        0,
        0,
        Submission::Read{std::as_writable_bytes(std::span{&this->timeout, 1}), 0},
    });

    return awaiter;
}

auto Timer::createTimerFileDescriptor(const std::source_location sourceLocation) -> int {
    const int fileDescriptor{timerfd_create(CLOCK_MONOTONIC, 0)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    return fileDescriptor;
}

auto Timer::setTime(const int fileDescriptor, const std::source_location sourceLocation) -> void {
    constexpr itimerspec time{
        {1, 0},
        {1, 0}
    };
    if (timerfd_settime(fileDescriptor, 0, &time, nullptr) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}
