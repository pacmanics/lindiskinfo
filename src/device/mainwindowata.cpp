// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "atawear.h"
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
#include <QJsonValue>


namespace
{

struct AtaHostIoTotals
{
    bool readsValid = false;
    bool writesValid = false;

    quint64 readBytes = 0;
    quint64 writeBytes = 0;
};

bool ataJsonUnsigned(
    const QJsonValue &value,
    quint64 *result
)
{
    if (!result)
        return false;

    if (value.isString()) {
        bool ok = false;

        const quint64 parsed =
            value.toString().toULongLong(
                &ok
            );

        if (!ok)
            return false;

        *result = parsed;
        return true;
    }

    if (!value.isDouble())
        return false;

    const double number =
        value.toDouble();

    if (!std::isfinite(number) ||
        number < 0.0 ||
        std::floor(number) != number) {
        return false;
    }

    if (number >
        static_cast<double>(
            std::numeric_limits<
                quint64
            >::max()
        )) {
        return false;
    }

    *result =
        static_cast<quint64>(
            number
        );

    return true;
}

bool ataSectorsToBytes(
    quint64 sectors,
    quint64 logicalBlockSize,
    quint64 *bytes
)
{
    if (!bytes ||
        logicalBlockSize == 0) {
        return false;
    }

    const quint64 maximum =
        std::numeric_limits<
            quint64
        >::max();

    if (sectors >
        maximum /
            logicalBlockSize) {
        return false;
    }

    *bytes =
        sectors *
        logicalBlockSize;

    return true;
}

AtaHostIoTotals ataHostIoTotals(
    const QJsonObject &data
)
{
    AtaHostIoTotals result;

    quint64 logicalBlockSize = 0;

    if (!ataJsonUnsigned(
            data.value(
                QStringLiteral(
                    "logical_block_size"
                )
            ),
            &logicalBlockSize
        ) ||
        logicalBlockSize == 0) {

        return result;
    }

    const QJsonArray pages =
        data.value(
            QStringLiteral(
                "ata_device_statistics"
            )
        )
        .toObject()
        .value(
            QStringLiteral(
                "pages"
            )
        )
        .toArray();

    for (const QJsonValue &pageValue :
         pages) {

        const QJsonObject page =
            pageValue.toObject();

        if (page.value(
                QStringLiteral(
                    "number"
                )
            ).toInt(-1) != 1) {
            continue;
        }

        const QJsonArray table =
            page.value(
                QStringLiteral(
                    "table"
                )
            ).toArray();

        for (const QJsonValue &entryValue :
             table) {

            const QJsonObject entry =
                entryValue.toObject();

            const QJsonObject flags =
                entry.value(
                    QStringLiteral(
                        "flags"
                    )
                ).toObject();

            if (!flags.value(
                    QStringLiteral(
                        "valid"
                    )
                ).toBool(false)) {
                continue;
            }

            quint64 sectors = 0;

            if (!ataJsonUnsigned(
                    entry.value(
                        QStringLiteral(
                            "value"
                        )
                    ),
                    &sectors
                )) {
                continue;
            }

            const int offset =
                entry.value(
                    QStringLiteral(
                        "offset"
                    )
                ).toInt(-1);

            if (offset == 0x018) {
                quint64 bytes = 0;

                if (ataSectorsToBytes(
                        sectors,
                        logicalBlockSize,
                        &bytes
                    )) {

                    result.writeBytes =
                        bytes;

                    result.writesValid =
                        true;
                }

            } else if (
                offset == 0x028
            ) {
                quint64 bytes = 0;

                if (ataSectorsToBytes(
                        sectors,
                        logicalBlockSize,
                        &bytes
                    )) {

                    result.readBytes =
                        bytes;

                    result.readsValid =
                        true;
                }
            }
        }

        break;
    }

    return result;
}

}

