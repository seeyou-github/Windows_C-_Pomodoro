#include <windows.h>
#include "MainWindow.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command) {
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        auto set_dpi_awareness_context =
            reinterpret_cast<SetDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_dpi_awareness_context != nullptr) {
            set_dpi_awareness_context(reinterpret_cast<HANDLE>(-4));
        }
    }

    Pomodoro::MainWindow window(instance);
    if (!window.Create(show_command)) {
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
