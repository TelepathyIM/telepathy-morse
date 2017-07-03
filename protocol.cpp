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

#include "protocol.hpp"
#include "connection.hpp"

#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>

MorseProtocol::MorseProtocol(const QDBusConnection &dbusConnection, const QString &name)
    : BaseProtocol(dbusConnection, name)
{
    qDebug() << Q_FUNC_INFO;
    setParameters(Tp::ProtocolParameterList()
                  << Tp::ProtocolParameter(QLatin1String("account"), QLatin1String("s"), Tp::ConnMgrParamFlagRequired)
                  << Tp::ProtocolParameter(QLatin1String("keepalive-interval"), QLatin1String("u"), Tp::ConnMgrParamFlagHasDefault, 15)
                  << Tp::ProtocolParameter(QLatin1String("proxy-type"), QLatin1String("s"), 0) // ATM we have only socks5 support, but Telegram supports http-proxy too
                  << Tp::ProtocolParameter(QLatin1String("proxy-server"), QLatin1String("s"), 0)
                  << Tp::ProtocolParameter(QLatin1String("proxy-port"), QLatin1String("u"), 0)
                  << Tp::ProtocolParameter(QLatin1String("proxy-username"), QLatin1String("s"), 0)
                  << Tp::ProtocolParameter(QLatin1String("proxy-password"), QLatin1String("s"), Tp::ConnMgrParamFlagSecret)
                  );

    setRequestableChannelClasses(MorseConnection::getRequestableChannelList());

    // callbacks
    setCreateConnectionCallback(memFun(this, &MorseProtocol::createConnection));
    setIdentifyAccountCallback(memFun(this, &MorseProtocol::identifyAccount));
    setNormalizeContactCallback(memFun(this, &MorseProtocol::normalizeContact));

    addrIface = Tp::BaseProtocolAddressingInterface::create();
    addrIface->setAddressableVCardFields(QStringList() << QLatin1String("tel"));
    addrIface->setAddressableUriSchemes(QStringList() << QLatin1String("tel"));
    addrIface->setNormalizeVCardAddressCallback(memFun(this, &MorseProtocol::normalizeVCardAddress));
    addrIface->setNormalizeContactUriCallback(memFun(this, &MorseProtocol::normalizeContactUri));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(addrIface));

    avatarsIface = Tp::BaseProtocolAvatarsInterface::create();
    avatarsIface->setAvatarDetails(Tp::AvatarSpec(/* supportedMimeTypes */ QStringList() << QLatin1String("image/jpeg"),
                                                  /* minHeight */ 0, /* maxHeight */ 160, /* recommendedHeight */ 160,
                                                  /* minWidth */ 0, /* maxWidth */ 160, /* recommendedWidth */ 160,
                                                  /* maxBytes */ 10240));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(avatarsIface));

    presenceIface = Tp::BaseProtocolPresenceInterface::create();
    presenceIface->setStatuses(Tp::PresenceSpecList(MorseConnection::getSimpleStatusSpecMap()));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(presenceIface));
}

MorseProtocol::~MorseProtocol()
{
}

Tp::BaseConnectionPtr MorseProtocol::createConnection(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << parameters;
    Q_UNUSED(error)

    Tp::BaseConnectionPtr newConnection = Tp::BaseConnection::create<MorseConnection>(QLatin1String("morse"), name(), parameters);

    return newConnection;
}

QString MorseProtocol::identifyAccount(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << parameters;
    error->set(QLatin1String("IdentifyAccount.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MorseProtocol::normalizeContact(const QString &contactId, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << contactId;
    error->set(QLatin1String("NormalizeContact.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MorseProtocol::normalizeVCardAddress(const QString &vcardField, const QString vcardAddress,
        Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << vcardField << vcardAddress;
    error->set(QLatin1String("NormalizeVCardAddress.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MorseProtocol::normalizeContactUri(const QString &uri, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << uri;
    error->set(QLatin1String("NormalizeContactUri.Error.NotImplemented"), QLatin1String(""));
    return QString();
}
