/**
 * @file    NetworkConfigSshRunner.h
 * @layer   GUILayer
 * @brief   OpenSSH process runner and network connectivity testing utilities.
 * @deps    QString, QWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

class QWidget;

class NetworkConfigSshRunner
{
public:
    static bool validateIPv4(QWidget *parent, const QString &ip, const QString &fieldName);
    static bool checkIpConnectivity(const QString &ip, int port, int timeoutMs = 1500);
    static bool runSshCommand(const QString &host, int port, const QString &user, const QString &pwd,
                              const QString &command, QString &stdOut, QString &stdErr, int timeoutMs = 15000);
};
