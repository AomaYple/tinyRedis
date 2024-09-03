#pragma once

#include "../coroutine/Awaiter.hpp"

class FileDescriptor {
public:
    explicit FileDescriptor(int fileDescriptor) noexcept;

    FileDescriptor(const FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&) noexcept = default;

    auto operator=(const FileDescriptor &) -> FileDescriptor & = delete;

    auto operator=(FileDescriptor &&) noexcept -> FileDescriptor & = delete;

    virtual ~FileDescriptor() = default;

    [[nodiscard]] auto getFileDescriptor() const noexcept -> int;

    [[nodiscard]] auto close() const noexcept -> Awaiter;

private:
    int fileDescriptor;
};
