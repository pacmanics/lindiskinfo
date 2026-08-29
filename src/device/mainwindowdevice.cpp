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

namespace
{

QString protocolName(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    if (data.contains(
            QStringLiteral("ata_smart_attributes")
        )) {
        return QStringLiteral("Serial ATA");
    }

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {
        return QStringLiteral("NVM Express");
    }

    const QString protocol =
        data.value(QStringLiteral("device"))
            .toObject()
            .value(QStringLiteral("protocol"))
            .toString();

    if (!protocol.isEmpty())
        return protocol;

    if (!drive.protocol.isEmpty())
        return drive.protocol;

    return QStringLiteral("—");
}

QString versionString(
    const QJsonObject &data
)
{
    const QJsonObject nvme =
        data.value(
            QStringLiteral("nvme_version")
        ).toObject();

    const QString nvmeString =
        nvme.value(
            QStringLiteral("string")
        ).toString();

    if (!nvmeString.isEmpty())
        return QStringLiteral("NVMe %1").arg(nvmeString);

    const QJsonObject ata =
        data.value(
            QStringLiteral("ata_version")
        ).toObject();

    const QString ataString =
        ata.value(
            QStringLiteral("string")
        ).toString();

    if (!ataString.isEmpty())
        return ataString;

    return QStringLiteral("—");
}

QString transferString(
    const QJsonObject &data
)
{
    const QJsonObject speed =
        data.value(
            QStringLiteral("interface_speed")
        ).toObject();

    const QString current =
        speed.value(
            QStringLiteral("current")
        ).toObject()
        .value(
            QStringLiteral("string")
        ).toString();

    const QString maximum =
        speed.value(
            QStringLiteral("max")
        ).toObject()
        .value(
            QStringLiteral("string")
        ).toString();

    if (!current.isEmpty() && !maximum.isEmpty())
        return current + QStringLiteral(" | ") + maximum;

    if (!current.isEmpty())
        return current;

    return QStringLiteral("—");
}

}

