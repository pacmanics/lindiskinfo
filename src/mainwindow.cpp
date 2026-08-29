// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "device/atawear.h"
#include "theme/themebackgroundwidget.h"
#include "theme/waifuthemes.h"
#include "ui/responsivetablelayout.h"
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

namespace
{

constexpr int ColumnStatus = 0;
constexpr int ColumnId = 1;
constexpr int ColumnAttribute = 2;
constexpr int ColumnValue = 3;
constexpr int ColumnCurrent = 4;
constexpr int ColumnWorst = 5;
constexpr int ColumnThreshold = 6;
constexpr int ColumnRaw = 7;



}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_backend(new SmartctlBackend(this))
{
    QSettings settings;

    m_systemPalette = qApp->palette();
    m_baseFont = qApp->font();

    const QString savedLanguage =
        settings.value(
            QStringLiteral("language"),
            QStringLiteral("en")
        ).toString();

    if (savedLanguage == QStringLiteral("de")) {

        m_language = Language::German;

    }

        const QString savedTheme =
        settings.value(
            QStringLiteral("theme"),
            QStringLiteral("system")
        ).toString();

    m_darkMode =
        savedTheme == QStringLiteral("dark") ||
        settings.value(
            QStringLiteral("darkMode"),
            false
        ).toBool();

const QString savedTemperatureUnit =
        settings.value(
            QStringLiteral("temperatureUnit"),
            QStringLiteral("celsius")
        ).toString();

    if (savedTemperatureUnit ==
        QStringLiteral("fahrenheit")) {
        m_temperatureUnit =
            TemperatureUnit::Fahrenheit;
    }

    m_temperatureCaution =
        settings.value(
            QStringLiteral(
                "temperatureCaution"
            ),
            55
        ).toInt();

    m_temperatureBad =
        settings.value(
            QStringLiteral(
                "temperatureBad"
            ),
            70
        ).toInt();

    m_nvmeCautionRemaining =
        settings.value(
            QStringLiteral(
                "nvmeCautionRemaining"
            ),
            10
        ).toInt();

    m_nvmeBadRemaining =
        settings.value(
            QStringLiteral(
                "nvmeBadRemaining"
            ),
            0
        ).toInt();

    m_ataSectorCautionCount =
        settings.value(
            QStringLiteral(
                "ataSectorCautionCount"
            ),
            1
        ).toULongLong();

    m_startupDelaySeconds =
        settings.value(
            QStringLiteral(
                "startupDelaySeconds"
            ),
            0
        ).toInt();

    m_driveSortMethod =
        settings.value(
            QStringLiteral(
                "driveSortMethod"
            ),
            QStringLiteral("default")
        ).toString();

    m_displayDriveLimit =
        settings.value(
            QStringLiteral(
                "displayDriveLimit"
            ),
            0
        ).toInt();

    m_trayBehavior =
        settings.value(
            QStringLiteral(
                "trayBehavior"
            ),
            QStringLiteral("hide")
        ).toString();

    m_hideNoSmart =
        settings.value(
            QStringLiteral(
                "hideNoSmart"
            ),
            false
        ).toBool();

    m_aamApmAutoEnabled =
        settings.value(
            QStringLiteral(
                "aamApmAutoAdjustment"
            ),
            false
        ).toBool();

    m_historyEnabled =
        settings.value(
            QStringLiteral(
                "historyEnabled"
            ),
            true
        ).toBool();

    loadHistory();

    m_ataPassThroughEnabled =
        settings.value(
            QStringLiteral(
                "ataPassThrough"
            ),
            true
        ).toBool();

    m_externalStorageEnabled =
        settings.value(
            QStringLiteral(
                "externalStorage"
            ),
            true
        ).toBool();

    m_megaRaidEnabled =
        settings.value(
            QStringLiteral(
                "megaRaidPhysicalDrives"
            ),
            true
        ).toBool();

    applyTheme();

    setWindowTitle(
        QStringLiteral("LinDiskInfo %1")
            .arg(QCoreApplication::applicationVersion())
    );
    resize(1050, 720);
    setMinimumSize(860, 600);

    buildInterface();
    buildMenus();
    applyTheme();
    applyLanguage();

    const QByteArray savedGeometry =
        settings.value(
            QStringLiteral(
                "windowGeometry"
            )
        ).toByteArray();

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(
            savedGeometry
        );
    }

    connect(
        m_backend,
        &SmartctlBackend::helperReady,
        this,
        [this]
        {
            refreshDevices();
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::helperFailed,
        this,
        [this](const QString &message)
        {
            QMessageBox::critical(
                this,
                QStringLiteral("LinDiskInfo"),
                tx(
                    "Authorization failed or the privileged helper could not be started.\n\n",
                    "Die Autorisierung ist fehlgeschlagen oder der privilegierte Helper konnte nicht gestartet werden.\n\n"
                ) + message
            );

            setStatus(
                tx(
                    "SMART access unavailable.",
                    "SMART-Zugriff nicht verfügbar."
                )
            );
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::scanFinished,
        this,
        [this](const QVector<DriveInfo> &drives)
        {
            QString previousDriveIdentity;
            QString previousDrivePath;

            if (m_selectedDrive >= 0 &&
                m_selectedDrive <
                    m_drives.size()) {

                const DriveInfo &selected =
                    m_drives.at(
                        m_selectedDrive
                    );

                previousDriveIdentity =
                    lindiskinfoDriveIdentity(
                        selected
                    );

                previousDrivePath =
                    selected.name;

            } else if (m_hasCurrentData) {

                previousDriveIdentity =
                    lindiskinfoDriveIdentity(
                        m_currentDrive
                    );

                previousDrivePath =
                    m_currentDrive.name;
            }

            if (previousDriveIdentity.isEmpty()) {
                previousDriveIdentity =
                    QSettings().value(
                        QStringLiteral(
                            "lastDriveIdentity"
                        )
                    ).toString();
            }

            // Legacy path-only preference compatibility.
            if (previousDrivePath.isEmpty()) {
                previousDrivePath =
                    QSettings().value(
                        QStringLiteral(
                            "lastDrive"
                        )
                    ).toString();
            }

            const QHash<QString, QJsonObject>
                previousData =
                    m_driveData;

            QVector<DriveInfo>
                filteredDrives;

            filteredDrives.reserve(
                drives.size()
            );

            for (const DriveInfo &candidate :
                 drives) {

                const QString transport =
                    candidate.transport
                        .trimmed()
                        .toLower();

                const QString type =
                    candidate.type
                        .trimmed()
                        .toLower();

                const bool external =
                    transport ==
                        QStringLiteral("usb") ||
                    transport ==
                        QStringLiteral("ieee1394") ||
                    transport ==
                        QStringLiteral("firewire") ||
                    transport ==
                        QStringLiteral("sbp");

                if (!m_externalStorageEnabled &&
                    external) {
                    continue;
                }

                if (!m_megaRaidEnabled &&
                    type.startsWith(
                        QStringLiteral(
                            "megaraid,"
                        )
                    )) {
                    continue;
                }

                filteredDrives.append(
                    candidate
                );
            }

            m_drives =
                filteredDrives;

            m_driveData.clear();

            for (const DriveInfo &drive :
                 m_drives) {

                if (previousData.contains(
                        lindiskinfoDriveIdentity(drive)
                    )) {

                    m_driveData.insert(
                        lindiskinfoDriveIdentity(drive),
                        previousData.value(
                            lindiskinfoDriveIdentity(drive)
                        )
                    );
                }
            }

            sortDrives();
            rebuildDriveButtons();
            applyDriveButtonLimit();
            rebuildDiskMenu();

            rebuildTrayMenu();
            updateTrayPresentation();

            if (m_drives.isEmpty()) {
                setStatus(
                    tx(
                        "No drives found.",
                        "Keine Laufwerke gefunden."
                    )
                );

                return;
            }

            int targetIndex = -1;
            int legacyPathIndex = -1;
            int legacyPathMatches = 0;

            for (int i = 0;
                 i < m_drives.size();
                 ++i) {

                const DriveInfo &candidate =
                    m_drives.at(i);

                if (!previousDriveIdentity.isEmpty() &&
                    lindiskinfoDriveIdentity(
                        candidate
                    ) ==
                        previousDriveIdentity) {

                    targetIndex = i;
                    break;
                }

                if (!previousDrivePath.isEmpty() &&
                    candidate.name ==
                        previousDrivePath) {

                    legacyPathIndex = i;
                    ++legacyPathMatches;
                }
            }

            // Old path-only setting is only safe when the
            // current path uniquely identifies one drive.
            if (targetIndex < 0 &&
                legacyPathMatches == 1) {

                targetIndex =
                    legacyPathIndex;
            }

            if (targetIndex < 0)
                targetIndex = 0;

            selectDrive(targetIndex);

            for (const DriveInfo &drive : m_drives)
                m_backend->requestDeviceData(drive);
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::scanFailed,
        this,
        [this](const QString &message)
        {
            QMessageBox::warning(
                this,
                QStringLiteral("LinDiskInfo"),
                message
            );
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::deviceDataReady,
        this,
        [this](
            const DriveInfo &drive,
            const QJsonObject &data
        )
        {
            m_driveData.insert(
                lindiskinfoDriveIdentity(drive),
                data
            );

            maybeAutoAdjustAta(
                drive,
                data
            );

            recordHistorySample(
                drive,
                data
            );

            updateTrayPresentation();
            rebuildTrayMenu();

            if (data.contains(
                    QStringLiteral(
                        "lindiskinfo_maintenance"
                    )
                )) {

                showMaintenanceResult(
                    drive,
                    data
                );
            }

            updateDriveButton(
                drive,
                data
            );


            rebuildDiskMenu();

            if (m_hideNoSmart ||
                m_driveSortMethod ==
                    QStringLiteral("health") ||
                m_driveSortMethod ==
                    QStringLiteral("temperature")) {

                refreshDrivePresentation();
            }

if (m_selectedDrive >= 0 &&
                m_selectedDrive < m_drives.size() &&
                lindiskinfoDriveIdentity(
                    m_drives.at(
                        m_selectedDrive
                    )
                ) ==
                lindiskinfoDriveIdentity(
                    drive
                )) {

                renderDevice(
                    drive,
                    data
                );
            }
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::deviceDataFailed,
        this,
        [this](
            const DriveInfo &drive,
            const QString &message
        )
        {
            if (m_selectedDrive >= 0 &&
                m_selectedDrive < m_drives.size() &&
                lindiskinfoDriveIdentity(
                    m_drives.at(
                        m_selectedDrive
                    )
                ) ==
                lindiskinfoDriveIdentity(
                    drive
                )) {

                setStatus(
                    tx(
                        "Unable to read SMART data.",
                        "SMART-Daten konnten nicht gelesen werden."
                    )
                );
            }

            QMessageBox::warning(
                this,
                QStringLiteral("LinDiskInfo"),
                driveDisplayIdentifier(drive) + QStringLiteral("\n\n") + message
            );
        }
    );

    setStatus(
        tx(
            "Waiting for authorization...",
            "Warte auf Autorisierung..."
        )
    );

    const bool autostartLaunch =
        QCoreApplication::arguments()
            .contains(
                QStringLiteral(
                    "--autostart"
                )
            );

    if (autostartLaunch &&
        m_startupDelaySeconds > 0) {

        setStatus(
            tx(
                "Startup delayed by %1 seconds...",
                "Systemstart um %1 Sekunden verzögert..."
            ).arg(
                m_startupDelaySeconds
            )
        );

        QTimer::singleShot(
            m_startupDelaySeconds * 1000,
            this,
            [this]
            {
                m_backend->start();
            }
        );
    } else {
        m_backend->start();
    }
}

QString MainWindow::tx(
    const char *english,
    const char *german
) const
{
    return m_language == Language::German
        ? QString::fromUtf8(german)
        : QString::fromUtf8(english);
}

MainWindow::HealthState
MainWindow::healthStateForData(
    const QJsonObject &data,
    int *percentage
) const
{
    if (percentage)
        *percentage = -1;

    const bool smartAvailable =
        data.value(
            QStringLiteral(
                "lindiskinfo_smart_available"
            )
        ).toBool(true);

    if (!smartAvailable)
        return HealthState::Unknown;

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {

        const QJsonObject nvme =
            data.value(
                QStringLiteral(
                    "nvme_smart_health_information_log"
                )
            ).toObject();

        const int remaining =
            std::max(
                0,
                100 -
                static_cast<int>(
                    jsonUnsigned(
                        nvme.value(
                            QStringLiteral(
                                "percentage_used"
                            )
                        )
                    )
                )
            );

        if (percentage)
            *percentage = remaining;

        const quint64 criticalWarning =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "critical_warning"
                    )
                )
            );

        const quint64 mediaErrors =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "media_errors"
                    )
                )
            );

        const quint64 spare =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "available_spare"
                    )
                )
            );

        const quint64 spareThreshold =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "available_spare_threshold"
                    )
                )
            );

        if (criticalWarning != 0 ||
            mediaErrors != 0 ||
            remaining <=
                m_nvmeBadRemaining) {

            return HealthState::Bad;
        }

        if (remaining <=
                m_nvmeCautionRemaining ||
            spare <= spareThreshold) {

            return HealthState::Caution;
        }

        return HealthState::Good;
    }

    if (data.contains(
            QStringLiteral(
                "ata_smart_attributes"
            )
        )) {

        const AtaLifeEstimate ataLife =
            ataLifeEstimate(data);

        if (
            percentage &&
            ataLife.valid
        ) {
            *percentage =
                ataLife.remainingPercent;
        }

        const bool passed =
            data.value(
                QStringLiteral(
                    "smart_status"
                )
            ).toObject()
            .value(
                QStringLiteral("passed")
            ).toBool(true);

        if (!passed)
            return HealthState::Bad;

        if (
            ataLife.valid &&
            ataLife.remainingPercent <=
                m_nvmeBadRemaining
        ) {
            return HealthState::Bad;
        }

        const QJsonArray attributes =
            data.value(
                QStringLiteral(
                    "ata_smart_attributes"
                )
            ).toObject()
            .value(
                QStringLiteral("table")
            ).toArray();

        bool caution =
            ataLife.valid &&
            ataLife.remainingPercent <=
                m_nvmeCautionRemaining;

        for (const QJsonValue &entry :
             attributes) {

            const QJsonObject attribute =
                entry.toObject();

            const QString name =
                attribute.value(
                    QStringLiteral("name")
                ).toString();

            const int current =
                attribute.value(
                    QStringLiteral("value")
                ).toInt();

            const int threshold =
                attribute.value(
                    QStringLiteral("thresh")
                ).toInt();

            const quint64 raw =
                jsonUnsigned(
                    attribute.value(
                        QStringLiteral("raw")
                    ).toObject()
                    .value(
                        QStringLiteral("value")
                    )
                );

            if (threshold > 0 &&
                current <= threshold) {

                return HealthState::Bad;
            }

            if ((name ==
                    QStringLiteral(
                        "Reallocated_Sector_Ct"
                    ) ||
                 name ==
                    QStringLiteral(
                        "Current_Pending_Sector"
                    ) ||
                 name ==
                    QStringLiteral(
                        "Offline_Uncorrectable"
                    )) &&
                raw >=
                    m_ataSectorCautionCount) {

                caution = true;
            }
        }

        return caution
            ? HealthState::Caution
            : HealthState::Good;
    }

    const QJsonObject status =
        data.value(
            QStringLiteral(
                "smart_status"
            )
        ).toObject();

    if (status.contains(
            QStringLiteral("passed")
        )) {

        return status.value(
            QStringLiteral("passed")
        ).toBool()
            ? HealthState::Good
            : HealthState::Bad;
    }

    return HealthState::Unknown;
}
