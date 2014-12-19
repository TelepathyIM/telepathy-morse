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

#include "protocol.hpp"
#include "connection.hpp"

#include <TelepathyQt/BaseConnection>
#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>

MorseProtocol::MorseProtocol(const QDBusConnection &dbusConnection, const QString &name)
    : BaseProtocol(dbusConnection, name)
{
    qDebug() << Q_FUNC_INFO;
    setParameters(Tp::ProtocolParameterList()
                  << Tp::ProtocolParameter(QLatin1String("account"), QLatin1String("s"), Tp::ConnMgrParamFlagRequired));

    setRequestableChannelClasses(Tp::RequestableChannelClassSpecList() << Tp::RequestableChannelClassSpec::textChat());

    // callbacks
    setCreateConnectionCallback(memFun(this, &MorseProtocol::createConnection));
    setIdentifyAccountCallback(memFun(this, &MorseProtocol::identifyAccount));
    setNormalizeContactCallback(memFun(this, &MorseProtocol::normalizeContact));

    addrIface = Tp::BaseProtocolAddressingInterface::create();
    addrIface->setAddressableVCardFields(QStringList() << QLatin1String("tel"));
    addrIface->setAddressableUriSchemes(QStringList() << QLatin1String("tel"));
    addrIface->setNormalizeVCardAddressCallback(memFun(this, &MorseProtocol::normalizeVCardAddress));
    addrIface->setNormalizeContactUriCallback(memFun(this, &MorseProtocol::normalizeContactUri));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(addrIface));

    avatarsIface = Tp::BaseProtocolAvatarsInterface::create();
    avatarsIface->setAvatarDetails(Tp::AvatarSpec(/* supportedMimeTypes */ QStringList() << QLatin1String("image/jpeg"),
                                                  /* minHeight */ 0, /* maxHeight */ 160, /* recommendedHeight */ 160,
                                                  /* minWidth */ 0, /* maxWidth */ 160, /* recommendedWidth */ 160,
                                                  /* maxBytes */ 10240));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(avatarsIface));

    presenceIface = Tp::BaseProtocolPresenceInterface::create();
    presenceIface->setStatuses(Tp::PresenceSpecList(MorseConnection::getSimpleStatusSpecMap()));
    plugInterface(Tp::AbstractProtocolInterfacePtr::dynamicCast(presenceIface));
}

MorseProtocol::~MorseProtocol()
{
}

Tp::BaseConnectionPtr MorseProtocol::createConnection(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << parameters;
    Q_UNUSED(error)

    Tp::BaseConnectionPtr newConnection = Tp::BaseConnection::create<MorseConnection>(QLatin1String("morse"), name(), parameters);

    return newConnection;
}

QString MorseProtocol::identifyAccount(const QVariantMap &parameters, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << parameters;
    error->set(QLatin1String("IdentifyAccount.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MorseProtocol::normalizeContact(const QString &contactId, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << contactId;
    error->set(QLatin1String("NormalizeContact.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MorseProtocol::normalizeVCardAddress(const QString &vcardField, const QString vcardAddress,
        Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << vcardField << vcardAddress;
    error->set(QLatin1String("NormalizeVCardAddress.Error.NotImplemented"), QLatin1String(""));
    return QString();
}

QString MorseProtocol::normalizeContactUri(const QString &uri, Tp::DBusError *error)
{
    qDebug() << Q_FUNC_INFO << uri;
    error->set(QLatin1String("NormalizeContactUri.Error.NotImplemented"), QLatin1String(""));
    return QString();
}
