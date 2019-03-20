#include "datastorage.hpp"
#include "info.hpp"

#include <TelegramQt/TelegramNamespace>

#include <QDir>
#include <QFile>
#include <QLoggingCategory>

static const QString c_telegramStateFile = QLatin1String("telegram-state.bin");

MorseDataStorage::MorseDataStorage(QObject *parent) :
    Telegram::Client::InMemoryDataStorage(parent)
{
}

void MorseDataStorage::setInfo(MorseInfo *info)
{
    m_info = info;
}

bool MorseDataStorage::saveData() const
{
    const QByteArray data = saveState();

    QDir dir;
    dir.mkpath(m_info->accountDataDirectory());
    QFile stateFile(m_info->accountDataDirectory() + QLatin1Char('/') + c_telegramStateFile);

    if (!stateFile.open(QIODevice::WriteOnly)) {
        qWarning() << Q_FUNC_INFO << "Unable to open state file" << stateFile.fileName();
    }

    if (stateFile.write(data) != data.size()) {
        qWarning() << Q_FUNC_INFO << "Unable to save the session data to file"
                   << "for account"
                   << Telegram::Utils::maskPhoneNumber(m_info->accountIdentifier());
    }
    qDebug() << Q_FUNC_INFO << "State saved to file" << stateFile.fileName();

    return true;
}

bool MorseDataStorage::loadData()
{
    QFile stateFile(m_info->accountDataDirectory() + QLatin1Char('/') + c_telegramStateFile);

    if (!stateFile.open(QIODevice::ReadOnly)) {
        qDebug() << Q_FUNC_INFO << "Unable to open state file" << stateFile.fileName();
        return false;
    }

    const QByteArray data = stateFile.readAll();
    qDebug() << Q_FUNC_INFO << m_info->accountIdentifier() << "(" << data.size() << "bytes)";

    loadState(data);

    return true;
}
