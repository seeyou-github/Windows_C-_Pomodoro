#include "OverlayWindows.h"

#include <algorithm>
#include <windowsx.h>

#include "ResourceLoader.h"
#include "res/resource.h"

namespace Pomodoro {
namespace {

constexpr wchar_t kOverlayClassName[] = L"PomodoroOverlayWindow";
constexpr wchar_t kAlertClassName[] = L"PomodoroAlertWindow";
constexpr UINT_PTR kOverlayTextTimerId = 1;
constexpr UINT_PTR kAlertContinueButtonId = 1001;

void RegisterWindowClass(const wchar_t* class_name, WNDPROC proc, HBRUSH brush) {
    WNDCLASSEXW window_class{};
    if (GetClassInfoExW(GetModuleHandleW(nullptr), class_name, &window_class)) {
        return;
    }

    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = brush;
    window_class.lpszClassName = class_name;
    RegisterClassExW(&window_class);
}

HFONT CreateUiFont(int height, int weight) {
    return CreateFontW(
        -height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

}  // namespace

MotivationalOverlay::MotivationalOverlay()
    : texts_(LoadMotivationalTexts()), random_engine_(std::random_device{}()) {
}

bool MotivationalOverlay::Create(HWND owner) {
    if (hwnd_ != nullptr) {
        return true;
    }

    owner_ = owner;
    RegisterWindowClass(kOverlayClassName, WindowProc, nullptr);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kOverlayClassName,
        L"",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 64,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (hwnd_ == nullptr) {
        return false;
    }

    // Keep a subtle translucent background so the text stays readable while
    // remaining lighter than a normal tooltip panel.
    font_ = CreateUiFont(30, FW_BOLD);

    UpdateText();
    PositionNearTopCenter();
    return true;
}

void MotivationalOverlay::Show() {
    if (hwnd_ == nullptr) {
        if (!Create(owner_)) {
            return;
        }
    }

    if (!IsVisible()) {
        UpdateText();
        PositionNearTopCenter();
        RedrawLayeredWindow();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        RestartRefreshTimer();
    }
}

void MotivationalOverlay::Hide() {
    if (hwnd_ == nullptr) {
        return;
    }

    KillTimer(hwnd_, kOverlayTextTimerId);
    ShowWindow(hwnd_, SW_HIDE);
}

bool MotivationalOverlay::IsVisible() const {
    return hwnd_ != nullptr && IsWindowVisible(hwnd_) != FALSE;
}

LRESULT CALLBACK MotivationalOverlay::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* self = reinterpret_cast<MotivationalOverlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = reinterpret_cast<MotivationalOverlay*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            self->hwnd_ = hwnd;
        }
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT MotivationalOverlay::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_TIMER:
        if (w_param == kOverlayTextTimerId) {
            UpdateText();
            RestartRefreshTimer();
        }
        return 0;
    case WM_LBUTTONDOWN:
        dragging_ = true;
        drag_offset_.x = GET_X_LPARAM(l_param);
        drag_offset_.y = GET_Y_LPARAM(l_param);
        SetCapture(hwnd_);
        return 0;
    case WM_MOUSEMOVE:
        if (dragging_) {
            POINT cursor{};
            GetCursorPos(&cursor);
            SetWindowPos(hwnd_, HWND_TOPMOST, cursor.x - drag_offset_.x, cursor.y - drag_offset_.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_LBUTTONUP:
        dragging_ = false;
        ReleaseCapture();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd_, kOverlayTextTimerId);
        if (font_ != nullptr) {
            DeleteObject(font_);
            font_ = nullptr;
        }
        hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, w_param, l_param);
    }
}

