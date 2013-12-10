/* Class TorcOverlayDecoder
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
#include <QPainter>

// Torc
#include "torclogging.h"
#include "videodecoder.h"
#include "torcqmlopengldefs.h"
#include "torcoverlaydecoder.h"

// FFmpeg/libbluray
extern "C"
{
#include "libavformat/avformat.h"
#include "libbluray/src/libbluray/decoders/overlay.h"
}

/*! \class TorcOverlayDecoder
 *  \brief Decodes overlays from different sources and displays them within the current scene graph.
 *
 * Overlays are currently divided into 3 planes; subtitles, graphics and menus, with the latter being rendered last
 * (i.e. on top). Subtitles are assumed to be any caption/text overlay, graphics any other non-interactive overlay
 * (in practice, these are currently limited to libbluray decoded subtitles) and menus any interactive video overlay
 * (such dvd and bluray menus and potentially MHEG etc).
 *
 * Currently unsupported subtitle/caption formats:
 * - raw text (see below)
 * - CEA608/708
 * - teletext
 *
 * \todo Add expiry for timed subtitles (quite important really:) )
 * \todo Add fix for odd AVSubtitles durations (set minimum/maximum duration).
 * \todo Raw text subtitles (or are these all handled via ASS now?).
 * \todo DVD palette.
 * \todo Handle BD_OVERLAY_HIDE.
 * \todo Handle RGBA<->BGRA swizzling for bluray overlay with OpenGL ES.
 * \todo Rationalise code duplicated between DecodeBDOverlay and DecodeBDARGBOverlay.
*/
TorcOverlayDecoder::TorcOverlayDecoder()
  : m_window(NULL),
    m_lastRootNode(NULL),
    m_subtitleNode(NULL),
    m_graphicsNode(NULL),
    m_menusNode(NULL)
{
#if defined(CONFIG_LIBASS) && CONFIG_LIBASS
    m_libassLibrary     = NULL;
    m_libassRenderer    = NULL;
    m_libassTrackNumber = 0;
    m_libassTrack       = NULL;
    m_libassFontCount   = 0;
    m_libassFrameSize   = QSizeF();
#endif // CONFIG_LIBASS
}

TorcOverlayDecoder::~TorcOverlayDecoder()
{
    ResetOverlayDecoder();
}

/*! \brief Set the window within which the decoder is operating.
 *
 * The QQuickWindow provides createTextureFromImage for creating the actual textures used for
 * displaying overlays.
*/
void TorcOverlayDecoder::SetOverlayWindow(QQuickWindow *Window)
{
    m_window = Window;
}

/*! \brief Clear all overlays and reset any overlay state.
 *
 * \not This is incomplete.
*/
void TorcOverlayDecoder::ResetOverlayDecoder(void)
{
#if defined(CONFIG_LIBASS) && CONFIG_LIBASS
    CleanupASSLibrary();
#endif // CONFIG_LIBASS
}

///brief Convert the given Image to a QSGTexture and add to Parent.
void TorcOverlayDecoder::RenderImage(QImage &Image, QRectF &Rect, QSGNode *Parent)
{
    if (!Parent)
        return;

    QSGTexture *texture = m_window->createTextureFromImage(Image);
    QSGSimpleTextureNode *node = new QSGSimpleTextureNode();
    node->setFiltering(QSGTexture::Linear);
    node->setTexture(texture);
    node->setRect(Rect);

    Parent->appendChildNode(node);
}

