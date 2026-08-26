// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
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

QString protocolName(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    if (data.contains(
            QStringLiteral("ata_smart_attributes")
        )) {
        return QStringLiteral("Serial ATA");
    }

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {
        return QStringLiteral("NVM Express");
    }

    const QString protocol =
        data.value(QStringLiteral("device"))
            .toObject()
            .value(QStringLiteral("protocol"))
            .toString();

    if (!protocol.isEmpty())
        return protocol;

    if (!drive.protocol.isEmpty())
        return drive.protocol;

    return QStringLiteral("—");
}

QString versionString(
    const QJsonObject &data
)
{
    const QJsonObject nvme =
        data.value(
            QStringLiteral("nvme_version")
        ).toObject();

    const QString nvmeString =
        nvme.value(
            QStringLiteral("string")
        ).toString();

    if (!nvmeString.isEmpty())
        return QStringLiteral("NVMe %1").arg(nvmeString);

    const QJsonObject ata =
        data.value(
            QStringLiteral("ata_version")
        ).toObject();

    const QString ataString =
        ata.value(
            QStringLiteral("string")
        ).toString();

    if (!ataString.isEmpty())
        return ataString;

    return QStringLiteral("—");
}

QString transferString(
    const QJsonObject &data
)
{
    const QJsonObject speed =
        data.value(
            QStringLiteral("interface_speed")
        ).toObject();

    const QString current =
        speed.value(
            QStringLiteral("current")
        ).toObject()
        .value(
            QStringLiteral("string")
        ).toString();

    const QString maximum =
        speed.value(
            QStringLiteral("max")
        ).toObject()
        .value(
            QStringLiteral("string")
        ).toString();

    if (!current.isEmpty() && !maximum.isEmpty())
        return current + QStringLiteral(" | ") + maximum;

    if (!current.isEmpty())
        return current;

    return QStringLiteral("—");
}

}


// ============================================================
// Lightweight native Qt history graph.
// No QtCharts dependency.
// ============================================================

