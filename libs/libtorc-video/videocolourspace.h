#ifndef VIDEOCOLOURSPACE_H
#define VIDEOCOLOURSPACE_H

// Qt
#include <QObject>

// Torc
#include "torcplayer.h"

extern "C" {
#include "libavformat/avformat.h"
}

class Matrix
{
  public:
    Matrix(float m11, float m12, float m13, float m14,
           float m21, float m22, float m23, float m24,
           float m31, float m32, float m33, float m34);
    Matrix();

    void setToIdentity(void);
    void scale(float val1, float val2, float val3);
    void translate(float val1, float val2, float val3);
    Matrix & operator*=(const Matrix &r);
    void product(int row, const Matrix &r);
    void debug(void);
    float m[4][4];
};

class VideoColourSpace : public QObject
{
    Q_OBJECT

  public:
    static QString ColourSpaceToString  (AVColorSpace ColorSpace);

  signals:
    void           PropertyChanged      (TorcPlayer::PlayerProperty Property, QVariant Value);

  public:
    explicit VideoColourSpace(AVColorSpace ColourSpace);
    ~VideoColourSpace();

    void           SetChanged           (void);
    bool           HasChanged           (void);
    float*         Data                 (void);

    AVColorSpace   GetColourSpace       (void);
    AVColorRange   GetColourRange       (void);
    int            GetBrightness        (void);
    int            GetContrast          (void);
    int            GetSaturation        (void);
    int            GetHue               (void);
    bool           GetStudioLevels      (void);

    void           ChangeBrightness     (bool Increase);
    void           ChangeContrast       (bool Increase);
    void           ChangeSaturation     (bool Increase);
    void           ChangeHue            (bool Increase);

    void           SetColourSpace       (AVColorSpace ColourSpace);
    void           SetColourRange       (AVColorRange ColourRange);
    void           SetBrightness        (int  Value);
    void           SetContrast          (int  Value);
    void           SetHue               (int  Value);
    void           SetSaturation        (int  Value);
    void           SetStudioLevels      (bool Value);

  private:
    void           SetBrightnessPriv    (int Value, bool UpdateMatrix, bool UpdateSettings);
    void           SetContrastPriv      (int Value, bool UpdateMatrix, bool UpdateSettings);
    void           SetHuePriv           (int Value, bool UpdateMatrix, bool UpdateSettings);
    void           SetSaturationPriv    (int Value, bool UpdateMatrix, bool UpdateSettings);

    void           SetStudioLevelsPriv  (bool Studio, bool UpdateMatrix);
    void           SetMatrix            (void);

  private:
    AVColorSpace   m_colourSpace;
    AVColorRange   m_colourRange;
    bool           m_changed;
    bool           m_studioLevels;
    float          m_brightness;
    float          m_contrast;
    float          m_saturation;
    float          m_hue;
    Matrix         m_matrix;
};

#endif // VIDEOCOLOURSPACE_H
