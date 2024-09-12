#pragma once

#include "../coroutine/Awaiter.hpp"

class FileDescriptor {
public:
    explicit FileDescriptor(int fileDescriptor) noexcept;

    FileDescriptor(const FileDescriptor &) = delete;

    constexpr FileDescriptor(FileDescriptor &&) noexcept = default;

    auto operator=(const FileDescriptor &) -> FileDescriptor & = delete;

    constexpr auto operator=(FileDescriptor &&) noexcept -> FileDescriptor & = delete;

    constexpr virtual ~FileDescriptor() = default;

    [[nodiscard]] auto getFileDescriptor() const noexcept -> int;

    [[nodiscard]] auto close() const noexcept -> Awaiter;

private:
    int fileDescriptor;
};
