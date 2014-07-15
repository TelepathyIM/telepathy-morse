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
                                         uint targetHandle, Tp::DBusError *error);

    Tp::UIntList requestHandles(uint handleType, const QStringList &identifiers, Tp::DBusError *error);

    Tp::ContactAttributesMap getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error);
    Tp::ContactAttributesMap getContactAttributes(const Tp::UIntList &handles, const QStringList &interfaces, Tp::DBusError *error);

    Tp::SimplePresence getPresence(uint handle);
    uint setPresence(const QString &status, const QString &message, Tp::DBusError *error);

    uint ensureContact(const QString &identifier);

public slots:
    void receiveMessage(const QString &sender, const QString &message);
    void setContactPresence(const QString &identifier, const QString &presence);

signals:
    void messageReceived(const QString &sender, const QString &message);

private slots:
    void connectStepTwo();
    void whenPhoneCodeRequired();
    void whenPhoneCodeIsInvalid();
    void connectSuccess();
    void whenGotContactList();

private:
    uint getHandle(const QString &identifier) const;
    uint addContact(const QString &identifier);
    uint addContacts(const QStringList &identifiers);

    void setPresenceState(const QList<uint> &handles, const QString &status);
    void setSubscriptionState(const QStringList &identifiers, const QList<uint> &handles, uint state);

    void startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error);

    Tp::ContactInfoMap getContactInfo(const Tp::UIntList &contacts, Tp::DBusError *error);

    Tp::BaseConnectionContactsInterfacePtr contactsIface;
    Tp::BaseConnectionSimplePresenceInterfacePtr simplePresenceIface;
    Tp::BaseConnectionContactListInterfacePtr contactListIface;
    Tp::BaseConnectionContactInfoInterfacePtr contactInfoIface;
    Tp::BaseConnectionAddressingInterfacePtr addressingIface;
    Tp::BaseConnectionRequestsInterfacePtr requestsIface;
    Tp::BaseChannelSASLAuthenticationInterfacePtr saslIface;

    Tp::SimpleContactPresences m_presences;

    QMap<uint, QString> m_handles;
    /* Maps a contact handle to its subscription state */
    QHash<uint, uint> m_contactsSubscription;

    CTelegramCore *m_core;

    QString m_selfPhone;
};

#endif // MORSE_CONNECTION_HPP