class LinDiskHistoryPlot final
    : public QWidget
{
public:
    explicit LinDiskHistoryPlot(
        QWidget *parent = nullptr
    )
        : QWidget(parent)
    {
        setMinimumHeight(360);
    }

    void setSamples(
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

protected:
    void paintEvent(
        QPaintEvent *event
    ) override
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

            minX =
                std::min(
                    minX,
                    point.x()
                );

            maxX =
                std::max(
                    maxX,
                    point.x()
                );

            minY =
                std::min(
                    minY,
                    point.y()
                );

            maxY =
                std::max(
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

            if (!m_unit.isEmpty())
                label +=
                    QStringLiteral(" ") +
                    m_unit;

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

        if (line.size() > 1)
            painter.drawPolyline(line);
        else
            painter.drawEllipse(
                line.first(),
                3.0,
                3.0
            );
    }

private:
    QVector<QPointF> m_samples;

    QString m_unit;
    QString m_emptyText;

    bool m_percentageRange = false;
};


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_backend(new SmartctlBackend(this))
{
    QSettings settings;

    m_systemPalette = qApp->palette();
    m_baseFont = qApp->font();

    const QString savedLanguage =
        settings.value(
            QStringLiteral("language"),
            QStringLiteral("en")
        ).toString();

    if (savedLanguage == QStringLiteral("de"))
        m_language = Language::German;

        const QString savedTheme =
        settings.value(
            QStringLiteral("theme"),
            QStringLiteral("system")
        ).toString();

    m_darkMode =
        savedTheme == QStringLiteral("dark") ||
        settings.value(
            QStringLiteral("darkMode"),
            false
        ).toBool();

const QString savedTemperatureUnit =
        settings.value(
            QStringLiteral("temperatureUnit"),
            QStringLiteral("celsius")
        ).toString();

    if (savedTemperatureUnit ==
        QStringLiteral("fahrenheit")) {
        m_temperatureUnit =
            TemperatureUnit::Fahrenheit;
    }

    m_temperatureCaution =
        settings.value(
            QStringLiteral(
                "temperatureCaution"
            ),
            55
        ).toInt();

    m_temperatureBad =
        settings.value(
            QStringLiteral(
                "temperatureBad"
            ),
            70
        ).toInt();

    m_nvmeCautionRemaining =
        settings.value(
            QStringLiteral(
                "nvmeCautionRemaining"
            ),
            10
        ).toInt();

    m_nvmeBadRemaining =
        settings.value(
            QStringLiteral(
                "nvmeBadRemaining"
            ),
            0
        ).toInt();

    m_ataSectorCautionCount =
        settings.value(
            QStringLiteral(
                "ataSectorCautionCount"
            ),
            1
        ).toULongLong();

    m_startupDelaySeconds =
        settings.value(
            QStringLiteral(
                "startupDelaySeconds"
            ),
            0
        ).toInt();

    m_driveSortMethod =
        settings.value(
            QStringLiteral(
                "driveSortMethod"
            ),
            QStringLiteral("default")
        ).toString();

    m_displayDriveLimit =
        settings.value(
            QStringLiteral(
                "displayDriveLimit"
            ),
            0
        ).toInt();

    m_trayBehavior =
        settings.value(
            QStringLiteral(
                "trayBehavior"
            ),
            QStringLiteral("hide")
        ).toString();

    m_hideNoSmart =
        settings.value(
            QStringLiteral(
                "hideNoSmart"
            ),
            false
        ).toBool();

    m_aamApmAutoEnabled =
        settings.value(
            QStringLiteral(
                "aamApmAutoAdjustment"
            ),
            false
        ).toBool();

    m_historyEnabled =
        settings.value(
            QStringLiteral(
                "historyEnabled"
            ),
            true
        ).toBool();

    loadHistory();

    m_ataPassThroughEnabled =
        settings.value(
            QStringLiteral(
                "ataPassThrough"
            ),
            true
        ).toBool();

    m_externalStorageEnabled =
        settings.value(
            QStringLiteral(
                "externalStorage"
            ),
            true
        ).toBool();

    m_megaRaidEnabled =
        settings.value(
            QStringLiteral(
                "megaRaidPhysicalDrives"
            ),
            true
        ).toBool();

    applyTheme();

    setWindowTitle(
        QStringLiteral("LinDiskInfo %1")
            .arg(QCoreApplication::applicationVersion())
    );
    resize(1050, 720);
    setMinimumSize(860, 600);

    buildInterface();
    buildMenus();
    applyLanguage();

    const QByteArray savedGeometry =
        settings.value(
            QStringLiteral(
                "windowGeometry"
            )
        ).toByteArray();

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(
            savedGeometry
        );
    }

    connect(
        m_backend,
        &SmartctlBackend::helperReady,
        this,
        [this]
        {
            refreshDevices();
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::helperFailed,
        this,
        [this](const QString &message)
        {
            QMessageBox::critical(
                this,
                QStringLiteral("LinDiskInfo"),
                tx(
                    "Authorization failed or the privileged helper could not be started.\n\n",
                    "Die Autorisierung ist fehlgeschlagen oder der privilegierte Helper konnte nicht gestartet werden.\n\n"
                ) + message
            );

            setStatus(
                tx(
                    "SMART access unavailable.",
                    "SMART-Zugriff nicht verfügbar."
                )
            );
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::scanFinished,
        this,
        [this](const QVector<DriveInfo> &drives)
        {
            QString previousDriveIdentity;
            QString previousDrivePath;

            if (m_selectedDrive >= 0 &&
                m_selectedDrive <
                    m_drives.size()) {

                const DriveInfo &selected =
                    m_drives.at(
                        m_selectedDrive
                    );

                previousDriveIdentity =
                    lindiskinfoDriveIdentity(
                        selected
                    );

                previousDrivePath =
                    selected.name;

            } else if (m_hasCurrentData) {

                previousDriveIdentity =
                    lindiskinfoDriveIdentity(
                        m_currentDrive
                    );

                previousDrivePath =
                    m_currentDrive.name;
            }

            if (previousDriveIdentity.isEmpty()) {
                previousDriveIdentity =
                    QSettings().value(
                        QStringLiteral(
                            "lastDriveIdentity"
                        )
                    ).toString();
            }

            // Legacy path-only preference compatibility.
            if (previousDrivePath.isEmpty()) {
                previousDrivePath =
                    QSettings().value(
                        QStringLiteral(
                            "lastDrive"
                        )
                    ).toString();
            }

            const QHash<QString, QJsonObject>
                previousData =
                    m_driveData;

            QVector<DriveInfo>
                filteredDrives;

            filteredDrives.reserve(
                drives.size()
            );

            for (const DriveInfo &candidate :
                 drives) {

                const QString transport =
                    candidate.transport
                        .trimmed()
                        .toLower();

                const QString type =
                    candidate.type
                        .trimmed()
                        .toLower();

                const bool external =
                    transport ==
                        QStringLiteral("usb") ||
                    transport ==
                        QStringLiteral("ieee1394") ||
                    transport ==
                        QStringLiteral("firewire") ||
                    transport ==
                        QStringLiteral("sbp");

                if (!m_externalStorageEnabled &&
                    external) {
                    continue;
                }

                if (!m_megaRaidEnabled &&
                    type.startsWith(
                        QStringLiteral(
                            "megaraid,"
                        )
                    )) {
                    continue;
                }

                filteredDrives.append(
                    candidate
                );
            }

            m_drives =
                filteredDrives;

            m_driveData.clear();

            for (const DriveInfo &drive :
                 m_drives) {

                if (previousData.contains(
                        lindiskinfoDriveIdentity(drive)
                    )) {

                    m_driveData.insert(
                        lindiskinfoDriveIdentity(drive),
                        previousData.value(
                            lindiskinfoDriveIdentity(drive)
                        )
                    );
                }
            }

            sortDrives();
            rebuildDriveButtons();
            applyDriveButtonLimit();
            rebuildDiskMenu();

            rebuildTrayMenu();
            updateTrayPresentation();

            if (m_drives.isEmpty()) {
                setStatus(
                    tx(
                        "No drives found.",
                        "Keine Laufwerke gefunden."
                    )
                );

                return;
            }

            int targetIndex = -1;
            int legacyPathIndex = -1;
            int legacyPathMatches = 0;

            for (int i = 0;
                 i < m_drives.size();
                 ++i) {

                const DriveInfo &candidate =
                    m_drives.at(i);

                if (!previousDriveIdentity.isEmpty() &&
                    lindiskinfoDriveIdentity(
                        candidate
                    ) ==
                        previousDriveIdentity) {

                    targetIndex = i;
                    break;
                }

                if (!previousDrivePath.isEmpty() &&
                    candidate.name ==
                        previousDrivePath) {

                    legacyPathIndex = i;
                    ++legacyPathMatches;
                }
            }

            // Old path-only setting is only safe when the
            // current path uniquely identifies one drive.
            if (targetIndex < 0 &&
                legacyPathMatches == 1) {

                targetIndex =
                    legacyPathIndex;
            }

            if (targetIndex < 0)
                targetIndex = 0;

            selectDrive(targetIndex);

            for (const DriveInfo &drive : m_drives)
                m_backend->requestDeviceData(drive);
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::scanFailed,
        this,
        [this](const QString &message)
        {
            QMessageBox::warning(
                this,
                QStringLiteral("LinDiskInfo"),
                message
            );
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::deviceDataReady,
        this,
        [this](
            const DriveInfo &drive,
            const QJsonObject &data
        )
        {
            m_driveData.insert(
                lindiskinfoDriveIdentity(drive),
                data
            );

            maybeAutoAdjustAta(
                drive,
                data
            );

            recordHistorySample(
                drive,
                data
            );

            updateTrayPresentation();
            rebuildTrayMenu();

            if (data.contains(
                    QStringLiteral(
                        "lindiskinfo_maintenance"
                    )
                )) {

                showMaintenanceResult(
                    drive,
                    data
                );
            }

            updateDriveButton(
                drive,
                data
            );


            rebuildDiskMenu();

            if (m_hideNoSmart ||
                m_driveSortMethod ==
                    QStringLiteral("health") ||
                m_driveSortMethod ==
                    QStringLiteral("temperature")) {

                refreshDrivePresentation();
            }

if (m_selectedDrive >= 0 &&
                m_selectedDrive < m_drives.size() &&
                lindiskinfoDriveIdentity(
                    m_drives.at(
                        m_selectedDrive
                    )
                ) ==
                lindiskinfoDriveIdentity(
                    drive
                )) {

                renderDevice(
                    drive,
                    data
                );
            }
        }
    );

    connect(
        m_backend,
        &SmartctlBackend::deviceDataFailed,
        this,
        [this](
            const DriveInfo &drive,
            const QString &message
        )
        {
            if (m_selectedDrive >= 0 &&
                m_selectedDrive < m_drives.size() &&
                lindiskinfoDriveIdentity(
                    m_drives.at(
                        m_selectedDrive
                    )
                ) ==
                lindiskinfoDriveIdentity(
                    drive
                )) {

                setStatus(
                    tx(
                        "Unable to read SMART data.",
                        "SMART-Daten konnten nicht gelesen werden."
                    )
                );
            }

            QMessageBox::warning(
                this,
                QStringLiteral("LinDiskInfo"),
                driveDisplayIdentifier(drive) + QStringLiteral("\n\n") + message
            );
        }
    );

    setStatus(
        tx(
            "Waiting for authorization...",
            "Warte auf Autorisierung..."
        )
    );

    const bool autostartLaunch =
        QCoreApplication::arguments()
            .contains(
                QStringLiteral(
                    "--autostart"
                )
            );

    if (autostartLaunch &&
        m_startupDelaySeconds > 0) {

        setStatus(
            tx(
                "Startup delayed by %1 seconds...",
                "Systemstart um %1 Sekunden verzögert..."
            ).arg(
                m_startupDelaySeconds
            )
        );

        QTimer::singleShot(
            m_startupDelaySeconds * 1000,
            this,
            [this]
            {
                m_backend->start();
            }
        );
    } else {
        m_backend->start();
    }
}

QString MainWindow::tx(
    const char *english,
    const char *german
) const
{
    return m_language == Language::German
        ? QString::fromUtf8(german)
        : QString::fromUtf8(english);
}

void MainWindow::applyTheme()
{
    if (!m_darkMode) {
        qApp->setPalette(m_systemPalette);
        return;
    }

    QPalette palette = m_systemPalette;

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

    qApp->setPalette(palette);
}

QLabel *MainWindow::createValueBox()
{
    auto *label =
        new QLabel(QStringLiteral("—"));

    label->setFrameShape(QFrame::StyledPanel);
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
    auto *central = new QWidget;

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

    driveScroll->setFixedHeight(66);

    m_driveBar = new QWidget;

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

    setCentralWidget(central);

    setHealth(
        HealthState::Unknown
    );

    setTemperature(-1);

    updateSerialButton();
}

void MainWindow::buildMenus()
{
    QSettings settings;

    auto disabledAction =
        [this](QMenu *menu, const QString &name)
        {
            QAction *action =
                menu->addAction(QString());

            action->setObjectName(name);
            action->setVisible(false);

            return action;
        };

    auto disabledMenu =
        [this](QMenu *parent, const QString &name)
        {
            QMenu *menu =
                parent->addMenu(QString());

            menu->setObjectName(name);
            menu->menuAction()->setVisible(false);

            return menu;
        };

    m_fileMenu =
        menuBar()->addMenu(QString());

    m_saveTextAction =
        m_fileMenu->addAction(QString());

    m_saveTextAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+T"))
    );

    connect(
        m_saveTextAction,
        &QAction::triggered,
        this,
        &MainWindow::saveTextReport
    );

    m_saveImageAction =
        m_fileMenu->addAction(QString());

    m_saveImageAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+S"))
    );

    connect(
        m_saveImageAction,
        &QAction::triggered,
        this,
        &MainWindow::saveImage
    );

    m_fileMenu->addSeparator();

    m_quitAction =
        m_fileMenu->addAction(QString());

    m_quitAction->setShortcut(
        QKeySequence(QStringLiteral("Alt+F4"))
    );

    connect(
        m_quitAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_forceQuit = true;
            close();
        }
    );

    m_editMenu =
        menuBar()->addMenu(QString());

    m_copyAction =
        m_editMenu->addAction(QString());

    m_copyAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+C"))
    );

    connect(
        m_copyAction,
        &QAction::triggered,
        this,
        &MainWindow::copyReport
    );

    m_copyOptionsMenu =
        m_editMenu->addMenu(QString());

    m_copyIdentifyAction =
        m_copyOptionsMenu->addAction(
            QStringLiteral("IDENTIFY_DEVICE")
        );

    m_copySmartDataAction =
        m_copyOptionsMenu->addAction(
            QStringLiteral("SMART_READ_DATA")
        );

    m_copyThresholdAction =
        m_copyOptionsMenu->addAction(
            QStringLiteral("SMART_READ_THRESHOLD")
        );

    m_copyOptionsMenu->addSeparator();

    m_copyAsciiAction =
        m_copyOptionsMenu->addAction(QString());

    for (QAction *action :
         {
             m_copyIdentifyAction,
             m_copySmartDataAction,
             m_copyThresholdAction,
             m_copyAsciiAction
         }) {

        action->setCheckable(true);
    }

    m_copyIdentifyAction->setChecked(true);
    m_copySmartDataAction->setChecked(true);
    m_copyThresholdAction->setChecked(true);

    m_settingsMenu =
        menuBar()->addMenu(QString());

    m_refreshAction =
        m_settingsMenu->addAction(QString());

    m_refreshAction->setShortcut(
        QKeySequence(QStringLiteral("F5"))
    );

    connect(
        m_refreshAction,
        &QAction::triggered,
        this,
        &MainWindow::refreshAllData
    );

    m_autoRefreshMenu =
        m_settingsMenu->addMenu(QString());

    auto *autoRefreshGroup =
        new QActionGroup(this);

    autoRefreshGroup->setExclusive(true);

    m_autoRefreshMinutes =
        settings.value(
            QStringLiteral("autoRefreshMinutes"),
            10
        ).toInt();

    for (int minutes :
         {
             1,
             3,
             5,
             10,
             30,
             60,
             120,
             180,
             360,
             720,
             1440,
             0
         }) {

        QAction *action =
            m_autoRefreshMenu->addAction(
                minutes == 0
                    ? QString()
                    : QStringLiteral("%1 min").arg(minutes)
            );

        action->setCheckable(true);
        action->setData(minutes);

        if (minutes == m_autoRefreshMinutes)
            action->setChecked(true);

        autoRefreshGroup->addAction(action);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, minutes]
            {
                setAutoRefreshInterval(minutes);
                applyLanguage();
            }
        );
    }

    m_autoRefreshTargetMenu =
        m_settingsMenu->addMenu(QString());

    QAction *selectAll =
        m_autoRefreshTargetMenu->addAction(QString());

    selectAll->setObjectName(
        QStringLiteral("selectAllRefreshTargets")
    );

    QAction *deselectAll =
        m_autoRefreshTargetMenu->addAction(QString());

    deselectAll->setObjectName(
        QStringLiteral("deselectAllRefreshTargets")
    );

    connect(
        selectAll,
        &QAction::triggered,
        this,
        [this]
        {
            m_autoRefreshAllDrives = true;

            QSettings().setValue(
                QStringLiteral("autoRefreshAllDrives"),
                true
            );
        }
    );

    connect(
        deselectAll,
        &QAction::triggered,
        this,
        [this]
        {
            m_autoRefreshAllDrives = false;

            QSettings().setValue(
                QStringLiteral("autoRefreshAllDrives"),
                false
            );
        }
    );

    m_autoRefreshAllDrives =
        settings.value(
            QStringLiteral("autoRefreshAllDrives"),
            true
        ).toBool();

    m_rereadAction =
        m_settingsMenu->addAction(QString());

    m_rereadAction->setShortcut(
        QKeySequence(QStringLiteral("F6"))
    );

    connect(
        m_rereadAction,
        &QAction::triggered,
        this,
        &MainWindow::refreshDevices
    );

    m_liveDetectionAction =
        m_settingsMenu->addAction(
            QString()
        );

    m_liveDetectionAction->setCheckable(
        true
    );

    m_liveDetectionAction->setChecked(
        settings.value(
            QStringLiteral(
                "liveDeviceDetection"
            ),
            true
        ).toBool()
    );

    connect(
        m_liveDetectionAction,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            QSettings().setValue(
                QStringLiteral(
                    "liveDeviceDetection"
                ),
                enabled
            );
        }
    );

    m_diagramAction =
        disabledAction(
            m_settingsMenu,
            QStringLiteral("diagramAction")
        );

    m_settingsMenu->addSeparator();

    m_hideSerialAction =
        m_settingsMenu->addAction(QString());

    m_hideSerialAction->setCheckable(true);
    m_hideSerialAction->setChecked(true);

    connect(
        m_hideSerialAction,
        &QAction::toggled,
        this,
        [this](bool hidden)
        {
            m_serialVisible = !hidden;

            m_serialEdit->setEchoMode(
                m_serialVisible
                    ? QLineEdit::Normal
                    : QLineEdit::Password
            );

            updateSerialButton();
        }
    );

    m_showTrayAction =
        m_settingsMenu->addAction(QString());

    m_showTrayAction->setCheckable(true);

    m_showTrayAction->setChecked(
        settings.value(
            QStringLiteral("showInTray"),
            false
        ).toBool()
    );

    m_startWithSystemAction =
        m_settingsMenu->addAction(QString());

    m_startWithSystemAction->setCheckable(true);

    m_startWithSystemAction->setChecked(
        QFile::exists(autostartPath())
    );

    m_settingsMenu->addSeparator();

    m_advancedOptionsMenu =
        m_settingsMenu->addMenu(QString());

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("aamApmManagementAction")
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("aamApmAutoAction")
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("stateSettingsAction")
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("temperatureWarningAction")
    );

    m_advancedOptionsMenu->addSeparator();

    m_temperatureMenu =
        m_advancedOptionsMenu->addMenu(QString());

    m_temperatureGroup =
        new QActionGroup(this);

    m_temperatureGroup->setExclusive(true);

    m_celsiusAction =
        m_temperatureMenu->addAction(
            QStringLiteral("Celsius (°C)")
        );

    m_fahrenheitAction =
        m_temperatureMenu->addAction(
            QStringLiteral("Fahrenheit (°F)")
        );

    m_celsiusAction->setCheckable(true);
    m_fahrenheitAction->setCheckable(true);

    m_temperatureGroup->addAction(
        m_celsiusAction
    );

    m_temperatureGroup->addAction(
        m_fahrenheitAction
    );

    m_celsiusAction->setChecked(
        m_temperatureUnit ==
        TemperatureUnit::Celsius
    );

    m_fahrenheitAction->setChecked(
        m_temperatureUnit ==
        TemperatureUnit::Fahrenheit
    );

    connect(
        m_celsiusAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_temperatureUnit =
                TemperatureUnit::Celsius;

            QSettings().setValue(
                QStringLiteral("temperatureUnit"),
                QStringLiteral("celsius")
            );

            if (m_hasCurrentData)
                renderDevice(
                    m_currentDrive,
                    m_currentData
                );

            for (const DriveInfo &drive : m_drives) {
                if (m_driveData.contains(lindiskinfoDriveIdentity(drive)))
                    updateDriveButton(
                        drive,
                        m_driveData.value(lindiskinfoDriveIdentity(drive))
                    );
            }
        }
    );

    connect(
        m_fahrenheitAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_temperatureUnit =
                TemperatureUnit::Fahrenheit;

            QSettings().setValue(
                QStringLiteral("temperatureUnit"),
                QStringLiteral("fahrenheit")
            );

            if (m_hasCurrentData)
                renderDevice(
                    m_currentDrive,
                    m_currentData
                );

            for (const DriveInfo &drive : m_drives) {
                if (m_driveData.contains(lindiskinfoDriveIdentity(drive)))
                    updateDriveButton(
                        drive,
                        m_driveData.value(lindiskinfoDriveIdentity(drive))
                    );
            }
        }
    );

    m_autoDetectionMenu =
        m_advancedOptionsMenu->addMenu(QString());

    auto *autoDetectionGroup =
        new QActionGroup(this);

    autoDetectionGroup->setExclusive(true);

    m_autoDetectionSeconds =
        settings.value(
            QStringLiteral("autoDetectionSeconds"),
            0
        ).toInt();

    for (int seconds : {5, 10, 20, 30, 0}) {
        QAction *action =
            m_autoDetectionMenu->addAction(
                seconds == 0
                    ? QString()
                    : QStringLiteral("%1 s").arg(seconds)
            );

        action->setCheckable(true);
        action->setData(seconds);

        if (seconds == m_autoDetectionSeconds)
            action->setChecked(true);

        autoDetectionGroup->addAction(action);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, seconds]
            {
                setAutoDetectionInterval(seconds);
                applyLanguage();
            }
        );
    }

    m_rawValuesMenu =
        m_advancedOptionsMenu->addMenu(QString());

    m_showRawAction =
        m_rawValuesMenu->addAction(QString());

    m_showRawAction->setCheckable(true);

    m_showRawAction->setChecked(
        settings.value(
            QStringLiteral("showRawValues"),
            false
        ).toBool()
    );

    connect(
        m_showRawAction,
        &QAction::toggled,
        this,
        [this](bool checked)
        {
            QSettings().setValue(
                QStringLiteral("showRawValues"),
                checked
            );

            updateRawColumn();
        }
    );

    m_rawValuesMenu->addSeparator();

    QAction *hexAction =
        m_rawValuesMenu->addAction(
            QStringLiteral("16 [HEX]")
        );

    hexAction->setCheckable(true);
    hexAction->setChecked(true);
    hexAction->setVisible(false);

    for (const QString &label :
         {
             QStringLiteral("10 [DEC]"),
             QStringLiteral("10 [DEC] - 2byte"),
             QStringLiteral("10 [DEC] - 1byte")
         }) {

        QAction *action =
            m_rawValuesMenu->addAction(label);

        action->setVisible(false);
    }

    QMenu *startupDelayMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("startupDelayMenu")
        );

    for (int seconds :
         {
             0, 5, 10, 15, 20, 30, 40,
             50, 60, 90, 120, 150,
             180, 210, 240
         }) {

        startupDelayMenu->addAction(
            QStringLiteral("%1 s").arg(seconds)
        );
    }

    QMenu *trayBehaviorMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("trayBehaviorMenu")
        );

    trayBehaviorMenu->addAction(QString());
    trayBehaviorMenu->addAction(QString());

    QMenu *driveSortMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("driveSortMenu")
        );

    driveSortMenu->addAction(QString());
    driveSortMenu->addAction(QString());

    QMenu *displayDrivesMenu =
        disabledMenu(
            m_advancedOptionsMenu,
            QStringLiteral("displayDrivesMenu")
        );

    displayDrivesMenu->addAction(
        QStringLiteral("8")
    );

    displayDrivesMenu->addAction(
        QStringLiteral("16")
    );

    m_advancedOptionsMenu->addSeparator();

    for (const QString &name :
         {
             QStringLiteral("advancedDriveSearchAction"),
             QStringLiteral("ataPassThroughAction"),
             QStringLiteral("usbIeeeAction"),
             QStringLiteral("intelAmdRaidAction"),
             QStringLiteral("amdRaidXpertAction"),
             QStringLiteral("megaRaidAction"),
             QStringLiteral("intelVrocAction")
         }) {

        disabledAction(
            m_advancedOptionsMenu,
            name
        );
    }

    m_advancedOptionsMenu->addSeparator();

    m_hideSmartInfoAction =
        m_advancedOptionsMenu->addAction(QString());

    m_hideSmartInfoAction->setCheckable(true);

    m_hideSmartInfoAction->setChecked(
        settings.value(
            QStringLiteral("hideSmartInfo"),
            false
        ).toBool()
    );

    connect(
        m_hideSmartInfoAction,
        &QAction::toggled,
        this,
        [this](bool hidden)
        {
            QSettings().setValue(
                QStringLiteral("hideSmartInfo"),
                hidden
            );

            m_table->setVisible(!hidden);
        }
    );

    disabledAction(
        m_advancedOptionsMenu,
        QStringLiteral("hideNoSmartAction")
    );

    m_viewMenu =
        menuBar()->addMenu(QString());

    m_zoomMenu =
        m_viewMenu->addMenu(QStringLiteral("Zoom"));

    auto *zoomGroup =
        new QActionGroup(this);

    zoomGroup->setExclusive(true);

    m_zoomPercent =
        settings.value(
            QStringLiteral("zoomPercent"),
            100
        ).toInt();

    for (int percent :
         {
             100,
             125,
             150,
             200,
             250,
             300
         }) {

        QAction *action =
            m_zoomMenu->addAction(
                QStringLiteral("%1%").arg(percent)
            );

        action->setCheckable(true);
        action->setData(percent);

        if (percent == m_zoomPercent)
            action->setChecked(true);

        zoomGroup->addAction(action);

        connect(
            action,
            &QAction::triggered,
            this,
            [this, percent]
            {
                setZoomPercent(percent);
            }
        );
    }

    m_fontAction =
        m_viewMenu->addAction(QString());

    connect(
        m_fontAction,
        &QAction::triggered,
        this,
        [this]
        {
            bool ok = false;

            const QFont font =
                QFontDialog::getFont(
                    &ok,
                    m_baseFont,
                    this
                );

            if (!ok)
                return;

            m_baseFont = font;

            QSettings().setValue(
                QStringLiteral("font"),
                font.toString()
            );

            setZoomPercent(m_zoomPercent);
        }
    );

    m_viewMenu->addSeparator();

    m_themeMenu =
        m_viewMenu->addMenu(QString());

    m_themeGroup =
        new QActionGroup(this);

    m_themeGroup->setExclusive(true);

    m_systemThemeAction =
        m_themeMenu->addAction(QString());

    m_darkThemeAction =
        m_themeMenu->addAction(QString());

    m_systemThemeAction->setCheckable(true);
    m_darkThemeAction->setCheckable(true);

    m_themeGroup->addAction(
        m_systemThemeAction
    );

    m_themeGroup->addAction(
        m_darkThemeAction
    );

    m_systemThemeAction->setChecked(
        !m_darkMode
    );

    m_darkThemeAction->setChecked(
        m_darkMode
    );

    connect(
        m_systemThemeAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_darkMode = false;

            QSettings settings;

            settings.setValue(
                QStringLiteral("theme"),
                QStringLiteral("system")
            );

            settings.remove(
                QStringLiteral("darkMode")
            );

            applyTheme();
        }
    );

    connect(
        m_darkThemeAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_darkMode = true;

            QSettings settings;

            settings.setValue(
                QStringLiteral("theme"),
                QStringLiteral("dark")
            );

            settings.remove(
                QStringLiteral("darkMode")
            );

            applyTheme();
        }
    );

    m_diskMenu =
        menuBar()->addMenu(QString());

    m_helpMenu =
        menuBar()->addMenu(QString());

    m_aboutAction =
        m_helpMenu->addAction(QString());

    connect(
        m_aboutAction,
        &QAction::triggered,
        this,
        [this]
        {
            const QString description =
                tx(
                    "A Qt-based S.M.A.R.T. and NVMe health monitor for Linux, featuring a clean interface inspired by CrystalDiskInfo.",
                    "Ein Qt-basierter S.M.A.R.T.- und NVMe-Zustandsmonitor für Linux mit einer übersichtlichen, von CrystalDiskInfo inspirierten Benutzeroberfläche."
                );

            const QString thirdPartyTitle =
                tx(
                    "Third-party software",
                    "Drittanbieter-Software"
                );

            const QString qtLicense =
                tx(
                    "Qt 6 — The Qt Company, under the applicable Qt license",
                    "Qt 6 — The Qt Company, gemäß der jeweils anwendbaren Qt-Lizenz"
                );

            QMessageBox::about(
                this,
                tx(
                    "About LinDiskInfo",
                    "Über LinDiskInfo"
                ),
                QStringLiteral(
                    "<h2>LinDiskInfo %1</h2>"
                    "<p>%2</p>"
                    "<p>"
                    "<b>Copyright © 2026 PacmanicS</b><br>"
                    "GPL-3.0-or-later"
                    "</p>"
                    "<hr>"
                    "<p><b>%3</b></p>"
                    "<p>"
                    "smartmontools / smartctl — GPL-2.0-or-later<br>"
                    "%4"
                    "</p>"
                ).arg(
                    QCoreApplication::
                        applicationVersion(),
                    description,
                    thirdPartyTitle,
                    qtLicense
                )
            );
        }
    );


    m_languageMenu =
        menuBar()->addMenu(QString());

    m_languageGroup =
        new QActionGroup(this);

    m_languageGroup->setExclusive(true);

    m_englishAction =
        m_languageMenu->addAction(
            QStringLiteral("English [English]")
        );

    m_germanAction =
        m_languageMenu->addAction(
            QStringLiteral("Deutsch [German]")
        );

    m_englishAction->setCheckable(true);
    m_germanAction->setCheckable(true);

    m_languageGroup->addAction(
        m_englishAction
    );

    m_languageGroup->addAction(
        m_germanAction
    );

    m_englishAction->setChecked(
        m_language ==
        Language::English
    );

    m_germanAction->setChecked(
        m_language ==
        Language::German
    );

    connect(
        m_englishAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_language =
                Language::English;

            QSettings().setValue(
                QStringLiteral("language"),
                QStringLiteral("en")
            );

            applyLanguage();
        }
    );

    connect(
        m_germanAction,
        &QAction::triggered,
        this,
        [this]
        {
            m_language =
                Language::German;

            QSettings().setValue(
                QStringLiteral("language"),
                QStringLiteral("de")
            );

            applyLanguage();
        }
    );

    m_autoRefreshTimer =
        new QTimer(this);

    connect(
        m_autoRefreshTimer,
        &QTimer::timeout,
        this,
        [this]
        {
            if (m_autoRefreshAllDrives) {
                refreshAllData();
                return;
            }

            if (m_selectedDrive >= 0 &&
                m_selectedDrive < m_drives.size()) {

                m_backend->requestDeviceData(
                    m_drives.at(m_selectedDrive)
                );
            }
        }
    );

    m_autoDetectionTimer =
        new QTimer(this);

    connect(
        m_autoDetectionTimer,
        &QTimer::timeout,
        this,
        [this]
        {
            m_backend->scanDevices();
        }
    );

    setAutoRefreshInterval(
        m_autoRefreshMinutes
    );

    setAutoDetectionInterval(
        m_autoDetectionSeconds
    );

    m_table->setVisible(
        !m_hideSmartInfoAction->isChecked()
    );

    m_deviceWatcher =
        new QFileSystemWatcher(this);

    m_liveDetectionDebounce =
        new QTimer(this);

    m_liveDetectionDebounce->setSingleShot(
        true
    );

    m_liveDetectionDebounce->setInterval(
        500
    );

    const QString blockDevicePath =
        QStringLiteral(
            "/sys/class/block"
        );

    if (QFileInfo::exists(
            blockDevicePath
        )) {

        m_deviceWatcher->addPath(
            blockDevicePath
        );
    }

    connect(
        m_deviceWatcher,
        &QFileSystemWatcher::directoryChanged,
        this,
        [this](const QString &)
        {
            if (!m_liveDetectionAction ||
                !m_liveDetectionAction
                    ->isChecked()) {
                return;
            }

            m_liveDetectionDebounce->start();
        }
    );

    connect(
        m_liveDetectionDebounce,
        &QTimer::timeout,
        this,
        [this]
        {
            setStatus(
                tx(
                    "Storage device change detected...",
                    "Datenträgeränderung erkannt..."
                )
            );

            m_backend->scanDevices();
        }
    );

    setupTrayIcon();

    connect(
        m_showTrayAction,
        &QAction::toggled,
        this,
        [this](bool enabled)
        {
            QSettings().setValue(
                QStringLiteral("showInTray"),
                enabled
            );

            if (!m_trayIcon)
                return;

            if (enabled)
                m_trayIcon->show();
            else
                m_trayIcon->hide();
        }
    );

    connect(
        m_startWithSystemAction,
        &QAction::toggled,
        this,
        &MainWindow::setStartWithSystem
    );

    setZoomPercent(
        m_zoomPercent
    );


    m_settingsMenu->addSeparator();

    m_storageUnitMenu =
        m_settingsMenu->addMenu(
            QString()
        );

    auto *storageUnitGroup =
        new QActionGroup(this);

    storageUnitGroup->setExclusive(true);

    const QString selectedStorageUnit =
        QSettings().value(
            QStringLiteral(
                "storageUnit"
            ),
            QStringLiteral("GB")
        ).toString();

    const auto addStorageUnit =
        [this,
         storageUnitGroup,
         &selectedStorageUnit](
            const QString &label,
            const QString &value
        )
        {
            QAction *action =
                m_storageUnitMenu->addAction(
                    label
                );

            action->setCheckable(true);
            action->setData(value);

            storageUnitGroup->addAction(
                action
            );

            action->setChecked(
                selectedStorageUnit == value
            );

            connect(
                action,
                &QAction::triggered,
                this,
                [this, value]
                {
                    QSettings().setValue(
                        QStringLiteral(
                            "storageUnit"
                        ),
                        value
                    );

                    if (m_hasCurrentData) {
                        renderDevice(
                            m_currentDrive,
                            m_currentData
                        );
                    }
                }
            );
        };

    addStorageUnit(
        QStringLiteral(
            "GB (decimal)"
        ),
        QStringLiteral("GB")
    );

    addStorageUnit(
        QStringLiteral(
            "GiB (binary)"
        ),
        QStringLiteral("GiB")
    );

    addStorageUnit(
        QStringLiteral(
            "TB (decimal)"
        ),
        QStringLiteral("TB")
    );

    addStorageUnit(
        QStringLiteral(
            "TiB (binary)"
        ),
        QStringLiteral("TiB")
    );



    {
        auto actionByName =
            [this](const char *name) -> QAction *
            {
                return findChild<QAction *>(
                    QString::fromLatin1(name)
                );
            };

        auto menuByName =
            [this](const char *name) -> QMenu *
            {
                return findChild<QMenu *>(
                    QString::fromLatin1(name)
                );
            };


        // ====================================================
        // Health Status Settings
        // ====================================================

        if (QAction *healthSettings =
                actionByName(
                    "stateSettingsAction"
                )) {

            healthSettings->setVisible(true);

            connect(
                healthSettings,
                &QAction::triggered,
                this,
                [this]
                {
                    QDialog dialog(this);

                    dialog.setWindowTitle(
                        tx(
                            "Health Status Settings",
                            "Zustandseinstellungen"
                        )
                    );

                    auto *form =
                        new QFormLayout(&dialog);

                    auto *nvmeCaution =
                        new QSpinBox(&dialog);

                    auto *nvmeBad =
                        new QSpinBox(&dialog);

                    auto *ataSector =
                        new QSpinBox(&dialog);

                    nvmeCaution->setRange(0, 100);
                    nvmeBad->setRange(0, 100);

                    ataSector->setRange(
                        1,
                        1000000000
                    );

                    nvmeCaution->setSuffix(
                        QStringLiteral(" %")
                    );

                    nvmeBad->setSuffix(
                        QStringLiteral(" %")
                    );

                    nvmeCaution->setValue(
                        m_nvmeCautionRemaining
                    );

                    nvmeBad->setValue(
                        m_nvmeBadRemaining
                    );

                    ataSector->setValue(
                        static_cast<int>(
                            std::min<quint64>(
                                m_ataSectorCautionCount,
                                1000000000ULL
                            )
                        )
                    );

                    form->addRow(
                        tx(
                            "NVMe caution at remaining life:",
                            "NVMe-Warnung ab Restlebensdauer:"
                        ),
                        nvmeCaution
                    );

                    form->addRow(
                        tx(
                            "NVMe bad at remaining life:",
                            "NVMe: Schlecht ab Restlebensdauer:"
                        ),
                        nvmeBad
                    );

                    form->addRow(
                        tx(
                            "ATA sector warning count:",
                            "ATA-Sektorwarnung ab Anzahl:"
                        ),
                        ataSector
                    );

                    auto *buttons =
                        new QDialogButtonBox(
                            QDialogButtonBox::Ok |
                            QDialogButtonBox::Cancel,
                            &dialog
                        );

                    form->addRow(buttons);

                    connect(
                        buttons,
                        &QDialogButtonBox::accepted,
                        &dialog,
                        &QDialog::accept
                    );

                    connect(
                        buttons,
                        &QDialogButtonBox::rejected,
                        &dialog,
                        &QDialog::reject
                    );

                    if (dialog.exec() !=
                        QDialog::Accepted) {
                        return;
                    }

                    m_nvmeCautionRemaining =
                        nvmeCaution->value();

                    m_nvmeBadRemaining =
                        std::min(
                            nvmeBad->value(),
                            m_nvmeCautionRemaining
                        );

                    m_ataSectorCautionCount =
                        static_cast<quint64>(
                            ataSector->value()
                        );

                    QSettings settings;

                    settings.setValue(
                        QStringLiteral(
                            "nvmeCautionRemaining"
                        ),
                        m_nvmeCautionRemaining
                    );

                    settings.setValue(
                        QStringLiteral(
                            "nvmeBadRemaining"
                        ),
                        m_nvmeBadRemaining
                    );

                    settings.setValue(
                        QStringLiteral(
                            "ataSectorCautionCount"
                        ),
                        QVariant::fromValue<
                            qulonglong
                        >(
                            m_ataSectorCautionCount
                        )
                    );

                    refreshDrivePresentation();
                }
            );
        }


        // ====================================================
        // Temperature warning thresholds
        // ====================================================

        if (QAction *temperatureSettings =
                actionByName(
                    "temperatureWarningAction"
                )) {

            temperatureSettings->setVisible(true);

            connect(
                temperatureSettings,
                &QAction::triggered,
                this,
                [this]
                {
                    QDialog dialog(this);

                    dialog.setWindowTitle(
                        tx(
                            "Temperature Warning Settings",
                            "Temperaturwarneinstellungen"
                        )
                    );

                    auto *form =
                        new QFormLayout(&dialog);

                    auto *caution =
                        new QSpinBox(&dialog);

                    auto *bad =
                        new QSpinBox(&dialog);

                    const bool fahrenheit =
                        m_temperatureUnit ==
                        TemperatureUnit::Fahrenheit;

                    const auto toDisplay =
                        [fahrenheit](int celsius)
                        {
                            if (!fahrenheit)
                                return celsius;

                            return qRound(
                                celsius *
                                9.0 / 5.0 +
                                32.0
                            );
                        };

                    const auto toCelsius =
                        [fahrenheit](int value)
                        {
                            if (!fahrenheit)
                                return value;

                            return qRound(
                                (value - 32.0) *
                                5.0 / 9.0
                            );
                        };

                    if (fahrenheit) {
                        caution->setRange(34, 248);
                        bad->setRange(34, 248);

                        caution->setSuffix(
                            QStringLiteral(" °F")
                        );

                        bad->setSuffix(
                            QStringLiteral(" °F")
                        );
                    } else {
                        caution->setRange(1, 120);
                        bad->setRange(1, 120);

                        caution->setSuffix(
                            QStringLiteral(" °C")
                        );

                        bad->setSuffix(
                            QStringLiteral(" °C")
                        );
                    }

                    caution->setValue(
                        toDisplay(
                            m_temperatureCaution
                        )
                    );

                    bad->setValue(
                        toDisplay(
                            m_temperatureBad
                        )
                    );

                    form->addRow(
                        tx(
                            "Caution:",
                            "Vorsicht:"
                        ),
                        caution
                    );

                    form->addRow(
                        tx(
                            "Bad:",
                            "Schlecht:"
                        ),
                        bad
                    );

                    auto *buttons =
                        new QDialogButtonBox(
                            QDialogButtonBox::Ok |
                            QDialogButtonBox::Cancel,
                            &dialog
                        );

                    form->addRow(buttons);

                    connect(
                        buttons,
                        &QDialogButtonBox::accepted,
                        &dialog,
                        &QDialog::accept
                    );

                    connect(
                        buttons,
                        &QDialogButtonBox::rejected,
                        &dialog,
                        &QDialog::reject
                    );

                    if (dialog.exec() !=
                        QDialog::Accepted) {
                        return;
                    }

                    int cautionC =
                        toCelsius(
                            caution->value()
                        );

                    int badC =
                        toCelsius(
                            bad->value()
                        );

                    if (badC <= cautionC)
                        badC = cautionC + 1;

                    m_temperatureCaution =
                        cautionC;

                    m_temperatureBad =
                        badC;

                    QSettings settings;

                    settings.setValue(
                        QStringLiteral(
                            "temperatureCaution"
                        ),
                        m_temperatureCaution
                    );

                    settings.setValue(
                        QStringLiteral(
                            "temperatureBad"
                        ),
                        m_temperatureBad
                    );

                    refreshDrivePresentation();
                }
            );
        }


        // ====================================================
        // Raw Value Modes
        // ====================================================

        {
            auto *rawGroup =
                new QActionGroup(this);

            rawGroup->setExclusive(true);

            const QString savedMode =
                settings.value(
                    QStringLiteral(
                        "rawValueMode"
                    ),
                    QStringLiteral("hex")
                ).toString();

            QList<QAction *> rawActions;

            for (QAction *entry :
                 m_rawValuesMenu->actions()) {

                if (!entry ||
                    entry->isSeparator() ||
                    entry == m_showRawAction) {
                    continue;
                }

                rawActions.append(entry);
            }

            const QStringList modes =
            {
                QStringLiteral("hex"),
                QStringLiteral("dec"),
                QStringLiteral("dec2"),
                QStringLiteral("dec1")
            };

            for (int i = 0;
                 i < rawActions.size() &&
                 i < modes.size();
                 ++i) {

                QAction *entry =
                    rawActions.at(i);

                const QString mode =
                    modes.at(i);

                entry->setVisible(true);
                entry->setCheckable(true);
                entry->setData(mode);

                rawGroup->addAction(entry);

                entry->setChecked(
                    savedMode == mode
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, mode]
                    {
                        QSettings().setValue(
                            QStringLiteral(
                                "rawValueMode"
                            ),
                            mode
                        );

                        if (m_hasCurrentData) {
                            renderDevice(
                                m_currentDrive,
                                m_currentData
                            );
                        }
                    }
                );
            }
        }


        // ====================================================
        // Startup Delay
        // ====================================================

        if (QMenu *startup =
                menuByName(
                    "startupDelayMenu"
                )) {

            startup->menuAction()
                ->setVisible(true);

            auto *group =
                new QActionGroup(this);

            group->setExclusive(true);

            for (QAction *entry :
                 startup->actions()) {

                const QString text =
                    entry->text();

                bool ok = false;

                const int seconds =
                    text.section(
                        QLatin1Char(' '),
                        0,
                        0
                    ).toInt(&ok);

                if (!ok)
                    continue;

                entry->setData(seconds);
                entry->setCheckable(true);

                group->addAction(entry);

                entry->setChecked(
                    seconds ==
                    m_startupDelaySeconds
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, seconds]
                    {
                        m_startupDelaySeconds =
                            seconds;

                        QSettings().setValue(
                            QStringLiteral(
                                "startupDelaySeconds"
                            ),
                            seconds
                        );
                    }
                );
            }
        }


        // ====================================================
        // Tray Behavior
        // ====================================================

        if (QMenu *tray =
                menuByName(
                    "trayBehaviorMenu"
                )) {

            tray->menuAction()
                ->setVisible(true);

            const QList<QAction *> entries =
                tray->actions();

            if (entries.size() >= 2) {
                auto *group =
                    new QActionGroup(this);

                group->setExclusive(true);

                const QStringList modes =
                {
                    QStringLiteral("hide"),
                    QStringLiteral("minimize")
                };

                for (int i = 0;
                     i < 2;
                     ++i) {

                    QAction *entry =
                        entries.at(i);

                    const QString mode =
                        modes.at(i);

                    entry->setCheckable(true);
                    entry->setData(mode);

                    group->addAction(entry);

                    entry->setChecked(
                        m_trayBehavior == mode
                    );

                    connect(
                        entry,
                        &QAction::triggered,
                        this,
                        [this, mode]
                        {
                            m_trayBehavior =
                                mode;

                            QSettings().setValue(
                                QStringLiteral(
                                    "trayBehavior"
                                ),
                                mode
                            );
                        }
                    );
                }
            }
        }


        // ====================================================
        // Drive Sort Method
        // ====================================================

        if (QMenu *sort =
                menuByName(
                    "driveSortMenu"
                )) {

            sort->menuAction()
                ->setVisible(true);

            sort->clear();

            auto *group =
                new QActionGroup(this);

            group->setExclusive(true);

            struct SortItem
            {
                const char *name;
                const char *mode;
            };

            const SortItem items[] =
            {
                {
                    "driveSortDefault",
                    "default"
                },
                {
                    "driveSortPath",
                    "path"
                },
                {
                    "driveSortModel",
                    "model"
                },
                {
                    "driveSortHealth",
                    "health"
                },
                {
                    "driveSortTemperature",
                    "temperature"
                }
            };

            for (const SortItem &item : items) {
                QAction *entry =
                    sort->addAction(
                        QString()
                    );

                entry->setObjectName(
                    QString::fromLatin1(
                        item.name
                    )
                );

                const QString mode =
                    QString::fromLatin1(
                        item.mode
                    );

                entry->setData(mode);
                entry->setCheckable(true);

                group->addAction(entry);

                entry->setChecked(
                    m_driveSortMethod ==
                    mode
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, mode]
                    {
                        m_driveSortMethod =
                            mode;

                        QSettings().setValue(
                            QStringLiteral(
                                "driveSortMethod"
                            ),
                            mode
                        );

                        refreshDrivePresentation();
                    }
                );
            }
        }


        // ====================================================
        // Display Number of Drives
        // ====================================================

        if (QMenu *display =
                menuByName(
                    "displayDrivesMenu"
                )) {

            display->menuAction()
                ->setVisible(true);

            display->clear();

            auto *group =
                new QActionGroup(this);

            group->setExclusive(true);

            const int limits[] =
            {
                0,
                4,
                8,
                16
            };

            for (int limit : limits) {
                QAction *entry =
                    display->addAction(
                        limit == 0
                            ? QString()
                            : QString::number(
                                  limit
                              )
                    );

                entry->setData(limit);
                entry->setCheckable(true);

                if (limit == 0) {
                    entry->setObjectName(
                        QStringLiteral(
                            "displayDrivesAll"
                        )
                    );
                }

                group->addAction(entry);

                entry->setChecked(
                    m_displayDriveLimit ==
                    limit
                );

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this, limit]
                    {
                        m_displayDriveLimit =
                            limit;

                        QSettings().setValue(
                            QStringLiteral(
                                "displayDriveLimit"
                            ),
                            limit
                        );

                        applyDriveButtonLimit();
                    }
                );
            }
        }


        // ====================================================
        // Full Device Rescan
        // ====================================================

        if (QAction *advancedSearch =
                actionByName(
                    "advancedDriveSearchAction"
                )) {

            advancedSearch->setVisible(true);

            connect(
                advancedSearch,
                &QAction::triggered,
                this,
                [this]
                {
                    setStatus(
                        tx(
                            "Performing full device rescan...",
                            "Führe vollständige Laufwerkssuche aus..."
                        )
                    );

                    m_driveData.clear();
                    refreshDevices();
                }
            );
        }


        // ====================================================
        // Hide devices without SMART
        // ====================================================

        if (QAction *hideNoSmart =
                actionByName(
                    "hideNoSmartAction"
                )) {

            hideNoSmart->setVisible(true);
            hideNoSmart->setCheckable(true);

            hideNoSmart->setChecked(
                m_hideNoSmart
            );

            connect(
                hideNoSmart,
                &QAction::toggled,
                this,
                [this](bool hidden)
                {
                    m_hideNoSmart =
                        hidden;

                    QSettings().setValue(
                        QStringLiteral(
                            "hideNoSmart"
                        ),
                        hidden
                    );

                    refreshDrivePresentation();
                }
            );
        }
    }



    {
        auto actionByName061 =
            [this](const char *name)
                -> QAction *
            {
                return findChild<QAction *>(
                    QString::fromLatin1(
                        name
                    )
                );
            };


        // ====================================================
        // Management
        // ====================================================

        if (QAction *management =
                actionByName061(
                    "aamApmManagementAction"
                )) {

            management->setVisible(true);

            connect(
                management,
                &QAction::triggered,
                this,
                [this]
                {
                    if (!m_hasCurrentData) {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "No drive is currently selected.",
                                "Derzeit ist kein Laufwerk ausgewählt."
                            )
                        );

                        return;
                    }

                    const bool ataDevice =
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_smart_attributes"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_aam"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_apm"
                            )
                        );

                    if (!ataDevice) {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "AAM/APM is an ATA feature and is not available for the selected drive.",
                                "AAM/APM ist eine ATA-Funktion und steht für das ausgewählte Laufwerk nicht zur Verfügung."
                            )
                        );

                        return;
                    }

                    const QJsonObject aam =
                        m_currentData.value(
                            QStringLiteral(
                                "ata_aam"
                            )
                        ).toObject();

                    const QJsonObject apm =
                        m_currentData.value(
                            QStringLiteral(
                                "ata_apm"
                            )
                        ).toObject();

                    const bool aamSupported =
                        !aam.isEmpty();

                    const bool apmSupported =
                        !apm.isEmpty();

                    if (!aamSupported &&
                        !apmSupported) {

                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "The selected ATA drive reports both AAM and APM as unavailable.",
                                "Das ausgewählte ATA-Laufwerk meldet sowohl AAM als auch APM als nicht verfügbar."
                            )
                        );

                        return;
                    }

                    QDialog dialog(this);

                    dialog.setWindowTitle(
                        tx(
                            "AAM/APM Management",
                            "AAM/APM-Verwaltung"
                        )
                    );

                    auto *form =
                        new QFormLayout(
                            &dialog
                        );

                    auto *driveLabel =
                        new QLabel(
                            m_titleLabel->text(),
                            &dialog
                        );

                    driveLabel->setWordWrap(
                        true
                    );

                    form->addRow(
                        tx(
                            "Drive:",
                            "Laufwerk:"
                        ),
                        driveLabel
                    );


                    // ----------------------------------------
                    // AAM
                    // ----------------------------------------

                    QCheckBox *aamEnabled =
                        nullptr;

                    QSpinBox *aamLevel =
                        nullptr;

                    if (aamSupported) {
                        aamEnabled =
                            new QCheckBox(
                                tx(
                                    "Enabled",
                                    "Aktiviert"
                                ),
                                &dialog
                            );

                        aamLevel =
                            new QSpinBox(
                                &dialog
                            );

                        aamLevel->setRange(
                            128,
                            254
                        );

                        aamLevel->setValue(
                            aam.value(
                                QStringLiteral(
                                    "level"
                                )
                            ).toInt(
                                aam.value(
                                    QStringLiteral(
                                        "recommended_level"
                                    )
                                ).toInt(254)
                            )
                        );

                        aamEnabled->setChecked(
                            aam.value(
                                QStringLiteral(
                                    "enabled"
                                )
                            ).toBool(false)
                        );

                        aamLevel->setEnabled(
                            aamEnabled->isChecked()
                        );

                        connect(
                            aamEnabled,
                            &QCheckBox::toggled,
                            aamLevel,
                            &QSpinBox::setEnabled
                        );

                        auto *aamWidget =
                            new QWidget(&dialog);

                        auto *aamLayout =
                            new QHBoxLayout(
                                aamWidget
                            );

                        aamLayout
                            ->setContentsMargins(
                                0, 0, 0, 0
                            );

                        aamLayout->addWidget(
                            aamEnabled
                        );

                        aamLayout->addWidget(
                            aamLevel
                        );

                        form->addRow(
                            QStringLiteral("AAM:"),
                            aamWidget
                        );

                        const int recommended =
                            aam.value(
                                QStringLiteral(
                                    "recommended_level"
                                )
                            ).toInt(-1);

                        if (recommended >= 128 &&
                            recommended <= 254) {

                            form->addRow(
                                tx(
                                    "AAM recommended:",
                                    "AAM empfohlen:"
                                ),
                                new QLabel(
                                    QString::number(
                                        recommended
                                    ),
                                    &dialog
                                )
                            );
                        }

                    } else {
                        form->addRow(
                            QStringLiteral("AAM:"),
                            new QLabel(
                                tx(
                                    "Unavailable",
                                    "Nicht verfügbar"
                                ),
                                &dialog
                            )
                        );
                    }


                    // ----------------------------------------
                    // APM
                    // ----------------------------------------

                    QCheckBox *apmEnabled =
                        nullptr;

                    QSpinBox *apmLevel =
                        nullptr;

                    if (apmSupported) {
                        apmEnabled =
                            new QCheckBox(
                                tx(
                                    "Enabled",
                                    "Aktiviert"
                                ),
                                &dialog
                            );

                        apmLevel =
                            new QSpinBox(
                                &dialog
                            );

                        apmLevel->setRange(
                            1,
                            254
                        );

                        apmLevel->setValue(
                            apm.value(
                                QStringLiteral(
                                    "level"
                                )
                            ).toInt(128)
                        );

                        apmEnabled->setChecked(
                            apm.value(
                                QStringLiteral(
                                    "enabled"
                                )
                            ).toBool(false)
                        );

                        apmLevel->setEnabled(
                            apmEnabled->isChecked()
                        );

                        connect(
                            apmEnabled,
                            &QCheckBox::toggled,
                            apmLevel,
                            &QSpinBox::setEnabled
                        );

                        auto *apmWidget =
                            new QWidget(&dialog);

                        auto *apmLayout =
                            new QHBoxLayout(
                                apmWidget
                            );

                        apmLayout
                            ->setContentsMargins(
                                0, 0, 0, 0
                            );

                        apmLayout->addWidget(
                            apmEnabled
                        );

                        apmLayout->addWidget(
                            apmLevel
                        );

                        form->addRow(
                            QStringLiteral("APM:"),
                            apmWidget
                        );

                    } else {
                        form->addRow(
                            QStringLiteral("APM:"),
                            new QLabel(
                                tx(
                                    "Unavailable",
                                    "Nicht verfügbar"
                                ),
                                &dialog
                            )
                        );
                    }


                    auto *note =
                        new QLabel(
                            tx(
                                "AAM controls acoustic/performance behavior. APM controls ATA power management. Support and exact behavior depend on the drive firmware.",
                                "AAM steuert Geräuschentwicklung und Leistung. APM steuert das ATA-Energiemanagement. Unterstützung und genaues Verhalten hängen von der Laufwerks-Firmware ab."
                            ),
                            &dialog
                        );

                    note->setWordWrap(true);

                    form->addRow(note);


                    auto *buttons =
                        new QDialogButtonBox(
                            QDialogButtonBox::Ok |
                            QDialogButtonBox::Cancel,
                            &dialog
                        );

                    form->addRow(buttons);

                    connect(
                        buttons,
                        &QDialogButtonBox::accepted,
                        &dialog,
                        &QDialog::accept
                    );

                    connect(
                        buttons,
                        &QDialogButtonBox::rejected,
                        &dialog,
                        &QDialog::reject
                    );

                    if (dialog.exec() !=
                        QDialog::Accepted) {

                        return;
                    }

                    QString aamValue;
                    QString apmValue;

                    if (aamSupported) {
                        aamValue =
                            aamEnabled->isChecked()
                                ? QString::number(
                                      aamLevel->value()
                                  )
                                : QStringLiteral(
                                      "off"
                                  );
                    }

                    if (apmSupported) {
                        apmValue =
                            apmEnabled->isChecked()
                                ? QString::number(
                                      apmLevel->value()
                                  )
                                : QStringLiteral(
                                      "off"
                                  );
                    }


                    // Save preferred values per physical drive.
                    const QString key =
                        ataSettingsKey(
                            m_currentDrive,
                            m_currentData
                        );

                    QSettings settings;

                    settings.beginGroup(
                        QStringLiteral(
                            "ataAuto"
                        )
                    );

                    settings.beginGroup(key);

                    if (!aamValue.isEmpty()) {
                        settings.setValue(
                            QStringLiteral("aam"),
                            aamValue
                        );
                    } else {
                        settings.remove(
                            QStringLiteral("aam")
                        );
                    }

                    if (!apmValue.isEmpty()) {
                        settings.setValue(
                            QStringLiteral("apm"),
                            apmValue
                        );
                    } else {
                        settings.remove(
                            QStringLiteral("apm")
                        );
                    }

                    settings.endGroup();
                    settings.endGroup();

                    m_autoAdjustedAtaDrives
                        .remove(key);

                    if (m_aamApmAutoEnabled) {
                        m_autoAdjustedAtaDrives
                            .insert(key);
                    }

                    setStatus(
                        tx(
                            "Applying AAM/APM settings...",
                            "AAM/APM-Einstellungen werden angewendet..."
                        )
                    );

                    m_backend
                        ->setAtaPowerSettings(
                            m_currentDrive,
                            aamValue,
                            apmValue
                        );
                }
            );
        }


        // ====================================================
        // Auto Adjustment
        // ====================================================

        if (QAction *automatic =
                actionByName061(
                    "aamApmAutoAction"
                )) {

            automatic->setVisible(true);
            automatic->setCheckable(true);

            automatic->setChecked(
                m_aamApmAutoEnabled
            );

            connect(
                automatic,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_aamApmAutoEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "aamApmAutoAdjustment"
                        ),
                        enabled
                    );

                    m_autoAdjustedAtaDrives
                        .clear();

                    if (!enabled)
                        return;

                    for (const DriveInfo &drive :
                         m_drives) {

                        if (!m_driveData.contains(
                                lindiskinfoDriveIdentity(drive)
                            )) {
                            continue;
                        }

                        maybeAutoAdjustAta(
                            drive,
                            m_driveData.value(
                                lindiskinfoDriveIdentity(drive)
                            )
                        );
                    }
                }
            );
        }
    }



    {
        auto actionByName062 =
            [this](const char *name)
                -> QAction *
            {
                return findChild<QAction *>(
                    QString::fromLatin1(
                        name
                    )
                );
            };


        // ====================================================
        // ATA PASS THROUGH / SAT fallback
        // ====================================================

        if (QAction *entry =
                actionByName062(
                    "ataPassThroughAction"
                )) {

            entry->setVisible(true);
            entry->setCheckable(true);

            entry->setChecked(
                m_ataPassThroughEnabled
            );

            entry->setStatusTip(
                tx(
                    "Use SAT/ATA pass-through as a fallback for external SCSI-style storage devices.",
                    "SAT/ATA-Passthrough als Fallback für externe SCSI-artige Datenträger verwenden."
                )
            );

            connect(
                entry,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_ataPassThroughEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "ataPassThrough"
                        ),
                        enabled
                    );

                    m_autoAdjustedAtaDrives
                        .clear();

                    refreshAllData();
                }
            );
        }


        // ====================================================
        // USB / IEEE 1394 devices
        // ====================================================

        if (QAction *entry =
                actionByName062(
                    "usbIeeeAction"
                )) {

            entry->setVisible(true);
            entry->setCheckable(true);

            entry->setChecked(
                m_externalStorageEnabled
            );

            entry->setStatusTip(
                tx(
                    "Include external USB and IEEE 1394 / FireWire storage devices.",
                    "Externe USB- und IEEE-1394-/FireWire-Datenträger einbeziehen."
                )
            );

            connect(
                entry,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_externalStorageEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "externalStorage"
                        ),
                        enabled
                    );

                    refreshDevices();
                }
            );
        }


        // ====================================================
        // MegaRAID physical members
        //
        // smartctl --scan-open already reports these as
        // /dev/bus/N -d megaraid,M when supported.
        // ====================================================

        if (QAction *entry =
                actionByName062(
                    "megaRaidAction"
                )) {

            entry->setVisible(true);
            entry->setCheckable(true);

            entry->setChecked(
                m_megaRaidEnabled
            );

            entry->setStatusTip(
                tx(
                    "Include physical drives discovered behind LSI/Broadcom MegaRAID and Dell PERC controllers.",
                    "Physische Laufwerke hinter LSI/Broadcom-MegaRAID- und Dell-PERC-Controllern einbeziehen."
                )
            );

            connect(
                entry,
                &QAction::toggled,
                this,
                [this](bool enabled)
                {
                    m_megaRaidEnabled =
                        enabled;

                    QSettings().setValue(
                        QStringLiteral(
                            "megaRaidPhysicalDrives"
                        ),
                        enabled
                    );

                    refreshDevices();
                }
            );
        }


        // ====================================================
        // Controller compatibility actions
        //
        // There is intentionally no invented smartctl -d type
        // for RAIDXpert2 or VROC. Linux-exposed members are
        // scanned through their real block/NVMe nodes.
        // ====================================================

        const auto addCompatibilityRescan =
            [this, actionByName062](
                const char *name,
                const QString &english,
                const QString &german
            )
            {
                QAction *entry =
                    actionByName062(name);

                if (!entry)
                    return;

                entry->setVisible(true);

                connect(
                    entry,
                    &QAction::triggered,
                    this,
                    [this,
                     english,
                     german]
                    {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                english
                                    .toUtf8()
                                    .constData(),
                                german
                                    .toUtf8()
                                    .constData()
                            )
                        );

                        refreshDevices();
                    }
                );
            };

        addCompatibilityRescan(
            "intelAmdRaidAction",
            QStringLiteral(
                "Linux does not provide one universal smartctl pass-through type for every Intel or AMD RAID controller. LinDiskInfo will rescan all block devices and controller members exposed by the kernel and smartctl."
            ),
            QStringLiteral(
                "Linux bietet keinen universellen smartctl-Passthrough-Typ für jeden Intel- oder AMD-RAID-Controller. LinDiskInfo durchsucht alle vom Kernel und von smartctl bereitgestellten Laufwerke und Controller-Mitglieder erneut."
            )
        );

        addCompatibilityRescan(
            "amdRaidXpertAction",
            QStringLiteral(
                "AMD RAIDXpert2 has no generic smartctl physical-drive pass-through mode on Linux. Drives exposed individually by the kernel are handled normally. LinDiskInfo will perform a fresh controller scan."
            ),
            QStringLiteral(
                "AMD RAIDXpert2 besitzt unter Linux keinen generischen smartctl-Passthrough-Modus für physische Laufwerke. Vom Kernel einzeln bereitgestellte Laufwerke werden normal verarbeitet. LinDiskInfo führt jetzt einen neuen Controller-Scan aus."
            )
        );

        addCompatibilityRescan(
            "intelVrocAction",
            QStringLiteral(
                "Intel VROC NVMe members exposed as normal /dev/nvme devices are handled automatically. Firmware-hidden members cannot be invented by LinDiskInfo. A fresh controller scan will now be performed."
            ),
            QStringLiteral(
                "Intel-VROC-NVMe-Mitglieder, die als normale /dev/nvme-Geräte bereitgestellt werden, verarbeitet LinDiskInfo automatisch. Von der Firmware verborgene Mitglieder kann LinDiskInfo nicht künstlich sichtbar machen. Jetzt wird ein neuer Controller-Scan durchgeführt."
            )
        );
    }



    {
        QMenu *selfTestMenu =
            m_advancedOptionsMenu
                ->addMenu(
                    QString()
                );

        selfTestMenu->setObjectName(
            QStringLiteral(
                "selfTestMenu"
            )
        );

        QAction *shortTest =
            selfTestMenu->addAction(
                QString()
            );

        shortTest->setObjectName(
            QStringLiteral(
                "shortSelfTestAction"
            )
        );

        QAction *longTest =
            selfTestMenu->addAction(
                QString()
            );

        longTest->setObjectName(
            QStringLiteral(
                "longSelfTestAction"
            )
        );

        selfTestMenu->addSeparator();

        QAction *abortTest =
            selfTestMenu->addAction(
                QString()
            );

        abortTest->setObjectName(
            QStringLiteral(
                "abortSelfTestAction"
            )
        );


        QMenu *logsMenu =
            m_advancedOptionsMenu
                ->addMenu(
                    QString()
                );

        logsMenu->setObjectName(
            QStringLiteral(
                "smartLogsMenu"
            )
        );

        QAction *selfTestLog =
            logsMenu->addAction(
                QString()
            );

        selfTestLog->setObjectName(
            QStringLiteral(
                "selfTestLogAction"
            )
        );

        QAction *errorLog =
            logsMenu->addAction(
                QString()
            );

        errorLog->setObjectName(
            QStringLiteral(
                "errorLogAction"
            )
        );

        logsMenu->addSeparator();

        QAction *deviceStatistics =
            logsMenu->addAction(
                QString()
            );

        deviceStatistics->setObjectName(
            QStringLiteral(
                "deviceStatisticsAction"
            )
        );

        QAction *sataPhy =
            logsMenu->addAction(
                QString()
            );

        sataPhy->setObjectName(
            QStringLiteral(
                "sataPhyAction"
            )
        );


        const auto invokeMaintenance =
            [this](
                const QString &operation,
                bool ataOnly,
                bool confirmExtended
            )
            {
                if (!m_hasCurrentData) {
                    QMessageBox::information(
                        this,
                        QStringLiteral(
                            "LinDiskInfo"
                        ),
                        tx(
                            "No drive is currently selected.",
                            "Derzeit ist kein Laufwerk ausgewählt."
                        )
                    );

                    return;
                }

                const bool smartAvailable =
                    m_currentData.value(
                        QStringLiteral(
                            "lindiskinfo_smart_available"
                        )
                    ).toBool(true);

                if (!smartAvailable) {
                    QMessageBox::information(
                        this,
                        QStringLiteral(
                            "LinDiskInfo"
                        ),
                        tx(
                            "The selected drive does not provide SMART diagnostics.",
                            "Das ausgewählte Laufwerk stellt keine SMART-Diagnosefunktionen bereit."
                        )
                    );

                    return;
                }

                if (ataOnly) {
                    const bool ataDevice =
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_smart_attributes"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "ata_version"
                            )
                        ) ||
                        m_currentData.contains(
                            QStringLiteral(
                                "sata_version"
                            )
                        );

                    if (!ataDevice) {
                        QMessageBox::information(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "This function is available for ATA/SATA drives only.",
                                "Diese Funktion ist nur für ATA-/SATA-Laufwerke verfügbar."
                            )
                        );

                        return;
                    }
                }

                if (confirmExtended) {
                    const auto answer =

                        QMessageBox::question(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "Start Extended Self-Test?\n\nAn extended self-test may take a long time and can reduce drive performance while it is running.\n\nStart the test now?",
                                "Erweiterten Selbsttest starten?\n\nEin erweiterter Selbsttest kann lange dauern und währenddessen die Laufwerksleistung reduzieren.\n\nTest jetzt starten?"
                            ),
                            QMessageBox::Yes |
                            QMessageBox::No,
                            QMessageBox::No
                        );

                    if (answer !=
                        QMessageBox::Yes) {
                        return;
                    }
                }

                if (operation ==
                    QStringLiteral(
                        "test_abort"
                    )) {

                    const auto answer =
                        QMessageBox::question(
                            this,
                            QStringLiteral(
                                "LinDiskInfo"
                            ),
                            tx(
                                "Abort running self-test?\n\nSend an abort command to the selected drive?",
                                "Laufenden Selbsttest abbrechen?\n\nAbbruchbefehl an das ausgewählte Laufwerk senden?"
                            ),
                            QMessageBox::Yes |
                            QMessageBox::No,
                            QMessageBox::No
                        );

                    if (answer !=
                        QMessageBox::Yes) {
                        return;
                    }
                }

                setStatus(
                    tx(
                        "Executing drive diagnostic command...",
                        "Laufwerksdiagnose wird ausgeführt..."
                    )
                );

                m_backend->requestMaintenance(
                    m_currentDrive,
                    operation
                );
            };


        connect(
            shortTest,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "test_short"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            longTest,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "test_long"
                    ),
                    false,
                    true
                );
            }
        );

        connect(
            abortTest,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "test_abort"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            selfTestLog,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_selftest"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            errorLog,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_error"
                    ),
                    false,
                    false
                );
            }
        );

        connect(
            deviceStatistics,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_devstat"
                    ),
                    true,
                    false
                );
            }
        );

        connect(
            sataPhy,
            &QAction::triggered,
            this,
            [invokeMaintenance]
            {
                invokeMaintenance(
                    QStringLiteral(
                        "log_sataphy"
                    ),
                    true,
                    false
                );
            }
        );
    }



    // Graph / History is now functional.
    if (m_diagramAction) {
        m_diagramAction->setVisible(
            true
        );

        connect(
            m_diagramAction,
            &QAction::triggered,
            this,
            &MainWindow::openHistoryGraph
        );
    }

}

