#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <vector>
#include "common_lib.h"
#include "MachineVisionLib.h"
#include "MathAlgorithmLib.h"
#include "ImagePreprocess.h"
#include "MVLVideo.h"

//* 关于VLC参数的详细说明参见：https://blog.csdn.net/jxbinwd/article/details/72722833
static CHAR* const l_pbaVLCBaseArgs[] =	//* const确保o_pbaVLCArg本身的值也就是地址不会被修改	
{
	"-I"
	, "dummy"
	"--ignore-config" 	
	//, "--rtsp-frame-buffer-size=1500000"	//* 如果不设置这个，vlc会自动寻找一个合适的，建议不设置，只不过启动时会慢一些，如果视频的分辨率过大的话
	//, "--longhelp"						//* 这两个参数可以打开VLC的参数说明，不过内容很多，最好在控制台将其重定向到TXT文件再看
	//, "--advanced"
};

//* VLC回调函数之锁定帧数据缓冲区函数，参数pvCBInputParam是传给回调函数的用户自定义参数
//* pvCBInputParam指定接收帧数据的缓冲区的首地址
void *VLCVideoPlayer::FCBLock(void *pvCBInputParam, void **ppvFrameBuf)
{	
	VLCVideoPlayer *pobjVideoPlayer = (VLCVideoPlayer *)pvCBInputParam;
	//*ppvFrameBuf = pobjVideoPlayer->o_mVideoFrameRGB.data;
	*ppvFrameBuf = pobjVideoPlayer->o_pubaVideoFrameData;		

	return *ppvFrameBuf;
}

//* VLC回调函数之解锁帧数据缓冲区函数，其实pvFrameData和ppvFrameBuf指向的是同一块内存，其值由FCBLock()函数返回
void VLCVideoPlayer::FCBUnlock(void *pvCBInputParam, void *pvFrameData, void* const *ppvFrameBuf)
{
	VLCVideoPlayer *pobjVideoPlayer = (VLCVideoPlayer *)pvCBInputParam;
	Mat mVideoFrameRGB = Mat(pobjVideoPlayer->o_unAdjustedHeight, pobjVideoPlayer->o_unAdjustedWidth, CV_8UC3, pobjVideoPlayer->o_pubaVideoFrameData);
	cvtColor(mVideoFrameRGB, pobjVideoPlayer->o_mVideoFrame, CV_RGB2BGR);

	pobjVideoPlayer->o_unNextFrameIdx++;	
}

//* VLC回调函数之显示函数
void VLCVideoPlayer::FCBDisplay(void *pvCBInputParam, void *pvFrameData)
{
	VLCVideoPlayer *pobjVideoPlayer = (VLCVideoPlayer *)pvCBInputParam;

	if (pobjVideoPlayer->o_pfcbDispPreprocessor)
	{		
		pobjVideoPlayer->o_pfcbDispPreprocessor(pobjVideoPlayer->o_mVideoFrame, pobjVideoPlayer->o_pvFunCBDispPreprocParam, pobjVideoPlayer->o_unNextFrameIdx);
		imshow(pobjVideoPlayer->o_strDisplayWinName, pobjVideoPlayer->o_mVideoFrame);
	}	
}

Mat VLCVideoPlayer::GetNextFrame(void)
{
	//* 看看下一帧数据是否已经到达
	if (o_unPrevFrameIdx != o_unNextFrameIdx)
	{				
		o_unPrevFrameIdx = o_unNextFrameIdx;
		
		return o_mVideoFrame;		
	}
	else
	{
		Mat mDummy;
		return mDummy;
	}
}

