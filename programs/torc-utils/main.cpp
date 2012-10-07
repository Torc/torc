// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "version.h"
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "utilscommandlineparser.h"

using namespace std;

int main(int argc, char **argv)
{
    new QCoreApplication(argc, argv);
    QCoreApplication::setApplicationName("torc-utils");
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    {
        QScopedPointer<UtilsCommandLineParser> cmdline(new UtilsCommandLineParser());
        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        bool justexit = false;
        if (!cmdline->Parse(argc, argv, justexit))
            return GENERIC_EXIT_INVALID_CMDLINE;
        if (justexit)
            return GENERIC_EXIT_OK;

        if (int error = TorcLocalContext::Create(cmdline.data()))
            return error;
    }

    TorcLocalContext::TearDown();

    return GENERIC_EXIT_OK;
}

