// Qt
#include <QCoreApplication>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtWidgets/QApplication>
#else
#include <QApplication>
#endif
#include <QThread>

// Torc
#include "version.h"
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "uiwindow.h"
#include "tenfoottheme.h"
#include "videouiplayer.h"
#include "clientcommandlineparser.h"
#include "torcmediamaster.h"

using namespace std;

int main(int argc, char **argv)
{
    new QApplication(argc, argv);
    QCoreApplication::setApplicationName(QObject::tr("torc-client"));
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    {
        QScopedPointer<ClientCommandLineParser> cmdline(new ClientCommandLineParser());
        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        bool justexit = false;
        if (!cmdline->Parse(argc, argv, justexit))
            return GENERIC_EXIT_INVALID_CMDLINE;
        if (justexit)
            return GENERIC_EXIT_OK;

        Torc::ApplicationFlags flags = Torc::Database | Torc::Server | Torc::Client | Torc::Storage |
                                       Torc::Power | Torc::USB | Torc::Network | Torc::AdminThread;
        if (int error = TorcLocalContext::Create(cmdline.data(), flags))
            return error;
    }

    int ret = GENERIC_EXIT_NO_THEME;
    UIWindow *window = UIWindow::Create();
    if (window)
    {
        window->ThemeReady(TenfootTheme::Load(false, window));

        // NB torc-client has no explicit dependancy on libtorc-videoui and GCC 4.6
        // will optimise away any linkage by default. So force its hand and initialise
        // the hardware decoders now.
        VideoUIPlayer::Initialise();

        // and force linking of libtorc-media
        if (!gTorcMediaMaster)
            LOG(VB_GENERAL, LOG_WARNING, "TorcMediaMaster not available");

        ret = qApp->exec();

        delete window;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create main window");
    }


    TorcLocalContext::TearDown();

    return ret;

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
