#include <QCoreApplication>

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/BaseDebug>
#include <TelepathyQt/Constants>
#include <TelepathyQt/Debug>

#include "protocol.hpp"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define QtMessageHandler QtMsgHandler
#define qInstallMessageHandler qInstallMsgHandler
#endif

QtMessageHandler defaultMessageHandler = 0;
QPointer<Tp::BaseDebug> debugInterfacePtr;

void debugViaDBusInterface(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!debugInterfacePtr.isNull()) {
        QString domain(QLatin1String("%1:%2, %3"));
        domain = domain.arg(QString::fromLatin1(context.file)).arg(context.line).arg(QString::fromLatin1(context.function));
        switch (type) {
        case QtDebugMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelDebug, msg);
            break;
        case QtInfoMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelInfo, msg);
            break;
        case QtWarningMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelWarning, msg);
            break;
        case QtCriticalMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelCritical, msg);
            break;
        case QtFatalMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelError, msg);
            break;
        }
    }

    if (defaultMessageHandler) {
        defaultMessageHandler(type, context, msg);
    }
}

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

    cm->addProtocol(proto);
    cm->registerObject();

    qInstallMessageHandler(debugViaDBusInterface);

    Tp::BaseDebug debug;
    debug.setGetMessagesLimit(-1);
    debug.registerObject();

    debugInterfacePtr = &debug;

    return app.exec();
}
