#ifndef MORSE_INFO_HPP
#define MORSE_INFO_HPP

#include <QObject>

class MorseInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildVersion READ buildVersion CONSTANT)

    Q_PROPERTY(QString accountDataDirectory READ accountDataDirectory NOTIFY accountDataDirectoryChanged)
    Q_PROPERTY(QString accountDataFilePath READ accountDataFilePath NOTIFY accountDataDirectoryChanged)
    Q_PROPERTY(QString accountIdentifier READ accountIdentifier WRITE setAccountIdentifier NOTIFY accountIdentifierChanged)
    Q_PROPERTY(QString serverIdentifier READ serverIdentifier WRITE setServerIdentifier NOTIFY serverIdentifierChanged)
public:
    explicit MorseInfo(QObject *parent = nullptr);
    static QString version();
    static QString buildVersion();

    QString accountDataDirectory() const;
    QString accountDataFilePath() const;
    QString accountIdentifier() const;
    QString serverIdentifier() const;

public slots:
    void setAccountIdentifier(const QString &newAccountIdentifier);
    void setServerIdentifier(const QString &newServerIdentifier);

signals:
    void accountDataDirectoryChanged();

    void accountIdentifierChanged();
    void serverIdentifierChanged();

protected:
    QString m_accountIdentifier;
    QString m_serverIdentifier;
};

#endif // MORSE_INFO_HPP
