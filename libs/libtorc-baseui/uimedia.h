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

    bool            HandleAction       (int Action);
    void            CopyFrom           (UIWidget *Other);
    bool            Draw               (quint64 TimeNow, UIWindow* Window, qreal XOffset, qreal YOffset);
    bool            Finalise           (void);

    bool            InitialisePlayer   (void);
    bool            HandlePlayerAction (int Action);

  public slots:
    void            PlayerStateChanged (TorcPlayer::PlayerState NewState);

  protected:
    bool            event              (QEvent      *Event);
    bool            InitialisePriv     (QDomElement *Element);
    void            CreateCopy         (UIWidget *Parent);

  private:
    Q_DISABLE_COPY(UIMedia);
};

Q_DECLARE_METATYPE(UIMedia*);

#endif // UIMEDIA_H