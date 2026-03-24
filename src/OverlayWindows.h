#pragma once

#include <functional>
#include <random>
#include <string>
#include <vector>

#include <windows.h>

namespace Pomodoro {

class MotivationalOverlay {
public:
    MotivationalOverlay();

    bool Create(HWND owner);
    void Show();
    void Hide();
    bool IsVisible() const;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);
    void UpdateText();
    void ResizeToFitText();
    void PositionNearTopCenter() const;
    void RestartRefreshTimer();
    void RedrawLayeredWindow() const;

    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    HFONT font_ = nullptr;
    bool dragging_ = false;
    POINT drag_offset_{};
    std::vector<std::wstring> texts_;
    std::mt19937 random_engine_;
    std::wstring current_text_;
};

class FullscreenAlert {
public:
    FullscreenAlert() = default;
    ~FullscreenAlert();

    bool Show(HWND owner, const std::wstring& message, std::function<void()> on_continue);
    void Close();
    bool IsVisible() const;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);
    void CreateFontsIfNeeded();
    void ResizeChildren() const;

    HWND hwnd_ = nullptr;
    HWND label_hwnd_ = nullptr;
    HWND button_hwnd_ = nullptr;
    HFONT title_font_ = nullptr;
    HFONT button_font_ = nullptr;
    std::function<void()> on_continue_;
};

}  // namespace Pomodoro
