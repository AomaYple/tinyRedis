#pragma once

#include "../coroutine/Awaiter.hpp"

class FileDescriptor {
public:
    explicit FileDescriptor(int fileDescriptor) noexcept;

    FileDescriptor(const FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&) = default;

    auto operator=(const FileDescriptor &) -> FileDescriptor & = delete;

    auto operator=(FileDescriptor &&) -> FileDescriptor & = delete;

    ~FileDescriptor() = default;

    [[nodiscard]] auto getFileDescriptor() const noexcept -> int;

    [[nodiscard]] auto cancel() const noexcept -> Awaiter;

    [[nodiscard]] auto close() const noexcept -> Awaiter;

private:
    int fileDescriptor;
};
