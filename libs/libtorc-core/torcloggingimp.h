#ifndef TORCLOGGINGIMP_H
#define TORCLOGGINGIMP_H

#define LOGLINE_MAX (2048-120)

class QFile;
class LogItem;

void RegisterLoggingThread(const QString &name);
void DeregisterLoggingThread(void);

class LoggerBase
{
  public:
    LoggerBase(QString FileName);
    virtual ~LoggerBase();

    virtual bool Logmsg(LogItem *Item) = 0;

  protected:
    QString      m_fileName;
};

class FileLogger : public LoggerBase
{
  public:
    FileLogger(QString Filename, bool ErrorsOnly, int Quiet);
   ~FileLogger();

    bool   Logmsg(LogItem *Item);

  private:
    bool   m_opened;
    QFile *m_file;
    bool   m_errorsOnly;
    int    m_quiet;
};

#endif // TORCLOGGINGIMP_H
