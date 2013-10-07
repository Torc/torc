// Qt
#include <QtEndian>

// Torc
#include "torcconfig.h"
#include "torccompat.h"
#include "audiooutpututil.h"

// Std
#include <math.h>

extern "C" {
#include "libavutil/avutil.h"
}

#if ARCH_X86 & HAVE_INLINE_ASM
static int has_sse2 = -1;

// Check cpuid for SSE2 support on x86 / x86_64
static inline bool sse_check()
{
    if (has_sse2 != -1)
        return (bool)has_sse2;
    __asm__(
        // -fPIC - we may not clobber ebx/rbx
#if ARCH_X86_64
        "push       %%rbx               \n\t"
#else
        "push       %%ebx               \n\t"
#endif
        "mov        $1, %%eax           \n\t"
        "cpuid                          \n\t"
        "and        $0x4000000, %%edx   \n\t"
        "shr        $26, %%edx          \n\t"
#if ARCH_X86_64
        "pop        %%rbx               \n\t"
#else
        "pop        %%ebx               \n\t"
#endif
        :"=d"(has_sse2)
        ::"%eax","%ecx"
    );
    return (bool)has_sse2;
}
#endif //ARCH_X86

#if !HAVE_LRINTF
static av_always_inline av_const long int lrintf(float x)
{
    return (int)(rint(x));
}
#endif

static inline float clipcheck(float f) {
    if (f > 1.0f) f = 1.0f;
    else if (f < -1.0f) f = -1.0f;
    return f;
}

/*
 All ToFloat variants require 16 byte aligned input and output buffers on x86
 The SSE code processes 16 bytes at a time and leaves any remainder for the C
 - there is no remainder in practice */

static int ToFloat8(float *Out, uchar *in, int Length)
{
    int i = 0;
    float f = 1.0f / ((1<<7) - 1);

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16)
    {
        int loops = Length >> 4;
        i = loops << 4;
        int a = 0x80808080;

        __asm__ volatile (
            "movd       %3, %%xmm0          \n\t"
            "movd       %4, %%xmm7          \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "1:                             \n\t"
            "movdqa     (%1), %%xmm1        \n\t"
            "xorpd      %%xmm2, %%xmm2      \n\t"
            "xorpd      %%xmm3, %%xmm3      \n\t"
            "psubb      %%xmm0, %%xmm1      \n\t"
            "xorpd      %%xmm4, %%xmm4      \n\t"
            "punpcklbw  %%xmm1, %%xmm2      \n\t"
            "xorpd      %%xmm5, %%xmm5      \n\t"
            "punpckhbw  %%xmm1, %%xmm3      \n\t"
            "punpcklwd  %%xmm2, %%xmm4      \n\t"
            "xorpd      %%xmm6, %%xmm6      \n\t"
            "punpckhwd  %%xmm2, %%xmm5      \n\t"
            "psrad      $24,    %%xmm4      \n\t"
            "punpcklwd  %%xmm3, %%xmm6      \n\t"
            "psrad      $24,    %%xmm5      \n\t"
            "punpckhwd  %%xmm3, %%xmm1      \n\t"
            "psrad      $24,    %%xmm6      \n\t"
            "cvtdq2ps   %%xmm4, %%xmm4      \n\t"
            "psrad      $24,    %%xmm1      \n\t"
            "cvtdq2ps   %%xmm5, %%xmm5      \n\t"
            "mulps      %%xmm7, %%xmm4      \n\t"
            "cvtdq2ps   %%xmm6, %%xmm6      \n\t"
            "mulps      %%xmm7, %%xmm5      \n\t"
            "movaps     %%xmm4, (%0)        \n\t"
            "cvtdq2ps   %%xmm1, %%xmm1      \n\t"
            "mulps      %%xmm7, %%xmm6      \n\t"
            "movaps     %%xmm5, 16(%0)      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "movaps     %%xmm6, 32(%0)      \n\t"
            "add        $16,    %1          \n\t"
            "movaps     %%xmm1, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out),"+r"(in)
            :"c"(loops), "r"(a), "r"(f)
        );
    }
