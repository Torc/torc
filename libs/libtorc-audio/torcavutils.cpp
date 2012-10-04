/* TorcAVUtils
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QtCore>

// Torc
#include "torcavutils.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

QString AVCodecToString(int Codec)
{
    switch (Codec)
    {
        case AV_CODEC_ID_NONE:              return QString("None!");
        case AV_CODEC_ID_MPEG1VIDEO:        return QString("MPEG1VIDEO");
        case AV_CODEC_ID_MPEG2VIDEO:        return QString("MPEG2VIDEO");
        case AV_CODEC_ID_MPEG2VIDEO_XVMC:   return QString("MPEG2VIDEO_XVMC");
        case AV_CODEC_ID_H261:              return QString("H261");
        case AV_CODEC_ID_H263:              return QString("H263");
        case AV_CODEC_ID_RV10:              return QString("RV10");
        case AV_CODEC_ID_RV20:              return QString("RV20)");
        case AV_CODEC_ID_MJPEG:             return QString("MJPEG");
        case AV_CODEC_ID_MJPEGB:            return QString("MJPEGB");
        case AV_CODEC_ID_LJPEG:             return QString("LJPEG");
        case AV_CODEC_ID_SP5X:              return QString("SP5X");
        case AV_CODEC_ID_JPEGLS:            return QString("JPEGLS");
        case AV_CODEC_ID_MPEG4:             return QString("MPEG4");
        case AV_CODEC_ID_RAWVIDEO:          return QString("RAWVIDEO");
        case AV_CODEC_ID_MSMPEG4V1:         return QString("MSMPEGV41");
        case AV_CODEC_ID_MSMPEG4V2:         return QString("MSMPEGV42");
        case AV_CODEC_ID_MSMPEG4V3:         return QString("MSMPEGV43");
        case AV_CODEC_ID_WMV1:              return QString("WMV1");
        case AV_CODEC_ID_WMV2:              return QString("WMV2");
        case AV_CODEC_ID_H263P:             return QString("H263P");
        case AV_CODEC_ID_H263I:             return QString("H263I");
        case AV_CODEC_ID_FLV1:              return QString("FLV1");
        case AV_CODEC_ID_SVQ1:              return QString("SVQ1");
        case AV_CODEC_ID_SVQ3:              return QString("SVQ3");
        case AV_CODEC_ID_DVVIDEO:           return QString("DVVIDEO");
        case AV_CODEC_ID_HUFFYUV:           return QString("HUFFYYUV");
        case AV_CODEC_ID_CYUV:              return QString("CYUV");
        case AV_CODEC_ID_H264:              return QString("H264");
        case AV_CODEC_ID_INDEO3:            return QString("INDEO3");
        case AV_CODEC_ID_VP3:               return QString("VP3");
        case AV_CODEC_ID_THEORA:            return QString("THEORA");
        case AV_CODEC_ID_ASV1:              return QString("ASV1");
        case AV_CODEC_ID_ASV2:              return QString("ASV2");
        case AV_CODEC_ID_FFV1:              return QString("FFV1");
        case AV_CODEC_ID_4XM:               return QString("4XM");
        case AV_CODEC_ID_VCR1:              return QString("VCR1");
        case AV_CODEC_ID_CLJR:              return QString("CLJR");
        case AV_CODEC_ID_MDEC:              return QString("MDEC");
        case AV_CODEC_ID_ROQ:               return QString("ROQ");
        case AV_CODEC_ID_INTERPLAY_VIDEO:   return QString("INTERPLAY_VIDEO");
        case AV_CODEC_ID_XAN_WC3:           return QString("XAN_WC3");
        case AV_CODEC_ID_XAN_WC4:           return QString("XAN_WC4");
        case AV_CODEC_ID_RPZA:              return QString("RPZA");
        case AV_CODEC_ID_CINEPAK:           return QString("CINEPAK");
        case AV_CODEC_ID_WS_VQA:            return QString("WS_VQA");
        case AV_CODEC_ID_MSRLE:             return QString("MSRLE");
        case AV_CODEC_ID_MSVIDEO1:          return QString("MSVIDEO1");
        case AV_CODEC_ID_IDCIN:             return QString("IDCIN");
        case AV_CODEC_ID_8BPS:              return QString("8BPS");
        case AV_CODEC_ID_SMC:               return QString("SMC");
        case AV_CODEC_ID_FLIC:              return QString("FLIC");
        case AV_CODEC_ID_TRUEMOTION1:       return QString("TRUEMOTION1");
        case AV_CODEC_ID_VMDVIDEO:          return QString("VMDVIDEO");
        case AV_CODEC_ID_MSZH:              return QString("MSZH");
        case AV_CODEC_ID_ZLIB:              return QString("ZLIB");
        case AV_CODEC_ID_QTRLE:             return QString("QTRLE");
        case AV_CODEC_ID_SNOW:              return QString("SNOW");
        case AV_CODEC_ID_TSCC:              return QString("TSCC");
        case AV_CODEC_ID_ULTI:              return QString("ULTI");
        case AV_CODEC_ID_QDRAW:             return QString("QDRAW");
        case AV_CODEC_ID_VIXL:              return QString("VIXL");
        case AV_CODEC_ID_QPEG:              return QString("QPEG");
        case AV_CODEC_ID_PNG:               return QString("PNG");
        case AV_CODEC_ID_PPM:               return QString("PPM");
        case AV_CODEC_ID_PBM:               return QString("PBM");
        case AV_CODEC_ID_PGM:               return QString("PGM");
        case AV_CODEC_ID_PGMYUV:            return QString("PGMYUV");
        case AV_CODEC_ID_PAM:               return QString("PAM");
        case AV_CODEC_ID_FFVHUFF:           return QString("FFVHUFF");
        case AV_CODEC_ID_RV30:              return QString("RV30");
        case AV_CODEC_ID_RV40:              return QString("RV40");
        case AV_CODEC_ID_VC1:               return QString("VC1");
        case AV_CODEC_ID_WMV3:              return QString("WMV3");
        case AV_CODEC_ID_LOCO:              return QString("LOCO");
        case AV_CODEC_ID_WNV1:              return QString("WNV1");
        case AV_CODEC_ID_AASC:              return QString("AASC");
        case AV_CODEC_ID_INDEO2:            return QString("INDEO2");
        case AV_CODEC_ID_FRAPS:             return QString("FRAPS");
        case AV_CODEC_ID_TRUEMOTION2:       return QString("TRUEMOTION2");
        case AV_CODEC_ID_BMP:               return QString("BMP");
        case AV_CODEC_ID_CSCD:              return QString("CSCD");
        case AV_CODEC_ID_MMVIDEO:           return QString("MMVIDEO");
        case AV_CODEC_ID_ZMBV:              return QString("ZMBV");
        case AV_CODEC_ID_AVS:               return QString("AVS");
        case AV_CODEC_ID_SMACKVIDEO:        return QString("SMACKVIDEO");
        case AV_CODEC_ID_NUV:               return QString("NUV");
        case AV_CODEC_ID_KMVC:              return QString("KMVC");
        case AV_CODEC_ID_FLASHSV:           return QString("FLASHSV");
        case AV_CODEC_ID_CAVS:              return QString("CAVS");
        case AV_CODEC_ID_JPEG2000:          return QString("JPEG2000");
        case AV_CODEC_ID_VMNC:              return QString("VMNC");
        case AV_CODEC_ID_VP5:               return QString("VP5");
        case AV_CODEC_ID_VP6:               return QString("VP6");
        case AV_CODEC_ID_VP6F:              return QString("VP6F");
        case AV_CODEC_ID_TARGA:             return QString("TARGA");
        case AV_CODEC_ID_DSICINVIDEO:       return QString("DSICINVIDEO");
        case AV_CODEC_ID_TIERTEXSEQVIDEO:   return QString("TIERTEXSEQVIDEO");
        case AV_CODEC_ID_TIFF:              return QString("TIFF");
        case AV_CODEC_ID_GIF:               return QString("GIF");
        case AV_CODEC_ID_DXA:               return QString("DXA");
        case AV_CODEC_ID_DNXHD:             return QString("DNXHD");
        case AV_CODEC_ID_THP:               return QString("THP");
        case AV_CODEC_ID_SGI:               return QString("SGI");
        case AV_CODEC_ID_C93:               return QString("C93");
        case AV_CODEC_ID_BETHSOFTVID:       return QString("BETHSOFTVID");
        case AV_CODEC_ID_PTX:               return QString("PTX");
        case AV_CODEC_ID_TXD:               return QString("TXD");
        case AV_CODEC_ID_VP6A:              return QString("VP6A");
        case AV_CODEC_ID_AMV:               return QString("AMV");
        case AV_CODEC_ID_VB:                return QString("VB");
        case AV_CODEC_ID_PCX:               return QString("PCX");
        case AV_CODEC_ID_SUNRAST:           return QString("SUNRAST");
        case AV_CODEC_ID_INDEO4:            return QString("INDEO4");
        case AV_CODEC_ID_INDEO5:            return QString("INDEO5");
        case AV_CODEC_ID_MIMIC:             return QString("MIMIC");
        case AV_CODEC_ID_RL2:               return QString("RL2");
        case AV_CODEC_ID_ESCAPE124:         return QString("ESCAPE124");
        case AV_CODEC_ID_DIRAC:             return QString("DIRAC");
        case AV_CODEC_ID_BFI:               return QString("BFI");
        case AV_CODEC_ID_CMV:               return QString("CMV");
        case AV_CODEC_ID_MOTIONPIXELS:      return QString("MOTIONPIXELS");
        case AV_CODEC_ID_TGV:               return QString("TGV");
        case AV_CODEC_ID_TGQ:               return QString("TGQ");
        case AV_CODEC_ID_TQI:               return QString("TQI");
        case AV_CODEC_ID_AURA:              return QString("AURA");
        case AV_CODEC_ID_AURA2:             return QString("AURA2");
        case AV_CODEC_ID_V210X:             return QString("V210X");
        case AV_CODEC_ID_TMV:               return QString("TMV");
        case AV_CODEC_ID_V210:              return QString("V210");
        case AV_CODEC_ID_DPX:               return QString("DPX");
        case AV_CODEC_ID_MAD:               return QString("MAD");
        case AV_CODEC_ID_FRWU:              return QString("FRWU");
        case AV_CODEC_ID_FLASHSV2:          return QString("FLASHSV2");
        case AV_CODEC_ID_CDGRAPHICS:        return QString("CDGRAPHICS");
        case AV_CODEC_ID_R210:              return QString("R210");
        case AV_CODEC_ID_ANM:               return QString("ANM");
        case AV_CODEC_ID_BINKVIDEO:         return QString("BINKVIDEO");
        case AV_CODEC_ID_IFF_ILBM:          return QString("IFF_ILBM");
        case AV_CODEC_ID_IFF_BYTERUN1:      return QString("IFF_BYTERUN1");
        case AV_CODEC_ID_KGV1:              return QString("KGV1");
        case AV_CODEC_ID_YOP:               return QString("YOP");
        case AV_CODEC_ID_VP8:               return QString("VP8");
        case AV_CODEC_ID_PICTOR:            return QString("PICTOR");
        case AV_CODEC_ID_ANSI:              return QString("ANSI");
        case AV_CODEC_ID_A64_MULTI:         return QString("A64_MULTI");
        case AV_CODEC_ID_A64_MULTI5:        return QString("A64_MULTI5");
        case AV_CODEC_ID_R10K:              return QString("R10K");
        case AV_CODEC_ID_MXPEG:             return QString("MXPEG");
        case AV_CODEC_ID_LAGARITH:          return QString("LAGARITH");
        case AV_CODEC_ID_PRORES:            return QString("PRORES");
        case AV_CODEC_ID_JV:                return QString("JV");
        case AV_CODEC_ID_DFA:               return QString("DFA");
        case AV_CODEC_ID_WMV3IMAGE:         return QString("WMV3IMAGE");
        case AV_CODEC_ID_VC1IMAGE:          return QString("VC1IMAGE");
        case AV_CODEC_ID_UTVIDEO:           return QString("UTVIDEO");
        case AV_CODEC_ID_BMV_VIDEO:         return QString("BMV_VIDEO");
        case AV_CODEC_ID_VBLE:              return QString("VBLE");
        case AV_CODEC_ID_DXTORY:            return QString("DXTORY");
        case AV_CODEC_ID_V410:              return QString("V410");
        case AV_CODEC_ID_XWD:               return QString("XWD");
        case AV_CODEC_ID_CDXL:              return QString("CDXL");
        case AV_CODEC_ID_XBM:               return QString("XBM");
        case AV_CODEC_ID_ZEROCODEC:         return QString("ZEROCODEC");
        case AV_CODEC_ID_MSS1:              return QString("MSS1");
        case AV_CODEC_ID_MSA1:              return QString("MSA1");
        case AV_CODEC_ID_TSCC2:             return QString("TSCC2");
        case AV_CODEC_ID_MTS2:              return QString("MTS2");
        case AV_CODEC_ID_CLLC:              return QString("CLLC");
        case AV_CODEC_ID_MSS2:              return QString("MSS2");

        case AV_CODEC_ID_PCM_S16LE:         return QString("PCM_S16LE");
        case AV_CODEC_ID_PCM_S16BE:         return QString("PCM_S16BE");
        case AV_CODEC_ID_PCM_U16LE:         return QString("PCM_U16LE");
        case AV_CODEC_ID_PCM_U16BE:         return QString("PCM_U16BE");
        case AV_CODEC_ID_PCM_S8:            return QString("PCM_S8");
        case AV_CODEC_ID_PCM_U8:            return QString("PCM_U8");
        case AV_CODEC_ID_PCM_MULAW:         return QString("PCM_MULAW");
        case AV_CODEC_ID_PCM_ALAW:          return QString("PCM_ALAW");
        case AV_CODEC_ID_PCM_S32LE:         return QString("PCM_S32LE");
        case AV_CODEC_ID_PCM_S32BE:         return QString("PCM_S32BE");
        case AV_CODEC_ID_PCM_U32LE:         return QString("PCM_U32LE");
        case AV_CODEC_ID_PCM_U32BE:         return QString("PCM_U32BE");
        case AV_CODEC_ID_PCM_S24LE:         return QString("PCM_S24LE");
        case AV_CODEC_ID_PCM_S24BE:         return QString("PCM_S24BE");
        case AV_CODEC_ID_PCM_U24LE:         return QString("PCM_U24LE");
        case AV_CODEC_ID_PCM_U24BE:         return QString("PCM_U24BE");
        case AV_CODEC_ID_PCM_S24DAUD:       return QString("PCM_S24DAUD");
        case AV_CODEC_ID_PCM_ZORK:          return QString("PCM_ZORK");
        case AV_CODEC_ID_PCM_S16LE_PLANAR:  return QString("PCM_S16LE_PLANAR");
        case AV_CODEC_ID_PCM_DVD:           return QString("PCM_DVD");
        case AV_CODEC_ID_PCM_F32BE:         return QString("PCM_F32BE");
        case AV_CODEC_ID_PCM_F32LE:         return QString("PCM_F32LE");
        case AV_CODEC_ID_PCM_F64BE:         return QString("PCM_F64BE");
        case AV_CODEC_ID_PCM_F64LE:         return QString("PCM_F64LE");
        case AV_CODEC_ID_PCM_BLURAY:        return QString("PCM_BLURAY");
        case AV_CODEC_ID_PCM_LXF:           return QString("PCM_LXF");
        case AV_CODEC_ID_S302M:             return QString("S302M");
        case AV_CODEC_ID_PCM_S8_PLANAR:     return QString("PCM_S8_PLANAR");

        case AV_CODEC_ID_ADPCM_IMA_QT:      return QString("ADPCM_IMA_QT");
        case AV_CODEC_ID_ADPCM_IMA_WAV:     return QString("ADPCM_IMA_WAV");
        case AV_CODEC_ID_ADPCM_IMA_DK3:     return QString("ADPCM_IMA_DK3");
        case AV_CODEC_ID_ADPCM_IMA_DK4:     return QString("ADPCM_IMA_DK4");
        case AV_CODEC_ID_ADPCM_IMA_WS:      return QString("ADPCM_IMA_WS");
        case AV_CODEC_ID_ADPCM_IMA_SMJPEG:  return QString("ADPCM_IMA_SMJPEG");
        case AV_CODEC_ID_ADPCM_MS:          return QString("ADPCM_MS");
        case AV_CODEC_ID_ADPCM_4XM:         return QString("ADPCM_4XM");
        case AV_CODEC_ID_ADPCM_XA:          return QString("ADPCM_XA");
        case AV_CODEC_ID_ADPCM_ADX:         return QString("ADPCM_ADX");
        case AV_CODEC_ID_ADPCM_EA:          return QString("ADPCM_EA");
        case AV_CODEC_ID_ADPCM_G726:        return QString("ADPCM_G726");
        case AV_CODEC_ID_ADPCM_CT:          return QString("ADPCM_CT");
        case AV_CODEC_ID_ADPCM_SWF:         return QString("ADPCM_SWF");
        case AV_CODEC_ID_ADPCM_YAMAHA:      return QString("ADPCM_YAMAHA");
        case AV_CODEC_ID_ADPCM_SBPRO_4:     return QString("ADPCM_SBPRO_4");
        case AV_CODEC_ID_ADPCM_SBPRO_3:     return QString("ADPCM_SBPRO_3");
        case AV_CODEC_ID_ADPCM_SBPRO_2:     return QString("ADPCM_SBPRO_2");
        case AV_CODEC_ID_ADPCM_THP:         return QString("ADPCM_THP");
        case AV_CODEC_ID_ADPCM_IMA_AMV:     return QString("ADPCM_IMA_AMV");
        case AV_CODEC_ID_ADPCM_EA_R1:       return QString("ADPCM_EA_R1");
        case AV_CODEC_ID_ADPCM_EA_R3:       return QString("ADPCM_EA_R3");
        case AV_CODEC_ID_ADPCM_EA_R2:       return QString("ADPCM_EA_R2");
        case AV_CODEC_ID_ADPCM_IMA_EA_SEAD: return QString("ADPCM_IMA_EA_SEAD");
        case AV_CODEC_ID_ADPCM_IMA_EA_EACS: return QString("ADPCM_IMA_EA_EACS");
        case AV_CODEC_ID_ADPCM_EA_XAS:      return QString("ADPCM_EA_XAS");
        case AV_CODEC_ID_ADPCM_EA_MAXIS_XA: return QString("ADPCM_EA_MAXIS_XA");
        case AV_CODEC_ID_ADPCM_IMA_ISS:     return QString("ADPCM_IMA_ISS");
        case AV_CODEC_ID_ADPCM_G722:        return QString("ADPCM_G722");
        case AV_CODEC_ID_ADPCM_IMA_APC:     return QString("ADPCM_IMA_APC");

        case AV_CODEC_ID_AMR_NB:            return QString("AMR_NB");
        case AV_CODEC_ID_AMR_WB:            return QString("AMR_WB");

        case AV_CODEC_ID_RA_144:            return QString("RA_144");
        case AV_CODEC_ID_RA_288:            return QString("RA_288");

        case AV_CODEC_ID_ROQ_DPCM:          return QString("ROQ_DPCM");
        case AV_CODEC_ID_INTERPLAY_DPCM:    return QString("INTERPLAY_DPCM");
        case AV_CODEC_ID_XAN_DPCM:          return QString("XAN_DPCM");
        case AV_CODEC_ID_SOL_DPCM:          return QString("SOL_DPCM");

        case AV_CODEC_ID_MP2:               return QString("MP2");
        case AV_CODEC_ID_MP3:               return QString("MP3");
        case AV_CODEC_ID_AAC:               return QString("AAC");
        case AV_CODEC_ID_AC3:               return QString("AC3");
        case AV_CODEC_ID_DTS:               return QString("DTS");
        case AV_CODEC_ID_VORBIS:            return QString("VORBIS");
        case AV_CODEC_ID_DVAUDIO:           return QString("DVAUDIO");
        case AV_CODEC_ID_WMAV1:             return QString("WMAV1");
        case AV_CODEC_ID_WMAV2:             return QString("WMAV2");
        case AV_CODEC_ID_MACE3:             return QString("MACE3");
        case AV_CODEC_ID_MACE6:             return QString("MACE6");
        case AV_CODEC_ID_VMDAUDIO:          return QString("VMDAUDIO");
        case AV_CODEC_ID_FLAC:              return QString("FLAC");
        case AV_CODEC_ID_MP3ADU:            return QString("MP3ADU");
        case AV_CODEC_ID_MP3ON4:            return QString("MP3ON4");
        case AV_CODEC_ID_SHORTEN:           return QString("SHORTEN");
        case AV_CODEC_ID_ALAC:              return QString("ALAC");
        case AV_CODEC_ID_WESTWOOD_SND1:     return QString("WESTWOOD_SND1");
        case AV_CODEC_ID_GSM:               return QString("GSM");
        case AV_CODEC_ID_QDM2:              return QString("QDM2");
        case AV_CODEC_ID_COOK:              return QString("COOK");
        case AV_CODEC_ID_TRUESPEECH:        return QString("TRUESPEECH");
        case AV_CODEC_ID_TTA:               return QString("TTA");
        case AV_CODEC_ID_SMACKAUDIO:        return QString("SMACKAUDIO");
        case AV_CODEC_ID_QCELP:             return QString("QCELP");
        case AV_CODEC_ID_WAVPACK:           return QString("WAVPACK");
        case AV_CODEC_ID_DSICINAUDIO:       return QString("DSICINAUDIO");
        case AV_CODEC_ID_IMC:               return QString("IMC");
        case AV_CODEC_ID_MUSEPACK7:         return QString("MUSEPACK7");
        case AV_CODEC_ID_MLP:               return QString("MLP");
        case AV_CODEC_ID_GSM_MS:            return QString("GMS_MS");
        case AV_CODEC_ID_ATRAC3:            return QString("ATRAC3");
        case AV_CODEC_ID_VOXWARE:           return QString("VOXWARE");
        case AV_CODEC_ID_APE:               return QString("APE");
        case AV_CODEC_ID_NELLYMOSER:        return QString("NELLYMOSER");
        case AV_CODEC_ID_MUSEPACK8:         return QString("MUSEPACK8");
        case AV_CODEC_ID_SPEEX:             return QString("SPEEX");
        case AV_CODEC_ID_WMAVOICE:          return QString("WMAVOICE");
        case AV_CODEC_ID_WMAPRO:            return QString("WMAPRO");
        case AV_CODEC_ID_WMALOSSLESS:       return QString("WMALOSSLESS");
        case AV_CODEC_ID_ATRAC3P:           return QString("ATRAC3P");
        case AV_CODEC_ID_EAC3:              return QString("EAC3");
        case AV_CODEC_ID_SIPR:              return QString("SIPR");
        case AV_CODEC_ID_MP1:               return QString("MP1");
        case AV_CODEC_ID_TWINVQ:            return QString("TWINVQ");
        case AV_CODEC_ID_TRUEHD:            return QString("TRUEHD");
        case AV_CODEC_ID_MP4ALS:            return QString("MP4ALS");
        case AV_CODEC_ID_ATRAC1:            return QString("ATRAC1");
        case AV_CODEC_ID_BINKAUDIO_RDFT:    return QString("BINKAUDIO_RDFT");
        case AV_CODEC_ID_BINKAUDIO_DCT:     return QString("BINKAUDIO_DCT");
        case AV_CODEC_ID_AAC_LATM:          return QString("AAC_LATM");
        case AV_CODEC_ID_QDMC:              return QString("QDMC");
        case AV_CODEC_ID_CELT:              return QString("CELT");
        case AV_CODEC_ID_G723_1:            return QString("G723_1");
        case AV_CODEC_ID_G729:              return QString("G729");
        case AV_CODEC_ID_8SVX_EXP:          return QString("8SVX_EXP");
        case AV_CODEC_ID_8SVX_FIB:          return QString("8SVX_FIB");
        case AV_CODEC_ID_BMV_AUDIO:         return QString("BMV_AUDIO");
        case AV_CODEC_ID_RALF:              return QString("RALF");
        case AV_CODEC_ID_IAC:               return QString("IAC");
        case AV_CODEC_ID_ILBC:              return QString("ILBC");

        case AV_CODEC_ID_DVD_SUBTITLE:      return QString("DVD_SUBTITLE");
        case AV_CODEC_ID_DVB_SUBTITLE:      return QString("DVB_SUBTITLE");
        case AV_CODEC_ID_TEXT:              return QString("TEXT");
        case AV_CODEC_ID_XSUB:              return QString("XSUB");
        case AV_CODEC_ID_SSA:               return QString("SSA");
        case AV_CODEC_ID_MOV_TEXT:          return QString("MOV_TEXT");
        case AV_CODEC_ID_HDMV_PGS_SUBTITLE: return QString("HDMV_PGS_SUBTITLE");
        case AV_CODEC_ID_DVB_TELETEXT:      return QString("DVB_TELETEXT");
        case AV_CODEC_ID_SRT:               return QString("SRT");

        case AV_CODEC_ID_TTF:               return QString("TTF");

        case AV_CODEC_ID_PROBE:             return QString("Probe");
        case AV_CODEC_ID_MPEG2TS:           return QString("MPEG2TS");
        case AV_CODEC_ID_MPEG4SYSTEMS:      return QString("MPEG4SYSTEMS");
        default:                            break;
    }

    return QString("Error");
}

QString AVMediaTypeToString(int Type)
{
    switch (Type)
    {
        case AVMEDIA_TYPE_VIDEO:      return QString("Video");
        case AVMEDIA_TYPE_AUDIO:      return QString("Audio");
        case AVMEDIA_TYPE_DATA:       return QString("Data");
        case AVMEDIA_TYPE_SUBTITLE:   return QString("Subtitle");
        case AVMEDIA_TYPE_ATTACHMENT: return QString("Attachment");
        default:                      break;
    }

    return QString("Unknown");
}

QString AVErrorToString(int Error)
{
    QByteArray error(128, 0);
    av_strerror(Error, error.data(), 128);
    return QString(error.data());
}

QString AVTimeToString(qint64 Time)
{
    qreal secs  = ((qreal)Time / (qreal)AV_TIME_BASE);
    int mins    = (int)secs / 60;
    secs        = secs - mins * 60;
    int hours   = mins / 60;
    mins %= 60;

    return QString("%1:%2:%3").arg(hours, 2, 10, QChar('0'))
                              .arg(mins,  2, 10, QChar('0'))
                              .arg(secs,  5, 'f', 2, QChar('0'));
}
