// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "themebackgroundwidget.h"
#include "themeassetstore.h"
#include "waifuthemes.h"

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

void MainWindow::applyTheme()
{
    QSettings settings;

    QString themeId =
        settings.value(
            QStringLiteral("theme"),
            QStringLiteral("system")
        ).toString();

    if (
        themeId ==
            QStringLiteral("system") &&
        settings.value(
            QStringLiteral("darkMode"),
            false
        ).toBool()
    ) {
        themeId =
            QStringLiteral("dark");
    }

    const WaifuTheme *waifuTheme =
        WaifuThemes::find(
            themeId
        );

    if (
        themeId !=
            QStringLiteral("system") &&
        themeId !=
            QStringLiteral("dark") &&
        !waifuTheme
    ) {
        themeId =
            QStringLiteral("system");
    }

    m_darkMode =
        themeId !=
        QStringLiteral("system");

    if (m_darkMode) {
        qApp->setPalette(
            WaifuThemes::darkPalette(
                m_systemPalette
            )
        );
    } else {
        qApp->setPalette(
            m_systemPalette
        );
    }

    waifuTheme =
        WaifuThemes::find(
            themeId
        );

    if (waifuTheme) {
        setMinimumSize(
            1320,
            600
        );

        if (m_themeBackground) {
            m_themeBackground->setTheme(
                ThemeAssetStore::pixmap(
                    waifuTheme->assetId
                ),
                waifuTheme->fallbackColor
            );
        }
    } else {
        setMinimumSize(
            860,
            600
        );

        if (m_themeBackground)
            m_themeBackground->clearTheme();
    }

    if (m_themeGroup) {
        for (QAction *action :
             m_themeGroup->actions()) {

            action->setChecked(
                action->data().toString() ==
                themeId
            );
        }
    }

    if (m_table) {
        QTimer::singleShot(
            0,
            this,
            [this]
            {
                applyTableColumnLayout();
            }
        );
    }
}
