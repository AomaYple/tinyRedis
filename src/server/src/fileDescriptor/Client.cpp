#include "Client.hpp"

#include <linux/io_uring.h>

Client::Client(const int fileDescriptor) noexcept : FileDescriptor{fileDescriptor} {}

auto Client::receive(const int ringBufferId) const noexcept -> Awaiter {
    return Awaiter{
        Submission{
                   this->getFileDescriptor(),
                   IOSQE_FIXED_FILE | IOSQE_BUFFER_SELECT,
                   IORING_RECVSEND_POLL_FIRST, 0,
                   Submission::Receive{std::span<std::byte>{}, 0, ringBufferId},
                   }
    };
}

auto Client::send(const std::span<const std::byte> data) const noexcept -> Awaiter {
    return Awaiter{
        Submission{
                   this->getFileDescriptor(),
                   IOSQE_FIXED_FILE, 0,
                   0, Submission::Send{data, 0, 0},
                   }
    };
}

auto Client::getContext() noexcept -> Context & { return this->context; }
