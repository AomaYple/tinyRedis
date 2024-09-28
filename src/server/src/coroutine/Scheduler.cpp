#include "Scheduler.hpp"

#include "../../../common/log/Exception.hpp"
#include "../fileDescriptor/Client.hpp"
#include "../ring/Completion.hpp"
#include "../ring/Ring.hpp"

#include <ranges>
#include <sys/resource.h>

auto Scheduler::getFileDescriptorLimit(const std::source_location sourceLocation) -> unsigned long {
    rlimit limit{};
    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    return limit.rlim_cur;
}

auto Scheduler::registerSignal(const std::source_location sourceLocation) -> void {
    struct sigaction signalAction {};

    signalAction.sa_handler = [](int) noexcept { switcher.clear(std::memory_order_relaxed); };

    if (sigaction(SIGTERM, &signalAction, nullptr) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }

    if (sigaction(SIGINT, &signalAction, nullptr) == -1) {
        throw Exception{
            Log{Log::Level::fatal, std::error_code{errno, std::generic_category()}.message(), sourceLocation}
        };
    }
}

Scheduler::Scheduler(const int sharedFileDescriptor, const unsigned int cpuCode, const bool main) :
    ring{[sharedFileDescriptor] {
        io_uring_params params{};
        params.flags = IORING_SETUP_CLAMP | IORING_SETUP_SUBMIT_ALL | IORING_SETUP_COOP_TASKRUN |
                       IORING_SETUP_TASKRUN_FLAG | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN;

        if (sharedFileDescriptor != -1) {
            params.wq_fd = sharedFileDescriptor;
            params.flags |= IORING_SETUP_ATTACH_WQ;
        }

        auto ring{std::make_shared<Ring>(2048 / std::thread::hardware_concurrency(), params)};

        return ring;
    }()},
    main{main} {
    const unsigned long fileDescriptorLimit{getFileDescriptorLimit()};

    this->ring->registerSelfFileDescriptor();
    this->ring->registerCpu(cpuCode);
    this->ring->registerSparseFileDescriptor(fileDescriptorLimit);

    std::vector fileDescriptors{Logger::create("log.log"), Server::create("127.0.0.1", 9090), Timer::create()};
    if (this->main) fileDescriptors.emplace_back(DatabaseManager::create());

    this->ring->allocateFileDescriptorRange(fileDescriptors.size(), fileDescriptorLimit - fileDescriptors.size());
    this->ring->updateFileDescriptors(0, fileDescriptors);

    for (unsigned int i{}; i != entries; ++i) this->ringBuffer.addBuffer(this->bufferGroup.getBuffer(i), i);
}

Scheduler::~Scheduler() {
    for (const auto &client : this->clients | std::views::values)
        this->submit(std::make_shared<Task>(this->close(client.getFileDescriptor())));
    this->submit(std::make_shared<Task>(this->close(this->timer.getFileDescriptor())));
    this->submit(std::make_shared<Task>(this->close(this->server.getFileDescriptor())));
    this->submit(std::make_shared<Task>(this->close(this->logger->getFileDescriptor())));
    if (this->main) this->submit(std::make_shared<Task>(this->close(databaseManager.getFileDescriptor())));

    this->ring->wait(this->main ? 4 : 3 + this->clients.size());
    this->frame();
}

auto Scheduler::getRingFileDescriptor() const noexcept -> int { return this->ring->getFileDescriptor(); }

auto Scheduler::run() -> void {
    this->submit(std::make_shared<Task>(this->accept()));
    this->submit(std::make_shared<Task>(this->timing()));

    while (switcher.test(std::memory_order::relaxed)) {
        if (this->logger->isWritable()) this->submit(std::make_shared<Task>(this->writeLog()));

        this->ring->wait(1);
        this->frame();
    }
}

