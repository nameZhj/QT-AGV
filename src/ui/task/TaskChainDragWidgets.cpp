/**
 * @file    TaskChainDragWidgets.cpp
 * @layer   GUILayer
 * @brief   Implementation of draggable list widgets, reordering filters, and lock overlay.
 * @deps    TaskChainDragWidgets.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainDragWidgets.h"
#include "TaskChainStyle.h"
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QAbstractButton>
#include <QTextEdit>
#include <QContextMenuEvent>
#include <QWheelEvent>

// ==============================================================================
// === [Section: DraggableListWidget] ===
// ==============================================================================

DraggableListWidget::DraggableListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void DraggableListWidget::startDrag(Qt::DropActions)
{
    QListWidgetItem *item = currentItem();
    if (!item) return;
    auto *drag = new QDrag(this);
    auto *mime = new QMimeData;
    mime->setText(QStringLiteral("action:%1").arg(item->text()));
    mime->setData("application/x-taskchain-action", item->text().toUtf8());
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

// ==============================================================================
// === [Section: DropTargetListWidget] ===
// ==============================================================================

DropTargetListWidget::DropTargetListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    setDefaultDropAction(Qt::MoveAction);
}

void DropTargetListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-taskchain-action") ||
        event->mimeData()->hasFormat("application/x-taskchain-group-reorder") ||
        event->mimeData()->hasFormat("application/x-taskchain-task-reorder")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropTargetListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-taskchain-action") ||
        event->mimeData()->hasFormat("application/x-taskchain-group-reorder") ||
        event->mimeData()->hasFormat("application/x-taskchain-task-reorder")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropTargetListWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-taskchain-group-reorder")) {
        int fromRow = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-group-reorder")).toInt();
        auto *dropItem = itemAt(event->position().toPoint());
        int toRow = dropItem ? row(dropItem) : count() - 1;
        if (onGroupReordered) onGroupReordered(fromRow, toRow);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasFormat("application/x-taskchain-task-reorder")) {
        int fromRow = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-task-reorder")).toInt();
        auto *dropItem = itemAt(event->position().toPoint());
        int toRow = dropItem ? row(dropItem) : count() - 1;
        if (onTaskReordered) onTaskReordered(fromRow, toRow);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasFormat("application/x-taskchain-action")) {
        QString actionName = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-action"));
        auto *dropItem = itemAt(event->position().toPoint());
        int dropRow = dropItem ? row(dropItem) : -1;
        if (onActionDropped) onActionDropped(actionName, dropRow);
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

// ==============================================================================
// === [Section: CardLongPressDragFilter] ===
// ==============================================================================

CardLongPressDragFilter::CardLongPressDragFilter(const QString &mimeType, int index, QWidget *container, QListWidget *parentList)
    : QObject(container), m_mimeType(mimeType), m_index(index), m_container(container), m_parentList(parentList)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &CardLongPressDragFilter::onLongPressTimeout);
}

void CardLongPressDragFilter::selectInParentList()
{
    QListWidget *list = m_parentList;
    if (!list && m_container) {
        QWidget *p = m_container->parentWidget();
        while (p && !qobject_cast<QListWidget*>(p)) {
            p = p->parentWidget();
        }
        list = qobject_cast<QListWidget*>(p);
    }
    if (list) {
        if (list->currentRow() != m_index) {
            list->setCurrentRow(m_index);
        }
        list->setFocus();
    }
    if (onSelected) {
        onSelected(m_index);
    }
}

bool CardLongPressDragFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            selectInParentList();
            QWidget *child = m_container->childAt(me->pos());
            if (child && (qobject_cast<QAbstractButton*>(child) || qobject_cast<QTextEdit*>(child))) {
                return QObject::eventFilter(watched, event);
            }
            m_pressPos = me->pos();
            m_longPressed = false;
            m_dragStarted = false;
            m_timer->start(300);
        } else if (me->button() == Qt::RightButton) {
            selectInParentList();
        }
    } else if (event->type() == QEvent::ContextMenu) {
        auto *ce = static_cast<QContextMenuEvent*>(event);
        selectInParentList();
        QWidget *p = m_container->parentWidget();
        while (p && !qobject_cast<QListWidget*>(p)) {
            p = p->parentWidget();
        }
        if (auto *list = qobject_cast<QListWidget*>(p)) {
            emit list->customContextMenuRequested(list->mapFromGlobal(ce->globalPos()));
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->buttons() & Qt::LeftButton) {
            int dist = (me->pos() - m_pressPos).manhattanLength();
            if (dist > 8 && m_timer->isActive()) {
                m_timer->stop();
            }
            if (m_longPressed && !m_dragStarted && dist > 3) {
                m_dragStarted = true;
                startDrag();
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_timer->stop();
        if (m_longPressed && m_container) {
            m_container->setStyleSheet(m_origStyle);
            m_container->unsetCursor();
        }
        m_longPressed = false;
    }
    return QObject::eventFilter(watched, event);
}

void CardLongPressDragFilter::onLongPressTimeout()
{
    m_longPressed = true;
    if (m_container) {
        m_origStyle = m_container->styleSheet();
        m_container->setStyleSheet(TaskChainStyle::kCardLongPressDrag);
        m_container->setCursor(Qt::ClosedHandCursor);
    }
}

void CardLongPressDragFilter::startDrag()
{
    if (!m_container) return;
    auto *drag = new QDrag(m_container);
    auto *mime = new QMimeData;
    mime->setData(m_mimeType, QByteArray::number(m_index));
    drag->setMimeData(mime);

    QPixmap pix = m_container->grab();
    QPixmap transparentPix(pix.size());
    transparentPix.fill(Qt::transparent);
    QPainter p(&transparentPix);
    p.setOpacity(0.75);
    p.drawPixmap(0, 0, pix);
    p.end();

    drag->setPixmap(transparentPix);
    drag->setHotSpot(m_pressPos);
    drag->exec(Qt::MoveAction);

    if (m_container) {
        m_container->setStyleSheet(m_origStyle);
        m_container->unsetCursor();
    }
    m_longPressed = false;
    m_dragStarted = false;
}

// ==============================================================================
// === [Section: TaskChainLockOverlay] ===
// ==============================================================================

TaskChainLockOverlay::TaskChainLockOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ForbiddenCursor);
}

void TaskChainLockOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(10, 14, 20, 45));
}

void TaskChainLockOverlay::mousePressEvent(QMouseEvent *e) { e->accept(); }
void TaskChainLockOverlay::mouseReleaseEvent(QMouseEvent *e) { e->accept(); }
void TaskChainLockOverlay::mouseDoubleClickEvent(QMouseEvent *e) { e->accept(); }
void TaskChainLockOverlay::mouseMoveEvent(QMouseEvent *e) { e->accept(); }
void TaskChainLockOverlay::wheelEvent(QWheelEvent *e) { e->accept(); }
void TaskChainLockOverlay::contextMenuEvent(QContextMenuEvent *e) { e->accept(); }
void TaskChainLockOverlay::dragEnterEvent(QDragEnterEvent *e) { e->acceptProposedAction(); }
void TaskChainLockOverlay::dragMoveEvent(QDragMoveEvent *e) { e->acceptProposedAction(); }
void TaskChainLockOverlay::dropEvent(QDropEvent *e) { e->acceptProposedAction(); }
