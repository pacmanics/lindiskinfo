// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../mainwindow.h"
#include "../theme/themebackgroundwidget.h"
#include "../theme/waifuthemes.h"
#include "../ui/responsivetablelayout.h"

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
#include <cmath>
#include <limits>
#include <utility>

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
