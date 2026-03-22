#include "Wacth.h"

//ClientViewCatch类的实现
ClientViewCatch::ClientViewCatch()
{
    BmpInfo.biSize = sizeof(BITMAPINFOHEADER);
	BmpInfo.biWidth = GetSystemMetrics(SM_CXSCREEN);
	BmpInfo.biHeight = GetSystemMetrics(SM_CYSCREEN);
	BmpInfo.biPlanes = 1;
	BmpInfo.biBitCount = 24;
	BmpInfo.biCompression = BI_RGB;
	bRunning = FALSE;
	hThread = nullptr;
	hCaptureEvent = nullptr;

	//将ffmpeg结构体全部初始化为nullptr
	ioctx = nullptr;
	fmtctx = nullptr;
	header_written = false;
	sws_ctx = nullptr;
	frame = nullptr;
	pkt = nullptr;
	c = nullptr;
	hBmp = nullptr;
	hdc = nullptr;
	lpMemVideoFile = nullptr;
};
ClientViewCatch::~ClientViewCatch()
{
	if (sws_ctx) 
		sws_freeContext(sws_ctx);
	if (frame) 
		av_frame_free(&frame);
	if (pkt)
		av_packet_free(&pkt);
	if (c)
		avcodec_free_context(&c);
	if (ioctx)
		avio_context_free(&ioctx);
	if (fmtctx)
		avformat_free_context(fmtctx);
	if (hBmp) DeleteObject(hBmp);
	if (hdc) DeleteDC(hdc);
	if (lpMemVideoFile) VirtualFree(lpMemVideoFile, 0, MEM_RELEASE);
};


bool ClientViewCatch::Initialize()
{
    DIBDatas.resize(targetframe);
    for (size_t i = 0; i < targetframe; i++)
    {
        DIBDatas[i].resize(BmpInfo.biWidth*BmpInfo.biHeight);
    }    
	targetframe = 30;	//默认捕获30帧,暂时规定，后续提供接口修改
    framerate = 1 / targetframe;
	hdc = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
	hBmp = CreateCompatibleBitmap(hdc, BmpInfo.biWidth, BmpInfo.biHeight);

	//FFmpeg库的初始化
	//初始化封装容器
	bd.size = BmpInfo.biWidth * BmpInfo.biHeight * 5 * targetframe * sizeof(Color_RGB);//这里的计算原理是：每帧的大小为宽*高*3字节（24位色），每秒30帧，那么总大小就是宽*高*3*targetframe
	lpMemVideoFile = (uint8_t*)VirtualAlloc(NULL, bd.size, MEM_COMMIT, PAGE_READWRITE);
	bd.buf = lpMemVideoFile;
	bd.pos = 0;
	avformat_alloc_output_context2(&fmtctx, NULL, "mp4", NULL);
	ofmt = fmtctx->oformat;
	auto avio_buffer = av_malloc(4096);
	ioctx = avio_alloc_context((unsigned char*)avio_buffer, 4096, 1, &bd, NULL, &write_packet, &seek);
	fmtctx->pb = ioctx;

	st = avformat_new_stream(fmtctx, NULL);
	st->id = fmtctx->nb_streams - 1;

	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	c = avcodec_alloc_context3(codec);
	pkt = av_packet_alloc();

	c->bit_rate = 400000;
	c->width = BmpInfo.biWidth;
	c->height = BmpInfo.biHeight;
	c->time_base = {1, 30};
	c->framerate = {30, 1};
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;
	int ret = av_opt_set(c->priv_data, "preset", "fast", 0);
	if (ret < 0) {
		return false;
	}

	ret=avcodec_open2(c, codec, NULL);
	if (ret<0)
		return false;

	sws_ctx = sws_getContext(BmpInfo.biWidth, BmpInfo.biHeight, AV_PIX_FMT_BGR24, BmpInfo.biWidth, BmpInfo.biHeight, AV_PIX_FMT_YUV420P,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (!sws_ctx)
		return false;

	frame = av_frame_alloc();
	frame->format = c->pix_fmt;
	frame->width = c->width;
	frame->height = c->height;
	av_frame_get_buffer(frame, 0);
	av_frame_make_writable(frame);

	avcodec_parameters_from_context(st->codecpar, c);
	return true;
}

bool ClientViewCatch::OnScreenShots()
{
	HDC hdcMem = CreateCompatibleDC(hdc);
	HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);
	BitBlt(hdcMem, 0, 0, BmpInfo.biWidth, BmpInfo.biHeight, hdc, 0, 0, SRCCOPY);
	GetDIBits(hdcMem, hBmp, 0, BmpInfo.biHeight, DIBDatas[frame_index].data(), (BITMAPINFO*)&BmpInfo, DIB_RGB_COLORS);
	SelectObject(hdcMem, hOldBmp);
	DeleteDC(hdcMem);
	return true;
};