void MainWindow::applyLanguage()
{
    auto action =
        [this](const char *name) -> QAction *
        {
            return findChild<QAction *>(
                QString::fromLatin1(name)
            );
        };

    auto menu =
        [this](const char *name) -> QMenu *
        {
            return findChild<QMenu *>(
                QString::fromLatin1(name)
            );
        };

    m_fileMenu->setTitle(
        tx("&File", "&Datei")
    );

    m_editMenu->setTitle(
        tx("&Edit", "&Bearbeiten")
    );

    m_settingsMenu->setTitle(
        tx("&Options", "&Optionen")
    );

    m_viewMenu->setTitle(
        tx("&View", "&Ansicht")
    );

    m_diskMenu->setTitle(
        tx("&Disk", "&Laufwerk")
    );

    m_helpMenu->setTitle(
        tx("&Help", "&Hilfe")
    );

    m_languageMenu->setTitle(
        tx("&Language", "&Sprache")
    );

    m_saveTextAction->setText(
        tx(
            "Save (text)",
            "Speichern (Text)"
        )
    );

    m_saveImageAction->setText(
        tx(
            "Save (image)",
            "Speichern (Bild)"
        )
    );

    m_quitAction->setText(
        tx("Exit", "Beenden")
    );

    m_copyAction->setText(
        tx("Copy", "Kopieren")
    );

    m_copyOptionsMenu->setTitle(
        tx(
            "Copy Options",
            "Kopieroptionen"
        )
    );

    m_copyIdentifyAction->setText(
        tx(
            "Device Identification",
            "Geräteidentifikation"
        )
    );

    m_copySmartDataAction->setText(
        tx(
            "SMART Data",
            "SMART-Daten"
        )
    );

    m_copyThresholdAction->setText(
        tx(
            "SMART Thresholds",
            "SMART-Grenzwerte"
        )
    );

    m_copyAsciiAction->setText(
        tx(
            "ASCII View",
            "ASCII-Ansicht"
        )
    );

    m_refreshAction->setText(
        tx(
            "Refresh",
            "Aktualisieren"
        )
    );

    m_autoRefreshMenu->setTitle(
        tx(
            "Auto Refresh",
            "Automatische Aktualisierung"
        )
    );

    for (QAction *entry :
         m_autoRefreshMenu->actions()) {

        if (entry->data().toInt() == 0)
            entry->setText(
                tx(
                    "Disabled",
                    "Deaktivieren"
                )
            );
    }

    m_autoRefreshTargetMenu->setTitle(
        tx(
            "Auto Refresh Target",
            "Aktualisierungsziel"
        )
    );

    if (QAction *entry =
            action("selectAllRefreshTargets")) {

        entry->setText(
            tx(
                "All drives",
                "Alle Laufwerke"
            )
        );
    }

    if (QAction *entry =
            action("deselectAllRefreshTargets")) {

        entry->setText(
            tx(
                "Selected drive only",
                "Nur ausgewähltes Laufwerk"
            )
        );
    }

    m_rereadAction->setText(
        tx(
            "Reread",
            "Neu einlesen"
        )
    );

    m_liveDetectionAction->setText(
        tx(
            "Live Device Detection",
            "Live-Geräteerkennung"
        )
    );

    m_diagramAction->setText(
        tx(
            "Graph / History",
            "Diagramm / Verlauf"
        )
    );

    m_hideSerialAction->setText(
        tx(
            "Hide Serial Number",
            "Seriennummer ausblenden"
        )
    );

    m_showTrayAction->setText(
        tx(
            "Show in System Tray",
            "Im Infobereich anzeigen"
        )
    );

    m_startWithSystemAction->setText(
        tx(
            "Start with System",
            "Mit System starten"
        )
    );

    m_advancedOptionsMenu->setTitle(
        tx(
            "Advanced Options",
            "Erweiterte Optionen"
        )
    );

    if (QAction *entry = action("aamApmManagementAction"))
        entry->setText(
            tx(
                "AAM/APM Management",
                "AAM/APM-Verwaltung"
            )
        );

    if (QAction *entry = action("aamApmAutoAction"))
        entry->setText(
            tx(
                "AAM/APM Auto Adjustment",
                "Automatische AAM/APM-Anpassung"
            )
        );

    if (QAction *entry = action("stateSettingsAction"))
        entry->setText(
            tx(
                "Health Status Settings",
                "Zustandseinstellungen"
            )
        );

    if (QAction *entry = action("temperatureWarningAction"))
        entry->setText(
            tx(
                "Warning - Temperature",
                "Temperaturwarnung"
            )
        );

    m_temperatureMenu->setTitle(
        tx(
            "Temperature Unit",
            "Temperatureinheit"
        )
    );

    m_autoDetectionMenu->setTitle(
        tx(
            "Auto Detection",
            "Automatische Erkennung"
        )
    );

    for (QAction *entry :
         m_autoDetectionMenu->actions()) {

        if (entry->data().toInt() == 0)
            entry->setText(
                tx(
                    "Disabled",
                    "Deaktivieren"
                )
            );
    }

    m_rawValuesMenu->setTitle(
        tx(
            "Raw Values",
            "Rohwerte"
        )
    );

    m_showRawAction->setText(
        tx(
            "Show Raw Values",
            "Rohwerte anzeigen"
        )
    );

    if (QMenu *entry = menu("startupDelayMenu"))
        entry->setTitle(
            tx(
                "Startup Delay",
                "Verzögerung beim Systemstart"
            )
        );

    if (QMenu *entry = menu("trayBehaviorMenu")) {
        entry->setTitle(
            tx(
                "System Tray Behavior",
                "Infobereich-Verhalten"
            )
        );

        const QList<QAction *> actions =
            entry->actions();

        if (actions.size() >= 2) {
            actions.at(0)->setText(
                tx(
                    "Hide main window",
                    "Hauptfenster ausblenden"
                )
            );

            actions.at(1)->setText(
                tx(
                    "Minimize main window",
                    "Hauptfenster minimieren"
                )
            );
        }
    }

    if (QMenu *entry = menu("driveSortMenu")) {
        entry->setTitle(
            tx(
                "Drive Sort Method",
                "Sortierung der Laufwerke"
            )
        );

        const QList<QAction *> actions =
            entry->actions();

        if (actions.size() >= 2) {
            actions.at(0)->setText(
                tx(
                    "Device path",
                    "Gerätepfad"
                )
            );

            actions.at(1)->setText(
                tx(
                    "Model name",
                    "Modellname"
                )
            );
        }
    }

    if (QMenu *entry = menu("displayDrivesMenu"))
        entry->setTitle(
            tx(
                "Display Number of Drives",
                "Anzahl angezeigter Laufwerke"
            )
        );

    if (QAction *entry = action("advancedDriveSearchAction"))
        entry->setText(
            tx(
                "Advanced Drive Search",
                "Erweiterte Laufwerkssuche"
            )
        );

    if (QAction *entry = action("ataPassThroughAction"))
        entry->setText(
            tx(
                "ATA Pass Through",
                "ATA-Passthrough"
            )
        );

    if (QAction *entry = action("usbIeeeAction"))
        entry->setText(QStringLiteral("USB/IEEE 1394"));

    if (QAction *entry = action("intelAmdRaidAction"))
        entry->setText(QStringLiteral("Intel/AMD RAID"));

    if (QAction *entry = action("amdRaidXpertAction"))
        entry->setText(QStringLiteral("AMD RAIDXpert2"));

    if (QAction *entry = action("megaRaidAction"))
        entry->setText(QStringLiteral("MegaRAID"));

    if (QAction *entry = action("intelVrocAction"))
        entry->setText(QStringLiteral("Intel VROC"));

    m_hideSmartInfoAction->setText(
        tx(
            "Hide S.M.A.R.T. Information",
            "S.M.A.R.T.-Infos ausblenden"
        )
    );

    if (QAction *entry = action("hideNoSmartAction"))
        entry->setText(
            tx(
                "Hide drives without S.M.A.R.T.",
                "Laufwerke ohne S.M.A.R.T. ausblenden"
            )
        );

    m_fontAction->setText(
        tx(
            "Font Settings",
            "Schrifteinstellung"
        )
    );

    m_themeMenu->setTitle(
        tx(
            "Theme",
            "Darstellung"
        )
    );

    m_systemThemeAction->setText(
        tx(
            "System",
            "System"
        )
    );

    m_darkThemeAction->setText(
        tx(
            "Dark",
            "Dunkel"
        )
    );

    m_aboutAction->setText(
        tx(
            "About LinDiskInfo",
            "Über LinDiskInfo"
        )
    );

    m_healthCaption->setText(
        tx(
            "Health Status",
            "Gesundheitsstatus"
        )
    );

    m_temperatureCaption->setText(
        tx(
            "Temperature",
            "Temperatur"
        )
    );

    m_firmwareCaption->setText(
        QStringLiteral("Firmware")
    );

    m_serialCaption->setText(
        tx(
            "Serial Number",
            "Seriennummer"
        )
    );

    m_interfaceCaption->setText(
        tx(
            "Interface",
            "Schnittstelle"
        )
    );

    m_transferCaption->setText(
        tx(
            "Transfer Mode",
            "Übertragungsmodus"
        )
    );

    m_standardCaption->setText(
        QStringLiteral("Standard")
    );

    m_featuresCaption->setText(
        tx(
            "Features",
            "Eigenschaften"
        )
    );

    m_readsCaption->setText(
        tx(
            "Total Host Reads",
            "Gesamt gelesen"
        )
    );

    m_writesCaption->setText(
        tx(
            "Total Host Writes",
            "Gesamt geschrieben"
        )
    );

    m_rotationCaption->setText(
        tx(
            "Rotation Rate",
            "Drehzahl"
        )
    );

    m_powerCyclesCaption->setText(
        tx(
            "Power On Count",
            "Einschaltvorgänge"
        )
    );

    m_powerHoursCaption->setText(
        tx(
            "Power On Hours",
            "Betriebsstunden"
        )
    );

    updateSerialButton();
    rebuildDiskMenu();

    if (m_hasCurrentData)
        renderDevice(
            m_currentDrive,
            m_currentData
        );
    else
        updateRawColumn();

    for (const DriveInfo &drive : m_drives) {
        if (m_driveData.contains(lindiskinfoDriveIdentity(drive)))
            updateDriveButton(
                drive,
                m_driveData.value(lindiskinfoDriveIdentity(drive))
            );
    }


    if (m_storageUnitMenu) {
        m_storageUnitMenu->setTitle(
            tx(
                "Storage Unit",
                "Speichereinheit"
            )
        );

        for (QAction *entry :
             m_storageUnitMenu->actions()) {

            const QString unit =
                entry->data().toString();

            if (unit == QStringLiteral("GB")) {
                entry->setText(
                    tx(
                        "GB (decimal)",
                        "GB (dezimal)"
                    )
                );

            } else if (
                unit == QStringLiteral("GiB")
            ) {

                entry->setText(
                    tx(
                        "GiB (binary)",
                        "GiB (binär)"
                    )
                );

            } else if (
                unit == QStringLiteral("TB")
            ) {

                entry->setText(
                    tx(
                        "TB (decimal)",
                        "TB (dezimal)"
                    )
                );

            } else if (
                unit == QStringLiteral("TiB")
            ) {

                entry->setText(
                    tx(
                        "TiB (binary)",
                        "TiB (binär)"
                    )
                );
            }
        }
    }



    if (QAction *entry =
            action("driveSortDefault")) {

        entry->setText(
            tx(
                "Default (SATA / NVMe / USB)",
                "Standard (SATA / NVMe / USB)"
            )
        );
    }

    if (QAction *entry =
            action("driveSortPath")) {

        entry->setText(
            tx(
                "Device path",
                "Gerätepfad"
            )
        );
    }

    if (QAction *entry =
            action("driveSortModel")) {

        entry->setText(
            tx(
                "Model name",
                "Modellname"
            )
        );
    }

    if (QAction *entry =
            action("driveSortHealth")) {

        entry->setText(
            tx(
                "Health status",
                "Gesundheitszustand"
            )
        );
    }

    if (QAction *entry =
            action("driveSortTemperature")) {

        entry->setText(
            tx(
                "Temperature",
                "Temperatur"
            )
        );
    }

    if (QAction *entry =
            action("displayDrivesAll")) {

        entry->setText(
            tx(
                "All",
                "Alle"
            )
        );
    }



    if (QMenu *entry =
            menu("selfTestMenu")) {

        entry->setTitle(
            tx(
                "Self Tests",
                "Selbsttests"
            )
        );
    }

    if (QAction *entry =
            action(
                "shortSelfTestAction"
            )) {

        entry->setText(
            tx(
                "Short Self-Test",
                "Kurzer Selbsttest"
            )
        );
    }

    if (QAction *entry =
            action(
                "longSelfTestAction"
            )) {

        entry->setText(
            tx(
                "Extended Self-Test",
                "Erweiterter Selbsttest"
            )
        );
    }

    if (QAction *entry =
            action(
                "abortSelfTestAction"
            )) {

        entry->setText(
            tx(
                "Abort Self-Test",
                "Selbsttest abbrechen"
            )
        );
    }

    if (QMenu *entry =
            menu("smartLogsMenu")) {

        entry->setTitle(
            tx(
                "SMART Logs",
                "SMART-Protokolle"
            )
        );
    }

    if (QAction *entry =
            action(
                "selfTestLogAction"
            )) {

        entry->setText(
            tx(
                "Self-Test Status / Log",
                "Selbsttest-Status / Protokoll"
            )
        );
    }

    if (QAction *entry =
            action(
                "errorLogAction"
            )) {

        entry->setText(
            tx(
                "Error Log",
                "Fehlerprotokoll"
            )
        );
    }

    if (QAction *entry =
            action(
                "deviceStatisticsAction"
            )) {

        entry->setText(
            tx(
                "ATA Device Statistics",
                "ATA-Gerätestatistiken"
            )
        );
    }

    if (QAction *entry =
            action(
                "sataPhyAction"
            )) {

        entry->setText(
            tx(
                "SATA PHY Event Counters",
                "SATA-PHY-Ereigniszähler"
            )
        );
    }



    rebuildTrayMenu();
    updateTrayPresentation();

}

