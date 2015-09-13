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

#define INSECURE_SAVE

#ifdef INSECURE_SAVE

#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif // QT_VERSION >= 0x050000

#include <QDir>
#include <QFile>

static const QString secretsDirPath = QLatin1String("/secrets/");
#endif // INSECURE_SAVE

static const QString c_onlineSimpleStatusKey = QLatin1String("available");

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
    spAvailable.canHaveMessage = false;

    Tp::SimpleStatusSpec spHidden;
    spHidden.type = Tp::ConnectionPresenceTypeHidden;
    spHidden.maySetOnSelf = true;
    spHidden.canHaveMessage = false;

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
    m_core(0),
    m_authReconnectionsCount(0)
{
    qDebug() << Q_FUNC_INFO;
    /* Connection.Interface.Contacts */
#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
    contactsIface = Tp::BaseConnectionContactsInterface::create(this);
#else
    contactsIface = Tp::BaseConnectionContactsInterface::create();
#endif
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this, &MorseConnection::getContactAttributes));
    contactsIface->setContactAttributeInterfaces(QStringList()
                                                 << TP_QT_IFACE_CONNECTION
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING
                                                 << TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS);
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    /* Connection.Interface.SimplePresence */
    simplePresenceIface = Tp::BaseConnectionSimplePresenceInterface::create();
    simplePresenceIface->setStatuses(getSimpleStatusSpecMap());
    simplePresenceIface->setSetPresenceCallback(Tp::memFun(this,&MorseConnection::setPresence));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(simplePresenceIface));

    /* Connection.Interface.ContactList */
    contactListIface = Tp::BaseConnectionContactListInterface::create();
    contactListIface->setContactListPersists(true);
    contactListIface->setCanChangeContactList(true);
    contactListIface->setDownloadAtConnection(true);
    contactListIface->setGetContactListAttributesCallback(Tp::memFun(this, &MorseConnection::getContactListAttributes));
    contactListIface->setRequestSubscriptionCallback(Tp::memFun(this, &MorseConnection::requestSubscription));
    contactListIface->setRemoveContactsCallback(Tp::memFun(this, &MorseConnection::removeContacts));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactListIface));

    /* Connection.Interface.Aliasing */
    aliasingIface = Tp::BaseConnectionAliasingInterface::create();
    aliasingIface->setGetAliasesCallback(Tp::memFun(this, &MorseConnection::getAliases));
    aliasingIface->setSetAliasesCallback(Tp::memFun(this, &MorseConnection::setAliases));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(aliasingIface));

    /* Connection.Interface.Avatars */
    avatarsIface = Tp::BaseConnectionAvatarsInterface::create();
    avatarsIface->setAvatarDetails(Tp::AvatarSpec(/* supportedMimeTypes */ QStringList() << QLatin1String("image/jpeg"),
                                                  /* minHeight */ 0, /* maxHeight */ 160, /* recommendedHeight */ 160,
                                                  /* minWidth */ 0, /* maxWidth */ 160, /* recommendedWidth */ 160,
                                                  /* maxBytes */ 10240));
    avatarsIface->setGetKnownAvatarTokensCallback(Tp::memFun(this, &MorseConnection::getKnownAvatarTokens));
    avatarsIface->setRequestAvatarsCallback(Tp::memFun(this, &MorseConnection::requestAvatars));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(avatarsIface));

    /* Connection.Interface.Requests */
    requestsIface = Tp::BaseConnectionRequestsInterface::create(this);
    /* Fill requestableChannelClasses */
    Tp::RequestableChannelClass personalChat;
    personalChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    personalChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeContact;
    personalChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    personalChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    requestsIface->requestableChannelClasses << personalChat;

#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
    Tp::RequestableChannelClass groupChat;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeRoom;
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    requestsIface->requestableChannelClasses << groupChat;

    Tp::RequestableChannelClass chatList;
    chatList.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST;
    chatList.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeNone;
    requestsIface->requestableChannelClasses << chatList;
