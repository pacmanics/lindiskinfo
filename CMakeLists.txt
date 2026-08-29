# SPDX-FileCopyrightText: 2026 PacmanicS
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

project(LinDiskInfo
    VERSION 1.0.4
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(
    Qt6
    REQUIRED
    COMPONENTS Widgets Network
)

qt_standard_project_setup()

qt_add_executable(
    lindiskinfo
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/device/mainwindowdevice.cpp
    src/device/mainwindownvme.cpp
    src/device/mainwindowata.cpp
    src/device/atawear.cpp
    src/device/atawear.h
    src/device/mainwindowformat.cpp
    src/ui/mainwindowtable.cpp
    src/ui/smarttablecolumns.h
    src/tray/mainwindowtray.cpp
    src/drives/mainwindowdrives.cpp
    src/maintenance/mainwindowmaintenance.cpp
    src/settings/mainwindowsettings.cpp
    src/theme/mainwindowtheme.cpp
    src/ui/mainwindowinterface.cpp
    src/ui/mainwindowmenus.cpp
    src/ui/mainwindowatamenus.cpp
    src/ui/mainwindowcontrollermenus.cpp
    src/ui/mainwindowmaintenancemenus.cpp
    src/ui/mainwindowadvancedmenus.cpp
    src/ui/mainwindowadvancedhealth.cpp
    src/ui/mainwindowadvancedsystem.cpp
    src/ui/mainwindowadvanceddrive.cpp
    src/ui/mainwindowlanguage.cpp
    src/reports/mainwindowreports.cpp
    src/history/mainwindowhistory.cpp
    src/history/lindiskhistoryplot.cpp
    src/history/lindiskhistoryplot.h
    src/theme/waifuthemes.cpp
    src/theme/waifuthemes.h
    src/theme/themeassetstore.cpp
    src/theme/themeassetstore.h
    src/theme/themebackgroundwidget.cpp
    src/theme/themebackgroundwidget.h
    src/ui/responsivetablelayout.cpp
    src/ui/responsivetablelayout.h
    src/smartctlbackend.cpp
    src/smartctlbackend.h
    src/privilegedhelper.cpp
    src/privilegedhelper.h
)

qt_add_resources(
    lindiskinfo
    "lindiskinfo_resources"
    PREFIX
        "/icons"
    BASE
        "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/hicolor/256x256/apps"
    FILES
        data/icons/hicolor/256x256/apps/lindiskinfo.png
)


qt_add_resources(
    lindiskinfo
    "lindiskinfo_asset_resources"
    PREFIX
        "/assets"
    BASE
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
    FILES
        data/lindiskinfo-assets.bin
)


target_link_libraries(
    lindiskinfo
    PRIVATE
        Qt6::Widgets
        Qt6::Network
)

install(
    TARGETS lindiskinfo
    RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}
)

configure_file(
    data/lindiskinfo.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/lindiskinfo.desktop
    @ONLY
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/lindiskinfo.desktop
    DESTINATION
        ${CMAKE_INSTALL_DATADIR}/applications
)

foreach(
    LINDISKINFO_ICON_SIZE
    IN ITEMS
        16
        32
        48
        64
        128
        256
        512
)
    install(
        FILES
            data/icons/hicolor/${LINDISKINFO_ICON_SIZE}x${LINDISKINFO_ICON_SIZE}/apps/lindiskinfo.png
        DESTINATION
            ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${LINDISKINFO_ICON_SIZE}x${LINDISKINFO_ICON_SIZE}/apps
    )
endforeach()

install(
    FILES
        data/io.github.pacmanics.lindiskinfo.metainfo.xml
    DESTINATION
        ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(
    FILES
        LICENSE
    DESTINATION
        ${CMAKE_INSTALL_DATADIR}/licenses/lindiskinfo
)
