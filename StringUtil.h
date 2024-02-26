#pragma once
#include <windows.h>
#include <string>
#include <format>

namespace Cygnus {

    std::wstring ConvertString(const std::string& str);

    std::string ConvertString(const std::wstring& str);
}