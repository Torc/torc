/* Class VideoColourSpace
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclocalcontext.h"
#include "videocolourspace.h"

Matrix::Matrix(float m11, float m12, float m13, float m14,
               float m21, float m22, float m23, float m24,
               float m31, float m32, float m33, float m34)
{
    m[0][0] = m11; m[0][1] = m12; m[0][2] = m13; m[0][3] = m14;
    m[1][0] = m21; m[1][1] = m22; m[1][2] = m23; m[1][3] = m24;
    m[2][0] = m31; m[2][1] = m32; m[2][2] = m33; m[2][3] = m34;
    m[3][0] = m[3][1] = m[3][2] = m[3][3] = 1.0f;
}

Matrix::Matrix()
{
    setToIdentity();
}

void Matrix::setToIdentity(void)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

void Matrix::scale(float val1, float val2, float val3)
{
    Matrix scale;
    scale.m[0][0] = val1;
    scale.m[1][1] = val2;
    scale.m[2][2] = val3;
    this->operator *=(scale);
}

void Matrix::translate(float val1, float val2, float val3)
{
    Matrix translate;
    translate.m[0][3] = val1;
    translate.m[1][3] = val2;
    translate.m[2][3] = val3;
    this->operator *=(translate);
}

Matrix & Matrix::operator*=(const Matrix &r)
{
    for (int i = 0; i < 3; i++)
        product(i, r);
    return *this;
}

void Matrix::product(int row, const Matrix &r)
{
    float t0, t1, t2, t3;
    t0 = m[row][0] * r.m[0][0] + m[row][1] * r.m[1][0] + m[row][2] * r.m[2][0];
    t1 = m[row][0] * r.m[0][1] + m[row][1] * r.m[1][1] + m[row][2] * r.m[2][1];
    t2 = m[row][0] * r.m[0][2] + m[row][1] * r.m[1][2] + m[row][2] * r.m[2][2];
    t3 = m[row][0] * r.m[0][3] + m[row][1] * r.m[1][3] + m[row][2] * r.m[2][3] + m[row][3];
    m[row][0] = t0; m[row][1] = t1; m[row][2] = t2; m[row][3] = t3;
}

void Matrix::debug(void)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("%1 %2 %3 %4")
        .arg(m[0][0], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[0][1], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[0][2], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[0][3], 4, 'f', 4, QLatin1Char('0')));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("%1 %2 %3 %4")
        .arg(m[1][0], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[1][1], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[1][2], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[1][3], 4, 'f', 4, QLatin1Char('0')));
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("%1 %2 %3 %4")
        .arg(m[2][0], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[2][1], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[2][2], 4, 'f', 4, QLatin1Char('0'))
        .arg(m[2][3], 4, 'f', 4, QLatin1Char('0')));
}

/*! \class VideoColourSpace
 *  \brief Colourspace tracker for video rendering.
 *
 * \note The studio levels setting should track the global UI setting.
 *
 * \todo Return matrix data
 * \todo Double to float conversion
*/

QString VideoColourSpace::ColourSpaceToString(AVColorSpace ColorSpace)
{
    switch (ColorSpace)
    {
        case AVCOL_SPC_RGB:         return QString("RGB");
        case AVCOL_SPC_BT709:       return QString("BT709");
        case AVCOL_SPC_UNSPECIFIED: return QString("Unspecified");
        case AVCOL_SPC_FCC:         return QString("FCC");
        case AVCOL_SPC_BT470BG:     return QString("BT470/BT601-6 525");
        case AVCOL_SPC_SMPTE170M:   return QString("SMPTE170M/BT601-6 525");
        case AVCOL_SPC_SMPTE240M:   return QString("SMPTE240M");
        case AVCOL_SPC_YCOCG:       return QString("YCOCG");
        default:
            break;
    }

    return QString("Unkown");
}

