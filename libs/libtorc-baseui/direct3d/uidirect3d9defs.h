#ifndef UIDIRECT3D9DEFS_H
#define UIDIRECT3D9DEFS_H

#include "include/d3dx9.h"

typedef HRESULT (__stdcall * TORC_D3DXCOMPILESHADER)
    (const char*, UINT,  const D3DXMACRO*, ID3DXInclude*, const char*,
     const char*, DWORD, ID3DXBuffer**, ID3DXBuffer**, ID3DXConstantTable**);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXMULTIPLY)
    (D3DXMATRIX*, const D3DXMATRIX*, const D3DXMATRIX*);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXPERSPECTIVEOFFCENTERLH)
    (D3DXMATRIX*, float, float, float, float, float, float);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXORTHOOFFCENTERLH)
    (D3DXMATRIX*, float, float, float, float, float, float);

typedef HRESULT (__stdcall * TORC_D3DXLOADSURFACEFROMMEMORY)
    (LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*, LPCVOID, D3DFORMAT, UINT,
     const PALETTEENTRY*, const RECT *, DWORD, D3DCOLOR);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXTRANSLATION)
    (D3DXMATRIX*, float, float, float);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXREFLECT)
    (D3DXMATRIX*, CONST D3DXPLANE*);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXROTATIONZ)
    (D3DXMATRIX*, float);

typedef D3DXMATRIX* (__stdcall * TORC_D3DXMATRIXSCALING)
    (D3DXMATRIX*, float, float, float);

#endif // UIDIRECT3D9DEFS_H