void MainWindow::renderDevice(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    m_currentDrive = drive;
    m_currentData = data;
    m_hasCurrentData = true;

    m_readsValue->setText(
        QStringLiteral("—")
    );

    m_writesValue->setText(
        QStringLiteral("—")
    );

    setHealth(
        HealthState::Unknown
    );

    const QString model =
        data.value(
            QStringLiteral("model_name")
        ).toString(
            data.value(
                QStringLiteral("model_number")
            ).toString()
        );

    const quint64 capacity =
        jsonUnsigned(
            data.value(
                QStringLiteral("user_capacity")
            ).toObject()
            .value(
                QStringLiteral("bytes")
            )
        );

    QString title =
        model.isEmpty()
        ? drive.name
        : model;

    if (capacity > 0) {
        title +=
            QStringLiteral(" : ") +
            formatBytes(capacity);
    }

    m_titleLabel->setText(title);

    m_firmwareValue->setText(
        data.value(
            QStringLiteral("firmware_version")
        ).toString(
            QStringLiteral("—")
        )
    );

    m_serialEdit->setText(
        data.value(
            QStringLiteral("serial_number")
        ).toString()
    );

    m_interfaceValue->setText(
        protocolName(
            drive,
            data
        )
    );

    const QString transport =
        data.value(
            QStringLiteral(
                "lindiskinfo_transport"
            )
        ).toString().trimmed().toLower();

    const bool usbDevice =
        transport ==
        QStringLiteral("usb");

    const auto localizedUsbProtocol =
        [this](QString value)
        {
            if (m_language !=
                Language::German) {

                return value;
            }

            value.replace(
                QStringLiteral(
                    "Mass Storage"
                ),
                QStringLiteral(
                    "Massenspeicher"
                )
            );

            value.replace(
                QStringLiteral(
                    "Bulk-Only"
                ),
                QStringLiteral(
                    "Bulk-Only-Transport"
                )
            );

            return value;
        };

    const auto localizedUsbPowerSource =
        [this](const QString &value)
        {
            if (m_language !=
                Language::German) {

                return value;
            }

            if (value ==
                QStringLiteral(
                    "Bus Powered"
                )) {

                return QString(
                    QStringLiteral(
                        "Busgespeist"
                    )
                );
            }

            if (value ==
                QStringLiteral(
                    "Self Powered"
                )) {

                return QString(
                    QStringLiteral(
                        "Eigenversorgt"
                    )
                );
            }

            if (value ==
                QStringLiteral(
                    "Bus/Self Powered"
                )) {

                return QString(
                    QStringLiteral(
                        "Bus-/Eigenversorgung"
                    )
                );
            }

            if (value ==
                QStringLiteral(
                    "Unknown"
                )) {

                return QString(
                    QStringLiteral(
                        "Unbekannt"
                    )
                );
            }

            return value;
        };

    m_transferValue->setText(
        transferString(data)
    );

    {
        const QString currentPcie =
            data.value(
                QStringLiteral(
                    "lindiskinfo_nvme_current_transfer_mode"
                )
            ).toString();

        const QString maxPcie =
            data.value(
                QStringLiteral(
                    "lindiskinfo_nvme_max_transfer_mode"
                )
            ).toString();

        if (!currentPcie.isEmpty()) {
            QString transferMode =
                currentPcie;

            if (!maxPcie.isEmpty()) {
                transferMode +=
                    QStringLiteral(" | ") +
                    maxPcie;
            }

            m_transferValue->setText(
                transferMode
            );
        }
    }

    if (usbDevice) {
        const quint64 usbMbps =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_link_mbps"
                    )
                )
            );

        const QString usbSpecification =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_specification"
                )
            ).toString();

        QString interfaceName =
            usbSpecification;

        if (usbSpecification ==
                QStringLiteral("USB 3.2") &&
            usbMbps >= 20000) {

            interfaceName +=
                QStringLiteral(" Gen 2x2");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.2") &&
            usbMbps >= 10000) {

            interfaceName +=
                QStringLiteral(" Gen 2");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.2") &&
            usbMbps >= 5000) {

            interfaceName +=
                QStringLiteral(" Gen 1");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.1") &&
            usbMbps >= 10000) {

            interfaceName +=
                QStringLiteral(" Gen 2");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.1") &&
            usbMbps >= 5000) {

            interfaceName +=
                QStringLiteral(" Gen 1");
        }

        if (!interfaceName.isEmpty()) {
            m_interfaceValue->setText(
                interfaceName
            );
        }

        if (usbMbps >= 1000) {
            m_transferValue->setText(
                QStringLiteral("%1 Gb/s")
                    .arg(
                        static_cast<double>(
                            usbMbps
                        ) / 1000.0,
                        0,
                        'f',
                        1
                    )
            );
        } else if (usbMbps > 0) {
            m_transferValue->setText(
                QStringLiteral("%1 Mb/s")
                    .arg(usbMbps)
            );
        }
    }

    m_standardValue->setText(
        versionString(data)
    );

    if (usbDevice) {
        const QString usbProtocol =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_protocol"
                )
            ).toString();

        if (!usbProtocol.isEmpty()) {
            m_standardValue->setText(
                localizedUsbProtocol(
                    usbProtocol
                )
            );
        }
    }

    const bool smartAvailable =
        data.value(
            QStringLiteral(
                "lindiskinfo_smart_available"
            )
        ).toBool(true);

    m_featuresValue->setText(
        smartAvailable
            ? QStringLiteral("S.M.A.R.T.")
            : tx(
                  "S.M.A.R.T. not supported",
                  "S.M.A.R.T. nicht unterstützt"
              )
    );

    const QJsonObject temperature =
        data.value(
            QStringLiteral("temperature")
        ).toObject();

    if (temperature.contains(
            QStringLiteral("current")
        )) {

        const int currentTemperature =
            static_cast<int>(
                jsonUnsigned(
                    temperature.value(
                        QStringLiteral("current")
                    )
                )
            );

        setTemperature(
            currentTemperature > 0
                ? currentTemperature
                : -1
        );
    } else {
        setTemperature(-1);
    }

    const QJsonObject powerTime =
        data.value(
            QStringLiteral("power_on_time")
        ).toObject();

    if (powerTime.contains(
            QStringLiteral("hours")
        )) {

        m_powerHoursValue->setText(
            tx(
                "%1 hours",
                "%1 Std."
            ).arg(
                formatNumber(
                    jsonUnsigned(
                        powerTime.value(
                            QStringLiteral("hours")
                        )
                    )
                )
            )
        );
    } else {
        m_powerHoursValue->setText(
            QStringLiteral("—")
        );
    }

    if (data.contains(
            QStringLiteral("power_cycle_count")
        )) {

        m_powerCyclesValue->setText(
            tx(
                "%1 count",
                "%1 mal"
            ).arg(
                formatNumber(
                    jsonUnsigned(
                        data.value(
                            QStringLiteral(
                                "power_cycle_count"
                            )
                        )
                    )
                )
            )
        );
    } else {
        m_powerCyclesValue->setText(
            QStringLiteral("—")
        );
    }

    const quint64 rotation =
        jsonUnsigned(
            data.value(
                QStringLiteral("rotation_rate")
            )
        );

    if (rotation > 0) {
        m_rotationValue->setText(
            QStringLiteral("%1 RPM")
                .arg(
                    formatNumber(rotation)
                )
        );
    } else if (usbDevice) {
        m_rotationValue->setText(
            QStringLiteral("---- (USB)")
        );
    } else {
        m_rotationValue->setText(
            QStringLiteral("---- (SSD)")
        );
    }

    m_table->setRowCount(0);

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {

        renderNvme(data);
    } else if (data.contains(
                   QStringLiteral(
                       "ata_smart_attributes"
                   )
               )) {

        renderAta(data);
    } else if (!smartAvailable) {
        setHealth(
            HealthState::Unknown
        );

        setTemperature(-1);

        m_table->setColumnHidden(
            ColumnStatus,
            true
        );

        m_table->setColumnHidden(
            ColumnId,
            true
        );

        m_table->setColumnHidden(
            ColumnAttribute,
            false
        );

        m_table->setColumnHidden(
            ColumnValue,
            false
        );

        m_table->setColumnHidden(
            ColumnCurrent,
            true
        );

        m_table->setColumnHidden(
            ColumnWorst,
            true
        );

        m_table->setColumnHidden(
            ColumnThreshold,
            true
        );

        m_table->setColumnHidden(
            ColumnRaw,
            true
        );

        m_table->setHorizontalHeaderLabels(
            {
                QString(),
                QStringLiteral("ID"),
                tx(
                    "Device Information",
                    "Geräteinformation"
                ),
                tx(
                    "Value",
                    "Wert"
                ),
                QString(),
                QString(),
                QString(),
                QString()
            }
        );

        m_table->horizontalHeader()
            ->setStretchLastSection(true);

        m_table->horizontalHeader()
            ->setSectionResizeMode(
                ColumnAttribute,
                QHeaderView::Interactive
            );

        m_table->horizontalHeader()
            ->setSectionResizeMode(
                ColumnValue,
                QHeaderView::Interactive
            );

        m_table->setColumnWidth(
            ColumnAttribute,
            330
        );

        auto addDeviceInfo =
            [this](
                const QString &name,
                const QString &value
            )
            {
                if (value.isEmpty() ||
                    value ==
                    QStringLiteral("—")) {
                    return;
                }

                const int row =
                    m_table->rowCount();

                m_table->insertRow(row);

                m_table->setItem(
                    row,
                    ColumnAttribute,
                    new QTableWidgetItem(name)
                );

                m_table->setItem(
                    row,
                    ColumnValue,
                    new QTableWidgetItem(value)
                );
            };

        addDeviceInfo(
            tx("Device", "Gerät"),
            drive.name
        );

        addDeviceInfo(
            tx("Vendor", "Hersteller"),
            data.value(
                QStringLiteral(
                    "lindiskinfo_vendor"
                )
            ).toString()
        );

        addDeviceInfo(
            tx("Product", "Produkt"),
            model
        );

        addDeviceInfo(
            tx("Revision", "Revision"),
            data.value(
                QStringLiteral(
                    "firmware_version"
                )
            ).toString()
        );

        addDeviceInfo(
            tx("Capacity", "Kapazität"),
            capacity > 0
                ? formatBytes(capacity)
                : QString()
        );

        addDeviceInfo(
            tx("Interface", "Schnittstelle"),
            m_interfaceValue->text()
        );

        const quint64 usbMbps =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_link_mbps"
                    )
                )
            );

        if (usbMbps >= 1000) {
            addDeviceInfo(
                tx(
                    "USB Link Speed",
                    "USB-Verbindungsgeschwindigkeit"
                ),
                QStringLiteral("%1 Gb/s")
                    .arg(
                        static_cast<double>(
                            usbMbps
                        ) / 1000.0,
                        0,
                        'f',
                        1
                    )
            );
        } else if (usbMbps > 0) {
            addDeviceInfo(
                tx(
                    "USB Link Speed",
                    "USB-Verbindungsgeschwindigkeit"
                ),
                QStringLiteral("%1 Mb/s")
                    .arg(usbMbps)
            );
        }

        addDeviceInfo(
            tx(
                "USB Specification",
                "USB-Spezifikation"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_specification"
                )
            ).toString()
        );

        const QString usbVendorId =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_vendor_id"
                )
            ).toString();

        const QString usbProductId =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_product_id"
                )
            ).toString();

        if (!usbVendorId.isEmpty() &&
            !usbProductId.isEmpty()) {

            addDeviceInfo(
                QStringLiteral("USB ID"),
                usbVendorId +
                    QStringLiteral(":") +
                    usbProductId
            );
        }

        addDeviceInfo(
            tx(
                "USB Port Path",
                "USB-Portpfad"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_port_path"
                )
            ).toString()
        );

        addDeviceInfo(
            tx(
                "USB Driver",
                "USB-Treiber"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_driver"
                )
            ).toString()
        );

        addDeviceInfo(
            tx(
                "USB Protocol",
                "USB-Protokoll"
            ),
            localizedUsbProtocol(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_protocol"
                    )
                ).toString()
            )
        );

        addDeviceInfo(
            tx(
                "Power Source",
                "Stromversorgung"
            ),
            localizedUsbPowerSource(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_power_source"
                    )
                ).toString()
            )
        );

        addDeviceInfo(
            tx(
                "Maximum Power",
                "Maximale Stromaufnahme"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_max_power"
                )
            ).toString()
        );

        addDeviceInfo(
            tx("Removable", "Wechseldatenträger"),
            data.value(
                QStringLiteral(
                    "lindiskinfo_removable"
                )
            ).toBool()
                ? tx("Yes", "Ja")
                : tx("No", "Nein")
        );

        addDeviceInfo(
            tx("Read Only", "Schreibgeschützt"),
            data.value(
                QStringLiteral(
                    "lindiskinfo_read_only"
                )
            ).toBool()
                ? tx("Yes", "Ja")
                : tx("No", "Nein")
        );

        const quint64 logicalSector =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_logical_sector"
                    )
                )
            );

        const quint64 physicalSector =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_physical_sector"
                    )
                )
            );

        if (logicalSector > 0) {
            addDeviceInfo(
                tx(
                    "Logical Sector Size",
                    "Logische Sektorgröße"
                ),
                QStringLiteral("%1 B")
                    .arg(logicalSector)
            );
        }

        if (physicalSector > 0) {
            addDeviceInfo(
                tx(
                    "Physical Sector Size",
                    "Physische Sektorgröße"
                ),
                QStringLiteral("%1 B")
                    .arg(physicalSector)
            );
        }

    } else {
        const QJsonObject smartStatus =
            data.value(
                QStringLiteral("smart_status")
            ).toObject();

        if (smartStatus.contains(
                QStringLiteral("passed")
            )) {

            setHealth(
                smartStatus.value(
                    QStringLiteral("passed")
                ).toBool()
                ? HealthState::Good
                : HealthState::Bad
            );
        } else {
            setHealth(
                HealthState::Unknown
            );
        }
    }

    setStatus(
        smartAvailable
            ? tx(
                  "SMART data loaded.",
                  "SMART-Daten geladen."
              )
            : tx(
                  "Device information loaded.",
                  "Geräteinformationen geladen."
              )
    );


}

