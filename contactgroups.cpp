#include "contactgroups.hpp"

#include <TelepathyQt/_gen/svc-connection.h>

#include <TelepathyQt/DBusObject>

ConnectionContactGroupsInterface::ConnectionContactGroupsInterface()
    : AbstractConnectionInterface(TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_GROUPS),
      mAdaptee(new QObject(this))
{
}

QVariantMap ConnectionContactGroupsInterface::immutableProperties() const
{
    QVariantMap map;
    return map;
}

void ConnectionContactGroupsInterface::createAdaptor()
{
    (void) new Tp::Service::ConnectionInterfaceContactGroupsAdaptor(dbusObject()->dbusConnection(),
            mAdaptee, dbusObject());
}
