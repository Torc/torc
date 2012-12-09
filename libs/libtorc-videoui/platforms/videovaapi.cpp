/* Class VideoVAAPI
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
#include <QGLWidget> // for GL/GLX includes
#include <QMutex>
#include <QLibrary>

// Torc
#include "torclogging.h"
#include "videovaapi.h"

extern "C" {
#include <X11/Xlib.h>
#include "va/va.h"
#include "libavcodec/vaapi.h"
}

static QMutex* gVAAPILock = new QMutex(QMutex::Recursive);

typedef VADisplay (*VAAPIGetDisplayGLX)     (Display*);
typedef VAStatus  (*VAAPICreateSurfaceGLX)  (VADisplay, GLenum, GLuint, void**);
typedef VAStatus  (*VAAPIDestroySurfaceGLX) (VADisplay, void*);
typedef VAStatus  (*VAAPICopySurfaceGLX)    (VADisplay, void*, VASurfaceID, unsigned int);

static class VAAPIGLX
{
  public:
    VAAPIGLX()
    {
        m_getDisplayGLX     = (VAAPIGetDisplayGLX)     QLibrary::resolve("va-glx", "vaGetDisplayGLX");
        m_createSurfaceGLX  = (VAAPICreateSurfaceGLX)  QLibrary::resolve("va-glx", "vaCreateSurfaceGLX");
        m_destroySurfaceGLX = (VAAPIDestroySurfaceGLX) QLibrary::resolve("va-glx", "vaDestroySurfaceGLX");
        m_copySurfaceGLX    = (VAAPICopySurfaceGLX)    QLibrary::resolve("va-glx", "vaCopySurfaceGLX");

        m_valid = m_getDisplayGLX && m_createSurfaceGLX && m_destroySurfaceGLX && m_copySurfaceGLX;
    }

    bool                   m_valid;
    VAAPIGetDisplayGLX     m_getDisplayGLX;
    VAAPICreateSurfaceGLX  m_createSurfaceGLX;
    VAAPIDestroySurfaceGLX m_destroySurfaceGLX;
    VAAPICopySurfaceGLX    m_copySurfaceGLX;
} VAAPIGLX;

bool VideoVAAPI::VAAPIAvailable(void)
{
    QMutexLocker locker(gVAAPILock);

    static bool available = false;
    static bool checked   = false;

    if (checked)
        return available;

    checked = true;

    if (!VAAPIGLX.m_valid)
        return available;

    Display *display = XOpenDisplay(NULL);
    if (display)
    {
        VADisplay vadisplay = VAAPIGLX.m_getDisplayGLX(display);
        if (vaDisplayIsValid(vadisplay))
        {
            LOG(VB_GENERAL, LOG_INFO, "VAAPI hardware decoding available");
            available = true;
            vaTerminate(vadisplay);
        }

        XCloseDisplay(display);
    }

    return available;
}