QString MainWindow::currentReportText() const
{
    QString report;
    QTextStream out(&report);

    out
        << "LinDiskInfo "
        << QCoreApplication::applicationVersion()
        << "\n";
    out << "===================\n\n";

    if (m_copyIdentifyAction->isChecked()) {
        out << m_titleLabel->text() << "\n\n";

        out << tx("Firmware", "Firmware")
            << ": "
            << m_firmwareValue->text()
            << "\n";

        out << tx("Serial Number", "Seriennummer")
            << ": "
            << (
                m_serialVisible
                    ? m_serialEdit->text()
                    : QStringLiteral("********")
            )
            << "\n";

        out << tx("Interface", "Schnittstelle")
            << ": "
            << m_interfaceValue->text()
            << "\n";

        out << tx("Transfer Mode", "Übertragungsmodus")
            << ": "
            << m_transferValue->text()
            << "\n";

        out << tx("Health Status", "Gesundheitsstatus")
            << ": "
            << m_healthValue->text()
            << "\n";

        out << tx("Temperature", "Temperatur")
            << ": "
            << m_temperatureValue->text()
            << "\n\n";
    }

    if (!m_copySmartDataAction->isChecked())
        return report;

    const QString separator =
        m_copyAsciiAction->isChecked()
            ? QStringLiteral(" | ")
            : QStringLiteral("\t");

    QList<int> columns;

    for (int column = 0;
         column < m_table->columnCount();
         ++column) {

        if (m_table->isColumnHidden(column))
            continue;

        if (!m_copyThresholdAction->isChecked() &&
            column == 6) {
            continue;
        }

        columns.append(column);
    }

    for (int column : columns) {
        const QTableWidgetItem *header =
            m_table->horizontalHeaderItem(column);

        out << (
            header
                ? header->text()
                : QString()
        );

        if (column != columns.last())
            out << separator;
    }

    out << "\n";

    for (int row = 0;
         row < m_table->rowCount();
         ++row) {

        for (int column : columns) {
            const QTableWidgetItem *item =
                m_table->item(row, column);

            out << (
                item
                    ? item->text()
                    : QString()
            );

            if (column != columns.last())
                out << separator;
        }

        out << "\n";
    }

    return report;
}

void MainWindow::saveTextReport()
{
    QString path =
        QFileDialog::getSaveFileName(
            this,
            tx(
                "Save report",
                "Bericht speichern"
            ),
            QStringLiteral(
                "LinDiskInfo_%1.txt"
            ).arg(
                QDateTime::currentDateTime()
                    .toString(
                        QStringLiteral(
                            "yyyy-MM-dd_HH-mm-ss"
                        )
                    )
            ),
            QStringLiteral("Text (*.txt)")
        );

    if (path.isEmpty())
        return;

    if (!path.endsWith(
            QStringLiteral(".txt"),
            Qt::CaseInsensitive
        )) {
        path += QStringLiteral(".txt");
    }

    QFile file(path);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text
        )) {

        QMessageBox::warning(
            this,
            QStringLiteral("LinDiskInfo"),
            tx(
                "Unable to save the file.",
                "Die Datei konnte nicht gespeichert werden."
            )
        );

        return;
    }

    QTextStream out(&file);
    out << currentReportText();
}

void MainWindow::saveImage()
{
    QString path =
        QFileDialog::getSaveFileName(
            this,
            tx(
                "Save image",
                "Bild speichern"
            ),
            QStringLiteral(
                "LinDiskInfo_%1.png"
            ).arg(
                QDateTime::currentDateTime()
                    .toString(
                        QStringLiteral(
                            "yyyy-MM-dd_HH-mm-ss"
                        )
                    )
            ),
            QStringLiteral("PNG (*.png)")
        );

    if (path.isEmpty())
        return;

    if (!path.endsWith(
            QStringLiteral(".png"),
            Qt::CaseInsensitive
        )) {
        path += QStringLiteral(".png");
    }

    if (!grab().save(path, "PNG")) {
        QMessageBox::warning(
            this,
            QStringLiteral("LinDiskInfo"),
            tx(
                "Unable to save the image.",
                "Das Bild konnte nicht gespeichert werden."
            )
        );
    }
}

void MainWindow::copyReport()
{
    QGuiApplication::clipboard()->setText(
        currentReportText()
    );
}

void MainWindow::refreshAllData()
{
    if (m_drives.isEmpty()) {
        refreshDevices();
        return;
    }

    setStatus(
        tx(
            "Refreshing S.M.A.R.T. data...",
            "Aktualisiere S.M.A.R.T.-Daten..."
        )
    );

    for (const DriveInfo &drive : m_drives)
        m_backend->requestDeviceData(drive);
}

void MainWindow::setAutoRefreshInterval(
    int minutes
)
{
    m_autoRefreshMinutes = minutes;

    QSettings().setValue(
        QStringLiteral("autoRefreshMinutes"),
        minutes
    );

    if (!m_autoRefreshTimer)
        return;

    m_autoRefreshTimer->stop();

    if (minutes > 0)
        m_autoRefreshTimer->start(
            minutes * 60 * 1000
        );
}

void MainWindow::setAutoDetectionInterval(
    int seconds
)
{
    m_autoDetectionSeconds = seconds;

    QSettings().setValue(
        QStringLiteral("autoDetectionSeconds"),
        seconds
    );

    if (!m_autoDetectionTimer)
        return;

    m_autoDetectionTimer->stop();

    if (seconds > 0)
        m_autoDetectionTimer->start(
            seconds * 1000
        );
}

void MainWindow::setZoomPercent(
    int percent
)
{
    m_zoomPercent = percent;

    QSettings().setValue(
        QStringLiteral("zoomPercent"),
        percent
    );

    const double factor =
        static_cast<double>(percent) /
        100.0;

    QFont normal = m_baseFont;

    if (normal.pointSizeF() > 0.0)
        normal.setPointSizeF(
            normal.pointSizeF() * factor
        );

    qApp->setFont(normal);

    QFont title = normal;
    title.setPointSizeF(
        normal.pointSizeF() + 7.0 * factor
    );

    m_titleLabel->setFont(title);

    QFont health = normal;
    health.setPointSizeF(
        normal.pointSizeF() + 5.0 * factor
    );
    health.setBold(true);

    m_healthValue->setFont(health);

    QFont temperature = normal;
    temperature.setPointSizeF(
        normal.pointSizeF() + 7.0 * factor
    );
    temperature.setBold(true);

    m_temperatureValue->setFont(
        temperature
    );
}

QString MainWindow::autostartPath() const
{
    return
        QStandardPaths::writableLocation(
            QStandardPaths::ConfigLocation
        ) +
        QStringLiteral(
            "/autostart/lindiskinfo.desktop"
        );
}

void MainWindow::setStartWithSystem(
    bool enabled
)
{
    const QString path =
        autostartPath();

    if (!enabled) {
        QFile::remove(path);
        return;
    }

    QDir().mkpath(
        QFileInfo(path).absolutePath()
    );

    QFile file(path);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text
        )) {

        QSignalBlocker blocker(
            m_startWithSystemAction
        );

        m_startWithSystemAction->setChecked(
            false
        );

        return;
    }

    QString executable =
        QCoreApplication::applicationFilePath();

    executable.replace(
        QLatin1Char('\\'),
        QStringLiteral("\\\\")
    );

    executable.replace(
        QLatin1Char('"'),
        QStringLiteral("\\\"")
    );

    QTextStream out(&file);

    out
        << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=LinDiskInfo\n"
        << "Exec=\"" << executable
        << "\" --autostart\n"
        << "Icon=lindiskinfo\n"
        << "Terminal=false\n";
}


void MainWindow::closeEvent(
    QCloseEvent *event
)
{
    saveUiState();

    if (m_forceQuit ||
        !m_showTrayAction ||
        !m_showTrayAction->isChecked() ||
        !m_trayIcon ||
        !m_trayIcon->isVisible()) {

        QMainWindow::closeEvent(
            event
        );

        return;
    }

    if (m_trayBehavior ==
        QStringLiteral(
            "minimize"
        )) {

        showMinimized();
        event->ignore();

        rebuildTrayMenu();

        return;
    }

    hide();
    event->ignore();

    rebuildTrayMenu();
}




void MainWindow::saveUiState() const
{
    QSettings settings;

    settings.setValue(
        QStringLiteral(
            "windowGeometry"
        ),
        saveGeometry()
    );

    saveCurrentTableWidths();

    if (m_selectedDrive >= 0 &&
        m_selectedDrive <
            m_drives.size()) {

        const DriveInfo &drive =
            m_drives.at(
                m_selectedDrive
            );

        settings.setValue(
            QStringLiteral(
                "lastDrive"
            ),
            drive.name
        );

        settings.setValue(
            QStringLiteral(
                "lastDriveIdentity"
            ),
            lindiskinfoDriveIdentity(
                drive
            )
        );
    }
}


void MainWindow::saveCurrentTableWidths() const
{
    if (!m_table ||
        !m_tableLayoutReady) {
        return;
    }

    QVariantList widths;

    for (int column = 0;
         column <
            m_table->columnCount();
         ++column) {

        widths.append(
            m_table->columnWidth(
                column
            )
        );
    }

    QSettings().setValue(
        m_currentTableIsNvme
            ? QStringLiteral(
                  "tableWidthsNvme"
              )
            : QStringLiteral(
                  "tableWidthsAta"
              ),
        widths
    );
}


void MainWindow::restoreCurrentTableWidths()
{
    if (!m_table)
        return;

    const QVariantList widths =
        QSettings().value(
            m_currentTableIsNvme
                ? QStringLiteral(
                      "tableWidthsNvme"
                  )
                : QStringLiteral(
                      "tableWidthsAta"
                  )
        ).toList();

    if (widths.size() ==
        m_table->columnCount()) {

        for (int column = 0;
             column <
                m_table->columnCount();
             ++column) {

            const int width =
                widths.at(column)
                    .toInt();

            if (width >= 20) {
                m_table->setColumnWidth(
                    column,
                    width
                );
            }
        }
    }

    m_tableLayoutReady = true;
}


void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::
            isSystemTrayAvailable()) {

        m_showTrayAction
            ->setEnabled(false);

        return;
    }

    m_trayIcon =
        new QSystemTrayIcon(
            this
        );

    m_trayMenu =
        new QMenu(this);

    m_trayIcon->setContextMenu(
        m_trayMenu
    );

    m_trayIcon->setIcon(
        QIcon::fromTheme(
            QStringLiteral(
                "lindiskinfo"
            ),
            qApp->windowIcon()
        )
    );

    m_trayIcon->setToolTip(
        QStringLiteral(
            "LinDiskInfo"
        )
    );

    connect(
        m_trayIcon,
        &QSystemTrayIcon::activated,
        this,
        [this](
            QSystemTrayIcon::
                ActivationReason reason
        )
        {
            if (reason !=
                QSystemTrayIcon::Trigger) {

                return;
            }

            if (isVisible() &&
                !isMinimized()) {

                hide();
                return;
            }

            showNormal();
            raise();
            activateWindow();
        }
    );

    connect(
        m_showTrayAction,
        &QAction::toggled,
        this,
        [this](bool)
        {
            rebuildTrayMenu();
            updateTrayPresentation();
        }
    );

    rebuildTrayMenu();
    updateTrayPresentation();

    if (m_showTrayAction->isChecked())
        m_trayIcon->show();
}



void MainWindow::rebuildTrayMenu()
{
    if (!m_trayMenu)
        return;

    m_trayMenu->clear();

    QAction *windowAction =
        m_trayMenu->addAction(
            isVisible() &&
            !isMinimized()
                ? tx(
                      "Hide LinDiskInfo",
                      "LinDiskInfo ausblenden"
                  )
                : tx(
                      "Show LinDiskInfo",
                      "LinDiskInfo anzeigen"
                  )
        );

    connect(
        windowAction,
        &QAction::triggered,
        this,
        [this]
        {
            if (isVisible() &&
                !isMinimized()) {

                hide();

            } else {
                showNormal();
                raise();
                activateWindow();
            }

            rebuildTrayMenu();
        }
    );

    m_trayMenu->addSeparator();


    if (m_drives.isEmpty()) {
        QAction *empty =
            m_trayMenu->addAction(
                tx(
                    "No drives found",
                    "Keine Laufwerke gefunden"
                )
            );

        empty->setEnabled(false);

    } else {
        for (int i = 0;
             i < m_drives.size();
             ++i) {

            const DriveInfo &drive =
                m_drives.at(i);

            QString model =
                drive.infoName;

            if (m_driveData.contains(
                    lindiskinfoDriveIdentity(drive)
                )) {

                const QJsonObject data =
                    m_driveData.value(
                        lindiskinfoDriveIdentity(drive)
                    );

                const QString candidate =
                    data.value(
                        QStringLiteral(
                            "model_name"
                        )
                    ).toString(
                        data.value(
                            QStringLiteral(
                                "model_number"
                            )
                        ).toString()
                    );

                if (!candidate.isEmpty())
                    model = candidate;
            }

            if (model.isEmpty())
                model = drive.name;

            QString stateText =
                QStringLiteral("?");

            QString temperatureText =
                m_temperatureUnit ==
                    TemperatureUnit::
                        Fahrenheit
                    ? QStringLiteral(
                          "-- °F"
                      )
                    : QStringLiteral(
                          "-- °C"
                      );

            if (m_driveData.contains(
                    lindiskinfoDriveIdentity(drive)
                )) {

                const QJsonObject data =
                    m_driveData.value(
                        lindiskinfoDriveIdentity(drive)
                    );

                HealthState state =
                    healthStateForData(
                        data
                    );

                const int temperature =
                    temperatureForData(
                        data
                    );

                if (temperature >=
                    m_temperatureBad) {

                    state =
                        HealthState::Bad;

                } else if (
                    temperature >=
                        m_temperatureCaution &&
                    state !=
                        HealthState::Bad) {

                    state =
                        HealthState::Caution;
                }

                switch (state) {
                case HealthState::Good:
                    stateText =
                        tx(
                            "Good",
                            "Gut"
                        );
                    break;

                case HealthState::Caution:
                    stateText =
                        tx(
                            "Caution",
                            "Vorsicht"
                        );
                    break;

                case HealthState::Bad:
                    stateText =
                        tx(
                            "Bad",
                            "Schlecht"
                        );
                    break;

                case HealthState::Unknown:
                    stateText =
                        tx(
                            "Unknown",
                            "Unbekannt"
                        );
                    break;
                }

                if (temperature > 0) {
                    temperatureText =
                        formatTemperature(
                            temperature
                        );
                }
            }

            QString label;

            if (i == m_selectedDrive)
                label +=
                    QStringLiteral("✓ ");

            label +=
                QStringLiteral(
                    "%1  %2  %3  [%4]"
                ).arg(
                    stateText,
                    temperatureText,
                    model,
                    driveDisplayIdentifier(
                        drive
                    )
                );

            QAction *entry =
                m_trayMenu->addAction(
                    label
                );

            connect(
                entry,
                &QAction::triggered,
                this,
                [this, i]
                {
                    if (i < 0 ||
                        i >=
                            m_drives.size()) {

                        return;
                    }

                    selectDrive(i);

                    showNormal();
                    raise();
                    activateWindow();

                    rebuildTrayMenu();
                }
            );
        }
    }


    m_trayMenu->addSeparator();


    QAction *refresh =
        m_trayMenu->addAction(
            tx(
                "Refresh",
                "Aktualisieren"
            )
        );

    connect(
        refresh,
        &QAction::triggered,
        this,
        &MainWindow::
            refreshAllData
    );


    QAction *quit =
        m_trayMenu->addAction(
            tx(
                "Exit",
                "Beenden"
            )
        );

    connect(
        quit,
        &QAction::triggered,
        this,
        [this]
        {
            saveUiState();

            m_forceQuit = true;

            close();
        }
    );
}


void MainWindow::updateTrayPresentation()
{
    if (!m_trayIcon)
        return;

    QStringList lines;

    lines.append(
        QStringLiteral(
            "LinDiskInfo"
        )
    );

    int worstSeverity = 0;


    for (const DriveInfo &drive :
         m_drives) {

        QString model =
            drive.infoName;

        if (model.isEmpty())
            model = drive.name;

        QString status =
            tx(
                "Unknown",
                "Unbekannt"
            );

        QString temperatureText =
            m_temperatureUnit ==
                TemperatureUnit::
                    Fahrenheit
                ? QStringLiteral(
                      "-- °F"
                  )
                : QStringLiteral(
                      "-- °C"
                  );

        int severity = 0;


        if (m_driveData.contains(
                lindiskinfoDriveIdentity(drive)
            )) {

            const QJsonObject data =
                m_driveData.value(
                    lindiskinfoDriveIdentity(drive)
                );

            const QString dataModel =
                data.value(
                    QStringLiteral(
                        "model_name"
                    )
                ).toString(
                    data.value(
                        QStringLiteral(
                            "model_number"
                        )
                    ).toString()
                );

            if (!dataModel.isEmpty())
                model = dataModel;


            HealthState state =
                healthStateForData(
                    data
                );

            const int temperature =
                temperatureForData(
                    data
                );


            if (temperature >=
                m_temperatureBad) {

                state =
                    HealthState::Bad;

            } else if (
                temperature >=
                    m_temperatureCaution &&
                state !=
                    HealthState::Bad) {

                state =
                    HealthState::
                        Caution;
            }


            switch (state) {
            case HealthState::Good:
                status =
                    tx(
                        "Good",
                        "Gut"
                    );

                severity = 0;
                break;

            case HealthState::Caution:
                status =
                    tx(
                        "Caution",
                        "Vorsicht"
                    );

                severity = 1;
                break;

            case HealthState::Bad:
                status =
                    tx(
                        "Bad",
                        "Schlecht"
                    );

                severity = 2;
                break;

            case HealthState::Unknown:
                status =
                    tx(
                        "Unknown",
                        "Unbekannt"
                    );

                severity = 0;
                break;
            }


            if (temperature > 0) {
                temperatureText =
                    formatTemperature(
                        temperature
                    );
            }


            const QString driveIdentity =
                lindiskinfoDriveIdentity(
                    drive
                );

            if (m_lastTraySeverity
                    .contains(
                        driveIdentity
                    )) {

                const int oldSeverity =
                    m_lastTraySeverity
                        .value(
                            driveIdentity
                        );

                if (severity >
                        oldSeverity &&
                    severity > 0 &&
                    m_trayIcon
                        ->isVisible()) {

                    m_trayIcon->showMessage(
                        QStringLiteral(
                            "LinDiskInfo"
                        ),
                        severity >= 2
                            ? tx(
                                  "Bad health state detected for %1.",
                                  "Schlechter Laufwerkszustand bei %1 erkannt."
                              ).arg(model)
                            : tx(
                                  "Caution state detected for %1.",
                                  "Warnzustand bei %1 erkannt."
                              ).arg(model),
                        severity >= 2
                            ? QSystemTrayIcon::
                                  Critical
                            : QSystemTrayIcon::
                                  Warning,
                        8000
                    );
                }
            }


            m_lastTraySeverity.insert(
                driveIdentity,
                severity
            );
        }


        worstSeverity =
            std::max(
                worstSeverity,
                severity
            );


        QString shortModel =
            model;

        if (shortModel.size() > 36) {
            shortModel =
                shortModel.left(33) +
                QStringLiteral("...");
        }

        if (driveDisplayIdentifier(
                drive
            ) != drive.name &&
            !drive.type.trimmed().isEmpty()) {

            shortModel +=
                QStringLiteral(
                    " [%1]"
                ).arg(
                    drive.type.trimmed()
                );
        }


        lines.append(
            QStringLiteral(
                "%1: %2, %3"
            ).arg(
                shortModel,
                status,
                temperatureText
            )
        );
    }


    m_trayIcon->setToolTip(
        lines.join(
            QLatin1Char('\n')
        )
    );


    QIcon icon;

    if (worstSeverity >= 2) {
        icon =
            QIcon::fromTheme(
                QStringLiteral(
                    "dialog-error"
                ),
                qApp->windowIcon()
            );

    } else if (
        worstSeverity == 1) {

        icon =
            QIcon::fromTheme(
                QStringLiteral(
                    "dialog-warning"
                ),
                qApp->windowIcon()
            );

    } else {
        icon =
            QIcon::fromTheme(
                QStringLiteral(
                    "lindiskinfo"
                ),
                qApp->windowIcon()
            );
    }


    if (!icon.isNull()) {
        m_trayIcon->setIcon(
            icon
        );
    }
}


