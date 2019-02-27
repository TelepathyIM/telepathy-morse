project(telepathy-morse)

cmake_minimum_required(VERSION 2.8)

find_package(TelepathyQt5 0.9.6 REQUIRED)
find_package(TelepathyQt5Service 0.9.6 REQUIRED)
set(TELEPATHY_QT_VERSION ${TELEPATHY_QT5_VERSION})

find_package(TelegramQt5 REQUIRED)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Xml Network)

include(GNUInstallDirs)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

add_definitions(-DQT_NO_CAST_FROM_ASCII)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(morse_SOURCES
    main.cpp
    connection.cpp
    connection.hpp
    protocol.cpp
    protocol.hpp
    textchannel.cpp
    textchannel.hpp
)

if (TELEPATHY_QT_VERSION VERSION_LESS "0.9.7")
    message(WARNING "TelepathyQt version < 0.9.7, so group chat and debug interface support will be disabled.")
else()
    set(ENABLE_GROUP_CHAT TRUE)
    set(ENABLE_DEBUG_IFACE TRUE)
endif()

if (ENABLE_DEBUG_IFACE)
    add_definitions(-DENABLE_DEBUG_IFACE)
    list(APPEND morse_SOURCES
        debug.cpp
        debug.hpp
    )
endif()

if (ENABLE_GROUP_CHAT)
    add_definitions(-DENABLE_GROUP_CHAT)

    if (TELEPATHY_QT_VERSION VERSION_LESS "0.9.8")
        add_definitions(-DUSE_BUNDLED_GROUPS_IFACE)
        set(USE_BUNDLED_GROUPS_IFACE TRUE)
    endif()
endif()

if (USE_BUNDLED_GROUPS_IFACE)
    list(APPEND morse_SOURCES
        contactgroups.cpp
        contactgroups.hpp
    )
endif()

# 'Unstable' stuff which is not a part of TelegramQt yet
list(APPEND morse_SOURCES
    extras/CFileManager.cpp
)

add_executable(telepathy-morse ${morse_SOURCES})

set_target_properties(telepathy-morse PROPERTIES AUTOMOC TRUE)

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

configure_file(dbus-service.in org.freedesktop.Telepathy.ConnectionManager.morse.service)

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
