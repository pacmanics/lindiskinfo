// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>

QString MainWindow::currentReportText() const
{
    QString report;
    QTextStream out(&report);

    out
        << "LinDiskInfo "
        << QCoreApplication::applicationVersion()
        << "\n";
    out << "===================\n\n";

    if (m_copyIdentifyAction->isChecked()) {
        out << m_titleLabel->text() << "\n\n";

        out << tx("Firmware", "Firmware")
            << ": "
            << m_firmwareValue->text()
            << "\n";

        out << tx("Serial Number", "Seriennummer")
            << ": "
            << (
                m_serialVisible
                    ? m_serialEdit->text()
                    : QStringLiteral("********")
            )
            << "\n";

        out << tx("Interface", "Schnittstelle")
            << ": "
            << m_interfaceValue->text()
            << "\n";

        out << tx("Transfer Mode", "Übertragungsmodus")
            << ": "
            << m_transferValue->text()
            << "\n";

        out << tx("Health Status", "Gesundheitsstatus")
            << ": "
            << m_healthValue->text()
            << "\n";

        out << tx("Temperature", "Temperatur")
            << ": "
            << m_temperatureValue->text()
            << "\n\n";
    }

    if (!m_copySmartDataAction->isChecked())
        return report;

    const QString separator =
        m_copyAsciiAction->isChecked()
            ? QStringLiteral(" | ")
            : QStringLiteral("\t");

    QList<int> columns;

    for (int column = 0;
         column < m_table->columnCount();
         ++column) {

        if (m_table->isColumnHidden(column))
            continue;

        if (!m_copyThresholdAction->isChecked() &&
            column == 6) {
            continue;
        }

        columns.append(column);
    }

    for (int column : columns) {
        const QTableWidgetItem *header =
            m_table->horizontalHeaderItem(column);

        out << (
            header
                ? header->text()
                : QString()
        );

        if (column != columns.last())
            out << separator;
    }

    out << "\n";

    for (int row = 0;
         row < m_table->rowCount();
         ++row) {

        for (int column : columns) {
            const QTableWidgetItem *item =
                m_table->item(row, column);

            out << (
                item
                    ? item->text()
                    : QString()
            );

            if (column != columns.last())
                out << separator;
        }

        out << "\n";
    }

    return report;
}

void MainWindow::saveTextReport()
{
    QString path =
        QFileDialog::getSaveFileName(
            this,
            tx(
                "Save report",
                "Bericht speichern"
            ),
            QStringLiteral(
                "LinDiskInfo_%1.txt"
            ).arg(
                QDateTime::currentDateTime()
                    .toString(
                        QStringLiteral(
                            "yyyy-MM-dd_HH-mm-ss"
                        )
                    )
            ),
            QStringLiteral("Text (*.txt)")
        );

    if (path.isEmpty())
        return;

    if (!path.endsWith(
            QStringLiteral(".txt"),
            Qt::CaseInsensitive
        )) {
        path += QStringLiteral(".txt");
    }

    QFile file(path);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text
        )) {

        QMessageBox::warning(
            this,
            QStringLiteral("LinDiskInfo"),
            tx(
                "Unable to save the file.",
                "Die Datei konnte nicht gespeichert werden."
            )
        );

        return;
    }

    QTextStream out(&file);
    out << currentReportText();
}

void MainWindow::saveImage()
{
    QString path =
        QFileDialog::getSaveFileName(
            this,
            tx(
                "Save image",
                "Bild speichern"
            ),
            QStringLiteral(
                "LinDiskInfo_%1.png"
            ).arg(
                QDateTime::currentDateTime()
                    .toString(
                        QStringLiteral(
                            "yyyy-MM-dd_HH-mm-ss"
                        )
                    )
            ),
            QStringLiteral("PNG (*.png)")
        );

    if (path.isEmpty())
        return;

    if (!path.endsWith(
            QStringLiteral(".png"),
            Qt::CaseInsensitive
        )) {
        path += QStringLiteral(".png");
    }

    if (!grab().save(path, "PNG")) {
        QMessageBox::warning(
            this,
            QStringLiteral("LinDiskInfo"),
            tx(
                "Unable to save the image.",
                "Das Bild konnte nicht gespeichert werden."
            )
        );
    }
}

void MainWindow::copyReport()
{
    QGuiApplication::clipboard()->setText(
        currentReportText()
    );
}
