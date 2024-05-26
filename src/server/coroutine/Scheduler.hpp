#pragma once

#include "../fileDescriptor/Logger.hpp"
#include "../fileDescriptor/Server.hpp"
#include "../ring/RingBuffer.hpp"

#include <unordered_map>

class Client;

class Scheduler {
public:
    static auto registerSignal(std::source_location sourceLocation = std::source_location::current()) -> void;

    Scheduler();

    Scheduler(const Scheduler &) = delete;

    auto operator=(const Scheduler &) -> Scheduler & = delete;

    Scheduler(Scheduler &&) = delete;

    auto operator=(Scheduler &&) -> Scheduler & = delete;

    ~Scheduler();

    auto run() -> void;

private:
    [[nodiscard]] static auto initializeRing(std::source_location sourceLocation = std::source_location::current())
        -> std::shared_ptr<Ring>;

    auto frame() -> void;

    auto submit(std::shared_ptr<Task> &&task) -> void;

    auto eraseCurrentTask() -> void;

    [[nodiscard]] auto write(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto accept(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto receive(const Client &client,
                               std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto send(const Client &client, std::vector<std::byte> &&data,
                            std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto close(int fileDescriptor, std::source_location sourceLocation = std::source_location::current())
        -> Task;

    auto closeAll() -> void;

    static constinit thread_local bool instance;
    static constinit std::mutex lock;
    static constinit int sharedRingFileDescriptor;
    static std::vector<int> ringFileDescriptors;
    static constinit std::atomic_flag switcher;

    const std::shared_ptr<Ring> ring{initializeRing()};
    const std::shared_ptr<Logger> logger{std::make_shared<Logger>(0)};
    const Server server{1};
    std::unordered_map<int, Client> clients;
    RingBuffer ringBuffer{this->ring, static_cast<unsigned int>(std::bit_ceil(2048 / ringFileDescriptors.size())), 1024,
                          0};
    std::unordered_map<unsigned long, std::shared_ptr<Task>> tasks;
    unsigned long currentUserData{};
};
