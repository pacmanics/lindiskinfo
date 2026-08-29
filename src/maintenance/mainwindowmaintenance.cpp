// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../theme/themebackgroundwidget.h"
#include "../theme/waifuthemes.h"
#include "../ui/responsivetablelayout.h"

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
#include <utility>

QString MainWindow::ataSettingsKey(
    const DriveInfo &drive,
    const QJsonObject &data
) const
{
    QString key =
        data.value(
            QStringLiteral(
                "serial_number"
            )
        ).toString()
        .trimmed();

    if (key.isEmpty()) {
        key =
            lindiskinfoDriveIdentity(
                drive
            );
    }

    key.replace(
        QLatin1Char('/'),
        QLatin1Char('_')
    );

    key.replace(
        QLatin1Char('\\'),
        QLatin1Char('_')
    );

    return key;
}

void MainWindow::maybeAutoAdjustAta(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    if (!m_aamApmAutoEnabled)
        return;

    const bool ataDevice =
        data.contains(
            QStringLiteral(
                "ata_smart_attributes"
            )
        ) ||
        data.contains(
            QStringLiteral("ata_aam")
        ) ||
        data.contains(
            QStringLiteral("ata_apm")
        );

    if (!ataDevice)
        return;

    const QString key =
        ataSettingsKey(
            drive,
            data
        );

    if (m_autoAdjustedAtaDrives
            .contains(key)) {
        return;
    }

    QSettings settings;

    settings.beginGroup(
        QStringLiteral(
            "ataAuto"
        )
    );

    settings.beginGroup(key);

    const QString aam =
        settings.value(
            QStringLiteral("aam")
        ).toString();

    const QString apm =
        settings.value(
            QStringLiteral("apm")
        ).toString();

    settings.endGroup();
    settings.endGroup();

    if (aam.isEmpty() &&
        apm.isEmpty()) {
        return;
    }

    const auto alreadyMatches =
        [&data](
            const char *jsonKey,
            const QString &wanted
        )
        {
            if (wanted.isEmpty())
                return true;

            const QJsonObject object =
                data.value(
                    QString::fromLatin1(
                        jsonKey
                    )
                ).toObject();

            if (object.isEmpty())
                return false;

            if (wanted ==
                QStringLiteral("off")) {

                return !object.value(
                    QStringLiteral(
                        "enabled"
                    )
                ).toBool(false);
            }

            bool ok = false;

            const int level =
                wanted.toInt(&ok);

            return
                ok &&
                object.value(
                    QStringLiteral(
                        "enabled"
                    )
                ).toBool(false) &&
                object.value(
                    QStringLiteral(
                        "level"
                    )
                ).toInt(-1) ==
                    level;
        };

    if (alreadyMatches(
            "ata_aam",
            aam
        ) &&
        alreadyMatches(
            "ata_apm",
            apm
        )) {

        m_autoAdjustedAtaDrives
            .insert(key);

        return;
    }

    m_autoAdjustedAtaDrives
        .insert(key);

    setStatus(
        tx(
            "Applying saved AAM/APM settings...",
            "Gespeicherte AAM/APM-Einstellungen werden angewendet..."
        )
    );

    m_backend->setAtaPowerSettings(
        drive,
        aam,
        apm
    );
}

