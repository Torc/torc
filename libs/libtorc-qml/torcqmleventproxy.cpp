/* Class TorcQMLEventProxy
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QThread>
#include <QQuickWindow>
#include <QGuiApplication>

// Torc
#include "torclocalcontext.h"
#include "torcloggingimp.h"
#include "torcevent.h"
#include "torcqmldisplay.h"
#include "torcqmleventproxy.h"

TorcQMLEventProxy::TorcQMLEventProxy(QWindow *Window, bool Hidemouse /* = false*/)
  : QObject(),
    m_window(Window),
    m_callbackLock(new QMutex()),
    m_mouseTimer(NULL),
    m_mouseHidden(false),
    m_display(TorcQMLDisplay::Create(Window))
{
    gLocalContext->SetUIObject(this);

    if (m_window)
    {
        m_window->installEventFilter(this);
        gLocalContext->AddObserver(this);

        QQuickWindow *window = dynamic_cast<QQuickWindow*>(m_window);
        if (window && gLocalContext)
        {
            connect(window, SIGNAL(sceneGraphInitialized()), this,          SLOT(SceneGraphInitialized()), Qt::DirectConnection);
            connect(this,   SIGNAL(SceneGraphReady()),       gLocalContext, SLOT(RegisterQThread()),       Qt::DirectConnection);
            connect(window, SIGNAL(sceneGraphInvalidated()), gLocalContext, SLOT(DeregisterQThread()),     Qt::DirectConnection);
        }
    }

    if (Hidemouse)
    {
        m_mouseTimer = new QTimer();
        connect(m_mouseTimer, SIGNAL(timeout()), this, SLOT(HideMouse()));
        m_mouseTimer->setTimerType(Qt::VeryCoarseTimer);
        m_mouseTimer->start(5000);
    }
}

TorcQMLEventProxy::~TorcQMLEventProxy()
{
    delete m_display;
    delete m_mouseTimer;

    if (m_window)
    {
        gLocalContext->RemoveObserver(this);
        m_window->removeEventFilter(this);
    }

    gLocalContext->SetUIObject(NULL);
}

void TorcQMLEventProxy::RegisterCallback(RenderCallback Function, void *Object, int Parameter)
{
    QMutexLocker locker(m_callbackLock);
    m_callbacks.push_back(TorcRenderCallback(Function, Object, Parameter));
}

void TorcQMLEventProxy::ProcessCallbacks(void)
{
    m_callbackLock->lock();
    if (m_callbacks.isEmpty())
    {
        m_callbackLock->unlock();
        return;
    }

    QList<TorcRenderCallback> callbacks = m_callbacks;
    m_callbacks.clear();
    m_callbackLock->unlock();

    for (int i = 0; i < callbacks.size(); ++i)
        (callbacks[i].m_function)(callbacks[i].m_object, callbacks[i].m_parameter);
}

TorcQMLDisplay* TorcQMLEventProxy::GetDisplay(void)
{
    return m_display;
}

QWindow* TorcQMLEventProxy::GetWindow(void)
{
    return m_window;
}

bool TorcQMLEventProxy::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* event = static_cast<TorcEvent*>(Event);
        if (event && event->GetEvent() == Torc::Exit && m_window)
        {
            LOG(VB_GENERAL, LOG_INFO, "Closing main window");
            QGuiApplication::quit();
            return true;
        }
    }

    return false;
}

bool TorcQMLEventProxy::eventFilter(QObject *Object, QEvent *Event)
{
    if (Event && Object && Object == m_window && QEvent::MouseMove == Event->type() && m_mouseHidden && m_mouseTimer)
    {
        m_mouseHidden = false;
        m_mouseTimer->start();
        QGuiApplication::restoreOverrideCursor();
    }

    return false;
}

void TorcQMLEventProxy::SceneGraphInitialized(void)
{
    QThread::currentThread()->setObjectName(QTRENDER_THREAD);
    emit SceneGraphReady();
    LOG(VB_GENERAL, LOG_INFO, "Qt SceneGraph ready");
}

void TorcQMLEventProxy::HideMouse(void)
{
    if (!m_mouseHidden)
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    m_mouseHidden = true;
}
