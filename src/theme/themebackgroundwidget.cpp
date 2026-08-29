// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "themebackgroundwidget.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QtGlobal>

ThemeBackgroundWidget::ThemeBackgroundWidget(
    QWidget *parent
)
    : QWidget(parent)
{
    setAutoFillBackground(false);

    auto *layout =
        new QHBoxLayout(this);

    layout->setContentsMargins(
        0,
        0,
        0,
        0
    );

    layout->setSpacing(0);

    m_contentWidget =
        new QWidget(this);

    m_contentWidget->setObjectName(
        QStringLiteral("themeContentPanel")
    );

    m_contentWidget->setAttribute(
        Qt::WA_StyledBackground,
        true
    );

    m_contentWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
    );

    m_safeZone =
        new QWidget(this);

    m_safeZone->setObjectName(
        QStringLiteral("themeSafeZone")
    );

    m_safeZone->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Expanding
    );

    m_safeZone->setAttribute(
        Qt::WA_TransparentForMouseEvents,
        true
    );

    layout->addWidget(
        m_contentWidget,
        1
    );

    layout->addWidget(
        m_safeZone,
        0
    );

    clearTheme();
}


QWidget *ThemeBackgroundWidget::contentWidget()
    const noexcept
{
    return m_contentWidget;
}


void ThemeBackgroundWidget::setTheme(
    const QPixmap &background,
    const QColor &fallbackColor
)
{
    m_sourceBackground =
        background;

    m_fallbackColor =
        fallbackColor;

    m_themed = true;
    m_cachedBackgroundHeight = -1;

    m_safeZone->show();

    applyContentStyle();
    updateSafeZone();
    rebuildScaledBackground();
    update();
}


void ThemeBackgroundWidget::clearTheme()
{
    m_sourceBackground =
        QPixmap();

    m_scaledBackground =
        QPixmap();

    m_fallbackColor =
        palette().color(
            QPalette::Window
        );

    m_themed = false;
    m_cachedBackgroundHeight = -1;

    if (m_safeZone) {
        m_safeZone->setFixedWidth(0);
        m_safeZone->hide();
    }

    if (m_contentWidget)
        m_contentWidget->setStyleSheet(
            QString()
        );

    update();
}


bool ThemeBackgroundWidget::hasTheme()
    const noexcept
{
    return m_themed;
}


void ThemeBackgroundWidget::paintEvent(
    QPaintEvent *event
)
{
    Q_UNUSED(event)

    QPainter painter(this);

    painter.fillRect(
        rect(),
        m_themed
            ? m_fallbackColor
            : palette().color(
                  QPalette::Window
              )
    );

    if (!m_themed ||
        m_scaledBackground.isNull()) {
        return;
    }

    const int x =
        width() -
        m_scaledBackground.width();

    const int y =
        (
            height() -
            m_scaledBackground.height()
        ) / 2;

    painter.drawPixmap(
        x,
        y,
        m_scaledBackground
    );
}


void ThemeBackgroundWidget::resizeEvent(
    QResizeEvent *event
)
{
    QWidget::resizeEvent(event);

    updateSafeZone();

    if (event->size().height() !=
        m_cachedBackgroundHeight) {
        rebuildScaledBackground();
    }
}


void ThemeBackgroundWidget::updateSafeZone()
{
    if (!m_safeZone)
        return;

    if (!m_themed) {
        m_safeZone->setFixedWidth(0);
        return;
    }

    const int safeWidth =
        qBound(
            440,
            qRound(
                static_cast<double>(width()) *
                0.37
            ),
            650
        );

    m_safeZone->setFixedWidth(
        safeWidth
    );
}


void ThemeBackgroundWidget::rebuildScaledBackground()
{
    m_cachedBackgroundHeight =
        height();

    if (!m_themed ||
        m_sourceBackground.isNull() ||
        height() <= 0) {

        m_scaledBackground =
            QPixmap();

        return;
    }

    m_scaledBackground =
        m_sourceBackground.scaledToHeight(
            height(),
            Qt::SmoothTransformation
        );
}


void ThemeBackgroundWidget::applyContentStyle()
{
    if (!m_contentWidget)
        return;

    if (!m_themed) {
        m_contentWidget->setStyleSheet(
            QString()
        );
        return;
    }

    m_contentWidget->setStyleSheet(
        QStringLiteral(
            "QWidget#themeContentPanel {"
            " background-color: rgba(18,18,20,118);"
            "}"
            "QWidget#themeContentPanel QScrollArea {"
            " background: transparent;"
            " border: none;"
            "}"
            "QWidget#themeContentPanel QTableWidget {"
            " background-color: rgba(22,22,24,145);"
            " alternate-background-color: rgba(38,38,40,128);"
            " gridline-color: rgba(255,255,255,28);"
            " border: 1px solid rgba(255,255,255,42);"
            "}"
            "QWidget#themeContentPanel QHeaderView::section {"
            " background-color: rgba(42,42,44,212);"
            " border-color: rgba(255,255,255,42);"
            "}"
            "QWidget#themeContentPanel QTableCornerButton::section {"
            " background-color: rgba(42,42,44,212);"
            " border-color: rgba(255,255,255,42);"
            "}"
            "QWidget#themeContentPanel QLineEdit {"
            " background-color: rgba(20,20,22,208);"
            " border: 1px solid rgba(255,255,255,52);"
            " border-radius: 2px;"
            "}"
            "QWidget#themeContentPanel QLabel[ldiValueBox=\"true\"] {"
            " background-color: rgba(20,20,22,205);"
            " border: 1px solid rgba(255,255,255,48);"
            " border-radius: 2px;"
            "}"
        )
    );
}
