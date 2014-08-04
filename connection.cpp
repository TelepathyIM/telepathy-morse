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

//#define SIMULATION

#define INSECURE_SAVE

#ifdef INSECURE_SAVE
#include <QDir>
#include <QFile>

static const QString secretsDirPath = QLatin1String("morse-secrets");
#endif

Tp::SimpleStatusSpecMap MorseConnection::getSimpleStatusSpecMap()
{
    //Presence
    Tp::SimpleStatusSpec spOffline;
    spOffline.type = Tp::ConnectionPresenceTypeOffline;
    spOffline.maySetOnSelf = true;
    spOffline.canHaveMessage = false;

    Tp::SimpleStatusSpec spAvailable;
    spAvailable.type = Tp::ConnectionPresenceTypeAvailable;
    spAvailable.maySetOnSelf = true;
    spAvailable.canHaveMessage = true;

    Tp::SimpleStatusSpec spHidden;
    spHidden.type = Tp::ConnectionPresenceTypeHidden;
    spHidden.maySetOnSelf = true;
    spHidden.canHaveMessage = true;

    Tp::SimpleStatusSpec spUnknown;
    spUnknown.type = Tp::ConnectionPresenceTypeUnknown;
    spUnknown.maySetOnSelf = false;
    spUnknown.canHaveMessage = false;

    Tp::SimpleStatusSpecMap specs;
    specs.insert(QLatin1String("offline"), spOffline);
    specs.insert(QLatin1String("available"), spAvailable);
    specs.insert(QLatin1String("hidden"), spHidden);
    specs.insert(QLatin1String("unknown"), spUnknown);
    return specs;
}

MorseConnection::MorseConnection(const QDBusConnection &dbusConnection, const QString &cmName, const QString &protocolName, const QVariantMap &parameters) :
    Tp::BaseConnection(dbusConnection, cmName, protocolName, parameters),
    m_core(0)
{
    qDebug() << Q_FUNC_INFO;
    /* Connection.Interface.Contacts */
    contactsIface = Tp::BaseConnectionContactsInterface::create();
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this, &MorseConnection::getContactAttributes));
    contactsIface->setContactAttributeInterfaces(QStringList()
                                                 << TP_QT_IFACE_CONNECTION
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE);
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    /* Connection.Interface.SimplePresence */
    simplePresenceIface = Tp::BaseConnectionSimplePresenceInterface::create();
    simplePresenceIface->setSetPresenceCallback(Tp::memFun(this,&MorseConnection::setPresence));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(simplePresenceIface));

    /* Connection.Interface.ContactList */
    contactListIface = Tp::BaseConnectionContactListInterface::create();
    contactListIface->setGetContactListAttributesCallback(Tp::memFun(this, &MorseConnection::getContactListAttributes));
    contactListIface->setRequestSubscriptionCallback(Tp::memFun(this, &MorseConnection::requestSubscription));
    contactListIface->setRemoveContactsCallback(Tp::memFun(this, &MorseConnection::removeContacts));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactListIface));

    /* Connection.Interface.Requests */
    requestsIface = Tp::BaseConnectionRequestsInterface::create(this);
    /* Fill requestableChannelClasses */
    Tp::RequestableChannelClass text;
    text.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    text.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeContact;
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    requestsIface->requestableChannelClasses << text;
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    m_selfPhone = parameters.value(QLatin1String("account")).toString();

    setSelfHandle(addContact(m_selfPhone));

    setConnectCallback(Tp::memFun(this, &MorseConnection::doConnect));
    setInspectHandlesCallback(Tp::memFun(this, &MorseConnection::inspectHandles));
    setCreateChannelCallback(Tp::memFun(this, &MorseConnection::createChannel));
    setRequestHandlesCallback(Tp::memFun(this, &MorseConnection::requestHandles));

    connect(this, SIGNAL(disconnected()), SLOT(whenDisconnected()));
}

MorseConnection::~MorseConnection()
{
}

void MorseConnection::doConnect(Tp::DBusError *error)
{
    Q_UNUSED(error);

#ifdef SIMULATION
    return whenPhoneCodeRequired();
#endif

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

    connect(m_core, SIGNAL(authenticated()), this, SLOT(connectSuccess()));

    const QByteArray sessionData = getSessionData(m_selfPhone);

    if (sessionData.isEmpty()) {
        connect(m_core, SIGNAL(connected()), this, SLOT(connectStepTwo()));
        connect(m_core, SIGNAL(phoneCodeRequired()), this, SLOT(whenPhoneCodeRequired()));
        connect(m_core, SIGNAL(phoneCodeIsInvalid()), this, SLOT(whenPhoneCodeIsInvalid()));

        m_core->initConnection(QLatin1String("173.240.5.1"), 443);
    } else {
        m_core->restoreConnection(sessionData);
    }
}

