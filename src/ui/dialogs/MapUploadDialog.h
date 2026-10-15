#ifndef MAPUPLOADDIALOG_H
#define MAPUPLOADDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

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
    void autoDetectSiblings(const QString &chosenPath);

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_pgmEdit = nullptr;
    QLineEdit *m_yamlEdit = nullptr;
    QLineEdit *m_pbStreamEdit = nullptr;
    QLineEdit *m_reflectorBinEdit = nullptr;
    QLineEdit *m_reflectorJsonEdit = nullptr;

    QPushButton *m_uploadBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

#endif // MAPUPLOADDIALOG_H
