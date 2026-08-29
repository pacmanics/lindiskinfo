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

void MainWindow::setAutoRefreshInterval(
    int minutes
)
{
    m_autoRefreshMinutes = minutes;

    QSettings().setValue(
        QStringLiteral("autoRefreshMinutes"),
        minutes
    );

    if (!m_autoRefreshTimer)
        return;

    m_autoRefreshTimer->stop();

    if (minutes > 0)
        m_autoRefreshTimer->start(
            minutes * 60 * 1000
        );
}

void MainWindow::setAutoDetectionInterval(
    int seconds
)
{
    m_autoDetectionSeconds = seconds;

    QSettings().setValue(
        QStringLiteral("autoDetectionSeconds"),
        seconds
    );

    if (!m_autoDetectionTimer)
        return;

    m_autoDetectionTimer->stop();

    if (seconds > 0)
        m_autoDetectionTimer->start(
            seconds * 1000
        );
}

void MainWindow::setZoomPercent(
    int percent
)
{
    m_zoomPercent = percent;

    QSettings().setValue(
        QStringLiteral("zoomPercent"),
        percent
    );

    const double factor =
        static_cast<double>(percent) /
        100.0;

    QFont normal = m_baseFont;

    if (normal.pointSizeF() > 0.0)
        normal.setPointSizeF(
            normal.pointSizeF() * factor
        );

    qApp->setFont(normal);

    QFont title = normal;
    title.setPointSizeF(
        normal.pointSizeF() + 7.0 * factor
    );

    m_titleLabel->setFont(title);

    QFont health = normal;
    health.setPointSizeF(
        normal.pointSizeF() + 5.0 * factor
    );
    health.setBold(true);

    m_healthValue->setFont(health);

    QFont temperature = normal;
    temperature.setPointSizeF(
        normal.pointSizeF() + 7.0 * factor
    );
    temperature.setBold(true);

    m_temperatureValue->setFont(
        temperature
    );
}

QString MainWindow::autostartPath() const
{
    return
        QStandardPaths::writableLocation(
            QStandardPaths::ConfigLocation
        ) +
        QStringLiteral(
            "/autostart/lindiskinfo.desktop"
        );
}

void MainWindow::setStartWithSystem(
    bool enabled
)
{
    const QString path =
        autostartPath();

    if (!enabled) {
        QFile::remove(path);
        return;
    }

    QDir().mkpath(
        QFileInfo(path).absolutePath()
    );

    QFile file(path);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text
        )) {

        QSignalBlocker blocker(
            m_startWithSystemAction
        );

        m_startWithSystemAction->setChecked(
            false
        );

        return;
    }

    QString executable =
        QCoreApplication::applicationFilePath();

    executable.replace(
        QLatin1Char('\\'),
        QStringLiteral("\\\\")
    );

    executable.replace(
        QLatin1Char('"'),
        QStringLiteral("\\\"")
    );

    QTextStream out(&file);

    out
        << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=LinDiskInfo\n"
        << "Exec=\"" << executable
        << "\" --autostart\n"
        << "Icon=lindiskinfo\n"
        << "Terminal=false\n";
}

void MainWindow::saveUiState() const
{
    QSettings settings;

    settings.setValue(
        QStringLiteral(
            "windowGeometry"
        ),
        saveGeometry()
    );

    saveCurrentTableWidths();

    if (m_selectedDrive >= 0 &&
        m_selectedDrive <
            m_drives.size()) {

        const DriveInfo &drive =
            m_drives.at(
                m_selectedDrive
            );

        settings.setValue(
            QStringLiteral(
                "lastDrive"
            ),
            drive.name
        );

        settings.setValue(
            QStringLiteral(
                "lastDriveIdentity"
            ),
            lindiskinfoDriveIdentity(
                drive
            )
        );
    }
}

void MainWindow::saveCurrentTableWidths() const
{
    if (!m_table ||
        !m_tableLayoutReady) {
        return;
    }

    QVariantList widths;

    for (int column = 0;
         column <
            m_table->columnCount();
         ++column) {

        widths.append(
            m_table->columnWidth(
                column
            )
        );
    }

    QSettings().setValue(
        m_currentTableIsNvme
            ? QStringLiteral(
                  "tableWidthsNvme"
              )
            : QStringLiteral(
                  "tableWidthsAta"
              ),
        widths
    );
}

void MainWindow::restoreCurrentTableWidths()
{
    if (!m_table)
        return;

    const QVariantList widths =
        QSettings().value(
            m_currentTableIsNvme
                ? QStringLiteral(
                      "tableWidthsNvme"
                  )
                : QStringLiteral(
                      "tableWidthsAta"
                  )
        ).toList();

    if (widths.size() ==
        m_table->columnCount()) {

        for (int column = 0;
             column <
                m_table->columnCount();
             ++column) {

            const int width =
                widths.at(column)
                    .toInt();

            if (width >= 20) {
                m_table->setColumnWidth(
                    column,
                    width
                );
            }
        }
    }

    m_tableLayoutReady = true;
}
