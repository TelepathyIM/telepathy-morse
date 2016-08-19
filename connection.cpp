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

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
#include "contactgroups.hpp"
#endif

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
static const QString c_saslMechanismTelepathyPassword = QLatin1String("X-TELEPATHY-PASSWORD");

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

Tp::RequestableChannelClassSpecList MorseConnection::getRequestableChannelList()
{
    Tp::RequestableChannelClassSpecList result;

    /* Fill requestableChannelClasses */
    Tp::RequestableChannelClass personalChat;
    personalChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    personalChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeContact;
    personalChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    personalChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    result << Tp::RequestableChannelClassSpec(personalChat);

#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
    Tp::RequestableChannelClass groupChat;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeRoom;
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    result << Tp::RequestableChannelClassSpec(groupChat);

    Tp::RequestableChannelClass chatList;
    chatList.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST;
    result << Tp::RequestableChannelClassSpec(chatList);
#endif // TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)

    return result;
}

MorseConnection::MorseConnection(const QDBusConnection &dbusConnection, const QString &cmName, const QString &protocolName, const QVariantMap &parameters) :
    Tp::BaseConnection(dbusConnection, cmName, protocolName, parameters),
    m_core(0),
    m_passwordInfo(0),
    m_authReconnectionsCount(0)
{
    qDebug() << Q_FUNC_INFO;
    /* Connection.Interface.Contacts */
    contactsIface = Tp::BaseConnectionContactsInterface::create();
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

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
    ConnectionContactGroupsInterfacePtr groupsIface = ConnectionContactGroupsInterface::create();
#else
    Tp::BaseConnectionContactGroupsInterfacePtr groupsIface = Tp::BaseConnectionContactGroupsInterface::create();
#endif
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(groupsIface));

    /* Connection.Interface.Requests */
    requestsIface = Tp::BaseConnectionRequestsInterface::create(this);
    requestsIface->requestableChannelClasses = getRequestableChannelList().bareClasses();

    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    m_selfPhone = parameters.value(QLatin1String("account")).toString();
    m_keepaliveInterval = parameters.value(QLatin1String("keepalive-interval"), CTelegramCore::defaultPingInterval() / 1000).toUInt();

    setConnectCallback(Tp::memFun(this, &MorseConnection::doConnect));
    setInspectHandlesCallback(Tp::memFun(this, &MorseConnection::inspectHandles));
    setCreateChannelCallback(Tp::memFun(this, &MorseConnection::createChannelCB));
    setRequestHandlesCallback(Tp::memFun(this, &MorseConnection::requestHandles));

    connect(this, SIGNAL(disconnected()), SLOT(whenDisconnected()));

    m_handles.insert(1, MorseIdentifier());
    setSelfHandle(1);
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
    m_core->setPingInterval(m_keepaliveInterval * 1000);
    m_core->setAppInformation(&appInfo);
    m_core->setMessageReceivingFilter(TelegramNamespace::MessageFlagOut|TelegramNamespace::MessageFlagRead);
    m_core->setAcceptableMessageTypes(
                    TelegramNamespace::MessageTypeText |
                    TelegramNamespace::MessageTypePhoto |
                    TelegramNamespace::MessageTypeAudio |
                    TelegramNamespace::MessageTypeVideo |
                    TelegramNamespace::MessageTypeContact |
                    TelegramNamespace::MessageTypeDocument |
                    TelegramNamespace::MessageTypeGeo );

    setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonNoneSpecified);

    connect(m_core, SIGNAL(connectionStateChanged(TelegramNamespace::ConnectionState)),
            this, SLOT(whenConnectionStateChanged(TelegramNamespace::ConnectionState)));
    connect(m_core, SIGNAL(selfUserAvailable(quint32)),
            SLOT(onSelfUserAvailable()));
    connect(m_core, SIGNAL(authorizationErrorReceived(TelegramNamespace::UnauthorizedError,QString)),
            this, SLOT(onAuthErrorReceived(TelegramNamespace::UnauthorizedError,QString)));
    connect(m_core, SIGNAL(phoneCodeRequired()),
            this, SLOT(whenPhoneCodeRequired()));
    connect(m_core, SIGNAL(authSignErrorReceived(TelegramNamespace::AuthSignError,QString)),
            this, SLOT(whenAuthSignErrorReceived(TelegramNamespace::AuthSignError,QString)));
    connect(m_core, SIGNAL(passwordInfoReceived(quint64)),
            SLOT(onPasswordInfoReceived(quint64)));
    connect(m_core, SIGNAL(avatarReceived(quint32,QByteArray,QString,QString)),
            this, SLOT(whenAvatarReceived(quint32,QByteArray,QString,QString)));
    connect(m_core, SIGNAL(contactListChanged()),
            this, SLOT(whenContactListChanged()));
    connect(m_core, SIGNAL(messageReceived(TelegramNamespace::Message)),
             this, SLOT(whenMessageReceived(TelegramNamespace::Message)));
    connect(m_core, SIGNAL(chatChanged(quint32)),
            this, SLOT(whenChatChanged(quint32)));
    connect(m_core, SIGNAL(contactStatusChanged(quint32,TelegramNamespace::ContactStatus)),
            this, SLOT(setContactStatus(quint32,TelegramNamespace::ContactStatus)));

    const QByteArray sessionData = getSessionData(m_selfPhone);

    if (sessionData.isEmpty()) {
        qDebug() << "init connection...";
        m_core->initConnection(QVector<TelegramNamespace::DcOption>(1, TelegramNamespace::DcOption(QLatin1String("149.154.175.50"), 443)));
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

    if (!saslIface_authCode.isNull()) {
        saslIface_authCode->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    if (!saslIface_password.isNull()) {
        saslIface_password->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    if (m_passwordInfo) {
        delete m_passwordInfo;
        m_passwordInfo = 0;
    }

    checkConnected();
    contactListIface->setContactListState(Tp::ContactListStateWaiting);
}

void MorseConnection::onSelfUserAvailable()
{
    qDebug() << Q_FUNC_INFO;

    MorseIdentifier selfIdentifier = MorseIdentifier::fromUserId(m_core->selfId());

    m_handles.insert(1, selfIdentifier);

    int selfHandle = 1;
    setSelfContact(selfHandle, selfIdentifier.toString());

    Tp::SimpleContactPresences presences;
    Tp::SimplePresence presence;

    if (m_wantedPresence.isNull()) {
        m_wantedPresence = c_onlineSimpleStatusKey;
    }

    presence.status = m_wantedPresence;
    presence.statusMessage = QString();
    presence.type = simplePresenceIface->statuses().value(m_wantedPresence).type;
    presences[selfHandle] = presence;
    simplePresenceIface->setPresences(presences);

    checkConnected();
}

void MorseConnection::onAuthErrorReceived(TelegramNamespace::UnauthorizedError errorCode, const QString &errorMessage)
{
    qDebug() << Q_FUNC_INFO << errorCode << errorMessage;

    if (errorCode == TelegramNamespace::UnauthorizedSessionPasswordNeeded) {
        if (!saslIface_authCode.isNull()) {
            saslIface_authCode->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
        }

        m_core->getPassword();
        return;
    }

    static const int reconnectionsLimit = 1;

    if (m_authReconnectionsCount < reconnectionsLimit) {
        qDebug() << "MorseConnection::whenAuthErrorReceived(): Auth error received. Trying to re-init connection without session data..." << m_authReconnectionsCount + 1 << " attempt.";
        setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonAuthenticationFailed);
        ++m_authReconnectionsCount;
        m_core->closeConnection();
        m_core->initConnection(QVector<TelegramNamespace::DcOption>(1, TelegramNamespace::DcOption(QLatin1String("149.154.175.50"), 443)));
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

    saslIface_authCode = Tp::BaseChannelSASLAuthenticationInterface::create(QStringList()
                                                                            << c_saslMechanismTelepathyPassword,
                                                                            /* hasInitialData */ true,
                                                                            /* canTryAgain */ true,
                                                                            /* authorizationIdentity */ m_selfPhone,
                                                                            /* defaultUsername */ QString(),
                                                                            /* defaultRealm */ QString(),
                                                                            /* maySaveResponse */ false);

    saslIface_authCode->setStartMechanismWithDataCallback(Tp::memFun(this, &MorseConnection::startMechanismWithData_authCode));

    baseChannel->setRequested(false);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(saslIface_authCode));
    baseChannel->registerObject(&error);

    if (error.isValid()) {
        qDebug() << Q_FUNC_INFO << error.name() << error.message();
    } else {
        addChannel(baseChannel);
    }
}

void MorseConnection::onPasswordInfoReceived(quint64 requestId)
{
    Q_UNUSED(requestId)

    qDebug() << Q_FUNC_INFO;

    m_passwordInfo = new TelegramNamespace::PasswordInfo();
    m_core->getPasswordInfo(m_passwordInfo, requestId);

    Tp::DBusError error;

    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION);

    Tp::BaseChannelServerAuthenticationTypePtr authType
            = Tp::BaseChannelServerAuthenticationType::create(TP_QT_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(authType));

    saslIface_password = Tp::BaseChannelSASLAuthenticationInterface::create(QStringList()
                                                                            << c_saslMechanismTelepathyPassword,
                                                                            /* hasInitialData */ true,
                                                                            /* canTryAgain */ true,
                                                                            /* authorizationIdentity */ m_selfPhone,
                                                                            /* defaultUsername */ QString(),
                                                                            /* defaultRealm */ QString(),
                                                                            /* maySaveResponse */ true);

    saslIface_password->setStartMechanismWithDataCallback(Tp::memFun(this, &MorseConnection::startMechanismWithData_password));

    baseChannel->setRequested(false);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(saslIface_password));
    baseChannel->registerObject(&error);

    if (error.isValid()) {
        qDebug() << Q_FUNC_INFO << error.name() << error.message();
    } else {
        addChannel(baseChannel);
    }
}

