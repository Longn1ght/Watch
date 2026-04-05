#include"../Watch.h"

DWORD WINAPI NetworkThread(LPVOID lpParam);
int main()
{
	g_ReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	ClientViewCatch cvc;
	if (!cvc.Initialize())
	{
		MessageBox(NULL, L"初始化失败！", L"错误", MB_OK | MB_ICONERROR);
		return -1;
	}
	CreateThread(NULL, 0, ClientViewCatch::CaptureThread, &cvc, 0, NULL);
	CreateThread(NULL, 0, NetworkThread, &cvc, 0, NULL);
	cvc.StartStopCapture();
	while (true)
	{
		//占位，后续当成命令处理线程，用Sleep减小CPU占用
		Sleep(100);
	}
	return 0;
}

DWORD WINAPI NetworkThread(LPVOID lpParam)
{
	static ClientViewCatch* cvc = static_cast<ClientViewCatch*>(lpParam);
	static NetworkModule nm(IDENTITY::CLIENT);
	//首次需要发送视频信息，后续只发送视频数据
	if (!nm.SendNetFrameMessage(Info, cvc->GetBmpInfo()))
	{
		MessageBox(nullptr, L"发送数据失败！", L"错误", MB_OK | MB_ICONERROR);
		return -1;
	}
	while (true)
	{
		WaitForSingleObject(g_ReadyEvent, INFINITE); // 等待捕获事件
		if (!nm.SendNetFrameMessage(MESSAGE_TYPE::Video, cvc->lpMemVideoFile))
		{
			MessageBox(nullptr, L"发送数据失败！", L"错误", MB_OK | MB_ICONERROR);
			break;
		}
		ResetEvent(g_ReadyEvent);
	}
	return 0;
}