/*! \brief Decode the overlay described by Overlay and add any decoded images to the scene graph.
*/
void TorcOverlayDecoder::GetOverlayImages(VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                          const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts, QSGNode* Root)
{
    if (!m_window || !Root)
        return;

    if (m_lastRootNode && (m_lastRootNode != Root))
    {
        // if the node has been replaced, our overlay nodes will have been deleted as well
        LOG(VB_GENERAL, LOG_INFO, "Creating overlay nodes");
        m_subtitleNode = NULL;
        m_graphicsNode = NULL;
        m_menusNode    = NULL;
    }

    m_lastRootNode = Root;

    // ignore broken overlays
    if (!Overlay || (Overlay && !Overlay->IsValid()))
        return;

    // avoid stupidity
    QSizeF videosize = VideoSize.expandedTo(QSizeF(1, 1));

    // FFmpeg subtitles
    if (Overlay->m_bufferType == TorcVideoOverlayItem::FFmpegSubtitle)
    {
        DecodeAVSubtitle(Decoder, Overlay, VideoBounds, videosize, VideoFrameRate, VideoPts);
    }
    else if (Overlay->m_bufferType == TorcVideoOverlayItem::BDOverlay)
    {
        DecodeBDOverlay(Decoder, Overlay, VideoBounds, videosize, VideoFrameRate, VideoPts);
    }
    else if (Overlay->m_bufferType == TorcVideoOverlayItem::BDARGBOverlay)
    {
        DecodeBDARGBOverlay(Decoder, Overlay, VideoBounds, videosize, VideoFrameRate, VideoPts);
    }

    // ass state updated via DecodeAVSubtitle, now retrieve updates for current pts
    DecodeASSSubtitle(VideoBounds, VideoPts);
}

