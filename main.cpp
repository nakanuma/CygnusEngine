#include "MyWindow.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	Cygnus::Window::Create(L"test", 1280, 720);

	while (!Cygnus::Window::ProcessMessage()) {
		// ゲームループ

	}

	return 0;
}