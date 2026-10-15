#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;
class QSlider;
class QSpinBox;
class QButtonGroup;
class QAbstractButton;
class MapPaintCanvas;

// 编辑地图：在已加载的栅格图(pgm)上用画笔/橡皮直接涂改，保存后上传。
// 对应 Vue editMap.vue：画笔=涂黑(标记障碍)，橡皮=涂白(标记可通行)，不做任何
// negate/分辨率换算——纯像素级编辑，与 Vue 参考完全一致。
class MapImageEditWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapImageEditWidget(const QString &mapId, QWidget *parent = nullptr);

signals:
    void goBack();

private slots:
    void onSave();
    void updateHistoryButtons();

private:
    void setupUI();
    void loadMap();

    MapPaintCanvas *m_canvas        = nullptr;
    QLabel          *m_titleLabel    = nullptr;
    QLabel          *m_statusLabel   = nullptr;
    QToolButton     *m_brushBtn      = nullptr;
    QToolButton     *m_eraserBtn     = nullptr;
    QToolButton     *m_moveBtn       = nullptr;
    QButtonGroup    *m_toolGroup     = nullptr;
    QSlider         *m_brushSizeSlider = nullptr;
    QSpinBox        *m_brushSizeSpin   = nullptr;
    QPushButton     *m_undoBtn       = nullptr;
    QPushButton     *m_redoBtn       = nullptr;
    QPushButton     *m_saveBtn       = nullptr;

    QString m_mapId;
    QString m_mapName;
};

