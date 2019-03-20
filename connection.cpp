/*
    This file is part of the telepathy-morse connection manager.
    Copyright (C) 2014-2016 Alexandr Akulich <akulichalexander@gmail.com>

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

#include "connection.hpp"
#include "protocol.hpp"

#include "textchannel.hpp"

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
#include "contactgroups.hpp"
#endif

#include <TelegramQt/AccountStorage>
#include <TelegramQt/AuthOperation>
#include <TelegramQt/AppInformation>
#include <TelegramQt/Client>
#include <TelegramQt/ClientSettings>
#include <TelegramQt/ConnectionApi>
#include <TelegramQt/ConnectionError>
#include <TelegramQt/ContactsApi>
#include <TelegramQt/ContactList>
#include <TelegramQt/DataStorage>
#include <TelegramQt/Debug>
#include <TelegramQt/DialogList>
#include <TelegramQt/PendingMessages>
#include <TelegramQt/MessagingApi>

#include <TelepathyQt/Constants>
#include <TelepathyQt/BaseChannel>

#include <QDebug>

#include <QStandardPaths>

#define DIALOGS_AS_CONTACTLIST
//#define BROADCAST_AS_CONTACT

#include <QDir>
#include <QFile>

#include "extras/CFileManager.hpp"

static constexpr int c_selfHandle = 1;
static const QString c_telegramAccountSubdir = QLatin1String("telepathy/morse");
static const QString c_accountFile = QLatin1String("account.bin");
static const QString c_stateFile = QLatin1String("state.json");

static const QString c_onlineSimpleStatusKey = QLatin1String("available");
static const QString c_saslMechanismTelepathyPassword = QLatin1String("X-TELEPATHY-PASSWORD");

using namespace Telegram;

Tp::AvatarSpec MorseConnection::avatarDetails()
{
    static const auto spec = Tp::AvatarSpec(/* supportedMimeTypes */ QStringList() << QLatin1String("image/jpeg"),
                                            /* minHeight */ 0, /* maxHeight */ 160, /* recommendedHeight */ 160,
                                            /* minWidth */ 0, /* maxWidth */ 160, /* recommendedWidth */ 160,
                                            /* maxBytes */ 10240);
    return spec;
}

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

#ifdef ENABLE_GROUP_CHAT
    Tp::RequestableChannelClass groupChat;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeRoom;
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    result << Tp::RequestableChannelClassSpec(groupChat);

    Tp::RequestableChannelClass chatList;
    chatList.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST;
    result << Tp::RequestableChannelClassSpec(chatList);
#endif // ENABLE_GROUP_CHAT

    return result;
}

MorseConnection::MorseConnection(const QDBusConnection &dbusConnection, const QString &cmName, const QString &protocolName, const QVariantMap &parameters) :
    Tp::BaseConnection(dbusConnection, cmName, protocolName, parameters)
{
    qDebug() << Q_FUNC_INFO;
    m_selfPhone = MorseProtocol::getAccount(parameters);
    m_serverAddress = MorseProtocol::getServerAddress(parameters);
    m_serverPort = MorseProtocol::getServerPort(parameters);
    m_serverKeyFile = MorseProtocol::getServerKey(parameters);
    m_keepAliveInterval = MorseProtocol::getKeepAliveInterval(parameters, Client::Settings::defaultPingInterval() / 1000);

    /* Connection.Interface.Contacts */
    contactsIface = Tp::BaseConnectionContactsInterface::create();
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this, &MorseConnection::getContactAttributes));
    contactsIface->setContactAttributeInterfaces({
                                                     TP_QT_IFACE_CONNECTION,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_INFO,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE,
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING,
                                                 #if 0
                                                     TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS,
                                                 #endif
                                                 });
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    /* Connection.Interface.SimplePresence */
    simplePresenceIface = Tp::BaseConnectionSimplePresenceInterface::create();
    simplePresenceIface->setStatuses(getSimpleStatusSpecMap());
    simplePresenceIface->setSetPresenceCallback(Tp::memFun(this, &MorseConnection::setPresence));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(simplePresenceIface));

    /* Connection.Interface.ContactList */
    contactListIface = Tp::BaseConnectionContactListInterface::create();
    contactListIface->setContactListPersists(true);
    contactListIface->setCanChangeContactList(true);
    contactListIface->setDownloadAtConnection(true);
    contactListIface->setGetContactListAttributesCallback(Tp::memFun(this, &MorseConnection::getContactListAttributes));
    contactListIface->setRemoveContactsCallback(Tp::memFun(this, &MorseConnection::removeContacts));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactListIface));

    /* Connection.Interface.ContactInfo */
    contactInfoIface = Tp::BaseConnectionContactInfoInterface::create();

    Tp::FieldSpec vcardSpecPhone;
    Tp::FieldSpec vcardSpecNickname;
    Tp::FieldSpec vcardSpecName;
    vcardSpecPhone.name = QLatin1String("tel");
    vcardSpecNickname.name = QLatin1String("n");
    vcardSpecName.name = QLatin1String("nickname");
    contactInfoIface->setSupportedFields(Tp::FieldSpecs()
                                         << vcardSpecPhone
                                         << vcardSpecNickname
                                         << vcardSpecName
                                         );
    contactInfoIface->setContactInfoFlags(Tp::ContactInfoFlagPush);
    contactInfoIface->setGetContactInfoCallback(Tp::memFun(this, &MorseConnection::getContactInfo));
    contactInfoIface->setRequestContactInfoCallback(Tp::memFun(this, &MorseConnection::requestContactInfo));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactInfoIface));

    /* Connection.Interface.Aliasing */
    aliasingIface = Tp::BaseConnectionAliasingInterface::create();
    aliasingIface->setGetAliasesCallback(Tp::memFun(this, &MorseConnection::getAliases));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(aliasingIface));

