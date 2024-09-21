#include "Ring.hpp"

#include "../../../common/log/Exception.hpp"
#include "Completion.hpp"
#include "Submission.hpp"

Ring::Ring(const unsigned int entries, io_uring_params &params) :
    handle{[entries, &params](const std::source_location sourceLocation = std::source_location::current()) {
        io_uring handle{};
        if (const int result{io_uring_queue_init_params(entries, &handle, &params)}; result != 0) {
            throw Exception{
                Log{Log::Level::fatal, std::error_code{std::abs(result), std::generic_category()}.message(),
                    sourceLocation}
            };
        }

        return handle;
    }()} {}

Ring::Ring(Ring &&other) noexcept : handle{other.handle} { other.handle.ring_fd = -1; }

auto Ring::operator=(Ring &&other) noexcept -> Ring & {
    if (this == &other) return *this;

    this->destroy();

    this->handle = other.handle;
    other.handle.ring_fd = -1;

    return *this;
}

Ring::~Ring() { this->destroy(); }

auto Ring::getFileDescriptor() const noexcept -> int { return this->handle.ring_fd; }

auto Ring::registerSelfFileDescriptor(const std::source_location sourceLocation) -> void {
    if (const int result{io_uring_register_ring_fd(&this->handle)}; result != 1) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::registerCpu(const unsigned int cpuCode, const std::source_location sourceLocation) -> void {
    constexpr cpu_set_t cpuSet{};
    CPU_SET(cpuCode, &cpuSet);

    if (const int result{io_uring_register_iowq_aff(&this->handle, sizeof(cpuSet), &cpuSet)}; result != 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::registerSparseFileDescriptor(const unsigned int count, const std::source_location sourceLocation) -> void {
    if (const int result{io_uring_register_files_sparse(&this->handle, count)}; result != 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::allocateFileDescriptorRange(const unsigned int offset, const unsigned int length,
                                       const std::source_location sourceLocation) -> void {
    if (const int result{io_uring_register_file_alloc_range(&this->handle, offset, length)}; result != 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::updateFileDescriptors(const unsigned int offset, const std::span<const int> fileDescriptors,
                                 const std::source_location sourceLocation) -> void {
    if (const int result{
            io_uring_register_files_update(&this->handle, offset, fileDescriptors.data(), fileDescriptors.size())};
        result < 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::setupRingBuffer(const unsigned int entries, const int id, const std::source_location sourceLocation)
    -> io_uring_buf_ring * {
    int result;
    io_uring_buf_ring *const ringBufferHandle{io_uring_setup_buf_ring(&this->handle, entries, id, 0, &result)};
    if (ringBufferHandle == nullptr) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }

    return ringBufferHandle;
}

auto Ring::freeRingBuffer(io_uring_buf_ring *const ringBufferHandle, const unsigned int entries, const int id,
                          const std::source_location sourceLocation) -> void {
    if (const int result{io_uring_free_buf_ring(&this->handle, ringBufferHandle, entries, id)}; result < 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::submit(const Submission &submission) -> void {
    io_uring_sqe *const sqe{this->getSqe()};

    switch (static_cast<Submission::Type>(submission.parameter.index())) {
        case Submission::Type::write:
            {
                const auto [buffer, offset]{std::get<Submission::Write>(submission.parameter)};
                io_uring_prep_write(sqe, submission.fileDescriptor, buffer.data(), buffer.size(), offset);

                break;
            }
        case Submission::Type::accept:
            {
                const auto [address, addressLength, flags]{std::get<Submission::Accept>(submission.parameter)};
                io_uring_prep_multishot_accept_direct(sqe, submission.fileDescriptor, address, addressLength, flags);

                break;
            }
        case Submission::Type::read:
            {
                const auto [buffer, offset]{std::get<Submission::Read>(submission.parameter)};
                io_uring_prep_read(sqe, submission.fileDescriptor, buffer.data(), buffer.size(), offset);

                break;
            }
        case Submission::Type::receive:
            {
                const auto [buffer, flags, ringBufferId]{std::get<Submission::Receive>(submission.parameter)};
                io_uring_prep_recv_multishot(sqe, submission.fileDescriptor, buffer.data(), buffer.size(), flags);
                sqe->buf_group = ringBufferId;

                break;
            }
        [[likely]] case Submission::Type::send:
            {
                const auto [buffer, flags, zeroCopyFlags]{std::get<Submission::Send>(submission.parameter)};
                io_uring_prep_send_zc(sqe, submission.fileDescriptor, buffer.data(), buffer.size(), flags,
                                      zeroCopyFlags);

                break;
            }
        case Submission::Type::truncate:
            io_uring_prep_ftruncate(sqe, submission.fileDescriptor,
                                    std::get<Submission::Truncate>(submission.parameter).length);

            break;
        case Submission::Type::close:
            io_uring_prep_close_direct(sqe, submission.fileDescriptor);

            break;
    }

    io_uring_sqe_set_flags(sqe, submission.flags);
    sqe->ioprio |= submission.ioPriority;
    io_uring_sqe_set_data64(sqe, submission.userData);
}

auto Ring::wait(const unsigned int count, const std::source_location sourceLocation) -> void {
    if (const int result{io_uring_submit_and_wait(&this->handle, count)}; result < 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
}

auto Ring::poll(std::move_only_function<auto(const Completion &completion)->void> &&action) const -> int {
    int count{};
    unsigned int head;

    const io_uring_cqe *cqe;
    io_uring_for_each_cqe(&this->handle, head, cqe) {
        action(Completion{
            Outcome{cqe->res, cqe->flags},
            io_uring_cqe_get_data64(cqe)
        });
        ++count;
    }

    return count;
}

auto Ring::advance(io_uring_buf_ring *const ringBuffer, const int completionCount, const int ringBufferCount) noexcept
    -> void {
    __io_uring_buf_ring_cq_advance(&this->handle, ringBuffer, completionCount, ringBufferCount);
}

auto Ring::destroy() noexcept -> void {
    if (this->handle.ring_fd != -1) io_uring_queue_exit(&this->handle);
}

auto Ring::getSqe(const std::source_location sourceLocation) -> io_uring_sqe * {
    io_uring_sqe *const sqe{io_uring_get_sqe(&this->handle)};
    if (sqe == nullptr) {
        throw Exception{
            Log{Log::Level::error, "no sqe available", sourceLocation}
        };
    }

    return sqe;
}
