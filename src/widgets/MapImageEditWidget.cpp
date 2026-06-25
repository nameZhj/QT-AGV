#include "MapImageEditWidget.h"
#include "MapPaintCanvas.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/MapPgmUtil.h"
#include "core/Toast.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QSlider>
#include <QSpinBox>
#include <QButtonGroup>
#include <QFrame>
#include <QPainter>
#include <QImage>

namespace {

static const char *kLabelStyle = "color: rgb(183, 183, 183); border: none;";

QIcon drawBrushToolIcon(bool eraser)
{
    int sz = 22;
    QPixmap pix(sz, sz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(220, 220, 220), 1.2));
    if (!eraser) {
        // 画笔：实心圆点，代表涂黑(标记障碍)
        p.setBrush(QColor(40, 40, 40));
        p.drawEllipse(QRectF(2, 2, 18, 18));
        p.setBrush(QColor(10, 10, 10));
        p.drawEllipse(QRectF(6, 6, 10, 10));
    } else {
        // 橡皮：白色方块，代表擦除为可通行区域
        p.setBrush(QColor(245, 245, 245));
        p.drawRect(QRectF(3, 3, 16, 16));
    }
    p.end();
    return QIcon(pix);
}

QIcon drawMoveToolIcon()
{
    int sz = 22;
    QPixmap pix(sz, sz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    // 移动：四向箭头，代表取消画笔/橡皮，仅用于平移浏览
    QColor col(220, 220, 220);
    double c = sz * 0.5;
    p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(c, sz*0.12), QPointF(c, sz*0.88));
    p.drawLine(QPointF(sz*0.12, c), QPointF(sz*0.88, c));
    p.setPen(Qt::NoPen); p.setBrush(col);
    QPolygonF up;    up    << QPointF(c, sz*0.04) << QPointF(c-sz*0.10, sz*0.18) << QPointF(c+sz*0.10, sz*0.18);
    QPolygonF down;  down  << QPointF(c, sz*0.96) << QPointF(c-sz*0.10, sz*0.82) << QPointF(c+sz*0.10, sz*0.82);
    QPolygonF left;  left  << QPointF(sz*0.04, c) << QPointF(sz*0.18, c-sz*0.10) << QPointF(sz*0.18, c+sz*0.10);
    QPolygonF right; right << QPointF(sz*0.96, c) << QPointF(sz*0.82, c-sz*0.10) << QPointF(sz*0.82, c+sz*0.10);
    p.drawPolygon(up); p.drawPolygon(down); p.drawPolygon(left); p.drawPolygon(right);
    p.end();
    return QIcon(pix);
}

} // namespace

MapImageEditWidget::MapImageEditWidget(const QString &mapId, QWidget *parent)
    : QWidget(parent), m_mapId(mapId)
{
    setupUI();
    loadMap();
}

