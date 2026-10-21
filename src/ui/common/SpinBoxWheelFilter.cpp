/**
 * @file    SpinBoxWheelFilter.cpp
 * @module  GUILayer
 * @brief   Implementation of spin box wheel event filter.
 * @deps    SpinBoxWheelFilter.h, QAbstractSpinBox, QEvent
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "SpinBoxWheelFilter.h"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QEvent>
#include <QWheelEvent>
#include <QWidget>

namespace {

// 递归查找自身或各级父级中的 QAbstractSpinBox
QAbstractSpinBox *findSpinBox(QObject *obj)
{
    while (obj) {
        if (auto *spin = qobject_cast<QAbstractSpinBox*>(obj)) {
            return spin;
        }
        if (auto *w = qobject_cast<QWidget*>(obj)) {
            obj = w->parentWidget();
        } else {
            break;
        }
    }
    return nullptr;
}

inline void ensureStrongFocusPolicy(QAbstractSpinBox *spin)
{
    if (spin && spin->focusPolicy() == Qt::WheelFocus) {
        spin->setFocusPolicy(Qt::StrongFocus);
    }
}

} // namespace

// === [Section: Constructors & Destructor] ===
SpinBoxWheelFilter::SpinBoxWheelFilter(QObject *parent)
    : QObject(parent)
{
}

// === [Section: Event Filtering] ===
bool SpinBoxWheelFilter::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type evType = event->type();

    if (evType == QEvent::Show || evType == QEvent::Polish) {
        ensureStrongFocusPolicy(qobject_cast<QAbstractSpinBox*>(watched));
        return QObject::eventFilter(watched, event);
    }

    if (evType != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }

    auto *spin = findSpinBox(watched);
    if (!spin) {
        return QObject::eventFilter(watched, event);
    }

    ensureStrongFocusPolicy(spin);

    // 判断步进器自身或其内部文本编辑框是否拥有输入焦点
    QWidget *focusWidget = QApplication::focusWidget();
    const bool hasFocus = (focusWidget && (focusWidget == spin || spin->isAncestorOf(focusWidget)));

    if (!hasFocus) {
        // 未获得焦点：忽略事件，阻止步进触发，并将事件向上冒泡给父级滚动容器（如 QScrollArea）
        event->ignore();
        return true;
    }

    return QObject::eventFilter(watched, event);
}