VideoColourSpace::VideoColourSpace(AVColorSpace ColourSpace)
  : m_colourSpace(ColourSpace),
    m_changed(true),
    m_studioLevels(false),
    m_brightness(0.0f),
    m_contrast(1.0f),
    m_saturation(1.0),
    m_hue(0.0f)
{
    SetBrightnessPriv(gLocalContext->GetSetting(TORC_GUI + "Brightness", 50), false, false);
    SetContrastPriv(gLocalContext->GetSetting(TORC_GUI + "Contrast", 50), false, false);
    SetSaturationPriv(gLocalContext->GetSetting(TORC_GUI + "Saturation", 50), false, false);
    SetHuePriv(gLocalContext->GetSetting(TORC_GUI + "Hue", 0), false, false);
    SetStudioLevels(gLocalContext->GetSetting(TORC_GUI + "StudioLevels", (bool)false), true);
}

VideoColourSpace::~VideoColourSpace()
{
}

void VideoColourSpace::SetChanged(void)
{
    m_changed = true;
}

bool VideoColourSpace::HasChanged(void)
{
    return m_changed;
}

float* VideoColourSpace::Data(void)
{
    m_changed = false;
    return &m_matrix.m[0][0];
}

AVColorSpace VideoColourSpace::GetColourSpace(void)
{
    return m_colourSpace;
}

int VideoColourSpace::GetBrightness(void)
{
    return ((m_brightness + 1.0f) / 0.02f);
}

int VideoColourSpace::GetContrast(void)
{
    return m_contrast / 0.02f;
}

int VideoColourSpace::GetSaturation(void)
{
    return m_saturation / 0.02f;
}

int VideoColourSpace::GetHue(void)
{
    return (m_hue * 180.0f) / (-3.6f * M_PI);
}

void VideoColourSpace::ChangeBrightness(bool Increase)
{
    int current = GetBrightness();
    current += Increase ? 1 : -1;
    SetBrightness(current);
}

void VideoColourSpace::ChangeContrast(bool Increase)
{
    int current = GetContrast();
    current += Increase ? 1 : -1;
    SetContrast(current);
}

void VideoColourSpace::ChangeSaturation(bool Increase)
{
    int current = GetContrast();
    current += Increase ? 1 : -1;
    SetSaturation(current);
}

void VideoColourSpace::ChangeHue(bool Increase)
{
    int current = GetHue();
    current += Increase ? 1 : -1;
    SetHue(current);
}

void VideoColourSpace::SetBrightness(int Value)
{
    SetBrightnessPriv(Value, true, true);
}

void VideoColourSpace::SetContrast(int Value)
{
    SetContrastPriv(Value, true, true);
}

void VideoColourSpace::SetSaturation(int Value)
{
    SetSaturationPriv(Value, true, true);
}

void VideoColourSpace::SetHue(int Value)
{
    SetHuePriv(Value, true, true);
}

void VideoColourSpace::SetBrightnessPriv(int Value, bool UpdateMatrix, bool UpdateSettings)
{
    int value = std::max(0, std::min(Value, 100));
    m_brightness = (value * 0.02f) - 1.0f;

    if (UpdateMatrix)
        SetMatrix();

    if (UpdateSettings)
        gLocalContext->SetSetting(TORC_GUI + "Brightness", value);
}


void VideoColourSpace::SetContrastPriv(int Value, bool UpdateMatrix, bool UpdateSettings)
{
    int value = std::max(0, std::min(Value, 100));
    m_contrast = value * 0.02f;

    if (UpdateMatrix)
        SetMatrix();

    if (UpdateSettings)
        gLocalContext->SetSetting(TORC_GUI + "Contrast", value);
}

void VideoColourSpace::SetSaturationPriv(int Value, bool UpdateMatrix, bool UpdateSettings)
{
    int value = std::max(0, std::min(Value, 100));
    m_saturation = value * 0.02f;

    if (UpdateMatrix)
        SetMatrix();

    if (UpdateSettings)
        gLocalContext->SetSetting(TORC_GUI + "Saturation", value);
}