inline quint32 YUVAtoARGB(quint32 Value)
{
    int y  = (Value >> 0) & 0xff;
    int cr = (Value >> 8) & 0xff;
    int cb = (Value >> 16) & 0xff;
    int a  = (Value >> 24) & 0xff;
    int r  = qBound(0, int(y + 1.4022 * (cr - 128)), 0xff);
    int b  = qBound(0, int(y + 1.7710 * (cb - 128)), 0xff);
    int g  = qBound(0, int(1.7047 * y - (0.1952 * b) - (0.5647 * r)), 0xff);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/*! \brief Decode the bd_overlay_s commands and render the bluray overlay.
 *
 * The libbluray approach assumes a current overlay image that can be updated piecemeal; an approach
 * that does not convert conveniently to the Qt scenegraph. So we create one texture for the overlay
 * and update this as needed.
*/
void TorcOverlayDecoder::DecodeBDOverlay(VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                         const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts)
{
    if (!m_window || !m_lastRootNode)
        return;

    QList<bd_overlay_s*> *overlays = static_cast<QList<bd_overlay_s*> *>(Overlay->m_buffer);

    if (!overlays || (overlays && overlays->isEmpty()))
        return;

    // iterate over the overlays to ensure we have the overlay size and a consistent plane
    int plane = overlays->at(0)->plane;

    // TorcBlurayBuffer should ensure BD_OVERLAY_DRAW is the last or only command. If we see it, delete the overlay
    // and return
    bool close = false;

    for (int i = 0; i < overlays->size(); ++i)
    {
        bd_overlay_s *overlay = overlays->at(i);

        if (overlay->cmd == BD_OVERLAY_CLOSE)
            close = true;

        if (plane != overlay->plane)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid overlay plane");
            return;
        }

        if (overlay->cmd == BD_OVERLAY_INIT)
        {
            m_bdOverlayRects[overlay->plane] = QRectF(overlay->x, overlay->y, overlay->w, overlay->h);
            continue;
        }
    }

    // ensure BD_OVERLAY_INIT has been seen
    if (m_bdOverlayRects[plane].isEmpty() && !close)
    {
        LOG(VB_GENERAL, LOG_ERR, "Bluray overlay plane not initialised");
        return;
    }

    // ensure we have the overlay node (but don't create it if closing)
    QSGSimpleTextureNode *node = NULL;
    if (plane == 0 /*graphics*/)
    {
        node = m_graphicsNode;
        if (!node && !close)
        {
            node = m_graphicsNode = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);

            // insert it at the correct point in the hierarchy
            if (m_subtitleNode)
                m_lastRootNode->insertChildNodeAfter(node, m_subtitleNode);
            else
                m_lastRootNode->insertChildNodeAfter(node, m_lastRootNode->firstChild()/*video*/);
        }
    }
    else // menus
    {
        node = m_menusNode;
        if (!node && !close)
        {
            node = m_menusNode = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);

            // insert it at the correct point in the hierarchy
            if (m_graphicsNode)
                m_lastRootNode->insertChildNodeAfter(node, m_graphicsNode);
            else if (m_subtitleNode)
                m_lastRootNode->insertChildNodeAfter(node, m_subtitleNode);
            else
                m_lastRootNode->insertChildNodeAfter(node, m_lastRootNode->firstChild()/*video*/);
        }
    }

    if (!node)
    {
        if (!close)
            LOG(VB_GENERAL, LOG_ERR, "Failed to find overlay node");
        return;
    }

    // close the overlay
    if (close)
    {
        m_lastRootNode->removeChildNode(node);
        delete node;
        m_graphicsNode = plane == 0 ? NULL : m_graphicsNode;
        m_menusNode    = plane == 1 ? NULL : m_menusNode;
        return;
    }

    // create our image if needed
    if (!node->texture())
    {
        QImage image(m_bdOverlayRects[plane].width(), m_bdOverlayRects[plane].height(), QImage::Format_ARGB32);
        image.fill(0);
        node->setTexture(m_window->createTextureFromImage(image));

        qreal xfactor = VideoBounds.width() / m_bdOverlayRects[plane].width();
        qreal yfactor = VideoBounds.height() / m_bdOverlayRects[plane].height();
        QRectF destination(VideoBounds.left() + m_bdOverlayRects[plane].left() * xfactor,
                           VideoBounds.top()  + m_bdOverlayRects[plane].top() * yfactor,
                           m_bdOverlayRects[plane].width() * xfactor, m_bdOverlayRects[plane].height() * yfactor);
        node->setRect(destination);
    }

    QSGTexture *texture = node->texture();
    if (!texture)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve bluray overlay texture");
        return;
    }

    // and draw/clear etc
    for (int i = 0; i < overlays->size(); ++i)
    {
        bd_overlay_s *overlay = overlays->at(i);

        if (overlay->cmd == BD_OVERLAY_DRAW)
        {
            if (overlay->img && overlay->palette)
            {
                // convert the palette
                uint32_t *oldpalette = (uint32_t *)(overlay->palette);
                QVector<QRgb> newpalette;
                for (int i = 0; i < 256; i++)
                    newpalette.push_back(YUVAtoARGB(oldpalette[i]));

                const BD_PG_RLE_ELEM *raw = overlay->img;
                uint datasize = overlay->w * overlay->h;
                QByteArray data(datasize * 4, '\0');
                quint32* p = (quint32*)data.data();

                for (uint j = 0 ; j < datasize; raw++)
                    for (uint k = 0; k < raw->len; k++)
                        p[j++] = newpalette[raw->color];

                texture->bind();
                // NB as far as I can tell, all Qt textures are GL_TEXTURE_2D
                // FIXME this will need some swizzling for GL_RGBA on OpenGL ES
                glTexSubImage2D(GL_TEXTURE_2D, 0, overlay->x, overlay->y, overlay->w, overlay->h, GL_BGRA, GL_UNSIGNED_BYTE, p);
                node->markDirty(QSGNode::DirtyMaterial);
            }
        }
        else if (overlay->cmd == BD_OVERLAY_WIPE)
        {
            quint32 bgcolor = 0;
            if (overlay->palette)
                bgcolor = YUVAtoARGB(((uint32_t *)(overlay->palette))[0xff]);

            QVector<quint32> data(overlay->w * overlay->h, bgcolor);

            // see texture comments above for draw...
            texture->bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, overlay->x, overlay->y, overlay->w, overlay->h, GL_BGRA, GL_UNSIGNED_BYTE, data.data());
            node->markDirty(QSGNode::DirtyMaterial);
        }
        else if (overlay->cmd == BD_OVERLAY_CLEAR)
        {
            quint32 bgcolor = 0;
            if (overlay->palette)
                bgcolor = YUVAtoARGB(((uint32_t *)(overlay->palette))[0xff]);

            QVector<quint32> data(m_bdOverlayRects[plane].width() * m_bdOverlayRects[plane].height(), bgcolor);

            // see texture comments above for draw...
            texture->bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (int)m_bdOverlayRects[plane].width(), (int)m_bdOverlayRects[plane].height(), GL_BGRA, GL_UNSIGNED_BYTE, data.data());
            node->markDirty(QSGNode::DirtyMaterial);
        }
    }
}

