#include "Watch.h"

HostViewDisplay::HostViewDisplay()
{
	ioctx = nullptr;
	fmtctx = nullptr;
	c = nullptr;
	frame = nullptr;
	pkt = nullptr;
	sws_ctx = nullptr;
	lpMemVideoFile = (uint8_t*)VirtualAlloc(NULL, sizeof(BITMAPINFOHEADER), MEM_COMMIT, PAGE_READWRITE);
}

VOID HostViewDisplay::SetBmpInfo(BITMAPINFOHEADER bmpInfo)
{
	BmpInfo = bmpInfo;
}

HostViewDisplay::~HostViewDisplay()
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
	if (lpMemVideoFile)
		VirtualFree(lpMemVideoFile, 0, MEM_RELEASE);
}

BOOL HostViewDisplay::Initialize()
{
	targetframe = 30;	//默认播放30帧,暂时规定，后续提供接口修改
    framerate = 1.0 / targetframe;
	ViewWidth = 1280;
	ViewHeight = 720;
	//后续开放接口修改分辨率
    ViewData.resize(ViewWidth * ViewHeight * 3); // 24bpp -> 3 bytes per pixel
	if (!lpMemVideoFile) return FALSE; // 空检查

    // 每帧为 width * height * sizeof(Color_RGB) 字节（Color_RGB == 3 bytes），总大小乘以每秒帧数 targetframe
	bd.size = (size_t)BmpInfo.biWidth * BmpInfo.biHeight * targetframe * sizeof(Color_RGB);

    // 重新分配视频缓冲区前停止网络对旧缓冲区的写入（或等到 NetworkThread 完成）
    VirtualFree(lpMemVideoFile, 0, MEM_RELEASE);
    lpMemVideoFile = (uint8_t*)VirtualAlloc(NULL, bd.size, MEM_COMMIT, PAGE_READWRITE);
    if (!lpMemVideoFile) return FALSE;
    bd.buf = lpMemVideoFile;
    bd.pos = 0;

    // --- 关键：先创建 AVFormatContext 和 AVIOContext ---
    fmtctx = avformat_alloc_context();
    if (!fmtctx) return FALSE;

    unsigned char* avio_buffer = (unsigned char*)av_malloc(4096);
    if (!avio_buffer) return FALSE;

    // 第三个参数 write_flag 应为 0（只读）
    ioctx = avio_alloc_context(avio_buffer, 4096, 0, &bd, &read_packet, NULL, &seek);
    if (!ioctx) return FALSE;
    fmtctx->pb = ioctx;

    int ret = avformat_open_input(&fmtctx, NULL, NULL, NULL);
    if (ret < 0) {
        // 处理错误并释放资源
        return FALSE;
    }
	//查找解码器
	avformat_find_stream_info(fmtctx, NULL);
	for (int i = 0; i < fmtctx->nb_streams; i++)
	{
		const AVCodec* codec = avcodec_find_decoder(fmtctx->streams[i]->codecpar->codec_id);
		if (codec->type == AVMEDIA_TYPE_VIDEO)
		{
			VideoStreamIndex = i;
			c = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(c, fmtctx->streams[i]->codecpar);
			avcodec_open2(c, codec, NULL);
			break;
		}
	}

	sws_ctx = sws_getContext(c->width, c->height, c->pix_fmt, ViewWidth, ViewHeight,
		AV_PIX_FMT_BGR24, SWS_BILINEAR, NULL, NULL, NULL);
	SetEvent(g_PlayEndedEvent);
	return TRUE;
}

BOOL HostViewDisplay::OnScreenDisplay(HDC hdc)
{
	HDC hdcMem = CreateCompatibleDC(hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, ViewWidth, ViewHeight);

	BITMAPINFO bitinfo = { 0 };
	bitinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitinfo.bmiHeader.biWidth = ViewWidth;
	bitinfo.bmiHeader.biHeight = -ViewHeight;
	bitinfo.bmiHeader.biPlanes = 1;
	bitinfo.bmiHeader.biBitCount = 24;
	bitinfo.bmiHeader.biCompression = BI_RGB;

	SelectObject(hdcMem, hBitmap);
	SetDIBits(hdcMem, hBitmap, 0, ViewHeight, ViewData.data(), &bitinfo, DIB_RGB_COLORS);
	BitBlt(hdc, 0, 0, ViewWidth, ViewHeight, hdcMem, 0, 0, SRCCOPY);

	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	return TRUE;
}

int HostViewDisplay::read_packet(void* opaque, uint8_t* buf, int buf_size)
{
	bdata* bd = (bdata*)opaque;
	size_t remaining = bd->size - bd->pos;
	size_t to_read = min((size_t)buf_size, remaining);
	memcpy(buf, bd->buf + bd->pos, to_read);
	bd->pos += to_read;
	return to_read;
}

int64_t HostViewDisplay::seek(void* opaque, int64_t offset, int whence)
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

double HostViewDisplay::GetFrameRate()
{
	return framerate;
}

BOOL HostViewDisplay::RequestFrame()
{
    if (!fmtctx || !c) return FALSE;
	while (TRUE)
	{
		AVPacket* localPkt = av_packet_alloc();
		if (!localPkt) return FALSE;
		int ret = av_read_frame(fmtctx, localPkt);
		if (ret == 0 && localPkt->stream_index == VideoStreamIndex)
			ret = avcodec_send_packet(c, localPkt);
		else if (ret == AVERROR_EOF)
			ret = avcodec_send_packet(c, NULL);

		if (ret != 0)
		{
			av_packet_unref(localPkt);
			av_packet_free(&localPkt);
			continue;
		}

		if (frame)
		{
			av_frame_free(&frame);
			frame = nullptr;
		}
		frame = av_frame_alloc();
		if (!frame)
		{
			av_packet_unref(localPkt);
			av_packet_free(&localPkt);
			return FALSE;
		}

		ret = avcodec_receive_frame(c, frame);

		av_packet_unref(localPkt);
		av_packet_free(&localPkt);

		if (ret == 0)
		{
			return TRUE;
		}
		else if (ret == AVERROR_EOF)
		{
			SetEvent(g_PlayEndedEvent); // 触发播放结束事件
			return FALSE;
		}

		av_frame_unref(frame);
		av_frame_free(&frame);
		frame = nullptr;
	}
}

BOOL HostViewDisplay::GetFrameData()
{
    if (!sws_ctx || !frame) return FALSE;
	uint8_t* data[AV_NUM_DATA_POINTERS] = { ViewData.data() };
	int linesize[AV_NUM_DATA_POINTERS] = { 0 };
	av_image_fill_linesizes(linesize, AV_PIX_FMT_BGR24, ViewWidth);
	sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, data, linesize);
	return TRUE;
}