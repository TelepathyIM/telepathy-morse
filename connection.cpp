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

#include "connection.hpp"

#include "textchannel.hpp"

#include <TelegramQt/CAppInformation>
#include <TelegramQt/CTelegramCore>

#include <TelepathyQt/Constants>
#include <TelepathyQt/BaseChannel>

#include <QDebug>
#include <QDateTime>

Tp::SimpleStatusSpecMap TelegramConnection::getSimpleStatusSpecMap()
{
    //Presence
    Tp::SimpleStatusSpec spAvailable;
    spAvailable.type = Tp::ConnectionPresenceTypeAvailable;
    spAvailable.maySetOnSelf = false;
    spAvailable.canHaveMessage = true;

    Tp::SimpleStatusSpec spOffline;
    spOffline.type = Tp::ConnectionPresenceTypeOffline;
    spOffline.maySetOnSelf = false;
    spOffline.canHaveMessage = false;

    Tp::SimpleStatusSpec spUnknown;
    spUnknown.type = Tp::ConnectionPresenceTypeUnknown;
    spUnknown.maySetOnSelf = false;
    spUnknown.canHaveMessage = false;

    Tp::SimpleStatusSpecMap specs;
    specs.insert(QLatin1String("available"), spAvailable);
    specs.insert(QLatin1String("offline"), spOffline);
    specs.insert(QLatin1String("unknown"), spUnknown);
    return specs;
}

TelegramConnection::TelegramConnection(const QDBusConnection &dbusConnection, const QString &cmName, const QString &protocolName, const QVariantMap &parameters) :
    Tp::BaseConnection(dbusConnection, cmName, protocolName, parameters),
    m_core(0),
    m_connectionError(0)
{
    qDebug() << Q_FUNC_INFO;
    /* Connection.Interface.Contacts */
    contactsIface = Tp::BaseConnectionContactsInterface::create();
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this, &TelegramConnection::getContactAttributes));
    contactsIface->setContactAttributeInterfaces(QStringList()
                                                 << TP_QT_IFACE_CONNECTION
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE);
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    /* Connection.Interface.SimplePresence */
    simplePresenceIface = Tp::BaseConnectionSimplePresenceInterface::create();
    simplePresenceIface->setSetPresenceCallback(Tp::memFun(this,&TelegramConnection::setPresence));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(simplePresenceIface));

    /* Connection.Interface.ContactList */
    contactListIface = Tp::BaseConnectionContactListInterface::create();
    contactListIface->setGetContactListAttributesCallback(Tp::memFun(this, &TelegramConnection::getContactListAttributes));
//    contactListIface->setRequestSubscriptionCallback(Tp::memFun(this, &SimpleConnection::requestSubscription));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactListIface));

    /* Connection.Interface.Requests */
    requestsIface = Tp::BaseConnectionRequestsInterface::create(this);
    /* Fill requestableChannelClasses */
    Tp::RequestableChannelClass text;
    text.fixedProperties[TP_QT_IFACE_CHANNEL+".ChannelType"] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    text.fixedProperties[TP_QT_IFACE_CHANNEL+".TargetHandleType"]  = Tp::HandleTypeContact;
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL+".TargetHandle");
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL+".TargetID");
    requestsIface->requestableChannelClasses << text;
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    m_selfPhone = parameters.value("account").toString();

    setSelfHandle(addContact(m_selfPhone + "@telegram"));

    setConnectCallback(Tp::memFun(this, &TelegramConnection::connect));
    setInspectHandlesCallback(Tp::memFun(this, &TelegramConnection::inspectHandles));
    setCreateChannelCallback(Tp::memFun(this, &TelegramConnection::createChannel));
    setRequestHandlesCallback(Tp::memFun(this, &TelegramConnection::requestHandles));
}

TelegramConnection::~TelegramConnection()
{
}

void TelegramConnection::connect(Tp::DBusError *error)
{
    CAppInformation appInfo;
    appInfo.setAppId(14617);
    appInfo.setAppHash(QLatin1String("e17ac360fd072f83d5d08db45ce9a121"));
    appInfo.setAppVersion(QLatin1String("0.1"));
    appInfo.setDeviceInfo(QLatin1String("pc"));
    appInfo.setOsInfo(QLatin1String("GNU/Linux"));
    appInfo.setLanguageCode(QLatin1String("en"));

    m_core = new CTelegramCore(this);
    m_core->setAppInformation(&appInfo);

    setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonRequested);

    qDebug() << "Get auth code for " << m_selfPhone;
    m_core->initialConnection("173.240.5.1", 443);

    QObject::connect(m_core, SIGNAL(dcConfigurationObtained()), this, SLOT(connectStepTwo()));

    m_connectionError = error;
}

