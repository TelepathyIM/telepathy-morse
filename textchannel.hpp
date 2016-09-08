/*
    Copyright (C) 2014 Alexandr Akulich <akulichalexander@gmail.com>

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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
    void whenMessageReceived(const TelegramNamespace::Message &message, uint contactHandle);
    void updateChatParticipants(const Tp::UIntList &handles);

    void whenChatDetailsChanged(quint32 chatId, const Tp::UIntList &handles);

protected slots:
    void setMessageInboxRead(TelegramNamespace::Peer peer, quint32 messageId);
    void setMessageOutboxRead(TelegramNamespace::Peer peer, quint32 messageId);
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
