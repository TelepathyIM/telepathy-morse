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

#include "textchannel.hpp"

#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>
#include <QDateTime>
#include <QTimer>

MorseTextChannel::MorseTextChannel(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID)
    : Tp::BaseChannelTextType(baseChannel),
      m_targetHandle(baseChannel->targetHandle()),
      m_targetHandleType(baseChannel->targetHandleType()),
      m_selfHandle(selfHandle),
      m_targetID(MorseIdentifier::fromString(baseChannel->targetID())),
      m_selfID(MorseIdentifier::fromString(selfID)),
      m_localTypingTimer(0)
{
    m_core = core;

    QStringList supportedContentTypes = QStringList()
            << QLatin1String("text/plain")
            << QLatin1String("application/geo+json")
               ;
    Tp::UIntList messageTypes = Tp::UIntList() << Tp::ChannelTextMessageTypeNormal << Tp::ChannelTextMessageTypeDeliveryReport;

    uint messagePartSupportFlags = 0;
    uint deliveryReportingSupport = Tp::DeliveryReportingSupportFlagReceiveSuccesses|Tp::DeliveryReportingSupportFlagReceiveRead;

    setMessageAcknowledgedCallback(Tp::memFun(this, &MorseTextChannel::messageAcknowledgedCallback));

    m_messagesIface = Tp::BaseChannelMessagesInterface::create(this,
                                                               supportedContentTypes,
                                                               messageTypes,
                                                               messagePartSupportFlags,
                                                               deliveryReportingSupport);

    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_messagesIface));
    m_messagesIface->setSendMessageCallback(Tp::memFun(this, &MorseTextChannel::sendMessageCallback));

    m_chatStateIface = Tp::BaseChannelChatStateInterface::create();
    m_chatStateIface->setSetChatStateCallback(Tp::memFun(this, &MorseTextChannel::setChatState));
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_chatStateIface));

    if (m_targetHandleType == Tp::HandleTypeContact) {
        connect(m_core.data(), SIGNAL(contactMessageActionChanged(quint32,TelegramNamespace::MessageAction)),
                SLOT(whenContactChatStateComposingChanged(quint32,TelegramNamespace::MessageAction)));
    } else if (m_targetHandleType == Tp::HandleTypeRoom) {
        Tp::ChannelGroupFlags groupFlags = Tp::ChannelGroupFlagProperties;

        // Permissions:
        groupFlags |= Tp::ChannelGroupFlagCanAdd;

        m_groupIface = Tp::BaseChannelGroupInterface::create();
        m_groupIface->setGroupFlags(groupFlags);
        m_groupIface->setSelfHandle(m_selfHandle);
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_groupIface));

        TelegramNamespace::GroupChat info;

        m_core->getChatInfo(&info, m_targetID.chatId());

        QDateTime creationTimestamp;
        if (info.date) {
            creationTimestamp.setTime_t(info.date);
        }

        m_roomIface = Tp::BaseChannelRoomInterface::create(/* roomName */ m_targetID.toString(),
                                                           /* server */ QString(),
                                                           /* creator */ QString(),
                                                           /* creatorHandle */ 0,
                                                           creationTimestamp);

        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomIface));

        m_roomConfigIface = Tp::BaseChannelRoomConfigInterface::create();
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomConfigIface));
        connect(m_core.data(), SIGNAL(contactChatMessageActionChanged(quint32,quint32,TelegramNamespace::MessageAction)),
                SLOT(whenContactRoomStateComposingChanged(quint32,quint32,TelegramNamespace::MessageAction)));
    }

    connect(m_core.data(), SIGNAL(messageReadInbox(TelegramNamespace::Peer,quint32)),
            SLOT(setMessageInboxRead(TelegramNamespace::Peer,quint32)));
    connect(m_core.data(), SIGNAL(sentMessageIdReceived(quint64,quint32)),
            SLOT(setResolvedMessageId(quint64,quint32)));
}

MorseTextChannelPtr MorseTextChannel::create(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID)
{
    return MorseTextChannelPtr(new MorseTextChannel(core, baseChannel, selfHandle, selfID));
}

MorseTextChannel::~MorseTextChannel()
{
}

QString MorseTextChannel::sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error)
{
    QString content;
    foreach (const Tp::MessagePart &part, messageParts) {
        if (part.contains(QLatin1String("content-type"))
                && part.value(QLatin1String("content-type")).variant().toString() == QLatin1String("text/plain")
                && part.contains(QLatin1String("content"))) {
            content = part.value(QLatin1String("content")).variant().toString();
            break;
        }
    }

    quint64 tmpId = m_core->sendMessage(m_targetID.toPeer(), content);
    m_sentMessageIds.append(SentMessageId(tmpId));

    return QString::number(tmpId);
}

