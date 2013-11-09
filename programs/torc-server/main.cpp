// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "version.h"
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "torccommandline.h"
#include "audiointerface.h"
#include "torcmediamaster.h"

int main(int argc, char **argv)
{
    new QCoreApplication(argc, argv);
    // the 'torc-' prefix is used for identification purposes elsewhere, don't change it
    QCoreApplication::setApplicationName("torc-server");
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    int ret = GENERIC_EXIT_OK;

    {
        bool justexit = false;
        QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::URI | TorcCommandLine::Database | TorcCommandLine::LogFile));

        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        ret = cmdline->Evaluate(argc, argv, justexit);

        if (ret != GENERIC_EXIT_OK)
            return ret;

        if (justexit)
            return ret;

        if (int error = TorcLocalContext::Create(cmdline.data(), Torc::Database | Torc::Server | Torc::Client | Torc::Storage | Torc::Power | Torc::USB | Torc::Network))
            return error;
    }

    if (!gTorcMediaMaster)
        LOG(VB_GENERAL, LOG_WARNING, "TorcMediaMaster not available");

    AudioInterface* interface = new AudioInterface(true);
    if (interface->InitialisePlayer())
        ret = qApp->exec();
    delete interface;

    TorcLocalContext::TearDown();

    return ret;
}