#endif //ARCH_x86
    for (; i < Length; i++)
        *Out++ = (*in++ - 0x80) * f;
    return Length << 2;
}

/*
  The SSE code processes 16 bytes at a time and leaves any remainder for the C
  - there is no remainder in practice */

static int FromFloat8(uchar *Out, float *in, int Length)
{
    int i = 0;
    float f = (1<<7) - 1;

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16 && ((unsigned long)Out & 0xf) == 0)
    {
        int loops = Length >> 4;
        i = loops << 4;
        int a = 0x80808080;

        __asm__ volatile (
            "movd       %3, %%xmm0          \n\t"
            "movd       %4, %%xmm7          \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "1:                             \n\t"
            "movups     (%1), %%xmm1        \n\t"
            "movups     16(%1), %%xmm2      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "movups     32(%1), %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm2      \n\t"
            "cvtps2dq   %%xmm1, %%xmm1      \n\t"
            "movups     48(%1), %%xmm4      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "cvtps2dq   %%xmm2, %%xmm2      \n\t"
            "mulps      %%xmm7, %%xmm4      \n\t"
            "cvtps2dq   %%xmm3, %%xmm3      \n\t"
            "packssdw   %%xmm2, %%xmm1      \n\t"
            "cvtps2dq   %%xmm4, %%xmm4      \n\t"
            "packssdw   %%xmm4, %%xmm3      \n\t"
            "add        $64,    %1          \n\t"
            "packsswb   %%xmm3, %%xmm1      \n\t"
            "paddb      %%xmm0, %%xmm1      \n\t"
            "movdqu     %%xmm1, (%0)        \n\t"
            "add        $16,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out),"+r"(in)
            :"c"(loops), "r"(a), "r"(f)
        );
    }
#endif //ARCH_x86
    for (;i < Length; i++)
        *Out++ = lrintf(clipcheck(*in++) * f) + 0x80;
    return Length;
}

static int ToFloat16(float *Out, short *in, int Length)
{
    int i = 0;
    float f = 1.0f / ((1<<15) - 1);

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16)
    {
        int loops = Length >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movd       %3, %%xmm7          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "1:                             \n\t"
            "xorpd      %%xmm2, %%xmm2      \n\t"
            "movdqa     (%1),   %%xmm1      \n\t"
            "xorpd      %%xmm3, %%xmm3      \n\t"
            "punpcklwd  %%xmm1, %%xmm2      \n\t"
            "movdqa     16(%1), %%xmm4      \n\t"
            "punpckhwd  %%xmm1, %%xmm3      \n\t"
            "psrad      $16,    %%xmm2      \n\t"
            "punpcklwd  %%xmm4, %%xmm5      \n\t"
            "psrad      $16,    %%xmm3      \n\t"
            "cvtdq2ps   %%xmm2, %%xmm2      \n\t"
            "punpckhwd  %%xmm4, %%xmm6      \n\t"
            "psrad      $16,    %%xmm5      \n\t"
            "mulps      %%xmm7, %%xmm2      \n\t"
            "cvtdq2ps   %%xmm3, %%xmm3      \n\t"
            "psrad      $16,    %%xmm6      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "cvtdq2ps   %%xmm5, %%xmm5      \n\t"
            "movaps     %%xmm2, (%0)        \n\t"
            "cvtdq2ps   %%xmm6, %%xmm6      \n\t"
            "mulps      %%xmm7, %%xmm5      \n\t"
            "movaps     %%xmm3, 16(%0)      \n\t"
            "mulps      %%xmm7, %%xmm6      \n\t"
            "movaps     %%xmm5, 32(%0)      \n\t"
            "add        $32, %1             \n\t"
            "movaps     %%xmm6, 48(%0)      \n\t"
            "add        $64, %0             \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out),"+r"(in)
            :"c"(loops), "r"(f)
        );
    }
#endif //ARCH_x86
    for (; i < Length; i++)
        *Out++ = *in++ * f;
    return Length << 2;
}

