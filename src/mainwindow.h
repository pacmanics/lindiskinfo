// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "smartctlbackend.h"

#include <QFont>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QPalette>
#include <QSet>
#include <QVector>

class QAction;
class QCloseEvent;
class QActionGroup;
class QLabel;
class QFrame;
class QFileSystemWatcher;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QMenu;
class QPushButton;
class QSystemTrayIcon;
class QTableWidget;
class QTimer;
class QToolButton;
class ThemeBackgroundWidget;
class ResponsiveTableLayout;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    enum class Language
    {
        English,
        German
    };

    enum class HealthState
    {
        Good,
        Caution,
        Bad,
        Unknown
    };

    enum class TemperatureUnit
    {
        Celsius,
        Fahrenheit
    };

    QString tx(
        const char *english,
        const char *german
    ) const;

    void buildInterface();
    void buildMenus();
    void setupAtaManagementMenus();
    void setupControllerMenus();
    void setupMaintenanceMenus();
    void setupAdvancedMenuActions();
    void setupAdvancedHealthAndValueActions();
    void setupAdvancedSystemBehaviorActions();
    void setupAdvancedDriveActions();
    void applyLanguage();
    void applyTheme();

    QLabel *createValueBox();
    QLabel *createCaptionLabel();

    void addInfoRow(
        QGridLayout *layout,
        int row,
        int column,
        QLabel *caption,
        QWidget *valueWidget
    );

    void refreshDevices();
    void refreshAllData();

    void sortDrives();
    void refreshDrivePresentation();
    void applyDriveButtonLimit();

    HealthState healthStateForData(
        const QJsonObject &data,
        int *percentage = nullptr
    ) const;

    int temperatureForData(
        const QJsonObject &data
    ) const;

    void selectDrive(int index);

    QString driveDisplayIdentifier(
        const DriveInfo &drive
    ) const;

    void rebuildDriveButtons();
    void rebuildDiskMenu();

    void updateDriveButton(
        const DriveInfo &drive,
        const QJsonObject &data
    );

    void renderDevice(
        const DriveInfo &drive,
        const QJsonObject &data
    );

    void renderNvme(
        const QJsonObject &data
    );

    void renderAta(
        const QJsonObject &data
    );

    void configureNvmeTable();
    void configureAtaTable();
    void applyTableColumnLayout();

    void clearDisplay();
    void setStatus(const QString &text);

    void setHealth(
        HealthState state,
        int percentage = -1
    );

    void setTemperature(int temperature);

    void addNvmeRow(
        const QString &id,
        const QString &attribute,
        const QString &value,
        const QString &raw,
        HealthState state = HealthState::Good
    );

    void addAtaRow(
        const QString &id,
        const QString &attribute,
        const QString &current,
        const QString &worst,
        const QString &threshold,
        const QString &raw,
        HealthState state = HealthState::Good
    );

    QString translateAtaAttribute(
        const QString &name
    ) const;

    QString formatTemperature(
        int celsius
    ) const;

    QString currentReportText() const;

    void saveTextReport();
    void saveImage();
    void copyReport();

    void setAutoRefreshInterval(int minutes);
    void setAutoDetectionInterval(int seconds);
    void setZoomPercent(int percent);

    void setupTrayIcon();

    void rebuildTrayMenu();
    void updateTrayPresentation();

    void saveUiState() const;

    void saveCurrentTableWidths() const;
    void restoreCurrentTableWidths();

    void setStartWithSystem(bool enabled);
    QString autostartPath() const;

    void toggleSerialVisibility();
    void updateSerialButton();
    void updateRawColumn();

    QString ataSettingsKey(
        const DriveInfo &drive,
        const QJsonObject &data
    ) const;

    void maybeAutoAdjustAta(
        const DriveInfo &drive,
        const QJsonObject &data
    );

    void showMaintenanceResult(
        const DriveInfo &drive,
        const QJsonObject &data
    );

    QString historyDeviceKey(
        const DriveInfo &drive,
        const QJsonObject &data
    ) const;

    void loadHistory();
    void saveHistory() const;

    void recordHistorySample(
        const DriveInfo &drive,
        const QJsonObject &data
    );

    void openHistoryGraph();

    void closeEvent(
        QCloseEvent *event
    ) override;

    static quint64 jsonUnsigned(
        const QJsonValue &value
    );

    static QString formatBytes(quint64 bytes);
    static QString formatNumber(quint64 value);

    QString formatRawValue(
        quint64 value
    ) const;

    SmartctlBackend *m_backend = nullptr;

    Language m_language =
        Language::English;

    TemperatureUnit m_temperatureUnit =
        TemperatureUnit::Celsius;

    QPalette m_systemPalette;
    QFont m_baseFont;

    QVector<DriveInfo> m_drives;
    QVector<QPushButton *> m_driveButtons;

    QHash<QString, QJsonObject> m_driveData;

    int m_selectedDrive = -1;

    DriveInfo m_currentDrive;
    QJsonObject m_currentData;

    bool m_hasCurrentData = false;
    bool m_serialVisible = false;
    bool m_darkMode = false;
    bool m_currentTableIsNvme = true;
    bool m_autoRefreshAllDrives = true;

    int m_autoRefreshMinutes = 10;
    int m_autoDetectionSeconds = 0;
    int m_zoomPercent = 100;

    int m_temperatureCaution = 55;
    int m_temperatureBad = 70;

    int m_nvmeCautionRemaining = 10;
    int m_nvmeBadRemaining = 0;

    quint64 m_ataSectorCautionCount = 1;

    int m_startupDelaySeconds = 0;
    int m_displayDriveLimit = 0;

    QString m_driveSortMethod =
        QStringLiteral("default");

    QString m_trayBehavior =
        QStringLiteral("hide");

    bool m_hideNoSmart = false;
    bool m_forceQuit = false;

    bool m_aamApmAutoEnabled = false;

    bool m_ataPassThroughEnabled = true;
    bool m_externalStorageEnabled = true;
    bool m_megaRaidEnabled = true;

    QSet<QString> m_autoAdjustedAtaDrives;

    bool m_historyEnabled = true;

    bool m_tableLayoutReady = false;

    QJsonArray m_historySamples;

    QHash<QString, qint64>
        m_lastHistorySampleMs;

    QWidget *m_driveBar = nullptr;
    QHBoxLayout *m_driveLayout = nullptr;

    QLabel *m_titleLabel = nullptr;

    QFrame *m_healthFrame = nullptr;
    QLabel *m_healthCaption = nullptr;
    QLabel *m_healthValue = nullptr;

    QFrame *m_temperatureFrame = nullptr;
    QLabel *m_temperatureCaption = nullptr;
    QLabel *m_temperatureValue = nullptr;

    QLabel *m_firmwareCaption = nullptr;
    QLabel *m_interfaceCaption = nullptr;
    QLabel *m_transferCaption = nullptr;
    QLabel *m_standardCaption = nullptr;
    QLabel *m_featuresCaption = nullptr;

    QLabel *m_firmwareValue = nullptr;
    QLabel *m_interfaceValue = nullptr;
    QLabel *m_transferValue = nullptr;
    QLabel *m_standardValue = nullptr;
    QLabel *m_featuresValue = nullptr;

    QLabel *m_serialCaption = nullptr;
    QLineEdit *m_serialEdit = nullptr;
    QToolButton *m_serialButton = nullptr;

    QLabel *m_readsCaption = nullptr;
    QLabel *m_writesCaption = nullptr;
    QLabel *m_rotationCaption = nullptr;
    QLabel *m_powerCyclesCaption = nullptr;
    QLabel *m_powerHoursCaption = nullptr;

    QLabel *m_readsValue = nullptr;
    QLabel *m_writesValue = nullptr;
    QLabel *m_rotationValue = nullptr;
    QLabel *m_powerCyclesValue = nullptr;
    QLabel *m_powerHoursValue = nullptr;

    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_table = nullptr;

    ThemeBackgroundWidget *m_themeBackground =
        nullptr;

    ResponsiveTableLayout *m_tableLayoutController =
        nullptr;

    QMenu *m_fileMenu = nullptr;
    QMenu *m_editMenu = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_diskMenu = nullptr;
    QMenu *m_helpMenu = nullptr;
    QMenu *m_languageMenu = nullptr;

    QMenu *m_copyOptionsMenu = nullptr;
    QMenu *m_autoRefreshMenu = nullptr;
    QMenu *m_autoRefreshTargetMenu = nullptr;
    QMenu *m_advancedOptionsMenu = nullptr;
    QMenu *m_temperatureMenu = nullptr;
    QMenu *m_storageUnitMenu = nullptr;
    QMenu *m_autoDetectionMenu = nullptr;
    QMenu *m_rawValuesMenu = nullptr;
    QMenu *m_themeMenu = nullptr;
    QMenu *m_zoomMenu = nullptr;

    QAction *m_saveTextAction = nullptr;
    QAction *m_saveImageAction = nullptr;
    QAction *m_quitAction = nullptr;

    QAction *m_copyAction = nullptr;
    QAction *m_copyIdentifyAction = nullptr;
    QAction *m_copySmartDataAction = nullptr;
    QAction *m_copyThresholdAction = nullptr;
    QAction *m_copyAsciiAction = nullptr;

    QAction *m_refreshAction = nullptr;
    QAction *m_rereadAction = nullptr;
    QAction *m_liveDetectionAction = nullptr;
    QAction *m_diagramAction = nullptr;

    QAction *m_hideSerialAction = nullptr;
    QAction *m_showTrayAction = nullptr;
    QAction *m_startWithSystemAction = nullptr;

    QAction *m_showRawAction = nullptr;
    QAction *m_hideSmartInfoAction = nullptr;

    QAction *m_systemThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QActionGroup *m_themeGroup = nullptr;

    QAction *m_celsiusAction = nullptr;
    QAction *m_fahrenheitAction = nullptr;
    QActionGroup *m_temperatureGroup = nullptr;

    QAction *m_fontAction = nullptr;

    QAction *m_englishAction = nullptr;
    QAction *m_germanAction = nullptr;
    QActionGroup *m_languageGroup = nullptr;

    QAction *m_aboutAction = nullptr;

    QTimer *m_autoRefreshTimer = nullptr;
    QTimer *m_autoDetectionTimer = nullptr;

    QFileSystemWatcher *m_deviceWatcher = nullptr;
    QTimer *m_liveDetectionDebounce = nullptr;

    QSystemTrayIcon *m_trayIcon = nullptr;

    QMenu *m_trayMenu = nullptr;

    QHash<QString, int>
        m_lastTraySeverity;
};
