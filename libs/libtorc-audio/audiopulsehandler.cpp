// Qt
#include <QMutexLocker>
#include <QString>
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torcthread.h"
#include "audiooutput.h"
#include "audiopulsehandler.h"

#define IS_READY(arg) ((PA_CONTEXT_READY      == arg) || \
                       (PA_CONTEXT_FAILED     == arg) || \
                       (PA_CONTEXT_TERMINATED == arg))

static QString PAStateToString(pa_context_state state)
{
    QString ret = "Unknown";
    switch (state)
    {
        case PA_CONTEXT_UNCONNECTED:  ret = "Unconnected";  break;
        case PA_CONTEXT_CONNECTING:   ret = "Connecting";   break;
        case PA_CONTEXT_AUTHORIZING:  ret = "Authorizing";  break;
        case PA_CONTEXT_SETTING_NAME: ret = "Setting Name"; break;
        case PA_CONTEXT_READY:        ret = "Ready!";       break;
        case PA_CONTEXT_FAILED:       ret = "Failed";       break;
        case PA_CONTEXT_TERMINATED:   ret = "Terminated";   break;
    }

    return ret;
}

PulseHandler* PulseHandler::gPulseHandler = NULL;
bool          PulseHandler::gPulseHandlerActive = false;

bool PulseHandler::Suspend(enum PulseAction Action)
{
    // global lock around all access to our global singleton
    static QMutex global_lock;
    QMutexLocker locker(&global_lock);

    // cleanup the PulseAudio server connection if requested
    if (kPulseCleanup == Action)
    {
        if (gPulseHandler)
        {
            LOG(VB_GENERAL, LOG_INFO, "Cleaning up PulseHandler");
            delete gPulseHandler;
            gPulseHandler = NULL;
        }
        return true;
    }

    // do nothing if PulseAudio is not currently running
    if (!AudioOutput::IsPulseAudioRunning())
    {
        LOG(VB_AUDIO, LOG_INFO, "PulseAudio not running");
        return false;
    }

    // make sure any pre-existing handler is still valid
    if (gPulseHandler && !gPulseHandler->Valid())
    {
        LOG(VB_AUDIO, LOG_INFO, "PulseHandler invalidated. Deleting.");
        delete gPulseHandler;
        gPulseHandler = NULL;
    }

    // create our handler
    if (!gPulseHandler)
    {
        PulseHandler* handler = new PulseHandler();
        if (handler)
        {
            LOG(VB_AUDIO, LOG_INFO, "Created PulseHandler object");
            gPulseHandler = handler;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create PulseHandler object");
            return false;
        }
    }

    bool result;
    // enable processing of incoming callbacks
    gPulseHandlerActive = true;
    result = gPulseHandler->SuspendInternal(kPulseSuspend == Action);
    // disable processing of incoming callbacks in case we delete/recreate our
    // instance due to a termination or other failure
    gPulseHandlerActive = false;
    return result;
}

static void StatusCallback(pa_context *ctx, void *userdata)
{
    // ignore any status updates while we're inactive, we can update
    // directly as needed
    if (!ctx || !PulseHandler::gPulseHandlerActive)
        return;

    // validate the callback
    PulseHandler *handler = static_cast<PulseHandler*>(userdata);
    if (!handler)
    {
        LOG(VB_GENERAL, LOG_ERR, "Callback: no handler.");
        return;
    }

    if (handler->m_context != ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, "Callback: handler/context mismatch.");
        return;
    }

    if (handler != PulseHandler::gPulseHandler)
    {
        LOG(VB_GENERAL, LOG_ERR, "Callback: returned handler is not the global handler.");
        return;
    }

    // update our status
    pa_context_state state = pa_context_get_state(ctx);
    LOG(VB_AUDIO, LOG_INFO, QString("Callback: State changed %1->%2")
            .arg(PAStateToString(handler->m_contextState))
            .arg(PAStateToString(state)));
    handler->m_contextState = state;
}

static void OperationCallback(pa_context *ctx, int success, void *userdata)
{
    if (!ctx)
        return;

    // ignore late updates but flag them as they may be an issue
    if (!PulseHandler::gPulseHandlerActive)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Received a late/unexpected operation callback. Ignoring.");
        return;
    }

    // validate the callback
    PulseHandler *handler = static_cast<PulseHandler*>(userdata);
    if (!handler)
    {
        LOG(VB_GENERAL, LOG_ERR, "Operation: no handler.");
        return;
    }

    if (handler->m_context != ctx)
    {
        LOG(VB_GENERAL, LOG_ERR, "Operation: handler/context mismatch.");
        return;
    }

    if (handler != PulseHandler::gPulseHandler)
    {
        LOG(VB_GENERAL, LOG_ERR, "Operation: returned handler is not the global handler.");
        return;
    }

    // update the context
    handler->m_pendingOperations--;
    LOG(VB_AUDIO, LOG_INFO, QString("Operation: success %1 remaining %2")
            .arg(success).arg(handler->m_pendingOperations));
}

