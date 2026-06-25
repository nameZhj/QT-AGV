#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QLocale>
#include <QTranslator>
#include <QLibraryInfo>
#include <QIcon>
#include "core/MainWindow.h"
#include "core/DeviceManagerDialog.h"
#include "core/AppConfig.h"
#include "core/ButtonClickEffect.h"

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

    fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
    fflush(stderr);

    if (g_logFile && g_logFile->isOpen()) {
        QTextStream s(g_logFile);
        s << line << "\n";
        s.flush();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QT-AGV");
    app.setOrganizationName("AGV");

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

    // 全局按钮点击动效，对所有按钮统一生效
    app.installEventFilter(new ButtonClickEffect(&app));

    // Log file
    QString logPath = app.applicationDirPath() + "/qt-agv.log";
    g_logFile = new QFile(logPath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        qInstallMessageHandler(messageHandler);

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
