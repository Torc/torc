#ifndef VIDEOVDA_H
#define VIDEOVDA_H

// Torc
#include "videoframe.h"

#define VDA_CV_FORMAT  kCVPixelFormatType_422YpCbCr8
#define VDA_PIX_FORMAT AV_PIX_FMT_UYVY422
#include "CoreVideo/CVPixelBuffer.h"
extern "C" {
#include "libavcodec/vda.h"
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

class VideoVDA
{
  public:
    static bool CanAccelerate (AVCodecContext *Context, AVPixelFormat Format);
};

#endif // VIDEOVDA_H
