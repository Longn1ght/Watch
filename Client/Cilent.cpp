#include"../Wacth.h"


int main()
{
	ClientViewCatch cvc;
	if (!cvc.Initialize())
	{
		MessageBoxA(NULL, "初始化失败！", "错误", MB_OK | MB_ICONERROR);
		return -1;
	}
	CreateThread(NULL, 0, ClientViewCatch::CaptureThread, &cvc, 0, NULL);
	while (true)
	{
		cvc.StartStopCapture();

	}
	return 0;
}