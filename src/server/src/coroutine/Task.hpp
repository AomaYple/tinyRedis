#pragma once

#include "../ring/Outcome.hpp"
#include "../ring/Submission.hpp"

#include <coroutine>

class Task {
public:
    class promise_type {
    public:
        [[nodiscard]] auto get_return_object() -> Task;

        [[nodiscard]] constexpr auto initial_suspend() const noexcept { return std::suspend_always{}; }

        [[nodiscard]] constexpr auto final_suspend() const noexcept { return std::suspend_always{}; }

        auto unhandled_exception() const -> void;

        auto setSubmission(const Submission &submission) noexcept -> void;

        [[nodiscard]] auto getSubmission() const noexcept -> const Submission &;

        auto setOutcome(Outcome outcome) noexcept -> void;

        [[nodiscard]] auto getOutcome() const noexcept -> Outcome;

    private:
        Submission submission;
        Outcome outcome;
    };

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept;

    Task(const Task &) = delete;

    Task(Task &&) noexcept;

    auto operator=(const Task &) -> Task & = delete;

    auto operator=(Task &&) noexcept -> Task &;

    ~Task();

    [[nodiscard]] auto getSubmission() const -> const Submission &;

    auto resume(Outcome outcome) const -> void;

private:
    auto destroy() const -> void;

    std::coroutine_handle<promise_type> handle;
};
