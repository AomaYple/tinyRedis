#include "Awaiter.hpp"

auto Awaiter::await_suspend(const std::coroutine_handle<Task::promise_type> handle) -> void {
    this->handle = handle;
    this->submission.userData = std::hash<std::coroutine_handle<Task::promise_type>>{}(this->handle);
    this->handle.promise().setSubmission(this->submission);
}

auto Awaiter::await_resume() const -> Outcome { return this->handle.promise().getOutcome(); }

auto Awaiter::setSubmission(const Submission &submission) noexcept -> void { this->submission = submission; }
