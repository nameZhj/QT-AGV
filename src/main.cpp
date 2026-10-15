#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QLocale>
#include <QTranslator>
#include <QLibraryInfo>
#include <QIcon>
#include <QNetworkProxy>
#include "ui/mainwindow/MainWindow.h"
#include "ui/dialogs/DeviceManagerDialog.h"
#include "app/AppConfig.h"
#include "ui/common/ButtonClickEffect.h"
#include "ui/common/SpinBoxWheelFilter.h"
#include "ui/common/StyleLoader.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static QFile *g_logFile = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);
    const char *level = "DEBUG";
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO "; break;
    case QtWarningMsg:  level = "WARN "; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }
    QString ts  = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString line = QString("[%1] [%2] %3").arg(ts, level, msg);

#ifdef Q_OS_WIN
    // Windows 控制台优先使用 WriteConsoleW 原生 Unicode 写入，防止终端控制台中文乱码
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode;
    if (hErr && hErr != INVALID_HANDLE_VALUE && GetConsoleMode(hErr, &mode)) {
        std::wstring wstr = (line + "\n").toStdWString();
        DWORD written = 0;
        WriteConsoleW(hErr, wstr.c_str(), static_cast<DWORD>(wstr.length()), &written, NULL);
    } else {
        // 输出到终端控制台/管道，统一按 UTF-8 编码输出，匹配 SetConsoleOutputCP(CP_UTF8)
        QByteArray utf8 = line.toUtf8();
        fprintf(stderr, "%s\n", utf8.constData());
        fflush(stderr);
    }
#else
    QByteArray utf8 = line.toUtf8();
    fprintf(stderr, "%s\n", utf8.constData());
    fflush(stderr);
#endif

    if (g_logFile && g_logFile->isOpen()) {
        QByteArray utf8 = line.toUtf8();
        g_logFile->write(utf8 + "\n");
        g_logFile->flush();
    }
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 设置控制台输入输出代码页为 UTF-8 (Code Page 65001)，解决 Windows 终端控制台中文输出乱码问题
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    // 仅在调试环境(Debug)下在运行 .exe 时分配/附着控制台窗口
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    // 在最早阶段安装日志处理器
    qInstallMessageHandler(messageHandler);

    QApplication app(argc, argv);
    app.setApplicationName("QT-AGV");
    app.setOrganizationName("AGV");

    // 禁用系统代理。本应用连接局域网 AGV 服务器，QTcpSocket/QWebSocket/
    // QNetworkAccessManager 默认会使用系统代理(含 VPN/科学上网工具)，导致
    // 原始 MQTT TCP 流量被路由到 HTTP/SOCKS 代理而连接失败(TransportInvalid)。
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    // 设置中文locale，让Qt内置对话框(OK/Cancel/Close等)显示中文
    QLocale::setDefault(QLocale::Chinese);
    // 加载Qt内置翻译(让QMessageBox/QInputDialog的标准按钮显示中文)
    static QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::Chinese, "qtbase", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // 设置应用图标
    app.setWindowIcon(QIcon(":/app-icon.svg"));

    // 应用全局样式表 (加载 resources/qss/ 下所有模块化样式)
    StyleLoader::applyApplicationStyle();

    // 全局按钮点击动效，对所有按钮统一生效
    app.installEventFilter(new ButtonClickEffect(&app));

    // 全局步进器滚轮防误触：只有在点击获取焦点后才响应滚轮
    app.installEventFilter(new SpinBoxWheelFilter(&app));

    // Log file
    QString logPath = app.applicationDirPath() + "/qt-agv.log";
    bool fileExisted = QFile::exists(logPath);
    g_logFile = new QFile(logPath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (!fileExisted || g_logFile->size() == 0) {
            static const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
            g_logFile->write(reinterpret_cast<const char *>(bom), sizeof(bom));
            g_logFile->flush();
        }
    }

    qInfo() << "QT-AGV starting";

    // Startup: show device manager dialog
    DeviceManagerDialog dlg;
    if (dlg.exec() != QDialog::Accepted) {
        if (g_logFile) { g_logFile->close(); delete g_logFile; }
        return 0;
    }

    qInfo() << "Device selected:"
            << AppConfig::instance().serverIp()
            << "carId:" << AppConfig::instance().agvId();

    MainWindow window;
    window.show();

    int ret = app.exec();

    if (g_logFile) { g_logFile->close(); delete g_logFile; g_logFile = nullptr; }
    return ret;
}