void MorseConnection::connectStepTwo()
{
    // Not actually correct. There will be attempt to restore session, which will fall-back to phone code request.
    m_core->requestPhoneCode(m_selfPhone);
}

void MorseConnection::whenPhoneCodeRequired()
{
    Tp::DBusError error;

    //Registration
    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION,
                                                             0, Tp::HandleTypeNone);

    Tp::BaseChannelServerAuthenticationTypePtr authType
            = Tp::BaseChannelServerAuthenticationType::create(TP_QT_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(authType));

    saslIface = Tp::BaseChannelSASLAuthenticationInterface::create(QStringList() << QLatin1String("X-TELEPATHY-PASSWORD"), false, true, QString(), QString(), QString(), /* maySaveResponse */ false);
    saslIface->setStartMechanismWithDataCallback( Tp::memFun(this, &MorseConnection::startMechanismWithData));

//    baseChannel->setUniqueName(QLatin1String("ServerSASLChannel")); // Needs new telepathy-qt version
    baseChannel->setRequested(false);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(saslIface));

    baseChannel->registerObject(&error);

    if (!error.isValid()) {
        addChannel(baseChannel);
    }
}

void MorseConnection::whenPhoneCodeIsInvalid()
{
    saslIface->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, QVariantMap());
}

void MorseConnection::startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << mechanism << data;

    saslIface->setSaslStatus(Tp::SASLStatusInProgress, QLatin1String("InProgress"), QVariantMap());

#ifdef SIMULATION
    static bool failThisTime = true;

    if (failThisTime) {
        whenPhoneCodeIsInvalid();
        failThisTime = false;
    } else {
        connectSuccess();
    }

    return;
#endif

    m_core->signIn(m_selfPhone, QString::fromAscii(data.constData()));
}

Tp::ContactInfoMap MorseConnection::getContactInfo(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << contacts;

    Tp::ContactInfoMap result;

    Tp::ContactInfoFieldList contactInfo;

    Tp::ContactInfoField contactInfoField;
    contactInfoField.fieldName = QLatin1String("fn");
    contactInfoField.fieldValue.append(QLatin1String("first last"));

    contactInfo.append(contactInfoField);

    result.insert(contacts.first(), contactInfo);

    return result;
}

void MorseConnection::connectSuccess()
{
    if (!saslIface.isNull()) {
        saslIface->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    saveSessionData(m_selfPhone, m_core->connectionSecretInfo());

    simplePresenceIface->setStatuses(getSimpleStatusSpecMap());

    Tp::SimpleContactPresences presences;
    Tp::SimplePresence presence;
    presence.status = QLatin1String("available");
    presence.statusMessage = QString();
    presence.type = Tp::ConnectionPresenceTypeAvailable;
    presences[selfHandle()] = presence;
    simplePresenceIface->setPresences(presences);

    setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);

#ifdef SIMULATION
    QTimer::singleShot(500, this, SLOT(whenGotContactList()));
    return;
#endif

    connect(m_core, SIGNAL(contactListChanged()), SLOT(whenContactListChanged()));
    connect(m_core, SIGNAL(messageReceived(QString,QString)), SLOT(receiveMessage(QString,QString)));
    connect(m_core, SIGNAL(contactStatusChanged(QString,TelegramNamespace::ContactStatus)), SLOT(updateContactPresence(QString)));

    contactListIface->setContactListState(Tp::ContactListStateWaiting);
    m_core->requestContactList();
}

QStringList MorseConnection::inspectHandles(uint handleType, const Tp::UIntList &handles, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO;

    if ((!m_core) || (!m_core->isAuthenticated())) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
        return QStringList();
    }

    if (handleType != Tp::HandleTypeContact) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Unsupported handle type"));
        return QStringList();
    }

    QStringList result;

    foreach (uint handle, handles) {
        if (!m_handles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Unknown handle"));
            return QStringList();
        }

        result.append(m_handles.value(handle));
    }

    return result;
}

