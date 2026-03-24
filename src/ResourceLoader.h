#pragma once

#include <string>
#include <vector>

namespace Pomodoro {

std::wstring LoadResString(unsigned int id);
std::vector<std::wstring> LoadMotivationalTexts();

}  // namespace Pomodoro