void MapImageEditWidget::setupUI()
{
    setStyleSheet("background:#1a1d23;");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // ---------------- 标题栏 ----------------
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QPushButton *backBtn = new QPushButton("< 返回");
    backBtn->setFixedSize(70, 26);
    connect(backBtn, &QPushButton::clicked, this, &MapImageEditWidget::goBack);
    headerLayout->addWidget(backBtn);

    m_titleLabel = new QLabel("编辑地图");
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: rgb(220, 220, 220); border: none;");
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();

    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("font-size: 11px; color: rgb(138, 138, 138); border: none;");
    headerLayout->addWidget(m_statusLabel);
    headerLayout->addSpacing(8);

    m_saveBtn = new QPushButton("保存并下发");
    m_saveBtn->setStyleSheet(
        "QPushButton{background:#1e6fd4; color:#fff; border:none; border-radius:4px;"
        "padding:5px 14px; font-size:12px; font-weight:600;}"
        "QPushButton:hover{background:#2980e8;}"
        "QPushButton:disabled{background:#2d3340; color:#556677;}");
    m_saveBtn->setFixedSize(96, 28);
    connect(m_saveBtn, &QPushButton::clicked, this, &MapImageEditWidget::onSave);
    headerLayout->addWidget(m_saveBtn);

    mainLayout->addLayout(headerLayout);

    // ---------------- 画布（先创建，工具栏控件需引用它） ----------------
    m_canvas = new MapPaintCanvas;

    // ---------------- 工具栏 ----------------
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(8);

    m_brushBtn = new QToolButton;
    m_brushBtn->setCheckable(true);
    m_brushBtn->setChecked(true);
    m_brushBtn->setIcon(drawBrushToolIcon(false));
    m_brushBtn->setIconSize(QSize(22, 22));
    m_brushBtn->setFixedSize(36, 30);
    m_brushBtn->setToolTip("画笔：涂黑（标记为障碍/墙体）");

    m_eraserBtn = new QToolButton;
    m_eraserBtn->setCheckable(true);
    m_eraserBtn->setIcon(drawBrushToolIcon(true));
    m_eraserBtn->setIconSize(QSize(22, 22));
    m_eraserBtn->setFixedSize(36, 30);
    m_eraserBtn->setToolTip("橡皮：擦除（标记为可通行区域）");

    m_moveBtn = new QToolButton;
    m_moveBtn->setCheckable(true);
    m_moveBtn->setIcon(drawMoveToolIcon());
    m_moveBtn->setIconSize(QSize(22, 22));
    m_moveBtn->setFixedSize(36, 30);
    m_moveBtn->setToolTip("移动：取消画笔/橡皮，仅平移浏览地图");

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->addButton(m_brushBtn);
    m_toolGroup->addButton(m_eraserBtn);
    m_toolGroup->addButton(m_moveBtn);
    connect(m_toolGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *btn) {
        MapPaintCanvas::Tool tool = MapPaintCanvas::Tool::Brush;
        if (btn == m_eraserBtn) tool = MapPaintCanvas::Tool::Eraser;
        else if (btn == m_moveBtn) tool = MapPaintCanvas::Tool::Move;
        m_canvas->setTool(tool);
    });

    toolbarLayout->addWidget(m_brushBtn);
    toolbarLayout->addWidget(m_eraserBtn);
    toolbarLayout->addWidget(m_moveBtn);

    QLabel *sizeLabel = new QLabel("笔刷大小:");
    sizeLabel->setStyleSheet(kLabelStyle);
    toolbarLayout->addWidget(sizeLabel);

    m_brushSizeSlider = new QSlider(Qt::Horizontal);
    m_brushSizeSlider->setRange(1, 200);
    m_brushSizeSlider->setValue(2);
    m_brushSizeSlider->setFixedWidth(140);
    toolbarLayout->addWidget(m_brushSizeSlider);

    m_brushSizeSpin = new QSpinBox;
    m_brushSizeSpin->setRange(1, 200);
    m_brushSizeSpin->setValue(2);
    m_brushSizeSpin->setFixedWidth(60);
    toolbarLayout->addWidget(m_brushSizeSpin);

    connect(m_brushSizeSlider, &QSlider::valueChanged, m_brushSizeSpin, &QSpinBox::setValue);
    connect(m_brushSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_brushSizeSlider, &QSlider::setValue);
    connect(m_brushSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_canvas, &MapPaintCanvas::setBrushSize);

    QFrame *sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: rgb(65, 65, 65);");
    toolbarLayout->addWidget(sep1);

    m_undoBtn = new QPushButton("撤销");
    m_undoBtn->setFixedSize(60, 28);
    m_redoBtn = new QPushButton("重做");
    m_redoBtn->setFixedSize(60, 28);
    toolbarLayout->addWidget(m_undoBtn);
    toolbarLayout->addWidget(m_redoBtn);
    connect(m_undoBtn, &QPushButton::clicked, m_canvas, &MapPaintCanvas::undo);
    connect(m_redoBtn, &QPushButton::clicked, m_canvas, &MapPaintCanvas::redo);

    QFrame *sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: rgb(65, 65, 65);");
    toolbarLayout->addWidget(sep2);

    QPushButton *zoomOutBtn = new QPushButton("缩小");
    QPushButton *zoomResetBtn = new QPushButton("适应窗口");
    QPushButton *zoomInBtn = new QPushButton("放大");
    zoomOutBtn->setFixedSize(60, 28);
    zoomResetBtn->setFixedSize(80, 28);
    zoomInBtn->setFixedSize(60, 28);
    toolbarLayout->addWidget(zoomOutBtn);
    toolbarLayout->addWidget(zoomResetBtn);
    toolbarLayout->addWidget(zoomInBtn);
    connect(zoomOutBtn, &QPushButton::clicked, m_canvas, &MapPaintCanvas::zoomOut);
    connect(zoomInBtn, &QPushButton::clicked, m_canvas, &MapPaintCanvas::zoomIn);
    connect(zoomResetBtn, &QPushButton::clicked, m_canvas, &MapPaintCanvas::resetView);

    toolbarLayout->addStretch();

    QLabel *tipLabel = new QLabel("左键绘制，右键拖拽平移，滚轮缩放；选中“移动”后左键也只平移不绘制");
    tipLabel->setStyleSheet("font-size: 11px; color: rgb(138, 138, 138); border: none;");
    toolbarLayout->addWidget(tipLabel);

    mainLayout->addLayout(toolbarLayout);

    mainLayout->addWidget(m_canvas, 1);

    connect(m_canvas, &MapPaintCanvas::historyChanged, this, &MapImageEditWidget::updateHistoryButtons);
    updateHistoryButtons();

    for (auto *btn : {m_undoBtn, m_redoBtn, zoomOutBtn, zoomResetBtn, zoomInBtn, backBtn})
        btn->setStyleSheet(
            "QPushButton{background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
            "border-radius:4px; font-size:12px; font-weight:600;}"
            "QPushButton:hover{background:#2d3340; border-color:#4a9eff;}"
            "QPushButton:disabled{color:#445566; border-color:#2d3340;}");
}

