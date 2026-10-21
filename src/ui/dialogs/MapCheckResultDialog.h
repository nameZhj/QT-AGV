/**
 * @file    MapCheckResultDialog.h
 * @module  GUILayer
 * @brief   Map diagnostic validation and topology error reporting dialog.
 * @deps    BaseDialog, QTableWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
#include <QMouseEvent>
#include <QPoint>
#include <QList>

class QTableWidget;
class QLabel;
class QPushButton;

struct CheckResultItem {
    enum Level {
        Error,
        Warning
    } level = Error;
    QString message;
    QString targetType; // "highLevelArea", "line", "curve"
    QString targetId;   // id of target element
};

class MapCheckResultDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MapCheckResultDialog(QWidget *parent = nullptr);
    ~MapCheckResultDialog() override = default;

    void setResults(const QList<CheckResultItem> &items);
    bool hasErrors() const;

signals:
    void jumpToFeature(const QString &targetType, const QString &targetId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUi();
    static QPixmap makeTitleIcon();
    static QPixmap makeErrorIcon();
    static QPixmap makeWarningIcon();

    QWidget      *m_titleBar   = nullptr;
    QLabel       *m_titleIcon  = nullptr;
    QLabel       *m_titleLabel = nullptr;
    QPushButton  *m_closeBtn   = nullptr;
    QTableWidget *m_table      = nullptr;
    QLabel       *m_tipLabel   = nullptr;

    QList<CheckResultItem> m_items;

    bool   m_dragging = false;
    QPoint m_dragPos;
};
