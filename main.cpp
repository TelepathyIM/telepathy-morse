#include <QCoreApplication>

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/Constants>
#include <TelepathyQt/Debug>

#include "protocol.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QLatin1String("telepathy-morse"));

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    Tp::BaseProtocolPtr proto = Tp::BaseProtocol::create<MorseProtocol>(QLatin1String("telegram"));
    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(QLatin1String("morse"));

    proto->setEnglishName(QLatin1String("Telegram"));
    proto->setIconName(QLatin1String("telegram"));
    proto->setVCardField(QLatin1String("tel"));

    QMetaObject::invokeMethod(proto.data(), "setConnectionManagerName", Qt::DirectConnection, Q_ARG(QString, cm->name()));

    cm->addProtocol(proto);

    cm->registerObject();

    return app.exec();
}
