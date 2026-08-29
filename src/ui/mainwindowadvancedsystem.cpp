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

void MainWindow::setupAdvancedSystemBehaviorActions()
{
    const auto menuByName =
        [this](const char *name) -> QMenu *
        {
            return findChild<QMenu *>(
                QString::fromLatin1(name)
            );
        };

// ====================================================
        // Startup Delay
        // ====================================================

        if (QMenu *startup =
                menuByName(
                    "startupDelayMenu"
                )) {

            startup->menuAction()
                ->setVisible(true);

            auto *group =
                new QActionGroup(this);

            group->setExclusive(true);

            for (QAction *entry :
                 startup->actions()) {

                const QString text =
                    entry->text();

                bool ok = false;

                const int seconds =
                    text.section(
                        QLatin1Char(' '),
                        0,
                        0
                    ).toInt(&ok);

                if (!ok)
                    continue;

                entry->setData(seconds);
                entry->setCheckable(true);

                group->addAction(entry);

                entry->setChecked(
                    seconds ==
                    m_startupDelaySeconds
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, seconds]
                    {
                        m_startupDelaySeconds =
                            seconds;

                        QSettings().setValue(
                            QStringLiteral(
                                "startupDelaySeconds"
                            ),
                            seconds
                        );
                    }
                );
            }
        }


        // ====================================================
        // Tray Behavior
        // ====================================================

        if (QMenu *tray =
                menuByName(
                    "trayBehaviorMenu"
                )) {

            tray->menuAction()
                ->setVisible(true);

            const QList<QAction *> entries =
                tray->actions();

            if (entries.size() >= 2) {
                auto *group =
                    new QActionGroup(this);

                group->setExclusive(true);

                const QStringList modes =
                {
                    QStringLiteral("hide"),
                    QStringLiteral("minimize")
                };

                for (int i = 0;
                     i < 2;
                     ++i) {

                    QAction *entry =
                        entries.at(i);

                    const QString mode =
                        modes.at(i);

                    entry->setCheckable(true);
                    entry->setData(mode);

                    group->addAction(entry);

                    entry->setChecked(
                        m_trayBehavior == mode
                    );

                    connect(
                        entry,
                        &QAction::triggered,
                        this,
                        [this, mode]
                        {
                            m_trayBehavior =
                                mode;

                            QSettings().setValue(
                                QStringLiteral(
                                    "trayBehavior"
                                ),
                                mode
                            );
                        }
                    );
                }
            }
        }
}
