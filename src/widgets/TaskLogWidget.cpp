#include "TaskLogWidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOption>

namespace {
// 自定义树控件绘制样式：定制青蓝色展开/折叠三角指示器，去除冗余连线
class LogTreeStyle : public QProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override
    {
        if (element == PE_IndicatorBranch) {
            if (option->state & State_Children) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                QRect r = option->rect;
                qreal size = 8.0;
                qreal cx = r.center().x();
                qreal cy = r.center().y();

                QPainterPath path;
                if (option->state & State_Open) {
                    // 向下指示器 v
                    path.moveTo(cx - size / 2.0, cy - size / 4.0);
                    path.lineTo(cx + size / 2.0, cy - size / 4.0);
                    path.lineTo(cx, cy + size / 2.0);
                    path.closeSubpath();
                } else {
                    // 向右指示器 >
                    path.moveTo(cx - size / 4.0, cy - size / 2.0);
                    path.lineTo(cx + size / 2.0, cy);
                    path.lineTo(cx - size / 4.0, cy + size / 2.0);
                    path.closeSubpath();
                }
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor("#00a8ff"));
                painter->drawPath(path);
                painter->restore();
            }
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};
}

TaskLogWidget::TaskLogWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    buildSampleData();
}

void TaskLogWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tree = new QTreeWidget(this);
    m_tree->setStyle(new LogTreeStyle);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({"Key / Time", "Value"});

    // 列宽与自适应
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->setColumnWidth(0, 360);
    m_tree->setIndentation(18);
    m_tree->setAnimated(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAlternatingRowColors(false);

    // 深色终端风格样式
    m_tree->setStyleSheet(
        "QTreeWidget {"
        "    background-color: #1e1e1e;"
        "    color: #a8a8a8;"
        "    border: none;"
        "    outline: none;"
        "    font-family: 'Consolas', 'Courier New', 'Microsoft YaHei', monospace;"
        "    font-size: 13px;"
        "}"
        "QTreeWidget::item {"
        "    min-height: 25px;"
        "    padding-top: 1px;"
        "    padding-bottom: 1px;"
        "    border-bottom: 1px solid #2d2d2d;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: #282a2d;"
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: #0c1012;"
        "    color: inherit;"
        "}"
        "QHeaderView::section {"
        "    background-color: #252526;"
        "    color: #9e9e9e;"
        "    border: none;"
        "    border-bottom: 1px solid #333333;"
        "    padding: 6px 10px;"
        "    font-weight: normal;"
        "    font-size: 13px;"
        "    font-family: 'Consolas', 'Courier New', 'Microsoft YaHei', monospace;"
        "}"
        "QScrollBar:vertical {"
        "    background: #1e1e1e;"
        "    width: 8px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #3e3e42;"
        "    min-height: 20px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #4e4e52;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
    );

    layout->addWidget(m_tree);
}

void TaskLogWidget::refreshData()
{
    // 具体数据通过接口获取，当前接口尚未就绪，在此处预留接入位置
    // TODO: 待接口开发完成后调用接口并解析成树节点
}