#if 0
    /* Connection.Interface.Avatars */
    avatarsIface = Tp::BaseConnectionAvatarsInterface::create();
    avatarsIface->setAvatarDetails(avatarDetails());
    avatarsIface->setGetKnownAvatarTokensCallback(Tp::memFun(this, &MorseConnection::getKnownAvatarTokens));
    avatarsIface->setRequestAvatarsCallback(Tp::memFun(this, &MorseConnection::requestAvatars));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(avatarsIface));
#endif

#ifdef ENABLE_GROUP_CHAT
# ifdef USE_BUNDLED_GROUPS_IFACE
    ConnectionContactGroupsInterfacePtr groupsIface = ConnectionContactGroupsInterface::create();
# else
    Tp::BaseConnectionContactGroupsInterfacePtr groupsIface = Tp::BaseConnectionContactGroupsInterface::create();
# endif
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(groupsIface));
#endif

    /* Connection.Interface.Requests */
    requestsIface = Tp::BaseConnectionRequestsInterface::create(this);
    requestsIface->requestableChannelClasses = getRequestableChannelList().bareClasses();

    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    setConnectCallback(Tp::memFun(this, &MorseConnection::doConnect));
    setInspectHandlesCallback(Tp::memFun(this, &MorseConnection::inspectHandles));
    setCreateChannelCallback(Tp::memFun(this, &MorseConnection::createChannelCB));
    setRequestHandlesCallback(Tp::memFun(this, &MorseConnection::requestHandles));

    connect(this, &BaseConnection::disconnected, this, &MorseConnection::onDisconnected);

    m_contactHandles.insert(c_selfHandle, Telegram::Peer());
    setSelfHandle(c_selfHandle);

    m_appInfo = new Client::AppInformation(this);
    m_appInfo->setAppId(14617);
    m_appInfo->setAppHash(QLatin1String("e17ac360fd072f83d5d08db45ce9a121"));
    m_appInfo->setAppVersion(QLatin1String("0.2"));
    m_appInfo->setDeviceInfo(QLatin1String("pc"));
    m_appInfo->setOsInfo(QLatin1String("GNU/Linux"));
    m_appInfo->setLanguageCode(QLocale::system().bcp47Name());

    m_client = new Client::Client(this);

    Client::FileAccountStorage *accountStorage = new Client::FileAccountStorage(m_client);
    accountStorage->setPhoneNumber(m_selfPhone);
    accountStorage->setAccountIdentifier(m_selfPhone);
    accountStorage->setFileName(getAccountDataDirectory() + QLatin1Char('/') + c_accountFile);

    Client::Settings *clientSettings = new Client::Settings(m_client);
    m_dataStorage = new Client::InMemoryDataStorage(m_client);
    m_client->setSettings(clientSettings);
    m_client->setAccountStorage(accountStorage);
    m_client->setDataStorage(m_dataStorage);

    if (!m_serverAddress.isEmpty()) {
        if ((m_serverPort == 0) || (m_serverKeyFile.isEmpty())) {
            qCritical() << "Invalid server configuration!";
        }
        RsaKey key = RsaKey::fromFile(m_serverKeyFile);
        if (!key.isValid()) {
            qCritical() << "Unable to read server key!";
        }
        DcOption customServer;
        customServer.address = m_serverAddress;
        customServer.port = m_serverPort;
        clientSettings->setServerConfiguration({customServer});
        clientSettings->setServerRsaKey(key);
    }

    clientSettings->setPingInterval(m_keepAliveInterval * 1000);
    m_client->setAppInformation(m_appInfo);

    connect(m_client->connectionApi(), &Telegram::Client::ConnectionApi::statusChanged,
            this, &MorseConnection::onConnectionStatusChanged);
    connect(m_client->messagingApi(), &Telegram::Client::MessagingApi::messageReceived,
             this, &MorseConnection::onNewMessageReceived);
