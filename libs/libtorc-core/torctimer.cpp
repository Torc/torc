#include "torctimer.h"

void TorcTimer::Start(void)
{
    m_running = true;
    m_timer.start();
}

int TorcTimer::Restart(void)
{
    int ret = Elapsed();
    m_timer.restart();
    return ret;
}

int TorcTimer::Elapsed(void)
{
    int ret = m_timer.elapsed();
    if (ret > 86300000)
    {
        ret = 0;
        m_timer.restart();
    }
    return ret;
}

void TorcTimer::Stop(void)
{
    m_running = false;
}

bool TorcTimer::IsRunning(void) const
{
    return m_running;
}

void TorcTimer::AddMSecs(int Ms)
{
    m_timer.addMSecs(Ms);
}
