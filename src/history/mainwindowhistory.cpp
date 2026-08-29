// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "lindiskhistoryplot.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

QString MainWindow::historyDeviceKey(
    const DriveInfo &drive,
    const QJsonObject &data
) const
{
    QString rawKey =
        data.value(
            QStringLiteral(
                "serial_number"
            )
        ).toString()
        .trimmed();


    if (rawKey.isEmpty()) {
        rawKey =
            data.value(
                QStringLiteral(
                    "model_name"
                )
            ).toString()
            .trimmed();


        if (rawKey.isEmpty()) {
            rawKey =
                data.value(
                    QStringLiteral(
                        "model_number"
                    )
                ).toString()
                .trimmed();
        }


        if (!rawKey.isEmpty()) {
            rawKey +=
                QStringLiteral("|") +
                lindiskinfoDriveIdentity(
                    drive
                );
        }
    }


    if (rawKey.isEmpty()) {
        rawKey =
            lindiskinfoDriveIdentity(
                drive
            );
    }


    const QByteArray digest =
        QCryptographicHash::hash(
            rawKey.toUtf8(),
            QCryptographicHash::Sha256
        ).toHex();


    return
        QStringLiteral("sha256:") +
        QString::fromLatin1(
            digest.constData(),
            digest.size()
        );
}

void MainWindow::loadHistory()
{
    m_historySamples =
        QJsonArray();


    const QString directory =
        QStandardPaths::
            writableLocation(
                QStandardPaths::
                    AppDataLocation
            );


    if (directory.isEmpty())
        return;


    QFile file(
        directory +
        QStringLiteral(
            "/history.json"
        )
    );


    if (!file.open(
            QIODevice::ReadOnly
        )) {

        return;
    }


    QJsonParseError error;


    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &error
        );


    if (error.error !=
            QJsonParseError::NoError ||
        !document.isArray()) {

        return;
    }


    const QJsonArray loaded =
        document.array();


    const qint64 cutoff =
        QDateTime::
            currentMSecsSinceEpoch() -
        90LL *
        24LL *
        60LL *
        60LL *
        1000LL;


    QJsonArray retained;

    bool changed = false;


    for (const QJsonValue &value :
         loaded) {

        QJsonObject sample =
            value.toObject();


        const qint64 timestamp =
            sample.value(
                QStringLiteral(
                    "timestamp"
                )
            ).toString()
            .toLongLong();


        if (timestamp < cutoff) {
            changed = true;
            continue;
        }


        const QString oldKey =
            sample.value(
                QStringLiteral(
                    "device_key"
                )
            ).toString()
            .trimmed();


        if (!oldKey.isEmpty() &&
            !oldKey.startsWith(
                QStringLiteral(
                    "sha256:"
                )
            )) {

            const QByteArray digest =
                QCryptographicHash::hash(
                    oldKey.toUtf8(),
                    QCryptographicHash::Sha256
                ).toHex();


            sample.insert(
                QStringLiteral(
                    "device_key"
                ),
                QStringLiteral(
                    "sha256:"
                ) +
                QString::fromLatin1(
                    digest.constData(),
                    digest.size()
                )
            );


            changed = true;
        }


        retained.append(
            sample
        );
    }


    m_historySamples =
        retained;


    if (changed)
        saveHistory();
}

void MainWindow::saveHistory() const
{
    const QString directory =
        QStandardPaths::
            writableLocation(
                QStandardPaths::
                    AppDataLocation
            );

    if (directory.isEmpty())
        return;

    if (!QDir().mkpath(directory))
        return;

    QSaveFile file(
        directory +
        QStringLiteral(
            "/history.json"
        )
    );

    if (!file.open(
            QIODevice::WriteOnly
        )) {
        return;
    }

    const QJsonDocument document(
        m_historySamples
    );

    file.write(
        document.toJson(
            QJsonDocument::Compact
        )
    );

    file.commit();
}

