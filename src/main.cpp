// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "privilegedhelper.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QString>
#include <cstdio>

namespace
{

QString instanceRuntimePath(
    const QString &fileName
)
{
    QString runtimePath =
        QStandardPaths::writableLocation(
            QStandardPaths::RuntimeLocation
        );

    if (runtimePath.isEmpty()) {
        runtimePath =
            QDir::tempPath() +
            QStringLiteral("/lindiskinfo-") +
            qEnvironmentVariable(
                "USER",
                QStringLiteral("user")
            );
    }

    QDir().mkpath(runtimePath);

    return QDir(runtimePath)
        .filePath(fileName);
}


bool activateExistingInstance(
    const QString &serverName
)
{
    for (int attempt = 0;
         attempt < 20;
         ++attempt) {

        QLocalSocket socket;

        socket.connectToServer(
            serverName,
            QIODevice::WriteOnly
        );

        if (socket.waitForConnected(100)) {
            socket.write("activate\n");
            socket.flush();
            socket.waitForBytesWritten(200);
            socket.disconnectFromServer();

            return true;
        }
    }

    return false;
}

}


int main(int argc, char *argv[])
{
    if (argc >= 2 &&
        QString::fromLocal8Bit(argv[1]) ==
            QStringLiteral("--privileged-helper")) {

        QCoreApplication app(argc, argv);

        return runPrivilegedHelper();
    }

#if defined(Q_OS_UNIX)
    if (!qEnvironmentVariableIsSet(
            "LINDISKINFO_DEBUG"
        )) {

        std::freopen(
            "/dev/null",
            "w",
            stdout
        );

        std::freopen(
            "/dev/null",
            "w",
            stderr
        );
    }
#endif

    QApplication app(argc, argv);

    QGuiApplication::setDesktopFileName(
        QStringLiteral(
            "lindiskinfo"
        )
    );

    const QIcon applicationIcon(
        QStringLiteral(
            ":/icons/lindiskinfo.png"
        )
    );

    app.setWindowIcon(
        applicationIcon
    );

    QCoreApplication::setApplicationName(
        QStringLiteral("LinDiskInfo")
    );

    QCoreApplication::setApplicationVersion(
        QStringLiteral("1.0.4")
    );

    QCoreApplication::setOrganizationName(
        QStringLiteral("LinDiskInfo")
    );


    const QString lockPath =
        instanceRuntimePath(
            QStringLiteral(
                "lindiskinfo.lock"
            )
        );

    const QString serverName =
        instanceRuntimePath(
            QStringLiteral(
                "lindiskinfo.socket"
            )
        );

    QLockFile instanceLock(lockPath);

    instanceLock.setStaleLockTime(0);

    if (!instanceLock.tryLock(0)) {
        activateExistingInstance(
            serverName
        );

        return 0;
    }

    QLocalServer::removeServer(
        serverName
    );

    QLocalServer instanceServer;

    instanceServer.setSocketOptions(
        QLocalServer::UserAccessOption
    );

    if (!instanceServer.listen(
            serverName
        )) {

        return 1;
    }


    MainWindow window;

    QObject::connect(
        &instanceServer,
        &QLocalServer::newConnection,
        &window,
        [&instanceServer, &window]
        {
            while (
                instanceServer
                    .hasPendingConnections()
            ) {
                QLocalSocket *socket =
                    instanceServer
                        .nextPendingConnection();

                if (!socket)
                    continue;

                socket->readAll();

                socket->disconnectFromServer();

                socket->deleteLater();
            }

            if (window.isMinimized()) {
                window.showNormal();

            } else if (!window.isVisible()) {
                window.show();
            }

            window.raise();
            window.activateWindow();
        }
    );

    window.show();

    return app.exec();
}
