// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../ui/responsivetablelayout.h"
#include "../ui/smarttablecolumns.h"

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

void MainWindow::configureNvmeTable()
{
    m_table->setHorizontalHeaderLabels(
        {
            QString(),
            QStringLiteral("ID"),
            tx(
                "Attribute Name",
                "Parametername"
            ),
            tx(
                "Value",
                "Wert"
            ),
            tx(
                "Current",
                "Aktuell"
            ),
            tx(
                "Worst",
                "Schlechtester Wert"
            ),
            tx(
                "Threshold",
                "Grenzwert"
            ),
            tx(
                "Raw Value",
                "Rohwert"
            )
        }
    );

    m_table->setColumnHidden(
        ColumnStatus,
        false
    );

    m_table->setColumnHidden(
        ColumnValue,
        false
    );

    m_table->setColumnHidden(
        ColumnCurrent,
        true
    );

    m_table->setColumnHidden(
        ColumnWorst,
        true
    );

    m_table->setColumnHidden(
        ColumnThreshold,
        true
    );

    updateRawColumn();
}

void MainWindow::configureAtaTable()
{
    m_table->setHorizontalHeaderLabels(
        {
            QString(),
            QStringLiteral("ID"),
            tx(
                "Attribute Name",
                "Parametername"
            ),
            tx(
                "Value",
                "Wert"
            ),
            tx(
                "Current",
                "Aktuell"
            ),
            tx(
                "Worst",
                "Schlechtester Wert"
            ),
            tx(
                "Threshold",
                "Grenzwert"
            ),
            tx(
                "Raw Value",
                "Rohwert"
            )
        }
    );

    m_table->setColumnHidden(
        ColumnStatus,
        false
    );

    m_table->setColumnHidden(
        ColumnValue,
        true
    );

    m_table->setColumnHidden(
        ColumnCurrent,
        false
    );

    m_table->setColumnHidden(
        ColumnWorst,
        false
    );

    m_table->setColumnHidden(
        ColumnThreshold,
        false
    );

    updateRawColumn();
}

void MainWindow::applyTableColumnLayout()
{
    if (!m_table ||
        !m_tableLayoutController) {
        return;
    }

    const bool rawVisible =
        m_showRawAction &&
        m_showRawAction->isChecked();

    m_tableLayoutController->configure(
        m_currentTableIsNvme,
        rawVisible
    );
}

void MainWindow::addNvmeRow(
    const QString &id,
    const QString &attribute,
    const QString &value,
    const QString &raw,
    HealthState state
)
{
    const int row =
        m_table->rowCount();

    m_table->insertRow(row);

    auto *statusItem =
        new QTableWidgetItem(
            QStringLiteral("●")
        );

    switch (state) {
    case HealthState::Good:
        statusItem->setForeground(
            QColor(QStringLiteral("#39aaf3"))
        );
        break;

    case HealthState::Caution:
        statusItem->setForeground(
            QColor(QStringLiteral("#ffd400"))
        );
        break;

    case HealthState::Bad:
        statusItem->setForeground(
            QColor(QStringLiteral("#ff3838"))
        );
        break;

    default:
        break;
    }

    m_table->setItem(
        row,
        ColumnStatus,
        statusItem
    );

    m_table->setItem(
        row,
        ColumnId,
        new QTableWidgetItem(id)
    );

    m_table->setItem(
        row,
        ColumnAttribute,
        new QTableWidgetItem(attribute)
    );

    m_table->setItem(
        row,
        ColumnValue,
        new QTableWidgetItem(value)
    );

    m_table->setItem(
        row,
        ColumnRaw,
        new QTableWidgetItem(raw)
    );
}

void MainWindow::addAtaRow(
    const QString &id,
    const QString &attribute,
    const QString &current,
    const QString &worst,
    const QString &threshold,
    const QString &raw,
    HealthState state
)
{
    const int row =
        m_table->rowCount();

    m_table->insertRow(row);

    auto *statusItem =
        new QTableWidgetItem(
            QStringLiteral("●")
        );

    switch (state) {
    case HealthState::Good:
        statusItem->setForeground(
            QColor(QStringLiteral("#39aaf3"))
        );
        break;

    case HealthState::Caution:
        statusItem->setForeground(
            QColor(QStringLiteral("#ffd400"))
        );
        break;

    case HealthState::Bad:
        statusItem->setForeground(
            QColor(QStringLiteral("#ff3838"))
        );
        break;

    default:
        break;
    }

    m_table->setItem(
        row,
        ColumnStatus,
        statusItem
    );

    m_table->setItem(
        row,
        ColumnId,
        new QTableWidgetItem(id)
    );

    m_table->setItem(
        row,
        ColumnAttribute,
        new QTableWidgetItem(attribute)
    );

    m_table->setItem(
        row,
        ColumnCurrent,
        new QTableWidgetItem(current)
    );

    m_table->setItem(
        row,
        ColumnWorst,
        new QTableWidgetItem(worst)
    );

    m_table->setItem(
        row,
        ColumnThreshold,
        new QTableWidgetItem(threshold)
    );

    m_table->setItem(
        row,
        ColumnRaw,
        new QTableWidgetItem(raw)
    );
}

void MainWindow::updateRawColumn()
{
    if (!m_table)
        return;

    const bool showRaw =
        m_showRawAction &&
        m_showRawAction->isChecked();

    m_table->setColumnHidden(
        ColumnRaw,
        !showRaw
    );

    saveCurrentTableWidths();

    applyTableColumnLayout();

    restoreCurrentTableWidths();
}