void MorseTextChannel::messageAcknowledgedCallback(const QString &messageId)
{
    m_core->setMessageRead(m_targetID.toPeer(), messageId.toUInt());
}

void MorseTextChannel::whenContactChatStateComposingChanged(quint32 userId, TelegramNamespace::MessageAction action)
{
    setMessageAction(MorseIdentifier::fromUserId(userId), action);
}

void MorseTextChannel::whenContactRoomStateComposingChanged(quint32 chatId, quint32 userId, TelegramNamespace::MessageAction action)
{
    setMessageAction(MorseIdentifier::fromUserInChatId(chatId, userId), action);
}

void MorseTextChannel::setMessageAction(const MorseIdentifier &identifier, TelegramNamespace::MessageAction action)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (identifier != m_targetID) {
        return;
    }

    if (action) {
        m_chatStateIface->chatStateChanged(m_targetHandle, Tp::ChannelChatStateComposing);
    } else {
        m_chatStateIface->chatStateChanged(m_targetHandle, Tp::ChannelChatStateActive);
    }
}

void MorseTextChannel::whenMessageReceived(const TelegramNamespace::Message &message, uint contactHandle)
{
    Tp::MessagePartList body;
    Tp::MessagePart text;
    text[QLatin1String("content-type")] = QDBusVariant(QLatin1String("text/plain"));
    text[QLatin1String("content")] = QDBusVariant(message.text);

    if (message.type != TelegramNamespace::MessageTypeText) { // More, than a plain text message
        TelegramNamespace::MessageMediaInfo info;
        m_core->getMessageMediaInfo(&info, message.id);

        Tp::MessagePart textMessage;
        textMessage[QLatin1String("content-type")] = QDBusVariant(QLatin1String("text/plain"));
        textMessage[QLatin1String("alternative")] = QDBusVariant(QLatin1String("multimedia"));

        if (info.alt().isEmpty()) {
            textMessage[QLatin1String("content")] = QDBusVariant(tr("Telepathy-Morse doesn't support multimedia messages yet."));
        } else {
            textMessage[QLatin1String("content")] = QDBusVariant(info.alt());
        }

        switch (message.type) {
        case TelegramNamespace::MessageTypeGeo: {
            static const QString jsonTemplate = QLatin1String("{\"type\":\"point\",\"coordinates\":\[%1, %2]}");
            Tp::MessagePart geo;
            geo[QLatin1String("content-type")] = QDBusVariant(QLatin1String("application/geo+json"));
            geo[QLatin1String("alternative")] = QDBusVariant(QLatin1String("multimedia"));
            geo[QLatin1String("content")] = QDBusVariant(jsonTemplate.arg(info.latitude()).arg(info.longitude()));
            body << geo;
        }
            break;
        default:
            break;
        }
        body << textMessage;
    }

    body << text;

    Tp::MessagePartList partList;
    Tp::MessagePart header;

    const MorseIdentifier contactID = MorseIdentifier::fromUserId(message.fromId);
    const QString token = QString::number(message.id);
    header[QLatin1String("message-token")] = QDBusVariant(token);
    header[QLatin1String("message-type")]  = QDBusVariant(Tp::ChannelTextMessageTypeNormal);
    header[QLatin1String("message-sent")]  = QDBusVariant(message.timestamp);

    if (message.flags & TelegramNamespace::MessageFlagOut) {
        header[QLatin1String("message-sender")]    = QDBusVariant(m_selfHandle);
        header[QLatin1String("message-sender-id")] = QDBusVariant(m_selfID.toString());
    } else {
        header[QLatin1String("message-sender")]    = QDBusVariant(contactHandle);
        header[QLatin1String("message-sender-id")] = QDBusVariant(contactID.toString());
    }

    // messageReceived signal is always emitted before maxMessageId update, so
    // the message is a new one, if its id is bigger, than the last known message id,
    // This works for both, In and Out messages.
    const bool scrollback = message.id <= m_core->maxMessageId();

    if (scrollback) {
        header[QLatin1String("scrollback")] = QDBusVariant(true);
        // Telegram has no timestamp for message read, only sent.
        // Fallback to the message sent timestamp to keep received messages in chronological order.
        // Alternatively, client can sort messages in order of message-sent.
        header[QLatin1String("message-received")]  = QDBusVariant(message.timestamp);
    } else {
        uint currentTimestamp = QDateTime::currentMSecsSinceEpoch() / 1000;
        header[QLatin1String("message-received")]  = QDBusVariant(currentTimestamp);
    }
    partList << header << body;
    addReceivedMessage(partList);
}

