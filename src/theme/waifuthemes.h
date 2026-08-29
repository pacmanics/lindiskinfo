// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPalette>
#include <QString>
#include <QtGlobal>
#include <QVector>

struct WaifuTheme final
{
    QString id;
    QString displayName;
    quint32 assetId = 0;
    QColor fallbackColor;
};

namespace WaifuThemes
{

const QVector<WaifuTheme> &all();

const WaifuTheme *find(
    const QString &id
);

QPalette darkPalette(
    const QPalette &systemPalette
);

}