void MainWindow::recordHistorySample(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    if (!m_historyEnabled)
        return;

    const qint64 now =
        QDateTime::
            currentMSecsSinceEpoch();

    const QString key =
        historyDeviceKey(
            drive,
            data
        );

    const qint64 previous =
        m_lastHistorySampleMs
            .value(
                key,
                0
            );

    // Do not turn normal fast refreshes into
    // thousands of history points.
    if (previous > 0 &&
        now - previous <
            60LL * 1000LL) {

        return;
    }

    m_lastHistorySampleMs.insert(
        key,
        now
    );

    QJsonObject sample;

    sample.insert(
        QStringLiteral("timestamp"),
        QString::number(now)
    );

    sample.insert(
        QStringLiteral("device_key"),
        key
    );

    sample.insert(
        QStringLiteral("device"),
        drive.name
    );

    QString model =
        data.value(
            QStringLiteral(
                "model_name"
            )
        ).toString(
            data.value(
                QStringLiteral(
                    "model_number"
                )
            ).toString()
        );

    if (!model.isEmpty()) {
        sample.insert(
            QStringLiteral("model"),
            model
        );
    }

    const int temperature =
        temperatureForData(data);

    if (temperature > 0) {
        sample.insert(
            QStringLiteral(
                "temperature_c"
            ),
            temperature
        );
    }

    int percentage = -1;

    const HealthState health =
        healthStateForData(
            data,
            &percentage
        );

    int healthValue = percentage;

    if (healthValue < 0) {
        switch (health) {
        case HealthState::Good:
            healthValue = 100;
            break;

        case HealthState::Caution:
            healthValue = 50;
            break;

        case HealthState::Bad:
            healthValue = 0;
            break;

        case HealthState::Unknown:
            healthValue = -1;
            break;
        }
    }

    if (healthValue >= 0) {
        sample.insert(
            QStringLiteral(
                "health_percent"
            ),
            healthValue
        );
    }

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {

        const QJsonObject nvme =
            data.value(
                QStringLiteral(
                    "nvme_smart_health_information_log"
                )
            ).toObject();

        const quint64 unitsRead =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "data_units_read"
                    )
                )
            );

        const quint64 unitsWritten =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "data_units_written"
                    )
                )
            );

        const quint64 maxUnits =
            std::numeric_limits<
                quint64
            >::max() /
            512000ULL;

        if (unitsRead <= maxUnits) {
            sample.insert(
                QStringLiteral(
                    "read_bytes"
                ),
                QString::number(
                    unitsRead *
                    512000ULL
                )
            );
        }

        if (unitsWritten <= maxUnits) {
            sample.insert(
                QStringLiteral(
                    "write_bytes"
                ),
                QString::number(
                    unitsWritten *
                    512000ULL
                )
            );
        }
    }

    m_historySamples.append(
        sample
    );

    const qint64 cutoff =
        now -
        90LL *
        24LL *
        60LL *
        60LL *
        1000LL;

    QJsonArray retained;

    const qsizetype startIndex =
        m_historySamples.size() >
            50000
            ? m_historySamples.size() -
                50000
            : 0;

    for (qsizetype i = startIndex;
         i < m_historySamples.size();
         ++i) {

        const QJsonObject entry =
            m_historySamples.at(i)
                .toObject();

        const qint64 timestamp =
            entry.value(
                QStringLiteral(
                    "timestamp"
                )
            ).toString()
            .toLongLong();

        if (timestamp >= cutoff)
            retained.append(entry);
    }

    m_historySamples = retained;

    saveHistory();
}

