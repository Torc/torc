#ifndef VIDEOVDA_H
#define VIDEOVDA_H

#define VDA_CV_FORMAT  kCVPixelFormatType_422YpCbCr8
#define VDA_PIX_FORMAT PIX_FMT_UYVY422
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
    static void  ReleaseBuffer    (AVCodecContext *Context, AVFrame *Frame);
    static bool  AgreePixelFormat (AVCodecContext *Context, PixelFormat Format);
    static void  GetFrame         (AVFrame &Avframe, VideoFrame *Frame, SwsContext *ConversionContext);
    static void  Cleanup          (AVStream *Stream);
};

#endif // VIDEOVDA_H