static int FromFloat16(short *Out, float *in, int Length)
{
    int i = 0;
    float f = (1<<15) - 1;

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16 && ((unsigned long)Out & 0xf) == 0)
    {
        int loops = Length >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movd       %3, %%xmm7          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "1:                             \n\t"
            "movups     (%1), %%xmm1        \n\t"
            "movups     16(%1), %%xmm2      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "movups     32(%1), %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm2      \n\t"
            "cvtps2dq   %%xmm1, %%xmm1      \n\t"
            "movups     48(%1), %%xmm4      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "cvtps2dq   %%xmm2, %%xmm2      \n\t"
            "mulps      %%xmm7, %%xmm4      \n\t"
            "cvtps2dq   %%xmm3, %%xmm3      \n\t"
            "cvtps2dq   %%xmm4, %%xmm4      \n\t"
            "packssdw   %%xmm2, %%xmm1      \n\t"
            "packssdw   %%xmm4, %%xmm3      \n\t"
            "add        $64,    %1          \n\t"
            "movdqu     %%xmm1, (%0)        \n\t"
            "movdqu     %%xmm3, 16(%0)      \n\t"
            "add        $32,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out),"+r"(in)
            :"c"(loops), "r"(f)
        );
    }
#endif //ARCH_x86
    for (;i < Length;i++)
        *Out++ = lrintf(clipcheck(*in++) * f);
    return Length << 1;
}

static int ToFloat32(AudioFormat Format, float *Out, int *in, int Length)
{
    int i = 0;
    int bits = AudioOutputSettings::FormatToBits(Format);
    float f = 1.0f / ((uint)(1<<(bits-1)) - 128);
    int shift = 32 - bits;

    if (Format == FORMAT_S24LSB)
        shift = 0;

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16)
    {
        int loops = Length >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movd       %3, %%xmm7          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "movd       %4, %%xmm6          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "1:                             \n\t"
            "movdqa     (%1),   %%xmm1      \n\t"
            "movdqa     16(%1), %%xmm2      \n\t"
            "psrad      %%xmm6, %%xmm1      \n\t"
            "movdqa     32(%1), %%xmm3      \n\t"
            "cvtdq2ps   %%xmm1, %%xmm1      \n\t"
            "psrad      %%xmm6, %%xmm2      \n\t"
            "movdqa     48(%1), %%xmm4      \n\t"
            "cvtdq2ps   %%xmm2, %%xmm2      \n\t"
            "psrad      %%xmm6, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "psrad      %%xmm6, %%xmm4      \n\t"
            "cvtdq2ps   %%xmm3, %%xmm3      \n\t"
            "movaps     %%xmm1, (%0)        \n\t"
            "mulps      %%xmm7, %%xmm2      \n\t"
            "cvtdq2ps   %%xmm4, %%xmm4      \n\t"
            "movaps     %%xmm2, 16(%0)      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm4      \n\t"
            "movaps     %%xmm3, 32(%0)      \n\t"
            "add        $64,    %1          \n\t"
            "movaps     %%xmm4, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out),"+r"(in)
            :"c"(loops), "r"(f), "r"(shift)
        );
    }
#endif //ARCH_x86
    for (; i < Length; i++)
        *Out++ = (*in++ >> shift) * f;
    return Length << 2;
}

