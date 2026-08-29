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

void MainWindow::setupMaintenanceMenus()
{
QMenu *selfTestMenu =
            m_advancedOptionsMenu
                ->addMenu(
                    QString()
                );

        selfTestMenu->setObjectName(
            QStringLiteral(
                "selfTestMenu"
            )
        );

        QAction *shortTest =
            selfTestMenu->addAction(
                QString()
            );

        shortTest->setObjectName(
            QStringLiteral(
                "shortSelfTestAction"
            )
        );

        QAction *longTest =
            selfTestMenu->addAction(
                QString()
            );

        longTest->setObjectName(
            QStringLiteral(
                "longSelfTestAction"
            )
        );

        selfTestMenu->addSeparator();

        QAction *abortTest =
            selfTestMenu->addAction(
                QString()
            );

        abortTest->setObjectName(
            QStringLiteral(
                "abortSelfTestAction"
            )
        );


        QMenu *logsMenu =
            m_advancedOptionsMenu
                ->addMenu(
                    QString()
                );

        logsMenu->setObjectName(
            QStringLiteral(
                "smartLogsMenu"
            )
        );

        QAction *selfTestLog =
            logsMenu->addAction(
                QString()
            );

        selfTestLog->setObjectName(
            QStringLiteral(
                "selfTestLogAction"
            )
        );

        QAction *errorLog =
            logsMenu->addAction(
                QString()
            );

        errorLog->setObjectName(
            QStringLiteral(
                "errorLogAction"
            )
        );

        logsMenu->addSeparator();

        QAction *deviceStatistics =
            logsMenu->addAction(
                QString()
            );

        deviceStatistics->setObjectName(
            QStringLiteral(
                "deviceStatisticsAction"
            )
        );

        QAction *sataPhy =
            logsMenu->addAction(
                QString()
            );

        sataPhy->setObjectName(
            QStringLiteral(
                "sataPhyAction"
            )
        );


        const auto invokeMaintenance =
            [this](
                const QString &operation,
                bool ataOnly,
                bool confirmExtended
            )
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

                const bool smartAvailable =
                    m_currentData.value(
                        QStringLiteral(
                            "lindiskinfo_smart_available"
                        )
                    ).toBool(true);

                if (!smartAvailable) {
                    QMessageBox::information(
                        this,
                        QStringLiteral(
                            "LinDiskInfo"
                        ),
                        tx(
                            "The selected drive does not provide SMART diagnostics.",
                            "Das ausgewählte Laufwerk stellt keine SMART-Diagnosefunktionen bereit."
                        )
                    );

                    return;
                }

                if (ataOnly) {
                    const bool ataDevice =
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_smart_attributes"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_version"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "sata_version"
                            )
                        );

                    if (!ataDevice) {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "This function is available for ATA/SATA drives only.",
                                "Diese Funktion ist nur für ATA-/SATA-Laufwerke verfügbar."
                            )
                        );

                        return;
                    }
                }

                if (confirmExtended) {
                    const auto answer =

                        QMessageBox::question(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "Start Extended Self-Test?\n\nAn extended self-test may take a long time and can reduce drive performance while it is running.\n\nStart the test now?",
                                "Erweiterten Selbsttest starten?\n\nEin erweiterter Selbsttest kann lange dauern und währenddessen die Laufwerksleistung reduzieren.\n\nTest jetzt starten?"
                            ),
                            QMessageBox::Yes |
                            QMessageBox::No,
                            QMessageBox::No
                        );

                    if (answer !=
                        QMessageBox::Yes) {
                        return;
                    }
                }

                if (operation ==
                    QStringLiteral(
                        "test_abort"
                    )) {

                    const auto answer =
                        QMessageBox::question(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "Abort running self-test?\n\nSend an abort command to the selected drive?",
                                "Laufenden Selbsttest abbrechen?\n\nAbbruchbefehl an das ausgewählte Laufwerk senden?"
                            ),
                            QMessageBox::Yes |
                            QMessageBox::No,
                            QMessageBox::No
                        );

                    if (answer !=
                        QMessageBox::Yes) {
                        return;
                    }
                }

                setStatus(
                    tx(
                        "Executing drive diagnostic command...",
                        "Laufwerksdiagnose wird ausgeführt..."
                    )
                );

                m_backend->requestMaintenance(
                    m_currentDrive,
                    operation
                );
            };


        connect(
            shortTest,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "test_short"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            longTest,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "test_long"
                    ),
                    false,
                    true
                );
            }
        );

        connect(
            abortTest,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "test_abort"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            selfTestLog,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_selftest"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            errorLog,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_error"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            deviceStatistics,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_devstat"
                    ),
                    true,
                    false
                );
            }
        );

        connect(
            sataPhy,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_sataphy"
                    ),
                    true,
                    false
                );
            }
        );
}
