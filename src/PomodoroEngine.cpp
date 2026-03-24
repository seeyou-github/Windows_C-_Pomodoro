#include "PomodoroEngine.h"

namespace Pomodoro {

PomodoroEngine::PomodoroEngine() {
    Reset();
}

void PomodoroEngine::ApplySettings(const Settings& settings) {
    settings_ = settings;
}

void PomodoroEngine::Reset() {
    running_ = false;
    current_cycle_ = 0;
    mode_ = SessionMode::Work;
    remaining_seconds_ = GetModeDurationSeconds(SessionMode::Work);
}

void PomodoroEngine::Start() {
    running_ = true;
}

void PomodoroEngine::Pause() {
    running_ = false;
}

void PomodoroEngine::StartRestPhase() {
    // The Python version switches to long break whenever the completed work
    // cycle count is divisible by the configured cycle size.
    mode_ = (current_cycle_ > 0 && current_cycle_ % settings_.max_cycles == 0)
        ? SessionMode::LongBreak
        : SessionMode::ShortBreak;
    remaining_seconds_ = GetModeDurationSeconds(mode_);
    running_ = true;
}

void PomodoroEngine::StartWorkPhase() {
    mode_ = SessionMode::Work;
    remaining_seconds_ = GetModeDurationSeconds(SessionMode::Work);
    running_ = true;
}

CompletionAction PomodoroEngine::Tick() {
    if (!running_) {
        return CompletionAction::None;
    }

    // The UI drives this once per second. When the counter reaches zero,
    // the caller decides how to notify the user and when to continue.
    if (remaining_seconds_ > 0) {
        --remaining_seconds_;
    }

    if (remaining_seconds_ > 0) {
        return CompletionAction::None;
    }

    running_ = false;

    if (mode_ == SessionMode::Work) {
        ++current_cycle_;
        return CompletionAction::ShowStartRestAlert;
    }

    if (mode_ == SessionMode::LongBreak) {
        return CompletionAction::ShowCycleFinishedAlert;
    }

    return CompletionAction::ShowStartWorkAlert;
}

bool PomodoroEngine::IsRunning() const {
    return running_;
}

int PomodoroEngine::GetRemainingSeconds() const {
    return remaining_seconds_;
}

int PomodoroEngine::GetCurrentCycle() const {
    return current_cycle_;
}

SessionMode PomodoroEngine::GetMode() const {
    return mode_;
}

const Settings& PomodoroEngine::GetSettings() const {
    return settings_;
}

int PomodoroEngine::GetModeDurationSeconds(SessionMode mode) const {
    switch (mode) {
    case SessionMode::Work:
        return settings_.work_minutes * 60;
    case SessionMode::ShortBreak:
        return settings_.short_break_minutes * 60;
    case SessionMode::LongBreak:
        return settings_.long_break_minutes * 60;
    }

    return settings_.work_minutes * 60;
}

}  // namespace Pomodoro
