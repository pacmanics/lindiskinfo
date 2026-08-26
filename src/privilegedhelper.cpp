// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "privilegedhelper.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>

namespace
{

void sendResponse(
    QFile &output,
    const QJsonObject &response
)
{
    output.write(
        QJsonDocument(response)
            .toJson(
                QJsonDocument::Compact
            )
    );

    output.write("\n");
    output.flush();
}

bool validDeviceName(
    const QString &name
)
{
    if (!name.startsWith(
            QStringLiteral("/dev/")
        )) {
        return false;
    }

    if (name.contains(
            QStringLiteral("..")
        )) {
        return false;
    }

    return true;
}

bool validDeviceType(
    const QString &type
)
{
    if (type.isEmpty())
        return true;

    static const QRegularExpression expression(
        QStringLiteral(
            "^[A-Za-z0-9_,+.-]+$"
        )
    );

    return expression.match(type)
        .hasMatch();
}

bool validAamValue(
    const QString &value
)
{
    if (value.isEmpty() ||
        value == QStringLiteral("off")) {
        return true;
    }

    bool ok = false;

    const int level =
        value.toInt(&ok);

    return
        ok &&
        level >= 128 &&
        level <= 254;
}


bool validApmValue(
    const QString &value
)
{
    if (value.isEmpty() ||
        value == QStringLiteral("off")) {
        return true;
    }

    bool ok = false;

    const int level =
        value.toInt(&ok);

    return
        ok &&
        level >= 1 &&
        level <= 254;
}


QJsonObject runSmartctl(
    const QStringList &arguments
)
{
    QProcess process;

    process.start(
        QStringLiteral("/usr/bin/smartctl"),
        arguments
    );

    if (!process.waitForStarted(5000)) {
        return {
            {
                QStringLiteral("ok"),
                false
            },
            {
                QStringLiteral("error"),
                QStringLiteral(
                    "Unable to start smartctl."
                )
            }
        };
    }

    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();

        return {
            {
                QStringLiteral("ok"),
                false
            },
            {
                QStringLiteral("error"),
                QStringLiteral(
                    "smartctl timed out."
                )
            }
        };
    }

    const QByteArray stdoutData =
        process.readAllStandardOutput();

    const QByteArray stderrData =
        process.readAllStandardError();

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            stdoutData,
            &parseError
        );

    if (parseError.error !=
            QJsonParseError::NoError ||
        !document.isObject()) {

        QString error =
            QString::fromUtf8(
                stderrData
            ).trimmed();

        if (error.isEmpty()) {
            error =
                QStringLiteral(
                    "Invalid JSON returned by smartctl."
                );
        }

        return {
            {
                QStringLiteral("ok"),
                false
            },
            {
                QStringLiteral("error"),
                error
            }
        };
    }

    return {
        {
            QStringLiteral("ok"),
            true
        },
        {
            QStringLiteral(
                "process_exit_code"
            ),
            process.exitCode()
        },
        {
            QStringLiteral("payload"),
            document.object()
        }
    };
}

QString smartctlActionError(
    const QJsonObject &result
)
{
    if (!result.value(
            QStringLiteral("ok")
        ).toBool()) {

        return result.value(
            QStringLiteral("error")
        ).toString(
            QStringLiteral(
                "smartctl command failed."
            )
        );
    }

    const QJsonObject payload =
        result.value(
            QStringLiteral("payload")
        ).toObject();

    const QJsonArray messages =
        payload.value(
            QStringLiteral("smartctl")
        ).toObject()
        .value(
            QStringLiteral("messages")
        ).toArray();

    QStringList errors;

    for (const QJsonValue &value :
         messages) {

        if (value.isObject()) {
            const QJsonObject message =
                value.toObject();

            const QString severity =
                message.value(
                    QStringLiteral(
                        "severity"
                    )
                ).toString();

            const QString text =
                message.value(
                    QStringLiteral(
                        "string"
                    )
                ).toString();

            if (severity.compare(
                    QStringLiteral("error"),
                    Qt::CaseInsensitive
                ) == 0 &&
                !text.isEmpty()) {

                errors.append(text);
            }

        } else if (value.isString()) {
            const QString text =
                value.toString();

            if (!text.isEmpty())
                errors.append(text);
        }
    }

    return errors.join(
        QLatin1Char('\n')
    );
}


QString effectiveDeviceType(
    const QString &type
)
{
    return type;
}


