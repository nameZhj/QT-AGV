#include "ZipUtil.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace {
// 默认执行参数与超时常量定义，消除魔数
constexpr int kDefaultTarTimeoutMs = 15000;
constexpr int kDefaultProcessKillWaitMs = 1000;

/**
 * @brief 安全运行控制台子进程，防止超时后进程残留或句柄泄露
 */
bool runSafeProcess(const QString &program,
                    const QStringList &arguments,
                    int timeoutMs,
                    int *outExitCode,
                    QString *outStdErr)
{
    QProcess process;
    process.start(program, arguments);

    const bool finished = process.waitForFinished(timeoutMs);
    if (!finished) {
        if (outStdErr) {
            *outStdErr = QString("Process timed out after %1 ms").arg(timeoutMs);
        }
        process.kill();
        process.waitForFinished(kDefaultProcessKillWaitMs);
        return false;
    }

    if (outExitCode) {
        *outExitCode = process.exitCode();
    }
    if (outStdErr) {
        *outStdErr = QString::fromUtf8(process.readAllStandardError()).trimmed();
    }

    return (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
}
} // anonymous namespace

bool ZipUtil::extractZip(const QString &zipFilePath,
                         const QString &destDir,
                         QString *errorMessage,
                         int timeoutMs)
{
    const QFileInfo zipInfo(zipFilePath);
    if (!zipInfo.exists() || zipInfo.size() == 0) {
        const QString err = QString("ZIP 压缩包不存在或文件为空: %1").arg(zipFilePath);
        qWarning().noquote() << "[ZipUtil]" << err;
        if (errorMessage) *errorMessage = err;
        return false;
    }

    QDir targetDir(destDir);
    if (!targetDir.exists()) {
        if (!targetDir.mkpath(".")) {
            const QString err = QString("无法创建目标解压目录: %1").arg(destDir);
            qWarning().noquote() << "[ZipUtil]" << err;
            if (errorMessage) *errorMessage = err;
            return false;
        }
    }

    const QString nativeZip = QDir::toNativeSeparators(zipInfo.absoluteFilePath());
    const QString nativeDest = QDir::toNativeSeparators(targetDir.absolutePath());

    // ── 方案 1：优先采用系统原生 bsdtar（零开销、跨平台标准） ──
    {
        const int tarTimeout = qMin(timeoutMs, kDefaultTarTimeoutMs);
        int exitCode = -1;
        QString stdErr;
        const QStringList args{"-xf", nativeZip, "-C", nativeDest};

        if (runSafeProcess("tar", args, tarTimeout, &exitCode, &stdErr)) {
            if (!targetDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
                qDebug().noquote() << QString("[ZipUtil] 成功解压 (bsdtar): %1 -> %2").arg(nativeZip, nativeDest);
                return true;
            }
        }
        qWarning().noquote() << QString("[ZipUtil] bsdtar 解压失败 (exitCode=%1, err=%2)，转入 PowerShell 容灾通道...")
                                    .arg(exitCode)
                                    .arg(stdErr);
    }

    // ── 方案 2：PowerShell Expand-Archive 容灾备用机制 ──
    {
        QString escapedZip = nativeZip;
        escapedZip.replace("'", "''");
        QString escapedDest = nativeDest;
        escapedDest.replace("'", "''");

        const QString cmd = QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                                .arg(escapedZip, escapedDest);
        const QStringList args{"-NoProfile", "-NonInteractive", "-Command", cmd};

        int exitCode = -1;
        QString stdErr;
        if (runSafeProcess("powershell", args, timeoutMs, &exitCode, &stdErr)) {
            if (!targetDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
                qDebug().noquote() << QString("[ZipUtil] 成功解压 (PowerShell): %1 -> %2").arg(nativeZip, nativeDest);
                return true;
            }
        }

        const QString err = QString("PowerShell 解压亦失败 (exitCode=%1, error=%2)").arg(exitCode).arg(stdErr);
        qWarning().noquote() << "[ZipUtil]" << err;
        if (errorMessage) *errorMessage = err;
    }

    return false;
}

bool ZipUtil::compressDir(const QString &sourceDir,
                          const QString &destZipFilePath,
                          QString *errorMessage,
                          int timeoutMs)
{
    const QDir src(sourceDir);
    if (!src.exists()) {
        const QString err = QString("待压缩的源目录不存在: %1").arg(sourceDir);
        qWarning().noquote() << "[ZipUtil]" << err;
        if (errorMessage) *errorMessage = err;
        return false;
    }

    const QFileInfo destInfo(destZipFilePath);
    const QDir destParentDir = destInfo.absoluteDir();
    if (!destParentDir.exists()) {
        if (!destParentDir.mkpath(".")) {
            const QString err = QString("无法创建输出压缩包的目标父目录: %1").arg(destParentDir.absolutePath());
            qWarning().noquote() << "[ZipUtil]" << err;
            if (errorMessage) *errorMessage = err;
            return false;
        }
    }

    const QString nativeSrc = QDir::toNativeSeparators(src.absolutePath());
    const QString nativeZip = QDir::toNativeSeparators(destInfo.absoluteFilePath());

    // 优先采用 bsdtar 打包 (-a 自动识别 zip 格式)
    {
        const int tarTimeout = qMin(timeoutMs, kDefaultTarTimeoutMs);
        int exitCode = -1;
        QString stdErr;
        const QStringList args{"-acf", nativeZip, "-C", nativeSrc, "."};

        if (runSafeProcess("tar", args, tarTimeout, &exitCode, &stdErr)) {
            const QFileInfo checkOut(nativeZip);
            if (checkOut.exists() && checkOut.size() > 0) {
                qDebug().noquote() << QString("[ZipUtil] 成功打包 (bsdtar): %1 -> %2").arg(nativeSrc, nativeZip);
                return true;
            }
        }
        qWarning().noquote() << QString("[ZipUtil] bsdtar 打包失败 (exitCode=%1, err=%2)，转入 PowerShell 容灾打包...")
                                    .arg(exitCode)
                                    .arg(stdErr);
    }

    // PowerShell Compress-Archive 容灾备用
    {
        QString escapedSrc = nativeSrc;
        escapedSrc.replace("'", "''");
        QString escapedZip = nativeZip;
        escapedZip.replace("'", "''");

        const QString cmd = QString("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
                                .arg(escapedSrc, escapedZip);
        const QStringList args{"-NoProfile", "-NonInteractive", "-Command", cmd};

        int exitCode = -1;
        QString stdErr;
        if (runSafeProcess("powershell", args, timeoutMs, &exitCode, &stdErr)) {
            const QFileInfo checkOut(nativeZip);
            if (checkOut.exists() && checkOut.size() > 0) {
                qDebug().noquote() << QString("[ZipUtil] 成功打包 (PowerShell): %1 -> %2").arg(nativeSrc, nativeZip);
                return true;
            }
        }

        const QString err = QString("PowerShell 打包亦失败 (exitCode=%1, error=%2)").arg(exitCode).arg(stdErr);
        qWarning().noquote() << "[ZipUtil]" << err;
        if (errorMessage) *errorMessage = err;
    }

    return false;
}