void TelegramConnection::connectStepTwo()
{
    Tp::DBusError *error = m_connectionError;

//    m_core->requestAuthCode(m_selfPhone);

    qDebug() << "Opening registration";

    //Registration
    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION,
                                                             0, Tp::HandleTypeNone);

    Tp::BaseChannelServerAuthenticationTypePtr authType
            = Tp::BaseChannelServerAuthenticationType::create(TP_QT_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(authType));

    saslIface = Tp::BaseChannelSASLAuthenticationInterface::create(QStringList() << QLatin1String("X-TELEPATHY-PASSWORD"), false, false, QString(), QString(), QString());
    saslIface->setStartMechanismWithDataCallback( Tp::memFun(this, &TelegramConnection::startMechanismWithData));

    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(saslIface));
//    SASL_Status_Server_Succeeded

    baseChannel->registerObject(error);

    if(!error->isValid()) {
        qDebug() << "Reg success";
        addChannel(baseChannel);
    }

    //    simplePresenceIface->setStatuses(getSimpleStatusSpecMap());

    //    Tp::SimpleContactPresences presences;
    //    Tp::SimplePresence presence;
    //    presence.status = "available";
    //    presence.statusMessage = "";
    //    presence.type = Tp::ConnectionPresenceTypeAvailable;
    //    presences[selfHandle()] = presence;
    //    simplePresenceIface->setPresences(presences);

    //    setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);

    //    /* Set ContactList status */
        //    contactListIface->setContactListState(Tp::ContactListStateSuccess);
}

QStringList TelegramConnection::inspectHandles(uint handleType, const Tp::UIntList &handles, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO;

    if (handleType != Tp::HandleTypeContact) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, "Unsupported handle type");
        return QStringList();
    }

    QStringList result;

    foreach (uint handle, handles) {
        if (!m_handles.contains(handle)) {
            return QStringList();
        }

        result.append(m_handles.value(handle));
    }

    return result;
}

Tp::BaseChannelPtr TelegramConnection::createChannel(const QString &channelType, uint targetHandleType, uint targetHandle, Tp::DBusError *error)
{
    qDebug() << "SimpleConnection::createChannel " << channelType
             << " " << targetHandleType
             << " " << targetHandle;

    if ((targetHandleType != Tp::HandleTypeContact) || (targetHandle == 0)) {
          error->set(TP_QT_ERROR_INVALID_HANDLE, "createChannel error");
          return Tp::BaseChannelPtr();
    }

    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, channelType, targetHandle, targetHandleType);

    QString identifier = m_handles.value(targetHandle);

    if (channelType == TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        SimpleTextChannelPtr textType = SimpleTextChannel::create(this, baseChannel.data(), targetHandle, identifier);
        qDebug() << "Text interface is called " << textType->interfaceName();
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(textType));
    }

    return baseChannel;
}

Tp::UIntList TelegramConnection::requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << identifiers;

    Tp::UIntList result;

    if (handleType != Tp::HandleTypeContact) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, "SimpleConnection::requestHandles - Handle Type unknown");
        return result;
    }

    foreach(const QString &identify, identifiers) {
         uint handle = m_handles.key(identify, 0);
         if (handle) {
             result.append(handle);
         } else {
             result.append(addContact(identify));
         }
     }

    return result;
}

Tp::ContactAttributesMap TelegramConnection::getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO;

    Tp::ContactAttributesMap contactAttributes;

    foreach (const uint handle, m_handles.keys()) {
        if (handle == selfHandle()) {
            continue;
        }
        QVariantMap attributes;
        attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = m_handles.value(handle);
        attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = Tp::SubscriptionStateYes;
        attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = Tp::SubscriptionStateYes;
        attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")] = QVariant::fromValue(getPresence(handle));
        contactAttributes[handle] = attributes;
    }
    return contactAttributes;
}

Tp::ContactAttributesMap TelegramConnection::getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error)
{
//    Connection.Interface.Contacts
//    http://telepathy.freedesktop.org/spec/Connection_Interface_Contacts.html#Method:GetContactAttributes
    qDebug() << Q_FUNC_INFO << handles;

    Tp::ContactAttributesMap contactAttributes;

    foreach (const uint handle, handles) {
        if (m_handles.contains(handle)){
            QVariantMap attributes;
            attributes["org.freedesktop.Telepathy.Connection/contact-id"] = m_handles.value(handle);

            if (handle != selfHandle() && interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = Tp::SubscriptionStateYes;
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = Tp::SubscriptionStateYes;
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")] = QVariant::fromValue(getPresence(handle));
            }
            contactAttributes[handle] = attributes;
        }
    }
    return contactAttributes;
}

Tp::SimplePresence TelegramConnection::getPresence(uint handle)
{
    if (!m_presences.contains(handle)) {
        return Tp::SimplePresence();
    }

    return m_presences.value(handle);
}

uint TelegramConnection::setPresence(const QString &status, const QString &message, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << "not implemented";
    return 0;
}

uint TelegramConnection::ensureContact(const QString &identifier)
{
    uint handle = getHandle(identifier);
    if (!handle) {
        handle = addContact(identifier);
    }
    return handle;
}

