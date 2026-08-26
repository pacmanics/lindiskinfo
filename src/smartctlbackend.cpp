// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "smartctlbackend.h"
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QFileInfo>
#include <QFile>
#include <QDir>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace
{

quint64 jsonUnsigned(
    const QJsonValue &value
)
{
    if (value.isDouble()) {
        const qint64 number =
            value.toInteger();

        return number < 0
            ? 0
            : static_cast<quint64>(number);
    }

    if (value.isString()) {
        bool ok = false;

        const quint64 number =
            value.toString().toULongLong(&ok);

        return ok ? number : 0;
    }

    return 0;
}

struct UsbMetadata
{
    QString specification;
    QString vendorId;
    QString productId;
    QString portPath;
    QString driver;
    QString protocol;
    QString powerSource;
    QString maxPower;

    quint64 linkMbps = 0;
};

QString readSysfsText(
    const QString &path
)
{
    QFile file(path);

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text
        )) {
        return QString();
    }

    return QString::fromUtf8(
        file.readAll()
    ).trimmed();
}

QString formatUsbSpecification(
    const QString &bcdUsb
)
{
    bool ok = false;

    const double value =
        bcdUsb.toDouble(&ok);

    if (!ok || value <= 0.0)
        return QString();

    return QStringLiteral("USB %1")
        .arg(
            QString::number(
                value,
                'f',
                1
            )
        );
}

UsbMetadata readUsbMetadata(
    const QString &devicePath
)
{
    UsbMetadata result;

    const QString deviceName =
        QFileInfo(devicePath)
            .fileName();

    QString current =
        QFileInfo(
            QStringLiteral(
                "/sys/class/block/%1/device"
            ).arg(deviceName)
        ).canonicalFilePath();

    if (current.isEmpty())
        return result;

    QString interfacePath;
    QString usbDevicePath;

    for (int depth = 0;
         depth < 20 &&
         !current.isEmpty();
         ++depth) {

        if (interfacePath.isEmpty() &&
            QFileInfo::exists(
                current +
                QStringLiteral(
                    "/bInterfaceClass"
                )
            )) {

            interfacePath = current;
        }

        if (QFileInfo::exists(
                current +
                QStringLiteral("/idVendor")
            ) &&
            QFileInfo::exists(
                current +
                QStringLiteral("/idProduct")
            )) {

            usbDevicePath = current;
            break;
        }

        QDir directory(current);

        if (!directory.cdUp())
            break;

        const QString parent =
            directory.canonicalPath();

        if (parent == current)
            break;

        current = parent;
    }

    if (usbDevicePath.isEmpty())
        return result;

    result.vendorId =
        readSysfsText(
            usbDevicePath +
            QStringLiteral("/idVendor")
        ).toLower();

    result.productId =
        readSysfsText(
            usbDevicePath +
            QStringLiteral("/idProduct")
        ).toLower();

    result.portPath =
        QFileInfo(
            usbDevicePath
        ).fileName();

    result.specification =
        formatUsbSpecification(
            readSysfsText(
                usbDevicePath +
                QStringLiteral("/version")
            )
        );

    {
        bool ok = false;

        const double speed =
            readSysfsText(
                usbDevicePath +
                QStringLiteral("/speed")
            ).toDouble(&ok);

        if (ok && speed > 0.0) {
            result.linkMbps =
                static_cast<quint64>(
                    speed + 0.5
                );
        }
    }

    QString maxPower =
        readSysfsText(
            usbDevicePath +
            QStringLiteral("/bMaxPower")
        );

    if (!maxPower.isEmpty()) {
        maxPower.replace(
            QRegularExpression(
                QStringLiteral(
                    "\\s*mA$"
                )
            ),
            QStringLiteral(" mA")
        );

        result.maxPower =
            maxPower;
    }

    {
        QString attributes =
            readSysfsText(
                usbDevicePath +
                QStringLiteral(
                    "/bmAttributes"
                )
            );

        attributes.remove(
            QStringLiteral("0x"),
            Qt::CaseInsensitive
        );

        bool ok = false;

        const uint value =
            attributes.toUInt(
                &ok,
                16
            );

        if (ok) {
            result.powerSource =
                (value & 0x40U)
                    ? QStringLiteral(
                          "Self Powered"
                      )
                    : QStringLiteral(
                          "Bus Powered"
                      );
        }
    }

    if (!interfacePath.isEmpty()) {
        const QFileInfo driverLink(
            interfacePath +
            QStringLiteral("/driver")
        );

        if (driverLink.isSymLink()) {
            result.driver =
                QFileInfo(
                    driverLink.symLinkTarget()
                ).fileName();
        }

        const QString interfaceClass =
            readSysfsText(
                interfacePath +
                QStringLiteral(
                    "/bInterfaceClass"
                )
            ).toLower();

        const QString interfaceSubclass =
            readSysfsText(
                interfacePath +
                QStringLiteral(
                    "/bInterfaceSubClass"
                )
            ).toLower();

        const QString interfaceProtocol =
            readSysfsText(
                interfacePath +
                QStringLiteral(
                    "/bInterfaceProtocol"
                )
            ).toLower();

        QStringList protocolParts;

        if (interfaceClass ==
            QStringLiteral("08")) {

            protocolParts
                << QStringLiteral(
                       "Mass Storage"
                   );
        }

        if (interfaceSubclass ==
            QStringLiteral("06")) {

            protocolParts
                << QStringLiteral("SCSI");
        }

        if (interfaceProtocol ==
            QStringLiteral("50")) {

            protocolParts
                << QStringLiteral(
                       "Bulk-Only"
                   );
        }

        result.protocol =
            protocolParts.join(
                QStringLiteral(" / ")
            );
    }

    return result;
}

