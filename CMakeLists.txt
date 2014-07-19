project(telepathy-morse)

cmake_minimum_required(VERSION 2.8)

find_package(TelepathyQt4 0.9.2.1 REQUIRED)

find_package(TelepathyQt4 0.9.2.1 REQUIRED)
find_package(TelepathyQt4Service 0.9.2.1 REQUIRED)

find_package(TelegramQt4 REQUIRED)

find_package(Qt4 REQUIRED)

include(GNUInstallDirs)

include_directories(
    ${QT_INCLUDES}
    ${TELEPATHY_QT4_INCLUDE_DIR}
    ${TELEGRAM_QT4_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}
)

set(morse_SOURCES
    main.cpp
    connection.cpp
    connection.hpp
    protocol.cpp
    protocol.hpp
    textchannel.cpp
    textchannel.hpp
)

add_executable(telepathy-morse ${morse_SOURCES})

set_target_properties(telepathy-morse PROPERTIES AUTOMOC TRUE)

target_link_libraries(telepathy-morse
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${TELEPATHY_QT4_LIBRARIES}
    ${TELEPATHY_QT4_SERVICE_LIBRARIES}
    ${TELEGRAM_QT4_LIBRARIES}
    -lcrypto
    -lz
)

configure_file(morse.service.in org.freedesktop.Telepathy.ConnectionManager.morse.service)

install(
    TARGETS telepathy-morse
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

install(
    FILES morse.manager
    DESTINATION share/telepathy/managers
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Telepathy.ConnectionManager.morse.service
    DESTINATION share/dbus-1/services
)

#install(
#    FILES morse.png
#    DESTINATION share/icons/hicolor/48x48/apps
#)