#endif

    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    m_selfPhone = parameters.value(QLatin1String("account")).toString();

    setSelfHandle(addContact(m_selfPhone));

    setConnectCallback(Tp::memFun(this, &MorseConnection::doConnect));
    setInspectHandlesCallback(Tp::memFun(this, &MorseConnection::inspectHandles));
    setCreateChannelCallback(Tp::memFun(this, &MorseConnection::createChannelCB));
    setRequestHandlesCallback(Tp::memFun(this, &MorseConnection::requestHandles));

    connect(this, SIGNAL(disconnected()), SLOT(whenDisconnected()));
}

MorseConnection::~MorseConnection()
{
    if (m_core) {
        m_core->deleteLater();
    }
}

void MorseConnection::doConnect(Tp::DBusError *error)
{
    Q_UNUSED(error);

    CAppInformation appInfo;
    appInfo.setAppId(14617);
    appInfo.setAppHash(QLatin1String("e17ac360fd072f83d5d08db45ce9a121"));
    appInfo.setAppVersion(QLatin1String("0.1"));
    appInfo.setDeviceInfo(QLatin1String("pc"));
    appInfo.setOsInfo(QLatin1String("GNU/Linux"));
    appInfo.setLanguageCode(QLatin1String("en"));

    m_authReconnectionsCount = 0;
    m_core = new CTelegramCore(0);
    m_core->setAppInformation(&appInfo);
    m_core->setMessageReceivingFilterFlags(TelegramNamespace::MessageFlagOut|TelegramNamespace::MessageFlagRead);
    m_core->setAcceptableMessageTypes(TelegramNamespace::MessageTypeText);

    setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonNoneSpecified);

    connect(m_core, SIGNAL(connectionStateChanged(TelegramNamespace::ConnectionState)),
            this, SLOT(whenConnectionStateChanged(TelegramNamespace::ConnectionState)));
    connect(m_core, SIGNAL(authorizationErrorReceived()),
            this, SLOT(whenAuthErrorReceived()));
    connect(m_core, SIGNAL(phoneCodeRequired()),
            this, SLOT(whenPhoneCodeRequired()));
    connect(m_core, SIGNAL(authSignErrorReceived(TelegramNamespace::AuthSignError,QString)),
            this, SLOT(whenAuthSignErrorReceived(TelegramNamespace::AuthSignError,QString)));
    connect(m_core, SIGNAL(avatarReceived(QString,QByteArray,QString,QString)),
            this, SLOT(whenAvatarReceived(QString,QByteArray,QString,QString)));
    connect(m_core, SIGNAL(contactListChanged()),
            this, SLOT(whenContactListChanged()));
    connect(m_core, SIGNAL(messageReceived(QString,QString,TelegramNamespace::MessageType,quint32,quint32,quint32)),
            this, SLOT(whenMessageReceived(QString,QString,TelegramNamespace::MessageType,quint32,quint32,quint32)));
    connect(m_core, SIGNAL(chatMessageReceived(quint32,QString,QString,TelegramNamespace::MessageType,quint32,quint32,quint32)),
            this, SLOT(whenChatMessageReceived(quint32,QString,QString,TelegramNamespace::MessageType,quint32,quint32,quint32)));
    connect(m_core, SIGNAL(chatChanged(quint32)),
            this, SLOT(whenChatChanged(quint32)));
    connect(m_core, SIGNAL(contactStatusChanged(QString,TelegramNamespace::ContactStatus)),
            this, SLOT(updateContactPresence(QString)));

    const QByteArray sessionData = getSessionData(m_selfPhone);

    if (sessionData.isEmpty()) {
        qDebug() << "init connection...";
        m_core->initConnection(QLatin1String("149.154.175.50"), 443);
    } else {
        qDebug() << "restore connection...";
        m_core->restoreConnection(sessionData);
    }
}

void MorseConnection::whenConnectionStateChanged(TelegramNamespace::ConnectionState state)
{
    qDebug() << Q_FUNC_INFO << state;
    switch (state) {
    case TelegramNamespace::ConnectionStateAuthRequired:
        m_core->requestPhoneCode(m_selfPhone);
        break;
    case TelegramNamespace::ConnectionStateAuthenticated:
        whenAuthenticated();
        break;
    case TelegramNamespace::ConnectionStateReady:
        whenConnectionReady();
        updateSelfContactState(Tp::ConnectionStatusConnected);
        break;
    case TelegramNamespace::ConnectionStateDisconnected:
        if (status() == Tp::ConnectionStatusConnected) {
            saveSessionData(m_selfPhone, m_core->connectionSecretInfo());
            setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonNetworkError);
            updateSelfContactState(Tp::ConnectionStatusDisconnected);
            emit disconnected();
        }
        break;
    default:
        break;
    }
}

