
Telepathy-Morse
================

Morse is a Qt-based Telegram connection manager for the Telepathy framework.

Features
========

* Contact list with first/last names
* Contact avatars
* Personal messaging (one to one, no secret chat yet)
* Basic group chat (unable to create a group, Telegram channels are not implemented yet)
* User typing events
* Two-step verification
* Full message delivery status support
* Own presence (online, offline, hidden)
* Loading unread messages on connect
* DBus activation
* Sessions (Means that you don't have to get confirmation code again and again)
* Restoring connection on network problem
* Supported incoming multimedia messages:
  - Geopoint (text/plain URI/RFC 5870, application/geo+json)
  - Stickers (text/plain with Unicode alternative)

Requirements
============

* CMake-2.8.12+ (required by TelepathyQt)
* Qt4 or Qt5
* TelepathyQt-0.9.7
* TelegramQt-0.1.0

Note: TelegramQt is available at https://github.com/Kaffeine/telegram-qt
Note: In order to use Morse, you need to have a complementary Telepathy Client application, such as KDE-Telepathy or Empathy.

Installation
============

    git clone https://github.com/TelepathyQt/telepathy-morse.git

or

    tar -xf telepathy-morse-0.1.0.tar.gz

    mkdir morse-build
    cd morse-build
    cmake ../telepathy-morse

Information about CMake build:
* By default CMake looks for the Qt5 build. You can pass USE_QT4 option (-DUSE_QT4=true) to process Qt4 build.
* Default installation prefix is /usr/local. Probably, you'll need to set CMAKE_INSTALL_PREFIX to /usr to make DBus activation works. (-DCMAKE_INSTALL_PREFIX=/usr)

<!-- markdown "code after list" workaround -->

    make -j4
    make install

Known issues
============

* Initial low-level encryption sometimes generates bad random values, which rarely can leads to “connection doesn’t work” issue.
* Authentication doesn't work on the Sailfish OS. It is possible to copy an authorization key from your pc to the device, the directory is ~/.cache/telepathy-morse/secrets.

License
=======

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

See COPYNG for details.
