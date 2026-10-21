/**
 * @file    MapViewSwitchMapDialog.h
 * @layer   GUI
 * @brief   Card-based map management and switching dialog with MQTT confirmation.
 * @deps    QDialog, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>

class QLineEdit;
class QLabel;
class QPushButton;
class QGridLayout;
class QFrame;

class MapViewSwitchMapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MapViewSwitchMapDialog(const QString &currentMapId, QWidget *parent = nullptr);
    ~MapViewSwitchMapDialog() override = default;

    QString selectedMapId() const { return m_selectedMapId; }
    QString selectedMapName() const { return m_selectedMapName; }
    QString successMessage() const { return m_switchSuccessMsg; }

private slots:
    void onRefreshList();
    void onFilterChanged(const QString &kw);
    void onConfirmSwitch();

private:
    void setupUI();
    void buildCards(const QJsonArray &list, const QString &filter);
    QFrame* createCard(const QJsonObject &m, const QString &mid, const QString &name,
                       bool isCurrent, bool isCached, bool isSelected);

    QString         m_currentMapId;
    QString         m_selectedMapId;
    QString         m_selectedMapName;
    QString         m_switchSuccessMsg;
    QJsonArray      m_mapList;
    QVector<QFrame*> m_allCards;

    QLineEdit   *m_searchEdit   = nullptr;
    QLabel      *m_statsLabel   = nullptr;
    QLabel      *m_selTipLabel  = nullptr;
    QPushButton *m_okBtn        = nullptr;
    QPushButton *m_cancelBtn    = nullptr;
    QGridLayout *m_grid         = nullptr;
};