void MotivationalOverlay::UpdateText() {
    if (hwnd_ == nullptr || texts_.empty()) {
        return;
    }

    std::uniform_int_distribution<size_t> index_distribution(0, texts_.size() - 1);
    current_text_ = texts_[index_distribution(random_engine_)];
    ResizeToFitText();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MotivationalOverlay::ResizeToFitText() {
    if (hwnd_ == nullptr || font_ == nullptr || current_text_.empty()) {
        return;
    }

    HDC dc = GetDC(hwnd_);
    HGDIOBJ old_font = SelectObject(dc, font_);

    RECT text_rect{0, 0, 0, 0};
    DrawTextW(dc, current_text_.c_str(), -1, &text_rect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(dc, old_font);
    ReleaseDC(hwnd_, dc);

    const int width = (text_rect.right - text_rect.left) + 36;
    const int height = (text_rect.bottom - text_rect.top) + 20;
    SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RedrawLayeredWindow();
}

void MotivationalOverlay::PositionNearTopCenter() const {
    if (hwnd_ == nullptr) {
        return;
    }

    RECT rect{};
    GetWindowRect(hwnd_, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    SetWindowPos(hwnd_, HWND_TOPMOST, (screen_width - width) / 2, 12, width, height, SWP_NOACTIVATE);
}

void MotivationalOverlay::RestartRefreshTimer() {
    if (hwnd_ == nullptr) {
        return;
    }

    KillTimer(hwnd_, kOverlayTextTimerId);
    SetTimer(hwnd_, kOverlayTextTimerId, 60 * 1000u, nullptr);
}

void MotivationalOverlay::RedrawLayeredWindow() const {
    if (hwnd_ == nullptr || font_ == nullptr) {
        return;
    }

    RECT window_rect{};
    GetWindowRect(hwnd_, &window_rect);
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    HDC screen_dc = GetDC(nullptr);
    if (screen_dc == nullptr) {
        return;
    }

    HDC memory_dc = CreateCompatibleDC(screen_dc);
    if (memory_dc == nullptr) {
        ReleaseDC(nullptr, screen_dc);
        return;
    }

    void* pixel_data = nullptr;
    HBITMAP dib = CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &pixel_data, nullptr, 0);
    if (dib == nullptr || pixel_data == nullptr) {
        if (dib != nullptr) {
            DeleteObject(dib);
        }
        DeleteDC(memory_dc);
        ReleaseDC(nullptr, screen_dc);
        return;
    }

    HGDIOBJ old_bitmap = SelectObject(memory_dc, dib);
    if (old_bitmap == nullptr || old_bitmap == HGDI_ERROR) {
        DeleteObject(dib);
        DeleteDC(memory_dc);
        ReleaseDC(nullptr, screen_dc);
        return;
    }

    GdiFlush();
    std::fill_n(static_cast<unsigned int*>(pixel_data), static_cast<size_t>(width) * static_cast<size_t>(height), 0u);

    constexpr BYTE background_alpha = 88;
    constexpr BYTE text_alpha = 255;
    const COLORREF background_color = RGB(34, 34, 38);
    const COLORREF border_color = RGB(64, 64, 70);
    const COLORREF text_color = RGB(232, 232, 235);

    auto set_pixel = [pixel_data, width, height](int x, int y, COLORREF color, BYTE alpha) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }
        auto* pixels = static_cast<unsigned int*>(pixel_data);
        const BYTE r = GetRValue(color);
        const BYTE g = GetGValue(color);
        const BYTE b = GetBValue(color);
        pixels[y * width + x] = (static_cast<unsigned int>(alpha) << 24) |
            (static_cast<unsigned int>(r) << 16) |
            (static_cast<unsigned int>(g) << 8) |
            static_cast<unsigned int>(b);
    };

    auto inside_rounded_rect = [width, height](int x, int y) {
        constexpr int radius = 16;
        const int left = 0;
        const int top = 0;
        const int right = width - 1;
        const int bottom = height - 1;

        if (x >= left + radius && x <= right - radius) return true;
        if (y >= top + radius && y <= bottom - radius) return true;

        const int corners[4][2] = {
            {left + radius, top + radius},
            {right - radius, top + radius},
            {left + radius, bottom - radius},
            {right - radius, bottom - radius}
        };

        for (const auto& corner : corners) {
            const int dx = x - corner[0];
            const int dy = y - corner[1];
            if (dx * dx + dy * dy <= radius * radius) {
                return true;
            }
        }
        return false;
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!inside_rounded_rect(x, y)) {
                continue;
            }

            const bool is_border = !inside_rounded_rect(std::max(0, x - 1), y) ||
                !inside_rounded_rect(std::min(width - 1, x + 1), y) ||
                !inside_rounded_rect(x, std::max(0, y - 1)) ||
                !inside_rounded_rect(x, std::min(height - 1, y + 1));
            set_pixel(x, y, is_border ? border_color : background_color, background_alpha);
        }
    }

    SetBkMode(memory_dc, TRANSPARENT);
    SetTextColor(memory_dc, text_color);
    HGDIOBJ old_font = SelectObject(memory_dc, font_);
    if (old_font == nullptr || old_font == HGDI_ERROR) {
        SelectObject(memory_dc, old_bitmap);
        DeleteObject(dib);
        DeleteDC(memory_dc);
        ReleaseDC(nullptr, screen_dc);
        return;
    }

    RECT text_rect{14, 6, width - 14, height - 6};
    DrawTextW(memory_dc, current_text_.c_str(), -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Make text fully opaque by fixing its alpha after GDI draws glyph colors.
    auto* pixels = static_cast<unsigned int*>(pixel_data);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned int& pixel = pixels[y * width + x];
            const BYTE b = static_cast<BYTE>(pixel & 0xFF);
            const BYTE g = static_cast<BYTE>((pixel >> 8) & 0xFF);
            const BYTE r = static_cast<BYTE>((pixel >> 16) & 0xFF);
            const BYTE a = static_cast<BYTE>((pixel >> 24) & 0xFF);

            if (a == 0 && (r != 0 || g != 0 || b != 0)) {
                pixel = (static_cast<unsigned int>(text_alpha) << 24) |
                    (static_cast<unsigned int>(r) << 16) |
                    (static_cast<unsigned int>(g) << 8) |
                    static_cast<unsigned int>(b);
            }
        }
    }

    POINT source_point{0, 0};
    SIZE size{width, height};
    POINT window_position{window_rect.left, window_rect.top};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd_, screen_dc, &window_position, &size, memory_dc, &source_point, 0, &blend, ULW_ALPHA);

    SelectObject(memory_dc, old_font);
    SelectObject(memory_dc, old_bitmap);
    DeleteObject(dib);
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);
}

