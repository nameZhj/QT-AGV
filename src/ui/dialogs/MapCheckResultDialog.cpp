#include "MapCheckResultDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

MapCheckResultDialog::MapCheckResultDialog(QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUi();
    resize(520, 260);
}

void MapCheckResultDialog::setupUi()
{
    setStyleSheet(
        "QDialog {"
        "  background-color: #23262c;"
        "  border: 1px solid #3c424d;"
        "  border-radius: 4px;"
        "}"
    );

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 4);
    mainLayout->setSpacing(0);

    // ── 标题栏 (深灰背景 + 青色菱形眼睛图标 + 检查结果 + 关闭按钮) ──────────────
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(30);
    m_titleBar->setStyleSheet("background-color: #2a2e36; border-top-left-radius: 3px; border-top-right-radius: 3px;");

    auto *tbLayout = new QHBoxLayout(m_titleBar);
    tbLayout->setContentsMargins(8, 0, 6, 0);
    tbLayout->setSpacing(6);

    m_titleIcon = new QLabel(m_titleBar);
    m_titleIcon->setPixmap(makeTitleIcon());
    m_titleIcon->setFixedSize(16, 16);
    tbLayout->addWidget(m_titleIcon);

    m_titleLabel = new QLabel("检查结果", m_titleBar);
    m_titleLabel->setStyleSheet("color: #e2e8f0; font-size: 12px; font-weight: bold; background: transparent;");
    tbLayout->addWidget(m_titleLabel);

    tbLayout->addStretch();

    m_closeBtn = new QPushButton("✕", m_titleBar);
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "  color: #94a3b8; background: transparent; border: none; font-size: 13px; font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  color: #ffffff; background-color: #e11d48; border-radius: 2px;"
        "}"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    tbLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(m_titleBar);

    // ── 表格区域 ─────────────────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->horizontalHeader()->setVisible(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 32);

    m_table->setStyleSheet(
        "QTableWidget {"
        "  background-color: #1e2127;"
        "  color: #e2e8f0;"
        "  border: none;"
        "  gridline-color: #2b2f38;"
        "  font-size: 12px;"
        "}"
        "QTableWidget::item {"
        "  padding-left: 6px;"
        "  padding-right: 6px;"
        "  border-bottom: 1px solid #2b2f38;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #1e3a5f;"
        "  color: #ffffff;"
        "}"
        "QTableWidget::item:hover {"
        "  background-color: #262a32;"
        "}"
        "QScrollBar:vertical {"
        "  background: #1e2127; width: 8px; margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #3b4250; min-height: 20px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #4a5568;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    );

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        if (row >= 0 && row < m_items.size()) {
            emit jumpToFeature(m_items[row].targetType, m_items[row].targetId);
        }
    });

    mainLayout->addWidget(m_table);

    // ── 底部提示信息 ─────────────────────────────────────────────────────────
    m_tipLabel = new QLabel("注:鼠标双击可跳转", this);
    m_tipLabel->setStyleSheet("color: #94a3b8; font-size: 11px; padding: 4px 8px; background-color: #1a1c22;");
    mainLayout->addWidget(m_tipLabel);
}

void MapCheckResultDialog::setResults(const QList<CheckResultItem> &items)
{
    m_items = items;
    m_table->setRowCount(items.size());

    static const QPixmap errIcon = makeErrorIcon();
    static const QPixmap warnIcon = makeWarningIcon();

    for (int i = 0; i < items.size(); ++i) {
        const auto &item = items[i];
        m_table->setRowHeight(i, 28);

        // 序号列
        auto *idxItem = new QTableWidgetItem(QString::number(i + 1));
        idxItem->setTextAlignment(Qt::AlignCenter);
        idxItem->setForeground(QBrush(QColor("#94a3b8")));
        m_table->setItem(i, 0, idxItem);

        // 提示内容列 (带图标)
        auto *contentItem = new QTableWidgetItem(item.message);
        contentItem->setIcon(item.level == CheckResultItem::Error ? errIcon : warnIcon);
        contentItem->setForeground(QBrush(QColor(item.level == CheckResultItem::Error ? "#f87171" : "#fbbf24")));
        contentItem->setToolTip(item.message);
        m_table->setItem(i, 1, contentItem);
    }
}

bool MapCheckResultDialog::hasErrors() const
{
    for (const auto &it : m_items) {
        if (it.level == CheckResultItem::Error) return true;
    }
    return false;
}

void MapCheckResultDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_titleBar && m_titleBar->rect().contains(m_titleBar->mapFrom(this, event->pos()))) {
        m_dragging = true;
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void MapCheckResultDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void MapCheckResultDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

QPixmap MapCheckResultDialog::makeTitleIcon()
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // 绘制菱形/眼睛图标
    QPolygonF poly;
    poly << QPointF(1, 8) << QPointF(8, 2) << QPointF(15, 8) << QPointF(8, 14);
    p.setPen(QPen(QColor("#38bdf8"), 1.2));
    p.setBrush(QBrush(QColor("#0284c7")));
    p.drawPolygon(poly);

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(Qt::white));
    p.drawEllipse(QPointF(8, 8), 2.2, 2.2);

    return pix;
}

QPixmap MapCheckResultDialog::makeErrorIcon()
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // 绘制粉红圆底
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(QColor("#e11d48")));
    p.drawEllipse(0.5, 0.5, 15, 15);

    // 绘制白色叉
    QPen pen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(5.0, 5.0), QPointF(11.0, 11.0));
    p.drawLine(QPointF(5.0, 11.0), QPointF(11.0, 5.0));

    return pix;
}

QPixmap MapCheckResultDialog::makeWarningIcon()
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // 绘制黄色警示三角形
    QPolygonF tri;
    tri << QPointF(8.0, 1.5) << QPointF(15.5, 14.5) << QPointF(0.5, 14.5);
    p.setPen(QPen(QColor("#d97706"), 1.0));
    p.setBrush(QBrush(QColor("#f59e0b")));
    p.drawPolygon(tri);

    // 感叹号
    QPen pen(QColor("#1e2025"), 1.8, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(8.0, 5.5), QPointF(8.0, 9.5));
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(QColor("#1e2025")));
    p.drawEllipse(QPointF(8.0, 12.2), 1.0, 1.0);

    return pix;
}
