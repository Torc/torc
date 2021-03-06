/* Class TorcQMLMediaElement
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
#include <QImage>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>


// Torc
#include "torclogging.h"
#include "torccoreutils.h"
#include "torclocalcontext.h"
#include "torcdecoder.h"
#include "videocolourspace.h"
#include "torcsgvideoplayer.h"
#include "torcsgvideoprovider.h"
#include "torcqmleventproxy.h"
#include "torcqmlmediaelement.h"

/*! \class TorcQMLMediaElement
 *  \brief A QtQuick media widget
 *
 * TorcQMLMediaElement is the media widget for embedding Torc playback functionality in
 * QML scenes.
 *
 * \sa TorcSGVideoPlayer
 * \sa TorcSGVideoProvider
 * \sa TorcPlayer
 * \sa TorcPlayerInterface
 *
 * \todo Add playabck properties.
 * \todo Optimise or do away with the refresh timer.
*/

#define BLACKLIST QString("InitialisePlayer,update")

TorcQMLMediaElement::TorcQMLMediaElement(QQuickItem *Parent)
  : QQuickItem(Parent),
    TorcPlayerInterface(this, TorcQMLMediaElement::staticMetaObject, BLACKLIST, false),
    m_videoColourSpace(new VideoColourSpace(AVCOL_SPC_UNSPECIFIED)),
    m_videoProvider(NULL),
    m_videoPlayer(NULL),
    m_refreshTimer(NULL),
    m_textureStale(false),
    m_geometryStale(true)
{
    // inform scenegraph that we need to be drawn
    setFlag(ItemHasContents, true);

    // listen for relevant Torc events
    gLocalContext->AddObserver(this);

    // create the player
    InitialisePlayer();

    // start the update timer
    m_refreshTimer = new QTimer();
    m_refreshTimer->setTimerType(Qt::PreciseTimer);
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(update()));
    m_refreshTimer->start(1000 / 120);

    // testing only - remove
    SetURI("/Users/mark/Dropbox/Videos/pioneer.ts");

    // request mouse events
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
}

TorcQMLMediaElement::~TorcQMLMediaElement()
{
    // stop listening for events
    gLocalContext->RemoveObserver(this);

    // stop the update timer
    delete m_refreshTimer;

    // delete the player
    if (m_player)
        m_player->disconnect();

    delete m_player;
    m_player = NULL;

    // delete the colourspace
    delete m_videoColourSpace;
    m_videoColourSpace = NULL;
}

/*! \brief Refresh the media being played.
 *
 * \note updatePaintNode operates in the Qt render thread and all scene graph (SG) and related
 *       objects must be created AND destroyed from the render thread. Do not reference or use them
 *       elsewhere.
*/
QSGNode* TorcQMLMediaElement::updatePaintNode(QSGNode *Node, UpdatePaintNodeData*)
{
    // handle callback requests that need to processed while the OpenGLContext is current
    TorcQMLEventProxy *proxy = dynamic_cast<TorcQMLEventProxy*>(gLocalContext->GetUIObject());
    if (proxy)
        proxy->ProcessCallbacks();

    // root node is a plain QSGNode
    QSGNode *root = Node;
    if (!root)
    {
        root = new QSGNode();
        m_geometryStale = true;
    }

    // create video texture provider
    if (!m_videoProvider)
    {
        // ensure we cleanup when the QOpenGLContext is deleted
        if (!connect(window()->openglContext(), SIGNAL(aboutToBeDestroyed()), this, SLOT(Cleanup()), Qt::DirectConnection))
            LOG(VB_GENERAL, LOG_ERR, "Error connecting to QOpenGLContext");

        // create the provider
        m_videoProvider = new TorcSGVideoProvider(m_videoColourSpace);

        // listen for texture changes
        connect(dynamic_cast<QSGTextureProvider*>(m_videoProvider), SIGNAL(textureChanged()), this, SLOT(TextureChanged()), Qt::DirectConnection);

        // tell the player about the provider
        if (m_videoPlayer)
        {
            m_videoPlayer->SetVideoProvider(m_videoProvider);
            m_videoPlayer->SetWindow(static_cast<QQuickWindow*>(proxy->GetWindow()));

            // testing - remove
            m_videoPlayer->PlayMedia(m_uri, false);
        }
    }

    // video node should always be the first in the list of child nodes
    QSGSimpleTextureNode *video = NULL;
    if (root->childCount() == 0 && m_videoProvider)
    {
        video = new QSGSimpleTextureNode();
        video->setTexture(m_videoProvider->texture());
        root->appendChildNode(video);

        m_textureStale      = false;
        m_geometryStale = true;
    }
    else
    {
        video = static_cast<QSGSimpleTextureNode*>(root->firstChild());
    }

    // NB although the player resides in the main thread, the main thread is blocked while the scenegraph is updated,
    // so this should be thread safe.
    bool dirtyframe = false;

    // update video
    if (m_player)
        dirtyframe = m_player->Refresh(TorcCoreUtils::GetMicrosecondCount(), boundingRect().size(), true);

    if (video && m_videoProvider)
    {
        if (m_textureStale)
        {
            video->setTexture(NULL);
            video->setTexture(m_videoProvider->texture());
            m_textureStale = false;
        }

        if (m_geometryStale || m_videoProvider->GeometryIsDirty())
        {
            m_mediaRect = m_videoProvider->GetGeometry(boundingRect(), 1.0f);
            video->setRect(m_mediaRect);
            m_geometryStale = false;
            video->markDirty(QSGNode::DirtyGeometry);
        }

        if (dirtyframe)
            video->markDirty(QSGNode::DirtyMaterial);
    }

    // update overlays
    if (m_videoPlayer)
        m_videoPlayer->RefreshOverlays(root);

    return root;
}

