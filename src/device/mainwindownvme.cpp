// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../ui/responsivetablelayout.h"
#include "../ui/smarttablecolumns.h"

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

void MainWindow::renderNvme(
    const QJsonObject &data
)
{
    saveCurrentTableWidths();

    m_currentTableIsNvme = true;

    configureNvmeTable();

    restoreCurrentTableWidths();

    const QJsonObject nvme =
        data.value(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        ).toObject();

    auto value =
        [&nvme](const char *key)
        {
            return jsonUnsigned(
                nvme.value(
                    QString::fromLatin1(key)
                )
            );
        };

    const quint64 criticalWarning =
        value("critical_warning");

    const quint64 spare =
        value("available_spare");

    const quint64 spareThreshold =
        value("available_spare_threshold");

    const quint64 percentageUsed =
        value("percentage_used");

    const quint64 dataRead =
        value("data_units_read");

    const quint64 dataWritten =
        value("data_units_written");

    const quint64 hostReads =
        value("host_reads");

    const quint64 hostWrites =
        value("host_writes");

    const quint64 busy =
        value("controller_busy_time");

    const quint64 cycles =
        value("power_cycles");

    const quint64 hours =
        value("power_on_hours");

    const quint64 unsafe =
        value("unsafe_shutdowns");

    const quint64 mediaErrors =
        value("media_errors");

    const quint64 errorEntries =
        value("num_err_log_entries");

    const quint64 warningTemp =
        value("warning_temp_time");

    const quint64 criticalTemp =
        value("critical_comp_time");

    const int remaining =
        std::max(
            0,
            100 -
            static_cast<int>(
                percentageUsed
            )
        );

    setHealth(
        healthStateForData(
            data
        ),
        remaining
    );

    m_readsValue->setText(
        formatBytes(
            dataRead * 512000ULL
        )
    );

    m_writesValue->setText(
        formatBytes(
            dataWritten * 512000ULL
        )
    );

    m_powerCyclesValue->setText(
        tx(
            "%1 count",
            "%1 mal"
        ).arg(
            formatNumber(cycles)
        )
    );

    m_powerHoursValue->setText(
        tx(
            "%1 hours",
            "%1 Std."
        ).arg(
            formatNumber(hours)
        )
    );

    addNvmeRow(
        QStringLiteral("01"),
        tx(
            "Critical Warning",
            "Kritische Warnung"
        ),
        criticalWarning == 0
            ? tx(
                  "None",
                  "Keine"
              )
            : QString::number(
                  criticalWarning
              ),
        formatRawValue(
            criticalWarning
        ),
        criticalWarning == 0
            ? HealthState::Good
            : HealthState::Bad
    );

    const int temperature =
        static_cast<int>(
            jsonUnsigned(
                data.value(
                    QStringLiteral("temperature")
                ).toObject()
                .value(
                    QStringLiteral("current")
                )
            )
        );

    addNvmeRow(
        QStringLiteral("02"),
        tx(
            "Composite Temperature",
            "Gesamttemperatur"
        ),
        temperature > 0
            ? formatTemperature(temperature)
            : QStringLiteral("—"),
        formatRawValue(
            temperature > 0
                ? static_cast<quint64>(
                      temperature
                  )
                : 0
        )
    );

    addNvmeRow(
        QStringLiteral("03"),
        tx(
            "Available Spare",
            "Verfügbare Reserve"
        ),
        QStringLiteral("%1 %")
            .arg(spare),
        formatRawValue(spare),
        spare <= spareThreshold
            ? HealthState::Bad
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("04"),
        tx(
            "Available Spare Threshold",
            "Reserve-Grenzwert"
        ),
        QStringLiteral("%1 %")
            .arg(spareThreshold),
        formatRawValue(
            spareThreshold
        )
    );

    addNvmeRow(
        QStringLiteral("05"),
        tx(
            "Percentage Used",
            "Verbrauchte Lebensdauer"
        ),
        QStringLiteral("%1 %")
            .arg(percentageUsed),
        formatRawValue(
            percentageUsed
        ),
        remaining <=
                m_nvmeBadRemaining
            ? HealthState::Bad
            : (
                remaining <=
                    m_nvmeCautionRemaining
                    ? HealthState::Caution
                    : HealthState::Good
              )
    );

    addNvmeRow(
        QStringLiteral("06"),
        tx(
            "Data Units Read",
            "Gelesene Daten"
        ),
        formatBytes(
            dataRead * 512000ULL
        ),
        formatRawValue(dataRead)
    );

    addNvmeRow(
        QStringLiteral("07"),
        tx(
            "Data Units Written",
            "Geschriebene Daten"
        ),
        formatBytes(
            dataWritten * 512000ULL
        ),
        formatRawValue(dataWritten)
    );

    addNvmeRow(
        QStringLiteral("08"),
        tx(
            "Host Read Commands",
            "Host-Lesebefehle"
        ),
        formatNumber(hostReads),
        formatRawValue(hostReads)
    );

    addNvmeRow(
        QStringLiteral("09"),
        tx(
            "Host Write Commands",
            "Host-Schreibbefehle"
        ),
        formatNumber(hostWrites),
        formatRawValue(hostWrites)
    );

    addNvmeRow(
        QStringLiteral("0A"),
        tx(
            "Controller Busy Time",
            "Controller-Aktivzeit"
        ),
        tx(
            "%1 min",
            "%1 Min."
        ).arg(
            formatNumber(busy)
        ),
        formatRawValue(busy)
    );

    addNvmeRow(
        QStringLiteral("0B"),
        tx(
            "Power Cycles",
            "Einschaltungen"
        ),
        formatNumber(cycles),
        formatRawValue(cycles)
    );

    addNvmeRow(
        QStringLiteral("0C"),
        tx(
            "Power On Hours",
            "Betriebsstunden"
        ),
        tx(
            "%1 hours",
            "%1 Std."
        ).arg(
            formatNumber(hours)
        ),
        formatRawValue(hours)
    );

    addNvmeRow(
        QStringLiteral("0D"),
        tx(
            "Unsafe Shutdowns",
            "Unsichere Abschaltungen"
        ),
        formatNumber(unsafe),
        formatRawValue(unsafe),
        unsafe > 0
            ? HealthState::Caution
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("0E"),
        tx(
            "Media and Data Integrity Errors",
            "Medien- und Datenintegritätsfehler"
        ),
        formatNumber(mediaErrors),
        formatRawValue(mediaErrors),
        mediaErrors > 0
            ? HealthState::Bad
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("0F"),
        tx(
            "Error Information Log Entries",
            "Fehlerprotokolleinträge"
        ),
        formatNumber(errorEntries),
        formatRawValue(errorEntries),
        errorEntries > 0
            ? HealthState::Caution
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("10"),
        tx(
            "Warning Temperature Time",
            "Zeit über Warntemperatur"
        ),
        tx(
            "%1 min",
            "%1 Min."
        ).arg(
            formatNumber(warningTemp)
        ),
        formatRawValue(warningTemp),
        warningTemp > 0
            ? HealthState::Caution
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("11"),
        tx(
            "Critical Temperature Time",
            "Zeit über kritischer Temperatur"
        ),
        tx(
            "%1 min",
            "%1 Min."
        ).arg(
            formatNumber(criticalTemp)
        ),
        formatRawValue(criticalTemp),
        criticalTemp > 0
            ? HealthState::Bad
            : HealthState::Good
    );
}
