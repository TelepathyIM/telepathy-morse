#include "contactsearchchannel.hpp"

#include <TelegramQt/CTelegramCore>

#include "identifier.hpp"

MorseContactSearchChannelPtr MorseContactSearchChannel::create(CTelegramCore *core, const QVariantMap &request)
{
    const uint limit = request.value(TP_QT_IFACE_CHANNEL_TYPE_CONTACT_SEARCH + QLatin1String(".Limit"), 30).toUInt();
    return MorseContactSearchChannelPtr(new MorseContactSearchChannel(core, limit));
}

MorseContactSearchChannel::MorseContactSearchChannel(CTelegramCore *core, uint limit) :
    BaseChannelContactSearchType(limit,
                                 QStringList() << QLatin1Literal(""),
                                 QLatin1Literal("")),
    m_core(core)
{
    setSearchCallback(Tp::memFun(this, &MorseContactSearchChannel::contactSearch));

    connect(m_core, SIGNAL(searchComplete(QString,QVector<TelegramNamespace::Peer>)),
            this, SLOT(onSearchComplete(QString,QVector<TelegramNamespace::Peer>)));
}

void MorseContactSearchChannel::contactSearch(const Tp::ContactSearchMap &terms, Tp::DBusError *error)
{
    if (terms.count() != 1) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Unsupported search terms"));
        return;
    }

    const QString key = terms.keys().first();
    if (!key.isNull() && !key.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Invalid search key"));
        return;
    }
    const QString value = terms.value(key);
    if (value.isNull() || value.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Empty search term"));
        return;
    }

    m_query = value;
    m_core->searchContacts(m_query, limit());

    setSearchState(Tp::ChannelContactSearchStateInProgress, QString(), QVariantMap());
}

Tp::ContactInfoFieldList getUserInfo(const quint32 userId, CTelegramCore *m_core)
{
    TelegramNamespace::UserInfo userInfo;
    if (!m_core->getUserInfo(&userInfo, userId)) {
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

void MorseContactSearchChannel::onSearchComplete(const QString &query, const QVector<TelegramNamespace::Peer> &peers)
{
    if (query != m_query) {
        return;
    }

    Tp::ContactSearchResultMap result;

    for (const TelegramNamespace::Peer peer : peers) {
        if (peer.type != TelegramNamespace::Peer::User) {
            continue;
        }

        const MorseIdentifier id = MorseIdentifier::fromPeer(peer);
        result.insert(id.toString(), getUserInfo(id.userId(), m_core));
    }

    searchResultReceived(result);
    setSearchState(Tp::ChannelContactSearchStateCompleted, QString(), QVariantMap());
}
