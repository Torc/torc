/* Class TorcMediaDirectoryMonitor
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

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcmediadirectorymonitor.h"

TorcMediaDirectoryMonitor::TorcMediaDirectoryMonitor()
  : QFileSystemWatcher()
{
    LOG(VB_GENERAL, LOG_INFO, "Starting directory monitor");

    connect(this, SIGNAL(directoryChanged(QString)), this, SLOT(DirectoryChanged(QString)));
    addPath("/Users/mark_kendall/Dropbox/Videos/");
}

TorcMediaDirectoryMonitor::~TorcMediaDirectoryMonitor()
{
    LOG(VB_GENERAL, LOG_INFO, "Stopping directory monitor");
}

void TorcMediaDirectoryMonitor::DirectoryChanged(const QString &Path)
{
    LOG(VB_GENERAL, LOG_INFO, Path);
}

class DirectoryMonitor : public TorcAdminObject
{
  public:
    DirectoryMonitor()
     :  TorcAdminObject(TORC_ADMIN_LOW_PRIORITY),
        m_directoryMonitor(NULL)
    {
    }

    void Create(void)
    {
        m_directoryMonitor = new TorcMediaDirectoryMonitor();
    }

    void Destroy(void)
    {
        delete m_directoryMonitor;
    }

  private:
    TorcMediaDirectoryMonitor *m_directoryMonitor;

} DirectoryMonitor;