//    connect(m_core, &CTelegramCore::chatChanged,
//            this, &MorseConnection::whenChatChanged);
//    connect(m_core, &CTelegramCore::contactStatusChanged,
//            this, &MorseConnection::setContactStatus);

    const QString proxyType = MorseProtocol::getProxyType(parameters);
    if (!proxyType.isEmpty()) {
        if (proxyType == QLatin1String("socks5")) {
            const QString proxyServer = MorseProtocol::getProxyAddress(parameters);
            const quint16 proxyPort = MorseProtocol::getProxyPort(parameters);
            const QString proxyUsername = MorseProtocol::getProxyUsername(parameters);
            const QString proxyPassword = MorseProtocol::getProxyPassword(parameters);
            if (proxyServer.isEmpty() || proxyPort == 0) {
                qWarning() << "Invalid proxy configuration, ignored";
            } else {
                qDebug() << Q_FUNC_INFO << "Set proxy";
                QNetworkProxy proxy;
                proxy.setType(QNetworkProxy::Socks5Proxy);
                proxy.setHostName(proxyServer);
                proxy.setPort(proxyPort);
                proxy.setUser(proxyUsername);
                proxy.setPassword(proxyPassword);
                clientSettings->setProxy(proxy);
            }
        } else {
            qWarning() << "Unknown proxy type" << proxyType << ", ignored.";
        }
    }
    m_fileManager = new CFileManager(m_client, this);
    connect(m_fileManager, &CFileManager::requestComplete, this, &MorseConnection::onFileRequestCompleted);
}

void MorseConnection::doConnect(Tp::DBusError *error)
{
    Q_UNUSED(error);

    m_authReconnectionsCount = 0;
    setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonRequested);

    if (m_client->accountStorage()->loadData() && m_client->accountStorage()->hasMinimalDataSet()) {
        Telegram::Client::AuthOperation *checkInOperation = m_client->connectionApi()->checkIn();
        checkInOperation->connectToFinished(this, &MorseConnection::onCheckInFinished, checkInOperation);
    } else {
        signInOrUp();
    }
}

void MorseConnection::signInOrUp()
{
    m_signOperation = m_client->connectionApi()->startAuthentication();
    m_signOperation->setPhoneNumber(m_client->accountStorage()->phoneNumber());

    connect(m_signOperation, &Client::AuthOperation::authCodeRequired,
            this, &MorseConnection::onAuthCodeRequired);
    connect(m_signOperation, &Client::AuthOperation::errorOccurred,
            this, &MorseConnection::onAuthErrorOccurred);
    connect(m_signOperation, &Client::AuthOperation::passwordRequired,
            this, &MorseConnection::onPasswordRequired);
    connect(m_signOperation, &Client::AuthOperation::passwordCheckFailed,
            this, &MorseConnection::onPasswordCheckFailed);
    connect(m_signOperation, &PendingOperation::finished,
            this, &MorseConnection::onSignInFinished);
}

void MorseConnection::onConnectionStatusChanged(Client::ConnectionApi::Status status,
                                                Client::ConnectionApi::StatusReason reason)
{
    qDebug() << Q_FUNC_INFO << status << reason;
    switch (status) {
    case Client::ConnectionApi::StatusConnected:
        onAuthenticated();
        break;
    case Client::ConnectionApi::StatusReady:
        onConnectionReady();
        updateSelfContactState(Tp::ConnectionStatusConnected);
        break;
    case Client::ConnectionApi::StatusDisconnected:
        if (reason == Client::ConnectionApi::StatusReasonLocal) {
            // Requested from adaptee, no signal needed.
            setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonRequested);
        } else {
            // There is not other reason to disconnect, is there?
            // setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonNetworkError);
            // updateSelfContactState(Tp::ConnectionStatusDisconnected);
            // emit disconnected();
        }
        break;
    default:
        break;
    }
}

