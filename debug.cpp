/*
    This file is part of the telepathy-morse connection manager.
    Copyright (C) 2016 Alexandr Akulich <akulichalexander@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

#if TP_QT_VERSION < TP_QT_VERSION_CHECK(0, 9, 8)
#include "debug.moc"
#endif