void MorseConnection::whenAuthenticated()
{
    qDebug() << Q_FUNC_INFO;

    if (!saslIface.isNull()) {
        saslIface->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    Tp::SimpleContactPresences presences;
    Tp::SimplePresence presence;

    if (m_wantedPresence.isNull()) {
        m_wantedPresence = c_onlineSimpleStatusKey;
    }

    presence.status = m_wantedPresence;
    presence.statusMessage = QString();
    presence.type = simplePresenceIface->statuses().value(m_wantedPresence).type;
    presences[selfHandle()] = presence;
    simplePresenceIface->setPresences(presences);

    setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);
    contactListIface->setContactListState(Tp::ContactListStateWaiting);
}

void MorseConnection::whenAuthErrorReceived()
{
    qDebug() << Q_FUNC_INFO;

    static const int reconnectionsLimit = 1;

    if (m_authReconnectionsCount < reconnectionsLimit) {
        qDebug() << "MorseConnection::whenAuthErrorReceived(): Auth error received. Trying to re-init connection without session data..." << m_authReconnectionsCount + 1 << " attempt.";
        setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonAuthenticationFailed);
        ++m_authReconnectionsCount;
        m_core->closeConnection();
        m_core->initConnection(QLatin1String("173.240.5.1"), 443);
    } else {
        qDebug() << "MorseConnection::whenAuthErrorReceived(): Auth error received. Can not connect (tried" << m_authReconnectionsCount << " times).";
        setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonAuthenticationFailed);
    }
}

void MorseConnection::whenPhoneCodeRequired()
{
    qDebug() << Q_FUNC_INFO;

    Tp::DBusError error;

    //Registration
    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION);

    Tp::BaseChannelServerAuthenticationTypePtr authType
            = Tp::BaseChannelServerAuthenticationType::create(TP_QT_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(authType));

    saslIface = Tp::BaseChannelSASLAuthenticationInterface::create(QStringList() << QLatin1String("X-TELEPATHY-PASSWORD"),
                                                                   /* hasInitialData */ false,
                                                                   /* canTryAgain */ true,
                                                                   /* authorizationIdentity */ m_selfPhone,
                                                                   /* defaultUsername */ QString(),
                                                                   /* defaultRealm */ QString(),
                                                                   /* maySaveResponse */ false);

    saslIface->setStartMechanismWithDataCallback( Tp::memFun(this, &MorseConnection::startMechanismWithData));

    baseChannel->setRequested(false);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(saslIface));

    baseChannel->registerObject(&error);

    if (!error.isValid()) {
        addChannel(baseChannel);
    }
}

void MorseConnection::whenAuthSignErrorReceived(TelegramNamespace::AuthSignError errorCode, const QString &errorMessage)
{
    qDebug() << Q_FUNC_INFO << errorCode << errorMessage;

    QVariantMap details;
    details[QLatin1String("server-message")] = errorMessage;

    switch (errorCode) {
        break;
    default:
        saslIface->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, details);
        break;
    }
}

void MorseConnection::startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << mechanism << data;

    saslIface->setSaslStatus(Tp::SASLStatusInProgress, QLatin1String("InProgress"), QVariantMap());

    m_core->signIn(m_selfPhone, QString::fromLatin1(data.constData()));
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

void MorseConnection::whenConnectionReady()
{
    qDebug() << Q_FUNC_INFO;

    saveSessionData(m_selfPhone, m_core->connectionSecretInfo());

    m_core->setOnlineStatus(m_wantedPresence == c_onlineSimpleStatusKey);
    m_core->setMessageReceivingFilterFlags(TelegramNamespace::MessageFlagNone);
    whenContactListChanged();
}

