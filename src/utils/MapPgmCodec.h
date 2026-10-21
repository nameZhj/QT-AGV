/**
 * @file    MapPgmCodec.h
 * @layer   Utils
 * @brief   PGM image encoding/decoding and ROS map_server YAML parser.
 * @deps    QImage, QByteArray, QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QByteArray>
#include <QImage>

/**
 * @brief ROS Map Server YAML 解析结果结构体
 */
struct MapYamlInfo
{
    double  resolution = 0.05;   ///< 地图分辨率 (米/像素)
    double  originX    = 0.0;    ///< 原点 X 坐标 (米)
    double  originY    = 0.0;    ///< 原点 Y 坐标 (米)
    double  originYaw  = 0.0;    ///< 原点偏航角 (弧度)
    bool    negate     = false;  ///< 灰度反转标志 (true 表示黑色为自由空间，白色为障碍物)
    QString imageFile;           ///< 关联的 PGM 图像文件名
    bool    isValid    = false;  ///< 解析是否成功且包含关键字段
};

/**
 * @brief PGM 图像编解码及 ROS YAML 解析类
 */
class MapPgmCodec final
{
public:
    MapPgmCodec() = delete;
    ~MapPgmCodec() = delete;
    MapPgmCodec(const MapPgmCodec &) = delete;
    MapPgmCodec &operator=(const MapPgmCodec &) = delete;

    static constexpr double kDefaultResolution  = 0.05; ///< 默认分辨率 (米/像素)
    static constexpr int    kDefaultPgmMaxVal   = 255;  ///< 默认 PGM 最大像素值

    /**
     * @brief 将 PGM(P5 二进制 / P2 文本) 字节流转换为 QImage(Format_Grayscale8)
     */
    static QImage pgmToImage(const QByteArray &pgmData, bool negate = false);

    /**
     * @brief 将灰度图像编码为标准 PGM(P5 二进制) 字节流
     */
    static QByteArray imageToPgm(const QImage &image, bool negate = false);

    /**
     * @brief 解析 ROS map_server 风格的 YAML 文本为结构体
     */
    static MapYamlInfo parseMapYaml(const QString &yamlText);

    /**
     * @brief 解析 ROS map_server 风格的 YAML 文本（兼容引用参数 API）
     */
    static bool parseMapYaml(const QString &yamlText,
                             double &resolution,
                             double &originX,
                             double &originY,
                             bool &negate,
                             QString *imageFile = nullptr);
};
