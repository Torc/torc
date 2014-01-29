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
#include "torclanguage.h"
#include "torcexitcodes.h"
#include "torccommandline.h"
#include "torcmediamaster.h"
#include "torcmediamasterfilter.h"
#include "torcqmlutils.h"
#include "torcqmleventproxy.h"
#include "torcqmlmediaelement.h"

int main(int argc, char *argv[])
{
    // create the application, name it and name the main thread
    Application app(argc, argv);
    // the 'torc-' prefix is used for identification purposes elsewhere, don't change it
    QCoreApplication::setApplicationName("torc-desktop");
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    int ret = GENERIC_EXIT_OK;

    // create local context
    {
        QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::Database | TorcCommandLine::LogFile));

        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        bool justexit = false;
        ret = cmdline->Evaluate(argc, argv, justexit);

        if (ret != GENERIC_EXIT_OK)
            return ret;

        if (justexit)
            return ret;

        Torc::ApplicationFlags flags = Torc::Database | Torc::Server | Torc::Client | Torc::Storage | Torc::USB | Torc::Network;
        if (int error = TorcLocalContext::Create(cmdline.data(), flags))
            return error;
    }

    // register Torc types and APIs
    qmlRegisterUncreatableType<TorcSetting>        ("Torc.Core",  0, 1, "TorcSetting",        "TorcSetting cannot be created within scripts");
    qmlRegisterUncreatableType<TorcNetworkService> ("Torc.Core",  0, 1, "TorcNetworkService", "TorcNetworkService cannot be created within scripts");
    qmlRegisterUncreatableType<TorcMedia>          ("Torc.Media", 0, 1, "TorcMedia",          "TorcMedia cannot be created within scripts");
    qmlRegisterType<TorcMediaMasterFilter>         ("Torc.Media", 0, 1, "TorcMediaMasterFilter");
    qmlRegisterType<TorcQMLMediaElement>           ("Torc.QML",   0, 1, "TorcQMLMediaElement");
    qmlRegisterType<QAbstractItemModel>();

    // create the engine
    QQmlApplicationEngine *engine = new QQmlApplicationEngine();
    QQmlContext *context = engine->rootContext();

    // add Torc objects before loading
    TorcQMLUtils::AddProperty("TorcLocalContext",     gLocalContext,     context);
    TorcQMLUtils::AddProperty("RootSetting",          gRootSetting,      context);
    TorcQMLUtils::AddProperty("TorcNetworkedContext", gNetworkedContext, context);
    TorcQMLUtils::AddProperty("TorcLanguage",         gLocalContext->GetLanguage(), context);
    TorcQMLUtils::AddProperty("TorcMediaMaster",      gTorcMediaMaster,  context);

    // load the 'theme'
    engine->load(GetTorcShareDir() + "torc-desktop/qml/main.qml");

    if (engine->rootObjects().size())
    {
        QObject *top = engine->rootObjects().value(0);
        QQuickWindow *window = qobject_cast<QQuickWindow *>(top);
        if (window)
        {
            // enable OS X fullscreen toggle
            window->setFlags(window->flags() | Qt::WindowFullscreenButtonHint);

            // install a Torc event handler (primarily for interrupt handling)
            QScopedPointer<TorcQMLEventProxy> proxy(new TorcQMLEventProxy(window));
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