static int FromFloat32(AudioFormat Format, int *Out, float *in, int Length)
{
    int i = 0;
    int bits = AudioOutputSettings::FormatToBits(Format);
    float f = (uint)(1<<(bits-1)) - 128;
    int shift = 32 - bits;

    if (Format == FORMAT_S24LSB)
        shift = 0;

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16 && ((unsigned long)Out & 0xf) == 0)
    {
        float o = 1, mo = -1;
        int loops = Length >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movd       %3, %%xmm7          \n\t"
            "movss      %4, %%xmm5          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "movss      %5, %%xmm6          \n\t"
            "punpckldq  %%xmm5, %%xmm5      \n\t"
            "punpckldq  %%xmm6, %%xmm6      \n\t"
            "movd       %6, %%xmm0          \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "punpckldq  %%xmm5, %%xmm5      \n\t"
            "punpckldq  %%xmm6, %%xmm6      \n\t"
            "1:                             \n\t"
            "movups     (%1), %%xmm1        \n\t"
            "movups     16(%1), %%xmm2      \n\t"
            "minps      %%xmm5, %%xmm1      \n\t"
            "movups     32(%1), %%xmm3      \n\t"
            "maxps      %%xmm6, %%xmm1      \n\t"
            "movups     48(%1), %%xmm4      \n\t"
            "mulps      %%xmm7, %%xmm1      \n\t"
            "minps      %%xmm5, %%xmm2      \n\t"
            "cvtps2dq   %%xmm1, %%xmm1      \n\t"
            "maxps      %%xmm6, %%xmm2      \n\t"
            "pslld      %%xmm0, %%xmm1      \n\t"
            "minps      %%xmm5, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm2      \n\t"
            "movdqu     %%xmm1, (%0)        \n\t"
            "cvtps2dq   %%xmm2, %%xmm2      \n\t"
            "maxps      %%xmm6, %%xmm3      \n\t"
            "minps      %%xmm5, %%xmm4      \n\t"
            "pslld      %%xmm0, %%xmm2      \n\t"
            "mulps      %%xmm7, %%xmm3      \n\t"
            "maxps      %%xmm6, %%xmm4      \n\t"
            "movdqu     %%xmm2, 16(%0)      \n\t"
            "cvtps2dq   %%xmm3, %%xmm3      \n\t"
            "mulps      %%xmm7, %%xmm4      \n\t"
            "pslld      %%xmm0, %%xmm3      \n\t"
            "cvtps2dq   %%xmm4, %%xmm4      \n\t"
            "movdqu     %%xmm3, 32(%0)      \n\t"
            "pslld      %%xmm0, %%xmm4      \n\t"
            "add        $64,    %1          \n\t"
            "movdqu     %%xmm4, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out), "+r"(in)
            :"c"(loops), "r"(f), "m"(o), "m"(mo), "r"(shift)
        );
    }
#endif //ARCH_x86
    for (;i < Length;i++)
        *Out++ = lrintf(clipcheck(*in++) * f) << shift;
    return Length << 2;
}

static int FromFloatFLT(float *Out, float *in, int Length)
{
    int i = 0;

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && Length >= 16 && ((unsigned long)in & 0xf) == 0)
    {
        int loops = Length >> 4;
        float o = 1, mo = -1;
        i = loops << 4;

        __asm__ volatile (
            "movss      %3, %%xmm6          \n\t"
            "movss      %4, %%xmm7          \n\t"
            "punpckldq  %%xmm6, %%xmm6      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "punpckldq  %%xmm6, %%xmm6      \n\t"
            "punpckldq  %%xmm7, %%xmm7      \n\t"
            "1:                             \n\t"
            "movups     (%1), %%xmm1        \n\t"
            "movups     16(%1), %%xmm2      \n\t"
            "minps      %%xmm6, %%xmm1      \n\t"
            "movups     32(%1), %%xmm3      \n\t"
            "maxps      %%xmm7, %%xmm1      \n\t"
            "minps      %%xmm6, %%xmm2      \n\t"
            "movups     48(%1), %%xmm4      \n\t"
            "maxps      %%xmm7, %%xmm2      \n\t"
            "movups     %%xmm1, (%0)        \n\t"
            "minps      %%xmm6, %%xmm3      \n\t"
            "movups     %%xmm2, 16(%0)      \n\t"
            "maxps      %%xmm7, %%xmm3      \n\t"
            "minps      %%xmm6, %%xmm4      \n\t"
            "movups     %%xmm3, 32(%0)      \n\t"
            "maxps      %%xmm7, %%xmm4      \n\t"
            "add        $64,    %1          \n\t"
            "movups     %%xmm4, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(Out), "+r"(in)
            :"c"(loops), "m"(o), "m"(mo)
        );
    }
#endif //ARCH_x86
    for (;i < Length;i++)
        *Out++ = clipcheck(*in++);
    return Length << 2;
}