struct NvmePcieMetadata
{
    QString currentMode;
    QString maxMode;
};

QString pcieGenerationFromSpeed(
    const QString &speedText
)
{
    static const QRegularExpression numberExpression(
        QStringLiteral(
            "([0-9]+(?:\\.[0-9]+)?)"
        )
    );

    const QRegularExpressionMatch match =
        numberExpression.match(
            speedText
        );

    if (!match.hasMatch())
        return QString();

    bool ok = false;

    const double speed =
        match.captured(1)
            .toDouble(&ok);

    if (!ok)
        return QString();

    if (speed >= 63.0)
        return QStringLiteral("PCIe 6.0");

    if (speed >= 31.0)
        return QStringLiteral("PCIe 5.0");

    if (speed >= 15.0)
        return QStringLiteral("PCIe 4.0");

    if (speed >= 7.0)
        return QStringLiteral("PCIe 3.0");

    if (speed >= 4.0)
        return QStringLiteral("PCIe 2.0");

    if (speed >= 2.0)
        return QStringLiteral("PCIe 1.0");

    return QString();
}

QString formatPcieMode(
    const QString &speedText,
    const QString &widthText
)
{
    const QString generation =
        pcieGenerationFromSpeed(
            speedText
        );

    bool widthOk = false;

    const int width =
        widthText.trimmed()
            .toInt(&widthOk);

    if (generation.isEmpty()) {
        if (!speedText.isEmpty() &&
            widthOk &&
            width > 0) {

            return QStringLiteral(
                "%1 x%2"
            ).arg(
                speedText.trimmed()
            ).arg(
                width
            );
        }

        return speedText.trimmed();
    }

    if (!widthOk || width <= 0)
        return generation;

    return QStringLiteral(
        "%1 x%2"
    ).arg(
        generation
    ).arg(
        width
    );
}

NvmePcieMetadata readNvmePcieMetadata(
    const QString &devicePath
)
{
    NvmePcieMetadata result;

    const QString baseName =
        QFileInfo(devicePath)
            .fileName();

    static const QRegularExpression
        controllerExpression(
            QStringLiteral(
                "^(nvme[0-9]+)"
                "(?:n[0-9]+)?$"
            )
        );

    const QRegularExpressionMatch match =
        controllerExpression.match(
            baseName
        );

    if (!match.hasMatch())
        return result;

    const QString controller =
        match.captured(1);

    const QString pciPath =
        QFileInfo(
            QStringLiteral(
                "/sys/class/nvme/%1/device"
            ).arg(
                controller
            )
        ).canonicalFilePath();

    if (pciPath.isEmpty())
        return result;

    const QString currentSpeed =
        readSysfsText(
            pciPath +
            QStringLiteral(
                "/current_link_speed"
            )
        );

    const QString currentWidth =
        readSysfsText(
            pciPath +
            QStringLiteral(
                "/current_link_width"
            )
        );

    const QString maxSpeed =
        readSysfsText(
            pciPath +
            QStringLiteral(
                "/max_link_speed"
            )
        );

    const QString maxWidth =
        readSysfsText(
            pciPath +
            QStringLiteral(
                "/max_link_width"
            )
        );

    result.currentMode =
        formatPcieMode(
            currentSpeed,
            currentWidth
        );

    result.maxMode =
        formatPcieMode(
            maxSpeed,
            maxWidth
        );

    return result;
}

QString cleanText(
    const QJsonValue &value
)
{
    return value.toString().trimmed();
}

QString displayProtocol(
    const QString &transport
)
{
    const QString value =
        transport.trimmed().toLower();

    if (value == QStringLiteral("usb"))
        return QStringLiteral("USB");

    if (value == QStringLiteral("sata"))
        return QStringLiteral("Serial ATA");

    if (value == QStringLiteral("ata"))
        return QStringLiteral("ATA");

    if (value == QStringLiteral("nvme"))
        return QStringLiteral("NVM Express");

    if (value == QStringLiteral("sas"))
        return QStringLiteral("SAS");

    if (value == QStringLiteral("scsi"))
        return QStringLiteral("SCSI");

    if (value == QStringLiteral("mmc"))
        return QStringLiteral("MMC");

    if (!transport.isEmpty())
        return transport.toUpper();

    return QString();
}

