#include "MainWindow.h"

#include <algorithm>
#include <dwmapi.h>
#include <shellapi.h>

#include "ResourceLoader.h"
#include "res/resource.h"

namespace Pomodoro {
namespace {

constexpr wchar_t kMainWindowClassName[] = L"PomodoroMainWindow";
constexpr UINT_PTR kMainTimerId = 1;
constexpr int kWindowWidth = 620;
constexpr int kWindowHeight = 700;
constexpr UINT kTrayIconMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayMenuShowId = 40001;
constexpr UINT kTrayMenuExitId = 40002;

HICON LoadAppIconResource(HINSTANCE instance, int size) {
    return static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        size,
        size,
        LR_DEFAULTCOLOR));
}

constexpr COLORREF kWindowColor = RGB(24, 24, 27);
constexpr COLORREF kPanelColor = RGB(32, 32, 36);
constexpr COLORREF kEditColor = RGB(42, 42, 46);
constexpr COLORREF kTextColor = RGB(226, 226, 228);
constexpr COLORREF kMutedTextColor = RGB(166, 166, 173);
constexpr COLORREF kAccentColor = RGB(78, 144, 108);
constexpr COLORREF kDangerColor = RGB(156, 64, 76);
constexpr COLORREF kButtonColor = RGB(46, 46, 52);

constexpr int ID_BUTTON_START = 101;
constexpr int ID_BUTTON_PAUSE = 102;
constexpr int ID_BUTTON_RESET = 103;
constexpr int ID_EDIT_WORK = 201;
constexpr int ID_EDIT_SHORT = 202;
constexpr int ID_EDIT_LONG = 203;
constexpr int ID_EDIT_CYCLES = 204;

HFONT CreateUiFont(int height, int weight) {
    return CreateFontW(
        -height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

bool ParsePositiveInteger(HWND edit, int min_value, int max_value, int& output) {
    wchar_t buffer[32] = {};
    GetWindowTextW(edit, buffer, static_cast<int>(std::size(buffer)));

    wchar_t* end = nullptr;
    const long value = wcstol(buffer, &end, 10);
    if (end == buffer || *end != L'\0' || value < min_value || value > max_value) {
        return false;
    }

    output = static_cast<int>(value);
    return true;
}

std::wstring FormatTime(int total_seconds) {
    const int minutes = total_seconds / 60;
    const int seconds = total_seconds % 60;
    wchar_t buffer[16] = {};
    wsprintfW(buffer, L"%02d:%02d", minutes, seconds);
    return buffer;
}

int GetDisplayedCycleNumber(const PomodoroEngine& engine) {
    const int completed_cycles = engine.GetCurrentCycle();
    if (engine.GetMode() == SessionMode::Work) {
        return completed_cycles + 1;
    }
    return completed_cycles > 0 ? completed_cycles : 1;
}

}  // namespace

MainWindow::MainWindow(HINSTANCE instance) : instance_(instance) {
}

MainWindow::~MainWindow() {
    DestroyFonts();
}

bool MainWindow::Create(int show_command) {
    RegisterWindowClass();
    CreateFonts();

    hwnd_ = CreateWindowExW(
        0,
        kMainWindowClassName,
        LoadResString(IDS_APP_TITLE).c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
        nullptr, nullptr, instance_, this);

    if (hwnd_ == nullptr) {
        DestroyFonts();
        return false;
    }

    HICON large_icon = LoadAppIconResource(instance_, GetSystemMetrics(SM_CXICON));
    HICON small_icon = LoadAppIconResource(instance_, GetSystemMetrics(SM_CXSMICON));
    if (large_icon != nullptr) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
    }
    if (small_icon != nullptr) {
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }
    if (large_icon != nullptr) {
        DestroyIcon(large_icon);
    }
    if (small_icon != nullptr) {
        DestroyIcon(small_icon);
    }

    CreateControls();
    CenterWindow();
    // Win10 title bars can participate in dark mode through DWM attributes.
    ApplyDarkTitleBar();
    UpdateUi();

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = reinterpret_cast<MainWindow*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE:
        overlay_.Create(hwnd_);
        EnsureTrayIcon();
        return 0;
    case WM_SIZE:
        LayoutControls(LOWORD(l_param), HIWORD(l_param));
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case ID_BUTTON_START:
            StartTimerCommand();
            return 0;
        case ID_BUTTON_PAUSE:
            PauseTimerCommand();
            return 0;
        case ID_BUTTON_RESET:
            ResetTimerCommand();
            return 0;
        case kTrayMenuShowId:
            RestoreFromTray();
            return 0;
        case kTrayMenuExitId:
            DestroyWindow(hwnd_);
            return 0;
        default:
            break;
        }
        break;
    case WM_TIMER:
        if (w_param == kMainTimerId) {
            const CompletionAction action = engine_.Tick();
            UpdateUi();
            if (action != CompletionAction::None) {
                HandleCompletion(action);
            }
        }
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
        return HandleCtlColor(reinterpret_cast<HDC>(w_param), reinterpret_cast<HWND>(l_param));
    case WM_DRAWITEM:
        return HandleDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(l_param));
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        PaintBackground(dc);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_DESTROY:
        StopTimer();
        RemoveTrayIcon();
        overlay_.Hide();
        DestroyFonts();
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        if (engine_.IsRunning()) {
            MinimizeToTray();
            return 0;
        }
        DestroyWindow(hwnd_);
        return 0;
    case kTrayIconMessage:
        if (l_param == WM_LBUTTONDBLCLK || l_param == WM_LBUTTONUP) {
            RestoreFromTray();
            return 0;
        }
        if (l_param == WM_RBUTTONUP || l_param == WM_CONTEXTMENU) {
            ShowTrayMenu();
            return 0;
        }
        break;
    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

