// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "responsivetablelayout.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QWidget>

#include <algorithm>

namespace
{

constexpr int ColumnStatus = 0;
constexpr int ColumnId = 1;
constexpr int ColumnAttribute = 2;
constexpr int ColumnValue = 3;
constexpr int ColumnCurrent = 4;
constexpr int ColumnWorst = 5;
constexpr int ColumnThreshold = 6;
constexpr int ColumnRaw = 7;

}


ResponsiveTableLayout::ResponsiveTableLayout(
    QTableWidget *table,
    QObject *parent
)
    : QObject(parent),
      m_table(table)
{
    if (!m_table)
        return;

    if (m_table->viewport()) {
        m_table->viewport()
            ->installEventFilter(this);
    }

    QHeaderView *header =
        m_table->horizontalHeader();

    if (header) {
        connect(
            header,
            &QHeaderView::sectionResized,
            this,
            [this](
                int logicalIndex,
                int,
                int
            )
            {
                if (m_applying)
                    return;

                m_lastUserResizedColumn =
                    logicalIndex;

                scheduleConstrain();
            }
        );
    }
}


void ResponsiveTableLayout::configure(
    bool nvmeMode,
    bool rawVisible
)
{
    m_nvmeMode = nvmeMode;
    m_rawVisible = rawVisible;
    m_configured = true;

    applyIdealLayout();
}


bool ResponsiveTableLayout::eventFilter(
    QObject *watched,
    QEvent *event
)
{
    if (m_table &&
        watched == m_table->viewport() &&
        event &&
        event->type() == QEvent::Resize) {

        m_lastUserResizedColumn = -1;
        scheduleConstrain();
    }

    return QObject::eventFilter(
        watched,
        event
    );
}


void ResponsiveTableLayout::scheduleConstrain()
{
    if (!m_configured ||
        m_constrainQueued) {
        return;
    }

    m_constrainQueued = true;

    QTimer::singleShot(
        0,
        this,
        [this]
        {
            m_constrainQueued = false;
            constrainToViewport();
        }
    );
}


int ResponsiveTableLayout::availableWidth() const
{
    if (!m_table ||
        !m_table->viewport()) {
        return 1;
    }

    return std::max(
        1,
        m_table->viewport()->width() - 2
    );
}


int ResponsiveTableLayout::headerMinimum(
    int column,
    int fallback
) const
{
    if (!m_table)
        return fallback;

    const QTableWidgetItem *item =
        m_table->horizontalHeaderItem(
            column
        );

    if (!item ||
        item->text().isEmpty()) {
        return fallback;
    }

    const QFontMetrics metrics(
        m_table->horizontalHeader()->font()
    );

    return std::max(
        fallback,
        metrics.horizontalAdvance(
            item->text()
        ) + 20
    );
}