//* 打开一个视频文件
void VLCVideoPlayer::OpenVideoFromFile(const CHAR *pszFile, PFCB_DISPLAY_PREPROCESSOR pfcbDispPreprocessor,
									const CHAR *pszDisplayWinName, ENUM_ASPECT_RATIO enumAspectRatio)
{
	UINT unArgc = sizeof(l_pbaVLCBaseArgs) / sizeof(l_pbaVLCBaseArgs[0]);
	CHAR **ppbaVLCArgs = new CHAR *[unArgc];
	for (UINT i = 0; i < unArgc; i++)	//* 对于播放视频文件，使用缺省参数即可
	{
		ppbaVLCArgs[i] = l_pbaVLCBaseArgs[i];
	}
	
	//* 初始化视频帧Matrix
	if (!o_unAdjustedWidth)	
		o_unAdjustedWidth = DEFAULT_VIDEO_FRAME_WIDTH;
	if (enumAspectRatio == AR_16_9)	
		o_unAdjustedHeight = (o_unAdjustedWidth / 16) * 9;
	else
		o_unAdjustedHeight = (o_unAdjustedWidth / 4) * 3;
	o_unFrameDataBufSize = o_unAdjustedWidth * o_unAdjustedHeight * 3;	
	o_pubaVideoFrameData = new UCHAR[o_unFrameDataBufSize];	

	o_pstVLCInstance = libvlc_new(unArgc, ppbaVLCArgs);	
	delete[] ppbaVLCArgs;	//* 删除刚才申请的参数缓冲区，libvlc_new()调用后就不用了

	libvlc_media_t* pstVLCMedia = libvlc_media_new_path(o_pstVLCInstance, pszFile);
	o_pstVLCMediaPlayer = libvlc_media_player_new_from_media(pstVLCMedia);
	libvlc_media_release(pstVLCMedia);	//* libvlc_media_player_new_from_media()调用完毕后，释放就可以了

	libvlc_video_set_callbacks(o_pstVLCMediaPlayer, FCBLock, FCBUnlock, FCBDisplay, this);								//* 设定回调函数
	libvlc_video_set_format(o_pstVLCMediaPlayer, "RV24", o_unAdjustedWidth, o_unAdjustedHeight, o_unAdjustedWidth * 3);	//* 设定播放格式	

	if (pfcbDispPreprocessor)
	{
		o_strDisplayWinName = pszDisplayWinName;
		o_pfcbDispPreprocessor = pfcbDispPreprocessor;
	}
}

//* 打开一个网络RTSP串流
void VLCVideoPlayer::OpenVideoFromeRtsp(const CHAR *pszURL, PFCB_DISPLAY_PREPROCESSOR pfcbDispPreprocessor,
									const CHAR *pszDisplayWinName, UINT unNetCachingTime, 
									BOOL blIsUsedTCP, ENUM_ASPECT_RATIO enumAspectRatio)
{
	UINT unBaseArgc = sizeof(l_pbaVLCBaseArgs) / sizeof(l_pbaVLCBaseArgs[0]);
	UINT unArgc;
	CHAR szNetCachingTime[30];
	CHAR **ppbaVLCArgs = new CHAR *[unBaseArgc + 2];

	sprintf_s(szNetCachingTime, "--network-caching=%d", unNetCachingTime);
	ppbaVLCArgs[unBaseArgc] = szNetCachingTime;
	if (blIsUsedTCP)
	{
		ppbaVLCArgs[unBaseArgc + 1] = "--rtsp-tcp";
		unArgc = unBaseArgc + 2;
	}
	else
		unArgc = unBaseArgc + 1;

	for (UINT i = 0; i < unBaseArgc; i++)	//* 对于播放视频文件，使用缺省参数即可
	{
		ppbaVLCArgs[i] = l_pbaVLCBaseArgs[i];
	}


	//* 初始化视频帧Matrix
	if (!o_unAdjustedWidth)
		o_unAdjustedWidth = DEFAULT_VIDEO_FRAME_WIDTH;
	if (enumAspectRatio == AR_16_9)
		o_unAdjustedHeight = (o_unAdjustedWidth / 16) * 9;
	else
		o_unAdjustedHeight = (o_unAdjustedWidth / 4) * 3;
	o_unFrameDataBufSize = o_unAdjustedWidth * o_unAdjustedHeight * 3;	
	o_pubaVideoFrameData = new UCHAR[o_unFrameDataBufSize];

	//cout << o_unAdjustedWidth << " * " << o_unAdjustedHeight << endl;
	
	o_pstVLCInstance = libvlc_new(unArgc, ppbaVLCArgs);
	delete[] ppbaVLCArgs;	//* 删除刚才申请的参数缓冲区，libvlc_new()调用后就不用了

	libvlc_media_t* pstVLCMedia = libvlc_media_new_location(o_pstVLCInstance, pszURL);
	o_pstVLCMediaPlayer = libvlc_media_player_new_from_media(pstVLCMedia);
	libvlc_media_release(pstVLCMedia);	//* libvlc_media_player_new_from_media()调用完毕后，释放就可以了

	libvlc_video_set_callbacks(o_pstVLCMediaPlayer, FCBLock, FCBUnlock, FCBDisplay, this);								//* 设定回调函数
	libvlc_video_set_format(o_pstVLCMediaPlayer, "RV24", o_unAdjustedWidth, o_unAdjustedHeight, o_unAdjustedWidth * 3);	//* 设定播放格式

	if (pfcbDispPreprocessor)
	{
		o_strDisplayWinName = pszDisplayWinName;
		o_pfcbDispPreprocessor = pfcbDispPreprocessor;
	}	
}