/**
 * Returns true if platform has an FPU.
 * Currently limited to testing if SSE2 is supported
 */
bool AudioOutputUtil::HasHardwareFPU(void)
{
#if ARCH_X86 & HAVE_INLINE_ASM
    return sse_check();
#else
    return false;
#endif
}

/**
 * Convert integer samples to floats
 *
 * Consumes 'bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioOutputUtil::ToFloat(AudioFormat Format, void *Out, void *In, int Bytes)
{
    if (Bytes <= 0)
        return 0;

    switch (Format)
    {
        case FORMAT_U8:
            return ToFloat8((float *)Out,  (uchar *)In, Bytes);
        case FORMAT_S16:
            return ToFloat16((float *)Out, (short *)In, Bytes >> 1);
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
            return ToFloat32(Format, (float *)Out, (int *)In, Bytes >> 2);
        case FORMAT_FLT:
            memcpy(Out, In, Bytes);
            return Bytes;
        default:
            break;
    }

    return 0;
}

/**
 * Convert float samples to integers
 *
 * Consumes 'Bytes' bytes from in and returns the numer of bytes written to out
 */
int AudioOutputUtil::FromFloat(AudioFormat Format, void *Out, void *In, int Bytes)
{
    if (Bytes <= 0)
        return 0;

    switch (Format)
    {
        case FORMAT_U8:
            return FromFloat8((uchar *)Out, (float *)In, Bytes >> 2);
        case FORMAT_S16:
            return FromFloat16((short *)Out, (float *)In, Bytes >> 2);
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
            return FromFloat32(Format, (int *)Out, (float *)In, Bytes >> 2);
        case FORMAT_FLT:
            return FromFloatFLT((float *)Out, (float *)In, Bytes >> 2);
        default:
            break;
    }

    return 0;
}

/**
 * Convert a mono stream to stereo by copying and interleaving samples
 */
void AudioOutputUtil::MonoToStereo(void *Dest, void *Source, int Samples)
{
    float *d = (float *)Dest;
    float *s = (float *)Source;
    for (int i = 0; i < Samples; i++)
    {
        *d++ = *s;
        *d++ = *s++;
    }
}

/**
 * Adjust the volume of samples
 *
 * Makes a crude attempt to normalise the relative volumes of
 * PCM from mythmusic, PCM from video and upmixed AC-3
 */
void AudioOutputUtil::AdjustVolume(void *buf, int Length, int Volume,
                                   bool Music, bool Upmix)
{
    float g     = Volume / 100.0f;
    float *fptr = (float *)buf;
    int samples = Length >> 2;
    int i       = 0;

    // Should be exponential - this'll do
    g *= g;

    // Try to ~ match stereo volume when upmixing
    if (Upmix)
        g *= 1.5f;

    // Music is relatively loud
    if (Music)
        g *= 0.4f;

    if (g == 1.0f)
        return;

#if ARCH_X86 & HAVE_INLINE_ASM
    if (sse_check() && samples >= 16)
    {
        int loops = samples >> 4;
        i = loops << 4;

        __asm__ volatile (
            "movss      %2, %%xmm0          \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "punpckldq  %%xmm0, %%xmm0      \n\t"
            "1:                             \n\t"
            "movups     (%0), %%xmm1        \n\t"
            "movups     16(%0), %%xmm2      \n\t"
            "mulps      %%xmm0, %%xmm1      \n\t"
            "movups     32(%0), %%xmm3      \n\t"
            "mulps      %%xmm0, %%xmm2      \n\t"
            "movups     48(%0), %%xmm4      \n\t"
            "mulps      %%xmm0, %%xmm3      \n\t"
            "movups     %%xmm1, (%0)        \n\t"
            "mulps      %%xmm0, %%xmm4      \n\t"
            "movups     %%xmm2, 16(%0)      \n\t"
            "movups     %%xmm3, 32(%0)      \n\t"
            "movups     %%xmm4, 48(%0)      \n\t"
            "add        $64,    %0          \n\t"
            "sub        $1, %%ecx           \n\t"
            "jnz        1b                  \n\t"
            :"+r"(fptr)
            :"c"(loops),"m"(g)
        );
    }
#endif //ARCH_X86
    for (; i < samples; i++)
        *fptr++ *= g;
}