void TorcQMLMediaElement::TextureChanged(void)
{
    m_textureStale = true;
}

void TorcQMLMediaElement::geometryChanged(const QRectF &NewGeometry, const QRectF &OldGeometry)
{
    if (NewGeometry != OldGeometry)
    {
        if (m_videoPlayer)
            m_videoPlayer->SetParentGeometry(NewGeometry);
        m_geometryStale = true;
    }

    QQuickItem::geometryChanged(NewGeometry, OldGeometry);
}

bool TorcQMLMediaElement::event(QEvent *Event)
{
    if (!Event)
        return false;

    if (TorcEvent::TorcEventType == Event->type())
        return HandleEvent(Event);

    return false;
}

void TorcQMLMediaElement::mousePressEvent(QMouseEvent *Event)
{
    if (Event && m_videoPlayer && m_mediaRect.contains(Event->localPos()))
        m_videoPlayer->event(Event);
}

void TorcQMLMediaElement::mouseReleaseEvent(QMouseEvent *Event)
{
    if (Event && m_videoPlayer && m_mediaRect.contains(Event->localPos()))
        m_videoPlayer->event(Event);
}

void TorcQMLMediaElement::mouseMoveEvent(QMouseEvent *Event)
{
    if (Event && m_videoPlayer && m_mediaRect.contains(Event->localPos()))
        m_videoPlayer->event(Event);
}

void TorcQMLMediaElement::wheelEvent(QWheelEvent *Event)
{
    if (Event && m_videoPlayer)
        m_videoPlayer->event(Event);
}

void TorcQMLMediaElement::hoverMoveEvent(QHoverEvent *Event)
{
    if (Event && m_videoPlayer && m_mediaRect.contains(Event->posF()))
        m_videoPlayer->event(Event);
}

void TorcQMLMediaElement::mouseDoubleClickEvent(QMouseEvent *Event)
{
    if (Event && m_videoPlayer && m_mediaRect.contains(Event->localPos()))
        m_videoPlayer->event(Event);
}

void TorcQMLMediaElement::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

///\brief Creates a TorcPlayer instance suitable for presenting video as well as audio.
bool TorcQMLMediaElement::InitialisePlayer(void)
{
    if (!m_player)
        m_player = TorcPlayer::Create(this, TorcPlayer::UserFacing, TorcDecoder::DecodeVideo | TorcDecoder::DecodeAudio);

    if (m_player)
    {
        connect(m_player, SIGNAL(StateChanged(TorcPlayer::PlayerState)),
                this, SLOT(PlayerStateChanged(TorcPlayer::PlayerState)));

        m_videoPlayer = static_cast<TorcSGVideoPlayer*>(m_player);
        if (m_videoPlayer)
            m_videoPlayer->SetParentGeometry(boundingRect());

        LOG(VB_GENERAL, LOG_INFO, "Player created (UI video and audio)");
    }

    return m_player;
}

void TorcQMLMediaElement::PlayerStateChanged(TorcPlayer::PlayerState State)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Player state: %1").arg(TorcPlayer::StateToString(State)));
}

/*! \brief Delete any objects created in the Qt render thread.
 *
 * This method is usally triggered when the QOpenGLContext is about to be deleted.
*/
void TorcQMLMediaElement::Cleanup(void)
{
    // the video provider is created in the Qt render thread and must be deleted there
    if (m_videoPlayer)
        m_videoPlayer->SetVideoProvider(NULL);

    delete m_videoProvider;
    m_videoProvider = NULL;
}