/*! \brief Decode the bd_argb_overlay_s commands and render the bluray overlay.
 *
 * The libbluray approach assumes a current overlay image that can be updated piecemeal; an approach
 * that does not convert conveniently to the Qt scenegraph. So we create one texture for the overlay
 * and update this as needed.
*/
void TorcOverlayDecoder::DecodeBDARGBOverlay(VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                             const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts)
{
    if (!m_window || !m_lastRootNode)
        return;

    QList<bd_argb_overlay_s*> *overlays = static_cast<QList<bd_argb_overlay_s*> *>(Overlay->m_buffer);

    if (!overlays || (overlays && overlays->isEmpty()))
        return;

    // iterate over the overlays to ensure we have the overlay size and a consistent plane
    int plane = overlays->at(0)->plane;

    // TorcBlurayBuffer should ensure BD_ARGB_OVERLAY_DRAW is the last or only command. If we see it, delete the overlay
    // and return
    bool close = false;

    for (int i = 0; i < overlays->size(); ++i)
    {
        bd_argb_overlay_s *overlay = overlays->at(i);

        if (overlay->cmd == BD_ARGB_OVERLAY_CLOSE)
            close = true;

        if (plane != overlay->plane)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid overlay plane");
            return;
        }

        if (overlay->cmd == BD_ARGB_OVERLAY_INIT)
        {
            m_bdOverlayRects[overlay->plane] = QRectF(overlay->x, overlay->y, overlay->w, overlay->h);
            continue;
        }
    }

    // ensure BD_OVERLAY_INIT has been seen
    if (m_bdOverlayRects[plane].isEmpty() && !close)
    {
        LOG(VB_GENERAL, LOG_ERR, "Bluray overlay plane not initialised");
        return;
    }

    // ensure we have the overlay node (but don't create it if closing)
    QSGSimpleTextureNode *node = NULL;
    if (plane == 0 /*graphics*/)
    {
        node = m_graphicsNode;
        if (!node && !close)
        {
            node = m_graphicsNode = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);

            // insert it at the correct point in the hierarchy
            if (m_subtitleNode)
                m_lastRootNode->insertChildNodeAfter(node, m_subtitleNode);
            else
                m_lastRootNode->insertChildNodeAfter(node, m_lastRootNode->firstChild()/*video*/);
        }
    }
    else // menus
    {
        node = m_menusNode;
        if (!node && !close)
        {
            node = m_menusNode = new QSGSimpleTextureNode();
            node->setFiltering(QSGTexture::Linear);

            // insert it at the correct point in the hierarchy
            if (m_graphicsNode)
                m_lastRootNode->insertChildNodeAfter(node, m_graphicsNode);
            else if (m_subtitleNode)
                m_lastRootNode->insertChildNodeAfter(node, m_subtitleNode);
            else
                m_lastRootNode->insertChildNodeAfter(node, m_lastRootNode->firstChild()/*video*/);
        }
    }

    if (!node)
    {
        if (!close)
            LOG(VB_GENERAL, LOG_ERR, "Failed to find overlay node");
        return;
    }

    // close the overlay
    if (close)
    {
        m_lastRootNode->removeChildNode(node);
        delete node;
        m_graphicsNode = plane == 0 ? NULL : m_graphicsNode;
        m_menusNode    = plane == 1 ? NULL : m_menusNode;
        return;
    }

    // create our image if needed
    if (!node->texture())
    {
        QImage image(m_bdOverlayRects[plane].width(), m_bdOverlayRects[plane].height(), QImage::Format_ARGB32);
        image.fill(0);
        node->setTexture(m_window->createTextureFromImage(image));

        qreal xfactor = VideoBounds.width() / m_bdOverlayRects[plane].width();
        qreal yfactor = VideoBounds.height() / m_bdOverlayRects[plane].height();
        QRectF destination(VideoBounds.left() + m_bdOverlayRects[plane].left() * xfactor,
                           VideoBounds.top()  + m_bdOverlayRects[plane].top() * yfactor,
                           m_bdOverlayRects[plane].width() * xfactor, m_bdOverlayRects[plane].height() * yfactor);
        node->setRect(destination);
    }

    QSGTexture *texture = node->texture();
    if (!texture)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve bluray overlay texture");
        return;
    }

    // and draw/clear etc
    for (int i = 0; i < overlays->size(); ++i)
    {
        bd_argb_overlay_s *overlay = overlays->at(i);

        if (overlay->cmd == BD_ARGB_OVERLAY_DRAW)
        {
            if (overlay->argb)
            {
                // NB TorcBlurayBuffer should ensure w == stride
                if (overlay->w == overlay->stride)
                {
                    texture->bind();
                    glTexSubImage2D(GL_TEXTURE_2D, 0, overlay->x, overlay->y, overlay->w, overlay->h, GL_BGRA, GL_UNSIGNED_BYTE, overlay->argb);
                }
                else if (overlay->w < overlay->stride)
                {
                    texture->bind();
                    quint32* p = (quint32*)overlay->argb;
                    for (quint16 height = 0; height < overlay->h; height++, p += overlay->stride)
                        glTexSubImage2D(GL_TEXTURE_2D, 0, overlay->x, overlay->y + height, overlay->w, 1, GL_BGRA, GL_UNSIGNED_BYTE, p);
                }

                node->markDirty(QSGNode::DirtyMaterial);
            }
        }
    }
}

