cmake_minimum_required(VERSION 3.1)

project(TelepathyMorse
    VERSION 0.2.0
    LANGUAGES CXX
)

set(OVERRIDE_CXX_STANDARD 11 CACHE STRING "Compile with custom C++ standard version")

set(CMAKE_CXX_STANDARD ${OVERRIDE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Xml Network)
find_package(TelegramQt5 REQUIRED)
find_package(TelepathyQt5 0.9.6 REQUIRED)
find_package(TelepathyQt5Service 0.9.6 REQUIRED)
set(TELEPATHY_QT_VERSION ${TELEPATHY_QT5_VERSION})

include(GNUInstallDirs)
include(FeatureSummary)

add_executable(telepathy-morse main.cpp)

target_sources(telepathy-morse PRIVATE
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

set(MORSE_BUILD_VERSION ${BUILD_VERSION})

set_target_properties(telepathy-morse
    PROPERTIES
        AUTOMOC TRUE
)

configure_file(version.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" @ONLY)
target_sources(telepathy-morse PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
)

if (TELEPATHY_QT_VERSION VERSION_LESS "0.9.7")
    message(WARNING "TelepathyQt version < 0.9.7, so group chat and debug interface support will be disabled.")
else()
    set(ENABLE_GROUP_CHAT TRUE)
    set(ENABLE_DEBUG_IFACE TRUE)
endif()

if (ENABLE_DEBUG_IFACE)
    target_compile_definitions(telepathy-morse PRIVATE
        ENABLE_DEBUG_IFACE
    )
    target_sources(telepathy-morse PRIVATE
        debug.cpp
        debug.hpp
    )
endif()

if (ENABLE_GROUP_CHAT)
    target_compile_definitions(telepathy-morse PRIVATE
        ENABLE_GROUP_CHAT
    )

    if (TELEPATHY_QT_VERSION VERSION_LESS "0.9.8")
        target_compile_definitions(telepathy-morse PRIVATE
            USE_BUNDLED_GROUPS_IFACE
        )
        target_sources(telepathy-morse PRIVATE
            contactgroups.cpp
            contactgroups.hpp
        )
        set(USE_BUNDLED_GROUPS_IFACE TRUE)
    endif()
endif()

target_include_directories(telepathy-morse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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

target_compile_definitions(telepathy-morse PRIVATE
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_CAST_TO_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_RESTRICTED_CAST_FROM_ASCII
    QT_STRICT_ITERATORS
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
