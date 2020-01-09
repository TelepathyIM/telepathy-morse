#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>

#include "info.hpp"

class MorseQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    MorseQmlPlugin(QObject *parent = nullptr) :
        QQmlExtensionPlugin(parent)
    {
    }

    void registerTypes(const char *uri) override;
};

void MorseQmlPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QByteArray(uri) == QByteArray("Morse"));

    int versionMajor = 0;
    int versionMinor = 1;

    qmlRegisterType<MorseInfo>(uri, versionMajor, versionMinor, "Info");
}

#include "plugin.moc"
