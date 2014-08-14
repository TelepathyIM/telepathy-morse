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

MorseTextChannel::MorseTextChannel(CTelegramCore *core, QObject *connection, Tp::BaseChannel *baseChannel, uint targetHandle, const QString &phone)
    : Tp::BaseChannelTextType(baseChannel),
      m_connection(connection),
      m_phone(phone),
      m_contactHandle(targetHandle)
{
    m_core = core;

    QStringList supportedContentTypes = QStringList() << QLatin1String("text/plain");
    Tp::UIntList messageTypes = Tp::UIntList() << Tp::ChannelTextMessageTypeNormal;

    uint messagePartSupportFlags = 0;
    uint deliveryReportingSupport = 0;

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
}

MorseTextChannelPtr MorseTextChannel::create(CTelegramCore *core, QObject *connection, Tp::BaseChannel *baseChannel, uint targetHandle, const QString &phone)
{
    return MorseTextChannelPtr(new MorseTextChannel(core, connection, baseChannel, targetHandle, phone));
}

MorseTextChannel::~MorseTextChannel()
{
}

QString MorseTextChannel::sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error)
{
    QString content;
    for (Tp::MessagePartList::const_iterator i = messageParts.begin()+1; i != messageParts.end(); ++i) {
        if(i->count(QLatin1String("content-type"))
            && i->value(QLatin1String("content-type")).variant().toString() == QLatin1String("text/plain")
            && i->count(QLatin1String("content")))
        {
            content = i->value(QLatin1String("content")).variant().toString();
            break;
        }
    }

    return QString::number(m_core->sendMessage(m_phone, content));
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

void MorseTextChannel::setChatState(uint state, Tp::DBusError *error)
{
    Q_UNUSED(error);

    m_core->setTyping(m_phone, state == Tp::ChannelChatStateComposing);
}
