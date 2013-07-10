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
#include "torcmediamasterfilter.h"
#include "eventproxy.h"

void AddProperty(const QString &Name, QObject* Property, QQmlContext *Context)
{
    if (Property && Context)
    {
        Context->setContextProperty(Name, Property);
        LOG(VB_GENERAL, LOG_INFO, QString("Added QML property '%1'").arg(Name));
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to add property %1").arg(Name));
    }
}

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
    qmlRegisterUncreatableType<TorcSetting>        ("Torc.Core",  0, 1, "TorcSetting",        "TorcSetting cannot be created within scripts");
    qmlRegisterUncreatableType<TorcNetworkService> ("Torc.Core",  0, 1, "TorcNetworkService", "TorcNetworkService cannot be created within scripts");
    qmlRegisterUncreatableType<TorcMedia>          ("Torc.Media", 0, 1, "TorcMedia",          "TorcMedia cannot be created within scripts");
    qmlRegisterType<TorcMediaMasterFilter>         ("Torc.Media", 0, 1, "TorcMediaMasterFilter");
    qmlRegisterType<QAbstractItemModel>();

    // create the engine
    QQmlApplicationEngine *engine = new QQmlApplicationEngine();
    QQmlContext *context = engine->rootContext();

    // add Torc objects before loading
    AddProperty("TorcLocalContext",     gLocalContext, context);
    AddProperty("RootSetting",          gRootSetting, context);
    AddProperty("TorcNetworkedContext", gNetworkedContext, context);
    AddProperty("TorcMediaMaster",      gTorcMediaMaster, context);

    // load the 'theme'
    engine->load(GetTorcShareDir() + "torc-desktop/qml/main.qml");

    int ret = -1;

    if (engine->rootObjects().size())
    {
        QObject *top = engine->rootObjects().value(0);
        QQuickWindow *window = qobject_cast<QQuickWindow *>(top);
        if (window)
        {
            // install a Torc event handler (primarily for interrupt handling)
            QScopedPointer<EventProxy> proxy(new EventProxy(window));
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
