/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qquickabstractitemview_p.h"
#include "qquickabstractitemview_p_p.h"

#include <QtQml/private/qqmlglobal_p.h>
#include <QtQml/qqmlcontext.h>

QT_BEGIN_NAMESPACE

// Default cacheBuffer for all views.
#ifndef QML_VIEW_DEFAULTCACHEBUFFER
#define QML_VIEW_DEFAULTCACHEBUFFER 320
#endif

FxAbstractViewItem::FxAbstractViewItem(QQuickItem *i, QQuickAbstractItemView *v, bool own, QQuickAbstractItemViewAttached *attached)
    : item(i)
    , view(v)
    , transitionableItem(0)
    , attached(attached)
    , ownItem(own)
    , releaseAfterTransition(false)
    , trackGeom(false)
{
    if (attached) // can be null for default components (see createComponentItem)
        attached->setView(view);
}

FxAbstractViewItem::~FxAbstractViewItem()
{
    delete transitionableItem;
    if (ownItem && item) {
        trackGeometry(false);
        item->setParentItem(0);
        item->deleteLater();
        item = 0;
    }
}

qreal FxAbstractViewItem::itemX() const
{
    return transitionableItem ? transitionableItem->itemX() : (item ? item->x() : 0);
}

qreal FxAbstractViewItem::itemY() const
{
    return transitionableItem ? transitionableItem->itemY() : (item ? item->y() : 0);
}

void FxAbstractViewItem::moveTo(const QPointF &pos, bool immediate)
{
    if (transitionableItem)
        transitionableItem->moveTo(pos, immediate);
    else if (item)
        item->setPosition(pos);
}

void FxAbstractViewItem::setVisible(bool visible)
{
    if (!visible && transitionableItem && transitionableItem->transitionScheduledOrRunning())
        return;
    if (item)
        QQuickItemPrivate::get(item)->setCulled(!visible);
}

void FxAbstractViewItem::trackGeometry(bool track)
{
    if (track) {
        if (!trackGeom) {
            if (item) {
                QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
                itemPrivate->addItemChangeListener(QQuickAbstractItemViewPrivate::get(view), QQuickItemPrivate::Geometry);
            }
            trackGeom = true;
        }
    } else {
        if (trackGeom) {
            if (item) {
                QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
                itemPrivate->removeItemChangeListener(QQuickAbstractItemViewPrivate::get(view), QQuickItemPrivate::Geometry);
            }
            trackGeom = false;
        }
    }
}

QQuickItemViewTransitioner::TransitionType FxAbstractViewItem::scheduledTransitionType() const
{
    return transitionableItem ? transitionableItem->nextTransitionType : QQuickItemViewTransitioner::NoTransition;
}

bool FxAbstractViewItem::transitionScheduledOrRunning() const
{
    return transitionableItem ? transitionableItem->transitionScheduledOrRunning() : false;
}

bool FxAbstractViewItem::transitionRunning() const
{
    return transitionableItem ? transitionableItem->transitionRunning() : false;
}

bool FxAbstractViewItem::isPendingRemoval() const
{
    return transitionableItem ? transitionableItem->isPendingRemoval() : false;
}

void FxAbstractViewItem::transitionNextReposition(QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, bool asTarget)
{
    if (!transitioner)
        return;
    if (!transitionableItem)
        transitionableItem = new QQuickItemViewTransitionableItem(item);
    transitioner->transitionNextReposition(transitionableItem, type, asTarget);
}

bool FxAbstractViewItem::prepareTransition(QQuickItemViewTransitioner *transitioner, const QRectF &viewBounds)
{
    return transitionableItem ? transitionableItem->prepareTransition(transitioner, index, viewBounds) : false;
}

void FxAbstractViewItem::startTransition(QQuickItemViewTransitioner *transitioner)
{
    if (transitionableItem)
        transitionableItem->startTransition(transitioner, index);
}

QQuickAbstractItemViewPrivate::QQuickAbstractItemViewPrivate()
    : itemCount(0),
      buffer(QML_VIEW_DEFAULTCACHEBUFFER),
      bufferMode(BufferBefore | BufferAfter),
      layoutDirection(Qt::LeftToRight),
      verticalLayoutDirection(QQuickItemView::TopToBottom),
      moveReason(Other),
      visibleIndex(0),
      currentIndex(-1),
      requestedIndex(-1),
      highlightComponent(nullptr),
      highlightRange(QQuickItemView::NoHighlightRange),
      highlightMoveDuration(150),
      transitioner(nullptr),
      minExtent(0),
      maxExtent(0),
      ownModel(false),
      wrap(false),
      keyNavigationEnabled(true),
      explicitKeyNavigationEnabled(false),
      inLayout(false),
      inViewportMoved(false),
      forceLayout(false),
      currentIndexCleared(false),
      haveHighlightRange(false),
      autoHighlight(true),
      fillCacheBuffer(false),
      inRequest(false),
      runDelayedRemoveTransition(false),
      delegateValidated(false)
{
    bufferPause.addAnimationChangeListener(this, QAbstractAnimationJob::Completion);
    bufferPause.setLoopCount(1);
    bufferPause.setDuration(16);
}

QQuickAbstractItemViewPrivate::~QQuickAbstractItemViewPrivate()
{
    if (transitioner)
        transitioner->setChangeListener(nullptr);
    delete transitioner;
}

void QQuickAbstractItemViewPrivate::init()
{
}

void QQuickAbstractItemViewPrivate::clear()
{
}

void QQuickAbstractItemViewPrivate::updateViewport()
{
}

