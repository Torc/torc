#ifndef VIDEOCOLOURSPACE_H
#define VIDEOCOLOURSPACE_H

// Qt
#include <QMatrix4x4>

extern "C" {
#include "libavformat/avformat.h"
}

class VideoColourSpace
{
  public:
    static QString ColourSpaceToString  (AVColorSpace ColorSpace);

  public:
    explicit VideoColourSpace(AVColorSpace ColourSpace);
    ~VideoColourSpace();

    bool           HasChanged           (void);

    AVColorSpace   GetColourSpace       (void);
    int            GetBrightness        (void);
    int            GetContrast          (void);
    int            GetSaturation        (void);
    int            GetHue               (void);

    void           ChangeBrightness     (bool Increase);
    void           ChangeContrast       (bool Increase);
    void           ChangeSaturation     (bool Increase);
    void           ChangeHue            (bool Increase);

    void           SetColourSpace       (AVColorSpace ColourSpace);
    void           SetBrightness        (int  Value);
    void           SetContrast          (int  Value);
    void           SetHue               (int  Value);
    void           SetSaturation        (int  Value);

  private:
    void           SetBrightnessPriv    (int Value, bool UpdateMatrix, bool UpdateSettings);
    void           SetContrastPriv      (int Value, bool UpdateMatrix, bool UpdateSettings);
    void           SetHuePriv           (int Value, bool UpdateMatrix, bool UpdateSettings);
    void           SetSaturationPriv    (int Value, bool UpdateMatrix, bool UpdateSettings);

    void           SetStudioLevels      (bool Studio, bool UpdateMatrix);
    void           SetMatrix            (void);

  private:
    AVColorSpace   m_colourSpace;
    bool           m_changed;
    bool           m_studioLevels;
    float          m_brightness;
    float          m_contrast;
    float          m_saturation;
    float          m_hue;
    QMatrix4x4     m_matrix;
};

#endif // VIDEOCOLOURSPACE_H