void MorseConnection::onAuthenticated()
{
    qDebug() << Q_FUNC_INFO;

    if (!saslIface_authCode.isNull()) {
        saslIface_authCode->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    if (!saslIface_password.isNull()) {
        saslIface_password->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    contactListIface->setContactListState(Tp::ContactListStateWaiting);
}

void MorseConnection::onSelfUserAvailable()
{
    qDebug() << Q_FUNC_INFO;

    const Telegram::Peer selfIdentifier = Telegram::Peer::fromUserId(m_client->contactsApi()->selfContactId());
    if (!selfIdentifier.isValid()) {
        qCritical() << Q_FUNC_INFO << "Self id unexpectedly not available";
        return;
    }

    m_contactHandles.insert(c_selfHandle, selfIdentifier);
    setSelfContact(c_selfHandle, selfIdentifier.toString());
}

void MorseConnection::onAuthCodeRequired()
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

void MorseConnection::onAuthErrorOccurred(TelegramNamespace::AuthenticationError errorCode,
                                          const QByteArray &errorMessage)
{
    QVariantMap details;
    switch (errorCode) {
    case TelegramNamespace::AuthenticationErrorPhoneCodeExpired:
        details[QLatin1String("server-message")] = QStringLiteral("Auth code expired");
        break;
    case TelegramNamespace::AuthenticationErrorPhoneCodeInvalid:
        details[QLatin1String("server-message")] = QStringLiteral("Invalid auth code");
        break;
    default:
        details[QLatin1String("server-message")] = QStringLiteral("Unexpected error: ") + QString::fromLatin1(errorMessage);
        break;
    }
    if (!saslIface_authCode.isNull()) {
        saslIface_authCode->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, details);
    }
}

void MorseConnection::onPasswordRequired()
{
    qDebug() << Q_FUNC_INFO;
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

    Tp::DBusError error;
    baseChannel->registerObject(&error);

    if (error.isValid()) {
        qDebug() << Q_FUNC_INFO << error.name() << error.message();
    } else {
        addChannel(baseChannel);
    }
}

void MorseConnection::onPasswordCheckFailed()
{
    QVariantMap details;
    details[QLatin1String("server-message")] = QStringLiteral("Invalid password");

    if (!saslIface_password.isNull()) {
        saslIface_password->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, details);
    }
}

void MorseConnection::onSignInFinished()
{
    qDebug() << Q_FUNC_INFO << m_signOperation->errorDetails();
}

void MorseConnection::onCheckInFinished(Client::AuthOperation *checkInOperation)
{
    qDebug() << Q_FUNC_INFO << checkInOperation->errorDetails();
    if (!checkInOperation->isSucceeded()) {
        signInOrUp();
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
    m_signOperation->submitAuthCode(QString::fromLatin1(data));
}

void MorseConnection::startMechanismWithData_password(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << mechanism << data;
    if (!saslIface_password->availableMechanisms().contains(mechanism)) {
        error->set(TP_QT_ERROR_NOT_IMPLEMENTED, QStringLiteral("Given SASL mechanism \"%1\" is not implemented").arg(mechanism));
        return;
    }

    saslIface_password->setSaslStatus(Tp::SASLStatusInProgress, QLatin1String("InProgress"), QVariantMap());
    m_signOperation->submitPassword(QString::fromUtf8(data));
}

void MorseConnection::onConnectionReady()
{
    qDebug() << Q_FUNC_INFO;
    //m_core->setOnlineStatus(m_wantedPresence == c_onlineSimpleStatusKey);
    //m_core->setMessageReceivingFilter(TelegramNamespace::MessageFlagNone);

#ifdef DIALOGS_AS_CONTACTLIST
    if (m_dialogs) {
        onDialogsReady();
    } else {
        m_dialogs = m_client->messagingApi()->getDialogList();
        connect(m_dialogs->becomeReady(), &PendingOperation::finished, this, &MorseConnection::onDialogsReady);
    }
#else
    if (m_contacts) {
        onContactListChanged();
    } else {
        m_contacts = m_client->contactsApi()->getContactList();
        connect(m_contacts->becomeReady(), &PendingOperation::finished, this, &MorseConnection::onContactListChanged);
    }
#endif

    onSelfUserAvailable();

    setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);
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
    }

    QStringList result;

    const QMap<uint, Telegram::Peer> handlesContainer = handleType == Tp::HandleTypeContact ? m_contactHandles : m_chatHandles;

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
    Telegram::Peer targetID;

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = m_contactHandles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = Telegram::Peer::fromString(request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString());
            targetHandle = ensureHandle(targetID);
        }
        break;
    case Tp::HandleTypeRoom:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = m_chatHandles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = Telegram::Peer::fromString(request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString());
            targetHandle = ensureHandle(targetID);
        }
        break;
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
#ifdef ENABLE_GROUP_CHAT
    case Tp::HandleTypeRoom:
#endif
        break;
    default:
        if (error) {
            error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Unknown target handle type"));
        }
        return Tp::BaseChannelPtr();
    }

    if (!targetHandle
            || ((targetHandleType == Tp::HandleTypeContact) && !m_contactHandles.contains(targetHandle))
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
        MorseTextChannelPtr textChannel = MorseTextChannel::create(this, baseChannel.data());
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(textChannel));

        if (targetHandleType == Tp::HandleTypeRoom) {
            connect(this, &MorseConnection::chatDetailsChanged,
                    textChannel.data(), &MorseTextChannel::onChatDetailsChanged);

            // onChatChanged(targetID.id);
        }
    }

    return baseChannel;
}

