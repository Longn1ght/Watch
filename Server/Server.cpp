#include"../Watch.h"
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI NetworkThread(LPVOID lpParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	g_PlayEndedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	g_GotVideoInfoEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	MSG msg;
	WNDCLASSEX wndclassex;
	wndclassex.cbSize = sizeof(WNDCLASSEX);
	wndclassex.style = CS_HREDRAW | CS_VREDRAW;
	wndclassex.lpfnWndProc = WndProc;
	wndclassex.cbClsExtra = 0;
	wndclassex.cbWndExtra = 0;
	wndclassex.hInstance = hInstance;
	wndclassex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclassex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclassex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndclassex.lpszMenuName = NULL;
	wndclassex.lpszClassName = L"WATCHSERVER";
	wndclassex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&wndclassex);
	RECT ClientRect = { 0, 0, 1280, 720 };
	AdjustWindowRectEx(&ClientRect, WS_OVERLAPPEDWINDOW, FALSE, 0);
	int winWidth = ClientRect.right - ClientRect.left;
	int winHeight = ClientRect.bottom - ClientRect.top;
	HWND hwnd = CreateWindowEx(0, L"WATCHSERVER", L"Watch Server", WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, CW_USEDEFAULT, winWidth, winHeight, NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HostViewDisplay hvd;
	switch (msg)
	{
	case WM_CREATE:
		CreateThread(NULL, 0, NetworkThread, &hvd, 0, NULL); // 创建网络线程
		WaitForSingleObject(g_GotVideoInfoEvent, INFINITE); // 等待获取视频信息事件
		hvd.Initialize(); // 初始化HostViewDisplay
		SetTimer(hwnd, 1, hvd.GetFrameRate() * 1000, NULL);// 创建一个定时器，每秒触发一次
		break;
	case WM_TIMER:
        hvd.RequestFrame(); // 请求新的一帧数据
		hvd.GetFrameData(); // 获取帧数据
		{
			HDC hdc = GetDC(hwnd);
			if (hdc)
			{
				hvd.OnScreenDisplay(hdc); // 在窗口上显示画面
				ReleaseDC(hwnd, hdc);
			}
		}
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		hvd.OnScreenDisplay(hdc); // 在窗口上显示画面
		EndPaint(hwnd, &ps);
	}
        break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

DWORD WINAPI NetworkThread(LPVOID lpParam)
{
	HostViewDisplay* pHvd = static_cast<HostViewDisplay*>(lpParam);
	if (!pHvd) return -1;

    NetworkModule nm(IDENTITY::HOST);

	BITMAPINFOHEADER header = {0};
	if (!nm.RecvNetFrameMessage(MESSAGE_TYPE::Info, &header))
	{
		MessageBox(nullptr, L"接收数据失败！", L"错误", MB_OK | MB_ICONERROR);
		return -1;
	}
	pHvd->SetBmpInfo(header);
	SetEvent(g_GotVideoInfoEvent);

	while (true)
	{
		WaitForSingleObject(g_PlayEndedEvent, INFINITE);
		if (!nm.RecvNetFrameMessage(MESSAGE_TYPE::Video, pHvd->lpMemVideoFile))
		{
			MessageBox(nullptr, L"接收数据失败！", L"错误", MB_OK | MB_ICONERROR);
			break;
		}
		ResetEvent(g_PlayEndedEvent);
	}
	return 0;
}
