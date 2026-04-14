#include <windows.h>
#include "MainWindow.h"
#include "ResourceLoader.h"
#include "res/resource.h"

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

    HANDLE single_instance_mutex = CreateMutexW(nullptr, FALSE, L"Global\\WindowsPomodoro_SingleInstance_Mutex");
    if (single_instance_mutex == nullptr) {
        MessageBoxW(nullptr, Pomodoro::LoadResString(IDS_SINGLE_INSTANCE_CREATE_FAILED).c_str(),
            Pomodoro::LoadResString(IDS_ERROR_TITLE).c_str(), MB_ICONERROR);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, Pomodoro::LoadResString(IDS_ALREADY_RUNNING).c_str(),
            Pomodoro::LoadResString(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
        CloseHandle(single_instance_mutex);
        return 0;
    }

    Pomodoro::MainWindow window(instance);
    if (!window.Create(show_command)) {
        CloseHandle(single_instance_mutex);
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CloseHandle(single_instance_mutex);
    return static_cast<int>(message.wParam);
}