void TaskLogWidget::buildSampleData()
{
    m_tree->clear();

    const QColor greenColor("#38e168");  // 完成亮绿色
    const QColor grayColor("#a6a6a6");   // 普通灰白色

    auto addRow = [&](const QString &key, const QString &val, QTreeWidgetItem *parent = nullptr) -> QTreeWidgetItem* {
        QTreeWidgetItem *item = nullptr;
        if (parent) {
            item = new QTreeWidgetItem(parent);
        } else {
            item = new QTreeWidgetItem(m_tree);
        }
        item->setText(0, key);
        item->setText(1, val);

        bool isCompleted = val.contains("完成") || (key.contains("完成") && val.isEmpty());
        QColor color = isCompleted ? greenColor : grayColor;
        item->setForeground(0, color);
        item->setForeground(1, color);
        return item;
    };

    addRow("2026-10-08 20:20:30.543", "GotoSpecifiedPose 1 完成");
    addRow("2026-10-08 20:21:05.540", "GotoSpecifiedPose 2 未知");
    addRow("2026-10-08 20:21:17.867", "GoByOdometer 未知 完成");
    addRow("2026-10-08 20:23:02.558", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:23:25.417", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:24:45.429", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:26:57.328", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:27:27.025", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:28:14.985", "GotoSpecifiedPose 2 被取消");

    // 展开状态的项（第二张效果图）
    QTreeWidgetItem *expandedItem = addRow("2026-10-08 20:29:04.205", "GotoSpecifiedPose 2 完成");

    // 子节点 1: TaskStart (展开状态)
    QTreeWidgetItem *taskStart = addRow("TaskStart", "", expandedItem);
    addRow("skill_name", "GotoSpecifiedPose", taskStart);
    addRow("target_x", "-6.599", taskStart);
    addRow("target_y", "4.339", taskStart);
    addRow("target_angle", "-2.3368682881716567", taskStart);
    addRow("target_name", "2", taskStart);
    addRow("reach_dist", "0.005", taskStart);
    addRow("reach_angle", "3.1415926535897931", taskStart);
    addRow("reach_method", "forward", taskStart);
    addRow("reach_vel_x", "", taskStart);
    addRow("max_speed", "1.1", taskStart);
    addRow("max_acc", "1", taskStart);
    addRow("max_rot", "1.5707963267948966", taskStart);
    addRow("max_rot_acc", "2.0943951023931953", taskStart);
    addRow("params", "key: parkRequired bool_value: false", taskStart);
    addRow("params", "key: final bool_value: true", taskStart);
    addRow("params", "key: allow_free_go bool_value: true", taskStart);
    addRow("params", "key: em_stop_dist double_value: -1", taskStart);
    addRow("params", "key: stop_acc double_value: -1", taskStart);
    addRow("params", "key: load_stop_acc double_value: -1", taskStart);
    addRow("task_id", "", taskStart);
    addRow("max_dec", "1", taskStart);
    addRow("max_rot_dec", "1.5707963267948966", taskStart);
    addRow("obs_stop_dist", "1", taskStart);
    addRow("obs_dec_dist", "3", taskStart);
    addRow("obs_dec_speed", "0.2", taskStart);
    addRow("obs_expansion", "0.1", taskStart);
    addRow("dec_obs_expansion", "0.2", taskStart);
    taskStart->setExpanded(true);

    // 任务执行中的时序子事件
    addRow("2026-10-08 20:29:04.206", "SoundPlayer play 播放音频", expandedItem);
    addRow("2026-10-08 20:29:04.206", "SoundPlayer play 播放音频 Cost", expandedItem);
    addRow("2026-10-08 20:29:04.227", "SoundPlayer play 播放音频", expandedItem);
    addRow("2026-10-08 20:29:04.227", "SoundPlayer play 播放音频 Cost", expandedItem);
    addRow("2026-10-08 20:29:17.622", "完成", expandedItem);
    addRow("2026-10-08 20:29:17.643", "SoundPlayer stop 停止播放音频", expandedItem);
    addRow("2026-10-08 20:29:17.643", "SoundPlayer stop 停止播放音频 Cost", expandedItem);
    expandedItem->setExpanded(true);

    addRow("2026-10-08 20:29:47.912", "GotoSpecifiedPose 2 完成");
    addRow("2026-10-08 20:34:26.725", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:34:39.307", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:34:58.713", "GotoSpecifiedPose 2 被取消");
    addRow("2026-10-08 20:37:32.494", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:37:34.187", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:37:38.195", "GoByOdometer 未知 完成");
    addRow("2026-10-08 20:38:31.493", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:38:35.995", "GoByOdometer 未知 完成");
    addRow("2026-10-08 20:39:11.020", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:39:11.218", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:39:16.068", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:39:17.163", "GoByOdometer 未知 未知");
    addRow("2026-10-08 20:39:19.892", "GoByOdometer 未知 未知");

    QTreeWidgetItem *selectedItem = addRow("2026-10-08 20:39:20.533", "GoByOdometer 未知 完成");
    m_tree->setCurrentItem(selectedItem);

    addRow("2026-10-08 21:01:28.501", "GoByOdometer 未知 被取消");
    addRow("2026-10-08 21:23:38.323", "GotoSpecifiedPose 1 完成");
    addRow("2026-10-08 21:45:01.045", "GotoSpecifiedPose 2 被取消");
}
