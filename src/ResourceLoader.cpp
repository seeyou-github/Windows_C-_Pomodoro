#include "ResourceLoader.h"

#include <windows.h>

#include "res/resource.h"

namespace Pomodoro {

std::wstring LoadResString(unsigned int id) {
    wchar_t buffer[512] = {};
    const int length = LoadStringW(GetModuleHandleW(nullptr), id, buffer, static_cast<int>(std::size(buffer)));
    if (length <= 0) {
        return L"";
    }

    return std::wstring(buffer, buffer + length);
}

std::vector<std::wstring> LoadMotivationalTexts() {
    std::vector<std::wstring> texts;
    // Motivation strings are kept in the resource file so source files stay
    // ASCII-safe and Chinese text is loaded through Win32 wide-character APIs.
    for (unsigned int id = IDS_MOTIVATION_01; id <= IDS_MOTIVATION_40; ++id) {
        std::wstring value = LoadResString(id);
        if (!value.empty()) {
            texts.push_back(value);
        }
    }
    return texts;
}

}  // namespace Pomodoro
