set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC TRUE)

find_package(Qt5 REQUIRED COMPONENTS Core Qml)

if(NOT INSTALL_QML_IMPORT_DIR)
    set(INSTALL_QML_IMPORT_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/qt5/qml")
endif()

add_library(MorseQmlPlugin SHARED plugin.cpp)

target_link_libraries(MorseQmlPlugin PUBLIC
    Qt5::Core
    Qt5::Qml
    MorseInfo
)

target_compile_definitions(MorseQmlPlugin PRIVATE
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_NO_CAST_FROM_BYTEARRAY
    QT_USE_FAST_OPERATOR_PLUS
    QT_USE_QSTRINGBUILDER
    QT_STRICT_ITERATORS
)

install(
    TARGETS MorseQmlPlugin
    DESTINATION ${INSTALL_QML_IMPORT_DIR}/Morse
)

install(
    FILES qmldir plugins.qmltypes
    DESTINATION ${INSTALL_QML_IMPORT_DIR}/Morse
)

set(GENERATED_QML_TYPES "${CMAKE_CURRENT_SOURCE_DIR}/plugins.qmltypes")
add_custom_target(generate-qmltypes
    COMMAND ${_qt5Core_install_prefix}/bin/qmlplugindump -noinstantiate -nonrelocatable -v Morse 0.1 ${INSTALL_QML_IMPORT_DIR} > ${CMAKE_CURRENT_SOURCE_DIR}/plugins.qmltypes
    COMMAND sed -i "s/\\[\"QtQuick 2.*\"\\]/\\[\"QtQuick 2.6\"\\]/" ${CMAKE_CURRENT_SOURCE_DIR}/plugins.qmltypes
    COMMAND sed -i "s/Morse 0.1.*/Morse 0.1\'/" ${CMAKE_CURRENT_SOURCE_DIR}/plugins.qmltypes
    SOURCES qmldir
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins.qmltypes"
    VERBATIM
)

unset(INSTALL_QML_IMPORT_DIR)
