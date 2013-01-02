#ifndef UIIMAGELOADER_H
#define UIIMAGELOADER_H

// Qt
#include <QRunnable>

class UIImage;
class UIImageTracker;

class UIImageLoader : public QRunnable
{
    friend class UIImageTracker;

  public:
    UIImageLoader(UIImageTracker *Parent, UIImage *Image);
    virtual ~UIImageLoader();

  protected:
    virtual void run();

  private:
    UIImageTracker *m_parent;
    UIImage        *m_image;

};

#endif // UIIMAGELOADER_H