QString nvmeControllerPath(
    const QString &path
)
{
    static const QRegularExpression expression(
        QStringLiteral(
            "^(/dev/nvme[0-9]+)n[0-9]+(?:p[0-9]+)?$"
        )
    );

    const QRegularExpressionMatch match =
        expression.match(path);

    if (!match.hasMatch())
        return QString();

    return match.captured(1);
}

bool ignoredDevicePath(
    const QString &path
)
{
    static const QStringList prefixes =
    {
        QStringLiteral("/dev/zram"),
        QStringLiteral("/dev/loop"),
        QStringLiteral("/dev/ram"),
        QStringLiteral("/dev/dm-"),
        QStringLiteral("/dev/md")
    };

    for (const QString &prefix : prefixes) {
        if (path.startsWith(prefix))
            return true;
    }

    return false;
}

int driveOrderRank(
    const DriveInfo &drive
)
{
    const QString transport =
        drive.transport.trimmed().toLower();

    const QString protocol =
        drive.protocol.trimmed().toLower();

    if (transport == QStringLiteral("usb") ||
        protocol == QStringLiteral("usb")) {
        return 2;
    }

    if (transport == QStringLiteral("nvme") ||
        protocol == QStringLiteral("nvm express") ||
        drive.name.startsWith(QStringLiteral("/dev/nvme"))) {
        return 1;
    }

    return 0;
}

bool hasSmartData(
    const QJsonObject &data
)
{
    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {
        return true;
    }

    if (data.contains(
            QStringLiteral(
                "ata_smart_attributes"
            )
        )) {
        return true;
    }

    const QJsonObject support =
        data.value(
            QStringLiteral("smart_support")
        ).toObject();

    if (support.contains(
            QStringLiteral("available")
        )) {
        return support.value(
            QStringLiteral("available")
        ).toBool();
    }

    return data.contains(
        QStringLiteral("smart_status")
    );
}

void mergeLinuxMetadata(
    DriveInfo &target,
    const DriveInfo &source
)
{
    if (target.model.isEmpty())
        target.model = source.model;

    if (target.vendor.isEmpty())
        target.vendor = source.vendor;

    if (target.transport.isEmpty())
        target.transport = source.transport;

    if (target.protocol.isEmpty())
        target.protocol = source.protocol;

    if (target.capacityBytes == 0)
        target.capacityBytes = source.capacityBytes;

    if (target.serial.isEmpty())
        target.serial = source.serial;

    if (target.revision.isEmpty())
        target.revision = source.revision;

    if (target.logicalSectorBytes == 0)
        target.logicalSectorBytes =
            source.logicalSectorBytes;

    if (target.physicalSectorBytes == 0)
        target.physicalSectorBytes =
            source.physicalSectorBytes;

    if (target.usbSpecification.isEmpty())
        target.usbSpecification =
            source.usbSpecification;

    if (target.usbVendorId.isEmpty())
        target.usbVendorId =
            source.usbVendorId;

    if (target.usbProductId.isEmpty())
        target.usbProductId =
            source.usbProductId;

    if (target.usbPortPath.isEmpty())
        target.usbPortPath =
            source.usbPortPath;

    if (target.usbDriver.isEmpty())
        target.usbDriver =
            source.usbDriver;

    if (target.usbProtocol.isEmpty())
        target.usbProtocol =
            source.usbProtocol;

    if (target.usbPowerSource.isEmpty())
        target.usbPowerSource =
            source.usbPowerSource;

    if (target.usbMaxPower.isEmpty())
        target.usbMaxPower =
            source.usbMaxPower;

    if (target.usbLinkMbps == 0)
        target.usbLinkMbps =
            source.usbLinkMbps;

    target.removable =
        target.removable ||
        source.removable;

    target.readOnly =
        target.readOnly ||
        source.readOnly;

    if (target.infoName.isEmpty())
        target.infoName = source.infoName;

    if (target.nvmeCurrentTransferMode.isEmpty())
        target.nvmeCurrentTransferMode =
            source.nvmeCurrentTransferMode;

    if (target.nvmeMaxTransferMode.isEmpty())
        target.nvmeMaxTransferMode =
            source.nvmeMaxTransferMode;
}

}

SmartctlBackend::SmartctlBackend(QObject *parent)
    : QObject(parent),
      m_helper(new QProcess(this))
{
    connect(
        m_helper,
        &QProcess::started,
        this,
        &SmartctlBackend::helperReady
    );

    connect(
        m_helper,
        &QProcess::readyReadStandardOutput,
        this,
        &SmartctlBackend::processHelperOutput
    );

    connect(
        m_helper,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError)
        {
            if (m_stopping ||
                QCoreApplication::closingDown()) {
                return;
            }

            QString error =
                QString::fromUtf8(
                    m_helper->readAllStandardError()
                ).trimmed();

            if (error.isEmpty())
                error = m_helper->errorString();

            emit helperFailed(error);
        }
    );

    connect(
        m_helper,
        &QProcess::finished,
        this,
        [this](int, QProcess::ExitStatus)
        {
            if (m_stopping ||
                QCoreApplication::closingDown()) {
                return;
            }

            QString error =
                QString::fromUtf8(
                    m_helper->readAllStandardError()
                ).trimmed();

            if (error.isEmpty()) {
                error =
                    QStringLiteral(
                        "The privileged SMART helper stopped unexpectedly."
                    );
            }

            emit helperFailed(error);
        }
    );
}

