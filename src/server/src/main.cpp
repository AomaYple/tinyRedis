#include "coroutine/Scheduler.hpp"

auto main() -> int {
    Scheduler::registerSignal();

    std::atomic_uint cpuCode;
    Scheduler scheduler{-1, cpuCode, true};

    std::vector<std::jthread> workers{std::jthread::hardware_concurrency() - 1};
    for (const int sharedFileDescriptor{scheduler.getRingFileDescriptor()}; auto &worker : workers) {
        worker = std::jthread{[sharedFileDescriptor, &cpuCode] {
            Scheduler otherScheduler{sharedFileDescriptor, cpuCode++, false};
            otherScheduler.run();
        }};
    }

    scheduler.run();

    return 0;
}
