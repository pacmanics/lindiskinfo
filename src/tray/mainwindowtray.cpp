// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../theme/themebackgroundwidget.h"
#include "../theme/waifuthemes.h"
#include "../ui/responsivetablelayout.h"

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
#include <utility>

void MainWindow::closeEvent(
    QCloseEvent *event
)
{
    saveUiState();

    if (m_forceQuit ||
        !m_showTrayAction ||
        !m_showTrayAction->isChecked() ||
        !m_trayIcon ||
        !m_trayIcon->isVisible()) {

        QMainWindow::closeEvent(
            event
        );

        return;
    }

    if (m_trayBehavior ==
        QStringLiteral(
            "minimize"
        )) {

        showMinimized();
        event->ignore();

        rebuildTrayMenu();

        return;
    }

    hide();
    event->ignore();

    rebuildTrayMenu();
}

void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::
            isSystemTrayAvailable()) {

        m_showTrayAction
            ->setEnabled(false);

        return;
    }

    m_trayIcon =
        new QSystemTrayIcon(
            this
        );

    m_trayMenu =
        new QMenu(this);

    m_trayIcon->setContextMenu(
        m_trayMenu
    );

    m_trayIcon->setIcon(
        qApp->windowIcon()
    );

    m_trayIcon->setToolTip(
        QStringLiteral(
            "LinDiskInfo"
        )
    );

    connect(
        m_trayIcon,
        &QSystemTrayIcon::activated,
        this,
        [this](
            QSystemTrayIcon::
                ActivationReason reason
        )
        {
            if (reason !=
                QSystemTrayIcon::Trigger) {

                return;
            }

            if (isVisible() &&
                !isMinimized()) {

                hide();
                return;
            }

            showNormal();
            raise();
            activateWindow();
        }
    );

    connect(
        m_showTrayAction,
        &QAction::toggled,
        this,
        [this](bool)
        {
            rebuildTrayMenu();
            updateTrayPresentation();
        }
    );

    rebuildTrayMenu();
    updateTrayPresentation();

    if (m_showTrayAction->isChecked())
        m_trayIcon->show();
}

void MainWindow::rebuildTrayMenu()
{
    if (!m_trayMenu)
        return;

    m_trayMenu->clear();

    QAction *windowAction =
        m_trayMenu->addAction(
            isVisible() &&
            !isMinimized()
                ? tx(
                      "Hide LinDiskInfo",
                      "LinDiskInfo ausblenden"
                  )
                : tx(
                      "Show LinDiskInfo",
                      "LinDiskInfo anzeigen"
                  )
        );

    connect(
        windowAction,
        &QAction::triggered,
        this,
        [this]
        {
            if (isVisible() &&
                !isMinimized()) {

                hide();

            } else {
                showNormal();
                raise();
                activateWindow();
            }

            rebuildTrayMenu();
        }
    );

    m_trayMenu->addSeparator();


    if (m_drives.isEmpty()) {
        QAction *empty =
            m_trayMenu->addAction(
                tx(
                    "No drives found",
                    "Keine Laufwerke gefunden"
                )
            );

        empty->setEnabled(false);

    } else {
        for (int i = 0;
             i < m_drives.size();
             ++i) {

            const DriveInfo &drive =
                m_drives.at(i);

            QString model =
                drive.infoName;

            if (m_driveData.contains(
                    lindiskinfoDriveIdentity(drive)
                )) {

                const QJsonObject data =
                    m_driveData.value(
                        lindiskinfoDriveIdentity(drive)
                    );

                const QString candidate =
                    data.value(
                        QStringLiteral(
                            "model_name"
                        )
                    ).toString(
                        data.value(
                            QStringLiteral(
                                "model_number"
                            )
                        ).toString()
                    );

                if (!candidate.isEmpty())
                    model = candidate;
            }

            if (model.isEmpty())
                model = drive.name;

            QString stateText =
                QStringLiteral("?");

            QString temperatureText =
                m_temperatureUnit ==
                    TemperatureUnit::
                        Fahrenheit
                    ? QStringLiteral(
                          "-- °F"
                      )
                    : QStringLiteral(
                          "-- °C"
                      );

            if (m_driveData.contains(
                    lindiskinfoDriveIdentity(drive)
                )) {

                const QJsonObject data =
                    m_driveData.value(
                        lindiskinfoDriveIdentity(drive)
                    );

                HealthState state =
                    healthStateForData(
                        data
                    );

                const int temperature =
                    temperatureForData(
                        data
                    );

                if (temperature >=
                    m_temperatureBad) {

                    state =
                        HealthState::Bad;

                } else if (
                    temperature >=
                        m_temperatureCaution &&
                    state !=
                        HealthState::Bad) {

                    state =
                        HealthState::Caution;
                }

                switch (state) {
                case HealthState::Good:
                    stateText =
                        tx(
                            "Good",
                            "Gut"
                        );
                    break;

                case HealthState::Caution:
                    stateText =
                        tx(
                            "Caution",
                            "Vorsicht"
                        );
                    break;

                case HealthState::Bad:
                    stateText =
                        tx(
                            "Bad",
                            "Schlecht"
                        );
                    break;

                case HealthState::Unknown:
                    stateText =
                        tx(
                            "Unknown",
                            "Unbekannt"
                        );
                    break;
                }

                if (temperature > 0) {
                    temperatureText =
                        formatTemperature(
                            temperature
                        );
                }
            }

            QString label;

            if (i == m_selectedDrive)
                label +=
                    QStringLiteral("✓ ");

            label +=
                QStringLiteral(
                    "%1  %2  %3  [%4]"
                ).arg(
                    stateText,
                    temperatureText,
                    model,
                    driveDisplayIdentifier(
                        drive
                    )
                );

            QAction *entry =
                m_trayMenu->addAction(
                    label
                );

            connect(
                entry,
                &QAction::triggered,
                this,
                [this, i]
                {
                    if (i < 0 ||
                        i >=
                            m_drives.size()) {

                        return;
                    }

                    selectDrive(i);

                    showNormal();
                    raise();
                    activateWindow();

                    rebuildTrayMenu();
                }
            );
        }
    }


    m_trayMenu->addSeparator();


    QAction *refresh =
        m_trayMenu->addAction(
            tx(
                "Refresh",
                "Aktualisieren"
            )
        );

    connect(
        refresh,
        &QAction::triggered,
        this,
        &MainWindow::
            refreshAllData
    );


    QAction *quit =
        m_trayMenu->addAction(
            tx(
                "Exit",
                "Beenden"
            )
        );

    connect(
        quit,
        &QAction::triggered,
        this,
        [this]
        {
            saveUiState();

            m_forceQuit = true;

            close();
        }
    );
}