void MainWindow::rebuildDiskMenu()
{
    if (!m_diskMenu)
        return;

    m_diskMenu->clear();

    if (m_drives.isEmpty()) {
        QAction *entry =
            m_diskMenu->addAction(
                tx(
                    "No drives found",
                    "Keine Laufwerke gefunden"
                )
            );

        entry->setEnabled(false);
        return;
    }

    for (int i = 0;
         i < m_drives.size();
         ++i) {

        const DriveInfo &drive =
            m_drives.at(i);

        if (m_hideNoSmart &&
            m_driveData.contains(
                lindiskinfoDriveIdentity(drive)
            ) &&
            !m_driveData.value(
                lindiskinfoDriveIdentity(drive)
            ).value(
                QStringLiteral(
                    "lindiskinfo_smart_available"
                )
            ).toBool(true)) {

            continue;
        }

        const QString displayId =
            driveDisplayIdentifier(
                drive
            );

        QString label =
            displayId;

        if (m_driveData.contains(
                lindiskinfoDriveIdentity(drive)
            )) {

            const QJsonObject data =
                m_driveData.value(
                    lindiskinfoDriveIdentity(drive)
                );

            const QString model =
                data.value(
                    QStringLiteral("model_name")
                ).toString(
                    data.value(
                        QStringLiteral("model_number")
                    ).toString()
                );

            if (!model.isEmpty()) {
                label =
                    model +
                    QStringLiteral("  [") +
                    displayId +
                    QStringLiteral("]");
            }
        }

        QAction *entry =
            m_diskMenu->addAction(label);

        entry->setCheckable(true);

        entry->setChecked(
            i == m_selectedDrive
        );

        connect(
            entry,
            &QAction::triggered,
            this,
            [this, i]
            {
                selectDrive(i);

                QTimer::singleShot(
                    0,
                    this,
                    &MainWindow::rebuildDiskMenu
                );
            }
        );
    }
}

void MainWindow::refreshDevices()
{
    m_selectedDrive = -1;

    m_drives.clear();
    m_driveData.clear();
    m_driveButtons.clear();

    clearDisplay();

    setStatus(
        tx(
            "Scanning drives...",
            "Suche Laufwerke..."
        )
    );

    m_backend->scanDevices();
}

QString MainWindow::driveDisplayIdentifier(
    const DriveInfo &drive
) const
{
    int samePathCount = 0;

    for (const DriveInfo &candidate :
         m_drives) {

        if (candidate.name ==
            drive.name) {

            ++samePathCount;
        }
    }

    const QString type =
        drive.type.trimmed();

    if (samePathCount > 1 &&
        !type.isEmpty()) {

        return QStringLiteral(
            "%1 [%2]"
        ).arg(
            drive.name,
            type
        );
    }

    return drive.name;
}


void MainWindow::rebuildDriveButtons()
{
    while (m_driveLayout->count() > 0) {
        QLayoutItem *item =
            m_driveLayout->takeAt(0);

        if (item->widget())
            item->widget()->deleteLater();

        delete item;
    }

    m_driveButtons.clear();

    for (int i = 0; i < m_drives.size(); ++i) {
        const DriveInfo &drive =
            m_drives.at(i);

        auto *button =
            new QPushButton(
                QStringLiteral(
                    "...\n-- °C\n%1"
                ).arg(
                    driveDisplayIdentifier(
                        drive
                    )
                )
            );

        button->setCheckable(true);
        button->setMinimumWidth(120);
        button->setMinimumHeight(55);

        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, i]
            {
                selectDrive(i);
            }
        );

        m_driveButtons.append(button);

        m_driveLayout->addWidget(
            button
        );
    }

    m_driveLayout->addStretch();
}

void MainWindow::selectDrive(int index)
{
    if (index < 0 ||
        index >= m_drives.size()) {
        return;
    }

    m_selectedDrive = index;

    for (int i = 0;
         i < m_driveButtons.size();
         ++i) {

        m_driveButtons.at(i)->setChecked(
            i == index
        );
    }

    const DriveInfo drive =
        m_drives.at(index);


    {
        QSettings settings;

        settings.setValue(
            QStringLiteral(
                "lastDrive"
            ),
            drive.name
        );

        settings.setValue(
            QStringLiteral(
                "lastDriveIdentity"
            ),
            lindiskinfoDriveIdentity(
                drive
            )
        );
    }


    // Keep Disk and Tray selection in sync.
    //
    // Deferred rebuild avoids deleting a QAction while
    // its triggered handler is still executing.
    QTimer::singleShot(
        0,
        this,
        &MainWindow::rebuildDiskMenu
    );

    QTimer::singleShot(
        0,
        this,
        &MainWindow::rebuildTrayMenu
    );


    if (m_driveData.contains(
            lindiskinfoDriveIdentity(drive)
        )) {

        renderDevice(
            drive,
            m_driveData.value(
                lindiskinfoDriveIdentity(drive)
            )
        );

        updateTrayPresentation();

        return;
    }


    clearDisplay();

    m_titleLabel->setText(
        driveDisplayIdentifier(
            drive
        )
    );

    setStatus(
        tx(
            "Reading SMART data...",
            "Lese SMART-Daten..."
        )
    );

    updateTrayPresentation();
}


MainWindow::HealthState
MainWindow::healthStateForData(
    const QJsonObject &data,
    int *percentage
) const
{
    if (percentage)
        *percentage = -1;

    const bool smartAvailable =
        data.value(
            QStringLiteral(
                "lindiskinfo_smart_available"
            )
        ).toBool(true);

    if (!smartAvailable)
        return HealthState::Unknown;

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {

        const QJsonObject nvme =
            data.value(
                QStringLiteral(
                    "nvme_smart_health_information_log"
                )
            ).toObject();

        const int remaining =
            std::max(
                0,
                100 -
                static_cast<int>(
                    jsonUnsigned(
                        nvme.value(
                            QStringLiteral(
                                "percentage_used"
                            )
                        )
                    )
                )
            );

        if (percentage)
            *percentage = remaining;

        const quint64 criticalWarning =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "critical_warning"
                    )
                )
            );

        const quint64 mediaErrors =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "media_errors"
                    )
                )
            );

        const quint64 spare =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "available_spare"
                    )
                )
            );

        const quint64 spareThreshold =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "available_spare_threshold"
                    )
                )
            );

        if (criticalWarning != 0 ||
            mediaErrors != 0 ||
            remaining <=
                m_nvmeBadRemaining) {

            return HealthState::Bad;
        }

        if (remaining <=
                m_nvmeCautionRemaining ||
            spare <= spareThreshold) {

            return HealthState::Caution;
        }

        return HealthState::Good;
    }

    if (data.contains(
            QStringLiteral(
                "ata_smart_attributes"
            )
        )) {

        const bool passed =
            data.value(
                QStringLiteral(
                    "smart_status"
                )
            ).toObject()
            .value(
                QStringLiteral("passed")
            ).toBool(true);

        if (!passed)
            return HealthState::Bad;

        const QJsonArray attributes =
            data.value(
                QStringLiteral(
                    "ata_smart_attributes"
                )
            ).toObject()
            .value(
                QStringLiteral("table")
            ).toArray();

        bool caution = false;

        for (const QJsonValue &entry :
             attributes) {

            const QJsonObject attribute =
                entry.toObject();

            const QString name =
                attribute.value(
                    QStringLiteral("name")
                ).toString();

            const int current =
                attribute.value(
                    QStringLiteral("value")
                ).toInt();

            const int threshold =
                attribute.value(
                    QStringLiteral("thresh")
                ).toInt();

            const quint64 raw =
                jsonUnsigned(
                    attribute.value(
                        QStringLiteral("raw")
                    ).toObject()
                    .value(
                        QStringLiteral("value")
                    )
                );

            if (threshold > 0 &&
                current <= threshold) {

                return HealthState::Bad;
            }

            if ((name ==
                    QStringLiteral(
                        "Reallocated_Sector_Ct"
                    ) ||
                 name ==
                    QStringLiteral(
                        "Current_Pending_Sector"
                    ) ||
                 name ==
                    QStringLiteral(
                        "Offline_Uncorrectable"
                    )) &&
                raw >=
                    m_ataSectorCautionCount) {

                caution = true;
            }
        }

        return caution
            ? HealthState::Caution
            : HealthState::Good;
    }

    const QJsonObject status =
        data.value(
            QStringLiteral(
                "smart_status"
            )
        ).toObject();

    if (status.contains(
            QStringLiteral("passed")
        )) {

        return status.value(
            QStringLiteral("passed")
        ).toBool()
            ? HealthState::Good
            : HealthState::Bad;
    }

    return HealthState::Unknown;
}


int MainWindow::temperatureForData(
    const QJsonObject &data
) const
{
    const QJsonObject temperature =
        data.value(
            QStringLiteral(
                "temperature"
            )
        ).toObject();

    if (!temperature.contains(
            QStringLiteral("current")
        )) {
        return -1;
    }

    const int current =
        static_cast<int>(
            jsonUnsigned(
                temperature.value(
                    QStringLiteral("current")
                )
            )
        );

    return current > 0
        ? current
        : -1;
}


void MainWindow::sortDrives()
{
    const QString selectedIdentity =
        (
            m_selectedDrive >= 0 &&
            m_selectedDrive <
                m_drives.size()
        )
        ? lindiskinfoDriveIdentity(
              m_drives.at(
                  m_selectedDrive
              )
          )
        : (
            m_hasCurrentData
                ? lindiskinfoDriveIdentity(
                      m_currentDrive
                  )
                : QString()
        );

    const auto modelFor =
        [this](
            const DriveInfo &drive
        )
        {
            if (m_driveData.contains(
                    lindiskinfoDriveIdentity(drive)
                )) {

                const QJsonObject data =
                    m_driveData.value(
                        lindiskinfoDriveIdentity(drive)
                    );

                const QString model =
                    data.value(
                        QStringLiteral(
                            "model_name"
                        )
                    ).toString(
                        data.value(
                            QStringLiteral(
                                "model_number"
                            )
                        ).toString()
                    );

                if (!model.isEmpty())
                    return model;
            }

            if (!drive.infoName.isEmpty())
                return drive.infoName;

            return drive.model;
        };

    const auto defaultRank =
        [](
            const DriveInfo &drive
        )
        {
            if (drive.transport.compare(
                    QStringLiteral("usb"),
                    Qt::CaseInsensitive
                ) == 0) {
                return 2;
            }

            if (drive.name.startsWith(
                    QStringLiteral(
                        "/dev/nvme"
                    )
                )) {
                return 1;
            }

            return 0;
        };

    std::stable_sort(
        m_drives.begin(),
        m_drives.end(),
        [this,
         &modelFor,
         &defaultRank](
            const DriveInfo &left,
            const DriveInfo &right
        )
        {
            if (m_driveSortMethod ==
                QStringLiteral("path")) {

                return
                    lindiskinfoDriveIdentity(
                        left
                    ) <
                    lindiskinfoDriveIdentity(
                        right
                    );
            }

            if (m_driveSortMethod ==
                QStringLiteral("model")) {

                const QString leftModel =
                    modelFor(left);

                const QString rightModel =
                    modelFor(right);

                const int compare =
                    QString::localeAwareCompare(
                        leftModel,
                        rightModel
                    );

                if (compare != 0)
                    return compare < 0;

                return
                    lindiskinfoDriveIdentity(
                        left
                    ) <
                    lindiskinfoDriveIdentity(
                        right
                    );
            }

            if (m_driveSortMethod ==
                QStringLiteral("health")) {

                const auto rank =
                    [this](
                        const DriveInfo &drive
                    )
                    {
                        if (!m_driveData.contains(
                                lindiskinfoDriveIdentity(drive)
                            )) {
                            return 2;
                        }

                        switch (
                            healthStateForData(
                                m_driveData.value(
                                    lindiskinfoDriveIdentity(drive)
                                )
                            )
                        ) {
                        case HealthState::Bad:
                            return 0;

                        case HealthState::Caution:
                            return 1;

                        case HealthState::Unknown:
                            return 2;

                        case HealthState::Good:
                            return 3;
                        }

                        return 2;
                    };

                const int leftRank =
                    rank(left);

                const int rightRank =
                    rank(right);

                if (leftRank != rightRank)
                    return leftRank <
                        rightRank;

                return
                    lindiskinfoDriveIdentity(
                        left
                    ) <
                    lindiskinfoDriveIdentity(
                        right
                    );
            }

            if (m_driveSortMethod ==
                QStringLiteral(
                    "temperature"
                )) {

                const auto temp =
                    [this](
                        const DriveInfo &drive
                    )
                    {
                        if (!m_driveData.contains(
                                lindiskinfoDriveIdentity(drive)
                            )) {
                            return -1;
                        }

                        return temperatureForData(
                            m_driveData.value(
                                lindiskinfoDriveIdentity(drive)
                            )
                        );
                    };

                const int leftTemp =
                    temp(left);

                const int rightTemp =
                    temp(right);

                if (leftTemp != rightTemp)
                    return leftTemp >
                        rightTemp;

                return
                    lindiskinfoDriveIdentity(
                        left
                    ) <
                    lindiskinfoDriveIdentity(
                        right
                    );
            }

            const int leftRank =
                defaultRank(left);

            const int rightRank =
                defaultRank(right);

            if (leftRank != rightRank)
                return leftRank <
                    rightRank;

            return left.name <
                right.name;
        }
    );

    if (!selectedIdentity.isEmpty()) {
        for (int i = 0;
             i < m_drives.size();
             ++i) {

            if (lindiskinfoDriveIdentity(
                    m_drives.at(i)
                ) ==
                selectedIdentity) {

                m_selectedDrive = i;
                break;
            }
        }
    }
}


void MainWindow::applyDriveButtonLimit()
{
    int displayed = 0;

    for (int i = 0;
         i < m_driveButtons.size();
         ++i) {

        QPushButton *button =
            m_driveButtons.at(i);

        if (!button)
            continue;

        bool allowed = true;

        if (i < m_drives.size() &&
            m_hideNoSmart &&
            m_driveData.contains(
                lindiskinfoDriveIdentity(m_drives.at(i))
            )) {

            allowed =
                m_driveData.value(
                    lindiskinfoDriveIdentity(m_drives.at(i))
                ).value(
                    QStringLiteral(
                        "lindiskinfo_smart_available"
                    )
                ).toBool(true);
        }

        bool withinLimit =
            m_displayDriveLimit <= 0 ||
            displayed <
                m_displayDriveLimit;

        button->setVisible(
            allowed &&
            withinLimit
        );

        if (allowed)
            ++displayed;
    }
}


void MainWindow::refreshDrivePresentation()
{
    QString selectedIdentity;

    if (m_selectedDrive >= 0 &&
        m_selectedDrive <
            m_drives.size()) {

        selectedIdentity =
            lindiskinfoDriveIdentity(
                m_drives.at(
                    m_selectedDrive
                )
            );

    } else if (m_hasCurrentData) {

        selectedIdentity =
            lindiskinfoDriveIdentity(
                m_currentDrive
            );
    }

    sortDrives();

    rebuildDriveButtons();
    applyDriveButtonLimit();
    rebuildDiskMenu();

    for (const DriveInfo &drive :
         m_drives) {

        if (m_driveData.contains(
                lindiskinfoDriveIdentity(drive)
            )) {

            updateDriveButton(
                drive,
                m_driveData.value(
                    lindiskinfoDriveIdentity(drive)
                )
            );
        }
    }

    int target = -1;

    for (int i = 0;
         i < m_drives.size();
         ++i) {

        const DriveInfo &drive =
            m_drives.at(i);

        bool allowed = true;

        if (m_hideNoSmart &&
            m_driveData.contains(
                lindiskinfoDriveIdentity(drive)
            )) {

            allowed =
                m_driveData.value(
                    lindiskinfoDriveIdentity(drive)
                ).value(
                    QStringLiteral(
                        "lindiskinfo_smart_available"
                    )
                ).toBool(true);
        }

        if (!allowed)
            continue;

        if (target < 0)
            target = i;

        if (!selectedIdentity.isEmpty() &&
            lindiskinfoDriveIdentity(
                drive
            ) ==
                selectedIdentity) {

            target = i;
            break;
        }
    }

    if (target >= 0)
        selectDrive(target);
}



QString MainWindow::ataSettingsKey(
    const DriveInfo &drive,
    const QJsonObject &data
) const
{
    QString key =
        data.value(
            QStringLiteral(
                "serial_number"
            )
        ).toString()
        .trimmed();

    if (key.isEmpty()) {
        key =
            lindiskinfoDriveIdentity(
                drive
            );
    }

    key.replace(
        QLatin1Char('/'),
        QLatin1Char('_')
    );

    key.replace(
        QLatin1Char('\\'),
        QLatin1Char('_')
    );

    return key;
}


void MainWindow::maybeAutoAdjustAta(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    if (!m_aamApmAutoEnabled)
        return;

    const bool ataDevice =
        data.contains(
            QStringLiteral(
                "ata_smart_attributes"
            )
        ) ||
        data.contains(
            QStringLiteral("ata_aam")
        ) ||
        data.contains(
            QStringLiteral("ata_apm")
        );

    if (!ataDevice)
        return;

    const QString key =
        ataSettingsKey(
            drive,
            data
        );

    if (m_autoAdjustedAtaDrives
            .contains(key)) {
        return;
    }

    QSettings settings;

    settings.beginGroup(
        QStringLiteral(
            "ataAuto"
        )
    );

    settings.beginGroup(key);

    const QString aam =
        settings.value(
            QStringLiteral("aam")
        ).toString();

    const QString apm =
        settings.value(
            QStringLiteral("apm")
        ).toString();

    settings.endGroup();
    settings.endGroup();

    if (aam.isEmpty() &&
        apm.isEmpty()) {
        return;
    }

    const auto alreadyMatches =
        [&data](
            const char *jsonKey,
            const QString &wanted
        )
        {
            if (wanted.isEmpty())
                return true;

            const QJsonObject object =
                data.value(
                    QString::fromLatin1(
                        jsonKey
                    )
                ).toObject();

            if (object.isEmpty())
                return false;

            if (wanted ==
                QStringLiteral("off")) {

                return !object.value(
                    QStringLiteral(
                        "enabled"
                    )
                ).toBool(false);
            }

            bool ok = false;

            const int level =
                wanted.toInt(&ok);

            return
                ok &&
                object.value(
                    QStringLiteral(
                        "enabled"
                    )
                ).toBool(false) &&
                object.value(
                    QStringLiteral(
                        "level"
                    )
                ).toInt(-1) ==
                    level;
        };

    if (alreadyMatches(
            "ata_aam",
            aam
        ) &&
        alreadyMatches(
            "ata_apm",
            apm
        )) {

        m_autoAdjustedAtaDrives
            .insert(key);

        return;
    }

    m_autoAdjustedAtaDrives
        .insert(key);

    setStatus(
        tx(
            "Applying saved AAM/APM settings...",
            "Gespeicherte AAM/APM-Einstellungen werden angewendet..."
        )
    );

    m_backend->setAtaPowerSettings(
        drive,
        aam,
        apm
    );
}



void MainWindow::showMaintenanceResult(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    const QJsonObject maintenance =
        data.value(
            QStringLiteral(
                "lindiskinfo_maintenance"
            )
        ).toObject();

    if (maintenance.isEmpty())
        return;

    const QString operation =
        maintenance.value(
            QStringLiteral(
                "operation"
            )
        ).toString();

    const bool success =
        maintenance.value(
            QStringLiteral(
                "success"
            )
        ).toBool(false);

    const QString error =
        maintenance.value(
            QStringLiteral(
                "error"
            )
        ).toString();

    const QJsonObject result =
        maintenance.value(
            QStringLiteral(
                "result"
            )
        ).toObject();

    const auto titleForOperation =
        [this, &operation]()
        {
            if (operation ==
                QStringLiteral(
                    "test_short"
                )) {

                return tx(
                    "Short Self-Test",
                    "Kurzer Selbsttest"
                );
            }

            if (operation ==
                QStringLiteral(
                    "test_long"
                )) {

                return tx(
                    "Extended Self-Test",
                    "Erweiterter Selbsttest"
                );
            }

            if (operation ==
                QStringLiteral(
                    "test_abort"
                )) {

                return tx(
                    "Abort Self-Test",
                    "Selbsttest abbrechen"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_selftest"
                )) {

                return tx(
                    "Self-Test Status / Log",
                    "Selbsttest-Status / Protokoll"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_error"
                )) {

                return tx(
                    "SMART Error Log",
                    "SMART-Fehlerprotokoll"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_devstat"
                )) {

                return tx(
                    "ATA Device Statistics",
                    "ATA-Gerätestatistiken"
                );
            }

            if (operation ==
                QStringLiteral(
                    "log_sataphy"
                )) {

                return tx(
                    "SATA PHY Event Counters",
                    "SATA-PHY-Ereigniszähler"
                );
            }

            return tx(
                "Drive Maintenance",
                "Laufwerksdiagnose"
            );
        };

    const QString title =
        titleForOperation();

    QStringList messages;

    const QJsonArray smartctlMessages =
        result.value(
            QStringLiteral(
                "smartctl"
            )
        ).toObject()
        .value(
            QStringLiteral(
                "messages"
            )
        ).toArray();

    for (const QJsonValue &value :
         smartctlMessages) {

        QString text;

        if (value.isObject()) {
            text =
                value.toObject()
                    .value(
                        QStringLiteral(
                            "string"
                        )
                    ).toString();

        } else if (value.isString()) {
            text = value.toString();
        }

        if (!text.isEmpty())
            messages.append(text);
    }

    const bool commandOperation =
        operation ==
            QStringLiteral(
                "test_short"
            ) ||
        operation ==
            QStringLiteral(
                "test_long"
            ) ||
        operation ==
            QStringLiteral(
                "test_abort"
            );

    const QByteArray prettyJson =
        QJsonDocument(result)
            .toJson(
                QJsonDocument::Indented
            );

    if (commandOperation) {
        QMessageBox box(this);

        box.setWindowTitle(
            QStringLiteral(
                "LinDiskInfo - "
            ) + title
        );

        box.setIcon(
            success
                ? QMessageBox::Information
                : QMessageBox::Warning
        );

        if (success) {
            if (operation ==
                QStringLiteral(
                    "test_abort"
                )) {

                box.setText(
                    tx(
                        "The self-test abort command was accepted.",
                        "Der Befehl zum Abbrechen des Selbsttests wurde akzeptiert."
                    )
                );

            } else {
                box.setText(
                    tx(
                        "The drive accepted the self-test command.",
                        "Das Laufwerk hat den Selbsttest-Befehl akzeptiert."
                    )
                );
            }

        } else {
            box.setText(
                tx(
                    "The drive rejected the requested operation or does not support it.",
                    "Das Laufwerk hat die angeforderte Funktion abgelehnt oder unterstützt sie nicht."
                )
            );
        }

        QString info;

        if (!messages.isEmpty()) {
            info =
                messages.join(
                    QLatin1Char('\n')
                );
        }

        if (!error.isEmpty()) {
            if (!info.isEmpty())
                info += QStringLiteral("\n\n");

            info += error;
        }

        if (!info.isEmpty()) {
            box.setInformativeText(
                info
            );
        }

        if (!prettyJson.isEmpty()) {
            box.setDetailedText(
                QString::fromUtf8(
                    prettyJson
                )
            );
        }

        box.exec();

        if (success) {
            setStatus(
                tx(
                    "Drive maintenance command completed.",
                    "Laufwerksdiagnose-Befehl ausgeführt."
                )
            );
        }

        return;
    }


    QDialog dialog(this);

    dialog.setWindowTitle(
        QStringLiteral(
            "LinDiskInfo - "
        ) + title
    );

    dialog.resize(
        900,
        650
    );

    auto *layout =
        new QVBoxLayout(
            &dialog
        );

    QString heading =
        QStringLiteral("<b>") +
        title +
        QStringLiteral("</b><br>") +
        drive.name;

    auto *label =
        new QLabel(
            heading,
            &dialog
        );

    label->setTextFormat(
        Qt::RichText
    );

    layout->addWidget(label);

    if (!success) {
        auto *errorLabel =
            new QLabel(
                error.isEmpty()
                    ? tx(
                          "The requested log is not supported by this drive.",
                          "Das angeforderte Protokoll wird von diesem Laufwerk nicht unterstützt."
                      )
                    : error,
                &dialog
            );

        errorLabel->setWordWrap(true);

        layout->addWidget(
            errorLabel
        );
    }

    auto *text =
        new QPlainTextEdit(
            &dialog
        );

    text->setReadOnly(true);

    text->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
        )
    );

    if (!prettyJson.isEmpty()) {
        text->setPlainText(
            QString::fromUtf8(
                prettyJson
            )
        );

    } else if (!error.isEmpty()) {
        text->setPlainText(error);

    } else {
        text->setPlainText(
            tx(
                "No log data returned.",
                "Keine Protokolldaten zurückgegeben."
            )
        );
    }

    layout->addWidget(
        text,
        1
    );

    auto *buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Close,
            &dialog
        );

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    layout->addWidget(buttons);

    dialog.exec();
}



