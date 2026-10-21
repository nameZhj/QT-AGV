/**
 * @file    MapUploadDialog.h
 * @layer   GUILayer
 * @brief   Map package archive import and synchronization upload dialog.
 * @deps    MapUploadStyle.h, MapUploadHandler.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

class MapUploadDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MapUploadDialog(QWidget *parent = nullptr);
    ~MapUploadDialog() override = default;

signals:
    void uploadSucceeded();

private slots:
    void onBrowsePgm();
    void onBrowseYaml();
    void onBrowsePbStream();
    void onBrowseReflectorBin();
    void onBrowseReflectorJson();
    void onStartUpload();

private:
    void setupUI();

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_pgmEdit = nullptr;
    QLineEdit *m_yamlEdit = nullptr;
    QLineEdit *m_pbStreamEdit = nullptr;
    QLineEdit *m_reflectorBinEdit = nullptr;
    QLineEdit *m_reflectorJsonEdit = nullptr;

    QPushButton *m_uploadBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    friend class MapUploadHandler;
};
