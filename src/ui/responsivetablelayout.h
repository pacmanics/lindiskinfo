// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>

class QEvent;
class QTableWidget;

class ResponsiveTableLayout final
    : public QObject
{
public:
    explicit ResponsiveTableLayout(
        QTableWidget *table,
        QObject *parent = nullptr
    );

    void configure(
        bool nvmeMode,
        bool rawVisible
    );

protected:
    bool eventFilter(
        QObject *watched,
        QEvent *event
    ) override;

private:
    void scheduleConstrain();
    void applyIdealLayout();
    void constrainToViewport();

    int availableWidth() const;

    int headerMinimum(
        int column,
        int fallback
    ) const;

    QPointer<QTableWidget> m_table;

    bool m_nvmeMode = true;
    bool m_rawVisible = true;
    bool m_configured = false;
    bool m_applying = false;
    bool m_constrainQueued = false;

    int m_lastUserResizedColumn = -1;
};
