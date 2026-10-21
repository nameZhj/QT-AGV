/**
 * @file    MapPropRowHelper.h
 * @layer   GUI
 * @brief   Helper functions for creating consistent form rows, checkable rows, and cards in property panels.
 * @deps    QWidget, QCheckBox, QLabel, QHBoxLayout, QVBoxLayout, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QPair>
#include <QSet>
#include <QString>

namespace MapPropRowHelper {

inline QWidget* mkFormRow(const QString &label, QWidget *editor, const QString &unit = QString())
{
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent; border:none;");
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 1, 2, 1);
    h->setSpacing(6);
    auto *lbl = new QLabel(label);
    lbl->setFixedWidth(110);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setStyleSheet("color:#7e96b0; font-size:11px; background:transparent; border:none;");
    h->addWidget(lbl);
    h->addWidget(editor, 1);
    if (!unit.isEmpty()) {
        auto *unitLbl = new QLabel(unit);
        unitLbl->setMinimumWidth(32);
        unitLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        unitLbl->setStyleSheet("color:#8fa6bf; font-size:11px; background:transparent; border:none; padding-left:2px;");
        h->addWidget(unitLbl);
    }
    return w;
}

inline QWidget* mkCheckableRow(QCheckBox *&chk, const QString &label, QWidget *editor, const QString &unit = QString())
{
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent; border:none;");
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 1, 2, 1);
    h->setSpacing(6);

    auto *leftCont = new QWidget;
    leftCont->setFixedWidth(110);
    leftCont->setStyleSheet("background:transparent; border:none;");
    auto *lh = new QHBoxLayout(leftCont);
    lh->setContentsMargins(0, 0, 0, 0);
    lh->setSpacing(4);

    chk = new QCheckBox;
    chk->setFixedSize(16, 16);
    chk->setStyleSheet(
        "QCheckBox { background: transparent; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #3d4a5d; border-radius: 2px; background: #171c26; }"
        "QCheckBox::indicator:checked { background: #1e6fd4; border-color: #1e6fd4; image: url(:/check.svg); }"
    );
    lh->addWidget(chk, 0, Qt::AlignVCenter);

    auto *lbl = new QLabel(label);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setStyleSheet("color: #8fa6bf; font-size: 11px; background: transparent; border: none;");
    lh->addWidget(lbl, 1, Qt::AlignVCenter);

    h->addWidget(leftCont, 0, Qt::AlignLeft | Qt::AlignVCenter);

    auto *editorCont = new QWidget;
    editorCont->setStyleSheet("background:transparent; border:none;");
    auto *eh = new QHBoxLayout(editorCont);
    eh->setContentsMargins(0, 0, 0, 0);
    eh->setSpacing(4);
    eh->addWidget(editor, 1);
    if (!unit.isEmpty()) {
        auto *unitLbl = new QLabel(unit);
        unitLbl->setMinimumWidth(32);
        unitLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        unitLbl->setStyleSheet("color:#8fa6bf; font-size:11px; background:transparent; border:none; padding-left:2px;");
        eh->addWidget(unitLbl);
    }
    h->addWidget(editorCont, 1);

    chk->setChecked(false);
    editorCont->setVisible(false);

    QObject::connect(chk, &QCheckBox::toggled, editorCont, [editorCont](bool checked) {
        editorCont->setVisible(checked);
    });

    return w;
}

inline QPair<QWidget*, QVBoxLayout*> mkCardContainer(const QString &title)
{
    auto *container = new QWidget;
    container->setStyleSheet("background:#1b2230; border:1px solid #263348; border-radius:6px;");
    auto *l = new QVBoxLayout(container);
    l->setContentsMargins(8,6,8,8);
    l->setSpacing(5);
    auto *lbl = new QLabel(title);
    lbl->setStyleSheet("color:#627e9e; font-size:11px; font-weight:bold; background:transparent; border:none;");
    l->addWidget(lbl);
    return {container, l};
}

inline void restoreCheckedProps(const QJsonArray &cpArr, const QList<QPair<QCheckBox*, QString>> &mapping)
{
    QSet<QString> chkSet;
    for (const auto &v : cpArr) chkSet.insert(v.toString());
    for (const auto &pair : mapping) {
        if (pair.first) {
            bool checked = chkSet.contains(pair.second);
            if (!checked && pair.second == "maxspeed") {
                checked = chkSet.contains("max_speed");
            }
            pair.first->setChecked(checked);
        }
    }
}

inline QJsonArray collectCheckedProps(const QList<QPair<QCheckBox*, QString>> &mapping)
{
    QJsonArray arr;
    for (const auto &pair : mapping) {
        if (pair.first && pair.first->isChecked()) {
            arr.append(pair.second);
        }
    }
    return arr;
}

} // namespace MapPropRowHelper