void MainWindow::openHistoryGraph()
{
    if (!m_hasCurrentData) {
        QMessageBox::information(
            this,
            QStringLiteral(
                "LinDiskInfo"
            ),
            tx(
                "No drive is currently selected.",
                "Derzeit ist kein Laufwerk ausgewählt."
            )
        );

        return;
    }

    const QString key =
        historyDeviceKey(
            m_currentDrive,
            m_currentData
        );

    QDialog dialog(this);

    dialog.setWindowTitle(
        tx(
            "Graph / History",
            "Diagramm / Verlauf"
        )
    );

    dialog.resize(
        980,
        650
    );

    auto *layout =
        new QVBoxLayout(
            &dialog
        );

    auto *heading =
        new QLabel(
            m_titleLabel->text(),
            &dialog
        );

    QFont headingFont =
        heading->font();

    headingFont.setBold(true);

    heading->setFont(
        headingFont
    );

    layout->addWidget(
        heading
    );

    auto *controls =
        new QHBoxLayout;

    auto *metric =
        new QComboBox(
            &dialog
        );

    metric->addItem(
        tx(
            "Temperature",
            "Temperatur"
        ),
        QStringLiteral(
            "temperature"
        )
    );

    metric->addItem(
        tx(
            "Health / Wear",
            "Zustand / Verschleiß"
        ),
        QStringLiteral(
            "health"
        )
    );

    metric->addItem(
        tx(
            "Total Reads",
            "Gesamt gelesen"
        ),
        QStringLiteral(
            "reads"
        )
    );

    metric->addItem(
        tx(
            "Total Writes",
            "Gesamt geschrieben"
        ),
        QStringLiteral(
            "writes"
        )
    );

    auto *range =
        new QComboBox(
            &dialog
        );

    const struct
    {
        const char *en;
        const char *de;
        qint64 seconds;
    }
    ranges[] =
    {
        {
            "1 hour",
            "1 Stunde",
            3600
        },
        {
            "6 hours",
            "6 Stunden",
            21600
        },
        {
            "24 hours",
            "24 Stunden",
            86400
        },
        {
            "7 days",
            "7 Tage",
            604800
        },
        {
            "30 days",
            "30 Tage",
            2592000
        },
        {
            "90 days",
            "90 Tage",
            7776000
        }
    };

    for (const auto &item : ranges) {
        range->addItem(
            tx(
                item.en,
                item.de
            ),
            QVariant::fromValue<
                qlonglong
            >(
                item.seconds
            )
        );
    }

    range->setCurrentIndex(2);

    auto *recording =
        new QCheckBox(
            tx(
                "Record history",
                "Verlauf aufzeichnen"
            ),
            &dialog
        );

    recording->setChecked(
        m_historyEnabled
    );

    auto *clearButton =
        new QPushButton(
            tx(
                "Clear drive history",
                "Laufwerksverlauf löschen"
            ),
            &dialog
        );

    controls->addWidget(
        metric
    );

    controls->addWidget(
        range
    );

    controls->addSpacing(12);

    controls->addWidget(
        recording
    );

    controls->addStretch(1);

    controls->addWidget(
        clearButton
    );

    layout->addLayout(
        controls
    );

    auto *summary =
        new QLabel(
            &dialog
        );

    layout->addWidget(
        summary
    );

    auto *graph =
        new LinDiskHistoryPlot(
            &dialog
        );

    layout->addWidget(
        graph,
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

    layout->addWidget(
        buttons
    );


    const auto updateGraph =
        [this,
         key,
         metric,
         range,
         graph,
         summary]
        {
            const QString mode =
                metric->currentData()
                    .toString();

            const qint64 seconds =
                range->currentData()
                    .toLongLong();

            const qint64 now =
                QDateTime::
                    currentMSecsSinceEpoch();

            const qint64 cutoff =
                now -
                seconds *
                1000LL;

            QVector<QPointF> points;

            QString unit;

            bool percentageRange = false;

            long double byteDivisor =
                1000000000.0L;

            const QString storageUnit =
                QSettings().value(
                    QStringLiteral(
                        "storageUnit"
                    ),
                    QStringLiteral("GB")
                ).toString();

            if (storageUnit ==
                QStringLiteral("GiB")) {

                byteDivisor =
                    1073741824.0L;

            } else if (
                storageUnit ==
                QStringLiteral("TB")
            ) {

                byteDivisor =
                    1000000000000.0L;

            } else if (
                storageUnit ==
                QStringLiteral("TiB")
            ) {

                byteDivisor =
                    1099511627776.0L;
            }

            for (const QJsonValue &value :
                 m_historySamples) {

                const QJsonObject sample =
                    value.toObject();

                if (sample.value(
                        QStringLiteral(
                            "device_key"
                        )
                    ).toString() != key) {

                    continue;
                }

                const qint64 timestamp =
                    sample.value(
                        QStringLiteral(
                            "timestamp"
                        )
                    ).toString()
                    .toLongLong();

                if (timestamp < cutoff)
                    continue;

                double y = 0.0;

                bool available = false;

                if (mode ==
                    QStringLiteral(
                        "temperature"
                    )) {

                    if (sample.contains(
                            QStringLiteral(
                                "temperature_c"
                            )
                        )) {

                        double temperature =
                            sample.value(
                                QStringLiteral(
                                    "temperature_c"
                                )
                            ).toDouble();

                        if (m_temperatureUnit ==
                            TemperatureUnit::
                                Fahrenheit) {

                            temperature =
                                temperature *
                                9.0 / 5.0 +
                                32.0;

                            unit =
                                QStringLiteral(
                                    "°F"
                                );

                        } else {
                            unit =
                                QStringLiteral(
                                    "°C"
                                );
                        }

                        y = temperature;
                        available = true;
                    }

                } else if (
                    mode ==
                    QStringLiteral(
                        "health"
                    )) {

                    if (sample.contains(
                            QStringLiteral(
                                "health_percent"
                            )
                        )) {

                        y =
                            sample.value(
                                QStringLiteral(
                                    "health_percent"
                                )
                            ).toDouble();

                        unit =
                            QStringLiteral("%");

                        percentageRange = true;
                        available = true;
                    }

                } else if (
                    mode ==
                    QStringLiteral(
                        "reads"
                    ) ||
                    mode ==
                    QStringLiteral(
                        "writes"
                    )) {

                    const QString field =
                        mode ==
                        QStringLiteral(
                            "reads"
                        )
                            ? QStringLiteral(
                                  "read_bytes"
                              )
                            : QStringLiteral(
                                  "write_bytes"
                              );

                    if (sample.contains(
                            field
                        )) {

                        bool ok = false;

                        const quint64 bytes =
                            sample.value(field)
                                .toString()
                                .toULongLong(
                                    &ok
                                );

                        if (ok) {
                            y =
                                static_cast<
                                    double
                                >(
                                    static_cast<
                                        long double
                                    >(bytes) /
                                    byteDivisor
                                );

                            unit =
                                storageUnit;

                            available = true;
                        }
                    }
                }

                if (available) {
                    points.append(
                        QPointF(
                            static_cast<
                                double
                            >(timestamp),
                            y
                        )
                    );
                }
            }

            std::sort(
                points.begin(),
                points.end(),
                [](
                    const QPointF &a,
                    const QPointF &b
                )
                {
                    return a.x() <
                        b.x();
                }
            );

            graph->setSamples(
                points,
                unit,
                tx(
                    "No history data is available for this metric and time range yet.",
                    "Für diese Messgröße und diesen Zeitraum liegen noch keine Verlaufsdaten vor."
                ),
                percentageRange
            );

            summary->setText(
                tx(
                    "%1 samples in selected range",
                    "%1 Messpunkte im gewählten Zeitraum"
                ).arg(
                    points.size()
                )
            );
        };


    connect(
        metric,
        &QComboBox::currentTextChanged,
        &dialog,
        [updateGraph](const QString &)
        {
            updateGraph();
        }
    );

    connect(
        range,
        &QComboBox::currentTextChanged,
        &dialog,
        [updateGraph](const QString &)
        {
            updateGraph();
        }
    );

    connect(
        recording,
        &QCheckBox::toggled,
        &dialog,
        [this](bool enabled)
        {
            m_historyEnabled =
                enabled;

            QSettings().setValue(
                QStringLiteral(
                    "historyEnabled"
                ),
                enabled
            );
        }
    );

    connect(
        clearButton,
        &QPushButton::clicked,
        &dialog,
        [this,
         key,
         updateGraph]
        {
            const auto answer =
                QMessageBox::question(
                    this,
                    QStringLiteral(
                        "LinDiskInfo"
                    ),
                    tx(
                        "Delete the stored history for the selected drive?",
                        "Gespeicherten Verlauf des ausgewählten Laufwerks löschen?"
                    ),
                    QMessageBox::Yes |
                    QMessageBox::No,
                    QMessageBox::No
                );

            if (answer !=
                QMessageBox::Yes) {

                return;
            }

            QJsonArray retained;

            for (const QJsonValue &value :
                 m_historySamples) {

                const QJsonObject sample =
                    value.toObject();

                if (sample.value(
                        QStringLiteral(
                            "device_key"
                        )
                    ).toString() != key) {

                    retained.append(
                        sample
                    );
                }
            }

            m_historySamples =
                retained;

            m_lastHistorySampleMs
                .remove(key);

            saveHistory();
            updateGraph();
        }
    );

    updateGraph();

    dialog.exec();
}
