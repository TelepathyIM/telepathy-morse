#ifndef MORSE_DATA_STORAGE
#define MORSE_DATA_STORAGE

#include <TelegramQt/DataStorage>

QT_FORWARD_DECLARE_CLASS(QTimer)

class MorseInfo;

class MorseDataStorage : public Telegram::Client::InMemoryDataStorage
{
    Q_OBJECT
public:
    explicit MorseDataStorage(QObject *parent = nullptr);

    void setInfo(MorseInfo *info);

public slots:
    void scheduleSave();
    bool saveData() const;
    bool loadData();

protected:
    MorseInfo *m_info = nullptr;
    QTimer *m_delayedSaveTimer = nullptr;

};

#endif // MORSE_DATA_STORAGE