void MainWindow::RegisterWindowClass() const {
    WNDCLASSEXW window_class{};
    if (GetClassInfoExW(instance_, kMainWindowClassName, &window_class)) {
        return;
    }

    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance_;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kMainWindowClassName;
    window_class.hIcon = static_cast<HICON>(LoadImageW(
        instance_, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    window_class.hIconSm = static_cast<HICON>(LoadImageW(
        instance_, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_SHARED));
    RegisterClassExW(&window_class);
}

void MainWindow::CreateFonts() {
    title_font_ = CreateUiFont(20, FW_SEMIBOLD);
    timer_font_ = CreateUiFont(97, FW_BOLD);
    status_font_ = CreateUiFont(38, FW_SEMIBOLD);
    body_font_ = CreateUiFont(24, FW_NORMAL);
    settings_font_ = CreateUiFont(18, FW_NORMAL);
    button_font_ = CreateUiFont(20, FW_SEMIBOLD);
}

void MainWindow::DestroyFonts() {
    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
        title_font_ = nullptr;
    }
    if (timer_font_ != nullptr) {
        DeleteObject(timer_font_);
        timer_font_ = nullptr;
    }
    if (status_font_ != nullptr) {
        DeleteObject(status_font_);
        status_font_ = nullptr;
    }
    if (body_font_ != nullptr) {
        DeleteObject(body_font_);
        body_font_ = nullptr;
    }
    if (settings_font_ != nullptr) {
        DeleteObject(settings_font_);
        settings_font_ = nullptr;
    }
    if (button_font_ != nullptr) {
        DeleteObject(button_font_);
        button_font_ = nullptr;
    }
}

void MainWindow::CreateControls() {
    header_label_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    timer_label_ = CreateWindowExW(0, L"STATIC", L"25:00",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    status_label_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    cycle_label_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    settings_group_ = CreateWindowExW(0, L"BUTTON", LoadResString(IDS_SETTINGS_GROUP).c_str(),
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    action_buttons_[0] = CreateWindowExW(0, L"BUTTON", LoadResString(IDS_START_BUTTON).c_str(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_BUTTON_START), instance_, nullptr);
    action_buttons_[1] = CreateWindowExW(0, L"BUTTON", LoadResString(IDS_PAUSE_BUTTON).c_str(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_BUTTON_PAUSE), instance_, nullptr);
    action_buttons_[2] = CreateWindowExW(0, L"BUTTON", LoadResString(IDS_RESET_BUTTON).c_str(),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_BUTTON_RESET), instance_, nullptr);

    const int edit_ids[4] = {ID_EDIT_WORK, ID_EDIT_SHORT, ID_EDIT_LONG, ID_EDIT_CYCLES};
    const int label_ids[4] = {IDS_LABEL_WORK, IDS_LABEL_SHORT, IDS_LABEL_LONG, IDS_LABEL_CYCLES};
    const wchar_t* defaults[4] = {L"25", L"5", L"15", L"4"};

    for (int index = 0; index < 4; ++index) {
        setting_labels_[index] = CreateWindowExW(0, L"STATIC", LoadResString(label_ids[index]).c_str(),
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
        setting_edits_[index] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaults[index],
            WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_,
            reinterpret_cast<HMENU>(edit_ids[index]), instance_, nullptr);
        SendMessageW(setting_edits_[index], EM_SETLIMITTEXT, 2, 0);
    }

    SendMessageW(header_label_, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    SendMessageW(timer_label_, WM_SETFONT, reinterpret_cast<WPARAM>(timer_font_), TRUE);
    SendMessageW(status_label_, WM_SETFONT, reinterpret_cast<WPARAM>(status_font_), TRUE);
    SendMessageW(cycle_label_, WM_SETFONT, reinterpret_cast<WPARAM>(body_font_), TRUE);
    SendMessageW(settings_group_, WM_SETFONT, reinterpret_cast<WPARAM>(settings_font_), TRUE);

    for (HWND button : action_buttons_) {
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(button_font_), TRUE);
    }
    for (HWND label : setting_labels_) {
        SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(settings_font_), TRUE);
    }
    for (HWND edit : setting_edits_) {
        SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(settings_font_), TRUE);
    }
}