QJsonObject runFullDeviceRead(
    const QString &name,
    const QString &type
)
{
    QStringList arguments;

    arguments
        << QStringLiteral("-x")
        << QStringLiteral("-j");

    const QString effectiveType =
        effectiveDeviceType(type);

    if (!effectiveType.isEmpty()) {
        arguments
            << QStringLiteral("-d")
            << effectiveType;
    }

    arguments << name;

    QJsonObject result =
        runSmartctl(arguments);

    if (!result.value(
            QStringLiteral("ok")
        ).toBool()) {
        return result;
    }

    QJsonObject payload =
        result.value(
            QStringLiteral("payload")
        ).toObject();

    const bool ataDevice =
        payload.contains(
            QStringLiteral(
                "ata_smart_attributes"
            )
        ) ||
        payload.contains(
            QStringLiteral(
                "ata_version"
            )
        ) ||
        payload.contains(
            QStringLiteral(
                "sata_version"
            )
        );

    if (!ataDevice)
        return result;

    QStringList settingArguments;

    settingArguments
        << QStringLiteral("-g")
        << QStringLiteral("aam")
        << QStringLiteral("-g")
        << QStringLiteral("apm")
        << QStringLiteral("-j");

    if (!effectiveType.isEmpty()) {
        settingArguments
            << QStringLiteral("-d")
            << effectiveType;
    }

    settingArguments << name;

    const QJsonObject settingResult =
        runSmartctl(
            settingArguments
        );

    if (settingResult.value(
            QStringLiteral("ok")
        ).toBool()) {

        const QJsonObject settings =
            settingResult.value(
                QStringLiteral("payload")
            ).toObject();

        if (settings.contains(
                QStringLiteral("ata_aam")
            )) {

            payload.insert(
                QStringLiteral("ata_aam"),
                settings.value(
                    QStringLiteral("ata_aam")
                )
            );
        }

        if (settings.contains(
                QStringLiteral("ata_apm")
            )) {

            payload.insert(
                QStringLiteral("ata_apm"),
                settings.value(
                    QStringLiteral("ata_apm")
                )
            );
        }

        result.insert(
            QStringLiteral("payload"),
            payload
        );
    }

    return result;
}


}

