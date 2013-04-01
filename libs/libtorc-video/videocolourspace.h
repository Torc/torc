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
    void           PropertyAvailable    (TorcPlayer::PlayerProperty Property);
    void           PropertyUnavailable  (TorcPlayer::PlayerProperty Property);

  public slots:
    void           SetPropertyAvailable (TorcPlayer::PlayerProperty Property);
    void           SetPropertyUnavailable (TorcPlayer::PlayerProperty Property);

  public:
    explicit VideoColourSpace(AVColorSpace ColourSpace);
    ~VideoColourSpace();

    QList<TorcPlayer::PlayerProperty> GetSupportedProperties (void);

    void           SetChanged           (void);
    bool           HasChanged           (void);
    float*         Data                 (void);

    AVColorSpace   GetColourSpace       (void);
    AVColorRange   GetColourRange       (void);
    bool           GetStudioLevels      (void);
    void           SetColourSpace       (AVColorSpace ColourSpace);
    void           SetColourRange       (AVColorRange ColourRange);
    void           SetStudioLevels      (bool Value);

    QVariant       GetProperty          (TorcPlayer::PlayerProperty Property);
    void           ChangeProperty       (TorcPlayer::PlayerProperty Property, bool Increase);
    void           SetProperty          (TorcPlayer::PlayerProperty Property, int Value);

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
    int            m_hueOffset;
    Matrix         m_matrix;
    QList<TorcPlayer::PlayerProperty> m_supportedProperties;
};

#endif // VIDEOCOLOURSPACE_H
