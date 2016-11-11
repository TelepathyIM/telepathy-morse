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

#ifndef MORSE_CONTACTSEARCHCHANNEL_HPP
#define MORSE_CONTACTSEARCHCHANNEL_HPP

#include <TelepathyQt/BaseChannel>

#include <QVariantMap>

#include <TelegramQt/TelegramNamespace>

class MorseContactSearchChannel;

class CTelegramCore;

typedef Tp::SharedPtr<MorseContactSearchChannel> MorseContactSearchChannelPtr;

class MorseContactSearchChannel : public Tp::BaseChannelContactSearchType
{
    Q_OBJECT
public:
    static MorseContactSearchChannelPtr create(CTelegramCore *core, const QVariantMap &request);

protected:
    MorseContactSearchChannel(CTelegramCore *core, uint limit);

    void contactSearch(const Tp::ContactSearchMap &terms, Tp::DBusError *error);

protected slots:
    void onSearchComplete(const QString &query, const QVector<TelegramNamespace::Peer> &peers);

protected:
    CTelegramCore *m_core;
    QString m_query;
};

#endif // MORSE_CONTACTSEARCHCHANNEL_HPP