SmartctlBackend::~SmartctlBackend()
{
    m_stopping = true;

    disconnect(
        m_helper,
        nullptr,
        this,
        nullptr
    );

    if (m_helper->state() ==
        QProcess::NotRunning) {
        return;
    }

    m_helper->closeWriteChannel();

    if (!m_helper->waitForFinished(500)) {
        m_helper->terminate();

        if (!m_helper->waitForFinished(500)) {
            m_helper->kill();
            m_helper->waitForFinished();
        }
    }
}

void SmartctlBackend::start()
{
    if (m_helper->state() !=
        QProcess::NotRunning) {
        return;
    }

    m_helper->start(
        QStringLiteral("/usr/bin/pkexec"),
        {
            QCoreApplication::applicationFilePath(),
            QStringLiteral("--privileged-helper")
        }
    );
}

void SmartctlBackend::scanDevices()
{
    m_linuxDrives =
        scanLinuxBlockDevices();

    sendRequest(
        {
            {
                QStringLiteral("command"),
                QStringLiteral("scan")
            }
        }
    );
}

void SmartctlBackend::requestDeviceData(
    const DriveInfo &drive
)
{
    QString effectiveType =
        drive.type.trimmed();

    const QString transport =
        drive.transport
            .trimmed()
            .toLower();

    const bool externalSatCandidate =
        transport ==
            QStringLiteral("usb") ||
        transport ==
            QStringLiteral("ieee1394") ||
        transport ==
            QStringLiteral("firewire") ||
        transport ==
            QStringLiteral("sbp");

    const bool ataPassThrough =
        QSettings().value(
            QStringLiteral(
                "ataPassThrough"
            ),
            true
        ).toBool();

    if (ataPassThrough &&
        externalSatCandidate &&
        (
            effectiveType.isEmpty() ||
            effectiveType.compare(
                QStringLiteral("auto"),
                Qt::CaseInsensitive
            ) == 0 ||
            effectiveType.compare(
                QStringLiteral("scsi"),
                Qt::CaseInsensitive
            ) == 0
        )) {

        effectiveType =
            QStringLiteral(
                "sat,auto"
            );
    }

    m_pendingDrives[
        drive.name
    ].enqueue(
        drive
    );

    sendRequest(
        {
            {
                QStringLiteral("command"),
                QStringLiteral("read")
            },
            {
                QStringLiteral("name"),
                drive.name
            },
            {
                QStringLiteral(
                    "device_type"
                ),
                effectiveType
            }
        }
    );
}

void SmartctlBackend::setAtaPowerSettings(
    const DriveInfo &drive,
    const QString &aamValue,
    const QString &apmValue
)
{
    QString effectiveType =
        drive.type.trimmed();

    const QString transport =
        drive.transport
            .trimmed()
            .toLower();

    const bool externalSatCandidate =
        transport ==
            QStringLiteral("usb") ||
        transport ==
            QStringLiteral("ieee1394") ||
        transport ==
            QStringLiteral("firewire") ||
        transport ==
            QStringLiteral("sbp");

    const bool ataPassThrough =
        QSettings().value(
            QStringLiteral(
                "ataPassThrough"
            ),
            true
        ).toBool();

    if (ataPassThrough &&
        externalSatCandidate &&
        (
            effectiveType.isEmpty() ||
            effectiveType.compare(
                QStringLiteral("auto"),
                Qt::CaseInsensitive
            ) == 0 ||
            effectiveType.compare(
                QStringLiteral("scsi"),
                Qt::CaseInsensitive
            ) == 0
        )) {

        effectiveType =
            QStringLiteral(
                "sat,auto"
            );
    }

    m_pendingDrives[
        drive.name
    ].enqueue(
        drive
    );

    sendRequest(
        {
            {
                QStringLiteral("command"),
                QStringLiteral("ata_set")
            },
            {
                QStringLiteral("name"),
                drive.name
            },
            {
                QStringLiteral(
                    "device_type"
                ),
                effectiveType
            },
            {
                QStringLiteral("aam"),
                aamValue
            },
            {
                QStringLiteral("apm"),
                apmValue
            }
        }
    );
}

