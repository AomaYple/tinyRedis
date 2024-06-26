#pragma once

#include "../fileDescriptor/DatabaseManager.hpp"
#include "../fileDescriptor/Logger.hpp"
#include "../fileDescriptor/Server.hpp"
#include "../fileDescriptor/Timer.hpp"
#include "../ring/RingBuffer.hpp"

class Client;

class Scheduler {
public:
    static auto registerSignal(std::source_location sourceLocation = std::source_location::current()) -> void;

    Scheduler(int sharedFileDescriptor, unsigned int cpuCode, bool main);

    Scheduler(const Scheduler &) = delete;

    Scheduler(Scheduler &&) = delete;

    auto operator=(const Scheduler &) -> Scheduler & = delete;

    auto operator=(Scheduler &&) -> Scheduler & = delete;

    ~Scheduler();

    [[nodiscard]] auto getRingFileDescriptor() const noexcept -> int;

    auto run() -> void;

private:
    auto frame() -> void;

    auto submit(std::shared_ptr<Task> &&task) -> void;

    auto eraseCurrentTask() -> void;

    [[nodiscard]] auto writeLog(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto accept(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto timing(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto receive(const Client &client,
                               std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto send(const Client &client, std::vector<std::byte> &&data,
                            std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto truncate(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto writeData(std::source_location sourceLocation = std::source_location::current()) -> Task;

    [[nodiscard]] auto close(int fileDescriptor, std::source_location sourceLocation = std::source_location::current())
        -> Task;

    static constinit std::atomic_flag switcher;
    static DatabaseManager databaseManager;

    const std::shared_ptr<Ring> ring;
    const std::shared_ptr<Logger> logger{std::make_shared<Logger>(0)};
    const Server server{1};
    Timer timer{2};
    std::unordered_map<int, Client> clients;
    RingBuffer ringBuffer{this->ring, std::bit_ceil(2048 / std::thread::hardware_concurrency()), 1024, 0};
    std::unordered_map<unsigned long, std::shared_ptr<Task>> tasks;
    unsigned long currentUserData{};
    bool main;
};
