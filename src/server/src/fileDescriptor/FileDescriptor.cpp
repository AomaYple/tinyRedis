#include "FileDescriptor.hpp"

FileDescriptor::FileDescriptor(const int fileDescriptor) noexcept : fileDescriptor{fileDescriptor} {}

auto FileDescriptor::getFileDescriptor() const noexcept -> int { return this->fileDescriptor; }

auto FileDescriptor::close() const noexcept -> Awaiter {
    Awaiter awaiter;
    awaiter.setSubmission(Submission{this->fileDescriptor, 0, 0, 0, Submission::Close{}});

    return awaiter;
}
