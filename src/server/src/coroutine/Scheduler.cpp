#include "Scheduler.hpp"

#include "../../../common/log/Exception.hpp"
#include "../database/Database.hpp"
#include "../fileDescriptor/Client.hpp"
#include "../ring/Completion.hpp"
#include "../ring/Ring.hpp"

#include <cstring>
#include <ranges>

auto Scheduler::registerSignal(const std::source_location sourceLocation) -> void {
    struct sigaction signalAction {};

    signalAction.sa_handler = [](int) noexcept { switcher.clear(std::memory_order_relaxed); };

    if (sigaction(SIGTERM, &signalAction, nullptr) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }

    if (sigaction(SIGINT, &signalAction, nullptr) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::strerror(errno), sourceLocation}
        };
    }
}

Scheduler::Scheduler() {
    this->ring->registerSelfFileDescriptor();

    {
        const std::lock_guard lockGuard{lock};

        this->ring->registerCpu(std::distance(ringFileDescriptors.begin(),
                                              std::ranges::find(ringFileDescriptors, this->ring->getFileDescriptor())));
    }

    this->ring->registerSparseFileDescriptor(Ring::getFileDescriptorLimit());

    const std::array fileDescriptors{Logger::create(), Server::create(), Timer::create()};
    this->ring->allocateFileDescriptorRange(fileDescriptors.size(),
                                            Ring::getFileDescriptorLimit() - fileDescriptors.size());
    this->ring->updateFileDescriptors(0, fileDescriptors);
}

Scheduler::~Scheduler() {
    this->closeAll();

    const std::lock_guard lockGuard{lock};

    auto result{std::ranges::find(ringFileDescriptors, this->ring->getFileDescriptor())};
    *result = -1;

    if (sharedRingFileDescriptor == this->ring->getFileDescriptor()) {
        sharedRingFileDescriptor = -1;

        result = std::ranges::find_if(ringFileDescriptors,
                                      [](const int fileDescriptor) noexcept { return fileDescriptor != -1; });
        if (result != ringFileDescriptors.cend()) sharedRingFileDescriptor = *result;
    }

    instance = false;
}

auto Scheduler::run() -> void {
    this->submit(std::make_shared<Task>(this->accept()));
    this->submit(std::make_shared<Task>(this->timing()));

    while (switcher.test(std::memory_order::relaxed)) {
        if (this->logger->writable()) this->submit(std::make_shared<Task>(this->write()));

        this->ring->wait(1);
        this->frame();
    }
}

auto Scheduler::initializeRing(const std::source_location sourceLocation) -> std::shared_ptr<Ring> {
    if (instance) {
        throw Exception{
            Log{Log::Level::fatal, "one thread can only have one Scheduler", sourceLocation}
        };
    }
    instance = true;

    io_uring_params params{};
    params.flags = IORING_SETUP_CLAMP | IORING_SETUP_SUBMIT_ALL | IORING_SETUP_COOP_TASKRUN |
                   IORING_SETUP_TASKRUN_FLAG | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN;

    const std::lock_guard lockGuard{lock};

    if (sharedRingFileDescriptor != -1) {
        params.wq_fd = sharedRingFileDescriptor;
        params.flags |= IORING_SETUP_ATTACH_WQ;
    }

    auto ring{std::make_shared<Ring>(2048 / ringFileDescriptors.size(), params)};

    if (sharedRingFileDescriptor == -1) sharedRingFileDescriptor = ring->getFileDescriptor();

    if (const auto result{std::ranges::find(ringFileDescriptors, -1)}; result != ringFileDescriptors.cend())
        *result = ring->getFileDescriptor();
    else {
        throw Exception{
            Log{Log::Level::fatal, "too many Scheduler", sourceLocation}
        };
    }

    return ring;
}

auto Scheduler::frame() -> void {
    const int completionCount{this->ring->poll([this](const Completion &completion) {
        if (completion.outcome.result != 0 || !(completion.outcome.flags & IORING_CQE_F_NOTIF)) {
            this->currentUserData = completion.userData;
            const std::shared_ptr task{this->tasks.at(this->currentUserData)};
            task->resume(completion.outcome);
        }
    })};
    this->ring->advance(this->ringBuffer.getHandle(), completionCount, this->ringBuffer.getAddedBufferCount());
}

auto Scheduler::submit(std::shared_ptr<Task> &&task) -> void {
    task->resume(Outcome{});
    this->ring->submit(task->getSubmission());
    this->tasks.emplace(task->getSubmission().userData, std::move(task));
}

auto Scheduler::eraseCurrentTask() -> void { this->tasks.erase(this->currentUserData); }

