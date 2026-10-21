/**
 * @file    TaskChainDragWidgets.h
 * @layer   GUILayer
 * @brief   Specialized drag-and-drop list widgets, overlay, and event filters for task chain.
 * @deps    QListWidget, QDrag, QMimeData, QTimer
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QListWidget>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QPoint>
#include <functional>

// 可拖拽的列表项 — 支持拖出动作标签
class DraggableListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit DraggableListWidget(QWidget *parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
};

// 可接收拖拽动作标签以及任务/组重新排序的通用列表控件
class DropTargetListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit DropTargetListWidget(QWidget *parent = nullptr);

    std::function<void(const QString &actionName, int row)> onActionDropped;
    std::function<void(int fromRow, int toRow)> onGroupReordered;
    std::function<void(int fromRow, int toRow)> onTaskReordered;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

// 通用卡片长按与拖拽重排序事件过滤器 (适用于任务卡片和组卡片)
class CardLongPressDragFilter : public QObject
{
    Q_OBJECT
public:
    CardLongPressDragFilter(const QString &mimeType, int index, QWidget *container, QListWidget *parentList = nullptr);

    std::function<void(int index)> onSelected;
    void selectInParentList();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onLongPressTimeout();
    void startDrag();

    QString m_mimeType;
    int m_index = 0;
    QWidget *m_container = nullptr;
    QPointer<QListWidget> m_parentList;
    QTimer *m_timer = nullptr;
    QPoint m_pressPos;
    bool m_dragStarted = false;
    bool m_longPressed = false;
    QString m_origStyle;
};

// 覆盖在四面板编辑区上的半透明遮罩 (执行中锁定，拦截一切点击与拖拽操作)
class TaskChainLockOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit TaskChainLockOverlay(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
};
