#ifndef TOAST_H
#define TOAST_H

#include <QWidget>
#include <QList>

// 轻量级吐司提示：非阻塞、自动淡出，深色主题风格与全局界面一致。
// 用法: Toast::success(this, "保存成功"); Toast::error(this, "保存失败");
class Toast : public QWidget
{
    Q_OBJECT
public:
    enum Type { Success, Error, Info };

    static void show(QWidget *parent, const QString &message, Type type = Success);
    static void success(QWidget *parent, const QString &message) { show(parent, message, Success); }
    static void error(QWidget *parent, const QString &message)   { show(parent, message, Error); }
    static void info(QWidget *parent, const QString &message)    { show(parent, message, Info); }

private:
    explicit Toast(QWidget *topLevel, const QString &message, Type type);
    void reposition();

    static QList<Toast *> s_active;
};

#endif // TOAST_H
