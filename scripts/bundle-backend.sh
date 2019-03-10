#!/bin/sh

BACKEND_REPO=/home/kaffeine/devel/telepathy/telepathy-bundle/telegram-qt/

# Remove all find_package TelegramQt
sed -i '/find_package(TelegramQt/d' CMakeLists.txt
sed -i 's/${TELEGRAM_QT._INCLUDE_DIR}/telegram-qt\/TelegramQt/g' CMakeLists.txt
sed -i 's/\${TELEGRAM_QT._LIBRARIES}/TelegramQt\${QT_VERSION_MAJOR}/g' CMakeLists.txt
echo "add_subdirectory(telegram-qt)" >> CMakeLists.txt

mkdir telegram-qt
cd telegram-qt
ln -s $BACKEND_REPO/* .