QStringList MorseConnection::inspectHandles(uint handleType, const Tp::UIntList &handles, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << handleType << handles;

    switch (handleType) {
    case Tp::HandleTypeContact:
    case Tp::HandleTypeRoom:
        break;
    default:
        if (error) {
            error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Unsupported handle type"));
        }
        return QStringList();
        break;
    }

    QStringList result;

    const QMap<uint, QString> handlesContainer = handleType == Tp::HandleTypeContact ? m_handles : m_chatHandles;

    foreach (uint handle, handles) {
        if (!handlesContainer.contains(handle)) {
            if (error) {
                error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Unknown handle"));
            }
            return QStringList();
        }

        result.append(handlesContainer.value(handle));
    }

    return result;
}

Tp::BaseChannelPtr MorseConnection::createChannelCB(const QVariantMap &request, Tp::DBusError *error)
{
    const QString channelType = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")).toString();

    if (channelType == TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST) {
        return createRoomListChannel();
    }

    uint targetHandleType = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")).toUInt();
    uint targetHandle = 0;
    QString targetID;

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = m_handles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString();
            targetHandle = ensureContact(targetID);
        }
        break;
    case Tp::HandleTypeRoom:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = m_chatHandles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString();
            targetHandle = ensureChat(targetID);
        }
    default:
        break;
    }

    // Looks like there is no any case for InitiatorID other than selfID
    uint initiatorHandle = 0;

    if (targetHandleType == Tp::HandleTypeContact) {
        initiatorHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".InitiatorHandle"), selfHandle()).toUInt();
    }

    qDebug() << "MorseConnection::createChannel " << channelType
             << " " << targetHandleType
             << " " << targetHandle
             << " " << request;

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
    case Tp::HandleTypeRoom:
#endif
        break;
    default:
        if (error) {
            error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Unknown target handle type"));
        }
        return Tp::BaseChannelPtr();
        break;
    }

    if (!targetHandle
            || ((targetHandleType == Tp::HandleTypeContact) && !m_handles.contains(targetHandle))
            || ((targetHandleType == Tp::HandleTypeRoom) && !m_chatHandles.contains(targetHandle))
            ) {
        if (error) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Target handle is unknown."));
        }
        return Tp::BaseChannelPtr();
    }

    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, channelType, Tp::HandleType(targetHandleType), targetHandle);
    baseChannel->setTargetID(targetID);
    baseChannel->setInitiatorHandle(initiatorHandle);

    if (channelType == TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        MorseTextChannelPtr textChannel = MorseTextChannel::create(m_core, baseChannel.data(), selfHandle(), m_selfPhone);
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(textChannel));

        if (targetHandleType == Tp::HandleTypeRoom) {
            quint32 chatId = targetID.section(QLatin1String("chat"), 1).toUInt();

            QStringList participants;
            if (m_core->getChatParticipants(&participants, chatId) && !participants.isEmpty()) {
                textChannel->whenChatDetailsChanged(chatId, requestHandles(Tp::HandleTypeContact, participants, 0));
            }

            connect(this, SIGNAL(chatDetailsChanged(quint32,Tp::UIntList,QStringList)),
                    textChannel.data(), SLOT(whenChatDetailsChanged(quint32,Tp::UIntList)));
        }
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
        result.append(ensureContact(identify));
    }

    return result;
}

Tp::ContactAttributesMap MorseConnection::getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error)
{
    Q_UNUSED(hold);

    Tp::UIntList handles = m_handles.keys();
    handles.removeOne(selfHandle());

    return getContactAttributes(handles, interfaces, error);
}