void ResponsiveTableLayout::applyIdealLayout()
{
    if (!m_table)
        return;

    QHeaderView *header =
        m_table->horizontalHeader();

    if (!header)
        return;

    m_applying = true;

    header->setStretchLastSection(false);
    header->setMinimumSectionSize(24);
    header->setSectionsMovable(false);
    header->setCascadingSectionResizes(false);

    for (int column = 0;
         column < m_table->columnCount();
         ++column) {

        header->setSectionResizeMode(
            column,
            QHeaderView::Interactive
        );
    }

    m_table->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
    );

    const int available =
        availableWidth();

    const int statusWidth = 30;

    const int idWidth =
        headerMinimum(
            ColumnId,
            46
        );

    m_table->setColumnWidth(
        ColumnStatus,
        statusWidth
    );

    m_table->setColumnWidth(
        ColumnId,
        idWidth
    );

    if (m_nvmeMode) {
        int valueWidth =
            std::max(
                headerMinimum(
                    ColumnValue,
                    118
                ),
                std::min(
                    180,
                    available * 18 / 100
                )
            );

        int rawWidth =
            m_rawVisible
                ? std::max(
                      headerMinimum(
                          ColumnRaw,
                          145
                      ),
                      std::min(
                          220,
                          available * 22 / 100
                      )
                  )
                : 0;

        const int attributeMinimum =
            headerMinimum(
                ColumnAttribute,
                180
            );

        int attributeWidth =
            available -
            statusWidth -
            idWidth -
            valueWidth -
            rawWidth;

        if (attributeWidth <
            attributeMinimum) {

            int deficit =
                attributeMinimum -
                attributeWidth;

            if (m_rawVisible) {
                const int rawFloor =
                    headerMinimum(
                        ColumnRaw,
                        110
                    );

                const int shrink =
                    std::min(
                        deficit,
                        std::max(
                            0,
                            rawWidth -
                            rawFloor
                        )
                    );

                rawWidth -= shrink;
                deficit -= shrink;
            }

            const int valueFloor =
                headerMinimum(
                    ColumnValue,
                    100
                );

            const int valueShrink =
                std::min(
                    deficit,
                    std::max(
                        0,
                        valueWidth -
                        valueFloor
                    )
                );

            valueWidth -= valueShrink;

            attributeWidth =
                available -
                statusWidth -
                idWidth -
                valueWidth -
                rawWidth;
        }

        attributeWidth =
            std::max(
                80,
                attributeWidth
            );

        m_table->setColumnWidth(
            ColumnAttribute,
            attributeWidth
        );

        m_table->setColumnWidth(
            ColumnValue,
            valueWidth
        );

        if (m_rawVisible) {
            m_table->setColumnWidth(
                ColumnRaw,
                rawWidth
            );
        }
    } else {
        int currentWidth =
            headerMinimum(
                ColumnCurrent,
                80
            );

        int worstWidth =
            headerMinimum(
                ColumnWorst,
                95
            );

        int thresholdWidth =
            headerMinimum(
                ColumnThreshold,
                100
            );

        int rawWidth =
            m_rawVisible
                ? headerMinimum(
                      ColumnRaw,
                      145
                  )
                : 0;

        const int attributeMinimum =
            headerMinimum(
                ColumnAttribute,
                180
            );

        int attributeWidth =
            available -
            statusWidth -
            idWidth -
            currentWidth -
            worstWidth -
            thresholdWidth -
            rawWidth;

        if (attributeWidth <
            attributeMinimum &&
            m_rawVisible) {

            const int rawFloor =
                headerMinimum(
                    ColumnRaw,
                    110
                );

            const int deficit =
                attributeMinimum -
                attributeWidth;

            const int shrink =
                std::min(
                    deficit,
                    std::max(
                        0,
                        rawWidth -
                        rawFloor
                    )
                );

            rawWidth -= shrink;

            attributeWidth =
                available -
                statusWidth -
                idWidth -
                currentWidth -
                worstWidth -
                thresholdWidth -
                rawWidth;
        }

        attributeWidth =
            std::max(
                80,
                attributeWidth
            );

        m_table->setColumnWidth(
            ColumnAttribute,
            attributeWidth
        );

        m_table->setColumnWidth(
            ColumnCurrent,
            currentWidth
        );

        m_table->setColumnWidth(
            ColumnWorst,
            worstWidth
        );

        m_table->setColumnWidth(
            ColumnThreshold,
            thresholdWidth
        );

        if (m_rawVisible) {
            m_table->setColumnWidth(
                ColumnRaw,
                rawWidth
            );
        }
    }

    m_applying = false;

    constrainToViewport();
}