///brief Decode subtitles from an AVSubtitle structure and add to the scene graph.
void TorcOverlayDecoder::DecodeAVSubtitle(VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                          const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts)
{
    if (!m_lastRootNode)
        return;

    if (!m_subtitleNode)
    {
        m_subtitleNode = new QSGNode();
        m_lastRootNode->appendChildNode(m_subtitleNode);
    }

    // FIXME this may be too aggresive
    // remove old subtitles
    m_subtitleNode->removeAllChildNodes();

    const AVSubtitle *subtitle = static_cast<AVSubtitle*>(Overlay->m_buffer);

    if (subtitle && subtitle->num_rects > 0)
    {
        for (uint i = 0; i < subtitle->num_rects; ++i)
        {
            AVSubtitleRect* avrect = subtitle->rects[i];

            if (avrect->type == SUBTITLE_BITMAP && avrect->w > 0 && avrect->h > 0)
            {
                // convert the palette to QVector/QRgb
                QVector<QRgb> palette(avrect->nb_colors);
                memcpy(palette.data(), avrect->pict.data[1], avrect->nb_colors << 2);

                // initialise the QImage
                QImage temp((const uchar*)avrect->pict.data[0], avrect->w, avrect->h, avrect->pict.linesize[0], QImage::Format_Indexed8);
                temp.setColorTable(palette);

                // scaling factors between video size and video bounds size
                QSize videosize = QSize(avrect->display_w, avrect->display_h);
                qreal xfactor = VideoBounds.width() / videosize.width();
                qreal yfactor = VideoBounds.height() / videosize.height();

                // fix subtitle positioning for broken implementations
                int right  = avrect->x + avrect->w;
                int bottom = avrect->y + avrect->h;

                if (Overlay->m_fixRect || videosize.width() < right || videosize.height() < bottom)
                {
                    qreal sdheight = ((VideoFrameRate > 26.0f || VideoFrameRate < 24.0f) && bottom <= 480) ? 480 : 576;
                    qreal height   = ((videosize.height() <= sdheight) && (bottom <= sdheight)) ? sdheight :
                                     ((videosize.height() <= 720) && bottom <= 720) ? 720 : 1080;
                    qreal width    = ((videosize.width()  <= 720) &&(right <= 720)) ? 720 :
                                     ((videosize.width()  <= 1280) && (right <= 1280)) ? 1280 : 1920;

                    xfactor = VideoBounds.width() / width;
                    yfactor = VideoBounds.height() / height;
                }

                // calculate the subtitle position within the video frame
                QRectF destination(VideoBounds.left() + avrect->x * xfactor,
                                   VideoBounds.top()  + avrect->y * yfactor,
                                   avrect->w * xfactor, avrect->h * yfactor);

                // and add
                temp = temp.convertToFormat(QImage::Format_ARGB32);
                RenderImage(temp, destination, m_subtitleNode);
            }
            // not sure this is actually used anymore
            else if (avrect->type == SUBTITLE_TEXT && avrect->text)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("text sub: '%1'").arg(avrect->text));
            }
            else if (avrect->type == SUBTITLE_ASS && avrect->ass)
            {
                LOG(VB_PLAYBACK, LOG_INFO, QString("libass sub: '%1'").arg(avrect->ass));
#if defined(CONFIG_LIBASS) && CONFIG_LIBASS
                QSizeF size = VideoBounds.size();
                InitialiseASSTrack(Overlay->m_streamIndex, Decoder, size);
                if (m_libassTrack)
                    ass_process_data(m_libassTrack, avrect->ass, strlen(avrect->ass));
#endif // USING_LIBASS
            }
        }
    }
}