void MainWindow::setHealth(
    HealthState state,
    int percentage
)
{
    QString text;
    QString style;

    switch (state) {
    case HealthState::Good:
        text =
            tx(
                "Good",
                "Gut"
            );

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #dff4ff,"
                "stop:0.45 #70c8ff,"
                "stop:1 #1ca3e8);"
                "color: #07131d;"
                "}"
            );
        break;

    case HealthState::Caution:
        text =
            tx(
                "Caution",
                "Vorsicht"
            );

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: #302400;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #d59c00;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #fffbd1,"
                "stop:0.45 #ffe44d,"
                "stop:1 #ffbd00);"
                "color: #241b00;"
                "}"
            );
        break;

    case HealthState::Bad:
        text =
            tx(
                "Bad",
                "Schlecht"
            );

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: white;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #c62828;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #ffd5d5,"
                "stop:0.45 #ff5555,"
                "stop:1 #e00000);"
                "color: white;"
                "}"
            );
        break;

    case HealthState::Unknown:
        text =
            QStringLiteral("—");

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #dff4ff,"
                "stop:0.45 #70c8ff,"
                "stop:1 #1ca3e8);"
                "color: #07131d;"
                "}"
            );
        break;
    }

    if (percentage >= 0) {
        text +=
            QStringLiteral("\n%1 %")
                .arg(percentage);
    }

    m_healthValue->setText(text);
    m_healthFrame->setStyleSheet(style);
}

