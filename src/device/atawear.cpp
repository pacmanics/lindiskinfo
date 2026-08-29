// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atawear.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

bool unsignedValue(
    const QJsonValue &value,
    quint64 *result
)
{
    if (!result)
        return false;

    if (value.isString()) {
        bool ok = false;

        const quint64 parsed =
            value.toString().toULongLong(
                &ok,
                10
            );

        if (!ok)
            return false;

        *result = parsed;
        return true;
    }

    if (!value.isDouble())
        return false;

    const double number =
        value.toDouble();

    if (
        !std::isfinite(number) ||
        number < 0.0 ||
        number >
            static_cast<double>(
                std::numeric_limits<
                    quint64
                >::max()
            ) ||
        std::floor(number) != number
    ) {
        return false;
    }

    *result =
        static_cast<quint64>(
            number
        );

    return true;
}


bool mergeCandidate(
    AtaLifeEstimate *result,
    int remaining
)
{
    if (
        !result ||
        remaining < 0 ||
        remaining > 100
    ) {
        return true;
    }

    if (!result->valid) {
        result->valid = true;
        result->remainingPercent =
            remaining;

        return true;
    }

    if (
        std::abs(
            result->remainingPercent -
            remaining
        ) > 1
    ) {
        return false;
    }

    result->remainingPercent =
        std::min(
            result->remainingPercent,
            remaining
        );

    return true;
}

}


AtaLifeEstimate ataLifeEstimate(
    const QJsonObject &data
)
{
    AtaLifeEstimate result;

    const QJsonArray attributes =
        data.value(
            QStringLiteral(
                "ata_smart_attributes"
            )
        ).toObject()
        .value(
            QStringLiteral("table")
        ).toArray();

    for (
        const QJsonValue &entryValue :
        attributes
    ) {
        const QJsonObject attribute =
            entryValue.toObject();

        const QString name =
            attribute.value(
                QStringLiteral("name")
            ).toString();

        int remaining = -1;

        if (
            name ==
                QStringLiteral(
                    "SSD_Life_Left"
                ) ||
            name ==
                QStringLiteral(
                    "Percent_Lifetime_Remain"
                ) ||
            name ==
                QStringLiteral(
                    "Remaining_Lifetime_Perc"
                ) ||
            name ==
                QStringLiteral(
                    "Media_Wearout_Indicator"
                )
        ) {
            quint64 current = 0;

            if (
                !unsignedValue(
                    attribute.value(
                        QStringLiteral("value")
                    ),
                    &current
                ) ||
                current > 100
            ) {
                continue;
            }

            remaining =
                static_cast<int>(
                    current
                );
        } else if (
            name ==
                QStringLiteral(
                    "Percent_Lifetime_Used"
                )
        ) {
            quint64 used = 0;

            const QJsonObject raw =
                attribute.value(
                    QStringLiteral("raw")
                ).toObject();

            if (
                !unsignedValue(
                    raw.value(
                        QStringLiteral("value")
                    ),
                    &used
                )
            ) {
                continue;
            }

            remaining =
                used >= 100
                ? 0
                : 100 -
                    static_cast<int>(
                        used
                    );
        } else {
            continue;
        }

        if (
            !mergeCandidate(
                &result,
                remaining
            )
        ) {
            return {};
        }
    }

    return result;
}