int runPrivilegedHelper()
{
    QFile input;
    QFile output;

    if (!input.open(
            stdin,
            QIODevice::ReadOnly
        )) {
        return 1;
    }

    if (!output.open(
            stdout,
            QIODevice::WriteOnly
        )) {
        return 1;
    }

    while (true) {
        const QByteArray line =
            input.readLine();

        if (line.isEmpty()) {
            if (input.atEnd())
                break;

            continue;
        }

        QJsonParseError parseError;

        const QJsonDocument document =
            QJsonDocument::fromJson(
                line,
                &parseError
            );

        if (parseError.error !=
                QJsonParseError::NoError ||
            !document.isObject()) {

            sendResponse(
                output,
                {
                    {
                        QStringLiteral("type"),
                        QStringLiteral("error")
                    },
                    {
                        QStringLiteral("ok"),
                        false
                    },
                    {
                        QStringLiteral("error"),
                        QStringLiteral(
                            "Invalid helper request."
                        )
                    }
                }
            );

            continue;
        }

        const QJsonObject request =
            document.object();

        const QString command =
            request.value(
                QStringLiteral("command")
            ).toString();

        if (command ==
            QStringLiteral("scan")) {

            QJsonObject result =
                runSmartctl(
                    {
                        QStringLiteral(
                            "--scan-open"
                        ),
                        QStringLiteral("-j")
                    }
                );

            result.insert(
                QStringLiteral("type"),
                QStringLiteral("scan")
            );

            sendResponse(
                output,
                result
            );

            continue;
        }

        if (command ==
            QStringLiteral("read")) {

            const QString name =
                request.value(
                    QStringLiteral("name")
                ).toString();

            const QString type =
                request.value(
                    QStringLiteral(
                        "device_type"
                    )
                ).toString();

            if (!validDeviceName(name) ||
                !validDeviceType(type)) {

                sendResponse(
                    output,
                    {
                        {
                            QStringLiteral("type"),
                            QStringLiteral("read")
                        },
                        {
                            QStringLiteral("name"),
                            name
                        },
                        {
                            QStringLiteral("ok"),
                            false
                        },
                        {
                            QStringLiteral("error"),
                            QStringLiteral(
                                "Invalid device request."
                            )
                        }
                    }
                );

                continue;
            }

            QJsonObject result =
                runFullDeviceRead(
                    name,
                    type
                );

            result.insert(
                QStringLiteral("type"),
                QStringLiteral("read")
            );

            result.insert(
                QStringLiteral("name"),
                name
            );

            sendResponse(
                output,
                result
            );

            continue;
        }


        if (command ==
            QStringLiteral("ata_set")) {

            const QString name =
                request.value(
                    QStringLiteral("name")
                ).toString();

            const QString type =
                request.value(
                    QStringLiteral(
                        "device_type"
                    )
                ).toString();

            const QString aam =
                request.value(
                    QStringLiteral("aam")
                ).toString();

            const QString apm =
                request.value(
                    QStringLiteral("apm")
                ).toString();

            if (!validDeviceName(name) ||
                !validDeviceType(type) ||
                !validAamValue(aam) ||
                !validApmValue(apm) ||
                (aam.isEmpty() &&
                 apm.isEmpty())) {

                sendResponse(
                    output,
                    {
                        {
                            QStringLiteral("type"),
                            QStringLiteral("read")
                        },
                        {
                            QStringLiteral("name"),
                            name
                        },
                        {
                            QStringLiteral("ok"),
                            false
                        },
                        {
                            QStringLiteral("error"),
                            QStringLiteral(
                                "Invalid ATA AAM/APM request."
                            )
                        }
                    }
                );

                continue;
            }

            QStringList arguments;

            arguments
                << QStringLiteral("-j");

            if (!aam.isEmpty()) {
                arguments
                    << QStringLiteral("-s")
                    << (
                        QStringLiteral("aam,") +
                        aam
                    );
            }

            if (!apm.isEmpty()) {
                arguments
                    << QStringLiteral("-s")
                    << (
                        QStringLiteral("apm,") +
                        apm
                    );
            }

            const QString effectiveType =
                effectiveDeviceType(type);

            if (!effectiveType.isEmpty()) {
                arguments
                    << QStringLiteral("-d")
                    << effectiveType;
            }

            arguments << name;

            const QJsonObject setResult =
                runSmartctl(arguments);

            const bool setOk =
                setResult.value(
                    QStringLiteral("ok")
                ).toBool() &&
                setResult.value(
                    QStringLiteral(
                        "process_exit_code"
                    )
                ).toInt(-1) == 0;

            if (!setOk) {
                QString error =
                    setResult.value(
                        QStringLiteral("error")
                    ).toString();

                if (error.isEmpty()) {
                    error =
                        QStringLiteral(
                            "smartctl rejected the requested AAM/APM setting."
                        );
                }

                sendResponse(
                    output,
                    {
                        {
                            QStringLiteral("type"),
                            QStringLiteral("read")
                        },
                        {
                            QStringLiteral("name"),
                            name
                        },
                        {
                            QStringLiteral("ok"),
                            false
                        },
                        {
                            QStringLiteral("error"),
                            error
                        }
                    }
                );

                continue;
            }

            QJsonObject result =
                runFullDeviceRead(
                    name,
                    type
                );

            result.insert(
                QStringLiteral("type"),
                QStringLiteral("read")
            );

            result.insert(
                QStringLiteral("name"),
                name
            );

            sendResponse(
                output,
                result
            );

            continue;
        }


        if (command ==
            QStringLiteral(
                "maintenance"
            )) {

            const QString name =
                request.value(
                    QStringLiteral("name")
                ).toString();

            const QString type =
                request.value(
                    QStringLiteral(
                        "device_type"
                    )
                ).toString();

            const QString operation =
                request.value(
                    QStringLiteral(
                        "operation"
                    )
                ).toString();

            if (!validDeviceName(name) ||
                !validDeviceType(type)) {

                sendResponse(
                    output,
                    {
                        {
                            QStringLiteral("type"),
                            QStringLiteral("read")
                        },
                        {
                            QStringLiteral("name"),
                            name
                        },
                        {
                            QStringLiteral("ok"),
                            false
                        },
                        {
                            QStringLiteral("error"),
                            QStringLiteral(
                                "Invalid maintenance request."
                            )
                        }
                    }
                );

                continue;
            }

            QStringList arguments;

            if (operation ==
                QStringLiteral(
                    "test_short"
                )) {

                arguments
                    << QStringLiteral("-t")
                    << QStringLiteral("short");

            } else if (
                operation ==
                QStringLiteral(
                    "test_long"
                )) {

                arguments
                    << QStringLiteral("-t")
                    << QStringLiteral("long");

            } else if (
                operation ==
                QStringLiteral(
                    "test_abort"
                )) {

                arguments
                    << QStringLiteral("-X");

            } else if (
                operation ==
                QStringLiteral(
                    "log_selftest"
                )) {

                arguments
                    << QStringLiteral("-l")
                    << QStringLiteral("selftest");

            } else if (
                operation ==
                QStringLiteral(
                    "log_error"
                )) {

                arguments
                    << QStringLiteral("-l")
                    << QStringLiteral("error");

            } else if (
                operation ==
                QStringLiteral(
                    "log_devstat"
                )) {

                arguments
                    << QStringLiteral("-l")
                    << QStringLiteral("devstat");

            } else if (
                operation ==
                QStringLiteral(
                    "log_sataphy"
                )) {

                arguments
                    << QStringLiteral("-l")
                    << QStringLiteral("sataphy");

            } else {

                sendResponse(
                    output,
                    {
                        {
                            QStringLiteral("type"),
                            QStringLiteral("read")
                        },
                        {
                            QStringLiteral("name"),
                            name
                        },
                        {
                            QStringLiteral("ok"),
                            false
                        },
                        {
                            QStringLiteral("error"),
                            QStringLiteral(
                                "Invalid maintenance operation."
                            )
                        }
                    }
                );

                continue;
            }

            arguments
                << QStringLiteral("-j");

            const QString effectiveType =
                effectiveDeviceType(type);

            if (!effectiveType.isEmpty()) {
                arguments
                    << QStringLiteral("-d")
                    << effectiveType;
            }

            arguments << name;

            const QJsonObject
                actionResult =
                    runSmartctl(
                        arguments
                    );

            const QString actionError =
                smartctlActionError(
                    actionResult
                );

            QJsonObject maintenance;

            maintenance.insert(
                QStringLiteral(
                    "operation"
                ),
                operation
            );

            maintenance.insert(
                QStringLiteral(
                    "success"
                ),
                actionError.isEmpty()
            );

            if (!actionError.isEmpty()) {
                maintenance.insert(
                    QStringLiteral(
                        "error"
                    ),
                    actionError
                );
            }

            if (actionResult.value(
                    QStringLiteral("ok")
                ).toBool()) {

                maintenance.insert(
                    QStringLiteral(
                        "result"
                    ),
                    actionResult.value(
                        QStringLiteral(
                            "payload"
                        )
                    ).toObject()
                );
            }

            // Always reread complete device state afterwards.
            // This updates current self-test state immediately.
            QJsonObject readResult =
                runFullDeviceRead(
                    name,
                    type
                );

            if (!readResult.value(
                    QStringLiteral("ok")
                ).toBool()) {

                sendResponse(
                    output,
                    {
                        {
                            QStringLiteral("type"),
                            QStringLiteral("read")
                        },
                        {
                            QStringLiteral("name"),
                            name
                        },
                        {
                            QStringLiteral("ok"),
                            false
                        },
                        {
                            QStringLiteral("error"),
                            readResult.value(
                                QStringLiteral(
                                    "error"
                                )
                            ).toString(
                                QStringLiteral(
                                    "Unable to reread device after maintenance command."
                                )
                            )
                        }
                    }
                );

                continue;
            }

            QJsonObject payload =
                readResult.value(
                    QStringLiteral(
                        "payload"
                    )
                ).toObject();

            payload.insert(
                QStringLiteral(
                    "lindiskinfo_maintenance"
                ),
                maintenance
            );

            readResult.insert(
                QStringLiteral(
                    "payload"
                ),
                payload
            );

            readResult.insert(
                QStringLiteral("type"),
                QStringLiteral("read")
            );

            readResult.insert(
                QStringLiteral("name"),
                name
            );

            sendResponse(
                output,
                readResult
            );

            continue;
        }


        sendResponse(
            output,
            {
                {
                    QStringLiteral("type"),
                    QStringLiteral("error")
                },
                {
                    QStringLiteral("ok"),
                    false
                },
                {
                    QStringLiteral("error"),
                    QStringLiteral(
                        "Unknown helper command."
                    )
                }
            }
        );
    }

    return 0;
}