void SmartctlBackend::requestMaintenance(
    const DriveInfo &drive,
    const QString &operation
)
{
    static const QSet<QString>
        allowedOperations =
        {
            QStringLiteral("test_short"),
            QStringLiteral("test_long"),
            QStringLiteral("test_abort"),
            QStringLiteral("log_selftest"),
            QStringLiteral("log_error"),
            QStringLiteral("log_devstat"),
            QStringLiteral("log_sataphy")
        };

    if (!allowedOperations.contains(
            operation
        )) {
        return;
    }

    QString effectiveType =
        drive.type.trimmed();

    const QString transport =
        drive.transport
            .trimmed()
            .toLower();

    const bool externalSatCandidate =
        transport ==
            QStringLiteral("usb") ||
        transport ==
            QStringLiteral("ieee1394") ||
        transport ==
            QStringLiteral("firewire") ||
        transport ==
            QStringLiteral("sbp");

    const bool ataPassThrough =
        QSettings().value(
            QStringLiteral(
                "ataPassThrough"
            ),
            true
        ).toBool();

    if (ataPassThrough &&
        externalSatCandidate &&
        (
            effectiveType.isEmpty() ||
            effectiveType.compare(
                QStringLiteral("auto"),
                Qt::CaseInsensitive
            ) == 0 ||
            effectiveType.compare(
                QStringLiteral("scsi"),
                Qt::CaseInsensitive
            ) == 0
        )) {

        effectiveType =
            QStringLiteral(
                "sat,auto"
            );
    }

    m_pendingDrives[
        drive.name
    ].enqueue(
        drive
    );

    sendRequest(
        {
            {
                QStringLiteral("command"),
                QStringLiteral(
                    "maintenance"
                )
            },
            {
                QStringLiteral("name"),
                drive.name
            },
            {
                QStringLiteral(
                    "device_type"
                ),
                effectiveType
            },
            {
                QStringLiteral(
                    "operation"
                ),
                operation
            }
        }
    );
}



void SmartctlBackend::sendRequest(
    const QJsonObject &request
)
{
    if (m_helper->state() !=
        QProcess::Running) {
        return;
    }

    QByteArray data =
        QJsonDocument(request)
            .toJson(QJsonDocument::Compact);

    data.append('\n');

    m_helper->write(data);
}

QVector<DriveInfo>
SmartctlBackend::scanLinuxBlockDevices() const
{
    QVector<DriveInfo> drives;

    QString executable =
        QStandardPaths::findExecutable(
            QStringLiteral("lsblk")
        );

    if (executable.isEmpty())
        executable =
            QStringLiteral("/usr/bin/lsblk");

    QProcess process;

    process.start(
        executable,
        {
            QStringLiteral("-J"),
            QStringLiteral("-b"),
            QStringLiteral("-d"),
            QStringLiteral("-o"),
            QStringLiteral(
                "NAME,PATH,MODEL,VENDOR,SERIAL,REV,TRAN,SIZE,TYPE,RM,RO,LOG-SEC,PHY-SEC"
            )
        }
    );

    if (!process.waitForStarted(2000))
        return drives;

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return drives;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            process.readAllStandardOutput(),
            &parseError
        );

    if (parseError.error !=
            QJsonParseError::NoError ||
        !document.isObject()) {
        return drives;
    }

    const QJsonArray devices =
        document.object()
            .value(
                QStringLiteral("blockdevices")
            ).toArray();

    for (const QJsonValue &value : devices) {
        const QJsonObject object =
            value.toObject();

        const QString deviceType =
            cleanText(
                object.value(
                    QStringLiteral("type")
                )
            );

        if (deviceType !=
            QStringLiteral("disk")) {
            continue;
        }

        DriveInfo drive;

        drive.name =
            cleanText(
                object.value(
                    QStringLiteral("path")
                )
            );

        if (drive.name.isEmpty()) {
            const QString name =
                cleanText(
                    object.value(
                        QStringLiteral("name")
                    )
                );

            if (!name.isEmpty()) {
                drive.name =
                    QStringLiteral("/dev/") +
                    name;
            }
        }

        if (drive.name.isEmpty())
            continue;

        static const QRegularExpression
            supportedDevicePath(
                QStringLiteral(
                    "^/dev/(?:sd[a-z]+|nvme[0-9]+n[0-9]+)$"
                )
            );

        if (!supportedDevicePath
                .match(drive.name)
                .hasMatch()) {
            continue;
        }

        drive.model =
            cleanText(
                object.value(
                    QStringLiteral("model")
                )
            );

        drive.vendor =
            cleanText(
                object.value(
                    QStringLiteral("vendor")
                )
            );

        drive.transport =
            cleanText(
                object.value(
                    QStringLiteral("tran")
                )
            );

        drive.serial =
            cleanText(
                object.value(
                    QStringLiteral("serial")
                )
            );

        drive.revision =
            cleanText(
                object.value(
                    QStringLiteral("rev")
                )
            );

        drive.logicalSectorBytes =
            jsonUnsigned(
                object.value(
                    QStringLiteral("log-sec")
                )
            );

        drive.physicalSectorBytes =
            jsonUnsigned(
                object.value(
                    QStringLiteral("phy-sec")
                )
            );

        drive.readOnly =
            jsonUnsigned(
                object.value(
                    QStringLiteral("ro")
                )
            ) != 0;

        if (drive.transport.compare(
                QStringLiteral("usb"),
                Qt::CaseInsensitive
            ) == 0) {

            const UsbMetadata usb =
                readUsbMetadata(
                    drive.name
                );

            drive.usbSpecification =
                usb.specification;

            drive.usbVendorId =
                usb.vendorId;

            drive.usbProductId =
                usb.productId;

            drive.usbPortPath =
                usb.portPath;

            drive.usbDriver =
                usb.driver;

            drive.usbProtocol =
                usb.protocol;

            drive.usbPowerSource =
                usb.powerSource;

            drive.usbMaxPower =
                usb.maxPower;

            drive.usbLinkMbps =
                usb.linkMbps;
        }

        drive.protocol =
            displayProtocol(
                drive.transport
            );

        drive.capacityBytes =
            jsonUnsigned(
                object.value(
                    QStringLiteral("size")
                )
            );

        const QJsonValue removable =
            object.value(
                QStringLiteral("rm")
            );

        if (removable.isBool()) {
            drive.removable =
                removable.toBool();
        } else {
            drive.removable =
                jsonUnsigned(removable) != 0;
        }

        QString combinedModel;

        if (!drive.vendor.isEmpty())
            combinedModel += drive.vendor;

        if (!drive.model.isEmpty()) {
            if (!combinedModel.isEmpty())
                combinedModel += QLatin1Char(' ');

            combinedModel += drive.model;
        }

        drive.infoName =
            combinedModel.trimmed();

        drive.smartctlDiscovered = false;

        if (drive.name.startsWith(
                QStringLiteral(
                    "/dev/nvme"
                )
            )) {

            const NvmePcieMetadata pcie =
                readNvmePcieMetadata(
                    drive.name
                );

            drive.nvmeCurrentTransferMode =
                pcie.currentMode;

            drive.nvmeMaxTransferMode =
                pcie.maxMode;
        }

        drives.append(drive);
    }

    return drives;
}

