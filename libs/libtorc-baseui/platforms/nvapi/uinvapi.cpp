/* UINVApi
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
* You should have received a copy of the GNU General Public License
*
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QLibrary>
#include <QMutex>

// Torc
#include "torclocaldefs.h"
#include "torccompat.h"
#include "torclogging.h"
#include "nvapi.h"
#include "uiedid.h"
#include "uinvapi.h"

typedef NvAPI_Status (__cdecl * TORC_NVAPIQUERYINTERFACE)             (unsigned int);
typedef NvAPI_Status (__cdecl * TORC_NVAPIINITIALIZE)                 (void);
typedef NvAPI_Status (__cdecl * TORC_NVAPIGETASSOCIATEDDISPLAYHANDLE) (const char*,  NvDisplayHandle*);
typedef NvAPI_Status (__cdecl * TORC_NVAPIGETPHYSICALGPUSFROMDISPLAY) (NvDisplayHandle, NvPhysicalGpuHandle [NVAPI_MAX_PHYSICAL_GPUS], NvU32*);
typedef NvAPI_Status (__cdecl * TORC_NVAPIGETASSOCIATEDDISPLAYID)     (NvDisplayHandle, NvU32*);
typedef NvAPI_Status (__cdecl * TORC_NVAPIGPUGETEDID)                 (NvPhysicalGpuHandle, NvU32, NV_EDID*);

QMutex* gNVApiLock = new QMutex(QMutex::Recursive);

/*! \class NVApiLibray
 *  \brief A wrapper around the Nvidia NvAPI library.
 *
 *  \note nvapi.dll does not directly export the required symbols so
 *  we use the QueryInterface method with known function pointers. This
 *  may break with future versions of the library.
 *
*/

class NVApiLibrary : public QLibrary
{
  public:
    static NVApiLibrary* Create(void)
    {
        QMutexLocker locker(gNVApiLock);
        static NVApiLibrary *gLib = NULL;

        if (gLib)
            return gLib;

        gLib = new NVApiLibrary("nvapi");

        if (gLib->IsValid())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Loaded NVAPI: %1").arg(gLib->fileName()));
            return gLib;
        }

        LOG(VB_GENERAL, LOG_DEBUG, "Failed to load Nvidia NVAPI library");
        delete gLib;
        return NULL;
    }

    NVApiLibrary(const QString &Library)
      : QLibrary(Library),
        m_initialised(false),
        m_nvapiQueryInterface(NULL),
        m_nvapiInitialise(NULL),
        m_nvapiGetAssociatedDisplayHandle(NULL),
        m_nvapiGetPhysicalGPUsFromDisplay(NULL),
        m_nvapiGetAssociatedDisplayID(NULL),
        m_nvapiGPUGetEDID(NULL)
    {
        m_nvapiQueryInterface = (TORC_NVAPIQUERYINTERFACE)resolve("nvapi_QueryInterface");

        if (!m_nvapiQueryInterface)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Qt: '%1'").arg(errorString()));
        }
        else
        {
            m_nvapiInitialise                 = (TORC_NVAPIINITIALIZE)m_nvapiQueryInterface(0x0150E828);
            m_nvapiGetAssociatedDisplayHandle = (TORC_NVAPIGETASSOCIATEDDISPLAYHANDLE)m_nvapiQueryInterface(0x35C29134);
            m_nvapiGetPhysicalGPUsFromDisplay = (TORC_NVAPIGETPHYSICALGPUSFROMDISPLAY)m_nvapiQueryInterface(0x34EF9506);
            m_nvapiGetAssociatedDisplayID     = (TORC_NVAPIGETASSOCIATEDDISPLAYID)m_nvapiQueryInterface(0xD995937E);
            m_nvapiGPUGetEDID                 = (TORC_NVAPIGPUGETEDID)m_nvapiQueryInterface(0x37D32E69);
            if (m_nvapiInitialise)
                m_initialised = m_nvapiInitialise() == NVAPI_OK;
        }
    }

    bool IsValid(void)
    {
        return m_nvapiQueryInterface && m_nvapiInitialise && m_nvapiGetAssociatedDisplayHandle &&
               m_nvapiGetPhysicalGPUsFromDisplay && m_nvapiGetAssociatedDisplayID &&
               m_nvapiGPUGetEDID && m_initialised;
    }

    bool                                 m_initialised;
    TORC_NVAPIQUERYINTERFACE             m_nvapiQueryInterface;
    TORC_NVAPIINITIALIZE                 m_nvapiInitialise;
    TORC_NVAPIGETASSOCIATEDDISPLAYHANDLE m_nvapiGetAssociatedDisplayHandle;
    TORC_NVAPIGETPHYSICALGPUSFROMDISPLAY m_nvapiGetPhysicalGPUsFromDisplay;
    TORC_NVAPIGETASSOCIATEDDISPLAYID     m_nvapiGetAssociatedDisplayID;
    TORC_NVAPIGPUGETEDID                 m_nvapiGPUGetEDID;
};

bool UINVApi::NVApiAvailable(void)
{
    return NVApiLibrary::Create();
}

class EDIDFactoryNVApi : public EDIDFactory
{
    void GetEDID(QMap<QPair<int, QString>, QByteArray> &EDIDMap, WId Window, int Screen)
    {
        (void)Screen;

        NVApiLibrary* lib = NVApiLibrary::Create();
        if (!lib)
            return;

        MONITORINFOEX monitor;
        memset(&monitor, 0, sizeof(MONITORINFOEX));
        monitor.cbSize = sizeof(MONITORINFOEX);

        HMONITOR monitorid = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
        GetMonitorInfo(monitorid, &monitor);

        NvDisplayHandle handle;
        if (lib->m_nvapiGetAssociatedDisplayHandle(monitor.szDevice, &handle) != NVAPI_OK)
            return;

        NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS] = {0};
        NvU32 numbergpus = 0;
        if (lib->m_nvapiGetPhysicalGPUsFromDisplay(handle, gpus, &numbergpus) == NVAPI_OK &&
            numbergpus > 0)
        {
            NvU32 displayid;
            if (lib->m_nvapiGetAssociatedDisplayID(handle, &displayid) != NVAPI_OK)
                return;

            NV_EDID nvedid = {0};
            nvedid.version = NV_EDID_VER2;

            if (lib->m_nvapiGPUGetEDID(gpus[0], displayid, &nvedid) != NVAPI_OK)
                return;

            QByteArray edid((const char*)nvedid.EDID_Data, NV_EDID_DATA_SIZE);

            if (nvedid.sizeofEDID > NV_EDID_DATA_SIZE)
            {
                NV_EDID nvedid2 = {0};
                nvedid2.version = NV_EDID_VER2;
                nvedid2.offset  = NV_EDID_DATA_SIZE;

                // NB this probably won't work as 'offset' is only strictly available in NV_EDID_V3
                if (lib->m_nvapiGPUGetEDID(gpus[0], displayid, &nvedid2) == NVAPI_OK)
                {
                    QByteArray edid2((const char*)nvedid2.EDID_Data, NV_EDID_DATA_SIZE);
                    edid.append(edid2);
                }
            }

            EDIDMap.insert(qMakePair(90, QString("Nvidia API")), edid);
            return;
        }
    }
} EDIDFactoryNVApi;
