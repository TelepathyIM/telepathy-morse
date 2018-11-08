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

#include <TelegramQt/TelegramNamespace>

#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>

static const QLatin1String c_account = QLatin1String("account");
static const QLatin1String c_serverAddress = QLatin1String("server-address");
static const QLatin1String c_serverPort = QLatin1String("server-port");
static const QLatin1String c_serverKey = QLatin1String("server-key");
static const QLatin1String c_proxyType = QLatin1String("proxy-type");
static const QLatin1String c_proxyAddress = QLatin1String("proxy-address");
static const QLatin1String c_proxyPort = QLatin1String("proxy-port");
static const QLatin1String c_proxyUsername= QLatin1String("proxy-username");
static const QLatin1String c_proxyPassword = QLatin1String("proxy-password");
static const QLatin1String c_keepalive = QLatin1String("keepalive");
static const QLatin1String c_keepaliveInterval = QLatin1String("keepalive-interval");

MorseProtocol::MorseProtocol(const QDBusConnection &dbusConnection, const QString &name)
    : BaseProtocol(dbusConnection, name)
{
    qDebug() << Q_FUNC_INFO;
    setEnglishName(QLatin1String("Telegram"));
    setIconName(QLatin1String("telegram"));
    setVCardField(QLatin1String("tel"));

    setParameters(Tp::ProtocolParameterList()
                  << Tp::ProtocolParameter(c_account, QLatin1String("s"), Tp::ConnMgrParamFlagRequired)
                  << Tp::ProtocolParameter(c_serverAddress, QLatin1String("s"), Tp::ConnMgrParamFlagHasDefault, QString())
                  << Tp::ProtocolParameter(c_serverPort, QLatin1String("u"), 0)
                  << Tp::ProtocolParameter(c_serverKey, QLatin1String("s"), Tp::ConnMgrParamFlagHasDefault, QString())
                  << Tp::ProtocolParameter(c_keepalive, QLatin1String("b"), Tp::ConnMgrParamFlagHasDefault, true)
                  << Tp::ProtocolParameter(c_keepaliveInterval, QLatin1String("u"), Tp::ConnMgrParamFlagHasDefault, 15)
                  << Tp::ProtocolParameter(c_proxyType, QLatin1String("s"), 0) // ATM we have only socks5 support, but Telegram supports http-proxy too
                  << Tp::ProtocolParameter(c_proxyAddress, QLatin1String("s"), 0)
                  << Tp::ProtocolParameter(c_proxyPort, QLatin1String("u"), 0)
                  << Tp::ProtocolParameter(c_proxyUsername, QLatin1String("s"), 0)
                  << Tp::ProtocolParameter(c_proxyPassword, QLatin1String("s"), Tp::ConnMgrParamFlagSecret)
                  );

    setRequestableChannelClasses(MorseConnection::getRequestableChannelList());

    // callbacks
    setCreateConnectionCallback(memFun(this, &MorseProtocol::createConnection));
    setIdentifyAccountCallback(memFun(this, &MorseProtocol::identifyAccount));
    setNormalizeContactCallback(memFun(this, &MorseProtocol::normalizeContact));

    addrIface = Tp::BaseProtocolAddressingInterface::create();
    addrIface->setAddressableVCardFields({vcardField()});
    addrIface->setAddressableUriSchemes({QLatin1String("tg")});
    addrIface->setNormalizeVCardAddressCallback(memFun(this, &MorseProtocol::normalizeVCardAddress));
    addrIface->setNormalizeContactUriCallback(memFun(this, &MorseProtocol::normalizeContactUri));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(addrIface));

    avatarsIface = Tp::BaseProtocolAvatarsInterface::create();
    avatarsIface->setAvatarDetails(MorseConnection::avatarDetails());
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(avatarsIface));

    presenceIface = Tp::BaseProtocolPresenceInterface::create();
    presenceIface->setStatuses(Tp::PresenceSpecList(MorseConnection::getSimpleStatusSpecMap()));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(presenceIface));
}

MorseProtocol::~MorseProtocol()
{
}

QString MorseProtocol::getAccount(const QVariantMap &parameters)
{
    return parameters.value(c_account).toString();
}

QString MorseProtocol::getServerAddress(const QVariantMap &parameters)
{
    return parameters.value(c_serverAddress).toString();
}

quint16 MorseProtocol::getServerPort(const QVariantMap &parameters)
{
    return parameters.value(c_serverPort, 0u).toUInt();
}

QString MorseProtocol::getServerKey(const QVariantMap &parameters)
{
    return parameters.value(c_serverKey).toString();
}

QString MorseProtocol::getProxyType(const QVariantMap &parameters)
{
    return parameters.value(c_proxyType).toString();
}

QString MorseProtocol::getProxyAddress(const QVariantMap &parameters)
{
    return parameters.value(c_proxyAddress).toString();
}

quint16 MorseProtocol::getProxyPort(const QVariantMap &parameters)
{
    return parameters.value(c_proxyPort, 0u).toUInt();
}

QString MorseProtocol::getProxyUsername(const QVariantMap &parameters)
{
    return parameters.value(c_proxyUsername).toString();
}

QString MorseProtocol::getProxyPassword(const QVariantMap &parameters)
{
    return parameters.value(c_proxyPassword).toString();
}

uint MorseProtocol::getKeepAliveInterval(const QVariantMap &parameters, uint defaultValue)
{
    return parameters.value(c_keepaliveInterval, defaultValue).toUInt();
}

Tp::BaseConnectionPtr MorseProtocol::createConnection(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << Telegram::Utils::maskPhoneNumber(parameters, c_account);
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
