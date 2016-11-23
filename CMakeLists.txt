project(telepathy-morse)

cmake_minimum_required(VERSION 2.8)

if (USE_QT4)
    find_package(TelepathyQt4 0.9.7 REQUIRED)
    find_package(TelepathyQt4Service 0.9.7 REQUIRED)
    set(TELEPATHY_QT_VERSION ${TELEPATHY_QT4_VERSION})

    find_package(TelegramQt4 REQUIRED)

    find_package(Qt4 4.6.0 REQUIRED QtCore QtNetwork)
else()
    find_package(TelepathyQt5 0.9.7 REQUIRED)
    find_package(TelepathyQt5Service 0.9.7 REQUIRED)
    set(TELEPATHY_QT_VERSION ${TELEPATHY_QT5_VERSION})

    find_package(TelegramQt5 REQUIRED)

    find_package(Qt5 REQUIRED COMPONENTS Core DBus Xml Network)
endif()

include(GNUInstallDirs)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

add_definitions(-DQT_NO_CAST_FROM_ASCII)

if (TELEPATHY_QT_VERSION VERSION_LESS "0.9.8")
    set(USE_BUNDLED_GROUPS_IFACE TRUE)
endif()

set(morse_SOURCES
    main.cpp
    connection.cpp
    connection.hpp
    debug.cpp
    debug.hpp
    identifier.cpp
    identifier.hpp
    protocol.cpp
    protocol.hpp
    textchannel.cpp
    textchannel.hpp
)

if (USE_BUNDLED_GROUPS_IFACE)
    list(APPEND morse_SOURCES
        contactgroups.cpp
        contactgroups.hpp
    )
endif()

add_executable(telepathy-morse ${morse_SOURCES})

set_target_properties(telepathy-morse PROPERTIES AUTOMOC TRUE)

if (USE_QT4)
    target_include_directories(telepathy-morse PRIVATE
        ${QT_INCLUDES}
        ${TELEPATHY_QT4_INCLUDE_DIR}
        ${TELEGRAM_QT4_INCLUDE_DIR})
    target_link_libraries(telepathy-morse
        ${QT_QTCORE_LIBRARY}
        ${QT_QTDBUS_LIBRARY}
        ${QT_QTNETWORK_LIBRARY}
        ${QT_QTGUI_LIBRARY}
        ${TELEPATHY_QT4_LIBRARIES}
        ${TELEPATHY_QT4_SERVICE_LIBRARIES}
        ${TELEGRAM_QT4_LIBRARIES}
        -lcrypto
        -lz
    )
else()
    target_include_directories(telepathy-morse PRIVATE
        ${TELEPATHY_QT5_INCLUDE_DIR}
        ${TELEGRAM_QT5_INCLUDE_DIR})

    target_link_libraries(telepathy-morse
        Qt5::Core
        Qt5::DBus
        Qt5::Network
        ${TELEPATHY_QT5_LIBRARIES}
        ${TELEPATHY_QT5_SERVICE_LIBRARIES}
        ${TELEGRAM_QT5_LIBRARIES}
        -lcrypto
        -lz
    )
endif()

configure_file(morse.service.in org.freedesktop.Telepathy.ConnectionManager.morse.service)

install(
    TARGETS telepathy-morse
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

install(
    FILES morse.manager
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/telepathy/managers
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Telepathy.ConnectionManager.morse.service
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/services
)

#install(
#    FILES morse.png
#    DESTINATION share/icons/hicolor/48x48/apps
#)