QVector<DriveInfo>
SmartctlBackend::mergeDriveLists(
    const QVector<DriveInfo> &smartctlDrives,
    const QVector<DriveInfo> &linuxDrives
) const
{
    QVector<DriveInfo> result =
        smartctlDrives;

    for (DriveInfo &drive : result) {
        drive.smartctlDiscovered = true;
    }


    for (const DriveInfo &linuxDrive :
         linuxDrives) {

        bool merged = false;


        // A single Linux device node may represent several
        // physical smartctl pass-through members.
        //
        // Do NOT stop after the first path match.
        for (DriveInfo &existing :
             result) {

            if (existing.name ==
                linuxDrive.name) {

                mergeLinuxMetadata(
                    existing,
                    linuxDrive
                );

                merged = true;
            }
        }


        if (merged)
            continue;


        // Preserve existing NVMe controller/namespace
        // enrichment behaviour, but also merge all matches.
        const QString controller =
            nvmeControllerPath(
                linuxDrive.name
            );

        if (!controller.isEmpty()) {
            for (DriveInfo &existing :
                 result) {

                if (existing.name ==
                    controller) {

                    mergeLinuxMetadata(
                        existing,
                        linuxDrive
                    );

                    merged = true;
                }
            }
        }


        if (!merged) {
            result.append(
                linuxDrive
            );
        }
    }


    // Defensive deduplication by the SAME central identity
    // used by the rest of LinDiskInfo.
    QVector<DriveInfo> unique;

    unique.reserve(
        result.size()
    );

    QSet<QString> seenIdentities;


    for (const DriveInfo &drive :
         result) {

        const QString identity =
            lindiskinfoDriveIdentity(
                drive
            );

        if (seenIdentities.contains(
                identity
            )) {

            continue;
        }

        seenIdentities.insert(
            identity
        );

        unique.append(
            drive
        );
    }


    result = unique;


    std::sort(
        result.begin(),
        result.end(),
        [](
            const DriveInfo &left,
            const DriveInfo &right
        )
        {
            const int leftRank =
                driveOrderRank(left);

            const int rightRank =
                driveOrderRank(right);

            if (leftRank != rightRank) {
                return leftRank <
                    rightRank;
            }

            return
                lindiskinfoDriveIdentity(
                    left
                ) <
                lindiskinfoDriveIdentity(
                    right
                );
        }
    );


    return result;
}