void QQuickAbstractItemViewPrivate::animationFinished(QAbstractAnimationJob *)
{
    Q_Q(QQuickAbstractItemView);
    fillCacheBuffer = true;
    q->polish();
}

QQuickItem *QQuickAbstractItemViewPrivate::createComponentItem(QQmlComponent *component, qreal zValue, bool createDefault) const
{
    Q_Q(const QQuickAbstractItemView);

    QQuickItem *item = 0;
    if (component) {
        QQmlContext *creationContext = component->creationContext();
        QQmlContext *context = new QQmlContext(
                creationContext ? creationContext : qmlContext(q));
        QObject *nobj = component->beginCreate(context);
        if (nobj) {
            QQml_setParent_noEvent(context, nobj);
            item = qobject_cast<QQuickItem *>(nobj);
            if (!item)
                delete nobj;
        } else {
            delete context;
        }
    } else if (createDefault) {
        item = new QQuickItem;
    }
    if (item) {
        if (qFuzzyIsNull(item->z()))
            item->setZ(zValue);
        QQml_setParent_noEvent(item, q->contentItem());
        item->setParentItem(q->contentItem());
    }
    if (component)
        component->completeCreate();
    return item;
}

void QQuickAbstractItemViewPrivate::createTransitioner()
{
    if (!transitioner) {
        transitioner = new QQuickItemViewTransitioner;
        transitioner->setChangeListener(this);
    }
}

QQuickAbstractItemView::QQuickAbstractItemView(QQuickFlickablePrivate &dd, QQuickItem *parent)
    : QQuickFlickable(dd, parent)
{
}

QQuickAbstractItemView::~QQuickAbstractItemView()
{
}

bool QQuickAbstractItemView::isWrapEnabled() const
{
    Q_D(const QQuickAbstractItemView);
    return d->wrap;
}

void QQuickAbstractItemView::setWrapEnabled(bool wrap)
{
    Q_D(QQuickAbstractItemView);
    if (d->wrap == wrap)
        return;
    d->wrap = wrap;
    emit keyNavigationWrapsChanged();
}

bool QQuickAbstractItemView::isKeyNavigationEnabled() const
{
    Q_D(const QQuickAbstractItemView);
    return d->explicitKeyNavigationEnabled ? d->keyNavigationEnabled : d->interactive;
}

void QQuickAbstractItemView::setKeyNavigationEnabled(bool keyNavigationEnabled)
{
    // TODO: default binding to "interactive" can be removed in Qt 6; it only exists for compatibility reasons.
    Q_D(QQuickAbstractItemView);
    const bool wasImplicit = !d->explicitKeyNavigationEnabled;
    if (wasImplicit)
        QObject::disconnect(this, &QQuickFlickable::interactiveChanged, this, &QQuickAbstractItemView::keyNavigationEnabledChanged);

    d->explicitKeyNavigationEnabled = true;

    // Ensure that we emit the change signal in case there is no different in value.
    if (d->keyNavigationEnabled != keyNavigationEnabled || wasImplicit) {
        d->keyNavigationEnabled = keyNavigationEnabled;
        emit keyNavigationEnabledChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::populateTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->populateTransition : 0;
}

void QQuickAbstractItemView::setPopulateTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->populateTransition != transition) {
        d->transitioner->populateTransition = transition;
        emit populateTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::addTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->addTransition : 0;
}

void QQuickAbstractItemView::setAddTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->addTransition != transition) {
        d->transitioner->addTransition = transition;
        emit addTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::addDisplacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->addDisplacedTransition : 0;
}

void QQuickAbstractItemView::setAddDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->addDisplacedTransition != transition) {
        d->transitioner->addDisplacedTransition = transition;
        emit addDisplacedTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::moveTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->moveTransition : 0;
}

void QQuickAbstractItemView::setMoveTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->moveTransition != transition) {
        d->transitioner->moveTransition = transition;
        emit moveTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::moveDisplacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->moveDisplacedTransition : 0;
}

void QQuickAbstractItemView::setMoveDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->moveDisplacedTransition != transition) {
        d->transitioner->moveDisplacedTransition = transition;
        emit moveDisplacedTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::removeTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->removeTransition : 0;
}

void QQuickAbstractItemView::setRemoveTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->removeTransition != transition) {
        d->transitioner->removeTransition = transition;
        emit removeTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::removeDisplacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->removeDisplacedTransition : 0;
}

void QQuickAbstractItemView::setRemoveDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->removeDisplacedTransition != transition) {
        d->transitioner->removeDisplacedTransition = transition;
        emit removeDisplacedTransitionChanged();
    }
}

QQuickTransition *QQuickAbstractItemView::displacedTransition() const
{
    Q_D(const QQuickAbstractItemView);
    return d->transitioner ? d->transitioner->displacedTransition : 0;
}

void QQuickAbstractItemView::setDisplacedTransition(QQuickTransition *transition)
{
    Q_D(QQuickAbstractItemView);
    d->createTransitioner();
    if (d->transitioner->displacedTransition != transition) {
        d->transitioner->displacedTransition = transition;
        emit displacedTransitionChanged();
    }
}

void QQuickAbstractItemView::initItem(int, QObject *object)
{
    QQuickItem* item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        if (qFuzzyIsNull(item->z()))
            item->setZ(1);
        item->setParentItem(contentItem());
        QQuickItemPrivate::get(item)->setCulled(true);
    }
}

void QQuickAbstractItemView::destroyingItem(QObject *object)
{
    Q_D(QQuickAbstractItemView);
    QQuickItem* item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        item->setParentItem(0);
        d->unrequestedItems.remove(item);
    }
}

QT_END_NAMESPACE
