#include "Logger.h"
#include "MyWindow.h"

void Cygnus::Log(const std::string& message)
{
	OutputDebugStringA(message.c_str());
}

void Cygnus::Log(const std::wstring& message)
{
	OutputDebugStringW(message.c_str());
}
