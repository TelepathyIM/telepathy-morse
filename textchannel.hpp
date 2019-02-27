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

#include <TelegramQt/TelegramNamespace>

#include <TelepathyQt/BaseChannel>

class QTimer;

class CTelegramCore;

class MorseTextChannel;
class MorseConnection;

namespace Telegram {

namespace Client {

class Client;
class MessagingApi;

} // Client namespace

} // Telegram namespace

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
    static MorseTextChannelPtr create(MorseConnection *morseConnection, Tp::BaseChannel *baseChannel);
    virtual ~MorseTextChannel();

    QString sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error);

    void messageAcknowledgedCallback(const QString &messageId);

public slots:
    void onMessageActionChanged(const Telegram::Peer &peer, quint32 userId, TelegramNamespace::MessageAction action);
    void setMessageAction(quint32 userId, TelegramNamespace::MessageAction action);
    void onMessageReceived(const Telegram::Message &message);
    void updateChatParticipants(const Tp::UIntList &handles);

    void onChatDetailsChanged(quint32 chatId, const Tp::UIntList &handles);

protected slots:
    void setMessageInboxRead(Telegram::Peer peer, quint32 messageId);
    void setMessageOutboxRead(Telegram::Peer peer, quint32 messageId);
    void setResolvedMessageId(Telegram::Peer peer, quint64 messageRandomId, quint32 messageId);
    void reactivateLocalTyping();

protected:
    void setChatState(uint state, Tp::DBusError *error);

private:
    MorseTextChannel(MorseConnection *morseConnection, Tp::BaseChannel *baseChannel);

    MorseConnection *m_connection;
    Telegram::Client::Client *m_client;
    Telegram::Client::MessagingApi *m_api = nullptr;

    uint m_targetHandle;
    uint m_targetHandleType;
    uint m_selfHandle;
    Telegram::Peer m_targetPeer;
    Telegram::Peer m_selfPeer;

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
