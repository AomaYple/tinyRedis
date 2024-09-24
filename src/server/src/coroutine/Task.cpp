#include "Task.hpp"

#include <utility>

auto Task::promise_type::get_return_object() -> Task {
    return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
}

auto Task::promise_type::unhandled_exception() const -> void { throw; }

auto Task::promise_type::setSubmission(const Submission &submission) noexcept -> void { this->submission = submission; }

auto Task::promise_type::getSubmission() const noexcept -> const Submission & { return this->submission; }

auto Task::promise_type::setOutcome(const Outcome outcome) noexcept -> void { this->outcome = outcome; }

auto Task::promise_type::getOutcome() const noexcept -> Outcome { return this->outcome; }

Task::Task(const std::coroutine_handle<promise_type> handle) noexcept : handle{handle} {}

Task::Task(Task &&other) noexcept : handle{std::exchange(other.handle, nullptr)} {}

auto Task::operator=(Task &&other) noexcept -> Task & {
    if (this == &other) return *this;

    this->destroy();

    this->handle = std::exchange(other.handle, nullptr);

    return *this;
}

Task::~Task() { this->destroy(); }

auto Task::getSubmission() const -> const Submission & { return this->handle.promise().getSubmission(); }

auto Task::resume(const Outcome outcome) const -> void {
    this->handle.promise().setOutcome(outcome);
    this->handle.resume();
}

auto Task::destroy() const -> void {
    if (this->handle) this->handle.destroy();
}
