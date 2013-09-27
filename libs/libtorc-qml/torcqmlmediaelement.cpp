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
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGSimpleTextureNode>

// Torc
#include "torclogging.h"
#include "torcloggingimp.h"
#include "torcqmlmediaelement.h"

TorcQMLMediaElement::TorcQMLMediaElement(QQuickItem *Parent)
  : QQuickItem(Parent)
{
    setFlag(ItemHasContents, true);
}

TorcQMLMediaElement::~TorcQMLMediaElement()
{
}

QSGNode* TorcQMLMediaElement::updatePaintNode(QSGNode *Node, UpdatePaintNodeData*)
{
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode*>(Node);

    if (!node)
    {
        QImage image("/Users/mark/Dropbox/dice.png");
        QSGTexture *texture = window()->createTextureFromImage(image);

        node = new QSGSimpleTextureNode();
        node->setTexture(texture);
    }

    node->setRect(boundingRect());
    return node;
}