void MainWindow::renderAta(
    const QJsonObject &data
)
{
    saveCurrentTableWidths();

    m_currentTableIsNvme = false;

    configureAtaTable();

    restoreCurrentTableWidths();

    const QJsonArray attributes =
        data.value(
            QStringLiteral(
                "ata_smart_attributes"
            )
        ).toObject()
        .value(
            QStringLiteral("table")
        ).toArray();

    const AtaLifeEstimate ataLife =
        ataLifeEstimate(data);

    bool caution =
        ataLife.valid &&
        ataLife.remainingPercent <=
            m_nvmeCautionRemaining;

    bool bad =
        ataLife.valid &&
        ataLife.remainingPercent <=
            m_nvmeBadRemaining;

    for (const QJsonValue &entry : attributes) {
        const QJsonObject attribute =
            entry.toObject();

        const int id =
            attribute.value(
                QStringLiteral("id")
            ).toInt();

        const QString name =
            attribute.value(
                QStringLiteral("name")
            ).toString();

        const int current =
            attribute.value(
                QStringLiteral("value")
            ).toInt();

        const int worst =
            attribute.value(
                QStringLiteral("worst")
            ).toInt();

        const int threshold =
            attribute.value(
                QStringLiteral("thresh")
            ).toInt();

        const QJsonObject raw =
            attribute.value(
                QStringLiteral("raw")
            ).toObject();

        const quint64 rawValue =
            jsonUnsigned(
                raw.value(
                    QStringLiteral("value")
                )
            );

        QString rawText =
            formatRawValue(
                rawValue
            );

        HealthState state =
            HealthState::Good;

        if ((name == QStringLiteral("Current_Pending_Sector") ||
             name == QStringLiteral("Offline_Uncorrectable")) &&
            rawValue >=
            m_ataSectorCautionCount) {

            state =
                HealthState::Caution;

            caution = true;
        }

        if (name == QStringLiteral("Reallocated_Sector_Ct") &&
            rawValue >=
            m_ataSectorCautionCount) {

            state =
                HealthState::Caution;

            caution = true;
        }

        if (threshold > 0 &&
            current <= threshold) {

            state =
                HealthState::Bad;

            bad = true;
        }

        addAtaRow(
            QStringLiteral("%1")
                .arg(
                    id,
                    2,
                    16,
                    QLatin1Char('0')
                )
                .toUpper(),
            translateAtaAttribute(name),
            QString::number(current),
            QString::number(worst),
            QString::number(threshold),
            rawText,
            state
        );
    }

    const bool passed =
        data.value(
            QStringLiteral("smart_status")
        ).toObject()
        .value(
            QStringLiteral("passed")
        ).toBool(true);

    const int lifePercentage =
        ataLife.valid
        ? ataLife.remainingPercent
        : -1;

    if (!passed || bad) {
        setHealth(
            HealthState::Bad,
            lifePercentage
        );
    } else if (caution) {
        setHealth(
            HealthState::Caution,
            lifePercentage
        );
    } else {
        setHealth(
            HealthState::Good,
            lifePercentage
        );
    }

    m_readsValue->setText(
        QStringLiteral("—")
    );

    m_writesValue->setText(
        QStringLiteral("—")
    );

    const AtaHostIoTotals hostIo =
        ataHostIoTotals(data);

    m_readsValue->setText(
        hostIo.readsValid
            ? formatBytes(
                  hostIo.readBytes
              )
            : QStringLiteral("—")
    );

    m_writesValue->setText(
        hostIo.writesValid
            ? formatBytes(
                  hostIo.writeBytes
              )
            : QStringLiteral("—")
    );
}

