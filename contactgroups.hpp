
#include <TelepathyQt/BaseConnection>

class ConnectionContactGroupsInterface;
typedef Tp::SharedPtr<ConnectionContactGroupsInterface> ConnectionContactGroupsInterfacePtr;

class ConnectionContactGroupsInterface : public Tp::AbstractConnectionInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(ConnectionContactGroupsInterface)

public:
    static ConnectionContactGroupsInterfacePtr create()
    {
        return ConnectionContactGroupsInterfacePtr(new ConnectionContactGroupsInterface());
    }

    QVariantMap immutableProperties() const;

protected:
    ConnectionContactGroupsInterface();

private:
    void createAdaptor();

    QObject *mAdaptee;
};
