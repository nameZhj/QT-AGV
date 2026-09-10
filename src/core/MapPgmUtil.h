#ifndef MAPPGMUTIL_H
#define MAPPGMUTIL_H

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QPolygonF>
#include <QList>
#include <QPainterPath>

// PGM(P5/P2) 解析 + ROS map_server 风格 YAML 解析。
// 用于直接从设备/服务器拉取的原始 carto_map.pgm + carto_map.yaml 渲染地图，
// 绕开后端为生成瓦片金字塔而对图片做 padding/缩放后派生的 mapWidth/mapHeight/offsetX/offsetY，
// 这些派生字段不代表 pgm 的真实物理尺寸与零点（参见 PcdToImgImpl.pgmToImg 的 padding 逻辑）。
class MapPgmUtil
{
public:
    // 将 PGM(P5二进制/P2文本) 字节流转换为 QImage(Format_Grayscale8)。
    // negate 为 true 时对灰度值取反（与 ROS map_server yaml 的 negate 字段语义一致）。
    static QImage pgmToImage(const QByteArray &pgmData, bool negate = false);

    // 将灰度图像编码为 PGM(P5二进制) 字节流，与 pgmToImage 的 negate 语义相反，
    // 用于保存编辑后的栅格图并直接上传（不写本地临时文件）。
    static QByteArray imageToPgm(const QImage &image, bool negate = false);

    // 解析 ROS map_server 风格的 yaml 文本：resolution / origin: [x,y,yaw] / negate
    static bool parseMapYaml(const QString &yamlText, double &resolution,
                              double &originX, double &originY, bool &negate);

    // 评估任意阶贝塞尔曲线在参数 t (0.0 到 1.0) 的坐标值
    static QPointF evaluateBezier(const QList<QPointF> &controlPoints, double t);

    // 为任意阶贝塞尔曲线生成 QPainterPath 绘制路径
    static QPainterPath getBezierPath(const QList<QPointF> &controlPoints);

    // 生成线路方向指示微型箭头多边形列表 (支持单向前进 0、单向后退 1、双向通行 10 或 2)
    // 采用现代流线型燕尾内凹设计，尺寸精细小巧，自然贴合线条
    static QList<QPolygonF> createDirectionArrows(const QPointF &mid, const QPointF &dirUnit, int direct, double lineLen = 100.0);

    // 生成贝塞尔曲线方向指示微型箭头多边形列表 (严格沿曲线切线方向评估，支持单向前进、单向后退、双向 ↔)
    static QList<QPolygonF> createBezierDirectionArrows(const QList<QPointF> &controlPoints, int direct, double curveLen = 100.0);

    // 生成路网点图元绘制路径：标准长条矩形(长0.50m, 宽0.30m 车辆物理比例) + 中间红色小十字
    static QPainterPath createPointPath(double pxPerMeterX, double pxPerMeterY,
                                        double *outW = nullptr, double *outH = nullptr,
                                        QPainterPath *outCrossPath = nullptr,
                                        double minWidth = 0.0);

    // 解析 .smap 格式文件，提取并转换为路网点(points)、线段(lines)、贝塞尔曲线(curves)与地图名称。
    // 自动将对称的反向贝塞尔曲线合并为单条双向曲线 (direct: 10)。
    static bool parseSmapFile(const QString &filePath,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError = nullptr,
                              QJsonObject *outHeader = nullptr);

    // 解析 .smap JSON 数据对象
    static bool parseSmapJson(const QJsonObject &smapObj,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError = nullptr,
                              QJsonObject *outHeader = nullptr);
};

#endif // MAPPGMUTIL_H
