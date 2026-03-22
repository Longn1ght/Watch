#pragma once
#include<winsock2.h>//应先于windows.h
#include<ws2tcpip.h>
#include<Windows.h>
#include<vector>
#include<chrono>
#include<thread>
#include<Shlobj.h>
#include<TCHAR.h>
#include<strsafe.h>
extern "C" {
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libavutil/avutil.h>
#include<libavutil/opt.h>
#include<libavutil/imgutils.h>  
#include<libswscale/swscale.h>
}

#pragma comment(lib,"Ws2_32.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;

struct Color_RGB
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct bdata
{
	uint8_t* buf;
	size_t size;
	size_t pos;
};

enum IDENTITY
{
	HOST=0,CLIENT=1
};

enum MESSAGE_TYPE
{
	Video=0,Command=1
};

class ClientViewCatch
{
private:
	int targetframe;
	double framerate;
	HDC hdc;
	HBITMAP hBmp;
	vector<vector<Color_RGB>>DIBDatas;	//一组DIB数据
	BITMAPINFOHEADER BmpInfo;
	BOOL bRunning;
	HANDLE hThread;
	HANDLE hCaptureEvent;

	//FFmpeg库
	AVIOContext* ioctx;
	const AVOutputFormat* ofmt;
	AVFormatContext* fmtctx;
	AVCodecContext* c;
	AVStream* st;
	AVFrame* frame;
	bdata bd;
	AVPacket* pkt;
	int frame_index;
	SwsContext* sws_ctx;
	bool header_written;
public:
	uint8_t* lpMemVideoFile;		//内存中的视频文件

	ClientViewCatch();
	~ClientViewCatch();
	//客户端的画面捕获，
	//应当涉及:画面初步捕捉，编码，封装
	bool Initialize();
	bool OnScreenShots();
	bool EncodeSomeFramesAndMuxing();
	bool FlushAndWriteTrailer();
	//后续可能会加入hdc更换、目标帧率修改之类的

	VOID StartStopCapture();

	static int write_packet(void* opaque, const uint8_t* buf, int buf_size);
	static int64_t seek(void* opaque, int64_t offset, int whence);

	//捕获线程
	static DWORD WINAPI CaptureThread(LPVOID lpParam);
};

class NetworkModule
{
public:
	bool SendNetFrameMessage(IDENTITY id,MESSAGE_TYPE mt,LPVOID lpData);
	bool RecvNetFrameMessage(IDENTITY id, MESSAGE_TYPE mt, LPVOID lpData);
};