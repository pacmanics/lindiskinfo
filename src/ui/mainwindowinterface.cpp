// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../theme/themebackgroundwidget.h"
#include "responsivetablelayout.h"

#include <QCryptographicHash>
#include <QVariantList>
#include <QHBoxLayout>
#include <QSaveFile>
#include <QPolygonF>
#include <QPaintEvent>
#include <QPainter>
#include <QLocale>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QFontDatabase>
#include <QCheckBox>
#include <QStringList>
#include <QSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QCloseEvent>
#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QCoreApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFontDialog>
#include <QGuiApplication>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

QLabel *MainWindow::createValueBox()
{
    auto *label =
        new QLabel(QStringLiteral("—"));

    label->setFrameShape(QFrame::StyledPanel);

    label->setProperty(
        "ldiValueBox",
        true
    );

    label->setTextInteractionFlags(
        Qt::TextSelectableByMouse
    );

    label->setMinimumHeight(27);
    label->setMargin(5);

    return label;
}

QLabel *MainWindow::createCaptionLabel()
{
    auto *label = new QLabel;

    label->setAlignment(
        Qt::AlignRight | Qt::AlignVCenter
    );

    return label;
}

void MainWindow::addInfoRow(
    QGridLayout *layout,
    int row,
    int column,
    QLabel *caption,
    QWidget *valueWidget
)
{
    layout->addWidget(
        caption,
        row,
        column
    );

    layout->addWidget(
        valueWidget,
        row,
        column + 1
    );
}

