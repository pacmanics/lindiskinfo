// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waifuthemes.h"

#include <Qt>

namespace WaifuThemes
{

const QVector<WaifuTheme> &all()
{
    static const QVector<WaifuTheme> themes =
    {
        {
            QStringLiteral("cyber-samurai"),
            QStringLiteral("Cyber Samurai"),
            0xA7F31C52u,
            QColor(QStringLiteral("#101827"))
        },
        {
            QStringLiteral("emerald-elf"),
            QStringLiteral("Emerald Elf"),
            0x19D8E4A1u,
            QColor(QStringLiteral("#102019"))
        },
        {
            QStringLiteral("frost-princess"),
            QStringLiteral("Frost Princess"),
            0xC46B927Du,
            QColor(QStringLiteral("#162335"))
        },
        {
            QStringLiteral("midnight-vampire"),
            QStringLiteral("Midnight Vampire"),
            0x5E12AF90u,
            QColor(QStringLiteral("#211016"))
        },
        {
            QStringLiteral("moon-catgirl"),
            QStringLiteral("Moon Catgirl"),
            0xD94C7306u,
            QColor(QStringLiteral("#17152a"))
        },
        {
            QStringLiteral("neon-idol"),
            QStringLiteral("Neon Idol"),
            0x2B8F41CDu,
            QColor(QStringLiteral("#181229"))
        },
        {
            QStringLiteral("rose-queen"),
            QStringLiteral("Rose Queen"),
            0xE1735A24u,
            QColor(QStringLiteral("#241116"))
        },
        {
            QStringLiteral("sakura-witch"),
            QStringLiteral("Sakura Witch"),
            0x63ACD8F1u,
            QColor(QStringLiteral("#21152c"))
        },
        {
            QStringLiteral("solar-captain"),
            QStringLiteral("Solar Captain"),
            0xB51E209Au,
            QColor(QStringLiteral("#271b12"))
        },
        {
            QStringLiteral("violet-cosmos"),
            QStringLiteral("Violet Cosmos"),
            0x0F97C6E3u,
            QColor(QStringLiteral("#181329"))
        }
    };

    return themes;
}



const WaifuTheme *find(
    const QString &id
)
{
    for (const WaifuTheme &theme : all()) {
        if (theme.id == id)
            return &theme;
    }

    return nullptr;
}


QPalette darkPalette(
    const QPalette &systemPalette
)
{
    QPalette palette = systemPalette;

    palette.setColor(
        QPalette::Window,
        QColor(45, 45, 45)
    );

    palette.setColor(
        QPalette::WindowText,
        QColor(235, 235, 235)
    );

    palette.setColor(
        QPalette::Base,
        QColor(30, 30, 30)
    );

    palette.setColor(
        QPalette::AlternateBase,
        QColor(38, 38, 38)
    );

    palette.setColor(
        QPalette::Text,
        QColor(235, 235, 235)
    );

    palette.setColor(
        QPalette::Button,
        QColor(45, 45, 45)
    );

    palette.setColor(
        QPalette::ButtonText,
        QColor(235, 235, 235)
    );

    palette.setColor(
        QPalette::Highlight,
        QColor(42, 130, 218)
    );

    palette.setColor(
        QPalette::HighlightedText,
        Qt::white
    );

    palette.setColor(
        QPalette::Disabled,
        QPalette::Text,
        QColor(120, 120, 120)
    );

    palette.setColor(
        QPalette::Disabled,
        QPalette::ButtonText,
        QColor(120, 120, 120)
    );

    return palette;
}

}
