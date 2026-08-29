// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lindiskhistoryplot.h"

#include <QDateTime>
#include <QLocale>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

LinDiskHistoryPlot::LinDiskHistoryPlot(
    QWidget *parent
)
    : QWidget(parent)
{
    setMinimumHeight(360);
}


void LinDiskHistoryPlot::setSamples(
    const QVector<QPointF> &samples,
    const QString &unit,
    const QString &emptyText,
    bool percentageRange
)
{
    m_samples = samples;
    m_unit = unit;
    m_emptyText = emptyText;
    m_percentageRange =
        percentageRange;

    update();
}


void LinDiskHistoryPlot::paintEvent(
    QPaintEvent *event
)
{
    QWidget::paintEvent(event);

    QPainter painter(this);

    painter.setRenderHint(
        QPainter::Antialiasing,
        true
    );

    const QRectF plot(
        72.0,
        20.0,
        std::max(
            10.0,
            width() - 92.0
        ),
        std::max(
            10.0,
            height() - 64.0
        )
    );

    QPen gridPen(
        palette().color(
            QPalette::Mid
        )
    );

    gridPen.setWidthF(0.7);

    painter.setPen(gridPen);
    painter.drawRect(plot);

    if (m_samples.isEmpty()) {
        painter.setPen(
            palette().color(
                QPalette::Text
            )
        );

        painter.drawText(
            plot,
            Qt::AlignCenter |
            Qt::TextWordWrap,
            m_emptyText
        );

        return;
    }

    double minX =
        m_samples.first().x();

    double maxX =
        m_samples.last().x();

    double minY =
        m_samples.first().y();

    double maxY =
        minY;

    for (const QPointF &point :
         m_samples) {

        minX = std::min(
            minX,
            point.x()
        );

        maxX = std::max(
            maxX,
            point.x()
        );

        minY = std::min(
            minY,
            point.y()
        );

        maxY = std::max(
            maxY,
            point.y()
        );
    }

    if (m_percentageRange) {
        minY = 0.0;
        maxY = 100.0;

    } else if (
        qFuzzyCompare(
            minY + 1.0,
            maxY + 1.0
        )
    ) {
        const double padding =
            std::max(
                1.0,
                std::abs(minY) *
                0.05
            );

        minY -= padding;
        maxY += padding;

    } else {
        const double padding =
            (maxY - minY) *
            0.08;

        minY -= padding;
        maxY += padding;
    }

    if (maxX <= minX)
        maxX = minX + 1.0;

    const auto mapX =
        [&plot, minX, maxX](
            double x
        )
        {
            return
                plot.left() +
                (
                    (x - minX) /
                    (maxX - minX)
                ) *
                plot.width();
        };

    const auto mapY =
        [&plot, minY, maxY](
            double y
        )
        {
            return
                plot.bottom() -
                (
                    (y - minY) /
                    (maxY - minY)
                ) *
                plot.height();
        };

    painter.setPen(gridPen);

    QLocale locale;

    for (int i = 0; i <= 4; ++i) {
        const double factor =
            static_cast<double>(i) /
            4.0;

        const double y =
            plot.bottom() -
            factor *
            plot.height();

        painter.drawLine(
            QPointF(
                plot.left(),
                y
            ),
            QPointF(
                plot.right(),
                y
            )
        );

        const double value =
            minY +
            factor *
            (maxY - minY);

        QString label =
            locale.toString(
                value,
                'f',
                m_percentageRange
                    ? 0
                    : (
                        std::abs(value) <
                                10.0
                            ? 2
                            : 1
                    )
            );

        if (!m_unit.isEmpty()) {
            label +=
                QStringLiteral(" ") +
                m_unit;
        }

        painter.setPen(
            palette().color(
                QPalette::Text
            )
        );

        painter.drawText(
            QRectF(
                0.0,
                y - 10.0,
                66.0,
                20.0
            ),
            Qt::AlignRight |
            Qt::AlignVCenter,
            label
        );

        painter.setPen(gridPen);
    }

    const qint64 span =
        static_cast<qint64>(
            maxX - minX
        );

    for (int i = 0; i <= 4; ++i) {
        const double factor =
            static_cast<double>(i) /
            4.0;

        const double x =
            plot.left() +
            factor *
            plot.width();

        painter.drawLine(
            QPointF(
                x,
                plot.top()
            ),
            QPointF(
                x,
                plot.bottom()
            )
        );

        const qint64 timestamp =
            static_cast<qint64>(
                minX +
                factor *
                (maxX - minX)
            );

        const QDateTime time =
            QDateTime::
                fromMSecsSinceEpoch(
                    timestamp
                );

        const QString label =
            span >
                48LL *
                60LL *
                60LL *
                1000LL
                ? time.toString(
                      QStringLiteral(
                          "dd.MM"
                      )
                  )
                : time.toString(
                      QStringLiteral(
                          "HH:mm"
                      )
                  );

        painter.setPen(
            palette().color(
                QPalette::Text
            )
        );

        painter.drawText(
            QRectF(
                x - 38.0,
                plot.bottom() + 7.0,
                76.0,
                24.0
            ),
            Qt::AlignHCenter |
            Qt::AlignTop,
            label
        );

        painter.setPen(gridPen);
    }

    QPolygonF line;

    line.reserve(
        m_samples.size()
    );

    for (const QPointF &point :
         m_samples) {

        line.append(
            QPointF(
                mapX(point.x()),
                mapY(point.y())
            )
        );
    }

    QPen linePen(
        palette().color(
            QPalette::Highlight
        )
    );

    linePen.setWidthF(2.2);

    painter.setPen(linePen);

    if (line.size() > 1) {
        painter.drawPolyline(line);
    } else {
        painter.drawEllipse(
            line.first(),
            3.0,
            3.0
        );
    }
}