Tp::UIntList MorseConnection::requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << identifiers;

    if (handleType != Tp::HandleTypeContact) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("MorseConnection::requestHandles - Handle Type unknown"));
        return Tp::UIntList();
    }

    Tp::UIntList result;
    for(const QString &identify : identifiers) {
        const Telegram::Peer id = Telegram::Peer::fromString(identify);
        if (!id.isValid()) {
            error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("MorseConnection::requestHandles - invalid identifier"));
            return Tp::UIntList();
        }
        result.append(ensureContact(id));
    }

    return result;
}

Tp::ContactAttributesMap MorseConnection::getContactListAttributes(const QStringList &interfaces, bool /* hold */, Tp::DBusError *error)
{
    return getContactAttributes(m_contactList.toList(), interfaces, error);
}

Tp::ContactAttributesMap MorseConnection::getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error)
{
//    http://telepathy.freedesktop.org/spec/Connection_Interface_Contacts.html#Method:GetContactAttributes
//    qDebug() << Q_FUNC_INFO << handles << interfaces;

    Tp::ContactAttributesMap contactAttributes;

    foreach (const uint handle, handles) {
        if (m_contactHandles.contains(handle)) {
            QVariantMap attributes;
            const Telegram::Peer identifier = m_contactHandles.value(handle);
            if (!identifier.isValid()) {
                qWarning() << Q_FUNC_INFO << "Handle is in map, but identifier is not valid";
                continue;
            }
            attributes[TP_QT_IFACE_CONNECTION + QLatin1String("/contact-id")] = identifier.toString();

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/subscribe")] = m_contactsSubscription.value(handle, Tp::SubscriptionStateUnknown);
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST + QLatin1String("/publish")] = m_contactsSubscription.value(handle, Tp::SubscriptionStateUnknown);
            }

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE + QLatin1String("/presence")] = QVariant::fromValue(getPresence(handle));
            }

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING + QLatin1String("/alias")] = QVariant::fromValue(getAlias(identifier));
            }

            //if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS)) {
            //    attributes[TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS + QLatin1String("/token")] = QVariant::fromValue(m_core->peerPictureToken(identifier));
            //}

            if (interfaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_INFO)) {
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_INFO + QLatin1String("/info")] = QVariant::fromValue(getUserInfo(identifier.id));
            }

            contactAttributes[handle] = attributes;
        }
    }
    return contactAttributes;
}

void MorseConnection::removeContacts(const Tp::UIntList &handles, Tp::DBusError *error)
{
    if (handles.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid argument (no handles provided)"));
    }

    if (status() != Tp::ConnectionStatusConnected) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    QVector<quint32> ids;

    foreach (uint handle, handles) {
        if (!m_contactHandles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Unknown handle"));
            return;
        }

        quint32 userId = m_contactHandles.value(handle).id;

        if (!userId) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Internal error (invalid handle)"));
        }

        ids.append(userId);
    }

    m_client->contactsApi()->deleteContacts(ids);
}

Tp::ContactInfoFieldList MorseConnection::requestContactInfo(uint handle, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << handle;

    if (!m_contactHandles.contains(handle)) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle"));
        return Tp::ContactInfoFieldList();
    }
    Telegram::Peer identifier = m_contactHandles.value(handle);
    if (!identifier.isValid()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid morse identifier"));
        return Tp::ContactInfoFieldList();
    }

    return getUserInfo(identifier.id);
}

Tp::ContactInfoFieldList MorseConnection::getUserInfo(const quint32 userId) const
{
    Telegram::UserInfo userInfo;
    if (!m_client->dataStorage()->getUserInfo(&userInfo, userId)) {
        return Tp::ContactInfoFieldList();
    }

    Tp::ContactInfoFieldList contactInfo;
    if (!userInfo.userName().isEmpty()) {
        Tp::ContactInfoField contactInfoField;
        contactInfoField.fieldName = QLatin1String("nickname");
        contactInfoField.fieldValue.append(userInfo.userName());
        contactInfo << contactInfoField;
    }
    if (!userInfo.phone().isEmpty()) {
        Tp::ContactInfoField contactInfoField;
        contactInfoField.fieldName = QLatin1String("tel");
        QString phone = userInfo.phone();
        if (!phone.startsWith(QLatin1Char('+'))) {
            phone.prepend(QLatin1Char('+'));
        }
        contactInfoField.parameters.append(QLatin1String("type=text"));
        contactInfoField.parameters.append(QLatin1String("type=cell"));
        contactInfoField.fieldValue.append(phone);
        contactInfo << contactInfoField;
    }

    QString name = userInfo.firstName() + QLatin1Char(' ') + userInfo.lastName();
    name = name.simplified();
    if (!name.isEmpty()) {
        Tp::ContactInfoField contactInfoField;
        contactInfoField.fieldName = QLatin1String("fn"); // Formatted name
        contactInfoField.fieldValue.append(name);
        contactInfo << contactInfoField;
    }
    {
        Tp::ContactInfoField contactInfoField;
        contactInfoField.fieldName = QLatin1String("n");
        contactInfoField.fieldValue.append(userInfo.lastName()); // "Surname"
        contactInfoField.fieldValue.append(userInfo.firstName()); // "Given"
        contactInfoField.fieldValue.append(QString()); // Additional
        contactInfoField.fieldValue.append(QString()); // Prefix
        contactInfoField.fieldValue.append(QString()); // Suffix
        contactInfo << contactInfoField;
    }

    return contactInfo;
}

