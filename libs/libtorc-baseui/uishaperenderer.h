#ifndef UISHAPERENDERER_H
#define UISHAPERENDERER_H

// Qt
#include <QRunnable>
#include <QSize>

class QPainterPath;
class QPaintDevice;
class UIImage;
class UIShapePath;
class UIImageTracker;

class UIShapeRenderer : public QRunnable
{
    friend class UIImageTracker;

  public:
    UIShapeRenderer(UIImageTracker *Parent, UIImage *Image,
                    UIShapePath    *Path,   QSizeF  Size);

    ~UIShapeRenderer();

     static void     RenderShape(QPaintDevice* Device, UIShapePath *Path);

   protected:
     virtual void    run(void);

   private:
     UIImageTracker *m_parent;
     UIImage        *m_image;
     UIShapePath    *m_path;
     QSizeF          m_size;
};

#endif // UISHAPERENDERER_H