Tp::ContactAttributesMap MorseConnection::getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error)
{
//    http://telepathy.freedesktop.org/spec/Connection_Interface_Contacts.html#Method:GetContactAttributes
//    qDebug() << Q_FUNC_INFO << handles << interfaces;

    Tp::ContactAttributesMap contactAttributes;

    foreach (const uint handle, handles) {
        if (m_handles.contains(handle)){
            QVariantMap attributes;
            const QString identifier = m_handles.value(handle);
            attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = identifier;

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = Tp::SubscriptionStateYes;
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = Tp::SubscriptionStateYes;
            }

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")] = QVariant::fromValue(getPresence(handle));
            }

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING + QLatin1String("/alias")] = QVariant::fromValue(getAlias(handle));
            }

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS + QLatin1String("/token")] = QVariant::fromValue(m_core->contactAvatarToken(identifier));
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

    if (!coreIsReady()) {
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

    if (!coreIsReady()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    m_core->deleteContacts(phoneNumbers);
}

Tp::AliasMap MorseConnection::getAliases(const Tp::UIntList &handles, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << handles;

    Tp::AliasMap aliases;

    foreach (uint handle, handles) {
        aliases[handle] = getAlias(handle);
    }

    return aliases;
}

void MorseConnection::setAliases(const Tp::AliasMap &aliases, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << aliases;
    error->set(TP_QT_ERROR_NOT_IMPLEMENTED, QLatin1String("Not implemented"));
}

QString MorseConnection::getAlias(uint handle)
{
    const QString phone = m_handles.value(handle);

    if (phone.isEmpty()) {
        return QString();
    }

    return m_core->contactFirstName(phone) + QLatin1Char(' ') + m_core->contactLastName(phone);
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
    Q_UNUSED(message)
    Q_UNUSED(error)

    m_wantedPresence = status;

    if (coreIsAuthenticated()) {
        m_core->setOnlineStatus(status == c_onlineSimpleStatusKey);
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

uint MorseConnection::ensureChat(const QString &identifier)
{
    uint handle = getChatHandle(identifier);
    if (!handle) {
        if (m_chatHandles.isEmpty()) {
            handle = 1;
        } else {
            handle = m_chatHandles.keys().last() + 1;
        }

        m_chatHandles.insert(handle, identifier);
    }
    return handle;
}

uint MorseConnection::addContacts(const QStringList &identifiers)
{
    qDebug() << Q_FUNC_INFO << identifiers;
    uint handle = 0;

    if (!m_handles.isEmpty()) {
        handle = m_handles.keys().last();
    }

    QList<uint> newHandles;
    QStringList newIdentifiers;
    foreach(const QString &identifier, identifiers) {
        if (getHandle(identifier)) {
            continue;
        }

        ++handle;
        m_handles.insert(handle, identifier);
        newHandles << handle;
        newIdentifiers << identifier;
    }

    updateContactsState(newIdentifiers);
    setSubscriptionState(newIdentifiers, newHandles, Tp::SubscriptionStateUnknown);

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

        if (handle == selfHandle()) {
            continue;
        }

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

void MorseConnection::updateSelfContactState(Tp::ConnectionStatus status)
{
    Tp::SimpleContactPresences newPresences;
    Tp::SimplePresence presence;
    if (status == Tp::ConnectionStatusConnected) {
        presence.status = QLatin1String("available");
        presence.type = Tp::ConnectionPresenceTypeAvailable;
    } else {
        presence.status = QLatin1String("offline");
        presence.type = Tp::ConnectionPresenceTypeOffline;
    }

    m_presences[selfHandle()] = presence;
    newPresences[selfHandle()] = presence;
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

/* Receive message from outside (telegram server) */
void MorseConnection::whenMessageReceived(const QString &identifier, const QString &message, TelegramNamespace::MessageType type, quint32 messageId, quint32 flags, quint32 timestamp)
{
    if (type != TelegramNamespace::MessageTypeText) {
        return;
    }

    uint initiatorHandle, targetHandle;

    initiatorHandle = targetHandle = ensureContact(identifier);

    //TODO: initiator should be group creator
    Tp::DBusError error;
    bool yours;

    QVariantMap request;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")] = targetHandle;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")] = Tp::HandleTypeContact;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".InitiatorHandle")] = initiatorHandle;

    Tp::BaseChannelPtr channel = ensureChannel(request, yours, /* suppressHandler */ false, &error);
    if (error.isValid()) {
        qWarning() << Q_FUNC_INFO << "ensureChannel failed:" << error.name() << " " << error.message();
        return;
    }

    MorseTextChannelPtr textChannel = MorseTextChannelPtr::dynamicCast(channel->interface(TP_QT_IFACE_CHANNEL_TYPE_TEXT));

    if (!textChannel) {
        qDebug() << Q_FUNC_INFO << "Error, channel is not a morseTextChannel?";
        return;
    }

    textChannel->whenMessageReceived(message, messageId, flags, timestamp);
}

void MorseConnection::whenChatMessageReceived(quint32 chatId, const QString &contact, const QString &message, TelegramNamespace::MessageType type, quint32 messageId, quint32 flags, quint32 timestamp)
{
    if (type != TelegramNamespace::MessageTypeText) {
        return;
    }

    const QString identifier = QString(QLatin1String("chat%1")).arg(chatId);

    uint initiatorHandle, targetHandle, contactHandle;

    targetHandle = ensureChat(identifier);
    contactHandle = ensureContact(contact);

    initiatorHandle = contactHandle; // For now

    //TODO: initiator should be group creator
    Tp::DBusError error;
    bool yours;

    QVariantMap request;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")] = Tp::HandleTypeRoom;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")] = targetHandle;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".InitiatorHandle")] = initiatorHandle;

    Tp::BaseChannelPtr channel = ensureChannel(request, yours, /* suppressHandler */ false, &error);
    if (error.isValid()) {
        qWarning() << Q_FUNC_INFO << "ensureChannel failed:" << error.name() << " " << error.message();
        return;
    }

    MorseTextChannelPtr textChannel = MorseTextChannelPtr::dynamicCast(channel->interface(TP_QT_IFACE_CHANNEL_TYPE_TEXT));

    if (!textChannel) {
        qDebug() << Q_FUNC_INFO << "Error, channel is not a morseTextChannel?";
        return;
    }

    textChannel->whenChatMessageReceived(contactHandle, message, messageId, flags, timestamp);
}

