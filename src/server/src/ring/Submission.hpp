#pragma once

#include <span>
#include <sys/socket.h>
#include <variant>

struct Submission {
    struct Write {
        std::span<const std::byte> buffer;
        unsigned long offset;
    };

    struct Accept {
        sockaddr *address;
        socklen_t *addressLength;
        int flags;
    };

    struct Read {
        std::span<std::byte> buffer;
        unsigned long offset;
    };

    struct Receive {
        std::span<std::byte> buffer;
        int flags;
        int ringBufferId;
    };

    struct Send {
        std::span<const std::byte> buffer;
        int flags;
        unsigned int zeroCopyFlags;
    };

    struct Cancel {
        int flags;
    };

    struct Close {};

    int fileDescriptor;
    unsigned int flags;
    unsigned long userData;
    std::variant<Write, Accept, Read, Receive, Send, Cancel, Close> parameter;
};
