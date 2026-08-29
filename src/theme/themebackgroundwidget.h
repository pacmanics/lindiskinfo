// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPixmap>
#include <QWidget>

class QPaintEvent;
class QResizeEvent;

class ThemeBackgroundWidget final
    : public QWidget
{
public:
    explicit ThemeBackgroundWidget(
        QWidget *parent = nullptr
    );

    QWidget *contentWidget() const noexcept;

    void setTheme(
        const QPixmap &background,
        const QColor &fallbackColor
    );

    void clearTheme();

    bool hasTheme() const noexcept;

protected:
    void paintEvent(
        QPaintEvent *event
    ) override;

    void resizeEvent(
        QResizeEvent *event
    ) override;

private:
    void updateSafeZone();
    void rebuildScaledBackground();
    void applyContentStyle();

    QWidget *m_contentWidget = nullptr;
    QWidget *m_safeZone = nullptr;

    QPixmap m_sourceBackground;
    QPixmap m_scaledBackground;

    QColor m_fallbackColor;

    bool m_themed = false;
    int m_cachedBackgroundHeight = -1;
};
