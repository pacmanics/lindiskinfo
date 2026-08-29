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

void MainWindow::setupAdvancedHealthAndValueActions()
{
    QSettings settings;

    const auto actionByName =
        [this](const char *name) -> QAction *
        {
            return findChild<QAction *>(
                QString::fromLatin1(name)
            );
        };

// ====================================================
        // Health Status Settings
        // ====================================================

        if (QAction *healthSettings =
                actionByName(
                    "stateSettingsAction"
                )) {

            healthSettings->setVisible(true);

            connect(
                healthSettings,
                &QAction::triggered,
                this,
                [this]
                {
                    QDialog dialog(this);

                    dialog.setWindowTitle(
                        tx(
                            "Health Status Settings",
                            "Zustandseinstellungen"
                        )
                    );

                    auto *form =
                        new QFormLayout(&dialog);

                    auto *nvmeCaution =
                        new QSpinBox(&dialog);

                    auto *nvmeBad =
                        new QSpinBox(&dialog);

                    auto *ataSector =
                        new QSpinBox(&dialog);

                    nvmeCaution->setRange(0, 100);
                    nvmeBad->setRange(0, 100);

                    ataSector->setRange(
                        1,
                        1000000000
                    );

                    nvmeCaution->setSuffix(
                        QStringLiteral(" %")
                    );

                    nvmeBad->setSuffix(
                        QStringLiteral(" %")
                    );

                    nvmeCaution->setValue(
                        m_nvmeCautionRemaining
                    );

                    nvmeBad->setValue(
                        m_nvmeBadRemaining
                    );

                    ataSector->setValue(
                        static_cast<int>(
                            std::min<quint64>(
                                m_ataSectorCautionCount,
                                1000000000ULL
                            )
                        )
                    );

                    form->addRow(
                        tx(
                            "SSD/NVMe caution at remaining life:",
                            "SSD/NVMe-Warnung ab Restlebensdauer:"
                        ),
                        nvmeCaution
                    );

                    form->addRow(
                        tx(
                            "SSD/NVMe bad at remaining life:",
                            "SSD/NVMe: Schlecht ab Restlebensdauer:"
                        ),
                        nvmeBad
                    );

                    form->addRow(
                        tx(
                            "ATA sector warning count:",
                            "ATA-Sektorwarnung ab Anzahl:"
                        ),
                        ataSector
                    );

                    auto *buttons =
                        new QDialogButtonBox(
                            QDialogButtonBox::Ok |
                            QDialogButtonBox::Cancel,
                            &dialog
                        );

                    form->addRow(buttons);

                    connect(
                        buttons,
                        &QDialogButtonBox::accepted,
                        &dialog,
                        &QDialog::accept
                    );

                    connect(
                        buttons,
                        &QDialogButtonBox::rejected,
                        &dialog,
                        &QDialog::reject
                    );

                    if (dialog.exec() !=
                        QDialog::Accepted) {
                        return;
                    }

                    m_nvmeCautionRemaining =
                        nvmeCaution->value();

                    m_nvmeBadRemaining =
                        std::min(
                            nvmeBad->value(),
                            m_nvmeCautionRemaining
                        );

                    m_ataSectorCautionCount =
                        static_cast<quint64>(
                            ataSector->value()
                        );

                    QSettings settings;

                    settings.setValue(
                        QStringLiteral(
                            "nvmeCautionRemaining"
                        ),
                        m_nvmeCautionRemaining
                    );

                    settings.setValue(
                        QStringLiteral(
                            "nvmeBadRemaining"
                        ),
                        m_nvmeBadRemaining
                    );

                    settings.setValue(
                        QStringLiteral(
                            "ataSectorCautionCount"
                        ),
                        QVariant::fromValue<
                            qulonglong
                        >(
                            m_ataSectorCautionCount
                        )
                    );

                    refreshDrivePresentation();
                }
            );
        }


        // ====================================================
        // Temperature warning thresholds
        // ====================================================

        if (QAction *temperatureSettings =
                actionByName(
                    "temperatureWarningAction"
                )) {

            temperatureSettings->setVisible(true);

            connect(
                temperatureSettings,
                &QAction::triggered,
                this,
                [this]
                {
                    QDialog dialog(this);

                    dialog.setWindowTitle(
                        tx(
                            "Temperature Warning Settings",
                            "Temperaturwarneinstellungen"
                        )
                    );

                    auto *form =
                        new QFormLayout(&dialog);

                    auto *caution =
                        new QSpinBox(&dialog);

                    auto *bad =
                        new QSpinBox(&dialog);

                    const bool fahrenheit =
                        m_temperatureUnit ==
                        TemperatureUnit::Fahrenheit;

                    const auto toDisplay =
                        [fahrenheit](int celsius)
                        {
                            if (!fahrenheit)
                                return celsius;

                            return qRound(
                                celsius *
                                9.0 / 5.0 +
                                32.0
                            );
                        };

                    const auto toCelsius =
                        [fahrenheit](int value)
                        {
                            if (!fahrenheit)
                                return value;

                            return qRound(
                                (value - 32.0) *
                                5.0 / 9.0
                            );
                        };

                    if (fahrenheit) {
                        caution->setRange(34, 248);
                        bad->setRange(34, 248);

                        caution->setSuffix(
                            QStringLiteral(" °F")
                        );

                        bad->setSuffix(
                            QStringLiteral(" °F")
                        );
                    } else {
                        caution->setRange(1, 120);
                        bad->setRange(1, 120);

                        caution->setSuffix(
                            QStringLiteral(" °C")
                        );

                        bad->setSuffix(
                            QStringLiteral(" °C")
                        );
                    }

                    caution->setValue(
                        toDisplay(
                            m_temperatureCaution
                        )
                    );

                    bad->setValue(
                        toDisplay(
                            m_temperatureBad
                        )
                    );

                    form->addRow(
                        tx(
                            "Caution:",
                            "Vorsicht:"
                        ),
                        caution
                    );

                    form->addRow(
                        tx(
                            "Bad:",
                            "Schlecht:"
                        ),
                        bad
                    );

                    auto *buttons =
                        new QDialogButtonBox(
                            QDialogButtonBox::Ok |
                            QDialogButtonBox::Cancel,
                            &dialog
                        );

                    form->addRow(buttons);

                    connect(
                        buttons,
                        &QDialogButtonBox::accepted,
                        &dialog,
                        &QDialog::accept
                    );

                    connect(
                        buttons,
                        &QDialogButtonBox::rejected,
                        &dialog,
                        &QDialog::reject
                    );

                    if (dialog.exec() !=
                        QDialog::Accepted) {
                        return;
                    }

                    int cautionC =
                        toCelsius(
                            caution->value()
                        );

                    int badC =
                        toCelsius(
                            bad->value()
                        );

                    if (badC <= cautionC)
                        badC = cautionC + 1;

                    m_temperatureCaution =
                        cautionC;

                    m_temperatureBad =
                        badC;

                    QSettings settings;

                    settings.setValue(
                        QStringLiteral(
                            "temperatureCaution"
                        ),
                        m_temperatureCaution
                    );

                    settings.setValue(
                        QStringLiteral(
                            "temperatureBad"
                        ),
                        m_temperatureBad
                    );

                    refreshDrivePresentation();
                }
            );
        }


        // ====================================================
        // Raw Value Modes
        // ====================================================

        {
            auto *rawGroup =
                new QActionGroup(this);

            rawGroup->setExclusive(true);

            const QString savedMode =
                settings.value(
                    QStringLiteral(
                        "rawValueMode"
                    ),
                    QStringLiteral("hex")
                ).toString();

            QList<QAction *> rawActions;

            for (QAction *entry :
                 m_rawValuesMenu->actions()) {

                if (!entry ||
                    entry->isSeparator() ||
                    entry == m_showRawAction) {
                    continue;
                }

                rawActions.append(entry);
            }

            const QStringList modes =
            {
                QStringLiteral("hex"),
                QStringLiteral("dec"),
                QStringLiteral("dec2"),
                QStringLiteral("dec1")
            };

            for (int i = 0;
                 i < rawActions.size() &&
                 i < modes.size();
                 ++i) {

                QAction *entry =
                    rawActions.at(i);

                const QString mode =
                    modes.at(i);

                entry->setVisible(true);
                entry->setCheckable(true);
                entry->setData(mode);

                rawGroup->addAction(entry);

                entry->setChecked(
                    savedMode == mode
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, mode]
                    {
                        QSettings().setValue(
                            QStringLiteral(
                                "rawValueMode"
                            ),
                            mode
                        );

                        if (m_hasCurrentData) {
                            renderDevice(
                                m_currentDrive,
                                m_currentData
                            );
                        }
                    }
                );
            }
        }
}