void MorseConnection::whenAuthSignErrorReceived(TelegramNamespace::AuthSignError errorCode, const QString &errorMessage)
{
    qDebug() << Q_FUNC_INFO << errorCode << errorMessage;

    QVariantMap details;
    details[QLatin1String("server-message")] = errorMessage;

    switch (errorCode) {
    case TelegramNamespace::AuthSignErrorPhoneCodeIsExpired:
    case TelegramNamespace::AuthSignErrorPhoneCodeIsInvalid:
        if (!saslIface_authCode.isNull()) {
            saslIface_authCode->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, details);
        }
        break;
    case TelegramNamespace::AuthSignErrorPasswordHashInvalid:
        if (!saslIface_password.isNull()) {
            saslIface_password->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, details);
        }
        break;
    default:
        qWarning() << Q_FUNC_INFO << "Unhandled!";
        break;
    }
}

void MorseConnection::startMechanismWithData_authCode(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << mechanism << data;

    if (!saslIface_authCode->availableMechanisms().contains(mechanism)) {
        error->set(TP_QT_ERROR_NOT_IMPLEMENTED, QString(QLatin1String("Given SASL mechanism \"%1\" is not implemented")).arg(mechanism));
        return;
    }

    saslIface_authCode->setSaslStatus(Tp::SASLStatusInProgress, QLatin1String("InProgress"), QVariantMap());

    m_core->signIn(m_selfPhone, QString::fromLatin1(data.constData()));
}

