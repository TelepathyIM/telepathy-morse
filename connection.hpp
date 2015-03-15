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

#ifndef MORSE_CONNECTION_HPP
#define MORSE_CONNECTION_HPP

#include <TelepathyQt/BaseConnection>

#include <TelegramQt/TelegramNamespace>

class CTelegramCore;

class MorseConnection : public Tp::BaseConnection
{
    Q_OBJECT
public:
    MorseConnection(const QDBusConnection &dbusConnection,
            const QString &cmName, const QString &protocolName,
            const QVariantMap &parameters);
    ~MorseConnection();

    static Tp::SimpleStatusSpecMap getSimpleStatusSpecMap();

    void doConnect(Tp::DBusError *error);

    QStringList inspectHandles(uint handleType, const Tp::UIntList &handles, Tp::DBusError *error);
    Tp::BaseChannelPtr createChannel(const QString &channelType, uint targetHandleType,
                                         uint targetHandle, const QVariantMap &request, Tp::DBusError *error);

    Tp::UIntList requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error);

    Tp::ContactAttributesMap getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error);
    Tp::ContactAttributesMap getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error);

    void requestSubscription(const Tp::UIntList &handles, const QString &message, Tp::DBusError *error);
    void removeContacts(const Tp::UIntList &handles, Tp::DBusError *error);

    Tp::AliasMap getAliases(const Tp::UIntList &handles, Tp::DBusError *error = 0);
    void setAliases(const Tp::AliasMap &aliases, Tp::DBusError *error);

    QString getAlias(uint handle);

    Tp::SimplePresence getPresence(uint handle);
    uint setPresence(const QString &status, const QString &message, Tp::DBusError *error);

    uint ensureContact(const QString &identifier);

public slots:
    void receiveMessage(const QString &identifier, const QString &message, TelegramNamespace::MessageType type, quint32 messageId, quint32 flags, quint32 timestamp);
    void updateContactPresence(const QString &identifier);

signals:
    void messageReceived(const QString &sender, const QString &message);

private slots:
    void whenConnectionStateChanged(TelegramNamespace::ConnectionState state);
    void whenAuthenticated();
    void whenAuthErrorReceived();
    void whenPhoneCodeRequired();
    void whenPhoneCodeIsInvalid();
    void whenConnectionReady();
    void whenContactListChanged();
    void whenDisconnected();
    void whenConnectionStatusChanged(int newStatus);

    /* Connection.Interface.Avatars */
    void whenAvatarReceived(const QString &contact, const QByteArray &data, const QString &mimeType, const QString &token);

private:
    static QByteArray getSessionData(const QString &phone);
    static bool saveSessionData(const QString &phone, const QByteArray &data);

    uint getHandle(const QString &identifier) const;
    uint addContact(const QString &identifier);
    uint addContacts(const QStringList &identifiers);

    void updateContactsState(const QStringList &identifiers);
    void setSubscriptionState(const QStringList &identifiers, const QList<uint> &handles, uint state);

    void startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error);

    Tp::ContactInfoMap getContactInfo(const Tp::UIntList &contacts, Tp::DBusError *error);

    /* Connection.Interface.Avatars */
    Tp::AvatarTokenMap getKnownAvatarTokens(const Tp::UIntList &contacts, Tp::DBusError *error);
    void requestAvatars(const Tp::UIntList &contacts, Tp::DBusError *error);

    bool coreIsReady();
    bool coreIsAuthenticated();

    Tp::BaseConnectionContactsInterfacePtr contactsIface;
    Tp::BaseConnectionSimplePresenceInterfacePtr simplePresenceIface;
    Tp::BaseConnectionContactListInterfacePtr contactListIface;
    Tp::BaseConnectionAliasingInterfacePtr aliasingIface;
    Tp::BaseConnectionAvatarsInterfacePtr avatarsIface;
    Tp::BaseConnectionAddressingInterfacePtr addressingIface;
    Tp::BaseConnectionRequestsInterfacePtr requestsIface;
    Tp::BaseChannelSASLAuthenticationInterfacePtr saslIface;

    Tp::SimpleContactPresences m_presences;

    QString m_wantedPresence;

    QMap<uint, QString> m_handles;
    /* Maps a contact handle to its subscription state */
    QHash<uint, uint> m_contactsSubscription;

    CTelegramCore *m_core;

    int m_authReconnectionsCount;

    QString m_selfPhone;
};

#endif // MORSE_CONNECTION_HPP
