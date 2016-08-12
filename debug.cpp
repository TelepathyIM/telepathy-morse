/*
    Copyright (C) 2016 Alexandr Akulich <akulichalexander@gmail.com>

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "debug.hpp"

#include <TelepathyQt/BaseDebug>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  define QtMessageHandler QtMsgHandler
#  define qInstallMessageHandler qInstallMsgHandler
#endif

static QtMessageHandler defaultMessageHandler = 0;

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
class FixedBaseDebug : public Tp::BaseDebug
{
    Q_OBJECT
public slots:
    bool registerObject(const QString &busName, Tp::DBusError *error = NULL)
    {
        if (isRegistered()) {
            return true;
        }

        Tp::DBusError _error;
        bool ret = Tp::DBusService::registerObject(busName, TP_QT_DEBUG_OBJECT_PATH, &_error);

        if (!ret && error) {
            error->set(_error.name(), _error.message());
        }

        return ret;
    }
};
static QPointer<FixedBaseDebug> debugInterfacePtr;
#else
static QPointer<Tp::BaseDebug> debugInterfacePtr;
#endif

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
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    const QString logMessage = qFormatLogMessage(type, context, msg);

    if (logMessage.isNull()) {
        return;
    }
#else
    const QString logMessage = QString::number(type) + QLatin1Char('|') + msg;
#endif

    fprintf(stderr, "%s\n", logMessage.toLocal8Bit().constData());
    fflush(stderr);
}

bool enableDebugInterface()
{
    if (!debugInterfacePtr.isNull()) {
        return debugInterfacePtr->isRegistered();
    }

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
    debugInterfacePtr = new FixedBaseDebug();
#else
    debugInterfacePtr = new Tp::BaseDebug();
#endif

    debugInterfacePtr->setGetMessagesLimit(-1);

    if (!debugInterfacePtr->registerObject(TP_QT_CONNECTION_MANAGER_BUS_NAME_BASE + QLatin1String("morse"))) {
        return false;
    }

    defaultMessageHandler = qInstallMessageHandler(debugViaDBusInterface);
    return true;
}

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
#include "debug.moc"
#endif
