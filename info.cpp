#include "info.hpp"

#include <QStandardPaths>

static const QString c_accountsDirectory = QLatin1String("telepathy/morse");
static const QString c_accountFile = QLatin1String("account.bin");

MorseInfo::MorseInfo(QObject *parent)
    : QObject(parent)
{
    connect(this, &MorseInfo::accountIdentifierChanged,
            this, &MorseInfo::accountDataDirectoryChanged);
    connect(this, &MorseInfo::serverIdentifierChanged,
            this, &MorseInfo::accountDataDirectoryChanged);
}

quint32 MorseInfo::appId()
{
    return 14617u;
}

QString MorseInfo::appHash()
{
    return QStringLiteral("e17ac360fd072f83d5d08db45ce9a121");
}

QString MorseInfo::accountDataDirectory() const
{
    if (m_accountIdentifier.isEmpty()) {
        return QString();
    }
    const QString serverIdentifier = m_serverIdentifier.isEmpty() ? QStringLiteral("official") : m_serverIdentifier;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QLatin1Char('/') + c_accountsDirectory
            + QLatin1Char('/') + serverIdentifier
            + QLatin1Char('/') + m_accountIdentifier;
}

QString MorseInfo::accountDataFilePath() const
{
    const QString directory = accountDataDirectory();
    if (directory.isEmpty()) {
        return QString();
    }
    return directory + QLatin1Char('/') + c_accountFile;
}

QString MorseInfo::accountIdentifier() const
{
    return m_accountIdentifier;
}

QString MorseInfo::serverIdentifier() const
{
    return m_serverIdentifier;
}

void MorseInfo::setAccountIdentifier(const QString &newAccountIdentifier)
{
    if (m_accountIdentifier == newAccountIdentifier) {
        return;
    }
    m_accountIdentifier = newAccountIdentifier;
    emit accountIdentifierChanged();
}

void MorseInfo::setServerIdentifier(const QString &newServerIdentifier)
{
    if (m_serverIdentifier == newServerIdentifier) {
        return;
    }
    m_serverIdentifier = newServerIdentifier;
    emit serverIdentifierChanged();
}