Tp::ContactInfoMap MorseConnection::getContactInfo(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << contacts;

    if (contacts.isEmpty()) {
        return Tp::ContactInfoMap();
    }

    Tp::ContactInfoMap result;

    foreach (uint handle, contacts) {
        const Tp::ContactInfoFieldList contactInfo = requestContactInfo(handle, error);
        if (!contactInfo.isEmpty()) {
            result.insert(handle, contactInfo);
        }
    }

    return result;
}

Tp::AliasMap MorseConnection::getAliases(const Tp::UIntList &handles, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << handles;

    Tp::AliasMap aliases;

    foreach (uint handle, handles) {
        aliases[handle] = getContactAlias(handle);
    }

    return aliases;
}

QString MorseConnection::getContactAlias(uint handle)
{
    return getAlias(m_contactHandles.value(handle));
}

QString MorseConnection::getAlias(const Telegram::Peer identifier)
{
    if (!identifier.isValid()) {
        return QString();
    }
    if (identifier.type == Telegram::Peer::User) {
        Telegram::UserInfo info;
        if (m_client->dataStorage()->getUserInfo(&info, identifier.id)) {
            if (!info.firstName().isEmpty() || !info.lastName().isEmpty()) {
                if (!info.firstName().isEmpty() && !info.lastName().isEmpty()) {
                    return info.firstName() + QLatin1Char(' ') + info.lastName();
                }
                return !info.firstName().isEmpty() ? info.firstName() : info.lastName();
            }
            return info.userName();
        }
    } else {
        Telegram::ChatInfo info;
        if (m_client->dataStorage()->getChatInfo(&info, identifier)) {
            return info.title();
        }
    }

    return QString();
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

    if (m_client->connectionApi()->isSignedIn()) {
        //m_core->setOnlineStatus(status == c_onlineSimpleStatusKey);
    }

    return c_selfHandle;
}

uint MorseConnection::ensureHandle(const Telegram::Peer &identifier)
{
    if (peerIsRoom(identifier)) {
        return ensureChat(identifier);
    } else {
        return ensureContact(identifier);
    }
}

uint MorseConnection::ensureContact(quint32 userId)
{
    return ensureContact(Telegram::Peer::fromUserId(userId));
}

uint MorseConnection::ensureContact(const Telegram::Peer &identifier)
{
    uint handle = getContactHandle(identifier);
    if (!handle) {
        handle = addContacts( {identifier});
    }
    return handle;
}

uint MorseConnection::ensureChat(const Telegram::Peer &identifier)
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

/**
 * Add contacts with identifiers \a identifiers to known contacts list (not roster)
 *
 * \return the maximum handle value
 */
uint MorseConnection::addContacts(const QVector<Telegram::Peer> &identifiers)
{
    qDebug() << Q_FUNC_INFO;
    uint handle = 0;

    if (!m_contactHandles.isEmpty()) {
        handle = m_contactHandles.keys().last();
    }

    QList<uint> newHandles;
    QVector<Telegram::Peer> newIdentifiers;
    for (const Telegram::Peer &identifier : identifiers) {
        if (getContactHandle(identifier)) {
            continue;
        }

        ++handle;
        m_contactHandles.insert(handle, identifier);
        newHandles << handle;
        newIdentifiers << identifier;
    }

    return handle;
}

