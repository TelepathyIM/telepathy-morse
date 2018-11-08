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

#ifndef MORSE_PROTOCOL_HPP
#define MORSE_PROTOCOL_HPP

#include <TelepathyQt/BaseProtocol>

class MorseProtocol : public Tp::BaseProtocol
{
    Q_OBJECT
    Q_DISABLE_COPY(MorseProtocol)

public:
    MorseProtocol(const QDBusConnection &dbusConnection, const QString &name);
    virtual ~MorseProtocol();

    static QString getAccount(const QVariantMap &parameters);
    static QString getServerAddress(const QVariantMap &parameters);
    static quint16 getServerPort(const QVariantMap &parameters);
    static QString getServerKey(const QVariantMap &parameters);
    static QString getProxyType(const QVariantMap &parameters);
    static QString getProxyAddress(const QVariantMap &parameters);
    static quint16 getProxyPort(const QVariantMap &parameters);
    static QString getProxyUsername(const QVariantMap &parameters);
    static QString getProxyPassword(const QVariantMap &parameters);
    static uint getKeepAliveInterval(const QVariantMap &parameters, uint defaultValue);

private:
    Tp::BaseConnectionPtr createConnection(const QVariantMap &parameters, Tp::DBusError *error);
    QString identifyAccount(const QVariantMap &parameters, Tp::DBusError *error);
    QString normalizeContact(const QString &contactId, Tp::DBusError *error);

    // Proto.I.Addressing
    QString normalizeVCardAddress(const QString &vCardField, const QString vCardAddress,
            Tp::DBusError *error);
    QString normalizeContactUri(const QString &uri, Tp::DBusError *error);

    Tp::BaseProtocolAddressingInterfacePtr addrIface;
    Tp::BaseProtocolAvatarsInterfacePtr avatarsIface;
    Tp::BaseProtocolPresenceInterfacePtr presenceIface;

};

#endif // MORSE_PROTOCOL_HPP
