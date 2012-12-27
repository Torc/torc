// Std
#include <cstdlib>
#include <cmath>

// Qt
#include <QFile>

// Torc
#include "torclogging.h"
#include "torccoreutils.h"
#include "uiperformance.h"

#define UNIX_PROC_STAT "/proc/stat"
#define MAX_CORES 8

UIPerformance::UIPerformance()
  : m_currentCount(0),
    m_totalCount(0),
    m_starttime(0),
    m_starttimeValid(false),
    m_lastFPS(0.0),
    m_lastTotalSD(0.0),
    m_lastRenderSD(0.0),
    m_CPUStat(NULL),
    m_lastStats(NULL)
{
    m_totalTimes.resize(m_totalCount);
    m_renderTimes.resize(m_totalCount);

#ifdef __linux__
    if (QFile::exists(UNIX_PROC_STAT))
    {
        m_CPUStat = new QFile(UNIX_PROC_STAT);
        if (m_CPUStat)
        {
            if (!m_CPUStat->open(QIODevice::ReadOnly))
            {
                delete m_CPUStat;
                m_CPUStat = NULL;
            }
            else
            {
                m_lastStats = new unsigned long long[MAX_CORES * 9];
            }
        }
    }
#endif
}

UIPerformance::~UIPerformance()
{
    if (m_CPUStat)
        m_CPUStat->close();
    delete m_CPUStat;
    delete [] m_lastStats;
}

void UIPerformance::SetFrameCount(int Count)
{
    m_currentCount = 0;
    m_totalCount   = Count;
    m_totalTimes.resize(m_totalCount);
    m_renderTimes.resize(m_totalCount);
    m_totalTimes.fill(0);
    m_renderTimes.fill(0);
}

quint64 UIPerformance::StartFrame(void)
{
    if (!m_totalCount)
        return 0;

    quint64 time = GetMicrosecondCount();
    RecordEndTime(time);
    RecordStartTime(time);
    return time;
}

void UIPerformance::FinishDrawing(void)
{
    if (!m_totalCount || !m_starttimeValid)
        return;

    m_renderTimes[m_currentCount] = GetMicrosecondCount() - m_starttime;
}

void UIPerformance::RecordEndTime(quint64 Time)
{
    if (!m_totalCount)
        return;

    int cycles = m_totalCount;

    if (m_starttimeValid)
    {
        m_totalTimes[m_currentCount] = Time - m_starttime;
        m_currentCount++;
    }

    m_starttimeValid = false;

    if (m_currentCount >= cycles)
    {
        qreal totalmean  = 0.0;
        qreal rendermean = 0.0;

        for(int i = 0; i < cycles; i++)
        {
            rendermean += m_renderTimes[i];
            totalmean  += m_totalTimes[i];
        }

        if (totalmean > 0)
            m_lastFPS = (cycles / totalmean) * 1000000.0;

        totalmean  /= cycles;
        rendermean /= cycles;

        qreal totaldeviations  = 0.0;
        qreal renderdeviations = 0.0;
        for(int i = 0; i < cycles; i++)
        {
            renderdeviations += (rendermean - m_renderTimes[i]) * (rendermean - m_renderTimes[i]);
            totaldeviations  += (totalmean - m_totalTimes[i]) * (totalmean - m_totalTimes[i]);
        }

        qreal rendersd = sqrt(renderdeviations / (cycles - 1));
        qreal totalsd  = sqrt(totaldeviations / (cycles - 1));
        if (totalmean > 0.0)
            m_lastTotalSD = totalsd / totalmean;
        if (rendermean > 0.0)
            m_lastRenderSD = rendersd / rendermean;

        QString extra;
        m_lastCPUStats = GetCPUStat();
        if (!m_lastCPUStats.isEmpty())
            extra = QString("Load: ") + m_lastCPUStats;

        LOG(VB_GUI, LOG_INFO,
                QString("FPS: %1%2%3 Draw: %4% ")
                .arg(m_lastFPS, 4, 'f', 3)
                .arg(QChar(0xB1, 0))
                .arg(m_lastTotalSD, 2, 'f', 2)
                .arg((rendermean / totalmean) * 100.0, 2, 'f', 0)
                + extra);

        m_currentCount = 0;
        return;
    }

    return;
}

void UIPerformance::RecordStartTime(quint64 Time)
{
    m_starttime = Time;
    m_starttimeValid = true;
}

QString UIPerformance::GetCPUStat(void)
{
    if (!m_CPUStat)
        return QString();

#ifdef __linux__
    QString res;
    m_CPUStat->seek(0);
    m_CPUStat->flush();

    QByteArray line = m_CPUStat->readLine(256);
    if (line.isEmpty())
        return res;

    int cores = 0;
    int ptr   = 0;
    line = m_CPUStat->readLine(256);
    while (!line.isEmpty() && cores < MAX_CORES)
    {
        static const int size = sizeof(unsigned long long) * 9;
        unsigned long long stats[9];
        memset(stats, 0, size);
        int num = 0;
        if (sscanf(line.constData(),
                   "cpu%30d %30llu %30llu %30llu %30llu %30llu "
                   "%30llu %30llu %30llu %30llu %*5000s\n",
                   &num, &stats[0], &stats[1], &stats[2], &stats[3],
                   &stats[4], &stats[5], &stats[6], &stats[7], &stats[8]) >= 4)
        {
            float load  = stats[0] + stats[1] + stats[2] + stats[4] +
                          stats[5] + stats[6] + stats[7] + stats[8] -
                          m_lastStats[ptr + 0] - m_lastStats[ptr + 1] -
                          m_lastStats[ptr + 2] - m_lastStats[ptr + 4] -
                          m_lastStats[ptr + 5] - m_lastStats[ptr + 6] -
                          m_lastStats[ptr + 7] - m_lastStats[ptr + 8];
            float total = load + stats[3] - m_lastStats[ptr + 3];
            if (total > 0)
                res += QString("%1% ").arg(load / total * 100, 0, 'f', 0);
            memcpy(&m_lastStats[ptr], stats, size);
        }
        line = m_CPUStat->readLine(256);
        cores++;
        ptr += 9;
    }
    return res;
#else
    return QString();
#endif
}