void MainWindow::LayoutControls(int client_width, int client_height) const {
    const int margin = 24;
    const int content_width = client_width - margin * 2;

    SetWindowPos(header_label_, nullptr, 0, 0, 0, 0, SWP_NOZORDER);
    SetWindowPos(timer_label_, nullptr, margin, 48, content_width, 132, SWP_NOZORDER);
    SetWindowPos(status_label_, nullptr, margin, 188, content_width, 56, SWP_NOZORDER);
    SetWindowPos(cycle_label_, nullptr, margin, 248, content_width, 40, SWP_NOZORDER);

    const int button_width = (content_width - 24) / 3;
    const int button_y = 304;
    SetWindowPos(action_buttons_[0], nullptr, margin, button_y, button_width, 48, SWP_NOZORDER);
    SetWindowPos(action_buttons_[1], nullptr, margin + button_width + 12, button_y, button_width, 48, SWP_NOZORDER);
    SetWindowPos(action_buttons_[2], nullptr, margin + (button_width + 12) * 2, button_y, button_width, 48, SWP_NOZORDER);

    const int group_y = 378;
    const int group_height = std::max(220, client_height - group_y - margin);
    SetWindowPos(settings_group_, nullptr, margin, group_y, content_width, group_height, SWP_NOZORDER);

    const int row_height = 82;
    const int left_x = margin + 24;
    const int right_x = margin + content_width / 2 + 8;
    const int label_width = 190;
    const int edit_width = 110;

    for (int row = 0; row < 2; ++row) {
        const int y = group_y + 42 + row * row_height;
        const int left_index = row * 2;
        const int right_index = left_index + 1;

        SetWindowPos(setting_labels_[left_index], nullptr, left_x, y, label_width, 28, SWP_NOZORDER);
        SetWindowPos(setting_edits_[left_index], nullptr, left_x, y + 30, edit_width, 30, SWP_NOZORDER);
        SetWindowPos(setting_labels_[right_index], nullptr, right_x, y, label_width, 28, SWP_NOZORDER);
        SetWindowPos(setting_edits_[right_index], nullptr, right_x, y + 30, edit_width, 30, SWP_NOZORDER);
    }
}