///brief Query the current libass track for new subtitles and add them to the scene graph.
void TorcOverlayDecoder::DecodeASSSubtitle(const QRectF &VideoBounds, qint64 VideoPts)
{
    if (!m_lastRootNode)
        return;

    if (!m_subtitleNode)
    {
        m_subtitleNode = new QSGNode();
        m_lastRootNode->appendChildNode(m_subtitleNode);
    }

#if defined(CONFIG_LIBASS) && CONFIG_LIBASS
    if (m_libassTrack && m_libassRenderer && VideoPts != AV_NOPTS_VALUE)
    {
        int changed = 0;
        ASS_Image *assimage = ass_render_frame(m_libassRenderer, m_libassTrack, VideoPts, &changed);
        if (!changed)
            return;

        // remove old subtitles
        m_subtitleNode->removeAllChildNodes();

        // and add new
        while (assimage)
        {
            if (assimage->w == 0 || assimage->h == 0)
            {
                assimage = assimage->next;
                continue;
            }

            uint8_t alpha = assimage->color & 0xFF;
            uint8_t blue  = assimage->color >> 8 & 0xFF;
            uint8_t green = assimage->color >> 16 & 0xFF;
            uint8_t red   = assimage->color >> 24 & 0xFF;

            if (alpha == 255)
            {
                assimage = assimage->next;
                continue;
            }

            QImage image(assimage->w, assimage->h, QImage::Format_ARGB32);
            image.fill(0x00000000);

            unsigned char *bitmap = assimage->bitmap;
            for (int y = 0; y < assimage->h; ++y)
            {
                for (int x = 0; x < assimage->w; ++x)
                {
                    uint8_t value = bitmap[x];
                    if (value)
                    {
                        uint32_t pixel = (value * (255 - alpha) / 255 << 24) | (red << 16) | (green << 8) | blue;
                        image.setPixel(x, y, pixel);
                    }
                }

                bitmap += assimage->stride;
            }

            QRectF destination(VideoBounds.left() + assimage->dst_x,
                               VideoBounds.top()  + assimage->dst_y,
                               assimage->w, assimage->h);
            RenderImage(image, destination, m_subtitleNode);

            assimage = assimage->next;
        }
    }
#else
    (void)VideoBounds;
    (void)VideoPts;
#endif // CONFIG_LIBASS
}

#if defined(CONFIG_LIBASS) && CONFIG_LIBASS

///brief Callback for libass logging events.
static void ASSLog(int Level, const char *Format, va_list VAList, void*)
{
    static QString ass("libass:");
    static const int length = 255;
    static QMutex lock;
    uint64_t mask  = VB_GENERAL;
    LogLevel level = LOG_INFO;

    switch (Level)
    {
        case 0: // fatal
            level = LOG_EMERG;
            break;
        case 1:
            level = LOG_ERR;
            break;
        case 2:
            level = LOG_WARNING;
            break;
        case 4:
            level = LOG_INFO;
            break;
        case 6:
        case 7: // debug
            level = LOG_DEBUG;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(mask, level))
        return;

    lock.lock();

    char str[length + 1];
    int bytes = vsnprintf(str, length + 1, Format, VAList);

    if (bytes > length)
        str[length-1] = '\n';

    ass += QString(str);
    if (ass.endsWith("\n"))
    {
        LOG(mask, level, ass.trimmed());
        ass.truncate(0);
    }

    lock.unlock();
}