void MorseConnection::updateContactsPresence(const QVector<Telegram::Peer> &identifiers)
{
    qDebug() << Q_FUNC_INFO;
    Tp::SimpleContactPresences newPresences;
    for (const Telegram::Peer &identifier : identifiers) {
        uint handle = ensureContact(identifier);

        if (handle == selfHandle()) {
            continue;
        }

        TelegramNamespace::ContactStatus st = TelegramNamespace::ContactStatusOnline;

        if (m_client) {
            // We list broadcast channels as Contacts
            if (identifier.type == Telegram::Peer::User) {
                Telegram::UserInfo info;
                m_client->dataStorage()->getUserInfo(&info, identifier.id);
                st = info.status();
            }
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
        case TelegramNamespace::ContactStatusUnknown:
            presence.status = QLatin1String("unknown");
            presence.type = Tp::ConnectionPresenceTypeUnknown;
            break;
        }

        presence.status = QLatin1String("available");
        presence.type = Tp::ConnectionPresenceTypeAvailable;
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

void MorseConnection::setSubscriptionState(const QVector<Telegram::Peer> &identifiers, const QVector<uint> &handles, uint state)
{
    qDebug() << Q_FUNC_INFO;
    if (identifiers.isEmpty()) {
        return;
    }
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
void MorseConnection::onNewMessageReceived(const Peer peer, quint32 messageId)
{
    addMessages(peer, {messageId});
}

void MorseConnection::addMessages(const Peer peer, const QVector<quint32> &messageIds)
{
    bool groupChatMessage = peerIsRoom(peer);

    if (groupChatMessage) {
        return;
    }

    uint targetHandle = ensureHandle(peer);

    QVector<quint32> newIds = messageIds;
    if (newIds.isEmpty()) {
        return;
    }

    //TODO: initiator should be group creator
    Tp::DBusError error;
    bool yours;

    QVariantMap request;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")] = targetHandle;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")] = groupChatMessage ? Tp::HandleTypeRoom : Tp::HandleTypeContact;
    Tp::BaseChannelPtr channel = ensureChannel(request, yours, /* suppressHandler */ false, &error);

    if (error.isValid()) {
        qWarning() << Q_FUNC_INFO << "ensureChannel failed:" << error.name() << " " << error.message();
        return;
    }

    MorseTextChannelPtr textChannel = MorseTextChannelPtr::dynamicCast(channel->interface(TP_QT_IFACE_CHANNEL_TYPE_TEXT));

    if (!textChannel) {
        qCritical() << Q_FUNC_INFO << "Error, channel is not a morseTextChannel?";
        return;
    }

    for (const quint32 messageId : newIds) {
        Telegram::Message message;
        m_client->dataStorage()->getMessage(&message, peer, messageId);
        textChannel->onMessageReceived(message);
    }
}

void MorseConnection::updateContactList()
{
    if (m_client->connectionApi()->status() != Client::ConnectionApi::StatusReady) {
        return;
    }
#ifdef DIALOGS_AS_CONTACTLIST
    const QVector<Telegram::Peer> ids = m_dialogs->peers();
#else
    const QVector<Telegram::Peer> ids = m_contacts->peers();
#endif

    qDebug() << this << __func__ << "ids:" << ids;

    QVector<uint> newContactListHandles;
    QVector<Telegram::Peer> newContactListIdentifiers;
    newContactListHandles.reserve(ids.count());
    newContactListIdentifiers.reserve(ids.count());

    for (const Telegram::Peer &peer : ids) {
        if (peerIsRoom(peer)) {
            continue;
        }
        Telegram::UserInfo info;
        if (peer.type == Telegram::Peer::User) {
            m_client->dataStorage()->getUserInfo(&info, peer.id);
            if (info.isDeleted()) {
                qDebug() << this << __func__ << "skip deleted user id" << peer.id;
                continue;
            }
        }
        newContactListIdentifiers.append(peer);
        newContactListHandles.append(ensureContact(newContactListIdentifiers.last()));
    }

    Tp::HandleIdentifierMap removals;
    foreach (uint handle, m_contactList) {
        if (newContactListHandles.contains(handle)) {
            continue;
        }
        const Telegram::Peer identifier = m_contactHandles.value(handle);
        if (!identifier.isValid()) {
            qWarning() << this << __func__ << "Internal corruption. Handle" << handle << "has invalid corresponding identifier";
        }
        removals.insert(handle, identifier.toString());
    }

    m_contactList = newContactListHandles;

    qDebug() << this << __func__ << "new:" << newContactListIdentifiers;
    qDebug() << this << __func__ << "removals:" << removals;
    Tp::ContactSubscriptionMap changes;
    Tp::HandleIdentifierMap identifiersMap;

    for (int i = 0; i < newContactListIdentifiers.size(); ++i) {
        Tp::ContactSubscriptions change;
        change.publish = Tp::SubscriptionStateYes;
        change.subscribe = Tp::SubscriptionStateYes;
        changes[newContactListHandles[i]] = change;
        identifiersMap[newContactListHandles[i]] = newContactListIdentifiers.at(i).toString();
        m_contactsSubscription[newContactListHandles[i]] = Tp::SubscriptionStateYes;
    }

    contactListIface->contactsChangedWithID(changes, identifiersMap, removals);

    updateContactsPresence(newContactListIdentifiers);

    contactListIface->setContactListState(Tp::ContactListStateSuccess);
}

void MorseConnection::onDialogsReady()
{
    updateContactList();
}

void MorseConnection::onDisconnected()
{
    qDebug() << Q_FUNC_INFO;
    m_client->connectionApi()->disconnectFromServer();
}

void MorseConnection::onFileRequestCompleted(const QString &uniqueId)
{
    qDebug() << Q_FUNC_INFO << uniqueId;
    if (m_peerPictureRequests.contains(uniqueId)) {
        const Telegram::Peer peer = m_peerPictureRequests.value(uniqueId);
        if (!peerIsRoom(peer)) {
            const FileInfo *fileInfo = m_fileManager->getFileInfo(uniqueId);
            avatarsIface->avatarRetrieved(peer.id, uniqueId, fileInfo->data(), fileInfo->mimeType());
        } else {
            qWarning() << "MorseConnection::onFileRequestCompleted(): Ignore room picture";
        }
    } else {
        qWarning() << "MorseConnection::onFileRequestCompleted(): Unexpected file id";
    }
}

void MorseConnection::onGotRooms()
{
    qDebug() << Q_FUNC_INFO;
    Tp::RoomInfoList rooms;

    const QVector<Telegram::Peer> dialogs = m_client->dataStorage()->dialogs();
    for(const Telegram::Peer peer : dialogs) {
        if (!peerIsRoom(peer)) {
            continue;
        }
        Telegram::ChatInfo chatInfo;
        if (!m_client->dataStorage()->getChatInfo(&chatInfo, peer)) {
            continue;
        }
        if (chatInfo.migratedTo().isValid()) {
            continue;
        }
        const Telegram::Peer chatID = peer;
        Tp::RoomInfo roomInfo;
        roomInfo.channelType = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
        roomInfo.handle = ensureChat(chatID);
        roomInfo.info[QLatin1String("handle-name")] = chatID.toString();
        roomInfo.info[QLatin1String("members-only")] = true;
        roomInfo.info[QLatin1String("invite-only")] = true;
        roomInfo.info[QLatin1String("password")] = false;
        roomInfo.info[QLatin1String("name")] = chatInfo.title();
        roomInfo.info[QLatin1String("members")] = chatInfo.participantsCount();
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

    if (status() != Tp::ConnectionStatusConnected) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    Tp::AvatarTokenMap result;
    foreach (quint32 handle, contacts) {
        if (!m_contactHandles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
        }
        //result.insert(handle, m_core->peerPictureToken(m_handles.value(handle)));
    }

    return result;
}

void MorseConnection::requestAvatars(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    if (contacts.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("No handles provided"));
    }

    if (status() != Tp::ConnectionStatusConnected) {
        error->set(TP_QT_ERROR_DISCONNECTED, QLatin1String("Disconnected"));
    }

    foreach (quint32 handle, contacts) {
        if (!m_contactHandles.contains(handle)) {
            error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
        }
        const Telegram::Peer peer = m_contactHandles.value(handle);
        Telegram::RemoteFile pictureFile;
        m_fileManager->getPeerPictureFileInfo(peer, &pictureFile);
        const QString requestId = pictureFile.getUniqueId();
        const FileInfo *fileInfo = m_fileManager->getFileInfo(requestId);
        if (fileInfo && fileInfo->isComplete()) {
            const QByteArray data = m_fileManager->getData(requestId);
            if (!data.isEmpty()) {
                // I don't see an easy way to delay the invocation; emit the signal synchronously for now. Should not be a problem for a good client.
                avatarsIface->avatarRetrieved(handle, requestId, data, fileInfo->mimeType());
            }
            continue;
        }
        const QString newRequestId = m_fileManager->requestFile(pictureFile);
        if (newRequestId != requestId) {
            qWarning() << "Unexpected request id!" << newRequestId << "(expected:" << requestId;
        }
        m_peerPictureRequests.insert(newRequestId, peer);
    }
}

void MorseConnection::roomListStartListing(Tp::DBusError *error)
{
    Q_UNUSED(error)

    QTimer::singleShot(0, this, SLOT(onGotRooms()));
    roomListChannel->setListingRooms(true);
}

void MorseConnection::roomListStopListing(Tp::DBusError *error)
{
    Q_UNUSED(error)
    roomListChannel->setListingRooms(false);
}

QString MorseConnection::getAccountDataDirectory() const
{
    const QString serverIdentifier = m_serverAddress.isEmpty() ? QStringLiteral("official") : m_serverAddress;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QLatin1Char('/') + c_telegramAccountSubdir
            + QLatin1Char('/') + serverIdentifier
            + QLatin1Char('/') + m_selfPhone;
}

bool MorseConnection::peerIsRoom(const Telegram::Peer peer) const
{
    if (peer.type == Telegram::Peer::User) {
        return false;
    }
#ifdef BROADCAST_AS_CONTACT
    if (peer.type == Telegram::Peer::Channel) {
        Telegram::ChatInfo info;
        if (m_client->dataStorage()->getChatInfo(&info, peer)) {
            if (info.broadcast()) {
                return false;
            }
        }
    }
#endif
    return true;
}

uint MorseConnection::getContactHandle(const Telegram::Peer &identifier) const
{
    return m_contactHandles.key(identifier, 0);
}

uint MorseConnection::getChatHandle(const Telegram::Peer &identifier) const
{
    return m_chatHandles.key(identifier, 0);
}
