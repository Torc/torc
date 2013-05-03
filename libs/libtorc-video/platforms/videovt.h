#ifndef VIDEOVT_H
#define VIDEOVT_H

// Torc
#include "videoframe.h"

// these should be the same format - hence requiring no conversion
#define VT_CV_FORMAT  kCVPixelFormatType_422YpCbCr8
#define VT_PIX_FORMAT AV_PIX_FMT_UYVY422

#include "CoreVideo/CVPixelBuffer.h"
extern "C" {
#include "libavcodec/vt.h"
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

class VideoVT
{
  public:
    static bool InitialiseDecoder (AVCodecContext *Context, AVPixelFormat Format);
};

#endif // VIDEOVT_H
