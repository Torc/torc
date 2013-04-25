#ifndef NVIDIAVDPAU_H
#define NVIDIAVDPAU_H

// Qt
#include <QString>

enum NVVDPAUFeatureSet
{
    Set_Unknown = -1,
    Set_None    = 0,
    Set_A1,
    Set_A,
    Set_B1,
    Set_B,
    Set_C,
    Set_D
};

NVVDPAUFeatureSet GetNVIDIAFeatureSet (int PCIID);
QString           FeatureSetToString  (NVVDPAUFeatureSet Set);

#endif // NVIDIAVDPAU_H
