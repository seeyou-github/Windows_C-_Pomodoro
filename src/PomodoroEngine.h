#pragma once

namespace Pomodoro {

enum class SessionMode {
    Work,
    ShortBreak,
    LongBreak
};

struct Settings {
    int work_minutes = 25;
    int short_break_minutes = 5;
    int long_break_minutes = 15;
    int max_cycles = 4;
};

enum class CompletionAction {
    None,
    ShowStartRestAlert,
    ShowStartWorkAlert,
    ShowCycleFinishedAlert
};

class PomodoroEngine {
public:
    // The engine owns only timer state and session transitions.
    // It deliberately knows nothing about windows, sounds, or dialogs.
    PomodoroEngine();

    void ApplySettings(const Settings& settings);
    void Reset();
    void Start();
    void Pause();
    void StartRestPhase();
    void StartWorkPhase();
    CompletionAction Tick();

    bool IsRunning() const;
    int GetRemainingSeconds() const;
    int GetCurrentCycle() const;
    SessionMode GetMode() const;
    const Settings& GetSettings() const;

private:
    int GetModeDurationSeconds(SessionMode mode) const;

    Settings settings_{};
    SessionMode mode_ = SessionMode::Work;
    int remaining_seconds_ = 25 * 60;
    int current_cycle_ = 0;
    bool running_ = false;
};

}  // namespace Pomodoro