bool ClientViewCatch::EncodeSomeFramesAndMuxing()
{
	// 如果还没有写入文件头，先写入
	if (!header_written) 
	{
		int ret = avformat_write_header(fmtctx, NULL);  // 需要 opt 可以传 NULL
		if (ret < 0) 
			return false;
		header_written = true;
	}

	// 遍历已收集的帧（frame_index 可能是 0~targetframe）
	for (int i = 0; i < frame_index; i++)
	{
		// 将 BGR 数据转换到 frame
		uint8_t* src_data[1] = { (uint8_t*)DIBDatas[i].data() };
		int src_linesize[1] = { BmpInfo.biWidth * 3 };
		sws_scale(sws_ctx, src_data, src_linesize, 0, BmpInfo.biHeight,
			frame->data, frame->linesize);

		// 设置 PTS（以帧为单位，这里用全局帧序号，或者用 i + 已编码帧数）
		// 如果每帧间隔 1/30 秒，并且时间基为 1/30，则 pts 就是帧序号
		frame->pts = i;  // 注意：如果多次调用 EncodeSomeFramesAndMuxing，这个 pts 需要累加

		// 发送帧到编码器
		int ret = avcodec_send_frame(c, frame);
		if (ret < 0) 
			return false;

		while (ret >= 0)
		{
			ret = avcodec_receive_packet(c, pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			if (ret < 0)
				return false;

			// 时间基转换（编码器的时间基是 c->time_base，封装器的时间基是 st->time_base）
			av_packet_rescale_ts(pkt, c->time_base, st->time_base);
			pkt->stream_index = st->index;
			av_interleaved_write_frame(fmtctx, pkt);
			av_packet_unref(pkt);
		}
	}

	// 重置 frame_index，准备下一轮捕获
	frame_index = 0;
	return true;
};	

bool ClientViewCatch::FlushAndWriteTrailer()
{
	// 冲刷编码器（发送 NULL 帧）
	int ret = avcodec_send_frame(c, NULL);
	if (ret < 0)
		return false;

	while (true) 
	{
		ret = avcodec_receive_packet(c, pkt);
		if (ret == AVERROR_EOF)
			break;
		if (ret < 0)
			return false;

		av_packet_rescale_ts(pkt, c->time_base, st->time_base);
		pkt->stream_index = st->index;
		av_interleaved_write_frame(fmtctx, pkt);
		av_packet_unref(pkt);
	}

	// 写文件尾
	av_write_trailer(fmtctx);
	return true;
}

int ClientViewCatch::write_packet(void* opaque, const uint8_t* buf, int buf_size)
{
	bdata* bd = (bdata*)opaque;
	if (bd->pos + buf_size > bd->size) {
		return -1; // Buffer overflow
	}
	memcpy_s(bd->buf + bd->pos, bd->size - bd->pos, buf, buf_size);
	bd->pos += buf_size;
	return buf_size;
}

int64_t ClientViewCatch::seek(void* opaque, int64_t offset, int whence)
{
	bdata* bd = (bdata*)opaque;
	switch (whence) {
	case SEEK_SET:
		bd->pos = offset;
		break;
	case SEEK_CUR:
		bd->pos += offset;
		break;
	case SEEK_END:
		bd->pos = bd->size + offset;
		break;
	default:
		return -1; // Invalid whence
	}
	if (bd->pos < 0 || bd->pos > bd->size) {
		return -1; // Out of bounds
	}
	return bd->pos;
}

DWORD WINAPI ClientViewCatch::CaptureThread(LPVOID lpParam)
{
	ClientViewCatch* pThis = (ClientViewCatch*)lpParam;

	while (pThis->bRunning)
	{
		WaitForSingleObject(pThis->hCaptureEvent, INFINITE); // 等待信号
		for (int f = 0; f < 30; f++) 
		{
			pThis->OnScreenShots();      // 捕获一帧
			Sleep(pThis->framerate);       // 控制帧率，实际应该用更精确的计时
		}
		pThis->EncodeSomeFramesAndMuxing();  // 编码这一秒
		pThis->FlushAndWriteTrailer();          // 完成
	}
	return 0;
}

VOID ClientViewCatch::StartStopCapture()
{
	if (bRunning) 
	{
		bRunning = FALSE;
		WaitForSingleObject(hThread, INFINITE); // 等待线程结束
		CloseHandle(hThread);
		hThread = nullptr;
		CloseHandle(hCaptureEvent);
		hCaptureEvent = nullptr;
	}
	else 
	{
		bRunning = TRUE;
		hCaptureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		hThread = CreateThread(NULL, 0, CaptureThread, this, 0, NULL);
	}
}