PulseHandler::PulseHandler(void)
  : m_contextState(PA_CONTEXT_UNCONNECTED),
    m_context(NULL),
    m_pendingOperations(0),
    m_loop(NULL),
    m_initialised(false),
    m_valid(false),
    m_thread(NULL)
{
}

PulseHandler::~PulseHandler(void)
{
    // TODO - do we need to drain the context??

    LOG(VB_AUDIO, LOG_INFO, "Destroying PulseAudio handler");

    // is this correct?
    if (m_context)
    {
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
    }

    if (m_loop)
    {
        pa_signal_done();
        pa_mainloop_free(m_loop);
    }
}

bool PulseHandler::Valid(void)
{
    if (m_initialised && m_valid)
    {
        m_contextState = pa_context_get_state(m_context);
        return PA_CONTEXT_READY == m_contextState;
    }
    return false;
}

bool PulseHandler::Init(void)
{
    if (m_initialised)
        return m_valid;
    m_initialised = true;

    // Initialse our connection to the server
    m_loop = pa_mainloop_new();
    if (!m_loop)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get PulseAudio mainloop");
        return m_valid;
    }

    pa_mainloop_api *api = pa_mainloop_get_api(m_loop);
    if (!api)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get PulseAudio api");
        return m_valid;
    }

    if (pa_signal_init(api) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to initialise signaling");
        return m_valid;
    }

    const char *client = "torc";
    m_context = pa_context_new(api, client);
    if (!m_context)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create context");
        return m_valid;
    }

    // remember which thread created this object for later sanity debugging
    m_thread = QThread::currentThread();

    // we set the callback, connect and then run the main loop 'by hand'
    // until we've successfully connected (or not)
    pa_context_set_state_callback(m_context, StatusCallback, this);
    pa_context_connect(m_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
    int ret = 0;
    int tries = 0;
    while ((tries++ < 100) && !IS_READY(m_contextState))
    {
        pa_mainloop_iterate(m_loop, 0, &ret);
        usleep(10000);
    }

    if (PA_CONTEXT_READY != m_contextState)
    {
        LOG(VB_GENERAL, LOG_ERR, "Context not ready after 1000ms");
        return m_valid;
    }

    LOG(VB_AUDIO, LOG_INFO, "Initialised handler");
    m_valid = true;
    return m_valid;
}

bool PulseHandler::SuspendInternal(bool Suspend)
{
    // set everything up...
    if (!Init())
        return false;

    // just in case it all goes pete tong
    if (!TorcThread::IsCurrentThread(m_thread))
        LOG(VB_AUDIO, LOG_WARNING, "PulseHandler called from a different thread");

    QString action = Suspend ? "suspend" : "resume";
    // don't bother to suspend a networked server
    if (!pa_context_is_local(m_context))
    {
        LOG(VB_GENERAL, LOG_ERR, "PulseAudio server is remote. No need to " + action);
        return false;
    }

    // create and dispatch 2 operations to suspend or resume all current sinks
    // and all current sources
    m_pendingOperations = 2;
    pa_operation *operation_sink =
        pa_context_suspend_sink_by_index(
            m_context, PA_INVALID_INDEX, Suspend, OperationCallback, this);
    pa_operation_unref(operation_sink);

    pa_operation *operation_source =
        pa_context_suspend_source_by_index(
            m_context, PA_INVALID_INDEX, Suspend, OperationCallback, this);
    pa_operation_unref(operation_source);

    // run the loop manually and wait for the callbacks
    int count = 0;
    int ret = 0;
    while (m_pendingOperations && count++ < 100)
    {
        pa_mainloop_iterate(m_loop, 0, &ret);
        usleep(10000);
    }

    // a failure isn't necessarily disastrous
    if (m_pendingOperations)
    {
        m_pendingOperations = 0;
        LOG(VB_GENERAL, LOG_ERR, "Failed to " + action);
        return false;
    }

    // rejoice
    LOG(VB_GENERAL, LOG_INFO, "PulseAudio " + action + " OK");
    return true;
}
