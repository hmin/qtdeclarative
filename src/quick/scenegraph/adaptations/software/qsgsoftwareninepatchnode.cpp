/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgsoftwareninepatchnode_p.h"
#include "qsgsoftwarepixmaptexture_p.h"
#include "qsgsoftwareimagenode_p.h"

QT_BEGIN_NAMESPACE

QSGSoftwareNinePatchNode::QSGSoftwareNinePatchNode()
{
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);
}

void QSGSoftwareNinePatchNode::setTexture(QSGTexture *texture)
{
    QSGSoftwarePixmapTexture *pt = qobject_cast<QSGSoftwarePixmapTexture*>(texture);
    if (!pt) {
        qWarning() << "Image used with invalid texture format.";
        return;
    }
    m_pixmap = pt->pixmap();
    markDirty(DirtyMaterial);
}

void QSGSoftwareNinePatchNode::setBounds(const QRectF &bounds)
{
    if (m_bounds == bounds)
        return;

    m_bounds = bounds;
    markDirty(DirtyGeometry);
}

void QSGSoftwareNinePatchNode::setDevicePixelRatio(qreal ratio)
{
    if (m_pixelRatio == ratio)
        return;

    m_pixelRatio = ratio;
    markDirty(DirtyGeometry);
}

void QSGSoftwareNinePatchNode::setPadding(qreal left, qreal top, qreal right, qreal bottom)
{
    QMargins margins(qRound(left), qRound(top), qRound(right), qRound(bottom));
    if (m_margins == margins)
        return;

    m_margins = QMargins(qRound(left), qRound(top), qRound(right), qRound(bottom));
    markDirty(DirtyGeometry);
}

void QSGSoftwareNinePatchNode::update()
{
}

void QSGSoftwareNinePatchNode::paint(QPainter *painter)
{
    if (m_margins.isNull())
        painter->drawPixmap(m_bounds, m_pixmap, QRectF(0, 0, m_pixmap.width(), m_pixmap.height()));
    else
        QSGSoftwareHelpers::qDrawBorderPixmap(painter, m_bounds.toRect(), m_margins, m_pixmap, QRect(0, 0, m_pixmap.width(), m_pixmap.height()),
                                              m_margins, Qt::StretchTile, QSGSoftwareHelpers::QDrawBorderPixmap::DrawingHints(0));
}

QRectF QSGSoftwareNinePatchNode::bounds() const
{
    return m_bounds;
}

QT_END_NAMESPACE