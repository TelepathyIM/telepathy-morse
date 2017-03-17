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

#ifndef MORSE_TEXTCHANNEL_HPP
#define MORSE_TEXTCHANNEL_HPP

#include <QPointer>

#include <TelepathyQt/BaseChannel>

#include <TelegramQt/CTelegramCore>

#include "identifier.hpp"

class QTimer;

class MorseTextChannel;

typedef Tp::SharedPtr<MorseTextChannel> MorseTextChannelPtr;

struct SentMessageId
{
    SentMessageId(quint64 random = 0, quint32 actualId = 0) :
        randomId(random),
        id(actualId)
    {
    }

    bool operator==(const SentMessageId &info) const
    {
        return randomId == info.randomId && id == info.id;
    }

    quint64 randomId;
    quint32 id;
};

class MorseTextChannel : public Tp::BaseChannelTextType
{
    Q_OBJECT
public:
    static MorseTextChannelPtr create(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID);
    virtual ~MorseTextChannel();

    QString sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error);

    void messageAcknowledgedCallback(const QString &messageId);

public slots:
    void whenContactChatStateComposingChanged(quint32 userId, TelegramNamespace::MessageAction action);
    void whenContactRoomStateComposingChanged(quint32 chatId, quint32 userId, TelegramNamespace::MessageAction action);
    void setMessageAction(const MorseIdentifier &identifier, TelegramNamespace::MessageAction action);
    void whenMessageReceived(const Telegram::Message &message, uint contactHandle);
    void updateChatParticipants(const Tp::UIntList &handles);

    void whenChatDetailsChanged(quint32 chatId, const Tp::UIntList &handles);

protected slots:
    void setMessageInboxRead(Telegram::Peer peer, quint32 messageId);
    void setMessageOutboxRead(Telegram::Peer peer, quint32 messageId);
    void setResolvedMessageId(quint64 randomId, quint32 resolvedId);
    void reactivateLocalTyping();

protected:
    void setChatState(uint state, Tp::DBusError *error);

private:
    MorseTextChannel(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID);

    QPointer<CTelegramCore> m_core;

    uint m_targetHandle;
    uint m_targetHandleType;
    uint m_selfHandle;
    MorseIdentifier m_targetID;
    MorseIdentifier m_selfID;

    Tp::BaseChannelTextTypePtr m_channelTextType;
    Tp::BaseChannelMessagesInterfacePtr m_messagesIface;
    Tp::BaseChannelChatStateInterfacePtr m_chatStateIface;
    Tp::BaseChannelGroupInterfacePtr m_groupIface;
    Tp::BaseChannelRoomInterfacePtr m_roomIface;
    Tp::BaseChannelRoomConfigInterfacePtr m_roomConfigIface;

    QVector<SentMessageId> m_sentMessageIds;
    QTimer *m_localTypingTimer;

};

#endif // MORSE_TEXTCHANNEL_HPP