auto Scheduler::write(const std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await this->logger->write()}; result < 0) {
        throw Exception{
            Log{Log::Level::error, std::strerror(std::abs(result)), sourceLocation}
        };
    }
    this->logger->wrote();

    this->eraseCurrentTask();
}

auto Scheduler::accept(const std::source_location sourceLocation) -> Task {
    while (true) {
        if (const auto [result, flags]{co_await this->server.accept()}; result >= 0 && flags & IORING_CQE_F_MORE) {
            this->clients.emplace(result, Client{result});

            const Client &client{this->clients.at(result)};

            this->submit(std::make_shared<Task>(this->receive(client)));
        } else {
            this->eraseCurrentTask();

            throw Exception{
                Log{Log::Level::error, std::strerror(std::abs(result)), sourceLocation}
            };
        }
    }
}

auto Scheduler::timing(const std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await this->timer.timing()}; result == sizeof(unsigned long))
        this->submit(std::make_shared<Task>(this->timing()));
    else {
        throw Exception{
            Log{Log::Level::error, std::strerror(std::abs(result)), sourceLocation}
        };
    }

    this->eraseCurrentTask();
}

auto Scheduler::receive(const Client &client, const std::source_location sourceLocation) -> Task {
    std::vector<std::byte> buffer;

    while (true) {
        if (const auto [result, flags]{co_await client.receive(this->ringBuffer.getId())};
            result > 0 && flags & IORING_CQE_F_MORE) {
            const std::span receivedData{this->ringBuffer.readFromBuffer(flags >> IORING_CQE_BUFFER_SHIFT, result)};
            buffer.insert(buffer.cend(), receivedData.cbegin(), receivedData.cend());

            if (!(flags & IORING_CQE_F_SOCK_NONEMPTY)) {
                std::vector response{Database::query(buffer)};
                buffer.clear();

                this->submit(std::make_shared<Task>(this->send(client, std::move(response))));
            }
        } else {
            this->logger->push(Log{
                Log::Level::warn, result == 0 ? "connection closed" : std::strerror(std::abs(result)), sourceLocation});

            this->submit(std::make_shared<Task>(this->close(client.getFileDescriptor())));

            break;
        }
    }

    this->eraseCurrentTask();
}

auto Scheduler::send(const Client &client, std::vector<std::byte> &&data, const std::source_location sourceLocation)
    -> Task {
    const std::vector response{std::move(data)};
    if (const auto [result, flags]{co_await client.send(response)}; result <= 0) {
        this->logger->push(
            Log{Log::Level::warn, result == 0 ? "connection closed" : std::strerror(std::abs(result)), sourceLocation});

        this->submit(std::make_shared<Task>(this->close(client.getFileDescriptor())));
    }

    this->eraseCurrentTask();
}

auto Scheduler::cancel(const Client &client, const std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await client.cancel()}; result < 0)
        this->logger->push(Log{Log::Level::warn, std::strerror(std::abs(result)), sourceLocation});

    this->eraseCurrentTask();
}

auto Scheduler::close(const int fileDescriptor, const std::source_location sourceLocation) -> Task {
    Outcome outcome;
    if (fileDescriptor == this->logger->getFileDescriptor()) outcome = co_await this->logger->close();
    else if (fileDescriptor == this->server.getFileDescriptor()) outcome = co_await this->server.close();
    else if (fileDescriptor == this->timer.getFileDescriptor()) outcome = co_await this->timer.close();
    else [[likely]] {
        outcome = co_await this->clients.at(fileDescriptor).close();
        this->clients.erase(fileDescriptor);
    }

    if (outcome.result < 0)
        this->logger->push(Log{Log::Level::warn, std::strerror(std::abs(outcome.result)), sourceLocation});

    this->eraseCurrentTask();
}

auto Scheduler::closeAll() -> void {
    for (const auto &client : this->clients | std::views::values)
        this->submit(std::make_shared<Task>(this->close(client.getFileDescriptor())));
    this->submit(std::make_shared<Task>(this->close(this->timer.getFileDescriptor())));
    this->submit(std::make_shared<Task>(this->close(this->server.getFileDescriptor())));
    this->submit(std::make_shared<Task>(this->close(this->logger->getFileDescriptor())));

    this->ring->wait(3 + this->clients.size());
    this->frame();
}

constinit thread_local bool Scheduler::instance{};
constinit std::mutex Scheduler::lock;
constinit int Scheduler::sharedRingFileDescriptor{-1};
std::vector<int> Scheduler::ringFileDescriptors{std::vector(std::thread::hardware_concurrency(), -1)};
constinit std::atomic_flag Scheduler::switcher{true};
