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

#ifndef MORSE_IDENTIFIER_HPP
#define MORSE_IDENTIFIER_HPP

#include <TelegramQt/TelegramNamespace>

class MorseIdentifier
{
public:
    MorseIdentifier();
    MorseIdentifier(const QString &string);

    bool operator==(const MorseIdentifier &identifier) const;
    bool operator!=(const MorseIdentifier &identifier) const;

    bool isNull() const { return !m_userId && !m_chatId; }

    quint32 userId() const { return m_userId; }
    quint32 chatId() const { return m_chatId; }
    Telegram::Peer toPeer() const;
    static MorseIdentifier fromPeer(const Telegram::Peer &peer);
    static MorseIdentifier fromChatId(quint32 chatId);
    static MorseIdentifier fromUserId(quint32 userId);
    static MorseIdentifier fromUserInChatId(quint32 chatId, quint32 userId);

    QString toString() const;
    static MorseIdentifier fromString(const QString &string);

private:
    quint32 m_userId;
    quint32 m_chatId;
};

#endif // MORSE_IDENTIFIER_HPP
