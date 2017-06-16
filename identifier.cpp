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

#include "identifier.hpp"

static const QLatin1String c_userPrefix = QLatin1String("user");
static const QLatin1String c_chatPrefix = QLatin1String("chat");
static const QLatin1String c_channelPrefix = QLatin1String("channel");

MorseIdentifier::MorseIdentifier(quint32 id, Telegram::Peer::Type t) :
    Telegram::Peer(id, t)
{
}

MorseIdentifier::MorseIdentifier(const Telegram::Peer &peer) :
    Telegram::Peer(peer)
{
}

bool MorseIdentifier::operator==(const MorseIdentifier &identifier) const
{
    return (type == identifier.type) && (id == identifier.id);
}

bool MorseIdentifier::operator!=(const MorseIdentifier &identifier) const
{
    return !(*this == identifier);
}

quint32 MorseIdentifier::userId() const
{
    if (type == User) {
        return id;
    }
    return 0;
}

quint32 MorseIdentifier::chatId() const
{
    if (type == Chat) {
        return id;
    }
    return 0;
}

quint32 MorseIdentifier::channelId() const
{
    if (type == Channel) {
        return id;
    }
    return 0;
}

MorseIdentifier MorseIdentifier::fromUserId(quint32 userId)
{
    return MorseIdentifier(userId, Telegram::Peer::User);
}

MorseIdentifier MorseIdentifier::fromChatId(quint32 chatId)
{
    return MorseIdentifier(chatId, Telegram::Peer::Chat);
}

MorseIdentifier MorseIdentifier::fromChannelId(quint32 channelId)
{
    return MorseIdentifier(channelId, Telegram::Peer::Channel);
}

QString MorseIdentifier::toString() const
{
    switch (type) {
    case User:
        return c_userPrefix + QString::number(id);
    case Chat:
        return c_chatPrefix + QString::number(id);
    case Channel:
        return c_channelPrefix + QString::number(id);
    default:
        break;
    }
    return QString();
}

MorseIdentifier MorseIdentifier::fromString(const QString &string)
{
    // Possible schemes: user1234, chat1234
    bool ok = true;
    if (string.startsWith(c_userPrefix)) {
        uint userId = string.midRef(c_userPrefix.size()).toUInt(&ok);
        if (ok) {
            return MorseIdentifier::fromUserId(userId);
        }
    } else if (string.startsWith(c_chatPrefix)) {
        uint chatId = string.midRef(c_chatPrefix.size()).toUInt(&ok);
        if (ok) {
            return MorseIdentifier::fromChatId(chatId);
        }
    } else if (string.startsWith(c_channelPrefix)) {
        uint channelId = string.midRef(c_channelPrefix.size()).toUInt(&ok);
        if (ok) {
            return MorseIdentifier::fromChannelId(channelId);
        }
    }

    return MorseIdentifier();
}
