/*
    This file is part of the telepathy-morse connection manager.
    Copyright (C) 2016 Alexandr Akulich <akulichalexander@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <QCoreApplication>

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/Constants>
#include <TelepathyQt/Debug>

#include "protocol.hpp"

#ifdef ENABLE_DEBUG_IFACE
#include "debug.hpp"
#endif

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QLatin1String("telepathy-morse"));

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);
#ifdef ENABLE_DEBUG_IFACE
    enableDebugInterface();
#endif

    Tp::BaseProtocolPtr proto = Tp::BaseProtocol::create<MorseProtocol>(QLatin1String("telegram"));
    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(QLatin1String("morse"));

    proto->setEnglishName(QLatin1String("Telegram"));
    proto->setIconName(QLatin1String("telegram"));
    proto->setVCardField(QLatin1String("tel"));

    cm->addProtocol(proto);
    cm->registerObject();

    return app.exec();
}
