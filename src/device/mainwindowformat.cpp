// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"

#include <QCryptographicHash>
#include <QVariantList>
#include <QHBoxLayout>
#include <QSaveFile>
#include <QPolygonF>
#include <QPaintEvent>
#include <QPainter>
#include <QLocale>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QFontDatabase>
#include <QCheckBox>
#include <QStringList>
#include <QSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QCloseEvent>
#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QCoreApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFontDialog>
#include <QGuiApplication>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

QString MainWindow::formatTemperature(
    int celsius
) const
{
    if (m_temperatureUnit ==
        TemperatureUnit::Fahrenheit) {

        const double fahrenheit =
            (static_cast<double>(celsius) * 9.0 / 5.0) + 32.0;

        return QStringLiteral("%1 °F")
            .arg(
                fahrenheit,
                0,
                'f',
                0
            );
    }

    return QStringLiteral("%1 °C")
        .arg(celsius);
}

quint64 MainWindow::jsonUnsigned(
    const QJsonValue &value
)
{
    if (value.isDouble()) {
        const qint64 integer =
            value.toInteger();

        return integer < 0
            ? 0
            : static_cast<quint64>(
                  integer
              );
    }

    if (value.isString()) {
        bool ok = false;

        const quint64 result =
            value.toString()
                .toULongLong(&ok);

        return ok ? result : 0;
    }

    return 0;
}

QString MainWindow::formatBytes(
    quint64 bytes
)
{
    const QString unit =
        QSettings().value(
            QStringLiteral(
                "storageUnit"
            ),
            QStringLiteral("GB")
        ).toString();

    long double divisor =
        1000000000.0L;

    QString suffix =
        QStringLiteral("GB");

    if (unit == QStringLiteral("GiB")) {
        divisor =
            1073741824.0L;

        suffix =
            QStringLiteral("GiB");

    } else if (
        unit == QStringLiteral("TB")
    ) {
        divisor =
            1000000000000.0L;

        suffix =
            QStringLiteral("TB");

    } else if (
        unit == QStringLiteral("TiB")
    ) {
        divisor =
            1099511627776.0L;

        suffix =
            QStringLiteral("TiB");
    }

    const long double value =
        static_cast<long double>(
            bytes
        ) / divisor;

    int decimals = 1;

    if (value < 1.0L) {
        decimals = 3;
    } else if (value < 10.0L) {
        decimals = 2;
    }

    QString number =
        QString::number(
            static_cast<double>(value),
            'f',
            decimals
        );

    while (
        number.contains(
            QLatin1Char('.')
        ) &&
        number.endsWith(
            QLatin1Char('0')
        )
    ) {
        number.chop(1);
    }

    if (number.endsWith(
            QLatin1Char('.')
        )) {
        number.chop(1);
    }

    return QStringLiteral(
        "%1 %2"
    ).arg(
        number,
        suffix
    );
}

QString MainWindow::formatNumber(
    quint64 value
)
{
    QString text =
        QString::number(value);

    for (int i = text.size() - 3;
         i > 0;
         i -= 3) {

        text.insert(
            i,
            QLatin1Char('.')
        );
    }

    return text;
}

QString MainWindow::formatRawValue(
    quint64 value
) const
{
    const QString mode =
        QSettings().value(
            QStringLiteral(
                "rawValueMode"
            ),
            QStringLiteral("hex")
        ).toString();

    if (mode ==
        QStringLiteral("dec")) {

        return QString::number(
            value
        );
    }

    if (mode ==
        QStringLiteral("dec2")) {

        QStringList groups;

        for (int shift = 48;
             shift >= 0;
             shift -= 16) {

            const quint64 part =
                (value >> shift) &
                0xFFFFULL;

            groups.append(
                QStringLiteral("%1")
                    .arg(
                        part,
                        5,
                        10,
                        QLatin1Char('0')
                    )
            );
        }

        return groups.join(
            QLatin1Char(' ')
        );
    }

    if (mode ==
        QStringLiteral("dec1")) {

        QStringList groups;

        for (int shift = 56;
             shift >= 0;
             shift -= 8) {

            const quint64 part =
                (value >> shift) &
                0xFFULL;

            groups.append(
                QStringLiteral("%1")
                    .arg(
                        part,
                        3,
                        10,
                        QLatin1Char('0')
                    )
            );
        }

        return groups.join(
            QLatin1Char(' ')
        );
    }

    return QStringLiteral("%1")
        .arg(
            value,
            16,
            16,
            QLatin1Char('0')
        )
        .toUpper();
}
