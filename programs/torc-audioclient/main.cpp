// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "version.h"
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "audioclientcommandlineparser.h"
#include "audiointerface.h"
#include "torcmediamaster.h"

int main(int argc, char **argv)
{
    new QCoreApplication(argc, argv);
    QCoreApplication::setApplicationName(QObject::tr("torc-audioclient"));
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    {
        QScopedPointer<AudioClientCommandLineParser> cmdline(new AudioClientCommandLineParser());
        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        bool justexit = false;
        if (!cmdline->Parse(argc, argv, justexit))
            return GENERIC_EXIT_INVALID_CMDLINE;
        if (justexit)
            return GENERIC_EXIT_OK;

        if (int error = TorcLocalContext::Create(cmdline.data(), Torc::Database | Torc::Server | Torc::Client | Torc::Storage | Torc::Power | Torc::USB | Torc::Network))
            return error;
    }

    int ret = GENERIC_EXIT_OK;

    if (!gTorcMediaMaster)
        LOG(VB_GENERAL, LOG_WARNING, "TorcMediaMaster not available");

    QObject dummy;
    AudioInterface* interface = new AudioInterface(&dummy, true);
    if (interface->InitialisePlayer())
        ret = qApp->exec();
    delete interface;

    TorcLocalContext::TearDown();

    return ret;
}


