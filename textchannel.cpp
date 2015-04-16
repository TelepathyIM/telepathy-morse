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

#include "textchannel.hpp"

#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>
#include <QDateTime>
#include <QTimer>

MorseTextChannel::MorseTextChannel(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint targetHandle, const QString &phone, uint selfHandle, const QString &selfPhone)
    : Tp::BaseChannelTextType(baseChannel),
      m_phone(phone),
      m_contactHandle(targetHandle),
      m_selfPhone(selfPhone),
      m_selfHandle(selfHandle),
      m_localTypingTimer(0)
{
    m_core = core;

    QStringList supportedContentTypes = QStringList() << QLatin1String("text/plain");
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

    connect(m_core.data(), SIGNAL(contactTypingStatusChanged(QString,bool)), SLOT(whenContactChatStateComposingChanged(QString,bool)));
    connect(m_core.data(), SIGNAL(sentMessageStatusChanged(QString,quint64,TelegramNamespace::MessageDeliveryStatus)),
            SLOT(sentMessageDeliveryStatusChanged(QString,quint64,TelegramNamespace::MessageDeliveryStatus)));
}

MorseTextChannelPtr MorseTextChannel::create(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint targetHandle, const QString &phone, uint selfHandle, const QString &selfPhone)
{
    return MorseTextChannelPtr(new MorseTextChannel(core, baseChannel, targetHandle, phone, selfHandle, selfPhone));
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

    return QString::number(m_core->sendMessage(m_phone, content));
}

void MorseTextChannel::messageAcknowledgedCallback(const QString &messageId)
{
    m_core->setMessageRead(m_phone, messageId.toUInt());
}

void MorseTextChannel::whenContactChatStateComposingChanged(const QString &phone, bool composing)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (phone != m_phone) {
        return;
    }

    if (composing) {
        m_chatStateIface->chatStateChanged(m_contactHandle, Tp::ChannelChatStateComposing);
    } else {
        m_chatStateIface->chatStateChanged(m_contactHandle, Tp::ChannelChatStateActive);
    }
}

void MorseTextChannel::whenMessageReceived(const QString &message, quint32 messageId, quint32 flags, uint timestamp)
{
    Tp::MessagePartList body;
    Tp::MessagePart text;
    text[QLatin1String("content-type")] = QDBusVariant(QLatin1String("text/plain"));
    text[QLatin1String("content")]      = QDBusVariant(message);
    body << text;

    Tp::MessagePartList partList;
    Tp::MessagePart header;

    const QString token = QString::number(messageId);
    header[QLatin1String("message-token")] = QDBusVariant(token);
    header[QLatin1String("message-type")]  = QDBusVariant(Tp::ChannelTextMessageTypeNormal);
    header[QLatin1String("message-sent")]  = QDBusVariant(timestamp);

    if (flags & TelegramNamespace::MessageFlagOut) {
        header[QLatin1String("message-sender")]    = QDBusVariant(m_selfHandle);
        header[QLatin1String("message-sender-id")] = QDBusVariant(m_selfPhone);
        partList << header << body;
        m_messagesIface->messageSent(partList, 0, token);
    } else {
        uint currentTimestamp = QDateTime::currentMSecsSinceEpoch() / 1000;

        header[QLatin1String("message-received")]  = QDBusVariant(currentTimestamp);
        header[QLatin1String("message-sender")]    = QDBusVariant(m_contactHandle);
        header[QLatin1String("message-sender-id")] = QDBusVariant(m_phone);
        partList << header << body;
        addReceivedMessage(partList);
    }
}

void MorseTextChannel::sentMessageDeliveryStatusChanged(const QString &phone, quint64 messageId, TelegramNamespace::MessageDeliveryStatus status)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (phone != m_phone) {
        return;
    }

    Tp::DeliveryStatus statusFlag;

    switch (status) {
    case TelegramNamespace::MessageDeliveryStatusSent:
        statusFlag = Tp::DeliveryStatusAccepted;
        break;
    case TelegramNamespace::MessageDeliveryStatusRead:
        statusFlag = Tp::DeliveryStatusRead;
        break;
    default:
        return;
    }

    const QString token = QString::number(messageId);

    Tp::MessagePartList partList;

    Tp::MessagePart header;
    header[QLatin1String("message-token")]     = QDBusVariant(token);
    header[QLatin1String("message-sender")]    = QDBusVariant(m_contactHandle);
    header[QLatin1String("message-sender-id")] = QDBusVariant(m_phone);
    header[QLatin1String("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeDeliveryReport);
    header[QLatin1String("delivery-status")]   = QDBusVariant(statusFlag);
    header[QLatin1String("delivery-token")]    = QDBusVariant(token);
    partList << header;

    uint flags = 0;

    // MessageSendingFlagReportDelivery for DeliveryStatusAccepted?
    if (statusFlag == Tp::DeliveryStatusRead) {
        flags = Tp::MessageSendingFlagReportRead;
    }

    m_messagesIface->messageSent(partList, flags, token);
}

void MorseTextChannel::reactivateLocalTyping()
{
    m_core->setTyping(m_phone, true);
}

void MorseTextChannel::setChatState(uint state, Tp::DBusError *error)
{
    Q_UNUSED(error);

    if (!m_localTypingTimer) {
        m_localTypingTimer = new QTimer(this);
        m_localTypingTimer->setInterval(CTelegramCore::localTypingRecommendedRepeatInterval());
        connect(m_localTypingTimer, SIGNAL(timeout()), this, SLOT(reactivateLocalTyping()));
    }

    m_core->setTyping(m_phone, state == Tp::ChannelChatStateComposing);

    if (state == Tp::ChannelChatStateComposing) {
        m_localTypingTimer->start();
    } else {
        m_localTypingTimer->stop();
    }
}