///brief Setup the libass library and initialise the renderer.
bool TorcOverlayDecoder::InitialiseASSLibrary(void)
{
    if (m_libassLibrary && m_libassRenderer)
        return true;

    if (!m_libassLibrary)
    {
        m_libassLibrary = ass_library_init();
        if (!m_libassLibrary)
            return false;

        ass_set_message_cb(m_libassLibrary, ASSLog, NULL);
        ass_set_extract_fonts(m_libassLibrary, true);

        LOG(VB_PLAYBACK, LOG_INFO, "Initialised libass object.");
    }

    if (!m_libassRenderer)
    {
        m_libassRenderer = ass_renderer_init(m_libassLibrary);
        if (!m_libassRenderer)
            return false;

        ass_set_fonts(m_libassRenderer, NULL, "sans-serif", 1, NULL, 1);
        ass_set_hinting(m_libassRenderer, ASS_HINTING_LIGHT);
        ass_set_margins(m_libassRenderer, 0, 0, 0, 0);
        ass_set_use_margins(m_libassRenderer, true);
        ass_set_font_scale(m_libassRenderer, 1.0);

        LOG(VB_PLAYBACK, LOG_INFO, "Initialised libass renderer.");
    }

    return true;
}

///brief Load ASS fonts embedded in the media stream.
void TorcOverlayDecoder::LoadASSFonts(VideoDecoder *Decoder)
{
    if (!m_libassLibrary || !Decoder)
        return;

    uint attachments = Decoder->GetStreamCount(StreamTypeAttachment);
    if (m_libassFontCount == attachments)
        return;

    ass_clear_fonts(m_libassLibrary);
    m_libassFontCount = 0;

    for (uint i = 0; i < attachments; ++i)
    {
        QString filename;
        QByteArray font = Decoder->GetAttachment(i, AV_CODEC_ID_TTF, filename);
        if (!font.isEmpty())
        {
            ass_add_font(m_libassLibrary, filename.toLocal8Bit().data(), font.data(), font.size());

            LOG(VB_GENERAL, LOG_INFO, QString("Retrieved font '%1'").arg(filename));
        }

        // strictly speaking this will include other attachment types, but it is just
        // acting as a guard against stream count changes etc.
        m_libassFontCount++;
    }
}

///brief Deinitialise all ass library objects.
void TorcOverlayDecoder:: CleanupASSLibrary(void)
{
    CleanupASSTrack();

    if (m_libassRenderer)
        ass_renderer_done(m_libassRenderer);
    m_libassRenderer = NULL;

    if (m_libassLibrary)
    {
        ass_clear_fonts(m_libassLibrary);
        m_libassFontCount = 0;
        ass_library_done(m_libassLibrary);
    }
    m_libassLibrary = NULL;

    m_libassFrameSize = QSizeF();
}

///brief Initialise individual ASS track.
void TorcOverlayDecoder:: InitialiseASSTrack(int Track, VideoDecoder *Decoder, QSizeF &Bounds)
{
    if (!InitialiseASSLibrary())
        return;

    if (m_libassFrameSize != Bounds)
    {
        m_libassFrameSize = Bounds;
        ass_set_frame_size(m_libassRenderer, (int)m_libassFrameSize.width(), (int)m_libassFrameSize.height());
    }

    if (Track == m_libassTrackNumber && m_libassTrack)
        return;

    CleanupASSTrack();

    m_libassTrack    = ass_new_track(m_libassLibrary);
    m_libassTrackNumber = Track;

    LoadASSFonts(Decoder);

    if (Decoder)
    {
        QByteArray header = Decoder->GetSubtitleHeader(Track);
        if (!header.isNull())
            ass_process_codec_private(m_libassTrack, header.data(), header.size());
    }
}

///brief Deinitialise current ASS track.
void TorcOverlayDecoder:: CleanupASSTrack(void)
{
    if (m_libassTrack)
        ass_free_track(m_libassTrack);
    m_libassTrack = NULL;
}
#endif // CONFIG_LIBASS