Tp::BaseChannelPtr MorseConnection::createChannel(const QString &channelType, uint targetHandleType, uint targetHandle, Tp::DBusError *error)
{
    qDebug() << "MorseConnection::createChannel " << channelType
             << " " << targetHandleType
             << " " << targetHandle;

    if ((targetHandleType != Tp::HandleTypeContact) || (targetHandle == 0)) {
          error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("createChannel error"));
          return Tp::BaseChannelPtr();
    }

    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, channelType, targetHandle, targetHandleType);

    QString identifier = m_handles.value(targetHandle);

    if (channelType == TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        MorseTextChannelPtr textChannel = MorseTextChannel::create(this, baseChannel.data(), targetHandle, identifier);
        qDebug() << "Text interface is called " << textChannel->interfaceName();
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(textChannel));

        connect(textChannel.data(), SIGNAL(sendMessage(QString,QString)), SLOT(sendMessage(QString,QString)));
    }

    return baseChannel;
}

Tp::UIntList MorseConnection::requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << identifiers;

    Tp::UIntList result;

    if (handleType != Tp::HandleTypeContact) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("MorseConnection::requestHandles - Handle Type unknown"));
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

Tp::ContactAttributesMap MorseConnection::getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << interfaces;

    Tp::ContactAttributesMap contactAttributes;

    foreach (const uint handle, m_handles.keys()) {
        if (handle == selfHandle()) {
            continue;
        }
        QVariantMap attributes;
        attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = m_handles.value(handle);

        if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST)) {
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = Tp::SubscriptionStateYes;
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = Tp::SubscriptionStateYes;
        }

        if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE)) {
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")] = QVariant::fromValue(getPresence(handle));
        }
        contactAttributes[handle] = attributes;
    }
    return contactAttributes;
}

Tp::ContactAttributesMap MorseConnection::getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error)
{
//    http://telepathy.freedesktop.org/spec/Connection_Interface_Contacts.html#Method:GetContactAttributes
    qDebug() << Q_FUNC_INFO << handles << interfaces;

    Tp::ContactAttributesMap contactAttributes;

    foreach (const uint handle, handles) {
        if (m_handles.contains(handle)){
            QVariantMap attributes;
            attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = m_handles.value(handle);

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = Tp::SubscriptionStateYes;
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = Tp::SubscriptionStateYes;
            }
            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")] = QVariant::fromValue(getPresence(handle));
            }
            contactAttributes[handle] = attributes;
        }
    }
    return contactAttributes;
}