void MainWindow::setTemperature(
    int temperature
)
{
    QString style;

    if (temperature < 0) {
        m_temperatureValue->setText(
            QStringLiteral("—")
        );

        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #e7f8ff,"
                "stop:0.45 #69d7ff,"
                "stop:1 #12a9ef);"
                "color: #07131d;"
                "}"
            );

        m_temperatureFrame
            ->setStyleSheet(style);

        return;
    }

    m_temperatureValue->setText(
        formatTemperature(
            temperature
        )
    );

    if (temperature >=
        m_temperatureBad) {

        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: white;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #c62828;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #ffdada,"
                "stop:0.5 #ff5c5c,"
                "stop:1 #d80000);"
                "color: white;"
                "}"
            );

    } else if (
        temperature >=
            m_temperatureCaution) {

        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: #302400;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #d59c00;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #fffac4,"
                "stop:0.5 #ffe552,"
                "stop:1 #ffb900);"
                "color: #241b00;"
                "}"
            );

    } else {
        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #e7f8ff,"
                "stop:0.45 #69d7ff,"
                "stop:1 #12a9ef);"
                "color: #07131d;"
                "}"
            );
    }

    m_temperatureFrame->setStyleSheet(
        style
    );
}

void MainWindow::toggleSerialVisibility()
{
    m_serialVisible =
        !m_serialVisible;

    m_serialEdit->setEchoMode(
        m_serialVisible
            ? QLineEdit::Normal
            : QLineEdit::Password
    );

    if (m_hideSerialAction) {
        QSignalBlocker blocker(
            m_hideSerialAction
        );

        m_hideSerialAction->setChecked(
            !m_serialVisible
        );
    }

    updateSerialButton();
}

