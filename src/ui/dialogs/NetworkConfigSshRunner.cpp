/**
 * @file    NetworkConfigSshRunner.cpp
 * @layer   GUILayer
 * @brief   OpenSSH process runner and network connectivity testing utilities.
 * @deps    NetworkConfigSshRunner.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "NetworkConfigSshRunner.h"

#include <QWidget>
#include <QMessageBox>
#include <QHostAddress>
#include <QTcpSocket>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>
#include <QStandardPaths>

bool NetworkConfigSshRunner::validateIPv4(QWidget *parent, const QString &ip,
                                         const QString &fieldName) {
  QString trimmed = ip.trimmed();
  if (trimmed.isEmpty()) {
    QMessageBox::warning(parent, "参数校验", QString("%1 不能为空").arg(fieldName));
    return false;
  }
  QHostAddress addr(trimmed);
  if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
    QMessageBox::warning(
        parent, "参数校验",
        QString("%1 格式不正确，请输入有效的 IPv4 地址 (例如 192.168.1.150)").arg(fieldName));
    return false;
  }
  return true;
}

bool NetworkConfigSshRunner::checkIpConnectivity(const QString &ip, int port, int timeoutMs) {
  QTcpSocket socket;
  socket.connectToHost(ip, static_cast<quint16>(port));
  if (socket.waitForConnected(timeoutMs)) {
    socket.disconnectFromHost();
    return true;
  }
  if (socket.error() == QAbstractSocket::ConnectionRefusedError) {
    return true;
  }

  QProcess pingProc;
  QStringList pingArgs;
#ifdef Q_OS_WIN
  pingArgs << "-n" << "1" << "-w" << QString::number(timeoutMs) << ip;
  pingProc.start("ping.exe", pingArgs);
#else
  pingArgs << "-c" << "1" << "-W" << QString::number(qMax(1, timeoutMs / 1000)) << ip;
  pingProc.start("ping", pingArgs);
#endif
  if (pingProc.waitForFinished(timeoutMs + 1000)) {
    if (pingProc.exitCode() == 0) {
      return true;
    }
  }

  return false;
}

static QString locateSshExecutable() {
#ifdef Q_OS_WIN
  QString winOpenSsh = "C:\\Windows\\System32\\OpenSSH\\ssh.exe";
  if (QFile::exists(winOpenSsh)) return winOpenSsh;
  QString found = QStandardPaths::findExecutable("ssh.exe");
  if (!found.isEmpty()) return found;
  return QStandardPaths::findExecutable("ssh");
#else
  return QStandardPaths::findExecutable("ssh");
#endif
}

bool NetworkConfigSshRunner::runSshCommand(const QString &host, int port,
                                          const QString &user, const QString &pwd,
                                          const QString &command, QString &stdOut,
                                          QString &stdErr, int timeoutMs) {
  QString sshPath = locateSshExecutable();
  if (sshPath.isEmpty()) {
    stdErr = "未找到系统的 OpenSSH (ssh.exe) 客户端程序，请确保系统已安装 OpenSSH。";
    return false;
  }

  QString tempBatPath = QDir::tempPath() + QString("/qt_agv_askpass_%1_%2.bat")
                             .arg(QCoreApplication::applicationPid())
                             .arg(QDateTime::currentMSecsSinceEpoch());
  QFile batFile(tempBatPath);
  if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    batFile.write("@echo off\r\necho %AGV_SSH_PWD%\r\n");
    batFile.close();
  }

  QProcess process;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("SSH_ASKPASS", QDir::toNativeSeparators(tempBatPath));
  env.insert("SSH_ASKPASS_REQUIRE", "force");
  env.insert("AGV_SSH_PWD", pwd);
  env.insert("DISPLAY", "dummy:0");
  process.setProcessEnvironment(env);

  QStringList args;
  args << "-p" << QString::number(port);
  args << "-o" << "StrictHostKeyChecking=no";
  args << "-o" << "UserKnownHostsFile=NUL";
  args << "-o" << "ConnectTimeout=6";
  args << "-o" << "PreferredAuthentications=password,keyboard-interactive";
  args << "-o" << "NumberOfPasswordPrompts=1";
  args << "-o" << "LogLevel=ERROR";
  args << QString("%1@%2").arg(user, host);
  args << command;

  process.start(sshPath, args);
  bool finished = process.waitForFinished(timeoutMs);

  QFile::remove(tempBatPath);

  if (!finished) {
    process.kill();
    stdErr = QString("SSH 连接或执行超时 (超过 %1 秒无响应)").arg(timeoutMs / 1000);
    return false;
  }

  stdOut = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  stdErr = QString::fromUtf8(process.readAllStandardError()).trimmed();

  return (process.exitCode() == 0);
}
