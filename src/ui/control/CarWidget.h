/**
 * @file    CarWidget.h
 * @module  GUILayer
 * @brief   Robot chassis control panel, dashboard dials, speed telemetry, and emergency controls.
 * @deps    QWidget, DirectionPadWidget, RobotInfoManager
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>

class QTableWidget;
class QComboBox;
class QLineEdit;
class QPushButton;

class CarWidget : public QWidget
{
    Q_OBJECT
    friend class CarTableHelper;
    friend class CarDialogHelper;

public:
    // === [Section: Constructors & Destructor] ===
    explicit CarWidget(QWidget *parent = nullptr);
    ~CarWidget() override = default;

private slots:
    // === [Section: Toolbar & Action Slots] ===
    void onSearch();
    void onReset();
    void onAdd();
    void onEdit();
    void onDelete();
    void onLink();
    void onTableSelectionChanged();
    void onStatusChanged(int row);

private:
    // === [Section: UI Setup & Data Loading] ===
    void setupUI();
    void loadMapOptions();
    void getList();
    QString resolveMapNameById(int mapId) const;
    void populateCarRow(int row, const QJsonObject &car);

    // === [Section: Dialog Helpers] ===
    void showAddDialog();
    void showEditDialog(const QJsonObject &carData);
    void showLinkDialog(const QJsonObject &carData);
    void checkCertification(const QString &ckey);

    // === [Section: UI Controls & State] ===
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