void MainWindow::showMaintenanceResult(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    const QJsonObject maintenance =
        data.value(
            QStringLiteral(
                "lindiskinfo_maintenance"
            )
        ).toObject();

    if (maintenance.isEmpty())
        return;

    const QString operation =
        maintenance.value(
            QStringLiteral(
                "operation"
            )
        ).toString();

    const bool success =
        maintenance.value(
            QStringLiteral(
                "success"
            )
        ).toBool(false);

    const QString error =
        maintenance.value(
            QStringLiteral(
                "error"
            )
        ).toString();

    const QJsonObject result =
        maintenance.value(
            QStringLiteral(
                "result"
            )
        ).toObject();

    const auto titleForOperation =
        [this, &operation]()
        {
            if (operation ==
                QStringLiteral(
                    "test_short"
                )) {

                return tx(
                    "Short Self-Test",
                    "Kurzer Selbsttest"
                );
            }

            if (operation ==
                QStringLiteral(
                    "test_long"
                )) {

                return tx(
                    "Extended Self-Test",
                    "Erweiterter Selbsttest"
                );
            }

            if (operation ==
                QStringLiteral(
                    "test_abort"
                )) {

                return tx(
                    "Abort Self-Test",
                    "Selbsttest abbrechen"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_selftest"
                )) {

                return tx(
                    "Self-Test Status / Log",
                    "Selbsttest-Status / Protokoll"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_error"
                )) {

                return tx(
                    "SMART Error Log",
                    "SMART-Fehlerprotokoll"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_devstat"
                )) {

                return tx(
                    "ATA Device Statistics",
                    "ATA-Gerätestatistiken"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_sataphy"
                )) {

                return tx(
                    "SATA PHY Event Counters",
                    "SATA-PHY-Ereigniszähler"
                );
            }

            return tx(
                "Drive Maintenance",
                "Laufwerksdiagnose"
            );
        };

    const QString title =
        titleForOperation();

    QStringList messages;

    const QJsonArray smartctlMessages =
        result.value(
            QStringLiteral(
                "smartctl"
            )
        ).toObject()
        .value(
            QStringLiteral(
                "messages"
            )
        ).toArray();

    for (const QJsonValue &value :
         smartctlMessages) {

        QString text;

        if (value.isObject()) {
            text =
                value.toObject()
                    .value(
                        QStringLiteral(
                            "string"
                        )
                    ).toString();

        } else if (value.isString()) {
            text = value.toString();
        }

        if (!text.isEmpty())
            messages.append(text);
    }

    const bool commandOperation =
        operation ==
            QStringLiteral(
                "test_short"
            ) ||
        operation ==
            QStringLiteral(
                "test_long"
            ) ||
        operation ==
            QStringLiteral(
                "test_abort"
            );

    const QByteArray prettyJson =
        QJsonDocument(result)
            .toJson(
                QJsonDocument::Indented
            );

    if (commandOperation) {
        QMessageBox box(this);

        box.setWindowTitle(
            QStringLiteral(
                "LinDiskInfo - "
            ) + title
        );

        box.setIcon(
            success
                ? QMessageBox::Information
                : QMessageBox::Warning
        );

        if (success) {
            if (operation ==
                QStringLiteral(
                    "test_abort"
                )) {

                box.setText(
                    tx(
                        "The self-test abort command was accepted.",
                        "Der Befehl zum Abbrechen des Selbsttests wurde akzeptiert."
                    )
                );

            } else {
                box.setText(
                    tx(
                        "The drive accepted the self-test command.",
                        "Das Laufwerk hat den Selbsttest-Befehl akzeptiert."
                    )
                );
            }

        } else {
            box.setText(
                tx(
                    "The drive rejected the requested operation or does not support it.",
                    "Das Laufwerk hat die angeforderte Funktion abgelehnt oder unterstützt sie nicht."
                )
            );
        }

        QString info;

        if (!messages.isEmpty()) {
            info =
                messages.join(
                    QLatin1Char('\n')
                );
        }

        if (!error.isEmpty()) {
            if (!info.isEmpty())
                info += QStringLiteral("\n\n");

            info += error;
        }

        if (!info.isEmpty()) {
            box.setInformativeText(
                info
            );
        }

        if (!prettyJson.isEmpty()) {
            box.setDetailedText(
                QString::fromUtf8(
                    prettyJson
                )
            );
        }

        box.exec();

        if (success) {
            setStatus(
                tx(
                    "Drive maintenance command completed.",
                    "Laufwerksdiagnose-Befehl ausgeführt."
                )
            );
        }

        return;
    }


    QDialog dialog(this);

    dialog.setWindowTitle(
        QStringLiteral(
            "LinDiskInfo - "
        ) + title
    );

    dialog.resize(
        900,
        650
    );

    auto *layout =
        new QVBoxLayout(
            &dialog
        );

    QString heading =
        QStringLiteral("<b>") +
        title +
        QStringLiteral("</b><br>") +
        drive.name;

    auto *label =
        new QLabel(
            heading,
            &dialog
        );

    label->setTextFormat(
        Qt::RichText
    );

    layout->addWidget(label);

    if (!success) {
        auto *errorLabel =
            new QLabel(
                error.isEmpty()
                    ? tx(
                          "The requested log is not supported by this drive.",
                          "Das angeforderte Protokoll wird von diesem Laufwerk nicht unterstützt."
                      )
                    : error,
                &dialog
            );

        errorLabel->setWordWrap(true);

        layout->addWidget(
            errorLabel
        );
    }

    auto *text =
        new QPlainTextEdit(
            &dialog
        );

    text->setReadOnly(true);

    text->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
        )
    );

    if (!prettyJson.isEmpty()) {
        text->setPlainText(
            QString::fromUtf8(
                prettyJson
            )
        );

    } else if (!error.isEmpty()) {
        text->setPlainText(error);

    } else {
        text->setPlainText(
            tx(
                "No log data returned.",
                "Keine Protokolldaten zurückgegeben."
            )
        );
    }

    layout->addWidget(
        text,
        1
    );

    auto *buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Close,
            &dialog
        );

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    layout->addWidget(buttons);

    dialog.exec();
}
