// Qt
#include <QtQml>
#include <QThread>
#include <QtQuick/QQuickView>
#include <QtCore/QString>

#ifndef QT_NO_WIDGETS
#include <QtWidgets/QApplication>
#define Application QApplication
#else
#include <QtGui/QGuiApplication>
#define Application QGuiApplication
#endif

// Torc
#include "torclocalcontext.h"
#include "torcnetworkedcontext.h"
#include "torcdirectories.h"
#include "torccommandlineparser.h"
#include "torcmediamaster.h"

int main(int argc, char *argv[])
{
    // create the application, name it and name the main thread
    Application app(argc, argv);
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    // create local context
    {
        // FIXME add proper command line handling
        QScopedPointer<TorcCommandLineParser> cmdline(new TorcCommandLineParser());
        Torc::ApplicationFlags flags = Torc::Database | Torc::Server | Torc::Client | Torc::Storage | Torc::USB | Torc::Network;
        if (int error = TorcLocalContext::Create(cmdline.data(), flags))
            return error;
    }

    // register Torc types and APIs
    qmlRegisterUncreatableType<TorcSetting>("Torc.Core.TorcSetting", 0, 1, "TorcSetting", "TorcSetting cannot be created within scripts");
    qmlRegisterUncreatableType<TorcNetworkService>("Torc.Core.TorcNetworkService", 0, 1, "TorcNetworkService", "TorcNetworkService cannot be created within scripts");
    qmlRegisterUncreatableType<TorcMedia>("Torc.Media.TorcMedia", 0, 1, "TorcMedia", "TorcMedia cannot be created within scripts");

    // create the engine and show the window
    QQmlApplicationEngine *engine = new QQmlApplicationEngine();
    engine->rootContext()->setContextProperty("TorcLocalContext", gLocalContext);
    if (gRootSetting)
        engine->rootContext()->setContextProperty("RootSetting", gRootSetting);
    if (gNetworkedContext)
        engine->rootContext()->setContextProperty("TorcNetworkedContext", gNetworkedContext);
    if (gTorcMediaMaster)
        engine->rootContext()->setContextProperty("TorcMediaMaster", gTorcMediaMaster);

    engine->load(GetTorcShareDir() + "torc-desktop/qml/main.qml");

    int ret = -1;

    if (engine->rootObjects().size())
    {
        QObject *top = engine->rootObjects().value(0);
        QQuickWindow *window = qobject_cast<QQuickWindow *>(top);
        if (window)
        {
            window->show();

            // and execute
            ret = app.exec();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Root object must be a window");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to load QML objects");
    }

    delete engine;

    TorcLocalContext::TearDown();

    return ret;
}
