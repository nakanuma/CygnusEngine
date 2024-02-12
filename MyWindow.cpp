#include "MyWindow.h"

LRESULT Cygnus::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Cygnus::Window::Create(LPCWSTR windowTitle, uint32_t width, uint32_t height)
{
	WNDCLASS wc{};

	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;

	// ウィンドウクラス名
	wc.lpszClassName = windowTitle;

	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);

	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);


	// ウィンドウクラを登録する
	RegisterClass(&wc);

	// クライアント領域のサイズ
	winWidth = width;
	winHeight = height;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,static_cast<LONG>(winWidth),static_cast<LONG>(winHeight) };

	// クライアント領域を元に実際のサイズをwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成

	hwnd = CreateWindow(
		wc.lpszClassName,		// 利用するクラス名
		windowTitle,			// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	// よく見るウィンドウスタイル
		CW_USEDEFAULT,			// 表示X座標(Windowsに任せる)
		CW_USEDEFAULT,			// 表示Y座標(WindowsOSに任せる)
		wrc.right - wrc.left,	// ウィンドウ横幅
		wrc.bottom - wrc.top,	// ウィンドウ縦幅
		nullptr,				// 親ウィンドウハンドル
		nullptr,				// メニューハンドル
		wc.hInstance,			// インスタンスハンドル
		nullptr					// オプション
	);

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);
}

bool Cygnus::Window::ProcessMessage()
{
	MSG msg{};

	// ウィンドウの×ボタンが押されるまでループ
	// Windowにメッセージが来てたら最優先で処理させる
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.message == WM_QUIT;
}

uint32_t Cygnus::Window::GetWidth()
{
	return winWidth;
}

uint32_t Cygnus::Window::GetHeight()
{
	return winHeight;
}
