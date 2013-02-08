/* UIADL
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
#include <QMutex>
#include <QLibrary>

// Torc
#include "torclogging.h"
#include "torcedid.h"
#include "adl_sdk.h"
#include "uiadl.h"

typedef int (__stdcall * TORC_ADLMAINCONTROLCREATE)          (ADL_MAIN_MALLOC_CALLBACK, int);
typedef int (__stdcall * TORC_ADLMAINCONTROLDESTROY)         (void);
typedef int (__stdcall * TORC_ADLMAINCONTROLREFRESH)         (void);
typedef int (__stdcall * TORC_ADLADAPTERNUMBEROFADAPTERSGET) (int*);
typedef int (__stdcall * TORC_ADLADAPTERADAPTERINFOGET)      (LPAdapterInfo, int);
typedef int (__stdcall * TORC_ADLDISPLAYDISPLAYINFOGET)      (int, int*, ADLDisplayInfo**, int);
typedef int (__stdcall * TORC_ADLDISPLAYEDIDDATAGET)         (int, int, ADLDisplayEDIDData*);

QMutex* gADLLock = new QMutex(QMutex::Recursive);

void* __stdcall ADLMainMemoryAlloc(int Size)
{
    return malloc(Size);
}

class ADLLibrary : public QLibrary
{
  public:
    static ADLLibrary* Create(void)
    {
        QMutexLocker locker(gADLLock);
        static ADLLibrary *gLib = NULL;

        if (gLib)
            return gLib;

        gLib = new ADLLibrary("atiadlxx");
        if (!gLib->IsValid())
        {
            delete gLib;
            gLib = new ADLLibrary("atiadlxy");
        }

        if (gLib->IsValid())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Loaded ADL: %1").arg(gLib->fileName()));
            return gLib;
        }

        LOG(VB_GENERAL, LOG_DEBUG, "Failed to load ATI ADL library");
        delete gLib;
        return NULL;
    }

    ADLLibrary(const QString &Library)
      : QLibrary(Library),
        m_initialised(false)
    {
        m_mainControlCreate          = (TORC_ADLMAINCONTROLCREATE)resolve("ADL_Main_Control_Create");
        m_mainControlDestroy         = (TORC_ADLMAINCONTROLDESTROY)resolve("ADL_Main_Control_Destroy");
        m_mainControlRefresh         = (TORC_ADLMAINCONTROLREFRESH)resolve("ADL_Main_Control_Refresh");
        m_adapterNumberofAdaptersGet = (TORC_ADLADAPTERNUMBEROFADAPTERSGET)resolve("ADL_Adapter_NumberOfAdapters_Get");
        m_adapterAdapterInfoGet      = (TORC_ADLADAPTERADAPTERINFOGET)resolve("ADL_Adapter_AdapterInfo_Get");
        m_displayDisplayInfoGet      = (TORC_ADLDISPLAYDISPLAYINFOGET)resolve("ADL_Display_DisplayInfo_Get");
        m_displayEDIDDataGet         = (TORC_ADLDISPLAYEDIDDATAGET)resolve("ADL_Display_EdidData_Get");
        if (m_mainControlCreate)
            m_initialised            = m_mainControlCreate(ADLMainMemoryAlloc, 1) == ADL_OK;
    }

    bool IsValid(void)
    {
        return m_mainControlCreate && m_mainControlDestroy && m_mainControlRefresh &&
               m_adapterNumberofAdaptersGet && m_adapterAdapterInfoGet && m_displayDisplayInfoGet &&
               m_displayEDIDDataGet && m_initialised;
    }

    bool                               m_initialised;
    TORC_ADLMAINCONTROLCREATE          m_mainControlCreate;
    TORC_ADLMAINCONTROLDESTROY         m_mainControlDestroy;
    TORC_ADLMAINCONTROLREFRESH         m_mainControlRefresh;
    TORC_ADLADAPTERNUMBEROFADAPTERSGET m_adapterNumberofAdaptersGet;
    TORC_ADLADAPTERADAPTERINFOGET      m_adapterAdapterInfoGet;
    TORC_ADLDISPLAYDISPLAYINFOGET      m_displayDisplayInfoGet;
    TORC_ADLDISPLAYEDIDDATAGET         m_displayEDIDDataGet;
};


bool UIADL::ADLAvailable(void)
{
    QMutexLocker locker(gADLLock);

    static bool available = false;
    static bool checked   = false;

    if (checked)
        return available;

    checked = true;
    available = ADLLibrary::Create();
    return available;
}

QByteArray UIADL::GetADLEDID(char *Display, int Screen, const QString Hint)
{
    (void)Screen;

    QMutexLocker locker(gADLLock);
    ADLLibrary* lib = ADLLibrary::Create();

    if (!lib)
        return QByteArray();

    lib->m_mainControlRefresh();

    // On windows, ADL appears to list every monitor against every display but orders them such that the
    // first returned display is always the one we are looking for (with no other obvious logic or key that
    // we can use). To sanitise this, check the hint against the EDID if it is valid and only inspect the
    // first display. If we miss the correct one, it should fall back to standard windows registry inspection and find it there.

    int numberofadapters = 0;
    if ((ADL_OK == lib->m_adapterNumberofAdaptersGet(&numberofadapters)) && (numberofadapters > 0))
    {
        int size = numberofadapters * sizeof(AdapterInfo);
        LPAdapterInfo adapter = (LPAdapterInfo)malloc(size);
        memset(adapter, 0, size);

        if (ADL_OK == lib->m_adapterAdapterInfoGet(adapter, size))
        {
            for (int i = 0; i < numberofadapters; ++i)
            {
                LPADLDisplayInfo display = NULL;
                int numberofdisplays = 0;
                int adapterindex = adapter[i].iAdapterIndex;

                if (strcmp(adapter[i].strDisplayName, Display) || !adapter[i].iPresent ||
                    ADL_OK != lib->m_displayDisplayInfoGet(adapterindex, &numberofdisplays, &display, 0))
                {
                    free(display);
                    continue;
                }

                for (int j = 0; j < std::min(1, numberofdisplays); ++j)
                {
                    if (display[j].iDisplayInfoValue & (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED))
                    {
                        LOG(VB_GENERAL, LOG_INFO, QString("Looking for EDID data for '%1' on display '%2'")
                            .arg(display[j].strDisplayName).arg(adapter[i].strDisplayName));

                        QByteArray edid;
                        for (int blockindex = 0; blockindex < 2; ++blockindex)
                        {
                            int displayindex = display[j].displayID.iDisplayLogicalIndex;
                            ADLDisplayEDIDData data;
                            memset(&data, 0, sizeof(ADLDisplayEDIDData));
                            data.iSize = sizeof(ADLDisplayEDIDData);
                            data.iBlockIndex = blockindex;

                            if (ADL_OK == lib->m_displayEDIDDataGet(adapterindex, displayindex, &data))
                            {
                                QByteArray block(data.cEDIDData, data.iEDIDSize);
                                edid.append(block);
                            }
                            else
                            {
                                break;
                            }
                        }

                        if (!edid.isEmpty())
                        {
                            TorcEDID check(edid);
                            if (Hint == check.GetMSString())
                                return edid;
                        }
                    }
                }

                free(display);
            }
        }

        free(adapter);
    }

    return QByteArray();
}
