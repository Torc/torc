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
#include "torcdirectories.h"
#include "torccommandlineparser.h"

static QObject *TorcLocalContextProvider(QQmlEngine*, QJSEngine*)
{
    return gLocalContext;
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
        Torc::ApplicationFlags flags = Torc::Database | Torc::Server | Torc::Client | Torc::Storage | Torc::Power | Torc::USB | Torc::Network;
        if (int error = TorcLocalContext::Create(cmdline.data(), flags))
            return error;
    }

    // register Torc types and APIs
    // QQmlEngine will take ownerhsip of gLocalContext - do not delete it.
    qmlRegisterSingletonType<TorcLocalContext>("Torc.Core.TorcLocalContext", 0, 1, "TorcLocalContext", TorcLocalContextProvider);
    qmlRegisterUncreatableType<TorcSetting>("Torc.Core.TorcSetting", 0, 1, "TorcSetting", "TorcSetting cannot be created within scripts");

    // create the engine and show the window
    QQmlApplicationEngine engine(GetTorcShareDir() + "torc-desktop/qml/main.qml");

    int ret = -1;

    if (engine.rootObjects().size())
    {
        QObject *top = engine.rootObjects().value(0);
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

    // N.B. QQmlEngine takes ownership of gLocalContext and deletes it on exit

    return ret;
}
