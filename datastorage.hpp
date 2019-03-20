#ifndef MORSE_DATA_STORAGE
#define MORSE_DATA_STORAGE

#include <TelegramQt/DataStorage>

class MorseInfo;

class MorseDataStorage : public Telegram::Client::InMemoryDataStorage
{
    Q_OBJECT
public:
    explicit MorseDataStorage(QObject *parent = nullptr);

    void setInfo(MorseInfo *info);

public slots:
    bool saveData() const;
    bool loadData();

protected:
    MorseInfo *m_info = nullptr;

};

#endif // MORSE_DATA_STORAGE