QString MainWindow::historyDeviceKey(
    const DriveInfo &drive,
    const QJsonObject &data
) const
{
    QString rawKey =
        data.value(
            QStringLiteral(
                "serial_number"
            )
        ).toString()
        .trimmed();


    if (rawKey.isEmpty()) {
        rawKey =
            data.value(
                QStringLiteral(
                    "model_name"
                )
            ).toString()
            .trimmed();


        if (rawKey.isEmpty()) {
            rawKey =
                data.value(
                    QStringLiteral(
                        "model_number"
                    )
                ).toString()
                .trimmed();
        }


        if (!rawKey.isEmpty()) {
            rawKey +=
                QStringLiteral("|") +
                lindiskinfoDriveIdentity(
                    drive
                );
        }
    }


    if (rawKey.isEmpty()) {
        rawKey =
            lindiskinfoDriveIdentity(
                drive
            );
    }


    const QByteArray digest =
        QCryptographicHash::hash(
            rawKey.toUtf8(),
            QCryptographicHash::Sha256
        ).toHex();


    return
        QStringLiteral("sha256:") +
        QString::fromLatin1(
            digest.constData(),
            digest.size()
        );
}


void MainWindow::loadHistory()
{
    m_historySamples =
        QJsonArray();


    const QString directory =
        QStandardPaths::
            writableLocation(
                QStandardPaths::
                    AppDataLocation
            );


    if (directory.isEmpty())
        return;


    QFile file(
        directory +
        QStringLiteral(
            "/history.json"
        )
    );


    if (!file.open(
            QIODevice::ReadOnly
        )) {

        return;
    }


    QJsonParseError error;


    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &error
        );


    if (error.error !=
            QJsonParseError::NoError ||
        !document.isArray()) {

        return;
    }


    const QJsonArray loaded =
        document.array();


    const qint64 cutoff =
        QDateTime::
            currentMSecsSinceEpoch() -
        90LL *
        24LL *
        60LL *
        60LL *
        1000LL;


    QJsonArray retained;

    bool changed = false;


    for (const QJsonValue &value :
         loaded) {

        QJsonObject sample =
            value.toObject();


        const qint64 timestamp =
            sample.value(
                QStringLiteral(
                    "timestamp"
                )
            ).toString()
            .toLongLong();


        if (timestamp < cutoff) {
            changed = true;
            continue;
        }


        const QString oldKey =
            sample.value(
                QStringLiteral(
                    "device_key"
                )
            ).toString()
            .trimmed();


        if (!oldKey.isEmpty() &&
            !oldKey.startsWith(
                QStringLiteral(
                    "sha256:"
                )
            )) {

            const QByteArray digest =
                QCryptographicHash::hash(
                    oldKey.toUtf8(),
                    QCryptographicHash::Sha256
                ).toHex();


            sample.insert(
                QStringLiteral(
                    "device_key"
                ),
                QStringLiteral(
                    "sha256:"
                ) +
                QString::fromLatin1(
                    digest.constData(),
                    digest.size()
                )
            );


            changed = true;
        }


        retained.append(
            sample
        );
    }


    m_historySamples =
        retained;


    if (changed)
        saveHistory();
}


void MainWindow::saveHistory() const
{
    const QString directory =
        QStandardPaths::
            writableLocation(
                QStandardPaths::
                    AppDataLocation
            );

    if (directory.isEmpty())
        return;

    if (!QDir().mkpath(directory))
        return;

    QSaveFile file(
        directory +
        QStringLiteral(
            "/history.json"
        )
    );

    if (!file.open(
            QIODevice::WriteOnly
        )) {
        return;
    }

    const QJsonDocument document(
        m_historySamples
    );

    file.write(
        document.toJson(
            QJsonDocument::Compact
        )
    );

    file.commit();
}


void MainWindow::recordHistorySample(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    if (!m_historyEnabled)
        return;

    const qint64 now =
        QDateTime::
            currentMSecsSinceEpoch();

    const QString key =
        historyDeviceKey(
            drive,
            data
        );

    const qint64 previous =
        m_lastHistorySampleMs
            .value(
                key,
                0
            );

    // Do not turn normal fast refreshes into
    // thousands of history points.
    if (previous > 0 &&
        now - previous <
            60LL * 1000LL) {

        return;
    }

    m_lastHistorySampleMs.insert(
        key,
        now
    );

    QJsonObject sample;

    sample.insert(
        QStringLiteral("timestamp"),
        QString::number(now)
    );

    sample.insert(
        QStringLiteral("device_key"),
        key
    );

    sample.insert(
        QStringLiteral("device"),
        drive.name
    );

    QString model =
        data.value(
            QStringLiteral(
                "model_name"
            )
        ).toString(
            data.value(
                QStringLiteral(
                    "model_number"
                )
            ).toString()
        );

    if (!model.isEmpty()) {
        sample.insert(
            QStringLiteral("model"),
            model
        );
    }

    const int temperature =
        temperatureForData(data);

    if (temperature > 0) {
        sample.insert(
            QStringLiteral(
                "temperature_c"
            ),
            temperature
        );
    }

    int percentage = -1;

    const HealthState health =
        healthStateForData(
            data,
            &percentage
        );

    int healthValue = percentage;

    if (healthValue < 0) {
        switch (health) {
        case HealthState::Good:
            healthValue = 100;
            break;

        case HealthState::Caution:
            healthValue = 50;
            break;

        case HealthState::Bad:
            healthValue = 0;
            break;

        case HealthState::Unknown:
            healthValue = -1;
            break;
        }
    }

    if (healthValue >= 0) {
        sample.insert(
            QStringLiteral(
                "health_percent"
            ),
            healthValue
        );
    }

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {

        const QJsonObject nvme =
            data.value(
                QStringLiteral(
                    "nvme_smart_health_information_log"
                )
            ).toObject();

        const quint64 unitsRead =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "data_units_read"
                    )
                )
            );

        const quint64 unitsWritten =
            jsonUnsigned(
                nvme.value(
                    QStringLiteral(
                        "data_units_written"
                    )
                )
            );

        const quint64 maxUnits =
            std::numeric_limits<
                quint64
            >::max() /
            512000ULL;

        if (unitsRead <= maxUnits) {
            sample.insert(
                QStringLiteral(
                    "read_bytes"
                ),
                QString::number(
                    unitsRead *
                    512000ULL
                )
            );
        }

        if (unitsWritten <= maxUnits) {
            sample.insert(
                QStringLiteral(
                    "write_bytes"
                ),
                QString::number(
                    unitsWritten *
                    512000ULL
                )
            );
        }
    }

    m_historySamples.append(
        sample
    );

    const qint64 cutoff =
        now -
        90LL *
        24LL *
        60LL *
        60LL *
        1000LL;

    QJsonArray retained;

    const qsizetype startIndex =
        m_historySamples.size() >
            50000
            ? m_historySamples.size() -
                50000
            : 0;

    for (qsizetype i = startIndex;
         i < m_historySamples.size();
         ++i) {

        const QJsonObject entry =
            m_historySamples.at(i)
                .toObject();

        const qint64 timestamp =
            entry.value(
                QStringLiteral(
                    "timestamp"
                )
            ).toString()
            .toLongLong();

        if (timestamp >= cutoff)
            retained.append(entry);
    }

    m_historySamples = retained;

    saveHistory();
}


void MainWindow::openHistoryGraph()
{
    if (!m_hasCurrentData) {
        QMessageBox::information(
            this,
            QStringLiteral(
                "LinDiskInfo"
            ),
            tx(
                "No drive is currently selected.",
                "Derzeit ist kein Laufwerk ausgewählt."
            )
        );

        return;
    }

    const QString key =
        historyDeviceKey(
            m_currentDrive,
            m_currentData
        );

    QDialog dialog(this);

    dialog.setWindowTitle(
        tx(
            "Graph / History",
            "Diagramm / Verlauf"
        )
    );

    dialog.resize(
        980,
        650
    );

    auto *layout =
        new QVBoxLayout(
            &dialog
        );

    auto *heading =
        new QLabel(
            m_titleLabel->text(),
            &dialog
        );

    QFont headingFont =
        heading->font();

    headingFont.setBold(true);

    heading->setFont(
        headingFont
    );

    layout->addWidget(
        heading
    );

    auto *controls =
        new QHBoxLayout;

    auto *metric =
        new QComboBox(
            &dialog
        );

    metric->addItem(
        tx(
            "Temperature",
            "Temperatur"
        ),
        QStringLiteral(
            "temperature"
        )
    );

    metric->addItem(
        tx(
            "Health / Wear",
            "Zustand / Verschleiß"
        ),
        QStringLiteral(
            "health"
        )
    );

    metric->addItem(
        tx(
            "Total Reads",
            "Gesamt gelesen"
        ),
        QStringLiteral(
            "reads"
        )
    );

    metric->addItem(
        tx(
            "Total Writes",
            "Gesamt geschrieben"
        ),
        QStringLiteral(
            "writes"
        )
    );

    auto *range =
        new QComboBox(
            &dialog
        );

    const struct
    {
        const char *en;
        const char *de;
        qint64 seconds;
    }
    ranges[] =
    {
        {
            "1 hour",
            "1 Stunde",
            3600
        },
        {
            "6 hours",
            "6 Stunden",
            21600
        },
        {
            "24 hours",
            "24 Stunden",
            86400
        },
        {
            "7 days",
            "7 Tage",
            604800
        },
        {
            "30 days",
            "30 Tage",
            2592000
        },
        {
            "90 days",
            "90 Tage",
            7776000
        }
    };

    for (const auto &item : ranges) {
        range->addItem(
            tx(
                item.en,
                item.de
            ),
            QVariant::fromValue<
                qlonglong
            >(
                item.seconds
            )
        );
    }

    range->setCurrentIndex(2);

    auto *recording =
        new QCheckBox(
            tx(
                "Record history",
                "Verlauf aufzeichnen"
            ),
            &dialog
        );

    recording->setChecked(
        m_historyEnabled
    );

    auto *clearButton =
        new QPushButton(
            tx(
                "Clear drive history",
                "Laufwerksverlauf löschen"
            ),
            &dialog
        );

    controls->addWidget(
        metric
    );

    controls->addWidget(
        range
    );

    controls->addSpacing(12);

    controls->addWidget(
        recording
    );

    controls->addStretch(1);

    controls->addWidget(
        clearButton
    );

    layout->addLayout(
        controls
    );

    auto *summary =
        new QLabel(
            &dialog
        );

    layout->addWidget(
        summary
    );

    auto *graph =
        new LinDiskHistoryPlot(
            &dialog
        );

    layout->addWidget(
        graph,
        1
    );

    auto *buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Close,
            &dialog
        );

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
    );

    layout->addWidget(
        buttons
    );


    const auto updateGraph =
        [this,
         key,
         metric,
         range,
         graph,
         summary]
        {
            const QString mode =
                metric->currentData()
                    .toString();

            const qint64 seconds =
                range->currentData()
                    .toLongLong();

            const qint64 now =
                QDateTime::
                    currentMSecsSinceEpoch();

            const qint64 cutoff =
                now -
                seconds *
                1000LL;

            QVector<QPointF> points;

            QString unit;

            bool percentageRange = false;

            long double byteDivisor =
                1000000000.0L;

            const QString storageUnit =
                QSettings().value(
                    QStringLiteral(
                        "storageUnit"
                    ),
                    QStringLiteral("GB")
                ).toString();

            if (storageUnit ==
                QStringLiteral("GiB")) {

                byteDivisor =
                    1073741824.0L;

            } else if (
                storageUnit ==
                QStringLiteral("TB")
            ) {

                byteDivisor =
                    1000000000000.0L;

            } else if (
                storageUnit ==
                QStringLiteral("TiB")
            ) {

                byteDivisor =
                    1099511627776.0L;
            }

            for (const QJsonValue &value :
                 m_historySamples) {

                const QJsonObject sample =
                    value.toObject();

                if (sample.value(
                        QStringLiteral(
                            "device_key"
                        )
                    ).toString() != key) {

                    continue;
                }

                const qint64 timestamp =
                    sample.value(
                        QStringLiteral(
                            "timestamp"
                        )
                    ).toString()
                    .toLongLong();

                if (timestamp < cutoff)
                    continue;

                double y = 0.0;

                bool available = false;

                if (mode ==
                    QStringLiteral(
                        "temperature"
                    )) {

                    if (sample.contains(
                            QStringLiteral(
                                "temperature_c"
                            )
                        )) {

                        double temperature =
                            sample.value(
                                QStringLiteral(
                                    "temperature_c"
                                )
                            ).toDouble();

                        if (m_temperatureUnit ==
                            TemperatureUnit::
                                Fahrenheit) {

                            temperature =
                                temperature *
                                9.0 / 5.0 +
                                32.0;

                            unit =
                                QStringLiteral(
                                    "°F"
                                );

                        } else {
                            unit =
                                QStringLiteral(
                                    "°C"
                                );
                        }

                        y = temperature;
                        available = true;
                    }

                } else if (
                    mode ==
                    QStringLiteral(
                        "health"
                    )) {

                    if (sample.contains(
                            QStringLiteral(
                                "health_percent"
                            )
                        )) {

                        y =
                            sample.value(
                                QStringLiteral(
                                    "health_percent"
                                )
                            ).toDouble();

                        unit =
                            QStringLiteral("%");

                        percentageRange = true;
                        available = true;
                    }

                } else if (
                    mode ==
                    QStringLiteral(
                        "reads"
                    ) ||
                    mode ==
                    QStringLiteral(
                        "writes"
                    )) {

                    const QString field =
                        mode ==
                        QStringLiteral(
                            "reads"
                        )
                            ? QStringLiteral(
                                  "read_bytes"
                              )
                            : QStringLiteral(
                                  "write_bytes"
                              );

                    if (sample.contains(
                            field
                        )) {

                        bool ok = false;

                        const quint64 bytes =
                            sample.value(field)
                                .toString()
                                .toULongLong(
                                    &ok
                                );

                        if (ok) {
                            y =
                                static_cast<
                                    double
                                >(
                                    static_cast<
                                        long double
                                    >(bytes) /
                                    byteDivisor
                                );

                            unit =
                                storageUnit;

                            available = true;
                        }
                    }
                }

                if (available) {
                    points.append(
                        QPointF(
                            static_cast<
                                double
                            >(timestamp),
                            y
                        )
                    );
                }
            }

            std::sort(
                points.begin(),
                points.end(),
                [](
                    const QPointF &a,
                    const QPointF &b
                )
                {
                    return a.x() <
                        b.x();
                }
            );

            graph->setSamples(
                points,
                unit,
                tx(
                    "No history data is available for this metric and time range yet.",
                    "Für diese Messgröße und diesen Zeitraum liegen noch keine Verlaufsdaten vor."
                ),
                percentageRange
            );

            summary->setText(
                tx(
                    "%1 samples in selected range",
                    "%1 Messpunkte im gewählten Zeitraum"
                ).arg(
                    points.size()
                )
            );
        };


    connect(
        metric,
        &QComboBox::currentTextChanged,
        &dialog,
        [updateGraph](const QString &)
        {
            updateGraph();
        }
    );

    connect(
        range,
        &QComboBox::currentTextChanged,
        &dialog,
        [updateGraph](const QString &)
        {
            updateGraph();
        }
    );

    connect(
        recording,
        &QCheckBox::toggled,
        &dialog,
        [this](bool enabled)
        {
            m_historyEnabled =
                enabled;

            QSettings().setValue(
                QStringLiteral(
                    "historyEnabled"
                ),
                enabled
            );
        }
    );

    connect(
        clearButton,
        &QPushButton::clicked,
        &dialog,
        [this,
         key,
         updateGraph]
        {
            const auto answer =
                QMessageBox::question(
                    this,
                    QStringLiteral(
                        "LinDiskInfo"
                    ),
                    tx(
                        "Delete the stored history for the selected drive?",
                        "Gespeicherten Verlauf des ausgewählten Laufwerks löschen?"
                    ),
                    QMessageBox::Yes |
                    QMessageBox::No,
                    QMessageBox::No
                );

            if (answer !=
                QMessageBox::Yes) {

                return;
            }

            QJsonArray retained;

            for (const QJsonValue &value :
                 m_historySamples) {

                const QJsonObject sample =
                    value.toObject();

                if (sample.value(
                        QStringLiteral(
                            "device_key"
                        )
                    ).toString() != key) {

                    retained.append(
                        sample
                    );
                }
            }

            m_historySamples =
                retained;

            m_lastHistorySampleMs
                .remove(key);

            saveHistory();
            updateGraph();
        }
    );

    updateGraph();

    dialog.exec();
}


void MainWindow::updateDriveButton(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    int index = -1;

    for (int i = 0;
         i < m_drives.size();
         ++i) {

        if (lindiskinfoDriveIdentity(
                m_drives.at(i)
            ) ==
            lindiskinfoDriveIdentity(
                drive
            )) {

            index = i;
            break;
        }
    }

    if (index < 0 ||
        index >=
            m_driveButtons.size()) {
        return;
    }

    int percentage = -1;

    HealthState state =
        healthStateForData(
            data,
            &percentage
        );

    const int temperature =
        temperatureForData(data);

    if (temperature >=
        m_temperatureBad) {

        state =
            HealthState::Bad;

    } else if (
        temperature >=
            m_temperatureCaution &&
        state != HealthState::Bad) {

        state =
            HealthState::Caution;
    }

    QString health;

    switch (state) {
    case HealthState::Good:
        health =
            tx(
                "Good",
                "Gut"
            );
        break;

    case HealthState::Caution:
        health =
            tx(
                "Caution",
                "Vorsicht"
            );
        break;

    case HealthState::Bad:
        health =
            tx(
                "Bad",
                "Schlecht"
            );
        break;

    case HealthState::Unknown:
        health =
            tx(
                "Unknown",
                "Unbekannt"
            );
        break;
    }

    if (percentage >= 0) {
        health +=
            QStringLiteral(" %1%")
                .arg(percentage);
    }

    const QString temperatureText =
        temperature > 0
            ? formatTemperature(
                  temperature
              )
            : (
                m_temperatureUnit ==
                    TemperatureUnit::Fahrenheit
                    ? QStringLiteral(
                          "-- °F"
                      )
                    : QStringLiteral(
                          "-- °C"
                      )
            );

    QPushButton *button =
        m_driveButtons.at(index);

    button->setText(
        health +
        QStringLiteral("\n") +
        temperatureText +
        QStringLiteral("\n") +
        driveDisplayIdentifier(
            drive
        )
    );

    QString background;
    QString hover;
    QString pressed;
    QString foreground;

    switch (state) {
    case HealthState::Good:
        background =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #B7F3C1,"
                "stop:0.16 #8BE69B,"
                "stop:0.52 #68D47E,"
                "stop:1 #43B85D)"
            );

        hover =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #C9F7D0,"
                "stop:0.18 #A0EDA9,"
                "stop:0.55 #7DDE90,"
                "stop:1 #55C46B)"
            );

        pressed =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #76D88A,"
                "stop:1 #3DA952)"
            );

        foreground =
            QStringLiteral("#101410");
        break;

    case HealthState::Caution:
        background =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #FFF3B0,"
                "stop:0.18 #FFE77D,"
                "stop:0.55 #FFD64F,"
                "stop:1 #DDB52A)"
            );

        hover =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #FFF8C7,"
                "stop:0.18 #FFED97,"
                "stop:0.55 #FFE176,"
                "stop:1 #E5C03D)"
            );

        pressed =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #F0D15E,"
                "stop:1 #C69B1E)"
            );

        foreground =
            QStringLiteral("#171408");
        break;

    case HealthState::Bad:
        background =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #F07A7A,"
                "stop:0.18 #DE5656,"
                "stop:0.55 #C73535,"
                "stop:1 #8F1E1E)"
            );

        hover =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #F58B8B,"
                "stop:0.2 #E76565,"
                "stop:0.55 #D74444,"
                "stop:1 #A02424)"
            );

        pressed =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #C83C3C,"
                "stop:1 #841818)"
            );

        foreground =
            QStringLiteral("#FFFFFF");
        break;

    case HealthState::Unknown:
        background =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #B8E2FF,"
                "stop:0.16 #8CCDF8,"
                "stop:0.52 #63B7EF,"
                "stop:1 #3C94D2)"
            );

        hover =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #D0ECFF,"
                "stop:0.18 #A3DAFB,"
                "stop:0.55 #78C5F4,"
                "stop:1 #4BA2DD)"
            );

        pressed =
            QStringLiteral(
                "qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #72C0F2,"
                "stop:1 #3588C4)"
            );

        foreground =
            QStringLiteral("#101418");
        break;
    }

    button->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            " background: %1;"
            " color: %2;"
            " border:"
            " 1px solid rgba(0,0,0,70);"
            " border-radius: 3px;"
            " padding: 4px 9px;"
            " font-weight: 600;"
            "}"
            "QPushButton:hover {"
            " background: %3;"
            "}"
            "QPushButton:pressed {"
            " background: %4;"
            "}"
            "QPushButton:checked {"
            " border:"
            " 1px solid rgba("
            "255,255,255,165);"
            "}"
        )
        .arg(background)
        .arg(foreground)
        .arg(hover)
        .arg(pressed)
    );
}