void MorseConnection::requestSubscription(const Tp::UIntList &handles, const QString &message, Tp::DBusError *error)
{
//    http://telepathy.freedesktop.org/spec/Connection_Interface_Contact_List.html#Method:RequestSubscription

    Q_UNUSED(message);
    const QStringList phoneNumbers = inspectHandles(Tp::HandleTypeContact, handles, error);

    if (error->isValid()) {
        return;
    }

    if (phoneNumbers.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
    }

    if (!m_core || !m_core->isAuthenticated()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    m_core->addContacts(phoneNumbers);
}

void MorseConnection::removeContacts(const Tp::UIntList &handles, Tp::DBusError *error)
{
    const QStringList phoneNumbers = inspectHandles(Tp::HandleTypeContact, handles, error);

    if (error->isValid()) {
        return;
    }

    if (phoneNumbers.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
    }

    if (!m_core || !m_core->isAuthenticated()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    m_core->deleteContacts(phoneNumbers);
}

Tp::SimplePresence MorseConnection::getPresence(uint handle)
{
    if (!m_presences.contains(handle)) {
        return Tp::SimplePresence();
    }

    return m_presences.value(handle);
}

uint MorseConnection::setPresence(const QString &status, const QString &message, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << status;

    if (m_core->isAuthenticated()) {
        m_core->setOnlineStatus(status == QLatin1String("available"));
    }

    return 0;
}

uint MorseConnection::ensureContact(const QString &identifier)
{
    uint handle = getHandle(identifier);
    if (!handle) {
        handle = addContact(identifier);
    }
    return handle;
}

uint MorseConnection::addContacts(const QStringList &identifiers)
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

    updateContactsState(identifiers);
    setSubscriptionState(identifiers, newHandles, Tp::SubscriptionStateUnknown);

    return handle;
}

uint MorseConnection::addContact(const QString &identifier)
{
    qDebug() << Q_FUNC_INFO;
    return addContacts(QStringList() << identifier);
}

void MorseConnection::updateContactsState(const QStringList &identifiers)
{
    qDebug() << Q_FUNC_INFO;
    Tp::SimpleContactPresences newPresences;
    foreach (const QString &phone, identifiers) {
        uint handle = ensureContact(phone);

        TelegramNamespace::ContactStatus st = TelegramNamespace::ContactStatusUnknown;

        if (m_core) {
            st = m_core->contactStatus(phone);
        }

        Tp::SimplePresence presence;

        switch (st) {
        case TelegramNamespace::ContactStatusOnline:
            presence.status = QLatin1String("available");
            presence.type = Tp::ConnectionPresenceTypeAvailable;
            break;
        case TelegramNamespace::ContactStatusOffline:
            presence.status = QLatin1String("offline");
            presence.type = Tp::ConnectionPresenceTypeOffline;
            break;
        default:
        case TelegramNamespace::ContactStatusUnknown:
            presence.status = QLatin1String("unknown");
            presence.type = Tp::ConnectionPresenceTypeUnknown;
            break;
        }

        m_presences[handle] = presence;
        newPresences[handle] = presence;
    }
    simplePresenceIface->setPresences(newPresences);
}

void MorseConnection::setSubscriptionState(const QStringList &identifiers, const QList<uint> &handles, uint state)
{
    qDebug() << Q_FUNC_INFO;
    Tp::ContactSubscriptionMap changes;
    Tp::HandleIdentifierMap identifiersMap;

    for(int i = 0; i < identifiers.size(); ++i) {
        Tp::ContactSubscriptions change;
        change.publish = Tp::SubscriptionStateYes;
        change.publishRequest = QString();
        change.subscribe = state;
        changes[handles[i]] = change;
        identifiersMap[handles[i]] = identifiers[i];
        m_contactsSubscription[handles[i]] = state;
    }
    Tp::HandleIdentifierMap removals;
    contactListIface->contactsChangedWithID(changes, identifiersMap, removals);
}

void MorseConnection::sendMessage(const QString &sender, const QString &message)
{
    m_core->sendMessage(sender, message);
}

/* Receive message from someone to ourself */
void MorseConnection::receiveMessage(const QString &sender, const QString &message)
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
    text[QLatin1String("content-type")] = QDBusVariant(QLatin1String("text/plain"));
    text[QLatin1String("content")]      = QDBusVariant(message);
    body << text;

    Tp::MessagePartList partList;
    Tp::MessagePart header;
    header[QLatin1String("message-received")]  = QDBusVariant(timestamp);
    header[QLatin1String("message-sender")]    = QDBusVariant(senderHandle);
    header[QLatin1String("message-sender-id")] = QDBusVariant(sender);
    header[QLatin1String("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeNormal);

    partList << header << body;
    textChannel->addReceivedMessage(partList);
}

void MorseConnection::whenContactListChanged()
{
#ifdef SIMULATION
    const QStringList identifiers = QStringList() << QLatin1String("1234567890");
#else
    const QStringList identifiers = m_core->contactList();
#endif

    qDebug() << Q_FUNC_INFO << identifiers;

    addContacts(identifiers);

//    Tp::ContactSubscriptionMap changes;
//    Tp::HandleIdentifierMap identifiers;
//    Tp::HandleIdentifierMap removals;

    QList<uint> handles;

    for (int i = 0; i < identifiers.count(); ++i) {
        handles.append(ensureContact(identifiers.at(i)));
    }

    setSubscriptionState(identifiers, handles, Tp::SubscriptionStateYes);
    updateContactsState(identifiers);

//    receiveMessage(identifiers.first(), QLatin1String("Message to add contact"));

    contactListIface->setContactListState(Tp::ContactListStateSuccess);
}

void MorseConnection::whenDisconnected()
{
    qDebug() << Q_FUNC_INFO;

    m_core->setOnlineStatus(false); // TODO: Real disconnect

    saveSessionData(m_selfPhone, m_core->connectionSecretInfo());
}

QByteArray MorseConnection::getSessionData(const QString &phone)
{
#ifdef INSECURE_SAVE
    QDir dir;
    dir.mkdir(secretsDirPath);

    QFile secretFile(secretsDirPath + QLatin1Char('/') + phone);

    if (secretFile.open(QIODevice::ReadOnly)) {
        return secretFile.readAll();
    }
#endif

    return QByteArray();
}

bool MorseConnection::saveSessionData(const QString &phone, const QByteArray &data)
{
#ifdef INSECURE_SAVE
    QDir dir;
    dir.mkdir(secretsDirPath);

    QFile secretFile(secretsDirPath + QLatin1Char('/') + phone);

    if (secretFile.open(QIODevice::WriteOnly)) {
        return secretFile.write(data) == data.size();
    }
#endif

    return false;
}

void MorseConnection::updateContactPresence(const QString &identifier)
{
    qDebug() << "Update presence for " << identifier;
    updateContactsState(QStringList() << identifier);
}

uint MorseConnection::getHandle(const QString &identifier) const
{
    foreach (uint key, m_handles.keys()) {
        if (m_handles.value(key) == identifier) {
            return key;
        }
    }

    return 0;
}
