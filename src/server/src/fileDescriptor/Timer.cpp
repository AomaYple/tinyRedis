#include "Timer.hpp"

#include "../log/Exception.hpp"

#include <cstring>
#include <linux/io_uring.h>
#include <sys/timerfd.h>

auto Timer::create() -> int {
    const int fileDescriptor{createTimerFileDescriptor()};
    setTime(fileDescriptor);

    return fileDescriptor;
}

Timer::Timer(int fileDescriptor) noexcept : FileDescriptor{fileDescriptor} {}

auto Timer::timing() noexcept -> Awaiter {
    Awaiter awaiter;
    awaiter.setSubmission(Submission{
        this->getFileDescriptor(),
        IOSQE_FIXED_FILE,
        0,
        Submission::Read{std::as_writable_bytes(std::span{&this->timeout, 1}), 0},
    });

    return awaiter;
}

auto Timer::add(int fileDescriptor, unsigned long seconds) -> void {
    const unsigned long level{seconds / (this->wheel.size() - 1)};
    const unsigned char point{static_cast<unsigned char>(seconds % (this->wheel.size() - 1))};

    this->wheel[point].emplace(fileDescriptor, level);
    this->location.emplace(fileDescriptor, point);
}

auto Timer::update(int fileDescriptor, unsigned long seconds) -> void {
    const unsigned long level{seconds / (this->wheel.size() - 1)};
    const unsigned char point{static_cast<unsigned char>(seconds % (this->wheel.size() - 1))};

    this->wheel[this->location.at(fileDescriptor)].erase(fileDescriptor);
    this->wheel[point].emplace(fileDescriptor, level);

    this->location.at(fileDescriptor) = point;
}

auto Timer::remove(int fileDescriptor) -> void {
    const auto result{this->location.find(fileDescriptor)};
    if (result != this->location.cend()) {
        this->wheel[result->second].erase(fileDescriptor);
        this->location.erase(result);
    }
}

auto Timer::clearTimeout() -> std::vector<int> {
    std::vector<int> result;

    while (this->timeout > 0) {
        std::unordered_map<int, unsigned long> &wheelPoint{this->wheel[this->now]};
        for (auto element{wheelPoint.begin()}; element != wheelPoint.end();) {
            if (element->second == 0) {
                result.emplace_back(element->first);
                this->location.erase(element->first);

                element = wheelPoint.erase(element);
            } else {
                --element->second;
                ++element;
            }
        }

        ++this->now;
        this->now %= this->wheel.size();
        --this->timeout;
    }

    return result;
}

auto Timer::createTimerFileDescriptor(std::source_location sourceLocation) -> int {
    const int fileDescriptor{timerfd_create(CLOCK_MONOTONIC, 0)};
    if (fileDescriptor == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }

    return fileDescriptor;
}

auto Timer::setTime(int fileDescriptor, std::source_location sourceLocation) -> void {
    constexpr itimerspec time{
        {1, 0},
        {1, 0}
    };
    if (timerfd_settime(fileDescriptor, 0, &time, nullptr) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }
}