void MorseTextChannel::updateChatParticipants(const Tp::UIntList &handles)
{
    m_groupIface->setMembers(handles, /* details */ QVariantMap());
}

void MorseTextChannel::whenChatDetailsChanged(quint32 chatId, const Tp::UIntList &handles)
{
    qDebug() << Q_FUNC_INFO << chatId;

    if (m_targetID.chatId() == chatId) {
        updateChatParticipants(handles);

        TelegramNamespace::GroupChat info;
        if (m_core->getChatInfo(&info, chatId)) {
            m_roomConfigIface->setTitle(info.title);
            m_roomConfigIface->setConfigurationRetrieved(true);
        }
    }
}

void MorseTextChannel::setMessageInboxRead(TelegramNamespace::Peer peer, quint32 messageId)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (MorseIdentifier::fromPeer(peer) != m_targetID) {
        return;
    }

    // TODO: Mark *all* messages up to this as read
    QStringList tokens;

    foreach (const Tp::MessagePartList &message, pendingMessages()) {
        if (message.isEmpty()) {
            // Invalid message
            continue;
        }
        const Tp::MessagePart &header = message.front();
        bool ok;
        const QString token = header.value(QLatin1String("message-token")).variant().toString();
        quint32 mId = token.toUInt(&ok);
        if (!ok) {
            // Invalid message token
            continue;
        }

        if (mId <= messageId) {
            tokens.append(token);
        }
    }

#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 8)
    Tp::DBusError error;
    acknowledgePendingMessages(tokens, &error);
#endif
}

void MorseTextChannel::setMessageOutboxRead(TelegramNamespace::Peer peer, quint32 messageId)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (MorseIdentifier::fromPeer(peer) != m_targetID) {
        return;
    }

    quint64 id = messageId;
    foreach (const SentMessageId &info, m_sentMessageIds) {
        if (info.id == messageId) {
            id = info.randomId;
            break;
        }
    }

    // TODO: Mark *all* messages up to this as read

    const QString token = QString::number(id);

    Tp::MessagePartList partList;

    Tp::MessagePart header;
    header[QLatin1String("message-sender")]    = QDBusVariant(m_selfHandle);
    header[QLatin1String("message-sender-id")] = QDBusVariant(m_selfID.toString());
    header[QLatin1String("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeDeliveryReport);
    header[QLatin1String("delivery-status")]   = QDBusVariant(Tp::DeliveryStatusRead);
    header[QLatin1String("delivery-token")]    = QDBusVariant(token);
    partList << header;

    addReceivedMessage(partList);
}

void MorseTextChannel::setResolvedMessageId(quint64 randomId, quint32 resolvedId)
{
    int index = m_sentMessageIds.indexOf(SentMessageId(randomId));
    if (index < 0) {
        return;
    }

    m_sentMessageIds[index].id = resolvedId;

    const QString token = QString::number(randomId);

    Tp::MessagePartList partList;

    Tp::MessagePart header;
    header[QLatin1String("message-sender")]    = QDBusVariant(m_targetHandle);
    header[QLatin1String("message-sender-id")] = QDBusVariant(m_targetID.toString());
    header[QLatin1String("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeDeliveryReport);
    header[QLatin1String("delivery-status")]   = QDBusVariant(Tp::DeliveryStatusAccepted);
    header[QLatin1String("delivery-token")]    = QDBusVariant(token);
    partList << header;

    addReceivedMessage(partList);
}

void MorseTextChannel::reactivateLocalTyping()
{
    m_core->setTyping(m_targetID.toPeer(), TelegramNamespace::MessageActionTyping);
}

void MorseTextChannel::setChatState(uint state, Tp::DBusError *error)
{
    Q_UNUSED(error);

    if (!m_localTypingTimer) {
        m_localTypingTimer = new QTimer(this);
        m_localTypingTimer->setInterval(CTelegramCore::localTypingRecommendedRepeatInterval());
        connect(m_localTypingTimer, SIGNAL(timeout()), this, SLOT(reactivateLocalTyping()));
    }

    if (state == Tp::ChannelChatStateComposing) {
        m_core->setTyping(m_targetID.toPeer(), TelegramNamespace::MessageActionTyping);
        m_localTypingTimer->start();
    } else {
        m_core->setTyping(m_targetID.toPeer(), TelegramNamespace::MessageActionNone);
        m_localTypingTimer->stop();
    }
}
