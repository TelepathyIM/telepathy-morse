/*
    Copyright (C) 2019 Alexandr Akulich <akulichalexander@gmail.com>

    This file is part of the telepathy-morse connection manager.

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

#include <QObject>

//#include "AccountStorage.hpp"
//#include "Client.hpp"
//#include "ClientSettings.hpp"
//#include "ConnectionApi.hpp"
//#include "ConnectionError.hpp"
//#include "ContactsApi.hpp"
//#include "DataStorage.hpp"
//#include "Utils.hpp"
//#include "TelegramNamespace.hpp"
//#include "CAppInformation.hpp"
//#include "CRawStream.hpp"

//#include "Operations/ClientAuthOperation.hpp"

//#include "ContactList.hpp"
//#include "ContactsApi.hpp"
//#include "DefaultAuthorizationProvider.hpp"
//#include "TelegramServer.hpp"
//#include "RemoteClientConnection.hpp"
//#include "TelegramServerUser.hpp"
//#include "ServerRpcLayer.hpp"
//#include "Session.hpp"
//#include "DcConfiguration.hpp"
//#include "LocalCluster.hpp"

//#include <QLoggingCategory>
//#include <QTest>
//#include <QSignalSpy>
//#include <QRegularExpression>
//#include <QTemporaryFile>

// Telegram test utils
#include "keys_data.hpp"
#include "TestAuthProvider.hpp"
#include "TestClientUtils.hpp"
#include "TestServerUtils.hpp"
#include "TestUserData.hpp"
#include "TestUtils.hpp"

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/Connection>
#include <TelepathyQt/ContactFactory>
#include <TelepathyQt/ChannelFactory>
#include <TelepathyQt/Constants>
#include <TelepathyQt/PendingReady>
#include <TelepathyQt/Debug>

// Morse headers
#include "protocol.hpp"
#include "connection.hpp"

static const QLatin1String c_account = QLatin1String("account");
static const QLatin1String c_serverAddress = QLatin1String("server-address");
static const QLatin1String c_serverPort = QLatin1String("server-port");
static const QLatin1String c_serverKey = QLatin1String("server-key");
static const QLatin1String c_proxyType = QLatin1String("proxy-type");
static const QLatin1String c_proxyAddress = QLatin1String("proxy-address");
static const QLatin1String c_proxyPort = QLatin1String("proxy-port");
static const QLatin1String c_proxyUsername= QLatin1String("proxy-username");
static const QLatin1String c_proxyPassword = QLatin1String("proxy-password");
static const QLatin1String c_keepalive = QLatin1String("keepalive");
static const QLatin1String c_keepaliveInterval = QLatin1String("keepalive-interval");

static const QLatin1String c_accountName = QLatin1String("morse-test-account");
static const QLatin1String c_protocolName = QLatin1String("telegram");
static const QLatin1String c_managerName = QLatin1String("morse");

#define TP_CHECK_ERROR(statement) \
    if (statement.isValid()) { \
        qWarning() << statement.name() << statement.message(); \
    } \
    QVERIFY(!statement.isValid())

class tst_morse : public QObject
{
    Q_OBJECT
public:
    explicit tst_morse(QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testEverything();
};

tst_morse::tst_morse(QObject *parent) :
    QObject(parent)
{
}

void tst_morse::initTestCase()
{
    qRegisterMetaType<UserData>();
    Telegram::initialize();
    QVERIFY(TestKeyData::initKeyFiles());

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);
}

void tst_morse::cleanupTestCase()
{
    QVERIFY(TestKeyData::cleanupKeyFiles());
}

void tst_morse::testEverything()
{
    const Telegram::DcOption clientDcOption = c_localDcOptions.first();
    const Telegram::RsaKey publicKey = Telegram::RsaKey::fromFile(TestKeyData::publicKeyFileName());
    const Telegram::RsaKey privateKey = Telegram::RsaKey::fromFile(TestKeyData::privateKeyFileName());
    QVERIFY(publicKey.isValid() && privateKey.isPrivate()); // Sanity check

    // Prepare server
    Telegram::Test::AuthProvider authProvider;
    Telegram::Server::LocalCluster cluster;
    cluster.setAuthorizationProvider(&authProvider);
    cluster.setServerPrivateRsaKey(privateKey);
    cluster.setServerConfiguration(c_localDcConfiguration);
    QVERIFY(cluster.start());
    // QSignalSpy serverAuthCodeSpy(&authProvider, &Telegram::Test::AuthProvider::codeSent);

    MorseProtocolPtr proto = Tp::BaseProtocol::create<MorseProtocol>(c_protocolName);
    proto->setManagerName(c_managerName);
    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(c_managerName);

    QVERIFY(cm->addProtocol(proto));

    Tp::DBusError tpError;
    cm->registerObject(&tpError);
    TP_CHECK_ERROR(tpError);

    QVariantMap parameters = {
        { c_account, c_accountName },
        { c_serverAddress, clientDcOption.address },
        { c_serverPort, clientDcOption.port },
        { c_serverKey, TestKeyData::publicKeyFileName() },
    };

    MorseConnectionPtr svcConnection = MorseConnectionPtr::dynamicCast(proto->createConnection(parameters, &tpError));
    TP_CHECK_ERROR(tpError);

    QVERIFY(svcConnection);
    svcConnection->registerObject(&tpError);
    TP_CHECK_ERROR(tpError);

    Tp::ContactFactoryConstPtr contactFactory = Tp::ContactFactory::create(Tp::Features()
                                                  << Tp::Contact::FeatureAlias
                                                  << Tp::Contact::FeatureSimplePresence
                                                  << Tp::Contact::FeatureCapabilities
                                                  << Tp::Contact::FeatureClientTypes
                                                  << Tp::Contact::FeatureAvatarData);

    Tp::ChannelFactoryConstPtr channelFactory = Tp::ChannelFactory::create(QDBusConnection::sessionBus());

    QDBusConnection bus = QDBusConnection::sessionBus();

    Tp::ConnectionPtr cliConnection = Tp::Connection::create(QDBusConnection::sessionBus(), svcConnection->busName(), svcConnection->objectPath(), channelFactory, contactFactory);
    Tp::PendingReady *ready = cliConnection->becomeReady();
    QTRY_VERIFY(ready->isFinished());
    QVERIFY(ready->isValid());

    svcConnection->doConnect(&tpError);
    TP_CHECK_ERROR(tpError);

    QDBusInterface dbusIface(svcConnection->busName(), svcConnection->objectPath(), TP_QT_IFACE_CONNECTION_INTERFACE_REQUESTS);

    // NewChannels

    //QSignalSpy newChannelSpy(cliConnection, )
    // QTRY_VERIFY
    qWarning() << svcConnection->busName() << svcConnection->objectPath();


    QTest::qWait(10000);
}

//int main(int argc, char *argv[])
//{
//    QCoreApplication app(argc, argv);
//    app.setAttribute(Qt::AA_Use96Dpi, true);

//    qRegisterMetaType<UserData>();
//    Telegram::initialize();
//    TestKeyData::initKeyFiles();

//    Tp::registerTypes();
//    Tp::enableDebug(true);
//    Tp::enableWarnings(true);

//    MorseProtocolPtr proto = Tp::BaseProtocol::create<MorseProtocol>(QLatin1String("telegram"));
//    proto->setManagerName(c_managerName);
//    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(c_managerName);
//    cm->addProtocol(proto);

//    Tp::DBusError tpError;
//    cm->registerObject(&tpError);
//    if (tpError.isValid()) {
//        qWarning() << tpError.name() << tpError.message();
//    }

//    tst_morse tc;
//    tc.initTestCase();
//    tc.testEverything();
//    tc.cleanupTestCase();

//    return tc.m_succeeded ? 0 : 1;
////    return 0;
//}
QTEST_GUILESS_MAIN(tst_morse)

#include "tst_morse.moc"
