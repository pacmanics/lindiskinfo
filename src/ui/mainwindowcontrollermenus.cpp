// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../theme/waifuthemes.h"
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantList>
#include <QStringList>

void MainWindow::setupControllerMenus()
{
auto actionByName062 =
            [this](const char *name)
                -> QAction *
            {
                return findChild<QAction *>(
                    QString::fromLatin1(
                        name
                    )
                );
            };


        // ====================================================
        // ATA PASS THROUGH / SAT fallback
        // ====================================================

        if (QAction *entry =
                actionByName062(
                    "ataPassThroughAction"
                )) {

            entry->setVisible(true);
            entry->setCheckable(true);

            entry->setChecked(
                m_ataPassThroughEnabled
            );

            entry->setStatusTip(
                tx(
                    "Use SAT/ATA pass-through as a fallback for external SCSI-style storage devices.",
                    "SAT/ATA-Passthrough als Fallback für externe SCSI-artige Datenträger verwenden."
                )
            );

            connect(
                entry,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_ataPassThroughEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "ataPassThrough"
                        ),
                        enabled
                    );

                    m_autoAdjustedAtaDrives
                        .clear();

                    refreshAllData();
                }
            );
        }


        // ====================================================
        // USB / IEEE 1394 devices
        // ====================================================

        if (QAction *entry =
                actionByName062(
                    "usbIeeeAction"
                )) {

            entry->setVisible(true);
            entry->setCheckable(true);

            entry->setChecked(
                m_externalStorageEnabled
            );

            entry->setStatusTip(
                tx(
                    "Include external USB and IEEE 1394 / FireWire storage devices.",
                    "Externe USB- und IEEE-1394-/FireWire-Datenträger einbeziehen."
                )
            );

            connect(
                entry,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_externalStorageEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "externalStorage"
                        ),
                        enabled
                    );

                    refreshDevices();
                }
            );
        }


        // ====================================================
        // MegaRAID physical members
        //
        // smartctl --scan-open already reports these as
        // /dev/bus/N -d megaraid,M when supported.
        // ====================================================

        if (QAction *entry =
                actionByName062(
                    "megaRaidAction"
                )) {

            entry->setVisible(true);
            entry->setCheckable(true);

            entry->setChecked(
                m_megaRaidEnabled
            );

            entry->setStatusTip(
                tx(
                    "Include physical drives discovered behind LSI/Broadcom MegaRAID and Dell PERC controllers.",
                    "Physische Laufwerke hinter LSI/Broadcom-MegaRAID- und Dell-PERC-Controllern einbeziehen."
                )
            );

            connect(
                entry,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_megaRaidEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "megaRaidPhysicalDrives"
                        ),
                        enabled
                    );

                    refreshDevices();
                }
            );
        }


        // ====================================================
        // Controller compatibility actions
        //
        // There is intentionally no invented smartctl -d type
        // for RAIDXpert2 or VROC. Linux-exposed members are
        // scanned through their real block/NVMe nodes.
        // ====================================================

        const auto addCompatibilityRescan =
            [this, actionByName062](
                const char *name,
                const QString &english,
                const QString &german
            )
            {
                QAction *entry =
                    actionByName062(name);

                if (!entry)
                    return;

                entry->setVisible(true);

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this,
                     english,
                     german]
                    {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                english
                                    .toUtf8()
                                    .constData(),
                                german
                                    .toUtf8()
                                    .constData()
                            )
                        );

                        refreshDevices();
                    }
                );
            };

        addCompatibilityRescan(
            "intelAmdRaidAction",
            QStringLiteral(
                "Linux does not provide one universal smartctl pass-through type for every Intel or AMD RAID controller. LinDiskInfo will rescan all block devices and controller members exposed by the kernel and smartctl."
            ),
            QStringLiteral(
                "Linux bietet keinen universellen smartctl-Passthrough-Typ für jeden Intel- oder AMD-RAID-Controller. LinDiskInfo durchsucht alle vom Kernel und von smartctl bereitgestellten Laufwerke und Controller-Mitglieder erneut."
            )
        );

        addCompatibilityRescan(
            "amdRaidXpertAction",
            QStringLiteral(
                "AMD RAIDXpert2 has no generic smartctl physical-drive pass-through mode on Linux. Drives exposed individually by the kernel are handled normally. LinDiskInfo will perform a fresh controller scan."
            ),
            QStringLiteral(
                "AMD RAIDXpert2 besitzt unter Linux keinen generischen smartctl-Passthrough-Modus für physische Laufwerke. Vom Kernel einzeln bereitgestellte Laufwerke werden normal verarbeitet. LinDiskInfo führt jetzt einen neuen Controller-Scan aus."
            )
        );

        addCompatibilityRescan(
            "intelVrocAction",
            QStringLiteral(
                "Intel VROC NVMe members exposed as normal /dev/nvme devices are handled automatically. Firmware-hidden members cannot be invented by LinDiskInfo. A fresh controller scan will now be performed."
            ),
            QStringLiteral(
                "Intel-VROC-NVMe-Mitglieder, die als normale /dev/nvme-Geräte bereitgestellt werden, verarbeitet LinDiskInfo automatisch. Von der Firmware verborgene Mitglieder kann LinDiskInfo nicht künstlich sichtbar machen. Jetzt wird ein neuer Controller-Scan durchgeführt."
            )
        );
}
