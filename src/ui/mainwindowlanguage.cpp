// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QToolButton>

void MainWindow::applyLanguage()
{
    auto action =
        [this](const char *name) -> QAction *
        {
            return findChild<QAction *>(
                QString::fromLatin1(name)
            );
        };

    auto menu =
        [this](const char *name) -> QMenu *
        {
            return findChild<QMenu *>(
                QString::fromLatin1(name)
            );
        };

    m_fileMenu->setTitle(
        tx("&File", "&Datei")
    );

    m_editMenu->setTitle(
        tx("&Edit", "&Bearbeiten")
    );

    m_settingsMenu->setTitle(
        tx("&Options", "&Optionen")
    );

    m_viewMenu->setTitle(
        tx("&View", "&Ansicht")
    );

    m_diskMenu->setTitle(
        tx("&Disk", "&Laufwerk")
    );

    m_helpMenu->setTitle(
        tx("&Help", "&Hilfe")
    );

    m_languageMenu->setTitle(
        tx("&Language", "&Sprache")
    );

    m_saveTextAction->setText(
        tx(
            "Save (text)",
            "Speichern (Text)"
        )
    );

    m_saveImageAction->setText(
        tx(
            "Save (image)",
            "Speichern (Bild)"
        )
    );

    m_quitAction->setText(
        tx("Exit", "Beenden")
    );

    m_copyAction->setText(
        tx("Copy", "Kopieren")
    );

    m_copyOptionsMenu->setTitle(
        tx(
            "Copy Options",
            "Kopieroptionen"
        )
    );

    m_copyIdentifyAction->setText(
        tx(
            "Device Identification",
            "Geräteidentifikation"
        )
    );

    m_copySmartDataAction->setText(
        tx(
            "SMART Data",
            "SMART-Daten"
        )
    );

    m_copyThresholdAction->setText(
        tx(
            "SMART Thresholds",
            "SMART-Grenzwerte"
        )
    );

    m_copyAsciiAction->setText(
        tx(
            "ASCII View",
            "ASCII-Ansicht"
        )
    );

    m_refreshAction->setText(
        tx(
            "Refresh",
            "Aktualisieren"
        )
    );

    m_autoRefreshMenu->setTitle(
        tx(
            "Auto Refresh",
            "Automatische Aktualisierung"
        )
    );

    for (QAction *entry :
         m_autoRefreshMenu->actions()) {

        if (entry->data().toInt() == 0)
            entry->setText(
                tx(
                    "Disabled",
                    "Deaktivieren"
                )
            );
    }

    m_autoRefreshTargetMenu->setTitle(
        tx(
            "Auto Refresh Target",
            "Aktualisierungsziel"
        )
    );

    if (QAction *entry =
            action("selectAllRefreshTargets")) {

        entry->setText(
            tx(
                "All drives",
                "Alle Laufwerke"
            )
        );
    }

    if (QAction *entry =
            action("deselectAllRefreshTargets")) {

        entry->setText(
            tx(
                "Selected drive only",
                "Nur ausgewähltes Laufwerk"
            )
        );
    }

    m_rereadAction->setText(
        tx(
            "Reread",
            "Neu einlesen"
        )
    );

    m_liveDetectionAction->setText(
        tx(
            "Live Device Detection",
            "Live-Geräteerkennung"
        )
    );

    m_diagramAction->setText(
        tx(
            "Graph / History",
            "Diagramm / Verlauf"
        )
    );

    m_hideSerialAction->setText(
        tx(
            "Hide Serial Number",
            "Seriennummer ausblenden"
        )
    );

    m_showTrayAction->setText(
        tx(
            "Show in System Tray",
            "Im Infobereich anzeigen"
        )
    );

    m_startWithSystemAction->setText(
        tx(
            "Start with System",
            "Mit System starten"
        )
    );

    m_advancedOptionsMenu->setTitle(
        tx(
            "Advanced Options",
            "Erweiterte Optionen"
        )
    );

    if (QAction *entry = action("aamApmManagementAction"))
        entry->setText(
            tx(
                "AAM/APM Management",
                "AAM/APM-Verwaltung"
            )
        );

    if (QAction *entry = action("aamApmAutoAction"))
        entry->setText(
            tx(
                "AAM/APM Auto Adjustment",
                "Automatische AAM/APM-Anpassung"
            )
        );

    if (QAction *entry = action("stateSettingsAction"))
        entry->setText(
            tx(
                "Health Status Settings",
                "Zustandseinstellungen"
            )
        );

    if (QAction *entry = action("temperatureWarningAction"))
        entry->setText(
            tx(
                "Warning - Temperature",
                "Temperaturwarnung"
            )
        );

    m_temperatureMenu->setTitle(
        tx(
            "Temperature Unit",
            "Temperatureinheit"
        )
    );

    m_autoDetectionMenu->setTitle(
        tx(
            "Auto Detection",
            "Automatische Erkennung"
        )
    );

    for (QAction *entry :
         m_autoDetectionMenu->actions()) {

        if (entry->data().toInt() == 0)
            entry->setText(
                tx(
                    "Disabled",
                    "Deaktivieren"
                )
            );
    }

    m_rawValuesMenu->setTitle(
        tx(
            "Raw Values",
            "Rohwerte"
        )
    );

    m_showRawAction->setText(
        tx(
            "Show Raw Values",
            "Rohwerte anzeigen"
        )
    );

    if (QMenu *entry = menu("startupDelayMenu"))
        entry->setTitle(
            tx(
                "Startup Delay",
                "Verzögerung beim Systemstart"
            )
        );

    if (QMenu *entry = menu("trayBehaviorMenu")) {
        entry->setTitle(
            tx(
                "System Tray Behavior",
                "Infobereich-Verhalten"
            )
        );

        const QList<QAction *> actions =
            entry->actions();

        if (actions.size() >= 2) {
            actions.at(0)->setText(
                tx(
                    "Hide main window",
                    "Hauptfenster ausblenden"
                )
            );

            actions.at(1)->setText(
                tx(
                    "Minimize main window",
                    "Hauptfenster minimieren"
                )
            );
        }
    }

    if (QMenu *entry = menu("driveSortMenu")) {
        entry->setTitle(
            tx(
                "Drive Sort Method",
                "Sortierung der Laufwerke"
            )
        );

        const QList<QAction *> actions =
            entry->actions();

        if (actions.size() >= 2) {
            actions.at(0)->setText(
                tx(
                    "Device path",
                    "Gerätepfad"
                )
            );

            actions.at(1)->setText(
                tx(
                    "Model name",
                    "Modellname"
                )
            );
        }
    }

    if (QMenu *entry = menu("displayDrivesMenu"))
        entry->setTitle(
            tx(
                "Display Number of Drives",
                "Anzahl angezeigter Laufwerke"
            )
        );

    if (QAction *entry = action("advancedDriveSearchAction"))
        entry->setText(
            tx(
                "Advanced Drive Search",
                "Erweiterte Laufwerkssuche"
            )
        );

    if (QAction *entry = action("ataPassThroughAction"))
        entry->setText(
            tx(
                "ATA Pass Through",
                "ATA-Passthrough"
            )
        );

    if (QAction *entry = action("usbIeeeAction"))
        entry->setText(QStringLiteral("USB/IEEE 1394"));

    if (QAction *entry = action("intelAmdRaidAction"))
        entry->setText(QStringLiteral("Intel/AMD RAID"));

    if (QAction *entry = action("amdRaidXpertAction"))
        entry->setText(QStringLiteral("AMD RAIDXpert2"));

    if (QAction *entry = action("megaRaidAction"))
        entry->setText(QStringLiteral("MegaRAID"));

    if (QAction *entry = action("intelVrocAction"))
        entry->setText(QStringLiteral("Intel VROC"));

    m_hideSmartInfoAction->setText(
        tx(
            "Hide S.M.A.R.T. Information",
            "S.M.A.R.T.-Infos ausblenden"
        )
    );

    if (QAction *entry = action("hideNoSmartAction"))
        entry->setText(
            tx(
                "Hide drives without S.M.A.R.T.",
                "Laufwerke ohne S.M.A.R.T. ausblenden"
            )
        );

    m_fontAction->setText(
        tx(
            "Font Settings",
            "Schrifteinstellung"
        )
    );

    m_themeMenu->setTitle(
        tx(
            "Theme",
            "Darstellung"
        )
    );

    m_systemThemeAction->setText(
        tx(
            "System",
            "System"
        )
    );

    m_darkThemeAction->setText(
        tx(
            "Dark",
            "Dunkel"
        )
    );

    m_aboutAction->setText(
        tx(
            "About LinDiskInfo",
            "Über LinDiskInfo"
        )
    );

    m_healthCaption->setText(
        tx(
            "Health Status",
            "Gesundheitsstatus"
        )
    );

    m_temperatureCaption->setText(
        tx(
            "Temperature",
            "Temperatur"
        )
    );

    m_firmwareCaption->setText(
        QStringLiteral("Firmware")
    );

    m_serialCaption->setText(
        tx(
            "Serial Number",
            "Seriennummer"
        )
    );

    m_interfaceCaption->setText(
        tx(
            "Interface",
            "Schnittstelle"
        )
    );

    m_transferCaption->setText(
        tx(
            "Transfer Mode",
            "Übertragungsmodus"
        )
    );

    m_standardCaption->setText(
        QStringLiteral("Standard")
    );

    m_featuresCaption->setText(
        tx(
            "Features",
            "Eigenschaften"
        )
    );

    m_readsCaption->setText(
        tx(
            "Total Host Reads",
            "Gesamt gelesen"
        )
    );

    m_writesCaption->setText(
        tx(
            "Total Host Writes",
            "Gesamt geschrieben"
        )
    );

    m_rotationCaption->setText(
        tx(
            "Rotation Rate",
            "Drehzahl"
        )
    );

    m_powerCyclesCaption->setText(
        tx(
            "Power On Count",
            "Einschaltvorgänge"
        )
    );

    m_powerHoursCaption->setText(
        tx(
            "Power On Hours",
            "Betriebsstunden"
        )
    );

    updateSerialButton();
    rebuildDiskMenu();

    if (m_hasCurrentData)
        renderDevice(
            m_currentDrive,
            m_currentData
        );
    else
        updateRawColumn();

    for (const DriveInfo &drive : m_drives) {
        if (m_driveData.contains(lindiskinfoDriveIdentity(drive)))
            updateDriveButton(
                drive,
                m_driveData.value(lindiskinfoDriveIdentity(drive))
            );
    }


    if (m_storageUnitMenu) {
        m_storageUnitMenu->setTitle(
            tx(
                "Storage Unit",
                "Speichereinheit"
            )
        );

        for (QAction *entry :
             m_storageUnitMenu->actions()) {

            const QString unit =
                entry->data().toString();

            if (unit == QStringLiteral("GB")) {
                entry->setText(
                    tx(
                        "GB (decimal)",
                        "GB (dezimal)"
                    )
                );

            } else if (
                unit == QStringLiteral("GiB")
            ) {

                entry->setText(
                    tx(
                        "GiB (binary)",
                        "GiB (binär)"
                    )
                );

            } else if (
                unit == QStringLiteral("TB")
            ) {

                entry->setText(
                    tx(
                        "TB (decimal)",
                        "TB (dezimal)"
                    )
                );

            } else if (
                unit == QStringLiteral("TiB")
            ) {

                entry->setText(
                    tx(
                        "TiB (binary)",
                        "TiB (binär)"
                    )
                );
            }
        }
    }



    if (QAction *entry =
            action("driveSortDefault")) {

        entry->setText(
            tx(
                "Default (SATA / NVMe / USB)",
                "Standard (SATA / NVMe / USB)"
            )
        );
    }

    if (QAction *entry =
            action("driveSortPath")) {

        entry->setText(
            tx(
                "Device path",
                "Gerätepfad"
            )
        );
    }

    if (QAction *entry =
            action("driveSortModel")) {

        entry->setText(
            tx(
                "Model name",
                "Modellname"
            )
        );
    }

    if (QAction *entry =
            action("driveSortHealth")) {

        entry->setText(
            tx(
                "Health status",
                "Gesundheitszustand"
            )
        );
    }

    if (QAction *entry =
            action("driveSortTemperature")) {

        entry->setText(
            tx(
                "Temperature",
                "Temperatur"
            )
        );
    }

    if (QAction *entry =
            action("displayDrivesAll")) {

        entry->setText(
            tx(
                "All",
                "Alle"
            )
        );
    }



    if (QMenu *entry =
            menu("selfTestMenu")) {

        entry->setTitle(
            tx(
                "Self Tests",
                "Selbsttests"
            )
        );
    }

    if (QAction *entry =
            action(
                "shortSelfTestAction"
            )) {

        entry->setText(
            tx(
                "Short Self-Test",
                "Kurzer Selbsttest"
            )
        );
    }

    if (QAction *entry =
            action(
                "longSelfTestAction"
            )) {

        entry->setText(
            tx(
                "Extended Self-Test",
                "Erweiterter Selbsttest"
            )
        );
    }

    if (QAction *entry =
            action(
                "abortSelfTestAction"
            )) {

        entry->setText(
            tx(
                "Abort Self-Test",
                "Selbsttest abbrechen"
            )
        );
    }

    if (QMenu *entry =
            menu("smartLogsMenu")) {

        entry->setTitle(
            tx(
                "SMART Logs",
                "SMART-Protokolle"
            )
        );
    }

    if (QAction *entry =
            action(
                "selfTestLogAction"
            )) {

        entry->setText(
            tx(
                "Self-Test Status / Log",
                "Selbsttest-Status / Protokoll"
            )
        );
    }

    if (QAction *entry =
            action(
                "errorLogAction"
            )) {

        entry->setText(
            tx(
                "Error Log",
                "Fehlerprotokoll"
            )
        );
    }

    if (QAction *entry =
            action(
                "deviceStatisticsAction"
            )) {

        entry->setText(
            tx(
                "ATA Device Statistics",
                "ATA-Gerätestatistiken"
            )
        );
    }

    if (QAction *entry =
            action(
                "sataPhyAction"
            )) {

        entry->setText(
            tx(
                "SATA PHY Event Counters",
                "SATA-PHY-Ereigniszähler"
            )
        );
    }



    rebuildTrayMenu();
    updateTrayPresentation();

}
