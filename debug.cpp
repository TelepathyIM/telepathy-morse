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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
bool enableDebugInterface() { return false; }
#else

static QtMessageHandler defaultMessageHandler = 0;

void debugViaDBusInterface(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!debugInterfacePtr.isNull()) {
        QString domain(QLatin1String("%1:%2, %3"));
        QByteArray fileName = QByteArray::fromRawData(context.file, qstrlen(context.file));

        static const char *namesToWrap[] = {
            "morse",
            "telepathy-qt"
        };

        for (int i = 0; i < 2; ++i) {
            int index = fileName.indexOf(namesToWrap[i]);
            if (index < 0) {
                continue;
            }

            fileName = fileName.mid(index);
            break;
        }

        domain = domain.arg(QString::fromLocal8Bit(fileName)).arg(context.line).arg(QString::fromLatin1(context.function));
        QString message = msg;
        if (message.startsWith(QLatin1String(context.function))) {
            message = message.mid(qstrlen(context.function));
            if (message.startsWith(QLatin1Char(' '))) {
                message.remove(0, 1);
            }
        }

        switch (type) {
        case QtDebugMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelDebug, message);
            break;
        case QtInfoMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelInfo, message);
            break;
        case QtWarningMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelWarning, message);
            break;
        case QtCriticalMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelCritical, message);
            break;
        case QtFatalMsg:
            debugInterfacePtr->newDebugMessage(domain, Tp::DebugLevelError, message);
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
#endif

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
#include "debug.moc"
#endif
