#pragma once
#include <Windows.h>
#include <cstdint>

namespace Cygnus {
	class Window
	{
	public:
		static void Create(LPCWSTR windowTitle, uint32_t width, uint32_t height);

		static bool ProcessMessage();

		static uint32_t GetWidth();
		static uint32_t GetHeight();
		static HWND GetHandle();

	private:
		inline static uint32_t winWidth;
		inline static uint32_t winHeight;

		inline static HWND hwnd;
	};

	// ウィンドウプロシージャ
	LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
}

