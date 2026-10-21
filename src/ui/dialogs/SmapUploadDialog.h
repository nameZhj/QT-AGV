/**
 * @file    SmapUploadDialog.h
 * @module  GUILayer
 * @brief   SMAP vector map format import and upload modal dialog.
 * @deps    BaseDialog, ApiService
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
 
class QLineEdit;
class QPushButton;

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
