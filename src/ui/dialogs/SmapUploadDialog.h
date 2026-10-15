#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class SmapUploadDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SmapUploadDialog(QWidget *parent = nullptr);
    ~SmapUploadDialog() override = default;

signals:
    void uploadSucceeded();

private slots:
    void onBrowseSmap();
    void onStartUpload();

private:
    void setupUI();

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_fileEdit = nullptr;

    QPushButton *m_uploadBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

