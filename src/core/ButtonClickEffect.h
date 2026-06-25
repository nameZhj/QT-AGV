#ifndef BUTTONCLICKEFFECT_H
#define BUTTONCLICKEFFECT_H

#include <QObject>

// 全局事件过滤器：给所有按钮(QPushButton/QToolButton/QCheckBox/ToggleSwitch 等
// QAbstractButton 派生类)点击时统一加上按下动效(透明度闪烁)，
// 不需要逐个按钮单独实现。安装一次到 QApplication 即可对全应用生效。
class ButtonClickEffect : public QObject
{
    Q_OBJECT
public:
    explicit ButtonClickEffect(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // BUTTONCLICKEFFECT_H
