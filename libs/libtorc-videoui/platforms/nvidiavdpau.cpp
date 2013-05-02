/* NVIDIAVDPAU
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include <QtGlobal>

// Torc
#include "nvidiavdpau.h"

NVInterop::NVInterop()
  : m_valid(false),
    m_initialised(false),
    m_registeredSurface(0),
    m_init(NULL),
    m_fini(NULL),
    m_registerOutputSurface(NULL),
    m_isSurface(NULL),
    m_unregisterSurface(NULL),
    m_surfaceAccess(NULL),
    m_mapSurface(NULL),
    m_unmapSurface(NULL)
{
    QByteArray extensions((const char*)(glGetString(GL_EXTENSIONS)));

    if (extensions.contains("GL_NV_vdpau_interop"))
    {
        m_init                  = (TORC_INITNV)     UIOpenGLWindow::GetProcAddress("glVDPAUInitNV");
        m_fini                  = (TORC_FININV)     UIOpenGLWindow::GetProcAddress("glVDPAUFiniNV");
        m_registerOutputSurface = (TORC_REGOUTSURF) UIOpenGLWindow::GetProcAddress("glVDPAURegisterOutputSurfaceNV");
        m_isSurface             = (TORC_ISSURFACE)  UIOpenGLWindow::GetProcAddress("glVDPAUIsSurfaceNV");
        m_unregisterSurface     = (TORC_UNREGSURF)  UIOpenGLWindow::GetProcAddress("glVDPAUUnregisterSurfaceNV");
        m_surfaceAccess         = (TORC_SURFACCESS) UIOpenGLWindow::GetProcAddress("glVDPAUSurfaceAccessNV");
        m_mapSurface            = (TORC_MAPSURF)    UIOpenGLWindow::GetProcAddress("glVDPAUMapSurfacesNV");
        m_unmapSurface          = (TORC_UNMAPSURF)  UIOpenGLWindow::GetProcAddress("glVDPAUUnmapSurfacesNV");
    }

    m_valid = m_init && m_fini && m_registerOutputSurface && m_unregisterSurface &&
              m_isSurface && m_surfaceAccess && m_mapSurface && m_unmapSurface;
}

bool NVInterop::IsValid(void)
{
    return m_valid;
}

QString FeatureSetToString(NVVDPAUFeatureSet Set)
{
    switch (Set)
    {
        case Set_None: return QString("None");
        case Set_A1:   return QString("A1");
        case Set_A:    return QString("A");
        case Set_B1:   return QString("B1");
        case Set_B:    return QString("B");
        case Set_C:    return QString("C");
        case Set_D:    return QString("D");
        case Set_Unknown:
            break;
    }

    return QString("Unknown");
}

// NB this probably only needs to track A1/B1 going forward - which aren't going to change
NVVDPAUFeatureSet GetNVIDIAFeatureSet(int PCIID)
{
    // list up to date for 313.30

    quint16 id = PCIID & 0xffff;

    switch (id)
    {
        // really really old
        case 0x0020:
        case 0x0028:
        case 0x0029:
        case 0x002C:
        case 0x002D:
        case 0x00A0:
        case 0x0100:
        case 0x0101:
        case 0x0103:
        case 0x0150:
        case 0x0151:
        case 0x0152:
        case 0x0153:
        // really old
        case 0x0110:
        case 0x0111:
        case 0x0112:
        case 0x0113:
        case 0x0170:
        case 0x0171:
        case 0x0172:
        case 0x0173:
        case 0x0174:
        case 0x0175:
        case 0x0176:
        case 0x0177:
        case 0x0178:
        case 0x0179:
        case 0x017A:
        case 0x017C:
        case 0x017D:
        case 0x0181:
        case 0x0182:
        case 0x0183:
        case 0x0185:
        case 0x0188:
        case 0x018A:
        case 0x018B:
        case 0x018C:
        case 0x01A0:
        case 0x01F0:
        case 0x0200:
        case 0x0201:
        case 0x0202:
        case 0x0203:
        case 0x0250:
        case 0x0251:
        case 0x0253:
        case 0x0258:
        case 0x0259:
        case 0x025B:
        case 0x0280:
        case 0x0281:
        case 0x0282:
        case 0x0286:
        case 0x0288:
        case 0x0289:
        case 0x028C:
        // old
        case 0x00FA:
        case 0x00FB:
        case 0x00FC:
        case 0x00FD:
        case 0x00FE:
        case 0x0301:
        case 0x0302:
        case 0x0308:
        case 0x0309:
        case 0x0311:
        case 0x0312:
        case 0x0314:
        case 0x031A:
        case 0x031B:
        case 0x031C:
        case 0x0320:
        case 0x0321:
        case 0x0322:
        case 0x0323:
        case 0x0324:
        case 0x0325:
        case 0x0326:
        case 0x0327:
        case 0x0328:
        case 0x032A:
        case 0x032B:
        case 0x032C:
        case 0x032D:
        case 0x0330:
        case 0x0331:
        case 0x0332:
        case 0x0333:
        case 0x0334:
        case 0x0338:
        case 0x033F:
        case 0x0341:
        case 0x0342:
        case 0x0343:
        case 0x0344:
        case 0x0347:
        case 0x0348:
        case 0x034C:
        case 0x034E:
        // nearly there
        case 0x0040:
        case 0x0041:
        case 0x0042:
        case 0x0043:
        case 0x0044:
        case 0x0045:
        case 0x0046:
        case 0x0047:
        case 0x0048:
        case 0x004E:
        case 0x0090:
        case 0x0091:
        case 0x0092:
        case 0x0093:
        case 0x0095:
        case 0x0098:
        case 0x0099:
        case 0x009D:
        case 0x00C0:
        case 0x00C1:
        case 0x00C2:
        case 0x00C3:
        case 0x00C8:
        case 0x00C9:
        case 0x00CC:
        case 0x00CD:
        case 0x00CE:
        case 0x00F1:
        case 0x00F2:
        case 0x00F3:
        case 0x00F4:
        case 0x00F5:
        case 0x00F6:
        case 0x00F8:
        case 0x00F9:
        case 0x0140:
        case 0x0141:
        case 0x0142:
        case 0x0143:
        case 0x0144:
        case 0x0145:
        case 0x0146:
        case 0x0147:
        case 0x0148:
        case 0x0149:
        case 0x014A:
        case 0x014C:
        case 0x014D:
        case 0x014E:
        case 0x014F:
        case 0x0160:
        case 0x0161:
        case 0x0162:
        case 0x0163:
        case 0x0164:
        case 0x0165:
        case 0x0166:
        case 0x0167:
        case 0x0168:
        case 0x0169:
        case 0x016A:
        case 0x01D0:
        case 0x01D1:
        case 0x01D2:
        case 0x01D3:
        case 0x01D6:
        case 0x01D7:
        case 0x01D8:
        case 0x01DA:
        case 0x01DB:
        case 0x01DC:
        case 0x01DD:
        case 0x01DE:
        case 0x01DF:
        case 0x0211:
        case 0x0212:
        case 0x0215:
        case 0x0218:
        case 0x0221:
        case 0x0222:
        case 0x0240:
        case 0x0241:
        case 0x0242:
        case 0x0244:
        case 0x0245:
        case 0x0247:
        case 0x0290:
        case 0x0291:
        case 0x0292:
        case 0x0293:
        case 0x0294:
        case 0x0295:
        case 0x0297:
        case 0x0298:
        case 0x0299:
        case 0x029A:
        case 0x029B:
        case 0x029C:
        case 0x029D:
        case 0x029E:
        case 0x029F:
        case 0x02E0:
        case 0x02E1:
        case 0x02E2:
        case 0x02E3:
        case 0x02E4:
        case 0x038B:
        case 0x0390:
        case 0x0391:
        case 0x0392:
        case 0x0393:
        case 0x0394:
        case 0x0395:
        case 0x0397:
        case 0x0398:
        case 0x0399:
        case 0x039C:
        case 0x039E:
        case 0x03D0:
        case 0x03D1:
        case 0x03D2:
        case 0x03D5:
        case 0x03D6:
        case 0x0531:
        case 0x0533:
        case 0x053A:
        case 0x053B:
        case 0x053E:
        case 0x07E0:
        case 0x07E1:
        case 0x07E2:
        case 0x07E3:
        case 0x07E5:
        // close, but no cigar (GeForce)
        case 0x0191:
        case 0x0193:
        case 0x0194:
        case 0x0406:
        case 0x0420:
        case 0x0423:
        case 0x06E3:
        case 0x06E7:
        case 0x084F:
        case 0x0A22:
        case 0x0A67:
        // no cigar and the big bucks (Quadro/Tesla)
        case 0x019D:
        case 0x019E:
        case 0x0197:
            return Set_None;
        // A1
        case 0x06E4:
            return Set_A1;
        // A GeForce
        case 0x0400:
        case 0x0401:
        case 0x0402:
        case 0x0403:
        case 0x0404:
        case 0x0405:
        case 0x0407:
        case 0x0408:
        case 0x0409:
        case 0x0410:
        case 0x0421:
        case 0x0422:
        case 0x0424:
        case 0x0425:
        case 0x0426:
        case 0x0427:
        case 0x0428:
        case 0x042C:
        case 0x042E:
        case 0x05E0:
        case 0x05E1:
        case 0x05E2:
        case 0x05E3:
        case 0x05E6:
        case 0x05EA:
        case 0x05EB:
        case 0x0600:
        case 0x0601:
        case 0x0602:
        case 0x0603:
        case 0x0604:
        case 0x0605:
        case 0x0606:
        case 0x0607:
        case 0x0608:
        case 0x0609:
        case 0x060A:
        case 0x060B:
        case 0x060C:
        case 0x060D:
        case 0x060F:
        case 0x0610:
        case 0x0611:
        case 0x0612:
        case 0x0613:
        case 0x0614:
        case 0x0615:
        case 0x0617:
        case 0x0618:
        case 0x0621:
        case 0x0622:
        case 0x0623:
        case 0x0625:
        case 0x0626:
        case 0x0627:
        case 0x0628:
        case 0x062A:
        case 0x062B:
        case 0x062C:
        case 0x062D:
        case 0x062E:
        case 0x0630:
        case 0x0631:
        case 0x0632:
        case 0x0635:
        case 0x0637:
        case 0x0640:
        case 0x0641:
        case 0x0643:
        case 0x0644:
        case 0x0645:
        case 0x0646:
        case 0x0647:
        case 0x0648:
        case 0x0649:
        case 0x064A:
        case 0x064B:
        case 0x064C:
        case 0x0651:
        case 0x0652:
        case 0x0653:
        case 0x0654:
        case 0x0655:
        case 0x0656:
        case 0x065B:
        case 0x0A2D:
        case 0x0CA0:
        case 0x0CA7:
        case 0x010C3:
        // A Quadro etc
        case 0x040A:
        case 0x040C:
        case 0x040D:
        case 0x040E:
        case 0x040F:
        case 0x042D:
        case 0x05ED:
        case 0x05F8:
        case 0x05F9:
        case 0x05FD:
        case 0x05FE:
        case 0x05FF:
        case 0x0619:
        case 0x061A:
        case 0x061B:
        case 0x061C:
        case 0x061D:
        case 0x061E:
        case 0x061F:
        case 0x0638:
        case 0x063A:
        case 0x0658:
        case 0x0659:
        case 0x065A:
        case 0x065C:
        case 0x040B:
        case 0x0429:
        case 0x042A:
        case 0x042B:
        case 0x042F:
        case 0x05E7:
            return Set_A;
        // B1 GeForce
        case 0x06E0:
        case 0x06E1:
        case 0x06E2:
        case 0x06E5:
        case 0x06E6:
        case 0x06E8:
        case 0x06E9:
        case 0x06EC:
        case 0x06EF:
        case 0x06F1:
        case 0x0840:
        case 0x0844:
        case 0x0845:
        case 0x0846:
        case 0x0847:
        case 0x0848:
        case 0x0849:
        case 0x084A:
        case 0x084B:
        case 0x084C:
        case 0x084D:
        case 0x0860:
        case 0x0861:
        case 0x0862:
        case 0x0863:
        case 0x0864:
        case 0x0865:
        case 0x0866:
        case 0x0867:
        case 0x0868:
        case 0x0869:
        case 0x086A:
        case 0x086C:
        case 0x086D:
        case 0x086E:
        case 0x086F:
        case 0x0870:
        case 0x0871:
        case 0x0872:
        case 0x0873:
        case 0x0874:
        case 0x0876:
        case 0x087A:
        case 0x087D:
        case 0x087E:
        case 0x087F:
        // B1 Quadro etc
        case 0x06F9:
        case 0x06FB:
        case 0x06FF:
        case 0x06EA:
        case 0x06EB:
        case 0x06F8:
        case 0x06FA:
        case 0x06FD:
            return Set_B1;
        // B GeForce
        case 0x0A68:
        case 0x0A69:
        case 0x10C0:
            return Set_B;
        // C GeForce
        case 0x06C0:
        case 0x06C4:
        case 0x06CA:
        case 0x06CD:
        case 0x08A0:
        case 0x08A2:
        case 0x08A3:
        case 0x08A4:
        case 0x08A5:
        case 0x0A20:
        case 0x0A23:
        case 0x0A26:
        case 0x0A27:
        case 0x0A28:
        case 0x0A29:
        case 0x0A2A:
        case 0x0A2B:
        case 0x0A32:
        case 0x0A34:
        case 0x0A35:
        case 0x0A60:
        case 0x0A62:
        case 0x0A63:
        case 0x0A64:
        case 0x0A65:
        case 0x0A66:
        case 0x0A6E:
        case 0x0A6F:
        case 0x0A70:
        case 0x0A71:
        case 0x0A72:
        case 0x0A73:
        case 0x0A74:
        case 0x0A75:
        case 0x0A76:
        case 0x0A7A:
        case 0x0CA2:
        case 0x0CA3:
        case 0x0CA4:
        case 0x0CA5:
        case 0x0CA8:
        case 0x0CA9:
        case 0x0CAC:
        case 0x0CAF:
        case 0x0CB0:
        case 0x0CB1:
        case 0x0DC0:
        case 0x0DC4:
        case 0x0DC5:
        case 0x0DC6:
        case 0x0DCD:
        case 0x0DCE:
        case 0x0DD1:
        case 0x0DD2:
        case 0x0DD3:
        case 0x0DD6:
        case 0x0DE0:
        case 0x0DE1:
        case 0x0DE2:
        case 0x0DE3:
        case 0x0DE4:
        case 0x0DE5:
        case 0x0DE8:
        case 0x0DE9:
        case 0x0DEA:
        case 0x0DEB:
        case 0x0DEC:
        case 0x0DED:
        case 0x0DEE:
        case 0x0DF0:
        case 0x0DF1:
        case 0x0DF2:
        case 0x0DF3:
        case 0x0DF4:
        case 0x0DF5:
        case 0x0DF6:
        case 0x0DF7:
        case 0x0E22:
        case 0x0E23:
        case 0x0E24:
        case 0x0E30:
        case 0x0E31:
        case 0x0F00:
        case 0x0F01:
        case 0x0FC0:
        case 0x0FC1:
        case 0x0FC2:
        case 0x0FCE:
        case 0x0FD2:
        case 0x0FD3:
        case 0x1040:
        case 0x1049:
        case 0x1050:
        case 0x1052:
        case 0x1058:
        case 0x1059:
        case 0x105A:
        case 0x1080:
        case 0x1081:
        case 0x1082:
        case 0x1084:
        case 0x1086:
        case 0x1087:
        case 0x1088:
        case 0x1089:
        case 0x108B:
        case 0x10C5:
        case 0x1140:
        case 0x1200:
        case 0x1201:
        case 0x1203:
        case 0x1205:
        case 0x1206:
        case 0x1207:
        case 0x1208:
        case 0x1210:
        case 0x1211:
        case 0x1212:
        case 0x1213:
        case 0x1241:
        case 0x1243:
        case 0x1244:
        case 0x1245:
        case 0x1246:
        case 0x1247:
        case 0x1248:
        case 0x1249:
        case 0x124B:
        case 0x124D:
        case 0x1251:
        case 0x1280:
        // C Quadro etc
        case 0x06D8:
        case 0x06D9:
        case 0x06DA:
        case 0x06DC:
        case 0x06DD:
        case 0x0A38:
        case 0x0A3C:
        case 0x0A78:
        case 0x0A7C:
        case 0x0CBC:
        case 0x0DD8:
        case 0x0DDA:
        case 0x0DF8:
        case 0x0DF9:
        case 0x0DFA:
        case 0x0E3A:
        case 0x0E3B:
        case 0x109A:
        case 0x109B:
        case 0x0A2C:
        case 0x0A6A:
        case 0x0A6C:
        case 0x0DEF:
        case 0x0DFC:
        case 0x10D8:
        //case 0x1140:
        case 0x06D1:
        case 0x0771:
        case 0x06D2:
        case 0x06DE:
        case 0x06DF:
        case 0x1091:
        case 0x1094:
        case 0x1096:
            return Set_C;
        // D GeForce
        case 0x0FC6:
        case 0x0FD1:
        //case 0x0FD2: both C and D - but no functional difference currently
        case 0x0FD4:
        case 0x0FD5:
        case 0x0FD8:
        case 0x0FD9:
        case 0x0FE0:
        case 0x0FE1:
        case 0x1005:
        case 0x1042:
        case 0x1048:
        case 0x104A:
        case 0x104B:
        case 0x1051:
        case 0x1054:
        case 0x1055:
        //case 0x1058: both C and D - but no functional difference currently
        case 0x105B:
        case 0x1180:
        case 0x1183:
        case 0x1185:
        case 0x1188:
        case 0x1189:
        case 0x11A0:
        case 0x11A1:
        case 0x11A2:
        case 0x11A3:
        case 0x11A7:
        case 0x11C0:
        case 0x11C3:
        case 0x11C6:
        // D Quadro etc
        case 0x0FF9:
        case 0x0FFA:
        case 0x0FFB:
        case 0x0FFC:
        case 0x0FFE:
        case 0x0FFF:
        case 0x11BA:
        case 0x11BC:
        case 0x11BD:
        case 0x11BE:
        case 0x11FA:
        case 0x0FFD:
        case 0x1056:
        case 0x1057:
        case 0x107D:
        case 0x1021:
        case 0x1022:
        case 0x1026:
        case 0x1028:
        case 0x118F:
        case 0x0FF2:
        case 0x11BF:
            return Set_D;
        default:
            break;
    }

    return Set_Unknown;
}