//* 设置显示预处理函数的输入参数
void VLCVideoPlayer::SetDispPreprocessorInputParam(void *pvParam)
{
	o_pvFunCBDispPreprocParam = pvParam;
}

//* 暂停/恢复当前视频/网络摄像机的播放
void VLCVideoPlayer::pause(BOOL blIsPaused)
{
	libvlc_media_player_set_pause(o_pstVLCMediaPlayer, blIsPaused);
}

//* 停止播放当前视频/网络摄像
void VLCVideoPlayer::stop(void)
{
	libvlc_media_player_stop(o_pstVLCMediaPlayer);

	//* 等待结束
	libvlc_state_t enumVLCPlayState;
	while (TRUE)
	{
		enumVLCPlayState = libvlc_media_player_get_state(o_pstVLCMediaPlayer);
		if (enumVLCPlayState == libvlc_Stopped || enumVLCPlayState == libvlc_Ended || enumVLCPlayState == libvlc_Error)
			break;
	}
}

BOOL VLCVideoPlayer::start(void)
{
	if (libvlc_media_player_play(o_pstVLCMediaPlayer) < 0)
	{
		cout << "libvlc_media_player_play()函数执行失败，请确认视频文件或RTSP流媒体地址正确！" << endl;

		return FALSE;
	}		

	return TRUE;
}

//* 视频文件是否已经播放完毕，这个函数对网络摄像机无效
BOOL VLCVideoPlayer::IsPlayEnd(void)
{
	libvlc_state_t enumVLCPlayState = libvlc_media_player_get_state(o_pstVLCMediaPlayer);	
	if (enumVLCPlayState == libvlc_Ended || enumVLCPlayState == libvlc_Stopped)
		return TRUE;

	return FALSE;
}

//* 释放申请的相关资源
VLCVideoPlayer::~VLCVideoPlayer() {
	if (libvlc_media_player_get_state(o_pstVLCMediaPlayer) != libvlc_Stopped)
		libvlc_media_player_stop(o_pstVLCMediaPlayer);

	libvlc_media_player_release(o_pstVLCMediaPlayer);
	libvlc_release(o_pstVLCInstance);

	//* 注意这个顺序，libvlc库在stop之前一直在使用它们，因此至少是stop后才能释放	
	o_mVideoFrame.release();
	delete[] o_pubaVideoFrameData;

	cout << "VLC player has successfully quit!" << endl;
};