void MorseConnection::startMechanismWithData_password(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << mechanism << data;

    if (!saslIface_password->availableMechanisms().contains(mechanism)) {
        error->set(TP_QT_ERROR_NOT_IMPLEMENTED, QString(QLatin1String("Given SASL mechanism \"%1\" is not implemented")).arg(mechanism));
        return;
    }

    saslIface_password->setSaslStatus(Tp::SASLStatusInProgress, QLatin1String("InProgress"), QVariantMap());

    m_core->tryPassword(m_passwordInfo->currentSalt(), data);
}

void MorseConnection::whenConnectionReady()
{
    qDebug() << Q_FUNC_INFO;

    saveSessionData(m_selfPhone, m_core->connectionSecretInfo());

    m_core->setOnlineStatus(m_wantedPresence == c_onlineSimpleStatusKey);
    m_core->setMessageReceivingFilter(TelegramNamespace::MessageFlagNone);
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

    const QMap<uint, MorseIdentifier> handlesContainer = handleType == Tp::HandleTypeContact ? m_handles : m_chatHandles;

    foreach (uint handle, handles) {
        if (!handlesContainer.contains(handle)) {
            if (error) {
                error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Unknown handle"));
            }
            return QStringList();
        }

        result.append(handlesContainer.value(handle).toString());
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
    MorseIdentifier targetID;

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = m_handles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = MorseIdentifier::fromString(request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString());
            targetHandle = ensureHandle(targetID);
        }
        break;
    case Tp::HandleTypeRoom:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = m_chatHandles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = MorseIdentifier::fromString(request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString());
            targetHandle = ensureHandle(targetID);
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
             << targetHandleType
             << targetHandle
             << request;

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
    case Tp::HandleTypeRoom:
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
    baseChannel->setTargetID(targetID.toString());
    baseChannel->setInitiatorHandle(initiatorHandle);

    if (channelType == TP_QT_IFACE_CHANNEL_TYPE_TEXT) {
        MorseTextChannelPtr textChannel = MorseTextChannel::create(m_core, baseChannel.data(), selfHandle(), selfID());
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(textChannel));

        if (targetHandleType == Tp::HandleTypeRoom) {
            connect(this, SIGNAL(chatDetailsChanged(quint32,Tp::UIntList)),
                    textChannel.data(), SLOT(whenChatDetailsChanged(quint32,Tp::UIntList)));

            whenChatChanged(targetID.chatId());
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
        result.append(ensureContact(MorseIdentifier::fromString(identify)));
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
        if (m_handles.contains(handle)) {
            QVariantMap attributes;
            const MorseIdentifier identifier = m_handles.value(handle);
            if (identifier.isNull()) {
                qWarning() << Q_FUNC_INFO << "Handle is in map, but identifier is not valid";
                continue;
            }
            attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = identifier.toString();

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
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS + QLatin1String("/token")] = QVariant::fromValue(m_core->contactAvatarToken(identifier.userId()));
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
    if (handles.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid argument (no handles provided)"));
    }

    if (!coreIsReady()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    QVector<quint32> ids;

    foreach (uint handle, handles) {
        if (!m_handles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Unknown handle"));
            return;
        }

        quint32 id = m_handles.value(handle).userId();

        if (!id) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Internal error (invalid handle)"));
        }

        ids.append(id);
    }

    m_core->deleteContacts(ids);
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
    const MorseIdentifier identifier = m_handles.value(handle);

    if (identifier.isNull()) {
        return QLatin1String("Invalid alias");
    }

    TelegramNamespace::UserInfo info;
    m_core->getUserInfo(&info, identifier.userId());

    QString name = info.firstName() + QLatin1Char(' ') + info.lastName();
    if (!name.simplified().isEmpty()) {
        return name;
    }

    if (!info.userName().isEmpty()) {
        return info.userName();
    }

    return tr("Unknown name");
}

Tp::SimplePresence MorseConnection::getPresence(uint handle)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    return *simplePresenceIface->getPresences(Tp::UIntList() << handle).constBegin();
#else
    return simplePresenceIface->getPresences(Tp::UIntList() << handle).first();
#endif
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

uint MorseConnection::ensureHandle(const MorseIdentifier &identifier)
{
    if (identifier.userId()) {
        return ensureContact(identifier);
    } else {
        return ensureChat(identifier);
    }
}

uint MorseConnection::ensureContact(const MorseIdentifier &identifier)
{
    uint handle = getHandle(identifier);
    if (!handle) {
        handle = addContact(identifier);
    }
    return handle;
}

uint MorseConnection::ensureChat(const MorseIdentifier &identifier)
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

uint MorseConnection::addContacts(const QVector<MorseIdentifier> &identifiers)
{
    qDebug() << Q_FUNC_INFO;
    uint handle = 0;

    if (!m_handles.isEmpty()) {
        handle = m_handles.keys().last();
    }

    QList<uint> newHandles;
    QVector<MorseIdentifier> newIdentifiers;
    foreach (const MorseIdentifier &identifier, identifiers) {
        if (getHandle(identifier)) {
            continue;
        }

        ++handle;
        m_handles.insert(handle, identifier);
        newHandles << handle;
        newIdentifiers << identifier;
    }

    return handle;
}

uint MorseConnection::addContact(const MorseIdentifier &identifier)
{
    qDebug() << Q_FUNC_INFO;
    return addContacts(QVector<MorseIdentifier>() << identifier);
}

void MorseConnection::updateContactsStatus(const QVector<MorseIdentifier> &identifiers)
{
    qDebug() << Q_FUNC_INFO;
    Tp::SimpleContactPresences newPresences;
    foreach (const MorseIdentifier &identifier, identifiers) {
        uint handle = ensureContact(identifier);

        if (handle == selfHandle()) {
            continue;
        }

        TelegramNamespace::ContactStatus st = TelegramNamespace::ContactStatusUnknown;

        if (m_core) {
            TelegramNamespace::UserInfo info;
            m_core->getUserInfo(&info, identifier.userId());

            st = info.status();
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

    newPresences[selfHandle()] = presence;
    simplePresenceIface->setPresences(newPresences);
}

void MorseConnection::setSubscriptionState(const QVector<uint> &handles, uint state)
{

}

void MorseConnection::setSubscriptionState(const QVector<MorseIdentifier> &identifiers, const QList<uint> &handles, uint state)
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
        identifiersMap[handles[i]] = identifiers[i].toString();
        m_contactsSubscription[handles[i]] = state;
    }
    Tp::HandleIdentifierMap removals;
    contactListIface->contactsChangedWithID(changes, identifiersMap, removals);
}

/* Receive message from outside (telegram server) */
void MorseConnection::whenMessageReceived(const TelegramNamespace::Message &message)
{
    bool chatMessage = message.peer().type != TelegramNamespace::Peer::User;

    uint contactHandle = ensureContact(MorseIdentifier::fromUserId(message.userId));
    uint targetHandle = ensureHandle(MorseIdentifier::fromPeer(message.peer()));
    uint initiatorHandle = 0;

    if (chatMessage) {
        initiatorHandle = contactHandle;
    } else {
        initiatorHandle = targetHandle;
    }

    //TODO: initiator should be group creator
    Tp::DBusError error;
    bool yours;

    QVariantMap request;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")] = targetHandle;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")] = chatMessage ? Tp::HandleTypeRoom : Tp::HandleTypeContact;
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

    textChannel->whenMessageReceived(message, contactHandle);
}

