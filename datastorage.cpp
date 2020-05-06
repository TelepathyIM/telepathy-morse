#include "datastorage.hpp"
#include "info.hpp"

#include <TelegramQt/TelegramNamespace>

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QTimer>

static const QString c_telegramStateFile = QLatin1String("telegram-state.bin");

MorseDataStorage::MorseDataStorage(QObject *parent) :
    Telegram::Client::InMemoryDataStorage(parent)
{
}

void MorseDataStorage::setInfo(MorseInfo *info)
{
    m_info = info;
}

void MorseDataStorage::scheduleSave()
{
    if (!m_delayedSaveTimer) {
        m_delayedSaveTimer = new QTimer(this);
        m_delayedSaveTimer->setSingleShot(true);
        m_delayedSaveTimer->setInterval(500);
        connect(m_delayedSaveTimer, &QTimer::timeout, this, &MorseDataStorage::saveData);
    }

    // Do not restart the timer if it is already active to make sure that
    // two or three messages per second do not completely prevent the client from the save.
    if (!m_delayedSaveTimer->isActive()) {
        m_delayedSaveTimer->start();
    }
}

bool MorseDataStorage::saveData() const
{
    const QByteArray data = saveState();

    QDir dir;
    dir.mkpath(m_info->accountDataDirectory());
    QFile stateFile(m_info->accountDataDirectory() + QLatin1Char('/') + c_telegramStateFile);

    // TODO: Save sent message ids map

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
    // TODO: Load sent message ids map

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
