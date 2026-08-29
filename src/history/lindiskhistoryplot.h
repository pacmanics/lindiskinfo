// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QPaintEvent;

class LinDiskHistoryPlot final
    : public QWidget
{
public:
    explicit LinDiskHistoryPlot(
        QWidget *parent = nullptr
    );

    void setSamples(
        const QVector<QPointF> &samples,
        const QString &unit,
        const QString &emptyText,
        bool percentageRange
    );

protected:
    void paintEvent(
        QPaintEvent *event
    ) override;

private:
    QVector<QPointF> m_samples;

    QString m_unit;
    QString m_emptyText;

    bool m_percentageRange = false;
};
