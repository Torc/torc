#ifndef TORCEVENT_H
#define TORCEVENT_H

// Qt
#include <QMap>
#include <QVariant>
#include <QEvent>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcEvent : public QEvent
{
  public:
    TorcEvent(int Event, const QVariantMap Data = QVariantMap());
    virtual ~TorcEvent();

    int          Event (void);
    QVariantMap& Data  (void);
    TorcEvent*   Copy  (void) const;

    static       Type   TorcEventType;

  private:
    int         m_event;
    QVariantMap m_data;
};

#endif // TORCEVENT_H