FullscreenAlert::~FullscreenAlert() {
    Close();
}

bool FullscreenAlert::Show(HWND owner, const std::wstring& message, std::function<void()> on_continue) {
    on_continue_ = std::move(on_continue);

    // A full-screen popup is used instead of a regular message box so the
    // break/work transition is hard to miss, which mirrors the Python app.
    RegisterWindowClass(kAlertClassName, WindowProc, nullptr);
    CreateFontsIfNeeded();

    if (hwnd_ == nullptr) {
        HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        GetMonitorInfoW(monitor, &monitor_info);
        const RECT& monitor_rect = monitor_info.rcMonitor;

        hwnd_ = CreateWindowExW(
            WS_EX_TOPMOST,
            kAlertClassName,
            L"",
            WS_POPUP,
            monitor_rect.left,
            monitor_rect.top,
            monitor_rect.right - monitor_rect.left,
            monitor_rect.bottom - monitor_rect.top,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            this);
    }

    if (hwnd_ == nullptr) {
        return false;
    }

    if (label_hwnd_ == nullptr) {
        label_hwnd_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        button_hwnd_ = CreateWindowExW(0, L"BUTTON", LoadResString(IDS_CONTINUE_BUTTON).c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_,
            reinterpret_cast<HMENU>(kAlertContinueButtonId), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(label_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
        SendMessageW(button_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(button_font_), TRUE);
    }

    SetWindowTextW(label_hwnd_, message.c_str());
    HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfoW(monitor, &monitor_info) != FALSE) {
        const RECT& monitor_rect = monitor_info.rcMonitor;
        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            monitor_rect.left,
            monitor_rect.top,
            monitor_rect.right - monitor_rect.left,
            monitor_rect.bottom - monitor_rect.top,
            SWP_SHOWWINDOW);
    }
    ResizeChildren();
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetActiveWindow(hwnd_);
    return true;
}

void FullscreenAlert::Close() {
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        label_hwnd_ = nullptr;
        button_hwnd_ = nullptr;
    }

    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
        title_font_ = nullptr;
    }
    if (button_font_ != nullptr) {
        DeleteObject(button_font_);
        button_font_ = nullptr;
    }
}

bool FullscreenAlert::IsVisible() const {
    return hwnd_ != nullptr && IsWindowVisible(hwnd_) != FALSE;
}

LRESULT CALLBACK FullscreenAlert::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* self = reinterpret_cast<FullscreenAlert*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = reinterpret_cast<FullscreenAlert*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            self->hwnd_ = hwnd;
        }
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT FullscreenAlert::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_COMMAND:
        if (LOWORD(w_param) == kAlertContinueButtonId) {
            auto callback = on_continue_;
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
            label_hwnd_ = nullptr;
            button_hwnd_ = nullptr;
            if (callback) {
                callback();
            }
        }
        return 0;
    case WM_SIZE:
        ResizeChildren();
        return 0;
    case WM_DRAWITEM: {
        const auto* draw_item = reinterpret_cast<DRAWITEMSTRUCT*>(l_param);
        if (draw_item != nullptr && draw_item->CtlID == kAlertContinueButtonId) {
            HBRUSH brush = CreateSolidBrush(RGB(45, 45, 48));
            FillRect(draw_item->hDC, &draw_item->rcItem, brush);
            DeleteObject(brush);

            SetBkMode(draw_item->hDC, TRANSPARENT);
            SetTextColor(draw_item->hDC, RGB(240, 240, 240));
            RECT rect = draw_item->rcItem;
            std::wstring text = LoadResString(IDS_CONTINUE_BUTTON);
            DrawTextW(draw_item->hDC, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        HBRUSH brush = CreateSolidBrush(RGB(150, 44, 56));
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        EndPaint(hwnd_, &paint);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        auto static_dc = reinterpret_cast<HDC>(w_param);
        SetTextColor(static_dc, RGB(255, 245, 245));
        SetBkColor(static_dc, RGB(150, 44, 56));
        static HBRUSH brush = CreateSolidBrush(RGB(150, 44, 56));
        return reinterpret_cast<LRESULT>(brush);
    }
    case WM_CLOSE:
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, w_param, l_param);
    }

    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

void FullscreenAlert::CreateFontsIfNeeded() {
    if (title_font_ == nullptr) {
        title_font_ = CreateUiFont(36, FW_SEMIBOLD);
    }
    if (button_font_ == nullptr) {
        button_font_ = CreateUiFont(18, FW_SEMIBOLD);
    }
}

void FullscreenAlert::ResizeChildren() const {
    if (hwnd_ == nullptr || label_hwnd_ == nullptr || button_hwnd_ == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    SetWindowPos(label_hwnd_, nullptr, 120, height / 2 - 120, width - 240, 160, SWP_NOZORDER);
    SetWindowPos(button_hwnd_, nullptr, width / 2 - 90, height / 2 + 50, 180, 54, SWP_NOZORDER);
}

}  // namespace Pomodoro