void MorseConnection::whenChatChanged(quint32 chatId)
{
    QStringList participants;
    if (m_core->getChatParticipants(&participants, chatId) && !participants.isEmpty()) {
        emit chatDetailsChanged(chatId, requestHandles(Tp::HandleTypeContact, participants, 0), participants);
    }
}

void MorseConnection::whenContactListChanged()
{
    const QStringList identifiers = m_core->contactList();

    qDebug() << Q_FUNC_INFO << identifiers;

//    Tp::ContactSubscriptionMap changes;
//    Tp::HandleIdentifierMap identifiers;
//    Tp::HandleIdentifierMap removals;

    QList<uint> handles;

    for (int i = 0; i < identifiers.count(); ++i) {
        handles.append(ensureContact(identifiers.at(i)));
    }

    setSubscriptionState(identifiers, handles, Tp::SubscriptionStateYes);
    updateContactsState(identifiers);

    contactListIface->setContactListState(Tp::ContactListStateSuccess);
}

void MorseConnection::whenDisconnected()
{
    qDebug() << Q_FUNC_INFO;

    m_core->setOnlineStatus(false); // TODO: Real disconnect

    saveSessionData(m_selfPhone, m_core->connectionSecretInfo());
    setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonRequested);
}

/* Connection.Interface.Avatars */
void MorseConnection::whenAvatarReceived(const QString &contact, const QByteArray &data, const QString &mimeType, const QString &token)
{
    avatarsIface->avatarRetrieved(ensureContact(contact), token , data, mimeType);
}

void MorseConnection::whenGotRooms()
{
    qDebug() << Q_FUNC_INFO;
    Tp::RoomInfoList rooms;

    foreach (quint32 chatId, m_core->chatList()) {
        Tp::RoomInfo roomInfo;
        TelegramNamespace::GroupChat chatInfo;

        const QString chatID = QString(QLatin1String("chat%1")).arg(chatId);
        roomInfo.channelType = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
        roomInfo.handle = ensureChat(chatID);
        roomInfo.info[QLatin1String("handle-name")] = chatID;
        roomInfo.info[QLatin1String("members-only")] = true;
        roomInfo.info[QLatin1String("invite-only")] = true;
        roomInfo.info[QLatin1String("password")] = false;

        if (m_core->getChatInfo(&chatInfo, chatId)) {
            roomInfo.info[QLatin1String("name")] = chatInfo.title;
            roomInfo.info[QLatin1String("members")] = chatInfo.participantsCount;
        }

        rooms << roomInfo;
    }

    roomListChannel->gotRooms(rooms);
    roomListChannel->setListingRooms(false);
}

