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

void MainWindow::setupAtaManagementMenus()
{
    QSettings settings;

auto actionByName061 =
            [this](const char *name)
                -> QAction *
            {
                return findChild<QAction *>(
                    QString::fromLatin1(
                        name
                    )
                );
            };


        // ====================================================
        // Management
        // ====================================================

        if (QAction *management =
                actionByName061(
                    "aamApmManagementAction"
                )) {

            management->setVisible(true);

            connect(
                management,
                &QAction::triggered,
                this,
                [this]
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

                    const bool ataDevice =
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_smart_attributes"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_aam"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_apm"
                            )
                        );

                    if (!ataDevice) {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "AAM/APM is an ATA feature and is not available for the selected drive.",
                                "AAM/APM ist eine ATA-Funktion und steht für das ausgewählte Laufwerk nicht zur Verfügung."
                            )
                        );

                        return;
                    }

                    const QJsonObject aam =
                        m_currentData.value(
                            QStringLiteral(
                                "ata_aam"
                            )
                        ).toObject();

                    const QJsonObject apm =
                        m_currentData.value(
                            QStringLiteral(
                                "ata_apm"
                            )
                        ).toObject();

                    const bool aamSupported =
                        !aam.isEmpty();

                    const bool apmSupported =
                        !apm.isEmpty();

                    if (!aamSupported &&
                        !apmSupported) {

                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "The selected ATA drive reports both AAM and APM as unavailable.",
                                "Das ausgewählte ATA-Laufwerk meldet sowohl AAM als auch APM als nicht verfügbar."
                            )
                        );

                        return;
                    }

                    QDialog dialog(this);

                    dialog.setWindowTitle(
                        tx(
                            "AAM/APM Management",
                            "AAM/APM-Verwaltung"
                        )
                    );

                    auto *form =
                        new QFormLayout(
                            &dialog
                        );

                    auto *driveLabel =
                        new QLabel(
                            m_titleLabel->text(),
                            &dialog
                        );

                    driveLabel->setWordWrap(
                        true
                    );

                    form->addRow(
                        tx(
                            "Drive:",
                            "Laufwerk:"
                        ),
                        driveLabel
                    );


                    // ----------------------------------------
                    // AAM
                    // ----------------------------------------

                    QCheckBox *aamEnabled =
                        nullptr;

                    QSpinBox *aamLevel =
                        nullptr;

                    if (aamSupported) {
                        aamEnabled =
                            new QCheckBox(
                                tx(
                                    "Enabled",
                                    "Aktiviert"
                                ),
                                &dialog
                            );

                        aamLevel =
                            new QSpinBox(
                                &dialog
                            );

                        aamLevel->setRange(
                            128,
                            254
                        );

                        aamLevel->setValue(
                            aam.value(
                                QStringLiteral(
                                    "level"
                                )
                            ).toInt(
                                aam.value(
                                    QStringLiteral(
                                        "recommended_level"
                                    )
                                ).toInt(254)
                            )
                        );

                        aamEnabled->setChecked(
                            aam.value(
                                QStringLiteral(
                                    "enabled"
                                )
                            ).toBool(false)
                        );

                        aamLevel->setEnabled(
                            aamEnabled->isChecked()
                        );

                        connect(
                            aamEnabled,
                            &QCheckBox::toggled,
                            aamLevel,
                            &QSpinBox::setEnabled
                        );

                        auto *aamWidget =
                            new QWidget(&dialog);

                        auto *aamLayout =
                            new QHBoxLayout(
                                aamWidget
                            );

                        aamLayout
                            ->setContentsMargins(
                                0, 0, 0, 0
                            );

                        aamLayout->addWidget(
                            aamEnabled
                        );

                        aamLayout->addWidget(
                            aamLevel
                        );

                        form->addRow(
                            QStringLiteral("AAM:"),
                            aamWidget
                        );

                        const int recommended =
                            aam.value(
                                QStringLiteral(
                                    "recommended_level"
                                )
                            ).toInt(-1);

                        if (recommended >= 128 &&
                            recommended <= 254) {

                            form->addRow(
                                tx(
                                    "AAM recommended:",
                                    "AAM empfohlen:"
                                ),
                                new QLabel(
                                    QString::number(
                                        recommended
                                    ),
                                    &dialog
                                )
                            );
                        }

                    } else {
                        form->addRow(
                            QStringLiteral("AAM:"),
                            new QLabel(
                                tx(
                                    "Unavailable",
                                    "Nicht verfügbar"
                                ),
                                &dialog
                            )
                        );
                    }


                    // ----------------------------------------
                    // APM
                    // ----------------------------------------

                    QCheckBox *apmEnabled =
                        nullptr;

                    QSpinBox *apmLevel =
                        nullptr;

                    if (apmSupported) {
                        apmEnabled =
                            new QCheckBox(
                                tx(
                                    "Enabled",
                                    "Aktiviert"
                                ),
                                &dialog
                            );

                        apmLevel =
                            new QSpinBox(
                                &dialog
                            );

                        apmLevel->setRange(
                            1,
                            254
                        );

                        apmLevel->setValue(
                            apm.value(
                                QStringLiteral(
                                    "level"
                                )
                            ).toInt(128)
                        );

                        apmEnabled->setChecked(
                            apm.value(
                                QStringLiteral(
                                    "enabled"
                                )
                            ).toBool(false)
                        );

                        apmLevel->setEnabled(
                            apmEnabled->isChecked()
                        );

                        connect(
                            apmEnabled,
                            &QCheckBox::toggled,
                            apmLevel,
                            &QSpinBox::setEnabled
                        );

                        auto *apmWidget =
                            new QWidget(&dialog);

                        auto *apmLayout =
                            new QHBoxLayout(
                                apmWidget
                            );

                        apmLayout
                            ->setContentsMargins(
                                0, 0, 0, 0
                            );

                        apmLayout->addWidget(
                            apmEnabled
                        );

                        apmLayout->addWidget(
                            apmLevel
                        );

                        form->addRow(
                            QStringLiteral("APM:"),
                            apmWidget
                        );

                    } else {
                        form->addRow(
                            QStringLiteral("APM:"),
                            new QLabel(
                                tx(
                                    "Unavailable",
                                    "Nicht verfügbar"
                                ),
                                &dialog
                            )
                        );
                    }


                    auto *note =
                        new QLabel(
                            tx(
                                "AAM controls acoustic/performance behavior. APM controls ATA power management. Support and exact behavior depend on the drive firmware.",
                                "AAM steuert Geräuschentwicklung und Leistung. APM steuert das ATA-Energiemanagement. Unterstützung und genaues Verhalten hängen von der Laufwerks-Firmware ab."
                            ),
                            &dialog
                        );

                    note->setWordWrap(true);

                    form->addRow(note);


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

                    QString aamValue;
                    QString apmValue;

                    if (aamSupported) {
                        aamValue =
                            aamEnabled->isChecked()
                                ? QString::number(
                                      aamLevel->value()
                                  )
                                : QStringLiteral(
                                      "off"
                                  );
                    }

                    if (apmSupported) {
                        apmValue =
                            apmEnabled->isChecked()
                                ? QString::number(
                                      apmLevel->value()
                                  )
                                : QStringLiteral(
                                      "off"
                                  );
                    }


                    // Save preferred values per physical drive.
                    const QString key =
                        ataSettingsKey(
                            m_currentDrive,
                            m_currentData
                        );

                    QSettings settings;

                    settings.beginGroup(
                        QStringLiteral(
                            "ataAuto"
                        )
                    );

                    settings.beginGroup(key);

                    if (!aamValue.isEmpty()) {
                        settings.setValue(
                            QStringLiteral("aam"),
                            aamValue
                        );
                    } else {
                        settings.remove(
                            QStringLiteral("aam")
                        );
                    }

                    if (!apmValue.isEmpty()) {
                        settings.setValue(
                            QStringLiteral("apm"),
                            apmValue
                        );
                    } else {
                        settings.remove(
                            QStringLiteral("apm")
                        );
                    }

                    settings.endGroup();
                    settings.endGroup();

                    m_autoAdjustedAtaDrives
                        .remove(key);

                    if (m_aamApmAutoEnabled) {
                        m_autoAdjustedAtaDrives
                            .insert(key);
                    }

                    setStatus(
                        tx(
                            "Applying AAM/APM settings...",
                            "AAM/APM-Einstellungen werden angewendet..."
                        )
                    );

                    m_backend
                        ->setAtaPowerSettings(
                            m_currentDrive,
                            aamValue,
                            apmValue
                        );
                }
            );
        }


        // ====================================================
        // Auto Adjustment
        // ====================================================

        if (QAction *automatic =
                actionByName061(
                    "aamApmAutoAction"
                )) {

            automatic->setVisible(true);
            automatic->setCheckable(true);

            automatic->setChecked(
                m_aamApmAutoEnabled
            );

            connect(
                automatic,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_aamApmAutoEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "aamApmAutoAdjustment"
                        ),
                        enabled
                    );

                    m_autoAdjustedAtaDrives
                        .clear();

                    if (!enabled)
                        return;

                    for (const DriveInfo &drive :
                         m_drives) {

                        if (!m_driveData.contains(
                                lindiskinfoDriveIdentity(drive)
                            )) {
                            continue;
                        }

                        maybeAutoAdjustAta(
                            drive,
                            m_driveData.value(
                                lindiskinfoDriveIdentity(drive)
                            )
                        );
                    }
                }
            );
        }
}