void MainWindow::buildInterface()
{
    m_themeBackground =
        new ThemeBackgroundWidget;

    QWidget *central =
        m_themeBackground->contentWidget();

    auto *root =
        new QVBoxLayout(central);

    root->setContentsMargins(
        8,
        6,
        8,
        6
    );

    root->setSpacing(6);

    auto *driveScroll =
        new QScrollArea;

    driveScroll->setWidgetResizable(true);

    driveScroll->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
    );

    driveScroll->setVerticalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
    );

    driveScroll->setFrameShape(
        QFrame::NoFrame
    );

    driveScroll->setAttribute(
        Qt::WA_TranslucentBackground,
        true
    );

    driveScroll->viewport()->setAutoFillBackground(
        false
    );

    driveScroll->viewport()->setStyleSheet(
        QStringLiteral(
            "background: transparent;"
        )
    );

    driveScroll->setFixedHeight(66);

    m_driveBar = new QWidget;

    m_driveBar->setAttribute(
        Qt::WA_TranslucentBackground,
        true
    );

    m_driveBar->setStyleSheet(
        QStringLiteral(
            "background: transparent;"
        )
    );

    m_driveLayout =
        new QHBoxLayout(m_driveBar);

    m_driveLayout->setContentsMargins(
        0,
        0,
        0,
        0
    );

    m_driveLayout->setSpacing(4);

    driveScroll->setWidget(m_driveBar);

    root->addWidget(driveScroll);

    m_titleLabel =
        new QLabel(QStringLiteral("LinDiskInfo"));

    QFont titleFont =
        m_titleLabel->font();

    titleFont.setPointSize(
        titleFont.pointSize() + 7
    );

    titleFont.setBold(false);

    m_titleLabel->setFont(titleFont);

    m_titleLabel->setAlignment(
        Qt::AlignCenter
    );

    root->addWidget(m_titleLabel);

    auto *mainGrid =
        new QGridLayout;

    mainGrid->setHorizontalSpacing(10);
    mainGrid->setVerticalSpacing(7);

    auto *leftColumn =
        new QVBoxLayout;

    leftColumn->setSpacing(7);

    m_healthCaption =
        new QLabel;

    m_healthCaption->setAlignment(
        Qt::AlignCenter
    );

    m_healthFrame =
        new QFrame;

    m_healthFrame->setObjectName(
        QStringLiteral("healthCard")
    );

    m_healthFrame->setMinimumSize(
        130,
        66
    );

    auto *healthLayout =
        new QVBoxLayout(m_healthFrame);

    healthLayout->setContentsMargins(
        8,
        6,
        8,
        6
    );

    m_healthValue =
        new QLabel(QStringLiteral("—"));

    QFont healthFont =
        m_healthValue->font();

    healthFont.setPointSize(
        healthFont.pointSize() + 5
    );

    healthFont.setBold(true);

    m_healthValue->setFont(healthFont);

    m_healthValue->setAlignment(
        Qt::AlignCenter
    );

    healthLayout->addWidget(
        m_healthValue
    );

    m_temperatureCaption =
        new QLabel;

    m_temperatureCaption->setAlignment(
        Qt::AlignCenter
    );

    m_temperatureFrame =
        new QFrame;

    m_temperatureFrame->setObjectName(
        QStringLiteral("temperatureCard")
    );

    m_temperatureFrame->setMinimumSize(
        130,
        50
    );

    auto *temperatureLayout =
        new QVBoxLayout(
            m_temperatureFrame
        );

    temperatureLayout->setContentsMargins(
        8,
        4,
        8,
        4
    );

    m_temperatureValue =
        new QLabel(QStringLiteral("—"));

    QFont temperatureFont =
        m_temperatureValue->font();

    temperatureFont.setPointSize(
        temperatureFont.pointSize() + 7
    );

    temperatureFont.setBold(true);

    m_temperatureValue->setFont(
        temperatureFont
    );

    m_temperatureValue->setAlignment(
        Qt::AlignCenter
    );

    temperatureLayout->addWidget(
        m_temperatureValue
    );

    leftColumn->addWidget(
        m_healthCaption
    );

    leftColumn->addWidget(
        m_healthFrame
    );

    leftColumn->addSpacing(6);

    leftColumn->addWidget(
        m_temperatureCaption
    );

    leftColumn->addWidget(
        m_temperatureFrame
    );

    leftColumn->addStretch();

    mainGrid->addLayout(
        leftColumn,
        0,
        0,
        1,
        1
    );

    auto *infoGrid =
        new QGridLayout;

    infoGrid->setHorizontalSpacing(6);
    infoGrid->setVerticalSpacing(3);

    m_firmwareCaption = createCaptionLabel();
    m_interfaceCaption = createCaptionLabel();
    m_transferCaption = createCaptionLabel();
    m_standardCaption = createCaptionLabel();
    m_featuresCaption = createCaptionLabel();

    m_firmwareValue = createValueBox();
    m_interfaceValue = createValueBox();
    m_transferValue = createValueBox();
    m_standardValue = createValueBox();
    m_featuresValue = createValueBox();

    m_serialCaption =
        createCaptionLabel();

    auto *serialContainer =
        new QWidget;

    auto *serialLayout =
        new QHBoxLayout(serialContainer);

    serialLayout->setContentsMargins(
        0,
        0,
        0,
        0
    );

    serialLayout->setSpacing(2);

    m_serialEdit =
        new QLineEdit;

    m_serialEdit->setReadOnly(true);

    m_serialEdit->setEchoMode(
        QLineEdit::Password
    );

    m_serialEdit->setMinimumHeight(27);

    m_serialButton =
        new QToolButton;

    m_serialButton->setFixedSize(
        30,
        27
    );

    m_serialButton->setAutoRaise(true);

    connect(
        m_serialButton,
        &QToolButton::clicked,
        this,
        &MainWindow::toggleSerialVisibility
    );

    serialLayout->addWidget(
        m_serialEdit,
        1
    );

    serialLayout->addWidget(
        m_serialButton
    );

    m_readsCaption = createCaptionLabel();
    m_writesCaption = createCaptionLabel();
    m_rotationCaption = createCaptionLabel();
    m_powerCyclesCaption = createCaptionLabel();
    m_powerHoursCaption = createCaptionLabel();

    m_readsValue = createValueBox();
    m_writesValue = createValueBox();
    m_rotationValue = createValueBox();
    m_powerCyclesValue = createValueBox();
    m_powerHoursValue = createValueBox();

    addInfoRow(
        infoGrid,
        0,
        0,
        m_firmwareCaption,
        m_firmwareValue
    );

    addInfoRow(
        infoGrid,
        1,
        0,
        m_serialCaption,
        serialContainer
    );

    addInfoRow(
        infoGrid,
        2,
        0,
        m_interfaceCaption,
        m_interfaceValue
    );

    addInfoRow(
        infoGrid,
        3,
        0,
        m_transferCaption,
        m_transferValue
    );

    addInfoRow(
        infoGrid,
        4,
        0,
        m_standardCaption,
        m_standardValue
    );

    addInfoRow(
        infoGrid,
        5,
        0,
        m_featuresCaption,
        m_featuresValue
    );

    addInfoRow(
        infoGrid,
        0,
        2,
        m_readsCaption,
        m_readsValue
    );

    addInfoRow(
        infoGrid,
        1,
        2,
        m_writesCaption,
        m_writesValue
    );

    addInfoRow(
        infoGrid,
        2,
        2,
        m_rotationCaption,
        m_rotationValue
    );

    addInfoRow(
        infoGrid,
        3,
        2,
        m_powerCyclesCaption,
        m_powerCyclesValue
    );

    addInfoRow(
        infoGrid,
        4,
        2,
        m_powerHoursCaption,
        m_powerHoursValue
    );

    infoGrid->setColumnStretch(
        1,
        3
    );

    infoGrid->setColumnStretch(
        3,
        2
    );

    mainGrid->addLayout(
        infoGrid,
        0,
        1,
        1,
        1
    );

    mainGrid->setColumnStretch(
        1,
        1
    );

    root->addLayout(mainGrid);

    m_table =
        new QTableWidget;

    m_table->setColumnCount(8);

    m_table->verticalHeader()->setVisible(false);

    m_table->setAlternatingRowColors(true);

    m_table->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );

    m_table->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );

    m_table->setShowGrid(true);

    m_table->horizontalHeader()
        ->setStretchLastSection(false);

    m_tableLayoutController =
        new ResponsiveTableLayout(
            m_table,
            this
        );

    root->addWidget(
        m_table,
        1
    );

    m_statusLabel =
        new QLabel;

    statusBar()->addWidget(
        m_statusLabel,
        1
    );

    setCentralWidget(
        m_themeBackground
    );

    setHealth(
        HealthState::Unknown
    );

    setTemperature(-1);

    updateSerialButton();
}