Tp::BaseChannelPtr MorseConnection::createRoomListChannel()
{
    qDebug() << Q_FUNC_INFO;
    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST);

    roomListChannel = Tp::BaseChannelRoomListType::create();
    roomListChannel->setListRoomsCallback(Tp::memFun(this, &MorseConnection::roomListStartListing));
    roomListChannel->setStopListingCallback(Tp::memFun(this, &MorseConnection::roomListStopListing));
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(roomListChannel));

    return baseChannel;
}

Tp::AvatarTokenMap MorseConnection::getKnownAvatarTokens(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    const QStringList identifiers = inspectHandles(Tp::HandleTypeContact, contacts, error);

    if (error->isValid()) {
        return Tp::AvatarTokenMap();
    }

    if (identifiers.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Invalid handle(s)"));
        return Tp::AvatarTokenMap();
    }

    if (!coreIsReady()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
        return Tp::AvatarTokenMap();
    }

    Tp::AvatarTokenMap result;
    for (int i = 0; i < contacts.count(); ++i) {
        result.insert(contacts.at(i), m_core->contactAvatarToken(identifiers.at(i)));
    }

    return result;
}

void MorseConnection::requestAvatars(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    const QStringList identifiers = inspectHandles(Tp::HandleTypeContact, contacts, error);

    if (error->isValid()) {
        return;
    }

    if (identifiers.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
    }

    if (!coreIsReady()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    foreach (const QString &identifier, identifiers) {
        m_core->requestContactAvatar(identifier);
    }
}

void MorseConnection::roomListStartListing(Tp::DBusError *error)
{
    Q_UNUSED(error)

    QTimer::singleShot(0, this, SLOT(whenGotRooms()));
    roomListChannel->setListingRooms(true);
}

void MorseConnection::roomListStopListing(Tp::DBusError *error)
{
    Q_UNUSED(error)
    roomListChannel->setListingRooms(false);
}

bool MorseConnection::coreIsReady()
{
    return m_core && (m_core->connectionState() == TelegramNamespace::ConnectionStateReady);
}

bool MorseConnection::coreIsAuthenticated()
{
    return m_core && (m_core->connectionState() >= TelegramNamespace::ConnectionStateAuthenticated);
}

QByteArray MorseConnection::getSessionData(const QString &phone)
{
#ifdef INSECURE_SAVE

#if QT_VERSION >= 0x050000
    QFile secretFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath + phone);
#else // QT_VERSION >= 0x050000
    QFile secretFile(QDesktopServices::storageLocation(QDesktopServices::CacheLocation) + secretsDirPath + phone);
#endif // QT_VERSION >= 0x050000

    if (secretFile.open(QIODevice::ReadOnly)) {
        return secretFile.readAll();
    }
#endif // INSECURE_SAVE

    return QByteArray();
}

bool MorseConnection::saveSessionData(const QString &phone, const QByteArray &data)
{
#ifdef INSECURE_SAVE
    QDir dir;
#if QT_VERSION >= 0x050000
    dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath);
    QFile secretFile(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + secretsDirPath + phone);
#else // QT_VERSION >= 0x050000
    dir.mkpath(QDesktopServices::storageLocation(QDesktopServices::CacheLocation) + secretsDirPath);
    QFile secretFile(QDesktopServices::storageLocation(QDesktopServices::CacheLocation) + secretsDirPath + phone);
#endif // QT_VERSION >= 0x050000

    if (secretFile.open(QIODevice::WriteOnly)) {
        return secretFile.write(data) == data.size();
    }
#endif // INSECURE_SAVE

    return false;
}

void MorseConnection::updateContactPresence(const QString &identifier)
{
    qDebug() << "Update presence for " << identifier;
    updateContactsState(QStringList() << identifier);
}

uint MorseConnection::getHandle(const QString &identifier) const
{
    return m_handles.key(identifier, 0);
}

uint MorseConnection::getChatHandle(const QString &identifier) const
{
    return m_chatHandles.key(identifier, 0);
}

