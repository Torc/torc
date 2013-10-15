// Qt
#include <QtQml>
#include <QThread>
#include <QtQuick/QQuickView>
#include <QtGui/QGuiApplication>

// Torc
#include "torclocalcontext.h"
#include "torcnetworkedcontext.h"
#include "torcdirectories.h"
#include "torcexitcodes.h"
#include "torccommandline.h"
#include "torcpower.h"
#include "torcmediamaster.h"
#include "torcmediamasterfilter.h"
#include "torcqmlutils.h"
#include "torcqmleventproxy.h"
#include "torcqmlframerate.h"
#include "torcqmlmediaelement.h"

#include <QtQuick/QQuickItem>

int main(int argc, char *argv[])
{
    // create the application, name it and name the main thread
    QGuiApplication app(argc, argv);
    // the 'torc-' prefix is used for identification purposes elsewhere, don't change it
    QCoreApplication::setApplicationName("torc-tv");
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

        Torc::ApplicationFlags flags = Torc::Database | Torc::Server | Torc::Client | Torc::Storage | Torc::USB | Torc::Network | Torc::Power;
        if (int error = TorcLocalContext::Create(cmdline.data(), flags))
            return error;
    }

    // register Torc types and APIs
    qmlRegisterUncreatableType<TorcSetting>        ("Torc.Core",  0, 1, "TorcSetting",        "TorcSetting cannot be created within scripts");
    qmlRegisterUncreatableType<TorcNetworkService> ("Torc.Core",  0, 1, "TorcNetworkService", "TorcNetworkService cannot be created within scripts");
    qmlRegisterUncreatableType<TorcPower>          ("Torc.Core",  0, 1, "TorcPower",          "TorcPower cannot be created within scripts");
    qmlRegisterUncreatableType<TorcMedia>          ("Torc.Media", 0, 1, "TorcMedia",          "TorcMedia cannot be created within scripts");
    qmlRegisterType<TorcMediaMasterFilter>         ("Torc.Media", 0, 1, "TorcMediaMasterFilter");
    qmlRegisterType<TorcQMLMediaElement>           ("Torc.QML",   0, 1, "TorcQMLMediaElement");
    qmlRegisterType<QAbstractItemModel>();

    // create the view
    QQuickView *view = new QQuickView();
    QQmlContext *context = view->rootContext();

    if (context)
    {
        QScopedPointer<TorcQMLEventProxy> proxy(new TorcQMLEventProxy(view));
        QScopedPointer<TorcQMLFrameRate>  fps(new TorcQMLFrameRate());

        QObject::connect(view->engine(), SIGNAL(quit()), view, SLOT(close()));
        QObject::connect(view, SIGNAL(frameSwapped()), fps.data(), SLOT(NewFrame()));

        // add Torc objects before loading
        TorcQMLUtils::AddProperty("torcLocalContext",     gLocalContext,     context);
        TorcQMLUtils::AddProperty("torcRootSetting",      gRootSetting,      context);
        TorcQMLUtils::AddProperty("torcNetworkedContext", gNetworkedContext, context);
        TorcQMLUtils::AddProperty("torcPower",            gPower,            context);
        TorcQMLUtils::AddProperty("torcMediaMaster",      gTorcMediaMaster,  context);
        TorcQMLUtils::AddProperty("torcFPS",              fps.data(),        context);

        // load qml
        view->setSource(GetTorcShareDir() + "torc-tv/qml/main.qml");

        // setup view
        view->setFlags(view->flags() | Qt::WindowFullscreenButtonHint);
        view->setWindowState(Qt::WindowFullScreen);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->showFullScreen();

        // and execute
        ret = app.exec();
    }

    delete view;
    TorcLocalContext::TearDown();

    return ret;
}

