#ifndef UIPERFORMANCE_H
#define UIPERFORMANCE_H

// Qt
#include <QtGlobal>
#include <QVector>
#include <QString>

class QFile;

class UIPerformance
{
  public:
    UIPerformance();
    virtual ~UIPerformance();

    void    SetFrameCount   (int Count);
    quint64 StartFrame      (void);
    void    FinishDrawing   (void);

  private:
    quint64 RecordStartTime (void);
    bool    RecordEndTime   (void);
    QString GetCPUStat      (void);

  private:
    int                     m_currentCount;
    int                     m_totalCount;
    quint64                 m_starttime;
    bool                    m_starttimeValid;
    QVector<uint>           m_totalTimes;
    QVector<uint>           m_renderTimes;
    qreal                   m_lastFPS;
    qreal                   m_lastTotalSD;
    qreal                   m_lastRenderSD;
    QFile                  *m_CPUStat;
    unsigned long long     *m_lastStats;
    QString                 m_lastCPUStats;
};

#endif // UIPERFORMANCE_H