void MainWindow::CenterWindow() const {
    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd_, nullptr, (screen_width - width) / 2, (screen_height - height) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void MainWindow::ApplyDarkTitleBar() const {
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
}

void MainWindow::UpdateUi() {
    // Rendering is driven from engine state so a reset, tick, or modal action
    // all converge on the same code path.
    SetWindowTextW(timer_label_, FormatTime(engine_.GetRemainingSeconds()).c_str());
    SetWindowTextW(status_label_, GetModeText(engine_.GetMode()).c_str());
    SetWindowTextW(cycle_label_, FormatCurrentCycle().c_str());

    const COLORREF timer_color = engine_.GetMode() == SessionMode::Work ? kTextColor : kAccentColor;
    SetPropW(timer_label_, L"TIMER_COLOR", reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(timer_color)));

    UpdateRunningStateButtons();
    UpdateMotivationalOverlay();
}

void MainWindow::UpdateRunningStateButtons() const {
    EnableWindow(action_buttons_[0], engine_.IsRunning() ? FALSE : TRUE);
    EnableWindow(action_buttons_[1], engine_.IsRunning() ? TRUE : FALSE);
}

void MainWindow::StartTimerIfNeeded() const {
    SetTimer(hwnd_, kMainTimerId, 1000, nullptr);
}

void MainWindow::StopTimer() const {
    KillTimer(hwnd_, kMainTimerId);
}

void MainWindow::ShowValidationError(const std::wstring& message) const {
    MessageBoxW(hwnd_, message.c_str(), LoadResString(IDS_ERROR_TITLE).c_str(), MB_OK | MB_ICONERROR);
}

void MainWindow::BeepNotify() const {
    MessageBeep(MB_ICONEXCLAMATION);
}

bool MainWindow::ReadSettingsFromUi(Settings& settings) const {
    if (!ParsePositiveInteger(setting_edits_[0], 1, 60, settings.work_minutes) ||
        !ParsePositiveInteger(setting_edits_[1], 1, 60, settings.short_break_minutes) ||
        !ParsePositiveInteger(setting_edits_[2], 1, 60, settings.long_break_minutes) ||
        !ParsePositiveInteger(setting_edits_[3], 2, 10, settings.max_cycles)) {
        return false;
    }
    return true;
}

std::wstring MainWindow::GetModeText(SessionMode mode) const {
    switch (mode) {
    case SessionMode::Work:
        return LoadResString(IDS_MODE_WORK);
    case SessionMode::ShortBreak:
        return LoadResString(IDS_MODE_SHORT_BREAK);
    case SessionMode::LongBreak:
        return LoadResString(IDS_MODE_LONG_BREAK);
    }
    return L"";
}

std::wstring MainWindow::FormatCurrentCycle() const {
    return LoadResString(IDS_CURRENT_CYCLE_PREFIX) + std::to_wstring(GetDisplayedCycleNumber(engine_));
}

void MainWindow::StartTimerCommand() {
    Settings settings{};
    if (!ReadSettingsFromUi(settings)) {
        ShowValidationError(LoadResString(IDS_ERROR_INVALID_INPUT));
        return;
    }

    // The Python version re-reads settings whenever the user starts again.
    engine_.ApplySettings(settings);
    engine_.Start();
    StartTimerIfNeeded();
    UpdateUi();
}

void MainWindow::PauseTimerCommand() {
    engine_.Pause();
    StopTimer();
    UpdateUi();
}

void MainWindow::ResetTimerCommand() {
    Settings settings{};
    if (!ReadSettingsFromUi(settings)) {
        ShowValidationError(LoadResString(IDS_ERROR_INVALID_INPUT));
        return;
    }

    StopTimer();
    // Reset also re-loads the editable values, matching the original script.
    engine_.ApplySettings(settings);
    engine_.Reset();
    UpdateUi();
}

