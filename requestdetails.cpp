#include "requestdetails.hpp"

#include <TelepathyQt/BaseConnection>

QString RequestDetails::channelType() const
{
    return value(TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")).toString();
}

Tp::HandleType RequestDetails::targetHandleType() const
{
    QVariant result = value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType"));
    if (result.isNull()) {
        return Tp::HandleTypeNone;
    }
    return static_cast<Tp::HandleType>(result.toUInt());
}

uint RequestDetails::initiatorHandle() const
{
    return value(TP_QT_IFACE_CHANNEL + QLatin1String(".InitiatorHandle")).toUInt();
}

bool RequestDetails::isRequested() const
{
    return value(TP_QT_IFACE_CHANNEL + QLatin1String(".Requested")).toBool();
}

QString RequestDetails::getTargetIdentifier(Tp::BaseConnection *connection) const
{
    if (contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
        return value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString();
    }
    if (contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
        Tp::DBusError error;
        const uint handle = value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
        const auto ids = connection->inspectHandles(targetHandleType(), QList<uint>({ handle }), &error);
        if (!error.isValid() && !ids.isEmpty()) {
            return ids.first();
        }
    }
    return QString();
}

uint RequestDetails::getTargetHandle(Tp::BaseConnection *connection) const
{
    if (contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
        return value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
    }
    if (contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
        Tp::DBusError error;
        const QString id = value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString();
        const auto handles = connection->requestHandles(targetHandleType(), { id }, &error);
        if (!error.isValid() && !handles.isEmpty()) {
            return handles.first();
        }
    }
    return 0;
}