void MapImageEditWidget::updateHistoryButtons()
{
    m_undoBtn->setEnabled(m_canvas->canUndo());
    m_redoBtn->setEnabled(m_canvas->canRedo());
}

// 与 Vue editMap.vue 完全一致：通过 downloadPcd 拿到 pcdPath，再从 pcdHost 拉取
// 原始 pgm 文件，按灰度原样渲染——不做 negate/分辨率换算（这里只是像素编辑，
// 不涉及世界坐标）。
void MapImageEditWidget::loadMap()
{
    if (m_mapId.isEmpty()) return;

    ApiService::instance().getMap(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject info = data.value("data").toObject(data);
        m_mapName = info.value("mapName").toString();
        m_titleLabel->setText(QString("编辑地图：%1").arg(m_mapName));
    });

    m_statusLabel->setText("正在加载地图...");
    QJsonObject query{{"mapId", m_mapId}};
    ApiService::instance().downloadPcd(query, [this](int code, const QJsonObject &data) {
        if (code != 200) {
            m_statusLabel->setText("获取地图文件失败");
            Toast::error(this, "获取地图文件失败");
            return;
        }
        QJsonObject body = data.value("data").toObject(data);
        QString pcdPath = body.value("pcdPath").toString();
        if (pcdPath.isEmpty()) {
            m_statusLabel->setText("该地图没有可编辑的栅格图");
            Toast::error(this, "该地图没有可编辑的栅格图");
            return;
        }

        QString url = AppConfig::instance().pcdHost() + pcdPath;
        ApiService::instance().downloadFile(url, [this](bool ok, const QByteArray &raw) {
            if (!ok) {
                m_statusLabel->setText("地图文件下载失败");
                Toast::error(this, "地图文件下载失败");
                return;
            }
            QImage img = MapPgmUtil::pgmToImage(raw, false);
            if (img.isNull()) {
                m_statusLabel->setText("地图图片解析失败");
                Toast::error(this, "地图图片解析失败");
                return;
            }
            m_canvas->setImage(img);
            m_statusLabel->clear();
        });
    });
}

void MapImageEditWidget::onSave()
{
    if (m_mapId.isEmpty() || m_canvas->image().isNull()) return;

    QByteArray pgmData = MapPgmUtil::imageToPgm(m_canvas->image(), false);
    if (pgmData.isEmpty()) {
        Toast::error(this, "生成PGM文件失败");
        return;
    }

    m_saveBtn->setEnabled(false);
    m_statusLabel->setText("正在保存并上传...");

    ApiService::instance().uploadMapPgm(m_mapId, pgmData, [this](int code, const QJsonObject &) {
        m_saveBtn->setEnabled(true);
        if (code == 200) {
            m_statusLabel->setText("已保存并下发到设备");
            Toast::success(this, "保存成功");
        } else {
            m_statusLabel->setText("下发失败");
            Toast::error(this, "保存失败");
        }
    });
}
