#include "info.hpp"

#include <QStandardPaths>

static const QString c_telegramAccountSubdir = QLatin1String("telepathy/morse");

MorseInfo::MorseInfo(QObject *parent)
    : QObject(parent)
{
}

QString MorseInfo::accountDataDirectory() const
{
    const QString serverIdentifier = m_serverIdentifier.isEmpty() ? QStringLiteral("official") : m_serverIdentifier;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QLatin1Char('/') + c_telegramAccountSubdir
            + QLatin1Char('/') + serverIdentifier
            + QLatin1Char('/') + m_accountIdentifier;
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