template <class AudioDataType>
void _MuteChannel(AudioDataType *Buffer, int Channels, int Channel, int Frames)
{
    AudioDataType *s1 = Buffer + Channel;
    AudioDataType *s2 = Buffer - Channel + 1;

    for (int i = 0; i < Frames; i++)
    {
        *s1 = *s2;
        s1 += Channels;
        s2 += Channels;
    }
}

/**
 * Mute individual channels through mono->stereo duplication
 *
 * Mute given channel (left or right) by copying right or left
 * channel over.
 */
void AudioOutputUtil::MuteChannel(int Bits, int Channels, int ch,
                                  void *Buffer, int Bytes)
{
    int frames = Bytes / ((Bits >> 3) * Channels);

    if (Bits == 8)
        _MuteChannel((uchar *)Buffer, Channels, ch, frames);
    else if (Bits == 16)
        _MuteChannel((short *)Buffer, Channels, ch, frames);
    else
        _MuteChannel((int *)Buffer, Channels, ch, frames);
}

#define PINK_MAX_RANDOM_ROWS (30)
#define PINK_RANDOM_BITS     (24)
#define PINK_RANDOM_SHIFT    ((sizeof(long) * 8) - PINK_RANDOM_BITS)

typedef struct
{
    long  pink_rows[PINK_MAX_RANDOM_ROWS];
    long  pink_running_sum;
    int   pink_index;
    int   pink_index_mask;
    float pink_scalar;
} pink_noise_t;

static unsigned long GenerateRandomNumber(void)
{
    static unsigned long rand_seed = 22222;
    rand_seed = (rand_seed * 196314165) + 907633515;
    return rand_seed;
}

void InitialisePinkNoise(pink_noise_t *Pink, int NumRows)
{
    Pink->pink_index = 0;
    Pink->pink_index_mask = (1 << NumRows) - 1;
    long pmax = (NumRows + 1) * (1 << (PINK_RANDOM_BITS-1));
    Pink->pink_scalar = 1.0f / pmax;
    for (int i = 0; i < NumRows; i++)
        Pink->pink_rows[i] = 0;
    Pink->pink_running_sum = 0;
}

float GeneratePinkNoise(pink_noise_t *Pink)
{
    long new_random;
    Pink->pink_index = (Pink->pink_index + 1) & Pink->pink_index_mask;

    if (Pink->pink_index != 0 )
    {
        int num_zeros = 0;
        int n = Pink->pink_index;
        while( (n & 1) == 0 )
        {
            n = n >> 1;
            num_zeros++;
        }

        Pink->pink_running_sum -= Pink->pink_rows[num_zeros];
        new_random = ((long)GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
        Pink->pink_running_sum += new_random;
        Pink->pink_rows[num_zeros] = new_random;
    }

    new_random = ((long)GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
    long sum = Pink->pink_running_sum + new_random;

    return (float)(Pink->pink_scalar * sum);
}

char *AudioOutputUtil::GeneratePinkFrames(char *Frames, int Channels, int Channel, int Count, int Bits)
{
    pink_noise_t pink;

    InitialisePinkNoise(&pink, Bits);

    double   res;
    int32_t  ires;
    int16_t *samp16 = (int16_t*) Frames;
    int32_t *samp32 = (int32_t*) Frames;

    while (Count-- > 0)
    {
        for(int chn = 0 ; chn < Channels; chn++)
        {
            if (chn == Channel)
            {
                // Don't use max volume
                res = GeneratePinkNoise(&pink) * 0x03fffffff;
                ires = res;
                if (Bits == 16)
                    *samp16++ = qFromLittleEndian(ires >> 16);
                else
                    *samp32++ = qFromLittleEndian(ires);
            }
            else
            {
                if (Bits == 16)
                    *samp16++ = 0;
                else
                    *samp32++ = 0;
            }
        }
    }

    return Frames;
}
  
