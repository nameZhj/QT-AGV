#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QLabel>

// 通用 HTTP 请求进度对话框
// 用法：show() 前调用 API，在回调中 close() + deleteLater()
// modality = Qt::ApplicationModal 时阻止所有窗口交互，适用于建图启动/停止
class LoadingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoadingDialog(const QString &message, QWidget *parent = nullptr,
                           Qt::WindowModality modality = Qt::WindowModal);
    void setMessage(const QString &msg);

private:
    QLabel *m_label = nullptr;
};

#endif // LOADINGDIALOG_H