void MainWindow::HandleCompletion(CompletionAction action) {
    StopTimer();
    BeepNotify();

    // Each completion path pauses first, then waits for the user to confirm
    // the transition through the full-screen alert window.
    if (action == CompletionAction::ShowStartRestAlert) {
        if (!alert_.Show(hwnd_, LoadResString(IDS_ALERT_WORK_COMPLETE), [this]() {
            engine_.StartRestPhase();
            StartTimerIfNeeded();
            UpdateUi();
        })) {
            MessageBoxW(hwnd_, LoadResString(IDS_ALERT_WORK_COMPLETE).c_str(), LoadResString(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            engine_.StartRestPhase();
            StartTimerIfNeeded();
            UpdateUi();
        }
        return;
    }

    if (action == CompletionAction::ShowStartWorkAlert) {
        if (!alert_.Show(hwnd_, LoadResString(IDS_ALERT_SHORT_BREAK_COMPLETE), [this]() {
            engine_.StartWorkPhase();
            StartTimerIfNeeded();
            UpdateUi();
        })) {
            MessageBoxW(hwnd_, LoadResString(IDS_ALERT_SHORT_BREAK_COMPLETE).c_str(), LoadResString(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            engine_.StartWorkPhase();
            StartTimerIfNeeded();
            UpdateUi();
        }
        return;
    }

    if (action == CompletionAction::ShowCycleFinishedAlert) {
        if (!alert_.Show(hwnd_, LoadResString(IDS_ALERT_LONG_BREAK_COMPLETE), [this]() {
            engine_.Reset();
            UpdateUi();
        })) {
            MessageBoxW(hwnd_, LoadResString(IDS_ALERT_LONG_BREAK_COMPLETE).c_str(), LoadResString(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
            engine_.Reset();
            UpdateUi();
        }
    }
}

void MainWindow::UpdateMotivationalOverlay() {
    if (engine_.IsRunning() && engine_.GetMode() == SessionMode::Work) {
        overlay_.Show();
    } else {
        overlay_.Hide();
    }
}

void MainWindow::EnsureTrayIcon() {
    if (tray_icon_visible_) {
        return;
    }

    NOTIFYICONDATAW notify_data{};
    notify_data.cbSize = sizeof(notify_data);
    notify_data.hWnd = hwnd_;
    notify_data.uID = kTrayIconId;
    notify_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    notify_data.uCallbackMessage = kTrayIconMessage;
    HICON tray_icon = LoadAppIconResource(instance_, GetSystemMetrics(SM_CXSMICON));
    notify_data.hIcon = tray_icon;
    lstrcpynW(notify_data.szTip, LoadResString(IDS_APP_TITLE).c_str(), ARRAYSIZE(notify_data.szTip));

    tray_icon_visible_ = Shell_NotifyIconW(NIM_ADD, &notify_data) == TRUE;
    if (tray_icon != nullptr) {
        DestroyIcon(tray_icon);
    }
}

void MainWindow::RemoveTrayIcon() {
    if (!tray_icon_visible_) {
        return;
    }

    NOTIFYICONDATAW notify_data{};
    notify_data.cbSize = sizeof(notify_data);
    notify_data.hWnd = hwnd_;
    notify_data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &notify_data);
    tray_icon_visible_ = false;
}

void MainWindow::MinimizeToTray() {
    ShowWindow(hwnd_, SW_HIDE);
}

void MainWindow::RestoreFromTray() {
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
    UpdateUi();
}

void MainWindow::ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    AppendMenuW(menu, MF_STRING, kTrayMenuShowId, LoadResString(IDS_TRAY_SHOW).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayMenuExitId, LoadResString(IDS_TRAY_EXIT).c_str());

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::PaintBackground(HDC dc) const {
    RECT client{};
    GetClientRect(hwnd_, &client);

    HBRUSH background_brush = CreateSolidBrush(kWindowColor);
    FillRect(dc, &client, background_brush);
    DeleteObject(background_brush);

    RECT hero = {18, 14, client.right - 18, 292};
    HBRUSH hero_brush = CreateSolidBrush(kPanelColor);
    FillRect(dc, &hero, hero_brush);
    DeleteObject(hero_brush);

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(52, 52, 58));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, hero.left, hero.top, hero.right, hero.bottom, 18, 18);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

LRESULT MainWindow::HandleCtlColor(HDC dc, HWND control) const {
    const auto timer_color_prop = reinterpret_cast<ULONG_PTR>(GetPropW(control, L"TIMER_COLOR"));
    if (control == timer_label_) {
        SetTextColor(dc, timer_color_prop != 0 ? static_cast<COLORREF>(timer_color_prop) : kTextColor);
        SetBkColor(dc, kPanelColor);
        static HBRUSH panel_brush = CreateSolidBrush(kPanelColor);
        return reinterpret_cast<LRESULT>(panel_brush);
    }

    if (control == header_label_ || control == status_label_ || control == cycle_label_) {
        SetTextColor(dc, control == cycle_label_ ? kMutedTextColor : kTextColor);
        SetBkColor(dc, kPanelColor);
        static HBRUSH panel_brush = CreateSolidBrush(kPanelColor);
        return reinterpret_cast<LRESULT>(panel_brush);
    }

    if (control == settings_group_ || control == setting_labels_[0] || control == setting_labels_[1] ||
        control == setting_labels_[2] || control == setting_labels_[3]) {
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kWindowColor);
        static HBRUSH window_brush = CreateSolidBrush(kWindowColor);
        return reinterpret_cast<LRESULT>(window_brush);
    }

    if (control == setting_edits_[0] || control == setting_edits_[1] ||
        control == setting_edits_[2] || control == setting_edits_[3]) {
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kEditColor);
        static HBRUSH edit_brush = CreateSolidBrush(kEditColor);
        return reinterpret_cast<LRESULT>(edit_brush);
    }

    SetTextColor(dc, kTextColor);
    SetBkColor(dc, kWindowColor);
    static HBRUSH default_brush = CreateSolidBrush(kWindowColor);
    return reinterpret_cast<LRESULT>(default_brush);
}

LRESULT MainWindow::HandleDrawItem(const DRAWITEMSTRUCT* draw_item) const {
    if (draw_item == nullptr || draw_item->CtlType != ODT_BUTTON) {
        return FALSE;
    }

    COLORREF background = kButtonColor;
    if ((draw_item->itemState & ODS_DISABLED) != 0) {
        background = RGB(36, 36, 40);
    } else if ((draw_item->itemState & ODS_SELECTED) != 0) {
        background = kDangerColor;
    } else if (draw_item->CtlID == ID_BUTTON_START) {
        background = kAccentColor;
    } else if (draw_item->CtlID == ID_BUTTON_RESET) {
        background = RGB(67, 74, 98);
    }

    HBRUSH brush = CreateSolidBrush(background);
    FillRect(draw_item->hDC, &draw_item->rcItem, brush);
    DeleteObject(brush);

    SetBkMode(draw_item->hDC, TRANSPARENT);
    SetTextColor(draw_item->hDC, (draw_item->itemState & ODS_DISABLED) != 0 ? kMutedTextColor : RGB(244, 244, 246));

    wchar_t text[64] = {};
    GetWindowTextW(draw_item->hwndItem, text, static_cast<int>(std::size(text)));
    RECT rect = draw_item->rcItem;
    DrawTextW(draw_item->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if ((draw_item->itemState & ODS_FOCUS) != 0) {
        RECT focus_rect = draw_item->rcItem;
        InflateRect(&focus_rect, -4, -4);
        DrawFocusRect(draw_item->hDC, &focus_rect);
    }

    return TRUE;
}

}  // namespace Pomodoro