QJsonObject SmartctlBackend::enrichPayload(
    const QJsonObject &payload,
    const DriveInfo &drive
) const
{
    QJsonObject data = payload;

    if (!data.contains(
            QStringLiteral("model_name")
        ) &&
        !drive.infoName.isEmpty()) {

        data.insert(
            QStringLiteral("model_name"),
            drive.infoName
        );
    }

    if (!data.contains(
            QStringLiteral("serial_number")
        ) &&
        !drive.serial.isEmpty()) {

        data.insert(
            QStringLiteral("serial_number"),
            drive.serial
        );
    }

    if (!data.contains(
            QStringLiteral("firmware_version")
        ) &&
        !drive.revision.isEmpty()) {

        data.insert(
            QStringLiteral("firmware_version"),
            drive.revision
        );
    }

    if (!drive.vendor.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_vendor"
            ),
            drive.vendor
        );
    }

    data.insert(
        QStringLiteral(
            "lindiskinfo_read_only"
        ),
        drive.readOnly
    );

    if (drive.logicalSectorBytes > 0) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_logical_sector"
            ),
            QString::number(
                drive.logicalSectorBytes
            )
        );
    }

    if (drive.physicalSectorBytes > 0) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_physical_sector"
            ),
            QString::number(
                drive.physicalSectorBytes
            )
        );
    }

    if (drive.usbLinkMbps > 0) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_link_mbps"
            ),
            QString::number(
                drive.usbLinkMbps
            )
        );
    }

    if (!drive.usbSpecification.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_specification"
            ),
            drive.usbSpecification
        );
    }

    if (!drive.usbVendorId.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_vendor_id"
            ),
            drive.usbVendorId
        );
    }

    if (!drive.usbProductId.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_product_id"
            ),
            drive.usbProductId
        );
    }

    if (!drive.usbPortPath.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_port_path"
            ),
            drive.usbPortPath
        );
    }

    if (!drive.usbDriver.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_driver"
            ),
            drive.usbDriver
        );
    }

    if (!drive.usbProtocol.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_protocol"
            ),
            drive.usbProtocol
        );
    }

    if (!drive.usbPowerSource.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_power_source"
            ),
            drive.usbPowerSource
        );
    }

    if (!drive.usbMaxPower.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_max_power"
            ),
            drive.usbMaxPower
        );
    }

    if (!data.contains(
            QStringLiteral("user_capacity")
        ) &&
        drive.capacityBytes > 0) {

        QJsonObject capacity;

        capacity.insert(
            QStringLiteral("bytes"),
            QString::number(
                drive.capacityBytes
            )
        );

        data.insert(
            QStringLiteral("user_capacity"),
            capacity
        );
    }

    QJsonObject device =
        data.value(
            QStringLiteral("device")
        ).toObject();

    if (!device.contains(
            QStringLiteral("name")
        )) {

        device.insert(
            QStringLiteral("name"),
            drive.name
        );
    }

    if (!device.contains(
            QStringLiteral("protocol")
        ) &&
        !drive.protocol.isEmpty()) {

        device.insert(
            QStringLiteral("protocol"),
            drive.protocol
        );
    }

    data.insert(
        QStringLiteral("device"),
        device
    );

    data.insert(
        QStringLiteral(
            "lindiskinfo_smart_available"
        ),
        hasSmartData(data)
    );

    data.insert(
        QStringLiteral(
            "lindiskinfo_removable"
        ),
        drive.removable
    );

    if (!drive.transport.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_transport"
            ),
            drive.transport
        );
    }


    if (!drive.nvmeCurrentTransferMode.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_nvme_current_transfer_mode"
            ),
            drive.nvmeCurrentTransferMode
        );
    }

    if (!drive.nvmeMaxTransferMode.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_nvme_max_transfer_mode"
            ),
            drive.nvmeMaxTransferMode
        );
    }

return data;
}

QJsonObject SmartctlBackend::fallbackPayload(
    const DriveInfo &drive,
    const QString &error
) const
{
    QJsonObject data;

    QString model =
        drive.infoName;

    if (model.isEmpty())
        model = drive.model;

    if (!model.isEmpty()) {
        data.insert(
            QStringLiteral("model_name"),
            model
        );
    }

    if (drive.capacityBytes > 0) {
        QJsonObject capacity;

        capacity.insert(
            QStringLiteral("bytes"),
            QString::number(
                drive.capacityBytes
            )
        );

        data.insert(
            QStringLiteral("user_capacity"),
            capacity
        );
    }

    if (!drive.serial.isEmpty()) {
        data.insert(
            QStringLiteral("serial_number"),
            drive.serial
        );
    }

    if (!drive.revision.isEmpty()) {
        data.insert(
            QStringLiteral("firmware_version"),
            drive.revision
        );
    }

    if (!drive.vendor.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_vendor"
            ),
            drive.vendor
        );
    }

    data.insert(
        QStringLiteral(
            "lindiskinfo_read_only"
        ),
        drive.readOnly
    );

    if (drive.logicalSectorBytes > 0) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_logical_sector"
            ),
            QString::number(
                drive.logicalSectorBytes
            )
        );
    }

    if (drive.physicalSectorBytes > 0) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_physical_sector"
            ),
            QString::number(
                drive.physicalSectorBytes
            )
        );
    }

    if (drive.usbLinkMbps > 0) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_link_mbps"
            ),
            QString::number(
                drive.usbLinkMbps
            )
        );
    }

    if (!drive.usbSpecification.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_specification"
            ),
            drive.usbSpecification
        );
    }

    if (!drive.usbVendorId.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_vendor_id"
            ),
            drive.usbVendorId
        );
    }

    if (!drive.usbProductId.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_product_id"
            ),
            drive.usbProductId
        );
    }

    if (!drive.usbPortPath.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_port_path"
            ),
            drive.usbPortPath
        );
    }

    if (!drive.usbDriver.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_driver"
            ),
            drive.usbDriver
        );
    }

    if (!drive.usbProtocol.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_protocol"
            ),
            drive.usbProtocol
        );
    }

    if (!drive.usbPowerSource.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_power_source"
            ),
            drive.usbPowerSource
        );
    }

    if (!drive.usbMaxPower.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_usb_max_power"
            ),
            drive.usbMaxPower
        );
    }

    QJsonObject device;

    device.insert(
        QStringLiteral("name"),
        drive.name
    );

    if (!drive.protocol.isEmpty()) {
        device.insert(
            QStringLiteral("protocol"),
            drive.protocol
        );
    }

    data.insert(
        QStringLiteral("device"),
        device
    );

    data.insert(
        QStringLiteral(
            "lindiskinfo_smart_available"
        ),
        false
    );

    data.insert(
        QStringLiteral(
            "lindiskinfo_removable"
        ),
        drive.removable
    );

    if (!drive.transport.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_transport"
            ),
            drive.transport
        );
    }

    if (!error.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_smart_error"
            ),
            error
        );
    }


    if (!drive.nvmeCurrentTransferMode.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_nvme_current_transfer_mode"
            ),
            drive.nvmeCurrentTransferMode
        );
    }

    if (!drive.nvmeMaxTransferMode.isEmpty()) {
        data.insert(
            QStringLiteral(
                "lindiskinfo_nvme_max_transfer_mode"
            ),
            drive.nvmeMaxTransferMode
        );
    }

