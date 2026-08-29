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

namespace
{

QStringList blockDeviceSnapshot()
{
    QDir directory(
        QStringLiteral(
            "/sys/class/block"
        )
    );

    return directory.entryList(
        QDir::AllEntries |
        QDir::System |
        QDir::NoDotAndDotDot,
        QDir::Name
    );
}

}

void MainWindow::buildMenus()
{
    QSettings settings;

    auto disabledAction =
        [this](QMenu *menu, const QString &name)
        {
            QAction *action =
                menu->addAction(QString());

            action->setObjectName(name);
            action->setVisible(false);

            return action;
        };

    auto disabledMenu =
        [this](QMenu *parent, const QString &name)
        {
            QMenu *menu =
                parent->addMenu(QString());

            menu->setObjectName(name);
            menu->menuAction()->setVisible(false);

            return menu;
        };

    m_fileMenu =
        menuBar()->addMenu(QString());

    m_saveTextAction =
        m_fileMenu->addAction(QString());

    m_saveTextAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+T"))
    );

    connect(
        m_saveTextAction,
        &QAction::triggered,
        this,
        &MainWindow::saveTextReport
    );

    m_saveImageAction =
        m_fileMenu->addAction(QString());

    m_saveImageAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+S"))
    );

    connect(
        m_saveImageAction,
        &QAction::triggered,
        this,
        &MainWindow::saveImage
    );

    m_fileMenu->addSeparator();

    m_quitAction =
        m_fileMenu->addAction(QString());

    m_quitAction->setShortcut(
        QKeySequence(QStringLiteral("Alt+F4"))
    );

    connect(
        m_quitAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_forceQuit = true;
            close();
        }
    );

    m_editMenu =
        menuBar()->addMenu(QString());

    m_copyAction =
        m_editMenu->addAction(QString());

    m_copyAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+C"))
    );

    connect(
        m_copyAction,
        &QAction::triggered,
        this,
        &MainWindow::copyReport
    );

    m_copyOptionsMenu =
        m_editMenu->addMenu(QString());

    m_copyIdentifyAction =
        m_copyOptionsMenu->addAction(
            QStringLiteral("IDENTIFY_DEVICE")
        );

    m_copySmartDataAction =
        m_copyOptionsMenu->addAction(
            QStringLiteral("SMART_READ_DATA")
        );

    m_copyThresholdAction =
        m_copyOptionsMenu->addAction(
            QStringLiteral("SMART_READ_THRESHOLD")
        );

    m_copyOptionsMenu->addSeparator();

    m_copyAsciiAction =
        m_copyOptionsMenu->addAction(QString());

    for (QAction *action :
         {
             m_copyIdentifyAction,
             m_copySmartDataAction,
             m_copyThresholdAction,
             m_copyAsciiAction
         }) {

        action->setCheckable(true);
    }

    m_copyIdentifyAction->setChecked(true);
    m_copySmartDataAction->setChecked(true);
    m_copyThresholdAction->setChecked(true);

    m_settingsMenu =
        menuBar()->addMenu(QString());

    m_refreshAction =
        m_settingsMenu->addAction(QString());

    m_refreshAction->setShortcut(
        QKeySequence(QStringLiteral("F5"))
    );

    connect(
        m_refreshAction,
        &QAction::triggered,
        this,
        &MainWindow::refreshAllData
    );

    m_autoRefreshMenu =
        m_settingsMenu->addMenu(QString());

    auto *autoRefreshGroup =
        new QActionGroup(this);

    autoRefreshGroup->setExclusive(true);

    m_autoRefreshMinutes =
        settings.value(
            QStringLiteral("autoRefreshMinutes"),
            10
        ).toInt();

    for (int minutes :
         {
             1,
             3,
             5,
             10,
             30,
             60,
             120,
             180,
             360,
             720,
             1440,
             0
         }) {

        QAction *action =
            m_autoRefreshMenu->addAction(
                minutes == 0
                    ? QString()
                    : QStringLiteral("%1 min").arg(minutes)
            );

        action->setCheckable(true);
        action->setData(minutes);

        if (minutes == m_autoRefreshMinutes)
            action->setChecked(true);

        autoRefreshGroup->addAction(action);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, minutes]
            {
                setAutoRefreshInterval(minutes);
                applyLanguage();
            }
        );
    }

    m_autoRefreshTargetMenu =
        m_settingsMenu->addMenu(QString());

    QAction *selectAll =
        m_autoRefreshTargetMenu->addAction(QString());

    selectAll->setObjectName(
        QStringLiteral("selectAllRefreshTargets")
    );

    QAction *deselectAll =
        m_autoRefreshTargetMenu->addAction(QString());

    deselectAll->setObjectName(
        QStringLiteral("deselectAllRefreshTargets")
    );

    connect(
        selectAll,
        &QAction::triggered,
        this,
        [this]
        {
            m_autoRefreshAllDrives = true;

            QSettings().setValue(
                QStringLiteral("autoRefreshAllDrives"),
                true
            );
        }
    );

    connect(
        deselectAll,
        &QAction::triggered,
        this,
        [this]
        {
            m_autoRefreshAllDrives = false;

            QSettings().setValue(
                QStringLiteral("autoRefreshAllDrives"),
                false
            );
        }
    );

    m_autoRefreshAllDrives =
        settings.value(
            QStringLiteral("autoRefreshAllDrives"),
            true
        ).toBool();

    m_rereadAction =
        m_settingsMenu->addAction(QString());

    m_rereadAction->setShortcut(
        QKeySequence(QStringLiteral("F6"))
    );

    connect(
        m_rereadAction,
        &QAction::triggered,
        this,
        &MainWindow::refreshDevices
    );

    m_liveDetectionAction =
        m_settingsMenu->addAction(
            QString()
        );

    m_liveDetectionAction->setCheckable(
        true
    );

    m_liveDetectionAction->setChecked(
        settings.value(
            QStringLiteral(
                "liveDeviceDetection"
            ),
            true
        ).toBool()
    );

    connect(
        m_liveDetectionAction,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            QSettings().setValue(
                QStringLiteral(
                    "liveDeviceDetection"
                ),
                enabled
            );
        }
    );

    m_diagramAction =
        disabledAction(
            m_settingsMenu,
            QStringLiteral("diagramAction")
        );

    m_settingsMenu->addSeparator();

    m_hideSerialAction =
        m_settingsMenu->addAction(QString());

    m_hideSerialAction->setCheckable(true);
    m_hideSerialAction->setChecked(true);

    connect(
        m_hideSerialAction,
        &QAction::toggled,
        this,
        [this](bool hidden)
        {
            m_serialVisible = !hidden;

            m_serialEdit->setEchoMode(
                m_serialVisible
                    ? QLineEdit::Normal
                    : QLineEdit::Password
            );

            updateSerialButton();
        }
    );

    m_showTrayAction =
        m_settingsMenu->addAction(QString());

    m_showTrayAction->setCheckable(true);

    m_showTrayAction->setChecked(
        settings.value(
            QStringLiteral("showInTray"),
            false
        ).toBool()
    );

    m_startWithSystemAction =
        m_settingsMenu->addAction(QString());

    m_startWithSystemAction->setCheckable(true);

    m_startWithSystemAction->setChecked(
        QFile::exists(autostartPath())
    );

    m_settingsMenu->addSeparator();

    m_advancedOptionsMenu =
        m_settingsMenu->addMenu(QString());

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("aamApmManagementAction")
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("aamApmAutoAction")
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("stateSettingsAction")
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("temperatureWarningAction")
    );

    m_advancedOptionsMenu->addSeparator();

    m_temperatureMenu =
        m_advancedOptionsMenu->addMenu(QString());

    m_temperatureGroup =
        new QActionGroup(this);

    m_temperatureGroup->setExclusive(true);

    m_celsiusAction =
        m_temperatureMenu->addAction(
            QStringLiteral("Celsius (°C)")
        );

    m_fahrenheitAction =
        m_temperatureMenu->addAction(
            QStringLiteral("Fahrenheit (°F)")
        );

    m_celsiusAction->setCheckable(true);
    m_fahrenheitAction->setCheckable(true);

    m_temperatureGroup->addAction(
        m_celsiusAction
    );

    m_temperatureGroup->addAction(
        m_fahrenheitAction
    );

    m_celsiusAction->setChecked(
        m_temperatureUnit ==
        TemperatureUnit::Celsius
    );

    m_fahrenheitAction->setChecked(
        m_temperatureUnit ==
        TemperatureUnit::Fahrenheit
    );

    connect(
        m_celsiusAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_temperatureUnit =
                TemperatureUnit::Celsius;

            QSettings().setValue(
                QStringLiteral("temperatureUnit"),
                QStringLiteral("celsius")
            );

            if (m_hasCurrentData)
                renderDevice(
                    m_currentDrive,
                    m_currentData
                );

            for (const DriveInfo &drive : m_drives) {
                if (m_driveData.contains(lindiskinfoDriveIdentity(drive)))
                    updateDriveButton(
                        drive,
                        m_driveData.value(lindiskinfoDriveIdentity(drive))
                    );
            }
        }
    );

    connect(
        m_fahrenheitAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_temperatureUnit =
                TemperatureUnit::Fahrenheit;

            QSettings().setValue(
                QStringLiteral("temperatureUnit"),
                QStringLiteral("fahrenheit")
            );

            if (m_hasCurrentData)
                renderDevice(
                    m_currentDrive,
                    m_currentData
                );

            for (const DriveInfo &drive : m_drives) {
                if (m_driveData.contains(lindiskinfoDriveIdentity(drive)))
                    updateDriveButton(
                        drive,
                        m_driveData.value(lindiskinfoDriveIdentity(drive))
                    );
            }
        }
    );

    m_autoDetectionMenu =
        m_advancedOptionsMenu->addMenu(QString());

    auto *autoDetectionGroup =
        new QActionGroup(this);

    autoDetectionGroup->setExclusive(true);

    m_autoDetectionSeconds =
        settings.value(
            QStringLiteral("autoDetectionSeconds"),
            0
        ).toInt();

    for (int seconds : {5, 10, 20, 30, 0}) {
        QAction *action =
            m_autoDetectionMenu->addAction(
                seconds == 0
                    ? QString()
                    : QStringLiteral("%1 s").arg(seconds)
            );

        action->setCheckable(true);
        action->setData(seconds);

        if (seconds == m_autoDetectionSeconds)
            action->setChecked(true);

        autoDetectionGroup->addAction(action);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, seconds]
            {
                setAutoDetectionInterval(seconds);
                applyLanguage();
            }
        );
    }

    m_rawValuesMenu =
        m_advancedOptionsMenu->addMenu(QString());

    m_showRawAction =
        m_rawValuesMenu->addAction(QString());

    m_showRawAction->setCheckable(true);

    m_showRawAction->setChecked(
        settings.value(
            QStringLiteral("showRawValues"),
            false
        ).toBool()
    );

    connect(
        m_showRawAction,
        &QAction::toggled,
        this,
        [this](bool checked)
        {
            QSettings().setValue(
                QStringLiteral("showRawValues"),
                checked
            );

            updateRawColumn();
        }
    );

    m_rawValuesMenu->addSeparator();

    QAction *hexAction =
        m_rawValuesMenu->addAction(
            QStringLiteral("16 [HEX]")
        );

    hexAction->setCheckable(true);
    hexAction->setChecked(true);
    hexAction->setVisible(false);

    for (const QString &label :
         {
             QStringLiteral("10 [DEC]"),
             QStringLiteral("10 [DEC] - 2byte"),
             QStringLiteral("10 [DEC] - 1byte")
         }) {

        QAction *action =
            m_rawValuesMenu->addAction(label);

        action->setVisible(false);
    }

    QMenu *startupDelayMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("startupDelayMenu")
        );

    for (int seconds :
         {
             0, 5, 10, 15, 20, 30, 40,
             50, 60, 90, 120, 150,
             180, 210, 240
         }) {

        startupDelayMenu->addAction(
            QStringLiteral("%1 s").arg(seconds)
        );
    }

    QMenu *trayBehaviorMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("trayBehaviorMenu")
        );

    trayBehaviorMenu->addAction(QString());
    trayBehaviorMenu->addAction(QString());

    QMenu *driveSortMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("driveSortMenu")
        );

    driveSortMenu->addAction(QString());
    driveSortMenu->addAction(QString());

    QMenu *displayDrivesMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("displayDrivesMenu")
        );

    displayDrivesMenu->addAction(
        QStringLiteral("8")
    );

    displayDrivesMenu->addAction(
        QStringLiteral("16")
    );

    m_advancedOptionsMenu->addSeparator();

    for (const QString &name :
         {
             QStringLiteral("advancedDriveSearchAction"),
             QStringLiteral("ataPassThroughAction"),
             QStringLiteral("usbIeeeAction"),
             QStringLiteral("intelAmdRaidAction"),
             QStringLiteral("amdRaidXpertAction"),
             QStringLiteral("megaRaidAction"),
             QStringLiteral("intelVrocAction")
         }) {

        disabledAction(
            m_advancedOptionsMenu,
            name
        );
    }

    m_advancedOptionsMenu->addSeparator();

    m_hideSmartInfoAction =
        m_advancedOptionsMenu->addAction(QString());

    m_hideSmartInfoAction->setCheckable(true);

    m_hideSmartInfoAction->setChecked(
        settings.value(
            QStringLiteral("hideSmartInfo"),
            false
        ).toBool()
    );

    connect(
        m_hideSmartInfoAction,
        &QAction::toggled,
        this,
        [this](bool hidden)
        {
            QSettings().setValue(
                QStringLiteral("hideSmartInfo"),
                hidden
            );

            m_table->setVisible(!hidden);
        }
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("hideNoSmartAction")
    );

    m_viewMenu =
        menuBar()->addMenu(QString());

    m_zoomMenu =
        m_viewMenu->addMenu(QStringLiteral("Zoom"));

    auto *zoomGroup =
        new QActionGroup(this);

    zoomGroup->setExclusive(true);

    m_zoomPercent =
        settings.value(
            QStringLiteral("zoomPercent"),
            100
        ).toInt();

    for (int percent :
         {
             100,
             125,
             150,
             200,
             250,
             300
         }) {

        QAction *action =
            m_zoomMenu->addAction(
                QStringLiteral("%1%").arg(percent)
            );

        action->setCheckable(true);
        action->setData(percent);

        if (percent == m_zoomPercent)
            action->setChecked(true);

        zoomGroup->addAction(action);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, percent]
            {
                setZoomPercent(percent);
            }
        );
    }

    m_fontAction =
        m_viewMenu->addAction(QString());

    connect(
        m_fontAction,
        &QAction::triggered,
        this,
        [this]
        {
            bool ok = false;

            const QFont font =
                QFontDialog::getFont(
                    &ok,
                    m_baseFont,
                    this
                );

            if (!ok)
                return;

            m_baseFont = font;

            QSettings().setValue(
                QStringLiteral("font"),
                font.toString()
            );

            setZoomPercent(m_zoomPercent);
        }
    );

    m_viewMenu->addSeparator();

    m_themeMenu =
        m_viewMenu->addMenu(QString());

    m_themeGroup =
        new QActionGroup(this);

    m_themeGroup->setExclusive(true);

    const auto addThemeAction =
        [this](
            const QString &id,
            const QString &label
        )
        {
            QAction *action =
                m_themeMenu->addAction(
                    label
                );

            action->setCheckable(true);
            action->setData(id);

            m_themeGroup->addAction(
                action
            );

            return action;
        };

    m_systemThemeAction =
        addThemeAction(
            QStringLiteral("system"),
            QString()
        );

    m_darkThemeAction =
        addThemeAction(
            QStringLiteral("dark"),
            QString()
        );

    m_themeMenu->addSeparator();

    for (const WaifuTheme &theme :
         WaifuThemes::all()) {

        addThemeAction(
            theme.id,
            theme.displayName
        );
    }

    QString selectedTheme =
        settings.value(
            QStringLiteral("theme"),
            QStringLiteral("system")
        ).toString();

    if (
        selectedTheme ==
            QStringLiteral("system") &&
        settings.value(
            QStringLiteral("darkMode"),
            false
        ).toBool()
    ) {
        selectedTheme =
            QStringLiteral("dark");
    }

    if (
        selectedTheme !=
            QStringLiteral("system") &&
        selectedTheme !=
            QStringLiteral("dark") &&
        !WaifuThemes::find(
            selectedTheme
        )
    ) {
        selectedTheme =
            QStringLiteral("system");
    }

    for (QAction *action :
         m_themeGroup->actions()) {

        action->setChecked(
            action->data().toString() ==
            selectedTheme
        );
    }

    connect(
        m_themeGroup,
        &QActionGroup::triggered,
        this,
        [this](QAction *action)
        {
            if (!action)
                return;

            const QString themeId =
                action->data().toString();

            if (
                themeId !=
                    QStringLiteral("system") &&
                themeId !=
                    QStringLiteral("dark") &&
                !WaifuThemes::find(
                    themeId
                )
            ) {
                return;
            }

            QSettings settings;

            settings.setValue(
                QStringLiteral("theme"),
                themeId
            );

            settings.remove(
                QStringLiteral("darkMode")
            );

            applyTheme();
        }
    );

    m_diskMenu =
        menuBar()->addMenu(QString());

    m_helpMenu =
        menuBar()->addMenu(QString());

    m_aboutAction =
        m_helpMenu->addAction(QString());

    connect(
        m_aboutAction,
        &QAction::triggered,
        this,
        [this]
        {
            const QString description =
                tx(
                    "A Qt-based S.M.A.R.T. and NVMe health monitor for Linux, featuring a clean interface inspired by CrystalDiskInfo.",
                    "Ein Qt-basierter S.M.A.R.T.- und NVMe-Zustandsmonitor für Linux mit einer übersichtlichen, von CrystalDiskInfo inspirierten Benutzeroberfläche."
                );

            const QString thirdPartyTitle =
                tx(
                    "Third-party software",
                    "Drittanbieter-Software"
                );

            const QString qtLicense =
                tx(
                    "Qt 6 — The Qt Company, under the applicable Qt license",
                    "Qt 6 — The Qt Company, gemäß der jeweils anwendbaren Qt-Lizenz"
                );

            QMessageBox::about(
                this,
                tx(
                    "About LinDiskInfo",
                    "Über LinDiskInfo"
                ),
                QStringLiteral(
                    "<h2>LinDiskInfo %1</h2>"
                    "<p>%2</p>"
                    "<p>"
                    "<b>Copyright © 2026 PacmanicS</b><br>"
                    "GPL-3.0-or-later"
                    "</p>"
                    "<hr>"
                    "<p><b>%3</b></p>"
                    "<p>"
                    "smartmontools / smartctl — GPL-2.0-or-later<br>"
                    "%4"
                    "</p>"
                ).arg(
                    QCoreApplication::
                        applicationVersion(),
                    description,
                    thirdPartyTitle,
                    qtLicense
                )
            );
        }
    );


    m_languageMenu =
        menuBar()->addMenu(QString());

    m_languageGroup =
        new QActionGroup(this);

    m_languageGroup->setExclusive(true);

    m_englishAction =
        m_languageMenu->addAction(
            QStringLiteral("English [English]")
        );

    m_germanAction =
        m_languageMenu->addAction(
            QStringLiteral("Deutsch [German]")
        );

    m_englishAction->setCheckable(true);
    m_germanAction->setCheckable(true);

    m_languageGroup->addAction(
        m_englishAction
    );

    m_languageGroup->addAction(
        m_germanAction
    );

    m_englishAction->setChecked(
        m_language ==
        Language::English
    );

    m_germanAction->setChecked(
        m_language ==
        Language::German
    );

    connect(
        m_englishAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_language =
                Language::English;

            QSettings().setValue(
                QStringLiteral("language"),
                QStringLiteral("en")
            );

            applyLanguage();
        }
    );

    connect(
        m_germanAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_language =
                Language::German;

            QSettings().setValue(
                QStringLiteral("language"),
                QStringLiteral("de")
            );

            applyLanguage();
        }
    );

    m_autoRefreshTimer =
        new QTimer(this);

    connect(
        m_autoRefreshTimer,
        &QTimer::timeout,
        this,
        [this]
        {
            if (m_autoRefreshAllDrives) {
                refreshAllData();
                return;
            }

            if (m_selectedDrive >= 0 &&
                m_selectedDrive < m_drives.size()) {

                m_backend->requestDeviceData(
                    m_drives.at(m_selectedDrive)
                );
            }
        }
    );

    m_autoDetectionTimer =
        new QTimer(this);

    connect(
        m_autoDetectionTimer,
        &QTimer::timeout,
        this,
        [this]
        {
            m_backend->scanDevices();
        }
    );

    setAutoRefreshInterval(
        m_autoRefreshMinutes
    );

    setAutoDetectionInterval(
        m_autoDetectionSeconds
    );

    m_table->setVisible(
        !m_hideSmartInfoAction->isChecked()
    );

    m_deviceWatcher =
        new QFileSystemWatcher(this);

    m_liveDetectionDebounce =
        new QTimer(this);

    m_liveDetectionDebounce->setSingleShot(
        true
    );

    m_liveDetectionDebounce->setInterval(
        500
    );

    const QString blockDevicePath =
        QStringLiteral(
            "/sys/class/block"
        );

    if (QFileInfo::exists(
            blockDevicePath
        )) {

        m_deviceWatcher->addPath(
            blockDevicePath
        );
    }

    connect(
        m_deviceWatcher,
        &QFileSystemWatcher::directoryChanged,
        this,
        [this](const QString &)
        {
            if (!m_liveDetectionAction ||
                !m_liveDetectionAction
                    ->isChecked()) {
                return;
            }

            m_liveDetectionDebounce->start();
        }
    );


    // ========================================================
    // Reliable live block-device detection fallback
    //
    // QFileSystemWatcher remains the fast event-driven path.
    // This timer compares only the names in /sys/class/block,
    // so it is cheap and independent of desktop automounting.
    // ========================================================

    QTimer *liveDevicePollTimer =
        new QTimer(this);

    liveDevicePollTimer->setInterval(
        750
    );

    liveDevicePollTimer->setProperty(
        "blockDeviceSnapshot",
        blockDeviceSnapshot()
    );

    connect(
        liveDevicePollTimer,
        &QTimer::timeout,
        this,
        [this, liveDevicePollTimer]
        {
            if (!m_liveDetectionAction ||
                !m_liveDetectionAction
                    ->isChecked()) {
                return;
            }

            const QStringList currentSnapshot =
                blockDeviceSnapshot();

            const QStringList previousSnapshot =
                liveDevicePollTimer
                    ->property(
                        "blockDeviceSnapshot"
                    )
                    .toStringList();

            if (currentSnapshot ==
                previousSnapshot) {
                return;
            }

            liveDevicePollTimer->setProperty(
                "blockDeviceSnapshot",
                currentSnapshot
            );

            // Existing debounce handles device settling and
            // coalesces disk/partition changes into one rescan.
            m_liveDetectionDebounce->start();
        }
    );

    connect(
        m_liveDetectionAction,
        &QAction::toggled,
        this,
        [this, liveDevicePollTimer](
            bool enabled
        )
        {
            liveDevicePollTimer->setProperty(
                "blockDeviceSnapshot",
                blockDeviceSnapshot()
            );

            if (enabled) {
                liveDevicePollTimer->start();

                // Enabling live detection should immediately
                // synchronize devices that may have changed
                // while detection was disabled.
                m_liveDetectionDebounce->start();

            } else {
                liveDevicePollTimer->stop();
            }
        }
    );

    if (m_liveDetectionAction &&
        m_liveDetectionAction->isChecked()) {

        liveDevicePollTimer->start();
    }


    connect(
        m_liveDetectionDebounce,
        &QTimer::timeout,
        this,
        [this]
        {
            setStatus(
                tx(
                    "Storage device change detected...",
                    "Datenträgeränderung erkannt..."
                )
            );

            m_backend->scanDevices();
        }
    );

    setupTrayIcon();

    connect(
        m_showTrayAction,
        &QAction::toggled,
        this,
        [this](bool enabled)
        {
            QSettings().setValue(
                QStringLiteral("showInTray"),
                enabled
            );

            if (!m_trayIcon)
                return;

            if (enabled)
                m_trayIcon->show();
            else
                m_trayIcon->hide();
        }
    );

    connect(
        m_startWithSystemAction,
        &QAction::toggled,
        this,
        &MainWindow::setStartWithSystem
    );

    setZoomPercent(
        m_zoomPercent
    );


    m_settingsMenu->addSeparator();

    m_storageUnitMenu =
        m_settingsMenu->addMenu(
            QString()
        );

    auto *storageUnitGroup =
        new QActionGroup(this);

    storageUnitGroup->setExclusive(true);

    const QString selectedStorageUnit =
        QSettings().value(
            QStringLiteral(
                "storageUnit"
            ),
            QStringLiteral("GB")
        ).toString();

    const auto addStorageUnit =
        [this,
         storageUnitGroup,
         &selectedStorageUnit](
            const QString &label,
            const QString &value
        )
        {
            QAction *action =
                m_storageUnitMenu->addAction(
                    label
                );

            action->setCheckable(true);
            action->setData(value);

            storageUnitGroup->addAction(
                action
            );

            action->setChecked(
                selectedStorageUnit == value
            );

            connect(
                action,
                &QAction::triggered,
                this,
                [this, value]
                {
                    QSettings().setValue(
                        QStringLiteral(
                            "storageUnit"
                        ),
                        value
                    );

                    if (m_hasCurrentData) {
                        renderDevice(
                            m_currentDrive,
                            m_currentData
                        );
                    }
                }
            );
        };

    addStorageUnit(
        QStringLiteral(
            "GB (decimal)"
        ),
        QStringLiteral("GB")
    );

    addStorageUnit(
        QStringLiteral(
            "GiB (binary)"
        ),
        QStringLiteral("GiB")
    );

    addStorageUnit(
        QStringLiteral(
            "TB (decimal)"
        ),
        QStringLiteral("TB")
    );

    addStorageUnit(
        QStringLiteral(
            "TiB (binary)"
        ),
        QStringLiteral("TiB")
    );



        setupAdvancedMenuActions();




        setupAtaManagementMenus();




        setupControllerMenus();




        setupMaintenanceMenus();




    // Graph / History is now functional.
    if (m_diagramAction) {
        m_diagramAction->setVisible(
            true
        );

        connect(
            m_diagramAction,
            &QAction::triggered,
            this,
            &MainWindow::openHistoryGraph
        );
    }

}
