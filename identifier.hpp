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

class MorseIdentifier : public Telegram::Peer
{
public:
    MorseIdentifier(quint32 id = 0, Telegram::Peer::Type t = User);
    MorseIdentifier(const Telegram::Peer &peer);

    bool operator==(const MorseIdentifier &identifier) const;
    bool operator!=(const MorseIdentifier &identifier) const;

    quint32 userId() const;
    quint32 chatId() const;
    static MorseIdentifier fromUserId(quint32 userId);
    static MorseIdentifier fromChatId(quint32 chatId);

    QString toString() const;
    static MorseIdentifier fromString(const QString &string);
};

#endif // MORSE_IDENTIFIER_HPP