void ResponsiveTableLayout::constrainToViewport()
{
    if (!m_table ||
        !m_configured ||
        m_applying) {
        return;
    }

    m_applying = true;

    const int available =
        availableWidth();

    QVector<int> columns =
    {
        ColumnStatus,
        ColumnId,
        ColumnAttribute
    };

    if (m_nvmeMode) {
        columns.append(
            ColumnValue
        );
    } else {
        columns.append(
            ColumnCurrent
        );

        columns.append(
            ColumnWorst
        );

        columns.append(
            ColumnThreshold
        );
    }

    if (m_rawVisible)
        columns.append(ColumnRaw);

    int total = 0;

    for (const int column : columns) {
        if (!m_table->isColumnHidden(column)) {
            total +=
                m_table->columnWidth(
                    column
                );
        }
    }

    int delta =
        available -
        total;

    if (delta > 0) {
        int expansionColumn =
            ColumnAttribute;

        if (
            m_lastUserResizedColumn ==
            ColumnAttribute
        ) {
            if (
                m_rawVisible &&
                !m_table->isColumnHidden(
                    ColumnRaw
                )
            ) {
                expansionColumn =
                    ColumnRaw;

            } else if (m_nvmeMode) {
                expansionColumn =
                    ColumnValue;

            } else {
                expansionColumn =
                    ColumnThreshold;
            }
        }

        m_table->setColumnWidth(
            expansionColumn,
            m_table->columnWidth(
                expansionColumn
            ) + delta
        );

        m_lastUserResizedColumn = -1;
        m_applying = false;
        return;
    }

    if (delta == 0) {
        m_lastUserResizedColumn = -1;
        m_applying = false;
        return;
    }

    const int protectedColumn =
        m_lastUserResizedColumn;

    int overflow = -delta;

    auto shrinkColumn =
        [this, &overflow](
            int column,
            int floor
        )
        {
            if (overflow <= 0 ||
                !m_table ||
                m_table->isColumnHidden(
                    column
                )) {
                return;
            }

            const int width =
                m_table->columnWidth(
                    column
                );

            const int removable =
                std::max(
                    0,
                    width - floor
                );

            const int shrink =
                std::min(
                    overflow,
                    removable
                );

            if (shrink > 0) {
                m_table->setColumnWidth(
                    column,
                    width - shrink
                );

                overflow -= shrink;
            }
        };

    const auto shrinkCandidate =
        [
            &shrinkColumn,
            protectedColumn
        ](
            int column,
            int floor
        )
        {
            if (
                column ==
                protectedColumn
            ) {
                return;
            }

            shrinkColumn(
                column,
                floor
            );
        };

    shrinkCandidate(
        ColumnAttribute,
        headerMinimum(
            ColumnAttribute,
            160
        )
    );

    if (m_rawVisible) {
        shrinkCandidate(
            ColumnRaw,
            headerMinimum(
                ColumnRaw,
                105
            )
        );
    }

    if (m_nvmeMode) {
        shrinkCandidate(
            ColumnValue,
            headerMinimum(
                ColumnValue,
                95
            )
        );

    } else {
        shrinkCandidate(
            ColumnWorst,
            headerMinimum(
                ColumnWorst,
                80
            )
        );

        shrinkCandidate(
            ColumnThreshold,
            headerMinimum(
                ColumnThreshold,
                80
            )
        );

        shrinkCandidate(
            ColumnCurrent,
            headerMinimum(
                ColumnCurrent,
                70
            )
        );
    }

    if (overflow > 0) {
        shrinkCandidate(
            ColumnAttribute,
            80
        );
    }

    if (
        overflow > 0 &&
        m_rawVisible
    ) {
        shrinkCandidate(
            ColumnRaw,
            80
        );
    }

    if (
        overflow > 0 &&
        m_nvmeMode
    ) {
        shrinkCandidate(
            ColumnValue,
            75
        );
    }

    if (
        overflow > 0 &&
        !m_nvmeMode
    ) {
        shrinkCandidate(
            ColumnWorst,
            70
        );

        shrinkCandidate(
            ColumnThreshold,
            70
        );

        shrinkCandidate(
            ColumnCurrent,
            65
        );
    }

    if (
        overflow > 0 &&
        protectedColumn >= 0
    ) {
        int protectedFloor = 24;

        switch (protectedColumn) {
        case ColumnStatus:
            protectedFloor = 24;
            break;

        case ColumnId:
            protectedFloor = 36;
            break;

        case ColumnAttribute:
            protectedFloor = 80;
            break;

        case ColumnValue:
            protectedFloor = 75;
            break;

        case ColumnCurrent:
            protectedFloor = 65;
            break;

        case ColumnWorst:
            protectedFloor = 70;
            break;

        case ColumnThreshold:
            protectedFloor = 70;
            break;

        case ColumnRaw:
            protectedFloor = 80;
            break;

        default:
            break;
        }

        shrinkColumn(
            protectedColumn,
            protectedFloor
        );
    }

    total = 0;

    for (const int column : columns) {
        if (!m_table->isColumnHidden(column)) {
            total +=
                m_table->columnWidth(
                    column
                );
        }
    }

    if (total < available) {
        m_table->setColumnWidth(
            ColumnAttribute,
            m_table->columnWidth(
                ColumnAttribute
            ) +
            (
                available -
                total
            )
        );
    }

    m_lastUserResizedColumn = -1;
    m_applying = false;
}