void MainWindow::renderDevice(
    const DriveInfo &drive,
    const QJsonObject &data
)
{
    m_currentDrive = drive;
    m_currentData = data;
    m_hasCurrentData = true;

    m_readsValue->setText(
        QStringLiteral("—")
    );

    m_writesValue->setText(
        QStringLiteral("—")
    );

    setHealth(
        HealthState::Unknown
    );

    const QString model =
        data.value(
            QStringLiteral("model_name")
        ).toString(
            data.value(
                QStringLiteral("model_number")
            ).toString()
        );

    const quint64 capacity =
        jsonUnsigned(
            data.value(
                QStringLiteral("user_capacity")
            ).toObject()
            .value(
                QStringLiteral("bytes")
            )
        );

    QString title =
        model.isEmpty()
        ? drive.name
        : model;

    if (capacity > 0) {
        title +=
            QStringLiteral(" : ") +
            formatBytes(capacity);
    }

    m_titleLabel->setText(title);

    m_firmwareValue->setText(
        data.value(
            QStringLiteral("firmware_version")
        ).toString(
            QStringLiteral("—")
        )
    );

    m_serialEdit->setText(
        data.value(
            QStringLiteral("serial_number")
        ).toString()
    );

    m_interfaceValue->setText(
        protocolName(
            drive,
            data
        )
    );

    const QString transport =
        data.value(
            QStringLiteral(
                "lindiskinfo_transport"
            )
        ).toString().trimmed().toLower();

    const bool usbDevice =
        transport ==
        QStringLiteral("usb");

    const auto localizedUsbProtocol =
        [this](QString value)
        {
            if (m_language !=
                Language::German) {

                return value;
            }

            value.replace(
                QStringLiteral(
                    "Mass Storage"
                ),
                QStringLiteral(
                    "Massenspeicher"
                )
            );

            value.replace(
                QStringLiteral(
                    "Bulk-Only"
                ),
                QStringLiteral(
                    "Bulk-Only-Transport"
                )
            );

            return value;
        };

    const auto localizedUsbPowerSource =
        [this](const QString &value)
        {
            if (m_language !=
                Language::German) {

                return value;
            }

            if (value ==
                QStringLiteral(
                    "Bus Powered"
                )) {

                return QString(
                    QStringLiteral(
                        "Busgespeist"
                    )
                );
            }

            if (value ==
                QStringLiteral(
                    "Self Powered"
                )) {

                return QString(
                    QStringLiteral(
                        "Eigenversorgt"
                    )
                );
            }

            if (value ==
                QStringLiteral(
                    "Bus/Self Powered"
                )) {

                return QString(
                    QStringLiteral(
                        "Bus-/Eigenversorgung"
                    )
                );
            }

            if (value ==
                QStringLiteral(
                    "Unknown"
                )) {

                return QString(
                    QStringLiteral(
                        "Unbekannt"
                    )
                );
            }

            return value;
        };

    m_transferValue->setText(
        transferString(data)
    );

    {
        const QString currentPcie =
            data.value(
                QStringLiteral(
                    "lindiskinfo_nvme_current_transfer_mode"
                )
            ).toString();

        const QString maxPcie =
            data.value(
                QStringLiteral(
                    "lindiskinfo_nvme_max_transfer_mode"
                )
            ).toString();

        if (!currentPcie.isEmpty()) {
            QString transferMode =
                currentPcie;

            if (!maxPcie.isEmpty()) {
                transferMode +=
                    QStringLiteral(" | ") +
                    maxPcie;
            }

            m_transferValue->setText(
                transferMode
            );
        }
    }

    if (usbDevice) {
        const quint64 usbMbps =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_link_mbps"
                    )
                )
            );

        const QString usbSpecification =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_specification"
                )
            ).toString();

        QString interfaceName =
            usbSpecification;

        if (usbSpecification ==
                QStringLiteral("USB 3.2") &&
            usbMbps >= 20000) {

            interfaceName +=
                QStringLiteral(" Gen 2x2");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.2") &&
            usbMbps >= 10000) {

            interfaceName +=
                QStringLiteral(" Gen 2");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.2") &&
            usbMbps >= 5000) {

            interfaceName +=
                QStringLiteral(" Gen 1");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.1") &&
            usbMbps >= 10000) {

            interfaceName +=
                QStringLiteral(" Gen 2");

        } else if (
            usbSpecification ==
                QStringLiteral("USB 3.1") &&
            usbMbps >= 5000) {

            interfaceName +=
                QStringLiteral(" Gen 1");
        }

        if (!interfaceName.isEmpty()) {
            m_interfaceValue->setText(
                interfaceName
            );
        }

        if (usbMbps >= 1000) {
            m_transferValue->setText(
                QStringLiteral("%1 Gb/s")
                    .arg(
                        static_cast<double>(
                            usbMbps
                        ) / 1000.0,
                        0,
                        'f',
                        1
                    )
            );
        } else if (usbMbps > 0) {
            m_transferValue->setText(
                QStringLiteral("%1 Mb/s")
                    .arg(usbMbps)
            );
        }
    }

    m_standardValue->setText(
        versionString(data)
    );

    if (usbDevice) {
        const QString usbProtocol =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_protocol"
                )
            ).toString();

        if (!usbProtocol.isEmpty()) {
            m_standardValue->setText(
                localizedUsbProtocol(
                    usbProtocol
                )
            );
        }
    }

    const bool smartAvailable =
        data.value(
            QStringLiteral(
                "lindiskinfo_smart_available"
            )
        ).toBool(true);

    m_featuresValue->setText(
        smartAvailable
            ? QStringLiteral("S.M.A.R.T.")
            : tx(
                  "S.M.A.R.T. not supported",
                  "S.M.A.R.T. nicht unterstützt"
              )
    );

    const QJsonObject temperature =
        data.value(
            QStringLiteral("temperature")
        ).toObject();

    if (temperature.contains(
            QStringLiteral("current")
        )) {

        const int currentTemperature =
            static_cast<int>(
                jsonUnsigned(
                    temperature.value(
                        QStringLiteral("current")
                    )
                )
            );

        setTemperature(
            currentTemperature > 0
                ? currentTemperature
                : -1
        );
    } else {
        setTemperature(-1);
    }

    const QJsonObject powerTime =
        data.value(
            QStringLiteral("power_on_time")
        ).toObject();

    if (powerTime.contains(
            QStringLiteral("hours")
        )) {

        m_powerHoursValue->setText(
            tx(
                "%1 hours",
                "%1 Std."
            ).arg(
                formatNumber(
                    jsonUnsigned(
                        powerTime.value(
                            QStringLiteral("hours")
                        )
                    )
                )
            )
        );
    } else {
        m_powerHoursValue->setText(
            QStringLiteral("—")
        );
    }

    if (data.contains(
            QStringLiteral("power_cycle_count")
        )) {

        m_powerCyclesValue->setText(
            tx(
                "%1 count",
                "%1 mal"
            ).arg(
                formatNumber(
                    jsonUnsigned(
                        data.value(
                            QStringLiteral(
                                "power_cycle_count"
                            )
                        )
                    )
                )
            )
        );
    } else {
        m_powerCyclesValue->setText(
            QStringLiteral("—")
        );
    }

    const quint64 rotation =
        jsonUnsigned(
            data.value(
                QStringLiteral("rotation_rate")
            )
        );

    if (rotation > 0) {
        m_rotationValue->setText(
            QStringLiteral("%1 RPM")
                .arg(
                    formatNumber(rotation)
                )
        );
    } else if (usbDevice) {
        m_rotationValue->setText(
            QStringLiteral("---- (USB)")
        );
    } else {
        m_rotationValue->setText(
            QStringLiteral("---- (SSD)")
        );
    }

    m_table->setRowCount(0);

    if (data.contains(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        )) {

        renderNvme(data);
    } else if (data.contains(
                   QStringLiteral(
                       "ata_smart_attributes"
                   )
               )) {

        renderAta(data);
    } else if (!smartAvailable) {
        setHealth(
            HealthState::Unknown
        );

        setTemperature(-1);

        m_table->setColumnHidden(
            ColumnStatus,
            true
        );

        m_table->setColumnHidden(
            ColumnId,
            true
        );

        m_table->setColumnHidden(
            ColumnAttribute,
            false
        );

        m_table->setColumnHidden(
            ColumnValue,
            false
        );

        m_table->setColumnHidden(
            ColumnCurrent,
            true
        );

        m_table->setColumnHidden(
            ColumnWorst,
            true
        );

        m_table->setColumnHidden(
            ColumnThreshold,
            true
        );

        m_table->setColumnHidden(
            ColumnRaw,
            true
        );

        m_table->setHorizontalHeaderLabels(
            {
                QString(),
                QStringLiteral("ID"),
                tx(
                    "Device Information",
                    "Geräteinformation"
                ),
                tx(
                    "Value",
                    "Wert"
                ),
                QString(),
                QString(),
                QString(),
                QString()
            }
        );

        m_table->horizontalHeader()
            ->setStretchLastSection(true);

        m_table->horizontalHeader()
            ->setSectionResizeMode(
                ColumnAttribute,
                QHeaderView::Interactive
            );

        m_table->horizontalHeader()
            ->setSectionResizeMode(
                ColumnValue,
                QHeaderView::Interactive
            );

        m_table->setColumnWidth(
            ColumnAttribute,
            330
        );

        auto addDeviceInfo =
            [this](
                const QString &name,
                const QString &value
            )
            {
                if (value.isEmpty() ||
                    value ==
                    QStringLiteral("—")) {
                    return;
                }

                const int row =
                    m_table->rowCount();

                m_table->insertRow(row);

                m_table->setItem(
                    row,
                    ColumnAttribute,
                    new QTableWidgetItem(name)
                );

                m_table->setItem(
                    row,
                    ColumnValue,
                    new QTableWidgetItem(value)
                );
            };

        addDeviceInfo(
            tx("Device", "Gerät"),
            drive.name
        );

        addDeviceInfo(
            tx("Vendor", "Hersteller"),
            data.value(
                QStringLiteral(
                    "lindiskinfo_vendor"
                )
            ).toString()
        );

        addDeviceInfo(
            tx("Product", "Produkt"),
            model
        );

        addDeviceInfo(
            tx("Revision", "Revision"),
            data.value(
                QStringLiteral(
                    "firmware_version"
                )
            ).toString()
        );

        addDeviceInfo(
            tx("Capacity", "Kapazität"),
            capacity > 0
                ? formatBytes(capacity)
                : QString()
        );

        addDeviceInfo(
            tx("Interface", "Schnittstelle"),
            m_interfaceValue->text()
        );

        const quint64 usbMbps =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_link_mbps"
                    )
                )
            );

        if (usbMbps >= 1000) {
            addDeviceInfo(
                tx(
                    "USB Link Speed",
                    "USB-Verbindungsgeschwindigkeit"
                ),
                QStringLiteral("%1 Gb/s")
                    .arg(
                        static_cast<double>(
                            usbMbps
                        ) / 1000.0,
                        0,
                        'f',
                        1
                    )
            );
        } else if (usbMbps > 0) {
            addDeviceInfo(
                tx(
                    "USB Link Speed",
                    "USB-Verbindungsgeschwindigkeit"
                ),
                QStringLiteral("%1 Mb/s")
                    .arg(usbMbps)
            );
        }

        addDeviceInfo(
            tx(
                "USB Specification",
                "USB-Spezifikation"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_specification"
                )
            ).toString()
        );

        const QString usbVendorId =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_vendor_id"
                )
            ).toString();

        const QString usbProductId =
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_product_id"
                )
            ).toString();

        if (!usbVendorId.isEmpty() &&
            !usbProductId.isEmpty()) {

            addDeviceInfo(
                QStringLiteral("USB ID"),
                usbVendorId +
                    QStringLiteral(":") +
                    usbProductId
            );
        }

        addDeviceInfo(
            tx(
                "USB Port Path",
                "USB-Portpfad"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_port_path"
                )
            ).toString()
        );

        addDeviceInfo(
            tx(
                "USB Driver",
                "USB-Treiber"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_driver"
                )
            ).toString()
        );

        addDeviceInfo(
            tx(
                "USB Protocol",
                "USB-Protokoll"
            ),
            localizedUsbProtocol(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_protocol"
                    )
                ).toString()
            )
        );

        addDeviceInfo(
            tx(
                "Power Source",
                "Stromversorgung"
            ),
            localizedUsbPowerSource(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_usb_power_source"
                    )
                ).toString()
            )
        );

        addDeviceInfo(
            tx(
                "Maximum Power",
                "Maximale Stromaufnahme"
            ),
            data.value(
                QStringLiteral(
                    "lindiskinfo_usb_max_power"
                )
            ).toString()
        );

        addDeviceInfo(
            tx("Removable", "Wechseldatenträger"),
            data.value(
                QStringLiteral(
                    "lindiskinfo_removable"
                )
            ).toBool()
                ? tx("Yes", "Ja")
                : tx("No", "Nein")
        );

        addDeviceInfo(
            tx("Read Only", "Schreibgeschützt"),
            data.value(
                QStringLiteral(
                    "lindiskinfo_read_only"
                )
            ).toBool()
                ? tx("Yes", "Ja")
                : tx("No", "Nein")
        );

        const quint64 logicalSector =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_logical_sector"
                    )
                )
            );

        const quint64 physicalSector =
            jsonUnsigned(
                data.value(
                    QStringLiteral(
                        "lindiskinfo_physical_sector"
                    )
                )
            );

        if (logicalSector > 0) {
            addDeviceInfo(
                tx(
                    "Logical Sector Size",
                    "Logische Sektorgröße"
                ),
                QStringLiteral("%1 B")
                    .arg(logicalSector)
            );
        }

        if (physicalSector > 0) {
            addDeviceInfo(
                tx(
                    "Physical Sector Size",
                    "Physische Sektorgröße"
                ),
                QStringLiteral("%1 B")
                    .arg(physicalSector)
            );
        }

    } else {
        const QJsonObject smartStatus =
            data.value(
                QStringLiteral("smart_status")
            ).toObject();

        if (smartStatus.contains(
                QStringLiteral("passed")
            )) {

            setHealth(
                smartStatus.value(
                    QStringLiteral("passed")
                ).toBool()
                ? HealthState::Good
                : HealthState::Bad
            );
        } else {
            setHealth(
                HealthState::Unknown
            );
        }
    }

    setStatus(
        smartAvailable
            ? tx(
                  "SMART data loaded.",
                  "SMART-Daten geladen."
              )
            : tx(
                  "Device information loaded.",
                  "Geräteinformationen geladen."
              )
    );







}

void MainWindow::renderNvme(
    const QJsonObject &data
)
{
    saveCurrentTableWidths();

    m_currentTableIsNvme = true;

    configureNvmeTable();

    restoreCurrentTableWidths();

    const QJsonObject nvme =
        data.value(
            QStringLiteral(
                "nvme_smart_health_information_log"
            )
        ).toObject();

    auto value =
        [&nvme](const char *key)
        {
            return jsonUnsigned(
                nvme.value(
                    QString::fromLatin1(key)
                )
            );
        };

    const quint64 criticalWarning =
        value("critical_warning");

    const quint64 spare =
        value("available_spare");

    const quint64 spareThreshold =
        value("available_spare_threshold");

    const quint64 percentageUsed =
        value("percentage_used");

    const quint64 dataRead =
        value("data_units_read");

    const quint64 dataWritten =
        value("data_units_written");

    const quint64 hostReads =
        value("host_reads");

    const quint64 hostWrites =
        value("host_writes");

    const quint64 busy =
        value("controller_busy_time");

    const quint64 cycles =
        value("power_cycles");

    const quint64 hours =
        value("power_on_hours");

    const quint64 unsafe =
        value("unsafe_shutdowns");

    const quint64 mediaErrors =
        value("media_errors");

    const quint64 errorEntries =
        value("num_err_log_entries");

    const quint64 warningTemp =
        value("warning_temp_time");

    const quint64 criticalTemp =
        value("critical_comp_time");

    const int remaining =
        std::max(
            0,
            100 -
            static_cast<int>(
                percentageUsed
            )
        );

    setHealth(
        healthStateForData(
            data
        ),
        remaining
    );

    m_readsValue->setText(
        formatBytes(
            dataRead * 512000ULL
        )
    );

    m_writesValue->setText(
        formatBytes(
            dataWritten * 512000ULL
        )
    );

    m_powerCyclesValue->setText(
        tx(
            "%1 count",
            "%1 mal"
        ).arg(
            formatNumber(cycles)
        )
    );

    m_powerHoursValue->setText(
        tx(
            "%1 hours",
            "%1 Std."
        ).arg(
            formatNumber(hours)
        )
    );

    addNvmeRow(
        QStringLiteral("01"),
        tx(
            "Critical Warning",
            "Kritische Warnung"
        ),
        criticalWarning == 0
            ? tx(
                  "None",
                  "Keine"
              )
            : QString::number(
                  criticalWarning
              ),
        formatRawValue(
            criticalWarning
        ),
        criticalWarning == 0
            ? HealthState::Good
            : HealthState::Bad
    );

    const int temperature =
        static_cast<int>(
            jsonUnsigned(
                data.value(
                    QStringLiteral("temperature")
                ).toObject()
                .value(
                    QStringLiteral("current")
                )
            )
        );

    addNvmeRow(
        QStringLiteral("02"),
        tx(
            "Composite Temperature",
            "Gesamttemperatur"
        ),
        temperature > 0
            ? formatTemperature(temperature)
            : QStringLiteral("—"),
        formatRawValue(
            temperature > 0
                ? static_cast<quint64>(
                      temperature
                  )
                : 0
        )
    );

    addNvmeRow(
        QStringLiteral("03"),
        tx(
            "Available Spare",
            "Verfügbare Reserve"
        ),
        QStringLiteral("%1 %")
            .arg(spare),
        formatRawValue(spare),
        spare <= spareThreshold
            ? HealthState::Bad
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("04"),
        tx(
            "Available Spare Threshold",
            "Reserve-Grenzwert"
        ),
        QStringLiteral("%1 %")
            .arg(spareThreshold),
        formatRawValue(
            spareThreshold
        )
    );

    addNvmeRow(
        QStringLiteral("05"),
        tx(
            "Percentage Used",
            "Verbrauchte Lebensdauer"
        ),
        QStringLiteral("%1 %")
            .arg(percentageUsed),
        formatRawValue(
            percentageUsed
        ),
        remaining <=
                m_nvmeBadRemaining
            ? HealthState::Bad
            : (
                remaining <=
                    m_nvmeCautionRemaining
                    ? HealthState::Caution
                    : HealthState::Good
              )
    );

    addNvmeRow(
        QStringLiteral("06"),
        tx(
            "Data Units Read",
            "Gelesene Daten"
        ),
        formatBytes(
            dataRead * 512000ULL
        ),
        formatRawValue(dataRead)
    );

    addNvmeRow(
        QStringLiteral("07"),
        tx(
            "Data Units Written",
            "Geschriebene Daten"
        ),
        formatBytes(
            dataWritten * 512000ULL
        ),
        formatRawValue(dataWritten)
    );

    addNvmeRow(
        QStringLiteral("08"),
        tx(
            "Host Read Commands",
            "Host-Lesebefehle"
        ),
        formatNumber(hostReads),
        formatRawValue(hostReads)
    );

    addNvmeRow(
        QStringLiteral("09"),
        tx(
            "Host Write Commands",
            "Host-Schreibbefehle"
        ),
        formatNumber(hostWrites),
        formatRawValue(hostWrites)
    );

    addNvmeRow(
        QStringLiteral("0A"),
        tx(
            "Controller Busy Time",
            "Controller-Aktivzeit"
        ),
        tx(
            "%1 min",
            "%1 Min."
        ).arg(
            formatNumber(busy)
        ),
        formatRawValue(busy)
    );

    addNvmeRow(
        QStringLiteral("0B"),
        tx(
            "Power Cycles",
            "Einschaltungen"
        ),
        formatNumber(cycles),
        formatRawValue(cycles)
    );

    addNvmeRow(
        QStringLiteral("0C"),
        tx(
            "Power On Hours",
            "Betriebsstunden"
        ),
        tx(
            "%1 hours",
            "%1 Std."
        ).arg(
            formatNumber(hours)
        ),
        formatRawValue(hours)
    );

    addNvmeRow(
        QStringLiteral("0D"),
        tx(
            "Unsafe Shutdowns",
            "Unsichere Abschaltungen"
        ),
        formatNumber(unsafe),
        formatRawValue(unsafe),
        unsafe > 0
            ? HealthState::Caution
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("0E"),
        tx(
            "Media and Data Integrity Errors",
            "Medien- und Datenintegritätsfehler"
        ),
        formatNumber(mediaErrors),
        formatRawValue(mediaErrors),
        mediaErrors > 0
            ? HealthState::Bad
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("0F"),
        tx(
            "Error Information Log Entries",
            "Fehlerprotokolleinträge"
        ),
        formatNumber(errorEntries),
        formatRawValue(errorEntries),
        errorEntries > 0
            ? HealthState::Caution
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("10"),
        tx(
            "Warning Temperature Time",
            "Zeit über Warntemperatur"
        ),
        tx(
            "%1 min",
            "%1 Min."
        ).arg(
            formatNumber(warningTemp)
        ),
        formatRawValue(warningTemp),
        warningTemp > 0
            ? HealthState::Caution
            : HealthState::Good
    );

    addNvmeRow(
        QStringLiteral("11"),
        tx(
            "Critical Temperature Time",
            "Zeit über kritischer Temperatur"
        ),
        tx(
            "%1 min",
            "%1 Min."
        ).arg(
            formatNumber(criticalTemp)
        ),
        formatRawValue(criticalTemp),
        criticalTemp > 0
            ? HealthState::Bad
            : HealthState::Good
    );
}

void MainWindow::renderAta(
    const QJsonObject &data
)
{
    saveCurrentTableWidths();

    m_currentTableIsNvme = false;

    configureAtaTable();

    restoreCurrentTableWidths();

    const QJsonArray attributes =
        data.value(
            QStringLiteral(
                "ata_smart_attributes"
            )
        ).toObject()
        .value(
            QStringLiteral("table")
        ).toArray();

    bool caution = false;
    bool bad = false;

    for (const QJsonValue &entry : attributes) {
        const QJsonObject attribute =
            entry.toObject();

        const int id =
            attribute.value(
                QStringLiteral("id")
            ).toInt();

        const QString name =
            attribute.value(
                QStringLiteral("name")
            ).toString();

        const int current =
            attribute.value(
                QStringLiteral("value")
            ).toInt();

        const int worst =
            attribute.value(
                QStringLiteral("worst")
            ).toInt();

        const int threshold =
            attribute.value(
                QStringLiteral("thresh")
            ).toInt();

        const QJsonObject raw =
            attribute.value(
                QStringLiteral("raw")
            ).toObject();

        const quint64 rawValue =
            jsonUnsigned(
                raw.value(
                    QStringLiteral("value")
                )
            );

        QString rawText =
            formatRawValue(
                rawValue
            );

        HealthState state =
            HealthState::Good;

        if ((name == QStringLiteral("Current_Pending_Sector") ||
             name == QStringLiteral("Offline_Uncorrectable")) &&
            rawValue >=
            m_ataSectorCautionCount) {

            state =
                HealthState::Caution;

            caution = true;
        }

        if (name == QStringLiteral("Reallocated_Sector_Ct") &&
            rawValue >=
            m_ataSectorCautionCount) {

            state =
                HealthState::Caution;

            caution = true;
        }

        if (threshold > 0 &&
            current <= threshold) {

            state =
                HealthState::Bad;

            bad = true;
        }

        addAtaRow(
            QStringLiteral("%1")
                .arg(
                    id,
                    2,
                    16,
                    QLatin1Char('0')
                )
                .toUpper(),
            translateAtaAttribute(name),
            QString::number(current),
            QString::number(worst),
            QString::number(threshold),
            rawText,
            state
        );
    }

    const bool passed =
        data.value(
            QStringLiteral("smart_status")
        ).toObject()
        .value(
            QStringLiteral("passed")
        ).toBool(true);

    if (!passed || bad) {
        setHealth(
            HealthState::Bad
        );
    } else if (caution) {
        setHealth(
            HealthState::Caution
        );
    } else {
        setHealth(
            HealthState::Good
        );
    }

    m_readsValue->setText(
        QStringLiteral("—")
    );

    m_writesValue->setText(
        QStringLiteral("—")
    );
}

void MainWindow::configureNvmeTable()
{
    m_table->setHorizontalHeaderLabels(
        {
            QString(),
            QStringLiteral("ID"),
            tx(
                "Attribute Name",
                "Parametername"
            ),
            tx(
                "Value",
                "Wert"
            ),
            tx(
                "Current",
                "Aktuell"
            ),
            tx(
                "Worst",
                "Schlechtester Wert"
            ),
            tx(
                "Threshold",
                "Grenzwert"
            ),
            tx(
                "Raw Value",
                "Rohwert"
            )
        }
    );

    m_table->setColumnHidden(
        ColumnStatus,
        false
    );

    m_table->setColumnHidden(
        ColumnValue,
        false
    );

    m_table->setColumnHidden(
        ColumnCurrent,
        true
    );

    m_table->setColumnHidden(
        ColumnWorst,
        true
    );

    m_table->setColumnHidden(
        ColumnThreshold,
        true
    );

    updateRawColumn();
}

void MainWindow::configureAtaTable()
{
    m_table->setHorizontalHeaderLabels(
        {
            QString(),
            QStringLiteral("ID"),
            tx(
                "Attribute Name",
                "Parametername"
            ),
            tx(
                "Value",
                "Wert"
            ),
            tx(
                "Current",
                "Aktuell"
            ),
            tx(
                "Worst",
                "Schlechtester Wert"
            ),
            tx(
                "Threshold",
                "Grenzwert"
            ),
            tx(
                "Raw Value",
                "Rohwert"
            )
        }
    );

    m_table->setColumnHidden(
        ColumnStatus,
        false
    );

    m_table->setColumnHidden(
        ColumnValue,
        true
    );

    m_table->setColumnHidden(
        ColumnCurrent,
        false
    );

    m_table->setColumnHidden(
        ColumnWorst,
        false
    );

    m_table->setColumnHidden(
        ColumnThreshold,
        false
    );

    updateRawColumn();
}

void MainWindow::applyTableColumnLayout()
{
    if (!m_table)
        return;

    QHeaderView *header =
        m_table->horizontalHeader();

    header->setStretchLastSection(false);
    header->setMinimumSectionSize(28);
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

    const int availableWidth =
        std::max(
            720,
            m_table->viewport()->width()
        );

    const bool rawVisible =
        m_showRawAction &&
        m_showRawAction->isChecked();

    const int statusWidth = 28;
    const int idWidth = 44;

    m_table->setColumnWidth(
        ColumnStatus,
        statusWidth
    );

    m_table->setColumnWidth(
        ColumnId,
        idWidth
    );

    if (m_currentTableIsNvme) {
        const int valueWidth =
            std::max(
                150,
                availableWidth * 18 / 100
            );

        const int rawWidth =
            rawVisible
            ? std::max(
                  190,
                  availableWidth * 22 / 100
              )
            : 0;

        const int attributeWidth =
            std::max(
                300,
                availableWidth
                - statusWidth
                - idWidth
                - valueWidth
                - rawWidth
                - 12
            );

        m_table->setColumnWidth(
            ColumnAttribute,
            attributeWidth
        );

        m_table->setColumnWidth(
            ColumnValue,
            valueWidth
        );

        if (rawVisible) {
            m_table->setColumnWidth(
                ColumnRaw,
                rawWidth
            );
        }
    } else {
        const int currentWidth =
            std::max(
                90,
                availableWidth * 7 / 100
            );

        const int worstWidth =
            std::max(
                90,
                availableWidth * 7 / 100
            );

        const int thresholdWidth =
            std::max(
                105,
                availableWidth * 8 / 100
            );

        const int rawWidth =
            rawVisible
            ? std::max(
                  200,
                  availableWidth * 20 / 100
              )
            : 0;

        const int attributeWidth =
            std::max(
                320,
                availableWidth
                - statusWidth
                - idWidth
                - currentWidth
                - worstWidth
                - thresholdWidth
                - rawWidth
                - 12
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

        if (rawVisible) {
            m_table->setColumnWidth(
                ColumnRaw,
                rawWidth
            );
        }
    }

    header->setStretchLastSection(true);
}

void MainWindow::setHealth(
    HealthState state,
    int percentage
)
{
    QString text;
    QString style;

    switch (state) {
    case HealthState::Good:
        text =
            tx(
                "Good",
                "Gut"
            );

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #dff4ff,"
                "stop:0.45 #70c8ff,"
                "stop:1 #1ca3e8);"
                "color: #07131d;"
                "}"
            );
        break;

    case HealthState::Caution:
        text =
            tx(
                "Caution",
                "Vorsicht"
            );

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: #302400;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #d59c00;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #fffbd1,"
                "stop:0.45 #ffe44d,"
                "stop:1 #ffbd00);"
                "color: #241b00;"
                "}"
            );
        break;

    case HealthState::Bad:
        text =
            tx(
                "Bad",
                "Schlecht"
            );

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: white;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #c62828;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #ffd5d5,"
                "stop:0.45 #ff5555,"
                "stop:1 #e00000);"
                "color: white;"
                "}"
            );
        break;

    case HealthState::Unknown:
        text =
            QStringLiteral("—");

        m_healthValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#healthCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 3px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #dff4ff,"
                "stop:0.45 #70c8ff,"
                "stop:1 #1ca3e8);"
                "color: #07131d;"
                "}"
            );
        break;
    }

    if (percentage >= 0) {
        text +=
            QStringLiteral("\n%1 %")
                .arg(percentage);
    }

    m_healthValue->setText(text);
    m_healthFrame->setStyleSheet(style);
}


void MainWindow::setTemperature(
    int temperature
)
{
    QString style;

    if (temperature < 0) {
        m_temperatureValue->setText(
            QStringLiteral("—")
        );

        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #e7f8ff,"
                "stop:0.45 #69d7ff,"
                "stop:1 #12a9ef);"
                "color: #07131d;"
                "}"
            );

        m_temperatureFrame
            ->setStyleSheet(style);

        return;
    }

    m_temperatureValue->setText(
        formatTemperature(
            temperature
        )
    );

    if (temperature >=
        m_temperatureBad) {

        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: white;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #c62828;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #ffdada,"
                "stop:0.5 #ff5c5c,"
                "stop:1 #d80000);"
                "color: white;"
                "}"
            );

    } else if (
        temperature >=
            m_temperatureCaution) {

        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: #302400;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #d59c00;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #fffac4,"
                "stop:0.5 #ffe552,"
                "stop:1 #ffb900);"
                "color: #241b00;"
                "}"
            );

    } else {
        m_temperatureValue->setStyleSheet(
            QStringLiteral(
                "color: #082235;"
            )
        );

        style =
            QStringLiteral(
                "QFrame#temperatureCard {"
                "border: 1px solid #2588d8;"
                "border-radius: 22px;"
                "background: qlineargradient("
                "x1:0,y1:0,x2:0,y2:1,"
                "stop:0 #e7f8ff,"
                "stop:0.45 #69d7ff,"
                "stop:1 #12a9ef);"
                "color: #07131d;"
                "}"
            );
    }

    m_temperatureFrame->setStyleSheet(
        style
    );
}