auto Scheduler::frame() -> void {
    const int completionCount{this->ring->poll([this](const Completion &completion) {
        if (completion.outcome.result != 0 || (completion.outcome.flags & IORING_CQE_F_NOTIF) == 0) {
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

auto Scheduler::writeLog(const std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await this->logger->write()}; result < 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
    this->logger->wrote();

    this->eraseCurrentTask();
}

auto Scheduler::accept(const std::source_location sourceLocation) -> Task {
    while (true) {
        if (const auto [result, flags]{co_await this->server.accept()};
            result >= 0 && (flags & IORING_CQE_F_MORE) != 0) {
            this->clients.emplace(result, Client{result});

            const Client &client{this->clients.at(result)};

            this->submit(std::make_shared<Task>(this->receive(client)));
        } else {
            this->eraseCurrentTask();

            throw Exception{
                Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                    sourceLocation}
            };
        }
    }
}

auto Scheduler::timing(const std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await this->timer.timing()}; result == sizeof(unsigned long))
        this->submit(std::make_shared<Task>(this->timing()));
    else {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }

    if (this->main && databaseManager.isWritable())
        this->submit(std::make_shared<Task>(databaseManager.isCanTruncate() ? this->truncate() : this->writeData()));

    this->eraseCurrentTask();
}

auto Scheduler::receive(const Client &client, const std::source_location sourceLocation) -> Task {
    std::vector<std::byte> receiveBuffer;

    while (true) {
        if (const auto [result, flags]{co_await client.receive(this->ringBuffer.getId())};
            result > 0 && (flags & IORING_CQE_F_MORE) != 0) {
            const auto index{static_cast<unsigned short>(flags >> IORING_CQE_BUFFER_SHIFT)};
            const std::span buffer{this->bufferGroup.getBuffer(index)}, receivedData{buffer.subspan(0, result)};
            this->ringBuffer.addBuffer(buffer, index);

            receiveBuffer.insert(receiveBuffer.cend(), receivedData.cbegin(), receivedData.cend());

            if ((flags & IORING_CQE_F_SOCK_NONEMPTY) == 0) {
                std::vector response{databaseManager.query(receiveBuffer)};
                receiveBuffer.clear();

                this->submit(std::make_shared<Task>(this->send(client, std::move(response))));
            }
        } else {
            this->logger->push(Log{
                Log::Level::warn,
                result == 0 ? "connection closed" :
                              std::error_code{std::abs(result), std::generic_category()}
                              .message(),
                sourceLocation
            });

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
        this->logger->push(Log{
            Log::Level::warn,
            result == 0 ? "connection closed" : std::error_code{std::abs(result), std::generic_category()}
                  .message(),
            sourceLocation
        });

        this->submit(std::make_shared<Task>(this->close(client.getFileDescriptor())));
    }

    this->eraseCurrentTask();
}

auto Scheduler::truncate(std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await databaseManager.truncate()}; result != 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
    this->submit(std::make_shared<Task>(this->writeData()));

    this->eraseCurrentTask();
}

auto Scheduler::writeData(std::source_location sourceLocation) -> Task {
    if (const auto [result, flags]{co_await databaseManager.write()}; result < 0) {
        throw Exception{
            Log{Log::Level::error, std::error_code{std::abs(result), std::generic_category()}.message(),
                sourceLocation}
        };
    }
    databaseManager.wrote();

    this->eraseCurrentTask();
}

auto Scheduler::close(const int fileDescriptor, const std::source_location sourceLocation) -> Task {
    Outcome outcome;
    if (fileDescriptor == this->logger->getFileDescriptor()) outcome = co_await this->logger->close();
    else if (fileDescriptor == this->server.getFileDescriptor()) outcome = co_await this->server.close();
    else if (fileDescriptor == this->timer.getFileDescriptor()) outcome = co_await this->timer.close();
    else if (this->main && fileDescriptor == databaseManager.getFileDescriptor())
        outcome = co_await databaseManager.close();
    else [[likely]] {
        outcome = co_await this->clients.at(fileDescriptor).close();
        this->clients.erase(fileDescriptor);
    }

    if (outcome.result < 0) {
        this->logger->push(Log{
            Log::Level::warn, std::error_code{std::abs(outcome.result), std::generic_category()}
             .message(),
            sourceLocation
        });
    }

    this->eraseCurrentTask();
}

constinit std::atomic_flag Scheduler::switcher{true};
const unsigned int Scheduler::entries{
    std::bit_ceil(static_cast<unsigned int>(getFileDescriptorLimit()) / std::thread::hardware_concurrency()) * 2};
DatabaseManager Scheduler::databaseManager{3};
