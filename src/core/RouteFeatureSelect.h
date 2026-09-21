#ifndef ROUTEFEATURESELECT_H
#define ROUTEFEATURESELECT_H

#include <QColor>
#include <QPointF>
#include <QGraphicsItem>

class QGraphicsScene;

/**
 * @brief 路网图元(站点/线路/曲线)的命中与选中效果，供"地图预览"与"启用编辑"共用同一份实现。
 *
 * 图元 data 约定 (两个页面保持一致)：
 *   data(0) = 类型字符串 "point" / "charge" / "highPrecision" / "line" / "curve"，空串表示非路网图元
 *   data(1) = 图元 ID
 *   data(2) = 原始配色 (点=填充色，线/曲线=线条色)，取消选中时据此还原
 */
namespace RouteFeatureSelect {

/// 选中态颜色：醒目的亮绿色，与既有配色都不重复
inline const QColor kSelectColor = QColor("#00e676");

QColor itemOriginalColor(QGraphicsItem *item);
void   setItemColor(QGraphicsItem *item, const QColor &col, bool selected);
void   applySelectColor(QGraphicsItem *item);
void   clearSelectColor(QGraphicsItem *item);

/// 按场景坐标与场景单位容差命中路网图元；子图元(十字/箭头/文字)自动上溯到父图元，未命中返回 nullptr
QGraphicsItem *pickItem(QGraphicsScene *scene, const QPointF &scenePos, double tolScene);

} // namespace RouteFeatureSelect

#endif // ROUTEFEATURESELECT_H