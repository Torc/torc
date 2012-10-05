#ifndef AUDIOPULSEHANDLER_H
#define AUDIOPULSEHANDLER_H

#include <pulse/pulseaudio.h>

class QThread;

class PulseHandler
{
  public:
    enum PulseAction
    {
        kPulseSuspend = 0,
        kPulseResume,
        kPulseCleanup
    };

    static PulseHandler *gPulseHandler;
    static bool          gPulseHandlerActive;
    static bool          Suspend         (enum PulseAction Action);

   ~PulseHandler(void);
    bool                 Valid           (void);

  private:
    PulseHandler(void);
    bool                 Init            (void);
    bool                 SuspendInternal (bool Suspend);

  public:
    pa_context_state     m_contextState;
    pa_context          *m_context;
    int                  m_pendingOperations;

  private:
    pa_mainloop         *m_loop;
    bool                 m_initialised;
    bool                 m_valid;
    QThread             *m_thread;
};

#endif // AUDIOPULSEHANDLER_H
