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
#include "torccommandline.h"
#include "torcexitcodes.h"
#include "uiwindow.h"
#include "tenfoottheme.h"
#include "videouiplayer.h"
#include "torcmediamaster.h"

using namespace std;

int main(int argc, char **argv)
{
    new QApplication(argc, argv);
    QCoreApplication::setApplicationName(QObject::tr("torc-client"));
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    int ret = GENERIC_EXIT_OK;

    {
        QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::None));
        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        bool justexit = false;
        ret = cmdline->Evaluate(argc, argv, justexit);

        if (ret != GENERIC_EXIT_OK)
            return ret;

        if (justexit)
            return ret;

        Torc::ApplicationFlags flags = Torc::Database | Torc::Server | Torc::Client | Torc::Storage |
                                       Torc::Power | Torc::USB | Torc::Network | Torc::AdminThread;
        if (int error = TorcLocalContext::Create(cmdline.data(), flags))
            return error;
    }

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
        ret = GENERIC_EXIT_NO_THEME;
    }


    TorcLocalContext::TearDown();

    return ret;

}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
