#ifndef MORSE_INFO_HPP
#define MORSE_INFO_HPP

#include <QObject>

class MorseInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildVersion READ buildVersion CONSTANT)
public:
    static QString version();
    static QString buildVersion();
};

#endif // MORSE_INFO_HPP
