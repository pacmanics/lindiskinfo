// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>

struct AtaLifeEstimate final
{
    bool valid = false;
    int remainingPercent = -1;
};

AtaLifeEstimate ataLifeEstimate(
    const QJsonObject &data
);
