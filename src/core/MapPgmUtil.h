#ifndef MAPPGMUTIL_H
#define MAPPGMUTIL_H

#include <QString>
#include <QByteArray>
#include <QImage>

#include <QPointF>
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
};

#endif // MAPPGMUTIL_H
