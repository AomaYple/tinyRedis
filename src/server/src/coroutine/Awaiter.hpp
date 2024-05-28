#pragma once

#include "Task.hpp"

class Awaiter {
public:
    [[nodiscard]] constexpr auto await_ready() const noexcept { return false; }

    auto await_suspend(std::coroutine_handle<Task::promise_type> handle) -> void;

    [[nodiscard]] auto await_resume() const -> Outcome;

    auto setSubmission(const Submission &submission) noexcept -> void;

private:
    std::coroutine_handle<Task::promise_type> handle;
    Submission submission{};
};
