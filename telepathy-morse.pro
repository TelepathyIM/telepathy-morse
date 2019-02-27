QT = core network dbus

TEMPLATE = app
TARGET   = telepathy-morse
VERSION = 0.2.0

CONFIG += c++11
CONFIG += link_pkgconfig
PKGCONFIG += openssl zlib
PKGCONFIG += TelepathyQt5
PKGCONFIG += TelepathyQt5Service
PKGCONFIG += TelegramQt5

SOURCES = main.cpp \
    connection.cpp \
    protocol.cpp \
    textchannel.cpp

HEADERS = \
    connection.hpp \
    protocol.hpp \
    textchannel.hpp

OTHER_FILES += CMakeLists.txt
OTHER_FILES += rpm/telepathy-morse.spec

# Installation directories
isEmpty(INSTALL_PREFIX) {
    unix {
        INSTALL_PREFIX = /usr/local
    } else {
        INSTALL_PREFIX = $$[QT_INSTALL_PREFIX]
    }
}

isEmpty(INSTALL_LIBEXEC_DIR) {
    INSTALL_LIBEXEC_DIR = $$INSTALL_PREFIX/libexec
}

isEmpty(INSTALL_DATA_DIR) {
    INSTALL_DATA_DIR = $$INSTALL_PREFIX/share
}

# Configure the dbus service file
DBUS_SERVICE_FILE_TEMPLATE = $$cat($$PWD/dbus-service.in, blob)
DBUS_SERVICE_FILE_CONTENT = $$replace(SERVICE_FILE_TEMPLATE, @CMAKE_INSTALL_FULL_LIBEXECDIR@, $$target.path)
DBUS_SERVICE_FILE_NAME = $$OUT_PWD/org.freedesktop.Telepathy.ConnectionManager.morse.service
write_file($$DBUS_SERVICE_FILE_NAME, DBUS_SERVICE_FILE_CONTENT)

# Installation
target.path = $$INSTALL_LIBEXEC_DIR

telepathy_manager.files = morse.manager
telepathy_manager.path = $$INSTALL_DATA_DIR/telepathy/managers

dbus_services.files = $$DBUS_SERVICE_FILE_NAME
dbus_services.path = $$INSTALL_DATA_DIR/dbus-1/services

INSTALLS += target telepathy_manager dbus_services