return data;
}

void SmartctlBackend::processHelperOutput()
{
    m_buffer +=
        m_helper->readAllStandardOutput();

    while (true) {
        const qsizetype newline =
            m_buffer.indexOf('\n');

        if (newline < 0)
            break;

        const QByteArray line =
            m_buffer.left(newline);

        m_buffer.remove(
            0,
            newline + 1
        );

        if (line.trimmed().isEmpty())
            continue;

        QJsonParseError parseError;

        const QJsonDocument document =
            QJsonDocument::fromJson(
                line,
                &parseError
            );

        if (parseError.error !=
                QJsonParseError::NoError ||
            !document.isObject()) {
            continue;
        }

        const QJsonObject response =
            document.object();

        const QString type =
            response.value(
                QStringLiteral("type")
            ).toString();

        const bool ok =
            response.value(
                QStringLiteral("ok")
            ).toBool();

        if (type == QStringLiteral("scan")) {
            if (!ok) {
                if (!m_linuxDrives.isEmpty()) {
                    emit scanFinished(
                        m_linuxDrives
                    );

                    continue;
                }

                emit scanFailed(
                    response.value(
                        QStringLiteral("error")
                    ).toString()
                );

                continue;
            }

            const QJsonObject payload =
                response.value(
                    QStringLiteral("payload")
                ).toObject();

            const QJsonArray devices =
                payload.value(
                    QStringLiteral("devices")
                ).toArray();

            QVector<DriveInfo> smartctlDrives;

            smartctlDrives.reserve(
                devices.size()
            );

            for (const QJsonValue &value :
                 devices) {

                const QJsonObject object =
                    value.toObject();

                DriveInfo drive;

                drive.name =
                    object.value(
                        QStringLiteral("name")
                    ).toString();

                if (ignoredDevicePath(drive.name))
                    continue;

                drive.infoName =
                    object.value(
                        QStringLiteral("info_name")
                    ).toString();

                drive.type =
                    object.value(
                        QStringLiteral("type")
                    ).toString();

                drive.protocol =
                    object.value(
                        QStringLiteral("protocol")
                    ).toString();

                drive.smartctlDiscovered = true;

                if (!drive.name.isEmpty())
                    smartctlDrives.append(drive);
            }

            emit scanFinished(
                mergeDriveLists(
                    smartctlDrives,
                    m_linuxDrives
                )
            );

            continue;
        }

        if (type == QStringLiteral("read")) {
            const QString name =
                response.value(
                    QStringLiteral("name")
                ).toString();

            DriveInfo drive;

            auto pending =
                m_pendingDrives.find(
                    name
                );

            if (pending !=
                    m_pendingDrives.end() &&
                !pending.value().isEmpty()) {

                drive =
                    pending.value()
                        .dequeue();

                if (pending.value().isEmpty()) {
                    m_pendingDrives.erase(
                        pending
                    );
                }
            }

            if (!ok) {
                if (!drive.name.isEmpty()) {
                    emit deviceDataReady(
                        drive,
                        fallbackPayload(
                            drive,
                            response.value(
                                QStringLiteral("error")
                            ).toString()
                        )
                    );

                    continue;
                }

                emit deviceDataFailed(
                    drive,
                    response.value(
                        QStringLiteral("error")
                    ).toString()
                );

                continue;
            }

            emit deviceDataReady(
                drive,
                enrichPayload(
                    response.value(
                        QStringLiteral("payload")
                    ).toObject(),
                    drive
                )
            );
        }
    }
}
