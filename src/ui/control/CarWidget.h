#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QJsonObject>
#include <QJsonArray>

class CarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CarWidget(QWidget *parent = nullptr);

private slots:
    void onSearch();
    void onReset();
    void onAdd();
    void onEdit();
    void onDelete();
    void onLink();
    void onTableSelectionChanged();
    void onStatusChanged(int row);

private:
    void setupUI();
    void loadMapOptions();
    void getList();
    void showAddDialog();
    void showEditDialog(const QJsonObject &carData);
    void showLinkDialog(const QJsonObject &carData);
    void checkCertification(const QString &ckey);

    QTableWidget *m_table = nullptr;
    QComboBox *m_mapFilter = nullptr;
    QLineEdit *m_nameFilter = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;

    int m_pageNum = 1;
    int m_pageSize = 10;
    int m_total = 0;
    QJsonArray m_carList;
    QJsonArray m_mapOptions;
    QJsonArray m_carMapOptions;

    // 认证状态
    bool m_certKeyStatus = false;
    QString m_certOldKey;
    QString m_certId;
};

