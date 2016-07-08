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
static QPointer<Tp::BaseDebug> debugInterfacePtr;

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

    debugInterfacePtr = new Tp::BaseDebug();
    debugInterfacePtr->setGetMessagesLimit(-1);

    if (!debugInterfacePtr->registerObject(TP_QT_CONNECTION_MANAGER_BUS_NAME_BASE + QLatin1String("morse"))) {
        return false;
    }

    defaultMessageHandler = qInstallMessageHandler(debugViaDBusInterface);
    return true;
}
