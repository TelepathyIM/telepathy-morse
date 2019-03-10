
set(QT_VERSION_MAJOR "5")
find_package(Qt5 REQUIRED COMPONENTS Core DBus Xml Network Test)
find_package(ZLIB REQUIRED)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH "")

# don't add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/telegram-qt/server
    ${CMAKE_SOURCE_DIR}/telegram-qt/tests/data
    ${CMAKE_SOURCE_DIR}/telegram-qt/tests/utils
    ${TELEPATHY_QT5_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
)

set(morse_lib_SOURCES
    ../connection.cpp
    ../connection.hpp
    ../protocol.cpp
    ../protocol.hpp
    ../textchannel.cpp
    ../textchannel.hpp
    ../extras/CFileManager.cpp

    ../contactgroups.cpp
    ../contactgroups.hpp
)

set(test_extra_META_HEADERS
    ${CMAKE_SOURCE_DIR}/telegram-qt/tests/utils/TestAuthProvider.hpp
)

set(test_name "telepathy-morse-test")
add_executable(${test_name} tst_morse.cpp ${morse_lib_SOURCES} ${test_extra_META_HEADERS})
target_link_libraries(${test_name}
    Qt5::Core
    Qt5::Test
    ${TELEPATHY_QT5_LIBRARIES}
    ${TELEPATHY_QT5_SERVICE_LIBRARIES}
    TelegramQt${QT_VERSION_MAJOR}
)
add_test(NAME ${test_name} COMMAND ${test_name} -maxwarnings 0)

target_link_libraries(${test_name}
    TelegramServerQt${QT_VERSION_MAJOR}
    test_keys_data
)
