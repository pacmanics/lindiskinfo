// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "privilegedhelper.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QString>
#include <cstdio>

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

    const QIcon applicationIcon =
        QIcon::fromTheme(
            QStringLiteral(
                "lindiskinfo"
            ),
            QIcon(
                QStringLiteral(
                    ":/icons/lindiskinfo.png"
                )
            )
        );

    app.setWindowIcon(
        applicationIcon
    );

    QCoreApplication::setApplicationName(
        QStringLiteral("LinDiskInfo")
    );

    QCoreApplication::setApplicationVersion(
        QStringLiteral("1.0.1")
    );

    QCoreApplication::setOrganizationName(
        QStringLiteral("LinDiskInfo")
    );


    MainWindow window;
    window.show();

    return app.exec();
}