void MainWindow::updateSerialButton()
{
    const QString iconName =
        m_serialVisible
        ? QStringLiteral("view-hidden")
        : QStringLiteral("view-visible");

    m_serialButton->setIcon(
        QIcon::fromTheme(iconName)
    );

    m_serialButton->setToolTip(
        m_serialVisible
        ? tx(
              "Hide serial number",
              "Seriennummer verbergen"
          )
        : tx(
              "Show serial number",
              "Seriennummer anzeigen"
          )
    );
}

void MainWindow::clearDisplay()
{
    m_hasCurrentData = false;
    m_currentData = QJsonObject();

    m_titleLabel->setText(
        QStringLiteral("LinDiskInfo")
    );

    setHealth(
        HealthState::Unknown
    );

    setTemperature(-1);

    m_firmwareValue->setText(
        QStringLiteral("—")
    );

    m_serialEdit->clear();

    m_interfaceValue->setText(
        QStringLiteral("—")
    );

    m_transferValue->setText(
        QStringLiteral("—")
    );

    m_standardValue->setText(
        QStringLiteral("—")
    );

    m_featuresValue->setText(
        QStringLiteral("—")
    );

    m_readsValue->setText(
        QStringLiteral("—")
    );

    m_writesValue->setText(
        QStringLiteral("—")
    );

    m_rotationValue->setText(
        QStringLiteral("—")
    );

    m_powerCyclesValue->setText(
        QStringLiteral("—")
    );

    m_powerHoursValue->setText(
        QStringLiteral("—")
    );

    m_table->setRowCount(0);
}

void MainWindow::setStatus(
    const QString &text
)
{
    m_statusLabel->setText(text);
}