uint TelegramConnection::addContacts(const QStringList &identifiers)
{
    qDebug() << Q_FUNC_INFO;
    uint handle = 0;

    if (!m_handles.isEmpty()) {
        handle = m_handles.keys().last();
    }

    QList<uint> newHandles;
    foreach(const QString &identifier, identifiers) {
        ++handle;
        m_handles.insert(handle, identifier);
        newHandles << handle;
    }

    setPresenceState(newHandles, "unknown");
    setSubscriptionState(identifiers, newHandles, Tp::SubscriptionStateUnknown);

    return handle;
}

uint TelegramConnection::addContact(const QString &identifier)
{
    qDebug() << Q_FUNC_INFO;
    return addContacts(QStringList() << identifier);
}

void TelegramConnection::setPresenceState(const QList<uint> &handles, const QString &status)
{
    qDebug() << Q_FUNC_INFO;
    Tp::SimpleContactPresences newPresences;
    const static Tp::SimpleStatusSpecMap statusSpecMap = getSimpleStatusSpecMap();
    foreach (uint handle, handles) {
        uint type = 0;
        if (statusSpecMap.contains(status)) {
            type = statusSpecMap.value(status).type;
        }

        Tp::SimplePresence presence;
        presence.status = status;
//        presence.statusMessage;
        presence.type = type;
        m_presences[handle] = presence;
        newPresences[handle] = presence;
    }
    simplePresenceIface->setPresences(newPresences);
}

void TelegramConnection::setSubscriptionState(const QStringList &identifiers, const QList<uint> &handles, uint state)
{
    qDebug() << Q_FUNC_INFO;
    Tp::ContactSubscriptionMap changes;
    Tp::HandleIdentifierMap identifiersMap;

    for(int i = 0; i < identifiers.size(); ++i) {
        Tp::ContactSubscriptions change;
        change.publish = Tp::SubscriptionStateYes;
        change.publishRequest = "";
        change.subscribe = state;
        changes[handles[i]] = change;
        identifiersMap[handles[i]] = identifiers[i];
        m_contactsSubscription[handles[i]] = state;
    }
    Tp::HandleIdentifierMap removals;
    contactListIface->contactsChangedWithID(changes, identifiersMap, removals);
}

void TelegramConnection::startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO;
}

/* Receive message from someone to ourself */
void TelegramConnection::receiveMessage(const QString &sender, const QString &message)
{
    uint senderHandle, targetHandle;

    Tp::HandleType handleType = Tp::HandleTypeContact;
    senderHandle = targetHandle = ensureContact(sender);

    //TODO: initiator should be group creator
    Tp::DBusError error;
    bool yours;
    Tp::BaseChannelPtr channel = ensureChannel(TP_QT_IFACE_CHANNEL_TYPE_TEXT, handleType, targetHandle, yours,
                                           senderHandle,
                                           false, &error);
    if (error.isValid()) {
        qWarning() << "ensureChannel failed:" << error.name() << " " << error.message();
        return;
    }

    Tp::BaseChannelTextTypePtr textChannel = Tp::BaseChannelTextTypePtr::dynamicCast(channel->interface(TP_QT_IFACE_CHANNEL_TYPE_TEXT));
    if (!textChannel) {
        qDebug() << "Error, channel is not a textChannel??";
        return;
    }

    uint timestamp = QDateTime::currentMSecsSinceEpoch() / 1000;

    Tp::MessagePartList body;
    Tp::MessagePart text;
    text["content-type"]            = QDBusVariant("text/plain");
    text["content"]                 = QDBusVariant(message);
    body << text;

    Tp::MessagePartList partList;
    Tp::MessagePart header;
    header["message-received"]      = QDBusVariant(timestamp);
    header["message-sender"]        = QDBusVariant(senderHandle);
    header["message-sender-id"]     = QDBusVariant(sender);
    //header["sender-nickname"]       = QDBusVariant(pushName);
    header["message-type"]          = QDBusVariant(Tp::ChannelTextMessageTypeNormal);

    partList << header << body;
    textChannel->addReceivedMessage(partList);
}

void TelegramConnection::setContactList(const QStringList &identifiers)
{
    // Actually it don't clear previous list (not implemented yet)
    addContacts(identifiers);

//    Tp::ContactSubscriptionMap changes;
//    Tp::HandleIdentifierMap identifiers;
//    Tp::HandleIdentifierMap removals;

    QList<uint> handles;

    for (int i = 0; i < identifiers.count(); ++i) {
        handles.append(ensureContact(identifiers.at(i)));
    }

    setSubscriptionState(identifiers, handles, Tp::SubscriptionStateYes);
}

void TelegramConnection::setContactPresence(const QString &identifier, const QString &presence)
{
    uint handle = ensureContact(identifier);
    setPresenceState(QList<uint>() << handle, presence);

    // Let it be here until proper subscription implementation
    if (handle != selfHandle()) {
        setSubscriptionState(QStringList() << identifier, QList<uint>() << handle, Tp::SubscriptionStateYes);
    }
}

uint TelegramConnection::getHandle(const QString &identifier) const
{
    foreach (uint key, m_handles.keys()) {
        if (m_handles.value(key) == identifier) {
            return key;
        }
    }

    return 0;
}