void MainWindow::updateTrayPresentation()
{
    if (!m_trayIcon)
        return;

    QStringList lines;

    lines.append(
        QStringLiteral(
            "LinDiskInfo"
        )
    );

    int worstSeverity = 0;


    for (const DriveInfo &drive :
         m_drives) {

        QString model =
            drive.infoName;

        if (model.isEmpty())
            model = drive.name;

        QString status =
            tx(
                "Unknown",
                "Unbekannt"
            );

        QString temperatureText =
            m_temperatureUnit ==
                TemperatureUnit::
                    Fahrenheit
                ? QStringLiteral(
                      "-- °F"
                  )
                : QStringLiteral(
                      "-- °C"
                  );

        int severity = 0;


        if (m_driveData.contains(
                lindiskinfoDriveIdentity(drive)
            )) {

            const QJsonObject data =
                m_driveData.value(
                    lindiskinfoDriveIdentity(drive)
                );

            const QString dataModel =
                data.value(
                    QStringLiteral(
                        "model_name"
                    )
                ).toString(
                    data.value(
                        QStringLiteral(
                            "model_number"
                        )
                    ).toString()
                );

            if (!dataModel.isEmpty())
                model = dataModel;


            HealthState state =
                healthStateForData(
                    data
                );

            const int temperature =
                temperatureForData(
                    data
                );


            if (temperature >=
                m_temperatureBad) {

                state =
                    HealthState::Bad;

            } else if (
                temperature >=
                    m_temperatureCaution &&
                state !=
                    HealthState::Bad) {

                state =
                    HealthState::
                        Caution;
            }


            switch (state) {
            case HealthState::Good:
                status =
                    tx(
                        "Good",
                        "Gut"
                    );

                severity = 0;
                break;

            case HealthState::Caution:
                status =
                    tx(
                        "Caution",
                        "Vorsicht"
                    );

                severity = 1;
                break;

            case HealthState::Bad:
                status =
                    tx(
                        "Bad",
                        "Schlecht"
                    );

                severity = 2;
                break;

            case HealthState::Unknown:
                status =
                    tx(
                        "Unknown",
                        "Unbekannt"
                    );

                severity = 0;
                break;
            }


            if (temperature > 0) {
                temperatureText =
                    formatTemperature(
                        temperature
                    );
            }


            const QString driveIdentity =
                lindiskinfoDriveIdentity(
                    drive
                );

            if (m_lastTraySeverity
                    .contains(
                        driveIdentity
                    )) {

                const int oldSeverity =
                    m_lastTraySeverity
                        .value(
                            driveIdentity
                        );

                if (severity >
                        oldSeverity &&
                    severity > 0 &&
                    m_trayIcon
                        ->isVisible()) {

                    m_trayIcon->showMessage(
                        QStringLiteral(
                            "LinDiskInfo"
                        ),
                        severity >= 2
                            ? tx(
                                  "Bad health state detected for %1.",
                                  "Schlechter Laufwerkszustand bei %1 erkannt."
                              ).arg(model)
                            : tx(
                                  "Caution state detected for %1.",
                                  "Warnzustand bei %1 erkannt."
                              ).arg(model),
                        severity >= 2
                            ? QSystemTrayIcon::
                                  Critical
                            : QSystemTrayIcon::
                                  Warning,
                        8000
                    );
                }
            }


            m_lastTraySeverity.insert(
                driveIdentity,
                severity
            );
        }


        worstSeverity =
            std::max(
                worstSeverity,
                severity
            );


        QString shortModel =
            model;

        if (shortModel.size() > 36) {
            shortModel =
                shortModel.left(33) +
                QStringLiteral("...");
        }

        if (driveDisplayIdentifier(
                drive
            ) != drive.name &&
            !drive.type.trimmed().isEmpty()) {

            shortModel +=
                QStringLiteral(
                    " [%1]"
                ).arg(
                    drive.type.trimmed()
                );
        }


        lines.append(
            QStringLiteral(
                "%1: %2, %3"
            ).arg(
                shortModel,
                status,
                temperatureText
            )
        );
    }


    m_trayIcon->setToolTip(
        lines.join(
            QLatin1Char('\n')
        )
    );


    QIcon icon;

    if (worstSeverity >= 2) {
        icon =
            QIcon::fromTheme(
                QStringLiteral(
                    "dialog-error"
                ),
                qApp->windowIcon()
            );

    } else if (
        worstSeverity == 1) {

        icon =
            QIcon::fromTheme(
                QStringLiteral(
                    "dialog-warning"
                ),
                qApp->windowIcon()
            );

    } else {
        icon =
            qApp->windowIcon();
    }


    if (!icon.isNull()) {
        m_trayIcon->setIcon(
            icon
        );
    }
}
