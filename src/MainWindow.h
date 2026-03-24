#pragma once

#include <array>
#include <string>

#include <windows.h>

#include "OverlayWindows.h"
#include "PomodoroEngine.h"

namespace Pomodoro {

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);
    bool Create(int show_command);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);

    void RegisterWindowClass() const;
    void CreateFonts();
    void CreateControls();
    void LayoutControls(int client_width, int client_height) const;
    void CenterWindow() const;
    void ApplyDarkTitleBar() const;
    void UpdateUi();
    void UpdateRunningStateButtons() const;
    void StartTimerIfNeeded() const;
    void StopTimer() const;
    void ShowValidationError(const std::wstring& message) const;
    void BeepNotify() const;
    bool ReadSettingsFromUi(Settings& settings) const;
    std::wstring GetModeText(SessionMode mode) const;
    std::wstring FormatCurrentCycle() const;
    void StartTimerCommand();
    void PauseTimerCommand();
    void ResetTimerCommand();
    void HandleCompletion(CompletionAction action);
    void UpdateMotivationalOverlay();
    void EnsureTrayIcon();
    void RemoveTrayIcon();
    void MinimizeToTray();
    void RestoreFromTray();
    void ShowTrayMenu();
    void PaintBackground(HDC dc) const;
    LRESULT HandleCtlColor(HDC dc, HWND control) const;
    LRESULT HandleDrawItem(const DRAWITEMSTRUCT* draw_item) const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    PomodoroEngine engine_{};
    MotivationalOverlay overlay_{};
    FullscreenAlert alert_{};

    HFONT title_font_ = nullptr;
    HFONT timer_font_ = nullptr;
    HFONT status_font_ = nullptr;
    HFONT body_font_ = nullptr;
    HFONT settings_font_ = nullptr;
    HFONT button_font_ = nullptr;

    std::array<HWND, 3> action_buttons_{};
    std::array<HWND, 4> setting_edits_{};
    std::array<HWND, 4> setting_labels_{};

    HWND timer_label_ = nullptr;
    HWND status_label_ = nullptr;
    HWND cycle_label_ = nullptr;
    HWND settings_group_ = nullptr;
    HWND header_label_ = nullptr;
    bool tray_icon_visible_ = false;
};

}  // namespace Pomodoro
