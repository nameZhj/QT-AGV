/**
 * @file    ZipUtil.h
 * @module  CoreTopology
 * @brief   Zip archive extractor utility for map packages.
 * @deps    QDir, QFile, QByteArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

/**
 * @brief ZIP 压缩包实用工具类（提供系统级解压与打包能力，具备多通道自动容灾与进程守护）
 * 
 * 架构规范：
 * 1. 纯静态工具类，显式禁用构造/析构与拷贝；
 * 2. 解压策略：优先采用系统原生 bsdtar（极速零运行时开销），失败自动降级至 PowerShell Expand-Archive；
 * 3. 压缩策略：支持目录打包为标准 ZIP 归档；
 * 4. 健壮性保障：超时自动阻断并成对回收子进程句柄，杜绝后台僵尸进程与文件锁死；
 * 5. 诊断能力：支持可选错误信息透传，且保持 API 100% 向下兼容既有调用点。
 */
class ZipUtil final
{
public:
    ZipUtil() = delete;
    ~ZipUtil() = delete;
    ZipUtil(const ZipUtil &) = delete;
    ZipUtil &operator=(const ZipUtil &) = delete;

    /**
     * @brief 解压 ZIP 文件至目标目录
     * @param zipFilePath 压缩包本地路径（支持绝对/相对路径）
     * @param destDir 目标解压目录
     * @param errorMessage 可选输出错误描述
     * @param timeoutMs 单次命令执行超时限制（毫秒，默认 30000ms）
     * @return 是否成功解压出有效内容
     */
    static bool extractZip(const QString &zipFilePath,
                           const QString &destDir,
                           QString *errorMessage = nullptr,
                           int timeoutMs = 30000);

    /**
     * @brief 将指定目录压缩打包为 ZIP 文件
     * @param sourceDir 待压缩的源目录路径
     * @param destZipFilePath 输出的 ZIP 文件完整路径
     * @param errorMessage 可选输出错误描述
     * @param timeoutMs 单次命令执行超时限制（毫秒，默认 30000ms）
     * @return 是否压缩成功且生成有效文件
     */
    static bool compressDir(const QString &sourceDir,
                            const QString &destZipFilePath,
                            QString *errorMessage = nullptr,
                            int timeoutMs = 30000);
};