void MainWindow::addNvmeRow(
    const QString &id,
    const QString &attribute,
    const QString &value,
    const QString &raw,
    HealthState state
)
{
    const int row =
        m_table->rowCount();

    m_table->insertRow(row);

    auto *statusItem =
        new QTableWidgetItem(
            QStringLiteral("●")
        );

    switch (state) {
    case HealthState::Good:
        statusItem->setForeground(
            QColor(QStringLiteral("#39aaf3"))
        );
        break;

    case HealthState::Caution:
        statusItem->setForeground(
            QColor(QStringLiteral("#ffd400"))
        );
        break;

    case HealthState::Bad:
        statusItem->setForeground(
            QColor(QStringLiteral("#ff3838"))
        );
        break;

    default:
        break;
    }

    m_table->setItem(
        row,
        ColumnStatus,
        statusItem
    );

    m_table->setItem(
        row,
        ColumnId,
        new QTableWidgetItem(id)
    );

    m_table->setItem(
        row,
        ColumnAttribute,
        new QTableWidgetItem(attribute)
    );

    m_table->setItem(
        row,
        ColumnValue,
        new QTableWidgetItem(value)
    );

    m_table->setItem(
        row,
        ColumnRaw,
        new QTableWidgetItem(raw)
    );
}

void MainWindow::addAtaRow(
    const QString &id,
    const QString &attribute,
    const QString &current,
    const QString &worst,
    const QString &threshold,
    const QString &raw,
    HealthState state
)
{
    const int row =
        m_table->rowCount();

    m_table->insertRow(row);

    auto *statusItem =
        new QTableWidgetItem(
            QStringLiteral("●")
        );

    switch (state) {
    case HealthState::Good:
        statusItem->setForeground(
            QColor(QStringLiteral("#39aaf3"))
        );
        break;

    case HealthState::Caution:
        statusItem->setForeground(
            QColor(QStringLiteral("#ffd400"))
        );
        break;

    case HealthState::Bad:
        statusItem->setForeground(
            QColor(QStringLiteral("#ff3838"))
        );
        break;

    default:
        break;
    }

    m_table->setItem(
        row,
        ColumnStatus,
        statusItem
    );

    m_table->setItem(
        row,
        ColumnId,
        new QTableWidgetItem(id)
    );

    m_table->setItem(
        row,
        ColumnAttribute,
        new QTableWidgetItem(attribute)
    );

    m_table->setItem(
        row,
        ColumnCurrent,
        new QTableWidgetItem(current)
    );

    m_table->setItem(
        row,
        ColumnWorst,
        new QTableWidgetItem(worst)
    );

    m_table->setItem(
        row,
        ColumnThreshold,
        new QTableWidgetItem(threshold)
    );

    m_table->setItem(
        row,
        ColumnRaw,
        new QTableWidgetItem(raw)
    );
}

QString MainWindow::translateAtaAttribute(
    const QString &name
) const
{
    static const QHash<
        QString,
        QPair<QString, QString>
    > names =
    {
        {
            QStringLiteral("Raw_Read_Error_Rate"),
            {
                QStringLiteral("Raw Read Error Rate"),
                QStringLiteral("Lesefehlerrate")
            }
        },
        {
            QStringLiteral("Throughput_Performance"),
            {
                QStringLiteral("Throughput Performance"),
                QStringLiteral("Datendurchsatzleistung")
            }
        },
        {
            QStringLiteral("Spin_Up_Time"),
            {
                QStringLiteral("Spin-Up Time"),
                QStringLiteral("Anlaufzeit")
            }
        },
        {
            QStringLiteral("Start_Stop_Count"),
            {
                QStringLiteral("Start/Stop Count"),
                QStringLiteral("Start-/Stopp-Zyklen")
            }
        },
        {
            QStringLiteral("Reallocated_Sector_Ct"),
            {
                QStringLiteral("Reallocated Sectors Count"),
                QStringLiteral("Wiederzugewiesene Sektoren")
            }
        },
        {
            QStringLiteral("Read_Channel_Margin"),
            {
                QStringLiteral("Read Channel Margin"),
                QStringLiteral("Lesekanalreserve")
            }
        },
        {
            QStringLiteral("Seek_Error_Rate"),
            {
                QStringLiteral("Seek Error Rate"),
                QStringLiteral("Suchfehlerrate")
            }
        },
        {
            QStringLiteral("Seek_Time_Performance"),
            {
                QStringLiteral("Seek Time Performance"),
                QStringLiteral("Suchzeitleistung")
            }
        },
        {
            QStringLiteral("Power_On_Hours"),
            {
                QStringLiteral("Power-On Hours"),
                QStringLiteral("Betriebsstunden")
            }
        },
        {
            QStringLiteral("Spin_Retry_Count"),
            {
                QStringLiteral("Spin Retry Count"),
                QStringLiteral("Wiederholte Anlaufversuche")
            }
        },
        {
            QStringLiteral("Calibration_Retry_Count"),
            {
                QStringLiteral("Calibration Retry Count"),
                QStringLiteral("Wiederholte Kalibrierungsversuche")
            }
        },
        {
            QStringLiteral("Power_Cycle_Count"),
            {
                QStringLiteral("Power Cycle Count"),
                QStringLiteral("Einschaltvorgänge")
            }
        },
        {
            QStringLiteral("Soft_Read_Error_Rate"),
            {
                QStringLiteral("Soft Read Error Rate"),
                QStringLiteral("Software-Lesefehlerrate")
            }
        },
        {
            QStringLiteral("Runtime_Bad_Block"),
            {
                QStringLiteral("Runtime Bad Block"),
                QStringLiteral("Laufzeit-Schlechtblöcke")
            }
        },
        {
            QStringLiteral("End-to-End_Error"),
            {
                QStringLiteral("End-to-End Error"),
                QStringLiteral("Ende-zu-Ende-Fehler")
            }
        },
        {
            QStringLiteral("Reported_Uncorrect"),
            {
                QStringLiteral("Reported Uncorrectable Errors"),
                QStringLiteral("Gemeldete unkorrigierbare Fehler")
            }
        },
        {
            QStringLiteral("Command_Timeout"),
            {
                QStringLiteral("Command Timeout"),
                QStringLiteral("Befehlszeitüberschreitungen")
            }
        },
        {
            QStringLiteral("High_Fly_Writes"),
            {
                QStringLiteral("High Fly Writes"),
                QStringLiteral("Schreibvorgänge bei großer Flughöhe")
            }
        },
        {
            QStringLiteral("Airflow_Temperature_Cel"),
            {
                QStringLiteral("Airflow Temperature"),
                QStringLiteral("Luftstromtemperatur")
            }
        },
        {
            QStringLiteral("Air_Flow_Temperature"),
            {
                QStringLiteral("Airflow Temperature"),
                QStringLiteral("Luftstromtemperatur")
            }
        },
        {
            QStringLiteral("G-Sense_Error_Rate"),
            {
                QStringLiteral("G-Sense Error Rate"),
                QStringLiteral("Erschütterungsfehlerrate")
            }
        },
        {
            QStringLiteral("Power-Off_Retract_Count"),
            {
                QStringLiteral("Power-Off Retract Count"),
                QStringLiteral("Kopfrückzüge bei Stromausfall")
            }
        },
        {
            QStringLiteral("Load_Cycle_Count"),
            {
                QStringLiteral("Load Cycle Count"),
                QStringLiteral("Kopf-Ladezyklen")
            }
        },
        {
            QStringLiteral("Temperature_Celsius"),
            {
                QStringLiteral("Temperature"),
                QStringLiteral("Temperatur")
            }
        },
        {
            QStringLiteral("Hardware_ECC_Recovered"),
            {
                QStringLiteral("Hardware ECC Recovered"),
                QStringLiteral("Durch Hardware-ECC korrigiert")
            }
        },
        {
            QStringLiteral("Reallocated_Event_Count"),
            {
                QStringLiteral("Reallocation Event Count"),
                QStringLiteral("Ereignisse mit Sektorneuzuweisung")
            }
        },
        {
            QStringLiteral("Current_Pending_Sector"),
            {
                QStringLiteral("Current Pending Sectors"),
                QStringLiteral("Aktuell ausstehende Sektoren")
            }
        },
        {
            QStringLiteral("Offline_Uncorrectable"),
            {
                QStringLiteral("Offline Uncorrectable"),
                QStringLiteral("Offline nicht korrigierbare Sektoren")
            }
        },
        {
            QStringLiteral("UDMA_CRC_Error_Count"),
            {
                QStringLiteral("UDMA CRC Error Count"),
                QStringLiteral("UDMA-CRC-Fehler")
            }
        },
        {
            QStringLiteral("CRC_Error_Count"),
            {
                QStringLiteral("CRC Error Count"),
                QStringLiteral("CRC-Fehler")
            }
        },
        {
            QStringLiteral("Multi_Zone_Error_Rate"),
            {
                QStringLiteral("Multi-Zone Error Rate"),
                QStringLiteral("Mehrzonenfehlerrate")
            }
        },
        {
            QStringLiteral("Head_Flying_Hours"),
            {
                QStringLiteral("Head Flying Hours"),
                QStringLiteral("Kopf-Flugstunden")
            }
        },
        {
            QStringLiteral("Total_LBAs_Written"),
            {
                QStringLiteral("Total LBAs Written"),
                QStringLiteral("Geschriebene LBAs gesamt")
            }
        },
        {
            QStringLiteral("Total_LBAs_Read"),
            {
                QStringLiteral("Total LBAs Read"),
                QStringLiteral("Gelesene LBAs gesamt")
            }
        },
        {
            QStringLiteral("Read_Error_Retry_Rate"),
            {
                QStringLiteral("Read Error Retry Rate"),
                QStringLiteral("Wiederholungsrate bei Lesefehlern")
            }
        },
        {
            QStringLiteral("Free_Fall_Sensor"),
            {
                QStringLiteral("Free-Fall Sensor"),
                QStringLiteral("Freifallsensor-Ereignisse")
            }
        },
        {
            QStringLiteral("Disk_Shift"),
            {
                QStringLiteral("Disk Shift"),
                QStringLiteral("Plattenverschiebung")
            }
        },
        {
            QStringLiteral("Loaded_Hours"),
            {
                QStringLiteral("Loaded Hours"),
                QStringLiteral("Stunden mit geladenen Köpfen")
            }
        },
        {
            QStringLiteral("Load_Retry_Count"),
            {
                QStringLiteral("Load Retry Count"),
                QStringLiteral("Wiederholte Kopf-Ladevorgänge")
            }
        },
        {
            QStringLiteral("Load_Friction"),
            {
                QStringLiteral("Load Friction"),
                QStringLiteral("Kopf-Ladereibung")
            }
        },
        {
            QStringLiteral("Load-in_Time"),
            {
                QStringLiteral("Load-In Time"),
                QStringLiteral("Kopf-Ladezeit")
            }
        },
        {
            QStringLiteral("Torque_Amplification_Count"),
            {
                QStringLiteral("Torque Amplification Count"),
                QStringLiteral("Drehmomentverstärkungen")
            }
        },
        {
            QStringLiteral("GMR_Head_Amplitude"),
            {
                QStringLiteral("GMR Head Amplitude"),
                QStringLiteral("GMR-Kopfamplitude")
            }
        },
        {
            QStringLiteral("Helium_Condition_Lower"),
            {
                QStringLiteral("Helium Condition Lower"),
                QStringLiteral("Heliumzustand Untergrenze")
            }
        },
        {
            QStringLiteral("Helium_Condition_Upper"),
            {
                QStringLiteral("Helium Condition Upper"),
                QStringLiteral("Heliumzustand Obergrenze")
            }
        },

        {
            QStringLiteral("Wear_Leveling_Count"),
            {
                QStringLiteral("Wear Leveling Count"),
                QStringLiteral("Verschleißausgleich")
            }
        },
        {
            QStringLiteral("Media_Wearout_Indicator"),
            {
                QStringLiteral("Media Wearout Indicator"),
                QStringLiteral("Medienverschleißanzeige")
            }
        },
        {
            QStringLiteral("SSD_Life_Left"),
            {
                QStringLiteral("SSD Life Left"),
                QStringLiteral("Verbleibende SSD-Lebensdauer")
            }
        },
        {
            QStringLiteral("Percent_Lifetime_Remain"),
            {
                QStringLiteral("Lifetime Remaining"),
                QStringLiteral("Verbleibende Lebensdauer")
            }
        },
        {
            QStringLiteral("Remaining_Lifetime_Perc"),
            {
                QStringLiteral("Remaining Lifetime"),
                QStringLiteral("Verbleibende Lebensdauer")
            }
        },
        {
            QStringLiteral("Percent_Lifetime_Used"),
            {
                QStringLiteral("Lifetime Used"),
                QStringLiteral("Verbrauchte Lebensdauer")
            }
        },
        {
            QStringLiteral("Available_Reservd_Space"),
            {
                QStringLiteral("Available Reserved Space"),
                QStringLiteral("Verfügbarer Reservespeicher")
            }
        },
        {
            QStringLiteral("Used_Rsvd_Blk_Cnt_Tot"),
            {
                QStringLiteral("Used Reserved Block Count"),
                QStringLiteral("Verwendete Reserveblöcke")
            }
        },
        {
            QStringLiteral("Used_Rsvd_Blk_Cnt_Chip"),
            {
                QStringLiteral("Used Reserved Blocks per Chip"),
                QStringLiteral("Verwendete Reserveblöcke pro Chip")
            }
        },
        {
            QStringLiteral("Unused_Rsvd_Blk_Cnt_Tot"),
            {
                QStringLiteral("Unused Reserved Block Count"),
                QStringLiteral("Unverwendete Reserveblöcke")
            }
        },
        {
            QStringLiteral("Program_Fail_Cnt_Total"),
            {
                QStringLiteral("Program Fail Count"),
                QStringLiteral("Programmierfehler")
            }
        },
        {
            QStringLiteral("Program_Fail_Count"),
            {
                QStringLiteral("Program Fail Count"),
                QStringLiteral("Programmierfehler")
            }
        },
        {
            QStringLiteral("Program_Fail_Count_Chip"),
            {
                QStringLiteral("Program Fail Count per Chip"),
                QStringLiteral("Programmierfehler pro Chip")
            }
        },
        {
            QStringLiteral("Erase_Fail_Count_Total"),
            {
                QStringLiteral("Erase Fail Count"),
                QStringLiteral("Löschfehler")
            }
        },
        {
            QStringLiteral("Erase_Fail_Count"),
            {
                QStringLiteral("Erase Fail Count"),
                QStringLiteral("Löschfehler")
            }
        },
        {
            QStringLiteral("Erase_Fail_Count_Chip"),
            {
                QStringLiteral("Erase Fail Count per Chip"),
                QStringLiteral("Löschfehler pro Chip")
            }
        },
        {
            QStringLiteral("Uncorrectable_Error_Cnt"),
            {
                QStringLiteral("Uncorrectable Error Count"),
                QStringLiteral("Nicht korrigierbare Fehler")
            }
        },
        {
            QStringLiteral("Uncorrectable_Error_Count"),
            {
                QStringLiteral("Uncorrectable Error Count"),
                QStringLiteral("Nicht korrigierbare Fehler")
            }
        },
        {
            QStringLiteral("ECC_Error_Rate"),
            {
                QStringLiteral("ECC Error Rate"),
                QStringLiteral("ECC-Fehlerrate")
            }
        },
        {
            QStringLiteral("POR_Recovery_Count"),
            {
                QStringLiteral("Power-On Recovery Count"),
                QStringLiteral("Wiederherstellungen nach Einschalten")
            }
        },
        {
            QStringLiteral("Unexpected_Power_Loss_Ct"),
            {
                QStringLiteral("Unexpected Power Loss Count"),
                QStringLiteral("Unerwartete Stromausfälle")
            }
        },
        {
            QStringLiteral("Host_Writes_32MiB"),
            {
                QStringLiteral("Host Writes (32 MiB)"),
                QStringLiteral("Host-Schreibdaten (32 MiB)")
            }
        },
        {
            QStringLiteral("Host_Reads_32MiB"),
            {
                QStringLiteral("Host Reads (32 MiB)"),
                QStringLiteral("Host-Lesedaten (32 MiB)")
            }
        },
        {
            QStringLiteral("NAND_Writes_1GiB"),
            {
                QStringLiteral("NAND Writes (1 GiB)"),
                QStringLiteral("NAND-Schreibdaten (1 GiB)")
            }
        },
        {
            QStringLiteral("Host_Writes_GiB"),
            {
                QStringLiteral("Host Writes (GiB)"),
                QStringLiteral("Host-Schreibdaten (GiB)")
            }
        },
        {
            QStringLiteral("Host_Reads_GiB"),
            {
                QStringLiteral("Host Reads (GiB)"),
                QStringLiteral("Host-Lesedaten (GiB)")
            }
        },
        {
            QStringLiteral("NAND_Writes_GiB"),
            {
                QStringLiteral("NAND Writes (GiB)"),
                QStringLiteral("NAND-Schreibdaten (GiB)")
            }
        },
        {
            QStringLiteral("Workld_Media_Wear_Indic"),
            {
                QStringLiteral("Workload Media Wear Indicator"),
                QStringLiteral("Arbeitslast-Medienverschleiß")
            }
        },
        {
            QStringLiteral("Workld_Host_Reads_Perc"),
            {
                QStringLiteral("Workload Host Reads Percentage"),
                QStringLiteral("Host-Leseanteil der Arbeitslast")
            }
        },
        {
            QStringLiteral("Workload_Minutes"),
            {
                QStringLiteral("Workload Minutes"),
                QStringLiteral("Arbeitslast-Minuten")
            }
        },
        {
            QStringLiteral("Thermal_Throttle_Status"),
            {
                QStringLiteral("Thermal Throttle Status"),
                QStringLiteral("Status der thermischen Drosselung")
            }
        },
        {
            QStringLiteral("Retired_Block_Count"),
            {
                QStringLiteral("Retired Block Count"),
                QStringLiteral("Ausgemusterte Blöcke")
            }
        },
        {
            QStringLiteral("Reallocated_Block_Count"),
            {
                QStringLiteral("Reallocated Block Count"),
                QStringLiteral("Neu zugewiesene Blöcke")
            }
        },
        {
            QStringLiteral("Flash_Writes_GiB"),
            {
                QStringLiteral("Flash Writes (GiB)"),
                QStringLiteral("Flash-Schreibdaten (GiB)")
            }
        },
        {
            QStringLiteral("Lifetime_Writes_GiB"),
            {
                QStringLiteral("Lifetime Writes (GiB)"),
                QStringLiteral("Schreibdaten über Lebensdauer (GiB)")
            }
        },
        {
            QStringLiteral("Lifetime_Reads_GiB"),
            {
                QStringLiteral("Lifetime Reads (GiB)"),
                QStringLiteral("Lesedaten über Lebensdauer (GiB)")
            }
        },
        {
            QStringLiteral("Wear_Range_Delta"),
            {
                QStringLiteral("Wear Range Delta"),
                QStringLiteral("Verschleißspannweite")
            }
        },
        {
            QStringLiteral("SATA_Downshift_Count"),
            {
                QStringLiteral("SATA Downshift Count"),
                QStringLiteral("SATA-Rückstufungen")
            }
        }
    };

    const auto it =
        names.constFind(name);

    if (it != names.constEnd()) {
        return m_language ==
                Language::German
            ? it.value().second
            : it.value().first;
    }

    QString readable = name;

    readable.replace(
        QLatin1Char('_'),
        QLatin1Char(' ')
    );

    // Vendor-specific attributes that are not in the
    // known translation table remain readable instead
    // of exposing raw smartctl identifiers with underscores.
    return readable;
}

QString MainWindow::formatTemperature(
    int celsius
) const
{
    if (m_temperatureUnit ==
        TemperatureUnit::Fahrenheit) {

        const double fahrenheit =
            (static_cast<double>(celsius) * 9.0 / 5.0) + 32.0;

        return QStringLiteral("%1 °F")
            .arg(
                fahrenheit,
                0,
                'f',
                0
            );
    }

    return QStringLiteral("%1 °C")
        .arg(celsius);
}

void MainWindow::toggleSerialVisibility()
{
    m_serialVisible =
        !m_serialVisible;

    m_serialEdit->setEchoMode(
        m_serialVisible
            ? QLineEdit::Normal
            : QLineEdit::Password
    );

    if (m_hideSerialAction) {
        QSignalBlocker blocker(
            m_hideSerialAction
        );

        m_hideSerialAction->setChecked(
            !m_serialVisible
        );
    }

    updateSerialButton();
}

void MainWindow::updateSerialButton()
{
    const QString iconName =
        m_serialVisible
        ? QStringLiteral("view-hidden")
        : QStringLiteral("view-visible");

    m_serialButton->setIcon(
        QIcon::fromTheme(iconName)
    );

    m_serialButton->setToolTip(
        m_serialVisible
        ? tx(
              "Hide serial number",
              "Seriennummer verbergen"
          )
        : tx(
              "Show serial number",
              "Seriennummer anzeigen"
          )
    );
}

void MainWindow::updateRawColumn()
{
    if (!m_table)
        return;

    const bool showRaw =
        m_showRawAction &&
        m_showRawAction->isChecked();

    m_table->setColumnHidden(
        ColumnRaw,
        !showRaw
    );

    saveCurrentTableWidths();

    applyTableColumnLayout();

    restoreCurrentTableWidths();
}

void MainWindow::clearDisplay()
{
    m_hasCurrentData = false;
    m_currentData = QJsonObject();

    m_titleLabel->setText(
        QStringLiteral("LinDiskInfo")
    );

    setHealth(
        HealthState::Unknown
    );

    setTemperature(-1);

    m_firmwareValue->setText(
        QStringLiteral("—")
    );

    m_serialEdit->clear();

    m_interfaceValue->setText(
        QStringLiteral("—")
    );

    m_transferValue->setText(
        QStringLiteral("—")
    );

    m_standardValue->setText(
        QStringLiteral("—")
    );

    m_featuresValue->setText(
        QStringLiteral("—")
    );

    m_readsValue->setText(
        QStringLiteral("—")
    );

    m_writesValue->setText(
        QStringLiteral("—")
    );

    m_rotationValue->setText(
        QStringLiteral("—")
    );

    m_powerCyclesValue->setText(
        QStringLiteral("—")
    );

    m_powerHoursValue->setText(
        QStringLiteral("—")
    );

    m_table->setRowCount(0);
}

void MainWindow::setStatus(
    const QString &text
)
{
    m_statusLabel->setText(text);
}

quint64 MainWindow::jsonUnsigned(
    const QJsonValue &value
)
{
    if (value.isDouble()) {
        const qint64 integer =
            value.toInteger();

        return integer < 0
            ? 0
            : static_cast<quint64>(
                  integer
              );
    }

    if (value.isString()) {
        bool ok = false;

        const quint64 result =
            value.toString()
                .toULongLong(&ok);

        return ok ? result : 0;
    }

    return 0;
}

QString MainWindow::formatBytes(
    quint64 bytes
)
{
    const QString unit =
        QSettings().value(
            QStringLiteral(
                "storageUnit"
            ),
            QStringLiteral("GB")
        ).toString();

    long double divisor =
        1000000000.0L;

    QString suffix =
        QStringLiteral("GB");

    if (unit == QStringLiteral("GiB")) {
        divisor =
            1073741824.0L;

        suffix =
            QStringLiteral("GiB");

    } else if (
        unit == QStringLiteral("TB")
    ) {
        divisor =
            1000000000000.0L;

        suffix =
            QStringLiteral("TB");

    } else if (
        unit == QStringLiteral("TiB")
    ) {
        divisor =
            1099511627776.0L;

        suffix =
            QStringLiteral("TiB");
    }

    const long double value =
        static_cast<long double>(
            bytes
        ) / divisor;

    int decimals = 1;

    if (value < 1.0L) {
        decimals = 3;
    } else if (value < 10.0L) {
        decimals = 2;
    }

    QString number =
        QString::number(
            static_cast<double>(value),
            'f',
            decimals
        );

    while (
        number.contains(
            QLatin1Char('.')
        ) &&
        number.endsWith(
            QLatin1Char('0')
        )
    ) {
        number.chop(1);
    }

    if (number.endsWith(
            QLatin1Char('.')
        )) {
        number.chop(1);
    }

    return QStringLiteral(
        "%1 %2"
    ).arg(
        number,
        suffix
    );
}

QString MainWindow::formatNumber(
    quint64 value
)
{
    QString text =
        QString::number(value);

    for (int i = text.size() - 3;
         i > 0;
         i -= 3) {

        text.insert(
            i,
            QLatin1Char('.')
        );
    }

    return text;
}

QString MainWindow::formatRawValue(
    quint64 value
) const
{
    const QString mode =
        QSettings().value(
            QStringLiteral(
                "rawValueMode"
            ),
            QStringLiteral("hex")
        ).toString();

    if (mode ==
        QStringLiteral("dec")) {

        return QString::number(
            value
        );
    }

    if (mode ==
        QStringLiteral("dec2")) {

        QStringList groups;

        for (int shift = 48;
             shift >= 0;
             shift -= 16) {

            const quint64 part =
                (value >> shift) &
                0xFFFFULL;

            groups.append(
                QStringLiteral("%1")
                    .arg(
                        part,
                        5,
                        10,
                        QLatin1Char('0')
                    )
            );
        }

        return groups.join(
            QLatin1Char(' ')
        );
    }

    if (mode ==
        QStringLiteral("dec1")) {

        QStringList groups;

        for (int shift = 56;
             shift >= 0;
             shift -= 8) {

            const quint64 part =
                (value >> shift) &
                0xFFULL;

            groups.append(
                QStringLiteral("%1")
                    .arg(
                        part,
                        3,
                        10,
                        QLatin1Char('0')
                    )
            );
        }

        return groups.join(
            QLatin1Char(' ')
        );
    }

    return QStringLiteral("%1")
        .arg(
            value,
            16,
            16,
            QLatin1Char('0')
        )
        .toUpper();
}