QString MainWindow::translateAtaAttribute(
    const QString &name
) const
{

    const QString normalizedName =
        name.trimmed();

    if (
        normalizedName ==
            QStringLiteral(
                "Unknown_Attribute"
            ) ||
        normalizedName ==
            QStringLiteral(
                "Unknown Attribute"
            )
    ) {
        return tx(
            "Unknown Attribute",
            "Unbekanntes Attribut"
        );
    }

    if (
        normalizedName ==
            QStringLiteral(
                "Unknown_SSD_Attribute"
            ) ||
        normalizedName ==
            QStringLiteral(
                "Unknown SSD Attribute"
            )
    ) {
        return tx(
            "Unknown SSD Attribute",
            "Unbekanntes SSD-Attribut"
        );
    }

    static const QHash<
        QString,
        QPair<QString, QString>
    > names =
    {
        {
            QStringLiteral("Raw_Read_Error_Rate"),
            {
                QStringLiteral("Raw Read Error Rate"),
                QStringLiteral("Lesefehlerrate")
            }
        },
        {
            QStringLiteral("Throughput_Performance"),
            {
                QStringLiteral("Throughput Performance"),
                QStringLiteral("Datendurchsatzleistung")
            }
        },
        {
            QStringLiteral("Spin_Up_Time"),
            {
                QStringLiteral("Spin-Up Time"),
                QStringLiteral("Anlaufzeit")
            }
        },
        {
            QStringLiteral("Start_Stop_Count"),
            {
                QStringLiteral("Start/Stop Count"),
                QStringLiteral("Start-/Stopp-Zyklen")
            }
        },
        {
            QStringLiteral("Reallocated_Sector_Ct"),
            {
                QStringLiteral("Reallocated Sectors Count"),
                QStringLiteral("Wiederzugewiesene Sektoren")
            }
        },
        {
            QStringLiteral("Read_Channel_Margin"),
            {
                QStringLiteral("Read Channel Margin"),
                QStringLiteral("Lesekanalreserve")
            }
        },
        {
            QStringLiteral("Seek_Error_Rate"),
            {
                QStringLiteral("Seek Error Rate"),
                QStringLiteral("Suchfehlerrate")
            }
        },
        {
            QStringLiteral("Seek_Time_Performance"),
            {
                QStringLiteral("Seek Time Performance"),
                QStringLiteral("Suchzeitleistung")
            }
        },
        {
            QStringLiteral("Power_On_Hours"),
            {
                QStringLiteral("Power-On Hours"),
                QStringLiteral("Betriebsstunden")
            }
        },
        {
            QStringLiteral("Spin_Retry_Count"),
            {
                QStringLiteral("Spin Retry Count"),
                QStringLiteral("Wiederholte Anlaufversuche")
            }
        },
        {
            QStringLiteral("Calibration_Retry_Count"),
            {
                QStringLiteral("Calibration Retry Count"),
                QStringLiteral("Wiederholte Kalibrierungsversuche")
            }
        },
        {
            QStringLiteral("Power_Cycle_Count"),
            {
                QStringLiteral("Power Cycle Count"),
                QStringLiteral("Einschaltvorgänge")
            }
        },
        {
            QStringLiteral("Soft_Read_Error_Rate"),
            {
                QStringLiteral("Soft Read Error Rate"),
                QStringLiteral("Software-Lesefehlerrate")
            }
        },
        {
            QStringLiteral("Runtime_Bad_Block"),
            {
                QStringLiteral("Runtime Bad Block"),
                QStringLiteral("Laufzeit-Schlechtblöcke")
            }
        },
        {
            QStringLiteral("End-to-End_Error"),
            {
                QStringLiteral("End-to-End Error"),
                QStringLiteral("Ende-zu-Ende-Fehler")
            }
        },
        {
            QStringLiteral("Reported_Uncorrect"),
            {
                QStringLiteral("Reported Uncorrectable Errors"),
                QStringLiteral("Gemeldete unkorrigierbare Fehler")
            }
        },
        {
            QStringLiteral("Command_Timeout"),
            {
                QStringLiteral("Command Timeout"),
                QStringLiteral("Befehlszeitüberschreitungen")
            }
        },
        {
            QStringLiteral("High_Fly_Writes"),
            {
                QStringLiteral("High Fly Writes"),
                QStringLiteral("Schreibvorgänge bei großer Flughöhe")
            }
        },
        {
            QStringLiteral("Airflow_Temperature_Cel"),
            {
                QStringLiteral("Airflow Temperature"),
                QStringLiteral("Luftstromtemperatur")
            }
        },
        {
            QStringLiteral("Air_Flow_Temperature"),
            {
                QStringLiteral("Airflow Temperature"),
                QStringLiteral("Luftstromtemperatur")
            }
        },
        {
            QStringLiteral("G-Sense_Error_Rate"),
            {
                QStringLiteral("G-Sense Error Rate"),
                QStringLiteral("Erschütterungsfehlerrate")
            }
        },
        {
            QStringLiteral("Power-Off_Retract_Count"),
            {
                QStringLiteral("Power-Off Retract Count"),
                QStringLiteral("Kopfrückzüge bei Stromausfall")
            }
        },
        {
            QStringLiteral("Load_Cycle_Count"),
            {
                QStringLiteral("Load Cycle Count"),
                QStringLiteral("Kopf-Ladezyklen")
            }
        },
        {
            QStringLiteral("Temperature_Celsius"),
            {
                QStringLiteral("Temperature"),
                QStringLiteral("Temperatur")
            }
        },
        {
            QStringLiteral("Hardware_ECC_Recovered"),
            {
                QStringLiteral("Hardware ECC Recovered"),
                QStringLiteral("Durch Hardware-ECC korrigiert")
            }
        },
        {
            QStringLiteral("Reallocated_Event_Count"),
            {
                QStringLiteral("Reallocation Event Count"),
                QStringLiteral("Ereignisse mit Sektorneuzuweisung")
            }
        },
        {
            QStringLiteral("Current_Pending_Sector"),
            {
                QStringLiteral("Current Pending Sectors"),
                QStringLiteral("Aktuell ausstehende Sektoren")
            }
        },
        {
            QStringLiteral("Offline_Uncorrectable"),
            {
                QStringLiteral("Offline Uncorrectable"),
                QStringLiteral("Offline nicht korrigierbare Sektoren")
            }
        },
        {
            QStringLiteral("UDMA_CRC_Error_Count"),
            {
                QStringLiteral("UDMA CRC Error Count"),
                QStringLiteral("UDMA-CRC-Fehler")
            }
        },
        {
            QStringLiteral("CRC_Error_Count"),
            {
                QStringLiteral("CRC Error Count"),
                QStringLiteral("CRC-Fehler")
            }
        },
        {
            QStringLiteral("Multi_Zone_Error_Rate"),
            {
                QStringLiteral("Multi-Zone Error Rate"),
                QStringLiteral("Mehrzonenfehlerrate")
            }
        },
        {
            QStringLiteral("Head_Flying_Hours"),
            {
                QStringLiteral("Head Flying Hours"),
                QStringLiteral("Kopf-Flugstunden")
            }
        },
        {
            QStringLiteral("Total_LBAs_Written"),
            {
                QStringLiteral("Total LBAs Written"),
                QStringLiteral("Geschriebene LBAs gesamt")
            }
        },
        {
            QStringLiteral("Total_LBAs_Read"),
            {
                QStringLiteral("Total LBAs Read"),
                QStringLiteral("Gelesene LBAs gesamt")
            }
        },
        {
            QStringLiteral("Read_Error_Retry_Rate"),
            {
                QStringLiteral("Read Error Retry Rate"),
                QStringLiteral("Wiederholungsrate bei Lesefehlern")
            }
        },
        {
            QStringLiteral("Free_Fall_Sensor"),
            {
                QStringLiteral("Free-Fall Sensor"),
                QStringLiteral("Freifallsensor-Ereignisse")
            }
        },
        {
            QStringLiteral("Disk_Shift"),
            {
                QStringLiteral("Disk Shift"),
                QStringLiteral("Plattenverschiebung")
            }
        },
        {
            QStringLiteral("Loaded_Hours"),
            {
                QStringLiteral("Loaded Hours"),
                QStringLiteral("Stunden mit geladenen Köpfen")
            }
        },
        {
            QStringLiteral("Load_Retry_Count"),
            {
                QStringLiteral("Load Retry Count"),
                QStringLiteral("Wiederholte Kopf-Ladevorgänge")
            }
        },
        {
            QStringLiteral("Load_Friction"),
            {
                QStringLiteral("Load Friction"),
                QStringLiteral("Kopf-Ladereibung")
            }
        },
        {
            QStringLiteral("Load-in_Time"),
            {
                QStringLiteral("Load-In Time"),
                QStringLiteral("Kopf-Ladezeit")
            }
        },
        {
            QStringLiteral("Torque_Amplification_Count"),
            {
                QStringLiteral("Torque Amplification Count"),
                QStringLiteral("Drehmomentverstärkungen")
            }
        },
        {
            QStringLiteral("GMR_Head_Amplitude"),
            {
                QStringLiteral("GMR Head Amplitude"),
                QStringLiteral("GMR-Kopfamplitude")
            }
        },
        {
            QStringLiteral("Helium_Condition_Lower"),
            {
                QStringLiteral("Helium Condition Lower"),
                QStringLiteral("Heliumzustand Untergrenze")
            }
        },
        {
            QStringLiteral("Helium_Condition_Upper"),
            {
                QStringLiteral("Helium Condition Upper"),
                QStringLiteral("Heliumzustand Obergrenze")
            }
        },

        {
            QStringLiteral("Wear_Leveling_Count"),
            {
                QStringLiteral("Wear Leveling Count"),
                QStringLiteral("Verschleißausgleich")
            }
        },
        {
            QStringLiteral("Media_Wearout_Indicator"),
            {
                QStringLiteral("Media Wearout Indicator"),
                QStringLiteral("Medienverschleißanzeige")
            }
        },
        {
            QStringLiteral("SSD_Life_Left"),
            {
                QStringLiteral("SSD Life Left"),
                QStringLiteral("Verbleibende SSD-Lebensdauer")
            }
        },
        {
            QStringLiteral("Percent_Lifetime_Remain"),
            {
                QStringLiteral("Lifetime Remaining"),
                QStringLiteral("Verbleibende Lebensdauer")
            }
        },
        {
            QStringLiteral("Remaining_Lifetime_Perc"),
            {
                QStringLiteral("Remaining Lifetime"),
                QStringLiteral("Verbleibende Lebensdauer")
            }
        },
        {
            QStringLiteral("Percent_Lifetime_Used"),
            {
                QStringLiteral("Lifetime Used"),
                QStringLiteral("Verbrauchte Lebensdauer")
            }
        },
        {
            QStringLiteral("Available_Reservd_Space"),
            {
                QStringLiteral("Available Reserved Space"),
                QStringLiteral("Verfügbarer Reservespeicher")
            }
        },
        {
            QStringLiteral("Used_Rsvd_Blk_Cnt_Tot"),
            {
                QStringLiteral("Used Reserved Block Count"),
                QStringLiteral("Verwendete Reserveblöcke")
            }
        },
        {
            QStringLiteral("Used_Rsvd_Blk_Cnt_Chip"),
            {
                QStringLiteral("Used Reserved Blocks per Chip"),
                QStringLiteral("Verwendete Reserveblöcke pro Chip")
            }
        },
        {
            QStringLiteral("Unused_Rsvd_Blk_Cnt_Tot"),
            {
                QStringLiteral("Unused Reserved Block Count"),
                QStringLiteral("Unverwendete Reserveblöcke")
            }
        },
        {
            QStringLiteral("Program_Fail_Cnt_Total"),
            {
                QStringLiteral("Program Fail Count"),
                QStringLiteral("Programmierfehler")
            }
        },
        {
            QStringLiteral("Program_Fail_Count"),
            {
                QStringLiteral("Program Fail Count"),
                QStringLiteral("Programmierfehler")
            }
        },
        {
            QStringLiteral("Program_Fail_Count_Chip"),
            {
                QStringLiteral("Program Fail Count per Chip"),
                QStringLiteral("Programmierfehler pro Chip")
            }
        },
        {
            QStringLiteral("Erase_Fail_Count_Total"),
            {
                QStringLiteral("Erase Fail Count"),
                QStringLiteral("Löschfehler")
            }
        },
        {
            QStringLiteral("Erase_Fail_Count"),
            {
                QStringLiteral("Erase Fail Count"),
                QStringLiteral("Löschfehler")
            }
        },
        {
            QStringLiteral("Erase_Fail_Count_Chip"),
            {
                QStringLiteral("Erase Fail Count per Chip"),
                QStringLiteral("Löschfehler pro Chip")
            }
        },
        {
            QStringLiteral("Uncorrectable_Error_Cnt"),
            {
                QStringLiteral("Uncorrectable Error Count"),
                QStringLiteral("Nicht korrigierbare Fehler")
            }
        },
        {
            QStringLiteral("Uncorrectable_Error_Count"),
            {
                QStringLiteral("Uncorrectable Error Count"),
                QStringLiteral("Nicht korrigierbare Fehler")
            }
        },
        {
            QStringLiteral("ECC_Error_Rate"),
            {
                QStringLiteral("ECC Error Rate"),
                QStringLiteral("ECC-Fehlerrate")
            }
        },
        {
            QStringLiteral("POR_Recovery_Count"),
            {
                QStringLiteral("Power-On Recovery Count"),
                QStringLiteral("Wiederherstellungen nach Einschalten")
            }
        },
        {
            QStringLiteral("Unexpected_Power_Loss_Ct"),
            {
                QStringLiteral("Unexpected Power Loss Count"),
                QStringLiteral("Unerwartete Stromausfälle")
            }
        },
        {
            QStringLiteral("Host_Writes_32MiB"),
            {
                QStringLiteral("Host Writes (32 MiB)"),
                QStringLiteral("Host-Schreibdaten (32 MiB)")
            }
        },
        {
            QStringLiteral("Host_Reads_32MiB"),
            {
                QStringLiteral("Host Reads (32 MiB)"),
                QStringLiteral("Host-Lesedaten (32 MiB)")
            }
        },
        {
            QStringLiteral("NAND_Writes_1GiB"),
            {
                QStringLiteral("NAND Writes (1 GiB)"),
                QStringLiteral("NAND-Schreibdaten (1 GiB)")
            }
        },
        {
            QStringLiteral("Host_Writes_GiB"),
            {
                QStringLiteral("Host Writes (GiB)"),
                QStringLiteral("Host-Schreibdaten (GiB)")
            }
        },
        {
            QStringLiteral("Host_Reads_GiB"),
            {
                QStringLiteral("Host Reads (GiB)"),
                QStringLiteral("Host-Lesedaten (GiB)")
            }
        },
        {
            QStringLiteral("NAND_Writes_GiB"),
            {
                QStringLiteral("NAND Writes (GiB)"),
                QStringLiteral("NAND-Schreibdaten (GiB)")
            }
        },
        {
            QStringLiteral("Workld_Media_Wear_Indic"),
            {
                QStringLiteral("Workload Media Wear Indicator"),
                QStringLiteral("Arbeitslast-Medienverschleiß")
            }
        },
        {
            QStringLiteral("Workld_Host_Reads_Perc"),
            {
                QStringLiteral("Workload Host Reads Percentage"),
                QStringLiteral("Host-Leseanteil der Arbeitslast")
            }
        },
        {
            QStringLiteral("Workload_Minutes"),
            {
                QStringLiteral("Workload Minutes"),
                QStringLiteral("Arbeitslast-Minuten")
            }
        },
        {
            QStringLiteral("Thermal_Throttle_Status"),
            {
                QStringLiteral("Thermal Throttle Status"),
                QStringLiteral("Status der thermischen Drosselung")
            }
        },
        {
            QStringLiteral("Retired_Block_Count"),
            {
                QStringLiteral("Retired Block Count"),
                QStringLiteral("Ausgemusterte Blöcke")
            }
        },
        {
            QStringLiteral("Reallocated_Block_Count"),
            {
                QStringLiteral("Reallocated Block Count"),
                QStringLiteral("Neu zugewiesene Blöcke")
            }
        },
        {
            QStringLiteral("Flash_Writes_GiB"),
            {
                QStringLiteral("Flash Writes (GiB)"),
                QStringLiteral("Flash-Schreibdaten (GiB)")
            }
        },
        {
            QStringLiteral("Lifetime_Writes_GiB"),
            {
                QStringLiteral("Lifetime Writes (GiB)"),
                QStringLiteral("Schreibdaten über Lebensdauer (GiB)")
            }
        },
        {
            QStringLiteral("Lifetime_Reads_GiB"),
            {
                QStringLiteral("Lifetime Reads (GiB)"),
                QStringLiteral("Lesedaten über Lebensdauer (GiB)")
            }
        },
        {
            QStringLiteral("Wear_Range_Delta"),
            {
                QStringLiteral("Wear Range Delta"),
                QStringLiteral("Verschleißspannweite")
            }
        },
        {
            QStringLiteral("SATA_Downshift_Count"),
            {
                QStringLiteral("SATA Downshift Count"),
                QStringLiteral("SATA-Rückstufungen")
            }
        }
    };

    const auto it =
        names.constFind(name);

    if (it != names.constEnd()) {
        return m_language ==
                Language::German
            ? it.value().second
            : it.value().first;
    }

    QString readable = name;

    readable.replace(
        QLatin1Char('_'),
        QLatin1Char(' ')
    );

    // Vendor-specific attributes that are not in the
    // known translation table remain readable instead
    // of exposing raw smartctl identifiers with underscores.
    return readable;
}