void MorseConnection::whenChatChanged(quint32 chatId)
{
    QVector<quint32> participants;
    if (m_core->getChatParticipants(&participants, chatId) && !participants.isEmpty()) {

        Tp::UIntList handles;
        foreach (quint32 participant, participants) {
            handles.append(ensureHandle(MorseIdentifier::fromUserId(participant)));
        }

        emit chatDetailsChanged(chatId, handles);
    }
}

void MorseConnection::whenContactListChanged()
{
    const QVector<quint32> ids = m_core->contactList();

    qDebug() << Q_FUNC_INFO << ids;

//    Tp::ContactSubscriptionMap changes;
//    Tp::HandleIdentifierMap identifiers;
//    Tp::HandleIdentifierMap removals;

    QVector<uint> handles;
    QVector<MorseIdentifier> identifiers;
    handles.reserve(ids.count());
    identifiers.reserve(ids.count());

    foreach (quint32 id, ids) {
        identifiers.append(MorseIdentifier::fromUserId(id));
        handles.append(ensureContact(identifiers.last()));
    }

    setSubscriptionState(handles, Tp::SubscriptionStateYes);
    updateContactsStatus(identifiers);

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
void MorseConnection::whenAvatarReceived(quint32 userId, const QByteArray &data, const QString &mimeType, const QString &token)
{
    avatarsIface->avatarRetrieved(ensureContact(MorseIdentifier::fromUserId(userId)), token , data, mimeType);
}

void MorseConnection::whenGotRooms()
{
    qDebug() << Q_FUNC_INFO;
    Tp::RoomInfoList rooms;

    foreach (quint32 chatId, m_core->chatList()) {
        Tp::RoomInfo roomInfo;
        TelegramNamespace::GroupChat chatInfo;

        const MorseIdentifier chatID = MorseIdentifier::fromChatId(chatId);
        roomInfo.channelType = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
        roomInfo.handle = ensureChat(chatID);
        roomInfo.info[QLatin1String("handle-name")] = chatID.toString();
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
    if (contacts.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("No handles provided"));
    }

    if (!coreIsReady()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    Tp::AvatarTokenMap result;
    foreach (quint32 handle, contacts) {
        if (!m_handles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
        }
        result.insert(handle, m_core->contactAvatarToken(m_handles.value(handle).userId()));
    }

    return result;
}

void MorseConnection::requestAvatars(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    if (contacts.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("No handles provided"));
    }

    if (!coreIsReady()) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    foreach (quint32 handle, contacts) {
        if (!m_handles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
        }
        m_core->requestContactAvatar(m_handles.value(handle).userId());
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

void MorseConnection::checkConnected()
{
    if (coreIsAuthenticated() && !m_handles.value(selfHandle()).isNull()) {
        setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);
    }
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

void MorseConnection::setContactStatus(quint32 userId, TelegramNamespace::ContactStatus status)
{
    qDebug() << "Update presence for " << userId << "to" << status;

    Tp::SimpleContactPresences newPresences;
    uint handle = ensureContact(MorseIdentifier::fromUserId(userId));

    if (handle == selfHandle()) {
        return;
    }

    Tp::SimplePresence presence;

    switch (status) {
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

    newPresences[handle] = presence;

    simplePresenceIface->setPresences(newPresences);
}

uint MorseConnection::getHandle(const MorseIdentifier &identifier) const
{
    return m_handles.key(identifier, 0);
}

uint MorseConnection::getChatHandle(const MorseIdentifier &identifier) const
{
    return m_chatHandles.key(identifier, 0);
}
