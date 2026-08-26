// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QString>
#include <QVector>

struct DriveInfo
{
    QString name;
    QString infoName;
    QString type;
    QString protocol;

    QString model;
    QString vendor;
    QString transport;

    QString serial;
    QString revision;

    QString usbSpecification;
    QString usbVendorId;
    QString usbProductId;
    QString usbPortPath;
    QString usbDriver;
    QString usbProtocol;
    QString usbPowerSource;
    QString usbMaxPower;

    QString nvmeCurrentTransferMode;
    QString nvmeMaxTransferMode;

    quint64 capacityBytes = 0;
    quint64 logicalSectorBytes = 0;
    quint64 physicalSectorBytes = 0;
    quint64 usbLinkMbps = 0;

    bool removable = false;
    bool readOnly = false;
    bool smartctlDiscovered = false;
};


inline QString lindiskinfoDriveIdentity(
    const DriveInfo &drive
)
{
    const QString name =
        drive.name.trimmed();

    const QString type =
        drive.type
            .trimmed()
            .toLower();

    return type.isEmpty()
        ? name
        : (
            name +
            QStringLiteral("||") +
            type
        );
}


Q_DECLARE_METATYPE(DriveInfo)
Q_DECLARE_METATYPE(QVector<DriveInfo>)

class SmartctlBackend : public QObject
{
    Q_OBJECT

public:
    explicit SmartctlBackend(QObject *parent = nullptr);
    ~SmartctlBackend() override;

    void start();
    void scanDevices();
    void requestDeviceData(const DriveInfo &drive);

    void setAtaPowerSettings(
        const DriveInfo &drive,
        const QString &aamValue,
        const QString &apmValue
    );

    void requestMaintenance(
        const DriveInfo &drive,
        const QString &operation
    );

signals:
    void helperReady();
    void helperFailed(const QString &message);

    void scanFinished(const QVector<DriveInfo> &drives);
    void scanFailed(const QString &message);

    void deviceDataReady(
        const DriveInfo &drive,
        const QJsonObject &data
    );

    void deviceDataFailed(
        const DriveInfo &drive,
        const QString &message
    );

private:
    void sendRequest(const QJsonObject &request);
    void processHelperOutput();

    QVector<DriveInfo> scanLinuxBlockDevices() const;

    QVector<DriveInfo> mergeDriveLists(
        const QVector<DriveInfo> &smartctlDrives,
        const QVector<DriveInfo> &linuxDrives
    ) const;

    QJsonObject enrichPayload(
        const QJsonObject &payload,
        const DriveInfo &drive
    ) const;

    QJsonObject fallbackPayload(
        const DriveInfo &drive,
        const QString &error = QString()
    ) const;

    QProcess *m_helper = nullptr;
    QByteArray m_buffer;

    QVector<DriveInfo> m_linuxDrives;

    QHash<
        QString,
        QQueue<DriveInfo>
    > m_pendingDrives;

    bool m_stopping = false;
};
