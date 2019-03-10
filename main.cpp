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

#include <TelegramQt/TelegramNamespace>

#include "protocol.hpp"

#ifdef ENABLE_DEBUG_IFACE
#include "debug.hpp"
#endif

static const QLatin1String c_protocolName = QLatin1String("telegram");
static const QLatin1String c_managerName = QLatin1String("morse");

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QLatin1String("TelepathyIM"));
    app.setApplicationName(QLatin1String("telepathy-morse"));

    Telegram::initialize();
    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);
#ifdef ENABLE_DEBUG_IFACE
    enableDebugInterface();
#endif

    MorseProtocolPtr proto = Tp::BaseProtocol::create<MorseProtocol>(c_protocolName);
    proto->setManagerName(c_managerName);
    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(c_managerName);

    if (!cm->addProtocol(proto)) {
        qCritical() << "Unable to add" << proto->name() << "protocol";
        return 1;
    }
    if (!cm->registerObject()) {
        qCritical() << "Unable to register the cm service";
        return 2;
    }

    return app.exec();
}
