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

void MainWindow::setupAdvancedDriveActions()
{
    const auto actionByName =
        [this](const char *name) -> QAction *
        {
            return findChild<QAction *>(
                QString::fromLatin1(name)
            );
        };

    const auto menuByName =
        [this](const char *name) -> QMenu *
        {
            return findChild<QMenu *>(
                QString::fromLatin1(name)
            );
        };

// ====================================================
        // Drive Sort Method
        // ====================================================

        if (QMenu *sort =
                menuByName(
                    "driveSortMenu"
                )) {

            sort->menuAction()
                ->setVisible(true);

            sort->clear();

            auto *group =
                new QActionGroup(this);

            group->setExclusive(true);

            struct SortItem
            {
                const char *name;
                const char *mode;
            };

            const SortItem items[] =
            {
                {
                    "driveSortDefault",
                    "default"
                },
                {
                    "driveSortPath",
                    "path"
                },
                {
                    "driveSortModel",
                    "model"
                },
                {
                    "driveSortHealth",
                    "health"
                },
                {
                    "driveSortTemperature",
                    "temperature"
                }
            };

            for (const SortItem &item : items) {
                QAction *entry =
                    sort->addAction(
                        QString()
                    );

                entry->setObjectName(
                    QString::fromLatin1(
                        item.name
                    )
                );

                const QString mode =
                    QString::fromLatin1(
                        item.mode
                    );

                entry->setData(mode);
                entry->setCheckable(true);

                group->addAction(entry);

                entry->setChecked(
                    m_driveSortMethod ==
                    mode
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, mode]
                    {
                        m_driveSortMethod =
                            mode;

                        QSettings().setValue(
                            QStringLiteral(
                                "driveSortMethod"
                            ),
                            mode
                        );

                        refreshDrivePresentation();
                    }
                );
            }
        }


        // ====================================================
        // Display Number of Drives
        // ====================================================

        if (QMenu *display =
                menuByName(
                    "displayDrivesMenu"
                )) {

            display->menuAction()
                ->setVisible(true);

            display->clear();

            auto *group =
                new QActionGroup(this);

            group->setExclusive(true);

            const int limits[] =
            {
                0,
                4,
                8,
                16
            };

            for (int limit : limits) {
                QAction *entry =
                    display->addAction(
                        limit == 0
                            ? QString()
                            : QString::number(
                                  limit
                              )
                    );

                entry->setData(limit);
                entry->setCheckable(true);

                if (limit == 0) {
                    entry->setObjectName(
                        QStringLiteral(
                            "displayDrivesAll"
                        )
                    );
                }

                group->addAction(entry);

                entry->setChecked(
                    m_displayDriveLimit ==
                    limit
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, limit]
                    {
                        m_displayDriveLimit =
                            limit;

                        QSettings().setValue(
                            QStringLiteral(
                                "displayDriveLimit"
                            ),
                            limit
                        );

                        applyDriveButtonLimit();
                    }
                );
            }
        }


        // ====================================================
        // Full Device Rescan
        // ====================================================

        if (QAction *advancedSearch =
                actionByName(
                    "advancedDriveSearchAction"
                )) {

            advancedSearch->setVisible(true);

            connect(
                advancedSearch,
                &QAction::triggered,
                this,
                [this]
                {
                    setStatus(
                        tx(
                            "Performing full device rescan...",
                            "Führe vollständige Laufwerkssuche aus..."
                        )
                    );

                    m_driveData.clear();
                    refreshDevices();
                }
            );
        }


        // ====================================================
        // Hide devices without SMART
        // ====================================================

        if (QAction *hideNoSmart =
                actionByName(
                    "hideNoSmartAction"
                )) {

            hideNoSmart->setVisible(true);
            hideNoSmart->setCheckable(true);

            hideNoSmart->setChecked(
                m_hideNoSmart
            );

            connect(
                hideNoSmart,
                &QAction::toggled,
                this,
                [this](bool hidden)
                {
                    m_hideNoSmart =
                        hidden;

                    QSettings().setValue(
                        QStringLiteral(
                            "hideNoSmart"
                        ),
                        hidden
                    );

                    refreshDrivePresentation();
                }
            );
        }
}
