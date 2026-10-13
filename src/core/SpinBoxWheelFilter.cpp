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

} // namespace

SpinBoxWheelFilter::SpinBoxWheelFilter(QObject *parent)
    : QObject(parent)
{
}

bool SpinBoxWheelFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        if (auto *spin = findSpinBox(watched)) {
            // 确保不通过滚轮偷取焦点
            if (spin->focusPolicy() == Qt::WheelFocus) {
                spin->setFocusPolicy(Qt::StrongFocus);
            }

            // 判断步进器自身或其内部文本编辑框是否拥有输入焦点
            QWidget *fw = QApplication::focusWidget();
            bool hasFocus = (fw && (fw == spin || spin->isAncestorOf(fw)));

            if (!hasFocus) {
                // 未获得焦点：忽略事件，阻止步进触发，并将事件向上冒泡给父级滚动容器（如 QScrollArea）
                event->ignore();
                return true;
            }
        }
    } else if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
        if (auto *spin = qobject_cast<QAbstractSpinBox*>(watched)) {
            if (spin->focusPolicy() == Qt::WheelFocus) {
                spin->setFocusPolicy(Qt::StrongFocus);
            }
        }
    }

    return QObject::eventFilter(watched, event);
}
