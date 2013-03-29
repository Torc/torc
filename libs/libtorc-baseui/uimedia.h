#ifndef UIMEDIA_H
#define UIMEDIA_H

// Torc
#include "torcplayer.h"
#include "uiwidget.h"

class UIMedia : public UIWidget, public TorcPlayerInterface
{
    Q_OBJECT

  public:
    static  int     kUIMediaType;
    UIMedia(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual         ~UIMedia();

    bool            HandleAction              (int Action);
    void            CopyFrom                  (UIWidget *Other);
    bool            Refresh                   (quint64 TimeNow);
    bool            Finalise                  (void);

    bool            InitialisePlayer          (void);
    bool            HandlePlayerAction        (int Action);

    // signals to update the theme
  signals:
    void            PropertyChanged           (int Property, QVariant Value);
    void            PropertyAvailable         (int Property);
    void            PropertyUnavailable       (int Property);

    // slots to interrogate the widget from theme etc (external)
  public slots:
    QVariant        GetProperty               (int Property);
    void            SetProperty               (int Property, const QVariant Value);
    bool            IsPropertyAvailable       (int Property);

    // slots that update the widget from child objects (internal)
  public slots:
    void            PlayerStateChanged        (TorcPlayer::PlayerState NewState);
    void            PlayerPropertyChanged     (TorcPlayer::PlayerProperty Property, const QVariant &Value);
    void            PlayerPropertyAvailable   (TorcPlayer::PlayerProperty Property);
    void            PlayerPropertyUnavailable (TorcPlayer::PlayerProperty Property);

  protected:
    bool            DrawSelf                  (UIWindow* Window, qreal XOffset, qreal YOffset);
    bool            event                     (QEvent      *Event);
    bool            InitialisePriv            (QDomElement *Element);
    void            CreateCopy                (UIWidget *Parent);

  private:
    Q_DISABLE_COPY(UIMedia);
};

Q_DECLARE_METATYPE(UIMedia*);

#endif // UIMEDIA_H
