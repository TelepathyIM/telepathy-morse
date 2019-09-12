cmake_minimum_required(VERSION 3.1)

project(telepathy-morse
    VERSION 0.2.0
    LANGUAGES CXX
)

set(OVERRIDE_CXX_STANDARD 11 CACHE STRING "Compile with custom C++ standard version")

set(CMAKE_CXX_STANDARD ${OVERRIDE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(TelepathyQt5 0.9.6 REQUIRED)
find_package(TelepathyQt5Service 0.9.6 REQUIRED)
set(TELEPATHY_QT_VERSION ${TELEPATHY_QT5_VERSION})

find_package(TelegramQt5 REQUIRED)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Xml Network)

include(GNUInstallDirs)
include(FeatureSummary)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

add_definitions(-DQT_NO_CAST_FROM_ASCII)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(morse_SOURCES
    main.cpp
    connection.cpp
    connection.hpp
    datastorage.cpp
    datastorage.hpp
    info.cpp
    info.hpp
    protocol.cpp
    protocol.hpp
    textchannel.cpp
    textchannel.hpp
)

if (NOT BUILD_VERSION)
    set(BUILD_VERSION "unknown")
endif()

set(MORSE_VERSION "${PROJECT_VERSION}")
set(MORSE_BUILD_VERSION ${BUILD_VERSION})

configure_file(version.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" @ONLY)
list(APPEND morse_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

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

add_executable(telepathy-morse ${morse_SOURCES})

set_target_properties(telepathy-morse PROPERTIES AUTOMOC TRUE)

target_include_directories(telepathy-morse PRIVATE
    ${TELEPATHY_QT5_INCLUDE_DIR}
)

target_link_libraries(telepathy-morse
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    ${TELEPATHY_QT5_LIBRARIES}
    ${TELEPATHY_QT5_SERVICE_LIBRARIES}
    TelegramQt5::Core
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

message(STATUS "${PROJECT_NAME} configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Qt: ${Qt5_VERSION} at ${_qt5Core_install_prefix}")

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