void VideoColourSpace::SetHuePriv(int Value, bool UpdateMatrix, bool UpdateSettings)
{
    int value = std::max(0, std::min(Value, 100));
    m_hue = value * (-3.6f * M_PI / 180.0f);

    if (UpdateMatrix)
        SetMatrix();

    if (UpdateSettings)
        gLocalContext->SetSetting(TORC_GUI + "Hue", value);
}

void VideoColourSpace::SetColourSpace(AVColorSpace ColourSpace)
{
    if (m_colourSpace != ColourSpace)
    {
        m_colourSpace = ColourSpace;
        SetMatrix();
    }
}

void VideoColourSpace::SetStudioLevels(bool Studio, bool UpdateMatrix)
{
    m_studioLevels = Studio;
    if (UpdateMatrix)
        SetMatrix();
}

void VideoColourSpace::SetMatrix(void)
{
    float luma_range    = m_studioLevels ? 255.0f : 219.0f;
    float chroma_range  = m_studioLevels ? 255.0f : 224.0f;
    float luma_offset   = m_studioLevels ? 0.0f   : -16.0f / 255.0f;
    float chroma_offset = -128.0f / 255.0f;

    float uvcos         = m_saturation * cos(m_hue);
    float uvsin         = m_saturation * sin(m_hue);
    float brightness    = m_brightness * 255.0f / luma_range;
    float luma_scale    = 255.0f / luma_range;
    float chroma_scale  = 255.0f / chroma_range;

    Matrix matrix;
    switch (m_colourSpace)
    {
        case AVCOL_SPC_SMPTE240M:
            matrix = Matrix(1.000f, ( 0.0000f * uvcos) + ( 1.5756f * uvsin), ( 1.5756f * uvcos) - ( 0.0000f * uvsin), 0.0f,
                            1.000f, (-0.2253f * uvcos) + ( 0.5000f * uvsin), ( 0.5000f * uvcos) - (-0.2253f * uvsin), 0.0f,
                            1.000f, ( 1.8270f * uvcos) + ( 0.0000f * uvsin), ( 0.0000f * uvcos) - ( 1.8270f * uvsin), 0.0f);
            break;
        case AVCOL_SPC_BT709:
            matrix = Matrix(1.000f, ( 0.0000f * uvcos) + ( 1.5701f * uvsin), ( 1.5701f * uvcos) - ( 0.0000f * uvsin), 0.0f,
                            1.000f, (-0.1870f * uvcos) + (-0.4664f * uvsin), (-0.4664f * uvcos) - (-0.1870f * uvsin), 0.0f,
                            1.000f, ( 1.8556f * uvcos) + ( 0.0000f * uvsin), ( 0.0000f * uvcos) - ( 1.8556f * uvsin), 0.0f);
            break;
        case AVCOL_SPC_RGB:
            matrix.setToIdentity();
            break;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_UNSPECIFIED:
        case AVCOL_SPC_FCC:
        case AVCOL_SPC_YCOCG:
        default:
            matrix = Matrix(1.000f, ( 0.0000f * uvcos) + ( 1.4030f * uvsin), ( 1.4030f * uvcos) - ( 0.0000f * uvsin), 0.0f,
                            1.000f, (-0.3440f * uvcos) + (-0.7140f * uvsin), (-0.7140f * uvcos) - (-0.3440f * uvsin), 0.0f,
                            1.000f, ( 1.7730f * uvcos) + ( 0.0000f * uvsin), ( 0.0000f * uvcos) - ( 1.7730f * uvsin), 0.0f);
    }

    m_matrix.setToIdentity();
    m_matrix.translate(brightness, brightness, brightness);
    m_matrix.scale(m_contrast, m_contrast, m_contrast);
    m_matrix *= matrix;
    m_matrix.scale(luma_scale, chroma_scale, chroma_scale);
    m_matrix.translate(luma_offset, chroma_offset, chroma_offset);
    m_changed = true;
}
